// SZNT Drive - realistic keyboard steering, pedals, weight and grip for ETS2/ATS.
#include "sznt_common.h"
#include <cmath>

#include "scssdk_input.h"
#include "scssdk_telemetry.h"
#include "common/scssdk_telemetry_common_configs.h"
#include "common/scssdk_telemetry_common_channels.h"
#include "common/scssdk_telemetry_truck_common_channels.h"
#include "common/scssdk_telemetry_trailer_common_channels.h"
#include "drive_model.h"

static scs_log_t g_log = nullptr;
static HConfig g_cfg, g_cfg_pending;
static HState g_st;
static CRITICAL_SECTION g_cfg_lock;
static volatile LONG g_cfg_dirty = 0;

static int g_key_left = 'A', g_key_right = 'D', g_key_thr = 'W', g_key_brk = 'S';
static int g_enabled = 1, g_output_mode = 0, g_debug = 0;

static volatile float g_speed_ms = 0;
static volatile LONG g_paused = 1, g_stop = 0;
static sznt::WalkCompat g_walk;
static HANDLE g_thread = nullptr;
static std::wstring g_ini_path, g_log_path;
static LARGE_INTEGER g_qpf, g_last_tick;
static bool g_gen_active = false;
static float g_dbg_timer = 0;

static const int MAX_TRAILERS = 10;
static volatile LONG g_trailer_on[MAX_TRAILERS] = {0};
static volatile LONG g_legacy_trailer = 0;
static bool g_indexed_trailers = false;
static float g_cargo_t = 0;
static int g_cargo_loaded = 0;
static float g_logged_total = -1;

static const int MAXW = 14, MAXS = 64;
static float g_subst_grip[MAXS];
static int g_nsubst = 0;
static int g_wheels = 0;
static bool g_steerable[MAXW];
static bool g_ground[MAXW];
static int g_subst[MAXW];
static bool g_wipers = false;

static void logf_(scs_log_type_t t, const char *fmt, ...)
{
    if (!g_log) return;
    char buf[600], out[640];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    snprintf(out, sizeof(out), "[SZNT Drive] %s", buf);
    g_log(t, out);
}

static void load_ini_now()
{
    HConfig c;
    int kl = 'A', kr = 'D', kt = 'W', kb = 'S', en = 1, mode = 0, dbg = 0;
    sznt::read_ini(g_ini_path, [&](const char *k, const char *v) {
        const float fv = sznt::to_f(v);
#define F(name) else if (!strcmp(k, #name)) c.name = fv;
        if (0) {}
        F(speed_low_kmh) F(speed_high_kmh) F(steer_rate_low) F(steer_rate_high) F(steer_ramp_time)
        F(steer_start_high) F(steer_hold_high) F(steer_start_highway) F(highway_kmh) F(counter_mult)
        F(lock_low) F(lock_high) F(center_rate) F(center_spring) F(center_speed_ref) F(center_delay)
        F(center_ramp) F(center_ramp_high) F(center_fast_kmh) F(center_curve) F(coast_time) F(center_stopped)
        F(steer_smooth) F(throttle_rise) F(throttle_fall) F(throttle_curve) F(throttle_ease_out) F(pedal_memory)
        F(memory_rise) F(boost_rise) F(boost_curve) F(tap_window) F(brake_rise) F(brake_fall) F(brake_curve)
        F(brake_start) F(brake_boost_rise) F(brake_boost_curve)
        F(weight_enabled) F(weight_reference_t) F(truck_mass_t) F(trailer_mass_t) F(weight_override_t)
        F(weight_steer) F(weight_throttle) F(weight_brake) F(weight_smooth)
        F(grip_enabled) F(grip_center) F(grip_light) F(grip_rain) F(grip_smooth)
#undef F
        else if (!strcmp(k, "key_left")) kl = sznt::to_i(v);
        else if (!strcmp(k, "key_right")) kr = sznt::to_i(v);
        else if (!strcmp(k, "key_throttle")) kt = sznt::to_i(v);
        else if (!strcmp(k, "key_brake")) kb = sznt::to_i(v);
        else if (!strcmp(k, "enabled")) en = sznt::to_i(v);
        else if (!strcmp(k, "output")) mode = sznt::to_i(v);
        else if (!strcmp(k, "debug")) dbg = sznt::to_i(v);
    });
    if (c.throttle_rise < 0.01f) c.throttle_rise = 0.01f;
    if (c.throttle_fall < 0.01f) c.throttle_fall = 0.01f;
    if (c.brake_rise < 0.01f) c.brake_rise = 0.01f;
    if (c.brake_fall < 0.01f) c.brake_fall = 0.01f;
    if (c.speed_high_kmh <= c.speed_low_kmh + 1) c.speed_high_kmh = c.speed_low_kmh + 1;
    if (c.center_speed_ref < 0.1f) c.center_speed_ref = 0.1f;
    c.weight_steer = clampf(c.weight_steer, 0, 1);
    c.weight_throttle = clampf(c.weight_throttle, 0, 1);
    c.weight_brake = clampf(c.weight_brake, 0, 1);

    EnterCriticalSection(&g_cfg_lock);
    g_cfg_pending = c;
    g_key_left = kl; g_key_right = kr; g_key_thr = kt; g_key_brk = kb;
    g_enabled = en; g_output_mode = mode; g_debug = dbg;
    LeaveCriticalSection(&g_cfg_lock);
    InterlockedExchange(&g_cfg_dirty, 1);
}

