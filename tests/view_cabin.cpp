#include <cstdio>
#include <cmath>
#include "view_model.h"

static const float D2R = 0.01745329f, R2D = 57.29578f;

struct Run { float pitch_view_max = 0, pitch_view_min = 0, z_min = 0, z_max = 0, roll_view_max = 0, x_max = 0, neck_roll = 0, neck_pitch = 0; };

// What the camera sees = cab tilt + what the head compensates.
static Run simulate(float stab_pitch, float stab_roll, float cabin_inertia, float roll_from_side, bool braking)
{
    HeadCfg c;
    c.breath_up = 0; c.breath_pitch_deg = 0; c.idle_sway_deg = 0; c.idle_sway_m = 0; c.surface_feel = 0;
    c.look_steer_deg = 0; c.look_ahead_time = 0;
    c.stabilize_pitch = stab_pitch; c.stabilize_roll = stab_roll; c.cabin_inertia = cabin_inertia; c.roll_from_side_deg = roll_from_side;
    HeadState st; HeadIn in; HeadOut o; Run r;
    in.speed_ms = 60 / 3.6f;
    const float dt = 1 / 60.0f;
    float prev_p = 0, prev_r = 0, cp = 0, cv = 0;
    for (int f = 0; f < 60 * 6; f++) {
        const float t = f * dt;
        float cab_p = 0, cab_r = 0;
        if (braking) {
            const float on = t > 1 && t < 4 ? 1.f : 0.f;
            in.az = on * 4.0f;
            const float target = -1.5f * D2R * on;
            const float w = 6.2831853f * 1.2f;
            const float a = w * w * (target - cp) - 2 * 0.35f * w * cv;
            cv += a * dt; cp += cv * dt;
            cab_p = cp;
            in.cab_alpha_pitch = a;
        } else {
            const float on = t > 1 && t < 4 ? 1.f : 0.f;
            in.ax = -on * 2.5f;
            const float target = -2.0f * D2R * on;
            const float w = 6.2831853f * 1.0f;
            const float a = w * w * (target - cp) - 2 * 0.4f * w * cv;
            cv += a * dt; cp += cv * dt;
            cab_r = cp;
            in.cab_alpha_roll = a;
        }
        in.cab_pitch = cab_p; in.cab_roll = cab_r;
        head_update(c, st, in, dt, o);
        const float seen_p = (cab_p + o.pitch) * R2D, seen_r = (cab_r + o.roll) * R2D;
        r.pitch_view_max = fmaxf(r.pitch_view_max, seen_p); r.pitch_view_min = fminf(r.pitch_view_min, seen_p);
        r.roll_view_max = fmaxf(r.roll_view_max, fabsf(seen_r));
        r.z_min = fminf(r.z_min, o.z - c.seat_back); r.z_max = fmaxf(r.z_max, o.z - c.seat_back);
        r.x_max = fmaxf(r.x_max, fabsf(o.x));
        r.neck_roll = fmaxf(r.neck_roll, fabsf(o.roll * R2D));
        r.neck_pitch = fmaxf(r.neck_pitch, fabsf((o.pitch - c.seat_pitch_deg * D2R) * R2D));
        prev_p = seen_p; prev_r = seen_r;
    }
    (void)prev_p; (void)prev_r;
    return r;
}

static void report(const char *name, const Run &r, bool braking)
{
    if (braking) printf("  %-8s head vs cab: pitch max %.2f deg | camera tilt [%+.2f %+.2f] deg | body forward %.1f cm\n", name, r.neck_pitch, r.pitch_view_min, r.pitch_view_max, -r.z_min * 100);
    else printf("  %-8s head vs cab: roll max %.2f deg | camera roll max %.2f deg | body sideways %.1f cm\n", name, r.neck_roll, r.roll_view_max, r.x_max * 100);
}

int main()
{
    HeadCfg d;
    for (int braking = 1; braking >= 0; braking--) {
        printf(braking ? "Hard braking (cab pitches 1.5 deg forward and rocks):\n" : "Turn (cab rolls 2 deg outwards):\n");
        report("off", simulate(0, 0, 0, d.roll_from_side_deg, braking), braking);
        report("beta.1", simulate(0.5f, 0.6f, 0.7f, 3.0f, braking), braking);
        report("now", simulate(d.stabilize_pitch, d.stabilize_roll, d.cabin_inertia, d.roll_from_side_deg, braking), braking);
    }
    return 0;
}