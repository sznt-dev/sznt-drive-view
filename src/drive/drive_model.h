#pragma once
#include <cmath>

struct HConfig {
    float speed_low_kmh       = 5.0f;
    float speed_high_kmh      = 80.0f;
    float steer_rate_low      = 0.36f;
    float steer_rate_high     = 0.08f;
    float steer_ramp_time     = 2.4f;
    float steer_start_high    = 0.28f;
    float steer_hold_high     = 2.0f;
    float steer_start_highway = 0.33f;
    float highway_kmh         = 60.0f;
    float counter_mult        = 2.5f;
    float lock_low            = 1.00f;
    float lock_high           = 0.15f;
    float center_rate         = 0.12f;
    float center_spring       = 1.6f;
    float center_speed_ref    = 40.0f;
    float center_delay        = 0.0f;
    float center_ramp         = 2.5f;
    float center_ramp_high    = 0.6f;
    float center_fast_kmh     = 30.0f;
    float center_curve        = 1.5f;
    float coast_time          = 0.15f;
    float center_stopped      = 0.0f;
    float steer_smooth        = 0.05f;

    float throttle_rise       = 9.0f;
    float throttle_fall       = 1.2f;
    float throttle_curve      = 1.9f;
    float throttle_ease_out   = 1.6f;
    float pedal_memory        = 3.0f;
    float memory_rise         = 0.8f;
    float boost_rise          = 3.5f;
    float boost_curve         = 1.3f;
    float tap_window          = 0.35f;

    float brake_rise          = 3.0f;
    float brake_fall          = 0.6f;
    float brake_curve         = 1.25f;
    float brake_start         = 0.04f;
    float brake_boost_rise    = 1.3f;
    float brake_boost_curve   = 1.1f;

    float weight_enabled      = 1.0f;
    float weight_reference_t  = 40.0f;
    float truck_mass_t        = 8.5f;
    float trailer_mass_t      = 7.0f;
    float weight_override_t   = 0.0f;
    float weight_steer        = 0.15f;
    float weight_throttle     = 0.25f;
    float weight_brake        = 0.15f;
    float weight_smooth       = 4.0f;

    float grip_enabled        = 1.0f;
    float grip_center         = 0.6f;
    float grip_light          = 0.15f;
    float grip_rain           = 0.85f;
    float grip_smooth         = 0.5f;
};

struct HState {
    float steer = 0, steer_out = 0, hand = 0, released = 0, sv = 0;
    int   last_dir = 0;
    float thr = 0, brk = 0, thr_out = 0, brk_out = 0, thr_mem = 0;
    float t = 0, w_press_t = -10, w_release_t = -10, s_press_t = -10, s_release_t = -10;
    bool  w_prev = false, boost = false, s_prev = false, bboost = false;
    float curve = 0, ease = 0, bcurve = 0;
    float load = -1;
    float grip = 1;
};

static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// Relative weight of the rig: 1.0 = reference mass (heavy load).
static inline float h_load(const HConfig &c, int trailers, float cargo_t)
{
    if (c.weight_enabled < 0.5f) return 1.0f;
    float total = c.truck_mass_t;
    if (trailers > 0) total += trailers * c.trailer_mass_t + (cargo_t > 0 ? cargo_t : 0);
    if (c.weight_override_t > 0) total = c.weight_override_t;
    return clampf(total / (c.weight_reference_t > 1 ? c.weight_reference_t : 1), 0.1f, 3.0f);
}

// Grip under the steered wheels (1 = dry asphalt). In the rain, paved surfaces get a little slippery.
static inline float h_grip(const HConfig &c, float surface_grip, bool rain)
{
    if (c.grip_enabled < 0.5f) return 1.0f;
    float g = clampf(surface_grip, 0.05f, 1.0f);
    if (rain) g *= clampf(c.grip_rain, 0.1f, 1.0f);
    return g;
}

