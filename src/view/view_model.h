// Output in the game's head-tracking space:
//   yaw (rad, + = left), pitch (rad, + = up), roll (rad, + = tilt left),
//   x (m, + = right), y (m, + = up), z (m, + = backwards).
#pragma once
#include <cmath>

struct HeadCfg {
    float seat_back = 0.08f, seat_up = 0.02f, seat_right = 0.0f, seat_pitch_deg = 0.0f;

    float look_steer_deg   = 3.0f;
    float look_steer_until = 45.0f;
    float look_ahead_time  = 0.25f;
    float look_ahead_from  = 25.0f;
    float look_max_deg     = 10.0f;
    float look_smooth      = 0.80f;

    float lean_side   = 0.011f;
    float lean_fwd    = 0.009f;
    float bounce_up   = 0.0f;
    float body_freq   = 1.8f;
    float body_damp   = 0.55f;
    float bounce_freq = 3.0f;
    float roll_from_side_deg = 2.0f;
    float pitch_from_fwd_deg = 1.5f;
    float max_offset  = 0.08f;

    float breath_pitch_deg = 0.22f;
    float breath_up        = 0.0035f;
    float breath_period    = 4.2f;
    float idle_sway_deg    = 0.35f;
    float idle_sway_m      = 0.004f;

    // Surface changes: values for going from smooth asphalt to very rough ground at 60 km/h.
    float surface_feel      = 1.0f;
    float surface_step      = 0.0035f;
    float surface_step_pitch_deg = 0.18f;
    float surface_step_roll_deg  = 0.25f;
    float surface_rock      = 0.0025f;
    float surface_rock_roll_deg  = 0.12f;
    float surface_freq      = 1.6f;

    // Stabilized gaze: share of the cab tilt the neck compensates (keeps the horizon steadier).
    float stabilize_pitch   = 0.15f;
    float stabilize_roll    = 0.15f;
    float stabilize_time    = 0.25f;
    float stabilize_max_deg = 1.5f;
    // Inertia felt in the seat: cab tilt and sway added to the chassis acceleration.
    float cabin_inertia     = 0.3f;
};

struct Spring { float p = 0, v = 0; };