static DWORD WINAPI worker(LPVOID)
{
    FILETIME last_ini = sznt::file_time(g_ini_path);
    sznt::LogTail tail;
    for (int tick = 0; !g_stop; tick++) {
        if (tick % 10 == 0) {
            FILETIME ft = sznt::file_time(g_ini_path);
            if (CompareFileTime(&ft, &last_ini) != 0) { last_ini = ft; Sleep(100); load_ini_now(); }
        }
        tail.poll(g_log_path, [](const std::string &l) { g_walk.on_line(l); });
        Sleep(50);
    }
    return 0;
}

static int trailers_connected()
{
    if (!g_indexed_trailers) return g_legacy_trailer ? 1 : 0;
    int n = 0;
    for (int i = 0; i < MAX_TRAILERS; i++) n += g_trailer_on[i] ? 1 : 0;
    return n;
}

static float current_load()
{
    const int n = trailers_connected();
    const float cargo = g_cargo_loaded ? g_cargo_t : 0.f;
    const float load = h_load(g_cfg, n, cargo);
    const float total = load * g_cfg.weight_reference_t;
    if (g_cfg.weight_enabled >= 0.5f && fabsf(total - g_logged_total) > 0.25f) {
        g_logged_total = total;
        logf_(SCS_LOG_TYPE_message, "weight: %d trailer(s), cargo %.1f t, total %.1f t (x%.2f)", n, n ? cargo : 0.f, total, load);
    }
    return load;
}

static float current_grip()
{
    const int n = g_wheels > MAXW ? MAXW : g_wheels;
    float sum = 0;
    int cnt = 0;
    for (int i = 0; i < n; i++) {
        if (!g_steerable[i] || !g_ground[i]) continue;
        const int s = g_subst[i];
        sum += (s >= 0 && s < g_nsubst) ? g_subst_grip[s] : 1.0f;
        cnt++;
    }
    return h_grip(g_cfg, cnt ? sum / cnt : 1.0f, g_wipers);
}