// Weight scales times and rates; low grip makes the wheel light (weaker self-centering).
static inline HConfig h_adjusted(const HConfig &c, float load, float grip)
{
    HConfig w = c;
    const float slip = clampf(1.0f - grip, 0, 1);
    if (slip > 1e-4f) {
        const float center = 1.0f - clampf(c.grip_center, 0, 1) * slip;
        const float light = 1.0f + clampf(c.grip_light, 0, 1) * slip;
        w.center_rate *= center;
        w.center_spring *= center;
        w.steer_rate_low *= light;
        w.steer_rate_high *= light;
    }
    if (load <= 0 || fabsf(load - 1.0f) < 1e-4f) return w;
    const float st = powf(load, -c.weight_steer);
    const float th = powf(load, c.weight_throttle);
    const float br = powf(load, c.weight_brake);
    w.steer_rate_low  *= st;
    w.steer_rate_high *= st;
    w.throttle_rise   *= th;
    w.boost_rise      *= th;
    w.brake_rise      *= br;
    w.brake_boost_rise *= br;
    return w;
}

static inline void h_update(const HConfig &cfg, HState &s, float dt, bool left, bool right,
                            bool thr, bool brk, float speed_kmh, float load = 1.0f, float grip = 1.0f)
{
    if (dt <= 0) return;
    if (dt > 0.1f) dt = 0.1f;

    const float v = fabsf(speed_kmh);
    if (s.load < 0 || v < 3.0f) s.load = load;
    else s.load += (load - s.load) * clampf(dt / (cfg.weight_smooth > 0.05f ? cfg.weight_smooth : 0.05f), 0, 1);
    s.grip += (grip - s.grip) * clampf(dt / (cfg.grip_smooth > 0.05f ? cfg.grip_smooth : 0.05f), 0, 1);
    const HConfig c = h_adjusted(cfg, s.load, s.grip);

    float t = clampf((v - c.speed_low_kmh) / (c.speed_high_kmh - c.speed_low_kmh), 0.f, 1.f);
    t = t * t * (3 - 2 * t);
    const float rate = lerpf(c.steer_rate_low, c.steer_rate_high, t);
    const float lock = lerpf(c.lock_low, c.lock_high, t);

    const int dir = (right ? 1 : 0) - (left ? 1 : 0);
    if (dir != 0) {
        s.released = 0;
        if (dir != s.last_dir) s.hand = 0;
        s.hand = clampf(s.hand + dt / (c.steer_ramp_time > 0.001f ? c.steer_ramp_time : 0.001f), 0, 1);
        const float th = clampf((v - c.highway_kmh) / 20.0f, 0, 1);
        const float start = lerpf(c.steer_start_high, c.steer_start_highway, th * th * (3 - 2 * th));
        const float hh = s.hand;
        const float ease = hh * hh * hh * (hh * (hh * 6 - 15) + 10);
        float r = rate * lerpf(start, c.steer_hold_high, ease);
        if (s.steer * dir < 0) r = rate * c.counter_mult * lerpf(0.4f, 1.0f, s.hand);
        s.sv = dir * r;
        float nxt = s.steer + s.sv * dt;
        if (nxt * dir > lock) { nxt = (s.steer * dir > lock) ? s.steer : dir * lock; s.sv = 0; }
        s.steer = nxt;
    } else {
        s.hand = 0;
        s.released += dt;
        float tc = clampf((v - c.speed_low_kmh) / ((c.center_fast_kmh > c.speed_low_kmh + 1 ? c.center_fast_kmh : c.speed_low_kmh + 1) - c.speed_low_kmh), 0, 1);
        tc = tc * tc * (3 - 2 * tc);
        const float cramp = lerpf(c.center_ramp, c.center_ramp_high, tc);
        float ramp = clampf((s.released - c.center_delay) / (cramp > 0.01f ? cramp : 0.01f), 0, 1);
        ramp = powf(ramp, c.center_curve > 0.1f ? c.center_curve : 0.1f);
        const float f = ramp * (c.center_stopped + (1.0f - c.center_stopped) * (v / (v + c.center_speed_ref)));
        const float want = (s.steer > 0 ? -1.f : (s.steer < 0 ? 1.f : 0.f))
                         * (c.center_rate + c.center_spring * fabsf(s.steer)) * f;
        const float k = c.coast_time > 0.01f ? clampf(dt / c.coast_time, 0, 1) : 1.0f;
        s.sv += (want - s.sv) * k;
        float nxt = s.steer + s.sv * dt;
        if (fabsf(nxt) > lock && fabsf(nxt) > fabsf(s.steer)) { nxt = s.steer; s.sv = 0; }
        if ((s.steer > 0 && nxt < 0) || (s.steer < 0 && nxt > 0)) { nxt = 0; s.sv = 0; }
        s.steer = nxt;
    }
    if (fabsf(s.steer) > lock) {
        const float excess = fabsf(s.steer) - lock;
        const float pull = excess < 0.6f * dt ? excess : 0.6f * dt;
        s.steer -= s.steer > 0 ? pull : -pull;
    }
    s.steer = clampf(s.steer, -1, 1);
    s.last_dir = dir;
    const float a = c.steer_smooth > 0.001f ? 1.0f - expf(-dt / c.steer_smooth) : 1.0f;
    s.steer_out += (s.steer - s.steer_out) * a;

    s.t += dt;
    if (thr && !s.w_prev) {
        const float gap = s.t - s.w_release_t, tap = s.w_release_t - s.w_press_t;
        s.boost = gap <= c.tap_window && tap >= 0 && tap <= c.tap_window;
        s.w_press_t = s.t;
    }
    if (!thr && s.w_prev) { s.w_release_t = s.t; s.boost = false; }
    s.w_prev = thr;
    if (thr) {
        const float rise = (s.thr < s.thr_mem) ? c.memory_rise : (s.boost ? c.boost_rise : c.throttle_rise);
        s.thr = clampf(s.thr + dt / (rise > 0.01f ? rise : 0.01f), 0, 1);
        if (s.thr >= s.thr_mem) s.thr_mem = s.thr;
    } else {
        s.thr_mem = clampf(s.thr_mem - dt * (s.thr_mem / (c.pedal_memory > 0.1f ? c.pedal_memory : 0.1f)) - dt * 0.02f, 0, 1);
        s.thr = clampf(s.thr - dt / c.throttle_fall, 0, 1);
    }

    if (brk && !s.s_prev) {
        const float gap = s.t - s.s_release_t, tap = s.s_release_t - s.s_press_t;
        s.bboost = gap <= c.tap_window && tap >= 0 && tap <= c.tap_window;
        s.s_press_t = s.t;
    }
    if (!brk && s.s_prev) { s.s_release_t = s.t; s.bboost = false; }
    s.s_prev = brk;
    if (brk) s.brk = clampf((s.brk < c.brake_start ? c.brake_start : s.brk) + dt / (s.bboost ? c.brake_boost_rise : c.brake_rise), 0, 1);
    else     s.brk = clampf(s.brk - dt / c.brake_fall, 0, 1);

    const float cwant = s.boost ? c.boost_curve : c.throttle_curve;
    if (s.curve == 0) s.curve = cwant;
    s.curve += (cwant - s.curve) * clampf(dt / 0.6f, 0, 1);
    const float ewant = s.boost ? 1.0f : (c.throttle_ease_out > 1.0f ? c.throttle_ease_out : 1.0f);
    if (s.ease == 0) s.ease = ewant;
    s.ease += (ewant - s.ease) * clampf(dt / 0.6f, 0, 1);
    s.thr_out = 1.0f - powf(1.0f - powf(s.thr, s.curve), s.ease);

    const float bwant = s.bboost ? c.brake_boost_curve : c.brake_curve;
    if (s.bcurve == 0) s.bcurve = bwant;
    s.bcurve += (bwant - s.bcurve) * clampf(dt / 0.4f, 0, 1);
    s.brk_out = powf(s.brk, s.bcurve);
}
