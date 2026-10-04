#include <cstdio>
#include <cmath>
#include "view_model.h"

struct Truck {
    Wheels w; WheelTrack tr; HeadCfg c; HeadState st; HeadOut o; HeadIn in;
    Truck()
    {
        const float xs[6] = {-1.0f, 1.0f, -0.9f, 0.9f, -0.9f, 0.9f}, zs[6] = {-3.6f, -3.6f, 0.0f, 0.0f, 1.35f, 1.35f};
        w.n = 6;
        for (int i = 0; i < 6; i++) { w.x[i] = xs[i]; w.z[i] = zs[i]; w.ground[i] = true; w.rough[i] = 0; }
    }
};

struct Stats { float ymin = 0, ymax = 0, pmin = 0, pmax = 0, rmin = 0, rmax = 0, pvel = 0, yvel = 0; int zc = 0; float last_y = 0; };

static void run(const char *name, float kmh, float secs, float (*rough_at)(int wheel, float t, float z, float kmh), bool print_series)
{
    Truck T;
    T.c.breath_up = 0; T.c.breath_pitch_deg = 0; T.c.idle_sway_deg = 0; T.c.idle_sway_m = 0;
    T.in.speed_ms = kmh / 3.6f;
    const float dt = 1.0f / 60.0f;
    Stats s; float prev_y = 0, prev_p = 0;
    float rms_y = 0; int rms_n = 0;
    for (int f = 0; f * dt < secs; f++) {
        const float t = f * dt;
        for (int i = 0; i < 6; i++) T.w.rough[i] = rough_at(i, t, T.w.z[i], kmh);
        wheels_input(T.w, T.tr, dt, T.in);
        head_update(T.c, T.st, T.in, dt, T.o);
        const float y = (T.o.y - T.c.seat_up) * 1000, p = T.o.pitch * 57.2958f, r = T.o.roll * 57.2958f;
        if (t > 0.5f) {
            s.ymin = fminf(s.ymin, y); s.ymax = fmaxf(s.ymax, y);
            s.pmin = fminf(s.pmin, p); s.pmax = fmaxf(s.pmax, p);
            s.rmin = fminf(s.rmin, r); s.rmax = fmaxf(s.rmax, r);
            s.pvel = fmaxf(s.pvel, fabsf(p - prev_p) / dt);
            s.yvel = fmaxf(s.yvel, fabsf(y - prev_y) / dt);
            if ((y > 0) != (s.last_y > 0)) s.zc++;
            s.last_y = y;
        }
        if (t > secs * 0.5f) { rms_y += y * y; rms_n++; }
        prev_y = y; prev_p = p;
        if (print_series && f % 3 == 0 && t > 1.9f && t < 3.2f)
            printf("   t=%.2f y %+5.2f mm  pitch %+5.3f  roll %+5.3f deg  rough %.2f\n", t, y, p, r, T.st.rough);
    }
    printf("%-38s y [%+.2f %+.2f] mm  pitch [%+.3f %+.3f]  roll [%+.3f %+.3f] deg  max vel: %.1f mm/s %.2f deg/s  rms(2nd half) %.2f mm  ~%.1f Hz\n",
           name, s.ymin, s.ymax, s.pmin, s.pmax, s.rmin, s.rmax, s.yvel, s.pvel, sqrtf(rms_y / (rms_n ? rms_n : 1)), s.zc / 2.0f / (secs - 0.5f));
}

static float delay(float z, float kmh) { return (z + 3.6f) / (kmh / 3.6f); }
static float all_gravel(int, float t, float z, float kmh) { return t >= 2.0f + delay(z, kmh) ? 0.7f : 0.0f; }
static float all_gravel_back(int, float t, float z, float kmh) { return (t >= 2.0f + delay(z, kmh) && t < 5.0f + delay(z, kmh)) ? 0.7f : 0.0f; }
static float right_shoulder(int i, float t, float z, float kmh) { return (i % 2 == 1 && t >= 2.0f + delay(z, kmh)) ? 0.8f : 0.0f; }
static float flicker(int i, float t, float, float) { return (i == 0 && t > 2 && t < 4 && ((int)(t / 0.03f)) % 2) ? 0.7f : 0.0f; }
static float concrete(int, float t, float z, float kmh) { return t >= 2.0f + delay(z, kmh) ? 0.18f : 0.0f; }
static float grass(int, float t, float, float) { return t > 1 ? 0.9f : 0.0f; }
static float asphalt(int, float, float, float) { return 0.0f; }

int main()
{
    printf("Asphalt -> gravel at 60 km/h (series):\n");
    run("asphalt->gravel 60 km/h", 60, 6, all_gravel, true);
    run("asphalt->gravel 30 km/h", 30, 6, all_gravel, false);
    run("asphalt->gravel 10 km/h", 10, 6, all_gravel, false);
    run("gravel for 3 s, back to asphalt 60", 60, 8, all_gravel_back, false);
    run("right wheels on the shoulder 60", 60, 6, right_shoulder, false);
    run("asphalt->concrete 90 km/h", 90, 6, concrete, false);
    run("edge flickering every 30 ms 60", 60, 6, flicker, false);
    run("grass 40 km/h", 40, 20, grass, false);
    run("grass 70 km/h", 70, 20, grass, false);
    run("asphalt only 80 km/h", 80, 10, asphalt, false);
    return 0;
}