static void frame_update()
{
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    float dt = (float)(now.QuadPart - g_last_tick.QuadPart) / (float)g_qpf.QuadPart;
    if (dt < 0.0005f) return;
    g_last_tick = now;

    if (g_cfg_dirty) {
        EnterCriticalSection(&g_cfg_lock); g_cfg = g_cfg_pending; LeaveCriticalSection(&g_cfg_lock);
        InterlockedExchange(&g_cfg_dirty, 0);
        g_logged_total = -1;
        logf_(SCS_LOG_TYPE_message, "settings loaded (enabled=%d, output=%d)", g_enabled, g_output_mode);
    }

    const bool live = g_enabled && !g_paused && !g_walk.walking && sznt::game_in_front();
    const bool l = live && sznt::key_down(g_key_left), r = live && sznt::key_down(g_key_right);
    const bool t = live && sznt::key_down(g_key_thr), b = live && sznt::key_down(g_key_brk);
    const float kmh = fabsf(g_speed_ms) * 3.6f;
    if (g_paused) dt = 0;
    h_update(g_cfg, g_st, dt, l, r, t, b, kmh, current_load(), current_grip());

    if (g_debug && dt > 0) {
        g_dbg_timer += dt;
        const bool busy = l || r || t || b || fabsf(g_st.steer_out) > 0.002f || g_st.thr_out > 0.002f || g_st.brk_out > 0.002f;
        if (g_dbg_timer > (busy ? 0.25f : 2.0f)) {
            g_dbg_timer = 0;
            logf_(SCS_LOG_TYPE_message, "dbg %.1f km/h keys[%c%c%c%c] steer %+.3f thr %.2f brk %.2f load x%.2f grip %.2f%s walk %ld out=%s",
                  kmh, l ? 'A' : '-', r ? 'D' : '-', t ? 'W' : '-', b ? 'S' : '-',
                  g_st.steer_out, g_st.thr_out, g_st.brk_out, g_st.load, g_st.grip, g_wipers ? " rain" : "", g_walk.walking,
                  g_gen_active && g_output_mode != 2 ? "joy" : "semantical");
        }
    }
}

static const scs_input_device_input_t g_gen_inputs[] = {
    { "sz_steer", "Steering", SCS_VALUE_TYPE_float },
    { "sz_throttle", "Throttle", SCS_VALUE_TYPE_float },
    { "sz_brake", "Brake", SCS_VALUE_TYPE_float },
};
static const scs_input_device_input_t g_sem_inputs[] = {
    { "steering", "Steering", SCS_VALUE_TYPE_float },
    { "aforward", "Throttle", SCS_VALUE_TYPE_float },
    { "abackward", "Brake", SCS_VALUE_TYPE_float },
};

struct Dev { const char *tag; bool generic; float sent[3]; unsigned next; };
static Dev g_gen = { "joystick", true, {-9, -9, -9}, 0 };
static Dev g_sem = { "semantical", false, {-9, -9, -9}, 0 };

static bool dev_is_output(const Dev *d)
{
    if (g_output_mode == 1) return d->generic;
    if (g_output_mode == 2) return !d->generic;
    return d->generic ? g_gen_active : !g_gen_active;
}

SCSAPI_VOID dev_active(const scs_u8_t active, const scs_context_t ctx)
{
    Dev *d = (Dev *)ctx;
    if (d->generic) g_gen_active = active != 0;
    for (float &s : d->sent) s = -9;
    logf_(SCS_LOG_TYPE_message, "device %s %s", d->tag, active ? "active" : "inactive");
}

SCSAPI_RESULT dev_event(scs_input_event_t *const ev, const scs_u32_t flags, const scs_context_t ctx)
{
    Dev *d = (Dev *)ctx;
    if (flags & SCS_INPUT_EVENT_CALLBACK_FLAG_first_after_activation) for (float &s : d->sent) s = -9;
    if (flags & SCS_INPUT_EVENT_CALLBACK_FLAG_first_in_frame) { frame_update(); d->next = 0; }

    const bool out = dev_is_output(d);
    const float vals[3] = { out ? g_st.steer_out : 0.f, out ? g_st.thr_out : 0.f, out ? g_st.brk_out : 0.f };
    while (d->next < 3) {
        const unsigned i = d->next++;
        if (fabsf(vals[i] - d->sent[i]) > 0.0005f || (vals[i] == 0.f && d->sent[i] != 0.f)) {
            d->sent[i] = vals[i];
            ev->input_index = i;
            ev->value_float.value = vals[i];
            return SCS_RESULT_ok;
        }
    }
    return SCS_RESULT_not_found;
}

