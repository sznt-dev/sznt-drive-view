#include <cstdio>
#include <cmath>
#include "drive_model.h"
namespace reference {
#include "fixtures/drive_model_reference.h"
}

// The hand-tuned values (the ones in sznt-drive.ini), applied to the frozen reference model.
static reference::HConfig reference_config(const HConfig &c)
{
    reference::HConfig r;
#define K(n) r.n = c.n;
    K(speed_low_kmh) K(speed_high_kmh) K(steer_rate_low) K(steer_rate_high) K(steer_ramp_time) K(steer_start_high)
    K(steer_hold_high) K(steer_start_highway) K(highway_kmh) K(counter_mult) K(lock_low) K(lock_high) K(center_rate)
    K(center_spring) K(center_speed_ref) K(center_delay) K(center_ramp) K(center_ramp_high) K(center_fast_kmh)
    K(center_curve) K(coast_time) K(center_stopped) K(steer_smooth) K(throttle_rise) K(throttle_fall) K(throttle_curve)
    K(throttle_ease_out) K(pedal_memory) K(memory_rise) K(boost_rise) K(boost_curve) K(tap_window) K(brake_rise)
    K(brake_fall) K(brake_curve) K(brake_start) K(brake_boost_rise) K(brake_boost_curve)
#undef K
    return r;
}

int main()
{
    HConfig c;
    struct { const char *name; int trailers; float cargo; } cases[] = {
        {"tractor only", 0, 0}, {"empty trailer", 1, 0}, {"10 t cargo", 1, 10}, {"24.5 t cargo", 1, 24.5f},
        {"40 t cargo", 1, 40}, {"80 t heavy haul", 1, 80}, {"double 2x 18 t", 2, 36},
    };
    for (auto &cs : cases) {
        const float load = h_load(c, cs.trailers, cs.cargo);
        const HConfig w = h_adjusted(c, load, 1.0f);
        HState s;
        float t = 0;
        while (s.thr_out < 0.999f && t < 30) { h_update(c, s, 0.01f, 0, 0, 1, 0, 0, load); t += 0.01f; }
        printf("%-20s %5.1f t  x%.3f  throttle 100%% in %4.1f s  steering %+3.0f%%  brake %.2f s\n", cs.name,
               load * c.weight_reference_t, load, t, (w.steer_rate_high / c.steer_rate_high - 1) * 100, w.brake_rise);
    }

    const reference::HConfig rc = reference_config(c);
    reference::HState ref;
    HState same;
    bool l = 0, r = 0, th = 0, b = 0;
    float v = 0, maxd = 0;
    unsigned seed = 7;
    auto rnd = [&]() { seed = seed * 1664525u + 1013904223u; return (seed >> 8) % 1000; };
    for (int i = 0; i < 100000; i++) {
        if (rnd() % 40 == 0) l = rnd() % 3 == 0;
        if (rnd() % 40 == 0) r = !l && rnd() % 3 == 0;
        if (rnd() % 30 == 0) th = rnd() % 2;
        if (rnd() % 50 == 0) b = !th && rnd() % 4 == 0;
        v = fminf(100, fmaxf(0, v + (th ? 0.05f : 0) - (b ? 0.1f : 0.01f)));
        const float dt = 0.005f + (rnd() % 100) * 0.0003f;
        reference::h_update(rc, ref, dt, l, r, th, b, v);
        h_update(c, same, dt, l, r, th, b, v, h_load(c, 1, 24.5f), h_grip(c, 1.0f, false));
        maxd = fmaxf(maxd, fabsf(ref.steer_out - same.steer_out) + fabsf(ref.thr_out - same.thr_out) + fabsf(ref.brk_out - same.brk_out));
    }
    printf("heavy load on dry asphalt vs the frozen hand-tuned model (100k random frames): max difference %g\n", maxd);

    struct { const char *name; float surface; bool rain; } grips[] = {
        {"dry asphalt", 1.0f, false}, {"wet asphalt", 1.0f, true}, {"gravel", 0.7f, false},
        {"snow", 0.4f, false}, {"ice", 0.2f, false},
    };
    printf("Wheel released at 60 km/h after a turn (position after 1 s / 2 s; 0 = center):\n");
    for (auto &g : grips) {
        const float grip = h_grip(c, g.surface, g.rain);
        HState s;
        for (int i = 0; i < 300; i++) h_update(c, s, 0.01f, 0, 0, 0, 0, 60, 1.0f, grip);
        for (int i = 0; i < 150; i++) h_update(c, s, 0.01f, 0, 1, 0, 0, 60, 1.0f, grip);
        const float start = s.steer;
        float p1 = 0, p2 = 0;
        for (int i = 1; i <= 200; i++) {
            h_update(c, s, 0.01f, 0, 0, 0, 0, 60, 1.0f, grip);
            if (i == 100) p1 = s.steer;
            if (i == 200) p2 = s.steer;
        }
        printf("  %-16s grip %.2f  wheel %.3f -> %.3f / %.3f\n", g.name, grip, start, p1, p2);
    }
    return maxd > 1e-6f;
}