static inline float h_clamp(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

static inline void spring_step(Spring &s, float accel, float static_gain, float freq, float damp, float dt)
{
    const float w = 6.2831853f * freq;
    const int n = dt > 0.008f ? (int)ceilf(dt / 0.008f) : 1;
    const float h = dt / n;
    for (int i = 0; i < n; i++) {
        const float a = -static_gain * w * w * accel - w * w * s.p - 2 * damp * w * s.v;
        s.v += a * h;
        s.p += s.v * h;
    }
}

static inline void spring_free(Spring &s, float target, float freq, float damp, float dt)
{
    const float w = 6.2831853f * freq;
    const int n = dt > 0.004f ? (int)ceilf(dt / 0.004f) : 1;
    const float h = dt / n;
    for (int i = 0; i < n; i++) {
        const float a = w * w * (target - s.p) - 2 * damp * w * s.v;
        s.v += a * h;
        s.p += s.v * h;
    }
}

struct HeadIn {
    float speed_ms = 0;
    float steer = 0;
    float yaw_rate = 0;
    float ax = 0, ay = 0, az = 0;
    // Surface (computed by the plugin from the tractor wheels):
    float rough = 0;        // mean roughness under grounded wheels, 0 = smooth asphalt, 1 = grass/dirt
    float step_heave = 0;   // roughness change this frame (+ = got rougher)
    float step_pitch = 0;   // + = change at the front axle, - = at the rear axles
    float step_roll = 0;    // + = change on the left side
    bool  grounded = true;
    // Cab (rad): tilt relative to the chassis (+ = nose up / leaning left),
    // cab angular acceleration (rad/s2) and chassis angular velocity (rad/s).
    float cab_pitch = 0, cab_roll = 0;
    float cab_alpha_pitch = 0, cab_alpha_roll = 0;
    float body_pitch_rate = 0, body_roll_rate = 0;
    float head_height = 1.2f;
};

struct HeadOut { float yaw = 0, pitch = 0, roll = 0, x = 0, y = 0, z = 0; };

struct HeadState {
    float look = 0, look_v = 0;
    Spring sx, sz, sy;
    float ay_avg = 0;
    float t = 0;
    unsigned rng = 12345;
    float sw_y = 0, sw_r = 0, sw_x = 0, sw_ty = 0, sw_tr = 0, sw_tx = 0, sw_timer = 0;
    Spring st_heave, st_pitch, st_roll;
    Spring rk_heave, rk_roll;
    float rk_th = 0, rk_tr = 0, rk_timer = 0;
    float rough = 0;
    float surf_y = 0, surf_pitch = 0, surf_roll = 0;
    float ch_pitch = 0, ch_roll = 0, alpha_p = 0, alpha_r = 0;
    Spring stab_p, stab_r;
};

static inline float rnd(unsigned &s) { s = s * 1664525u + 1013904223u; return ((s >> 8) & 0xFFFF) / 65535.0f * 2 - 1; }

// Tractor wheels: position (x + = right, z + = backwards) and surface roughness under each one.
static const int HEAD_MAXW = 14;
struct Wheels { int n = 0; float x[HEAD_MAXW] = {}, z[HEAD_MAXW] = {}, rough[HEAD_MAXW] = {}; bool ground[HEAD_MAXW] = {}; };

struct WheelTrack {
    float cur[HEAD_MAXW], pend[HEAD_MAXW], pend_t[HEAD_MAXW];
    WheelTrack() { reset(); }
    void reset() { for (int i = 0; i < HEAD_MAXW; i++) { cur[i] = -1; pend[i] = -1; pend_t[i] = 0; } }
};

// A new surface only counts after 60 ms under the wheel (surface edges flicker and would cause jitter).
static inline void wheels_input(const Wheels &w, WheelTrack &tr, float dt, HeadIn &in)
{
    in.step_heave = in.step_pitch = in.step_roll = 0;
    const int n = w.n > HEAD_MAXW ? HEAD_MAXW : w.n;
    if (n <= 0) { in.grounded = true; in.rough = 0; return; }
    float minz = 1e9f;
    for (int i = 0; i < n; i++) if (w.z[i] < minz) minz = w.z[i];
    int nf = 0, nr = 0;
    for (int i = 0; i < n; i++) (w.z[i] <= minz + 0.6f ? nf : nr)++;

    float rsum = 0, wsum = 0;
    int ng = 0;
    for (int i = 0; i < n; i++) {
        if (!w.ground[i]) { tr.pend_t[i] = 0; continue; }
        ng++;
        const bool front = w.z[i] <= minz + 0.6f;
        const float r = w.rough[i];
        if (tr.cur[i] < 0) tr.cur[i] = r;
        if (fabsf(r - tr.cur[i]) < 0.02f) tr.pend_t[i] = 0;
        else {
            if (fabsf(r - tr.pend[i]) > 0.02f) { tr.pend[i] = r; tr.pend_t[i] = 0; }
            tr.pend_t[i] += dt;
            if (tr.pend_t[i] >= 0.06f) {
                const float d = r - tr.cur[i];
                const float side = w.x[i] < 0 ? 1.0f : -1.0f;
                tr.cur[i] = r;
                tr.pend_t[i] = 0;
                if (front) {
                    in.step_heave += d / nf;
                    in.step_pitch += d / nf;
                    in.step_roll  += side * d / nf;
                } else if (nr > 0) {
                    in.step_heave += 0.35f * d / nr;
                    in.step_pitch -= 0.6f * d / nr;
                    in.step_roll  += 0.35f * side * d / nr;
                }
            }
        }
        const float wgt = front ? 2.0f : 1.0f;
        rsum += tr.cur[i] * wgt;
        wsum += wgt;
    }
    in.grounded = ng > 0;
    in.rough = wsum > 0 ? rsum / wsum : 0;
}

static inline void surface_update(const HeadCfg &c, HeadState &st, const HeadIn &in, float kmh, float dt)
{
    const float D2R = 0.01745329f;
    const float fade = h_clamp((kmh - 3.0f) / 7.0f, 0, 1);
    const float spd = h_clamp(kmh / 60.0f, 0, 1.4f);

    // Step: a wheel moves onto rougher ground (usually lower, e.g. the shoulder) and the cab dips a
    // little; back on asphalt it rises. One impulse into a soft, damped spring.
    const float fq = c.surface_freq > 0.3f ? c.surface_freq : 0.3f;
    const float w = 6.2831853f * fq;
    const float kick = 1.916f * w * c.surface_feel * fade * powf(spd, 0.6f);
    st.st_heave.v -= in.step_heave * c.surface_step * kick;
    st.st_pitch.v -= in.step_pitch * c.surface_step_pitch_deg * D2R * kick;
    st.st_roll.v  += in.step_roll * c.surface_step_roll_deg * D2R * kick;
    spring_free(st.st_heave, 0, fq, 0.55f, dt);
    spring_free(st.st_pitch, 0, fq, 0.55f, dt);
    spring_free(st.st_roll, 0, fq * 0.8f, 0.55f, dt);

    // Slow rocking while the ground is rough (no shake: nothing above ~1.2 Hz).
    st.rough += ((in.grounded ? in.rough : 0.0f) - st.rough) * h_clamp(dt / 0.4f, 0, 1);
    st.rk_timer -= dt;
    if (st.rk_timer <= 0) {
        st.rk_timer = (0.55f - 0.25f * h_clamp(spd, 0, 1)) * (1.0f + 0.35f * rnd(st.rng));
        st.rk_th = rnd(st.rng);
        st.rk_tr = rnd(st.rng);
    }
    const float amp = c.surface_feel * st.rough * fade * powf(spd, 0.7f);
    spring_free(st.rk_heave, st.rk_th * amp, 1.1f, 1.0f, dt);
    spring_free(st.rk_roll, st.rk_tr * amp, 0.9f, 1.0f, dt);

    st.surf_y     = h_clamp(st.st_heave.p + st.rk_heave.p * c.surface_rock, -0.012f, 0.012f);
    st.surf_pitch = h_clamp(st.st_pitch.p + st.rk_heave.p * c.surface_rock * 25.0f * D2R, -0.6f * D2R, 0.6f * D2R);
    st.surf_roll  = h_clamp(st.st_roll.p + st.rk_roll.p * c.surface_rock_roll_deg * D2R, -0.6f * D2R, 0.6f * D2R);
}

// Neck reflex: compensates part of the cab tilt and quick chassis rotations (bumps, entering and
// leaving turns). Slow, sustained chassis tilt (grades, road camber) is not compensated.
static inline void stabilize_update(const HeadCfg &c, HeadState &st, const HeadIn &in, float dt)
{
    const float D2R = 0.01745329f;
    const float leak = h_clamp(dt / 0.7f, 0, 1);
    st.ch_pitch = h_clamp(st.ch_pitch + in.body_pitch_rate * dt - st.ch_pitch * leak, -0.2f, 0.2f);
    st.ch_roll  = h_clamp(st.ch_roll + in.body_roll_rate * dt - st.ch_roll * leak, -0.2f, 0.2f);
    const float lim = c.stabilize_max_deg * D2R;
    const float tp = h_clamp(-c.stabilize_pitch * (in.cab_pitch + st.ch_pitch), -lim, lim);
    const float tr = h_clamp(-c.stabilize_roll * (in.cab_roll + st.ch_roll), -lim, lim);
    const float f = 1.0f / (6.2831853f * (c.stabilize_time > 0.01f ? c.stabilize_time : 0.01f));
    spring_free(st.stab_p, tp, f, 1.0f, dt);
    spring_free(st.stab_r, tr, f, 1.0f, dt);
}

static inline void head_update(const HeadCfg &c, HeadState &st, const HeadIn &in, float dt, HeadOut &o)
{
    const float D2R = 0.01745329f;
    if (dt <= 0) dt = 0;
    if (dt > 0.1f) dt = 0.1f;
    st.t += dt;
    const float kmh = fabsf(in.speed_ms) * 3.6f;

    const float low = 1.0f - h_clamp((kmh - 8.0f) / (c.look_steer_until - 8.0f), 0, 1);
    const float high = h_clamp((kmh - c.look_ahead_from) / 20.0f, 0, 1);
    float target = c.look_steer_deg * D2R * in.steer * low
                 + c.look_ahead_time * in.yaw_rate * high * (in.speed_ms >= 0 ? 1.f : -1.f);
    const float lm = c.look_max_deg * D2R;
    target = h_clamp(target, -lm, lm);
    if (dt > 0) {
        const float w = 2.0f / (c.look_smooth > 0.02f ? c.look_smooth : 0.02f);
        const int n = dt > 0.008f ? (int)ceilf(dt / 0.008f) : 1;
        const float h = dt / n;
        for (int i = 0; i < n; i++) {
            const float a = w * w * (target - st.look) - 2 * w * st.look_v;
            st.look_v += a * h;
            st.look += st.look_v * h;
        }
    }

    // The body sits in the cab: besides the chassis it feels the cab tilting (gravity)
    // and rocking (angular acceleration at head height).
    st.alpha_p += (in.cab_alpha_pitch - st.alpha_p) * h_clamp(dt / 0.12f, 0, 1);
    st.alpha_r += (in.cab_alpha_roll - st.alpha_r) * h_clamp(dt / 0.12f, 0, 1);
    const float G = 9.81f, ci = c.cabin_inertia;
    const float ax = in.ax + ci * (G * sinf(in.cab_roll) - st.alpha_r * in.head_height);
    const float az = in.az + ci * (-G * sinf(in.cab_pitch) + st.alpha_p * in.head_height);
    st.ay_avg += (in.ay - st.ay_avg) * (dt / 2.0f > 1 ? 1 : dt / 2.0f);
    spring_step(st.sx, ax, c.lean_side, c.body_freq, c.body_damp, dt);
    spring_step(st.sz, az, c.lean_fwd, c.body_freq, c.body_damp, dt);
    spring_step(st.sy, in.ay - st.ay_avg, c.bounce_up, c.bounce_freq, c.body_damp, dt);
    st.sx.p = h_clamp(st.sx.p, -c.max_offset, c.max_offset);
    st.sz.p = h_clamp(st.sz.p, -c.max_offset, c.max_offset);
    st.sy.p = h_clamp(st.sy.p, -c.max_offset * 0.5f, c.max_offset * 0.5f);

    if (dt > 0) {
        surface_update(c, st, in, kmh, dt);
        stabilize_update(c, st, in, dt);
    }

    const float breath = sinf(6.2831853f * st.t / (c.breath_period > 0.5f ? c.breath_period : 0.5f));
    st.sw_timer -= dt;
    if (st.sw_timer <= 0) {
        st.sw_timer = 1.5f + 1.5f * (rnd(st.rng) + 1);
        st.sw_ty = rnd(st.rng); st.sw_tr = rnd(st.rng); st.sw_tx = rnd(st.rng);
    }
    {
        const float k = h_clamp(dt / 1.6f, 0, 1);
        st.sw_y += (st.sw_ty - st.sw_y) * k; st.sw_r += (st.sw_tr - st.sw_r) * k; st.sw_x += (st.sw_tx - st.sw_x) * k;
    }

    o.yaw   = st.look + st.sw_y * c.idle_sway_deg * D2R;
    o.x     = c.seat_right + st.sx.p + st.sw_x * c.idle_sway_m - st.surf_roll * 1.1f;
    o.z     = c.seat_back + st.sz.p;
    o.y     = c.seat_up + st.sy.p + breath * c.breath_up + st.surf_y;
    o.roll  = -st.sx.p * 10.0f * c.roll_from_side_deg * D2R + st.sw_r * c.idle_sway_deg * 0.6f * D2R + st.surf_roll + st.stab_r.p;
    o.pitch = c.seat_pitch_deg * D2R + st.sz.p * 10.0f * c.pitch_from_fwd_deg * D2R
            + breath * c.breath_pitch_deg * D2R + st.surf_pitch + st.stab_p.p;
}