static bool reg_dev(const scs_input_init_params_v100_t *p, const char *name, const char *disp,
                    scs_input_device_type_t type, const scs_input_device_input_t *in, Dev *d)
{
    scs_input_device_t dev; memset(&dev, 0, sizeof(dev));
    dev.name = name; dev.display_name = disp; dev.type = type;
    dev.input_count = 3; dev.inputs = in; dev.callback_context = d;
    dev.input_active_callback = dev_active; dev.input_event_callback = dev_event;
    if (p->register_device(&dev) != SCS_RESULT_ok) { logf_(SCS_LOG_TYPE_error, "could not register %s", name); return false; }
    return true;
}

extern "C" __declspec(dllexport)
SCSAPI_RESULT scs_input_init(const scs_u32_t version, const scs_input_init_params_t *const params)
{
    if (version != SCS_INPUT_VERSION_1_00) return SCS_RESULT_unsupported;
    const scs_input_init_params_v100_t *p = static_cast<const scs_input_init_params_v100_t *>(params);
    g_log = p->common.log;
    InitializeCriticalSection(&g_cfg_lock);
    QueryPerformanceFrequency(&g_qpf); QueryPerformanceCounter(&g_last_tick);

    g_ini_path = sznt::module_dir() + L"\\sznt-drive.ini";
    g_log_path = sznt::game_log_path(p->common.game_id);
    load_ini_now();

    const bool a = reg_dev(p, "sznt_drive", "SZNT Drive", SCS_INPUT_DEVICE_TYPE_generic, g_gen_inputs, &g_gen);
    const bool b = reg_dev(p, "sznt_drive_sem", "SZNT Drive (semantical)", SCS_INPUT_DEVICE_TYPE_semantical, g_sem_inputs, &g_sem);
    if (!a && !b) return SCS_RESULT_generic_error;

    g_stop = 0;
    g_thread = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
    logf_(SCS_LOG_TYPE_message, "v%s ready - keys %c %c %c %c", SZNT_VERSION, g_key_left, g_key_right, g_key_thr, g_key_brk);
    return SCS_RESULT_ok;
}

extern "C" __declspec(dllexport)
SCSAPI_VOID scs_input_shutdown(void)
{
    InterlockedExchange(&g_stop, 1);
    if (g_thread) { WaitForSingleObject(g_thread, 2000); CloseHandle(g_thread); g_thread = nullptr; }
}

SCSAPI_VOID tel_speed(const scs_string_t, const scs_u32_t, const scs_value_t *const v, const scs_context_t)
{
    g_speed_ms = v ? v->value_float.value : 0.f;
}

SCSAPI_VOID tel_trailer(const scs_string_t, const scs_u32_t, const scs_value_t *const v, const scs_context_t ctx)
{
    const LONG on = v && v->value_bool.value ? 1 : 0;
    const size_t i = (size_t)ctx;
    if (i < (size_t)MAX_TRAILERS) InterlockedExchange(&g_trailer_on[i], on);
    else InterlockedExchange(&g_legacy_trailer, on);
}

SCSAPI_VOID tel_wheel_ground(const scs_string_t, const scs_u32_t i, const scs_value_t *const v, const scs_context_t)
{ if (i < (scs_u32_t)MAXW) g_ground[i] = v && v->value_bool.value; }
SCSAPI_VOID tel_wheel_subst(const scs_string_t, const scs_u32_t i, const scs_value_t *const v, const scs_context_t)
{ if (i < (scs_u32_t)MAXW) g_subst[i] = v ? (int)v->value_u32.value : 0; }
SCSAPI_VOID tel_wipers(const scs_string_t, const scs_u32_t, const scs_value_t *const v, const scs_context_t)
{ g_wipers = v && v->value_bool.value; }

SCSAPI_VOID tel_config(const scs_event_t, const void *const info, const scs_context_t)
{
    const scs_telemetry_configuration_t *c = (const scs_telemetry_configuration_t *)info;
    if (!c || !c->id) return;
    if (!strcmp(c->id, SCS_TELEMETRY_CONFIG_job)) {
        float mass = 0; int loaded = 1; bool any = false;
        for (const scs_named_value_t *a = c->attributes; a && a->name; a++) {
            any = true;
            if (!strcmp(a->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_cargo_mass)) mass = a->value.value_float.value;
            else if (!strcmp(a->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_is_cargo_loaded)) loaded = a->value.value_bool.value ? 1 : 0;
        }
        g_cargo_t = any ? mass / 1000.0f : 0.f;
        g_cargo_loaded = any ? loaded : 0;
    } else if (!strcmp(c->id, SCS_TELEMETRY_CONFIG_substances)) {
        int n = 0;
        for (const scs_named_value_t *a = c->attributes; a && a->name; a++) {
            if (strcmp(a->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_id) || a->index >= (scs_u32_t)MAXS) continue;
            g_subst_grip[a->index] = sznt::surface_of(a->value.value_string.value ? a->value.value_string.value : "").grip;
            if ((int)a->index + 1 > n) n = a->index + 1;
        }
        g_nsubst = n;
    } else if (!strcmp(c->id, SCS_TELEMETRY_CONFIG_truck)) {
        int wheels = 0;
        bool steer[MAXW] = {};
        for (const scs_named_value_t *a = c->attributes; a && a->name; a++) {
            if (!strcmp(a->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_wheel_count)) wheels = (int)a->value.value_u32.value;
            else if (!strcmp(a->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_wheel_steerable) && a->index < (scs_u32_t)MAXW)
                steer[a->index] = a->value.value_bool.value != 0;
        }
        memcpy(g_steerable, steer, sizeof(steer));
        g_wheels = wheels > MAXW ? MAXW : wheels;
    }
}

SCSAPI_VOID tel_pause(const scs_event_t ev, const void *const, const scs_context_t)
{
    InterlockedExchange(&g_paused, ev == SCS_TELEMETRY_EVENT_paused ? 1 : 0);
}

extern "C" __declspec(dllexport)
SCSAPI_RESULT scs_telemetry_init(const scs_u32_t version, const scs_telemetry_init_params_t *const params)
{
    if (version != SCS_TELEMETRY_VERSION_1_01 && version != SCS_TELEMETRY_VERSION_1_00) return SCS_RESULT_unsupported;
    const scs_telemetry_init_params_v100_t *p = static_cast<const scs_telemetry_init_params_v100_t *>(params);
    if (!g_log) g_log = p->common.log;
    const scs_u32_t N = SCS_U32_NIL, F0 = SCS_TELEMETRY_CHANNEL_FLAG_none;
    p->register_for_event(SCS_TELEMETRY_EVENT_paused, tel_pause, nullptr);
    p->register_for_event(SCS_TELEMETRY_EVENT_started, tel_pause, nullptr);
    p->register_for_event(SCS_TELEMETRY_EVENT_configuration, tel_config, nullptr);
    p->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_speed, N, SCS_VALUE_TYPE_float, F0, tel_speed, nullptr);
    p->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_wipers, N, SCS_VALUE_TYPE_bool, F0, tel_wipers, nullptr);
    for (scs_u32_t i = 0; i < (scs_u32_t)MAXW; i++) {
        p->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_wheel_on_ground, i, SCS_VALUE_TYPE_bool, F0, tel_wheel_ground, nullptr);
        p->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_wheel_substance, i, SCS_VALUE_TYPE_u32, F0, tel_wheel_subst, nullptr);
    }

    g_indexed_trailers = true;
    for (int i = 0; i < MAX_TRAILERS; i++) {
        char name[48];
        snprintf(name, sizeof(name), "trailer.%d.connected", i);
        if (p->register_for_channel(name, N, SCS_VALUE_TYPE_bool, SCS_TELEMETRY_CHANNEL_FLAG_no_value,
                                    tel_trailer, (scs_context_t)(size_t)i) != SCS_RESULT_ok) {
            g_indexed_trailers = false;
            break;
        }
    }
    if (!g_indexed_trailers)
        p->register_for_channel(SCS_TELEMETRY_TRAILER_CHANNEL_connected, N, SCS_VALUE_TYPE_bool,
                                SCS_TELEMETRY_CHANNEL_FLAG_no_value, tel_trailer, (scs_context_t)(size_t)MAX_TRAILERS);
    return SCS_RESULT_ok;
}

extern "C" __declspec(dllexport)
SCSAPI_VOID scs_telemetry_shutdown(void) {}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
