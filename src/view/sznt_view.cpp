// SZNT View - realistic head and gaze in the cab for ETS2/ATS (virtual head tracker).
#include "sznt_common.h"
#include <cmath>

#include "scssdk_input.h"
#include "scssdk_telemetry.h"
#include "common/scssdk_telemetry_common_channels.h"
#include "common/scssdk_telemetry_truck_common_channels.h"
#include "common/scssdk_telemetry_common_configs.h"
#include "view_model.h"

static scs_log_t g_log = nullptr;
static HeadCfg g_cfg, g_cfg_pending;
static HeadState g_st;
static HeadOut g_out;
static HeadIn g_in;
static CRITICAL_SECTION g_lock;
static volatile LONG g_cfg_dirty = 0;
static int g_enabled = 1, g_debug = 0;

static int   g_mouse_look = 1, g_mouse_invert = 0, g_recenter_key = 0;
static float g_mouse_deg_per_px = 0.015f, g_mouse_weight = 0.22f, g_mouse_max_speed = 42.f, g_mouse_accel = 0.5f;
static float g_mouse_yaw_limit = 150.f, g_mouse_up_limit = 45.f, g_mouse_down_limit = 60.f;
static volatile LONG g_mdx = 0, g_mdy = 0;
static DWORD g_hook_tid = 0;
static HANDLE g_hook_thread = nullptr;
static float m_turn_t = 0; static int m_turn_dir = 0;
static float m_tyaw = 0, m_tpitch = 0, m_yaw = 0, m_pitch = 0, m_vyaw = 0, m_vpitch = 0;

static int   g_lean_key = 0x04, g_lean_toggle = 0;
static float g_lean_fwd = 0.30f, g_lean_down = 0.03f, g_lean_pitch = 2.0f, g_lean_time = 0.7f;
static float l_pos = 0, l_vel = 0; static bool l_on = false, l_prev = false;

static const int MAX_CAM_KEYS = 16;
struct CamKeys { int inside[MAX_CAM_KEYS], outside[MAX_CAM_KEYS], n_in, n_out; };
static CamKeys g_cam_keys = { {0x31}, {0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39}, 1, 8 };
static CamKeys g_cam_keys_pending = g_cam_keys;
static bool g_cam_prev[256];
static int g_cam_interior = 1;

static volatile LONG g_paused = 1, g_stop = 0;
static sznt::WalkCompat g_walk;
static HANDLE g_thread = nullptr;
static std::wstring g_ini_path, g_log_path;
static LARGE_INTEGER g_qpf, g_last;
static float g_dbg_t = 0;

static void logf_(scs_log_type_t t, const char *fmt, ...)
{
    if (!g_log) return;
    char b[700], o[740];
    va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof(b), fmt, ap); va_end(ap);
    snprintf(o, sizeof(o), "[SZNT View] %s", b);
    g_log(t, o);
}

// ------------------------------------------------------------------ surfaces
static const int MAXW = HEAD_MAXW, MAXS = 64, MAX_OVR = 32;
struct RoughOverride { char name[40]; float value; };
static RoughOverride g_ovr[MAX_OVR], g_ovr_pending[MAX_OVR];
static int g_novr = 0, g_novr_pending = 0;

static char g_sname[MAXS][40];
static float g_srough[MAXS];
static int g_nsubst = 0;

static float roughness(const char *n)
{
    for (int i = 0; i < g_novr; i++) if (!strcmp(g_ovr[i].name, n)) return h_clamp(g_ovr[i].value, 0, 2);
    return sznt::surface_of(n).rough;
}

static void classify_substances()
{
    for (int i = 0; i < g_nsubst; i++) g_srough[i] = roughness(g_sname[i]);
}

static int g_subst[MAXW];
static Wheels g_wheels;
static WheelTrack g_track;

static void wheels_to_input(HeadIn &in, float dt)
{
    for (int i = 0; i < g_wheels.n && i < MAXW; i++) {
        const int s = g_subst[i];
        g_wheels.rough[i] = (s >= 0 && s < g_nsubst) ? g_srough[s] : 0.0f;
    }
    wheels_input(g_wheels, g_track, dt, in);
}

// ------------------------------------------------------------------ ini
static int parse_keys(const char *v, int *out)
{
    int n = 0;
    while (*v && n < MAX_CAM_KEYS) {
        while (*v == ' ' || *v == ',' || *v == '\t') v++;
        if (!*v) break;
        char *end = nullptr;
        const long k = strtol(v, &end, 0);
        if (end == v) break;
        if (k > 0 && k < 256) out[n++] = (int)k;
        v = end;
    }
    return n;
}

static void load_ini_now()
{
    HeadCfg c;
    int en = 1, dbg = 0;
    int ml = 1, minv = 0, mrk = 0, lk = 0x04, ltg = 0;
    float mdpp = 0.015f, mw = 0.22f, mms = 42.f, macc = 0.5f, myl = 150.f, mul = 45.f, mdl = 60.f;
    float lf = 0.30f, ld = 0.03f, lp = 2.0f, lt = 0.7f;
    CamKeys ck = g_cam_keys_pending;
    RoughOverride ovr[MAX_OVR]; int novr = 0;
    sznt::read_ini(g_ini_path, [&](const char *k, const char *v) {
        const float fv = sznt::to_f(v);
#define F(n) else if (!strcmp(k, #n)) c.n = fv;
        if (0) {}
        F(seat_back) F(seat_up) F(seat_right) F(seat_pitch_deg)
        F(look_steer_deg) F(look_steer_until) F(look_ahead_time) F(look_ahead_from) F(look_max_deg) F(look_smooth)
        F(lean_side) F(lean_fwd) F(bounce_up) F(body_freq) F(body_damp) F(bounce_freq)
        F(roll_from_side_deg) F(pitch_from_fwd_deg) F(max_offset)
        F(breath_pitch_deg) F(breath_up) F(breath_period) F(idle_sway_deg) F(idle_sway_m)
        F(surface_feel) F(surface_step) F(surface_step_pitch_deg) F(surface_step_roll_deg)
        F(surface_rock) F(surface_rock_roll_deg) F(surface_freq)
#undef F
        else if (!strcmp(k, "enabled")) en = sznt::to_i(v);
        else if (!strcmp(k, "debug")) dbg = sznt::to_i(v);
        else if (!strcmp(k, "mouse_look")) ml = sznt::to_i(v);
        else if (!strcmp(k, "mouse_invert")) minv = sznt::to_i(v);
        else if (!strcmp(k, "mouse_recenter_key")) mrk = sznt::to_i(v);
        else if (!strcmp(k, "mouse_sensitivity")) mdpp = fv;
        else if (!strcmp(k, "mouse_weight")) mw = fv;
        else if (!strcmp(k, "mouse_max_speed")) mms = fv;
        else if (!strcmp(k, "mouse_accel")) macc = fv;
        else if (!strcmp(k, "mouse_yaw_limit")) myl = fv;
        else if (!strcmp(k, "mouse_up_limit")) mul = fv;
        else if (!strcmp(k, "mouse_down_limit")) mdl = fv;
        else if (!strcmp(k, "lean_key")) lk = sznt::to_i(v);
        else if (!strcmp(k, "lean_toggle")) ltg = sznt::to_i(v);
        else if (!strcmp(k, "lean_forward")) lf = fv;
        else if (!strcmp(k, "lean_down")) ld = fv;
        else if (!strcmp(k, "lean_pitch_deg")) lp = fv;
        else if (!strcmp(k, "lean_time")) lt = fv;
        else if (!strcmp(k, "camera_inside_keys")) ck.n_in = parse_keys(v, ck.inside);
        else if (!strcmp(k, "camera_outside_keys")) ck.n_out = parse_keys(v, ck.outside);
        else if (!strncmp(k, "rough_", 6) && k[6] && novr < MAX_OVR) {
            snprintf(ovr[novr].name, sizeof(ovr[novr].name), "%s", k + 6);
            ovr[novr++].value = fv;
        }
    });
    if (c.body_freq < 0.2f) c.body_freq = 0.2f;
    if (c.bounce_freq < 0.2f) c.bounce_freq = 0.2f;
    if (c.look_steer_until < 10) c.look_steer_until = 10;
    if (c.surface_feel < 0) c.surface_feel = 0;

    EnterCriticalSection(&g_lock);
    g_cfg_pending = c; g_enabled = en; g_debug = dbg;
    g_mouse_look = ml; g_mouse_invert = minv; g_recenter_key = mrk;
    g_mouse_deg_per_px = mdpp; g_mouse_weight = mw < 0.02f ? 0.02f : mw; g_mouse_max_speed = mms < 10 ? 10 : mms;
    g_mouse_accel = macc < 0 ? 0 : macc;
    g_mouse_yaw_limit = myl; g_mouse_up_limit = mul; g_mouse_down_limit = mdl;
    g_lean_key = lk; g_lean_toggle = ltg; g_lean_fwd = lf; g_lean_down = ld; g_lean_pitch = lp;
    g_lean_time = lt < 0.05f ? 0.05f : lt;
    g_cam_keys_pending = ck;
    memcpy(g_ovr_pending, ovr, sizeof(ovr)); g_novr_pending = novr;
    LeaveCriticalSection(&g_lock);
    InterlockedExchange(&g_cfg_dirty, 1);
}

static DWORD WINAPI worker(LPVOID)
{
    FILETIME last = sznt::file_time(g_ini_path);
    sznt::LogTail tail;
    for (int tick = 0; !g_stop; tick++) {
        if (tick % 10 == 0) {
            FILETIME t = sznt::file_time(g_ini_path);
            if (CompareFileTime(&t, &last)) { last = t; Sleep(100); load_ini_now(); }
        }
        tail.poll(g_log_path, [](const std::string &l) { g_walk.on_line(l); });
        Sleep(50);
    }
    return 0;
}

// ------------------------------------------------------------------ mouse (low-level hook: read only, never blocks input)
static LRESULT CALLBACK mouse_proc(int code, WPARAM wp, LPARAM lp)
{
    if (code == HC_ACTION && wp == WM_MOUSEMOVE) {
        const MSLLHOOKSTRUCT *m = (const MSLLHOOKSTRUCT *)lp;
        POINT c;
        if (GetCursorPos(&c)) {
            InterlockedExchangeAdd(&g_mdx, m->pt.x - c.x);
            InterlockedExchangeAdd(&g_mdy, m->pt.y - c.y);
        }
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}

static DWORD WINAPI hook_thread(LPVOID)
{
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&mouse_proc, &self);
    HHOOK hk = SetWindowsHookExW(WH_MOUSE_LL, mouse_proc, self, 0);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    if (hk) UnhookWindowsHookEx(hk);
    return 0;
}

static bool interior() { return g_walk.camera_known ? g_walk.interior != 0 : g_cam_interior != 0; }
static bool mouse_owned() { return g_enabled && g_mouse_look && !g_walk.walking && interior(); }

static void camera_keys_update(bool active)
{
    auto edge = [&](int vk) {
        const bool down = active && sznt::key_down(vk);
        const bool hit = down && !g_cam_prev[vk & 0xFF];
        g_cam_prev[vk & 0xFF] = down;
        return hit;
    };
    for (int i = 0; i < g_cam_keys.n_in; i++) if (edge(g_cam_keys.inside[i])) g_cam_interior = 1;
    for (int i = 0; i < g_cam_keys.n_out; i++) if (edge(g_cam_keys.outside[i])) g_cam_interior = 0;
}

static void mouse_update(float dt)
{
    const LONG dx = InterlockedExchange(&g_mdx, 0), dy = InterlockedExchange(&g_mdy, 0);
    const float D2R = 0.0174533f;
    if (mouse_owned() && !g_paused && sznt::game_in_front()) {
        const float pxs = dt > 0 ? fabsf((float)dx) / dt : 0;
        const int d = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
        if (d != 0 && d == m_turn_dir && pxs > 150) m_turn_t += dt;
        else if (d != 0 && d != m_turn_dir) { m_turn_dir = d; m_turn_t = 0; }
        else m_turn_t = fmaxf(0.f, m_turn_t - dt * 2);
        const float boost = 1.0f + g_mouse_accel * fminf(m_turn_t / 0.6f, 1.0f);
        m_tyaw   -= dx * g_mouse_deg_per_px * D2R * boost;
        m_tpitch -= dy * g_mouse_deg_per_px * D2R * (g_mouse_invert ? -1.f : 1.f);
        if (sznt::key_down(g_recenter_key)) { m_tyaw = 0; m_tpitch = 0; }
    }
    const float yl = g_mouse_yaw_limit * D2R;
    m_tyaw = h_clamp(m_tyaw, -yl, yl);
    m_tpitch = h_clamp(m_tpitch, -g_mouse_down_limit * D2R, g_mouse_up_limit * D2R);
    if (dt <= 0) return;
    const float w = 2.0f / g_mouse_weight;
    const float vmax = g_mouse_max_speed * D2R * (1.0f + g_mouse_accel * fminf(m_turn_t / 0.6f, 1.0f));
    const int n = dt > 0.004f ? (int)ceilf(dt / 0.004f) : 1;
    const float h = dt / n;
    for (int i = 0; i < n; i++) {
        const float ay = w * w * (m_tyaw - m_yaw) - 2 * w * m_vyaw;
        const float ap = w * w * (m_tpitch - m_pitch) - 2 * w * m_vpitch;
        m_vyaw = h_clamp(m_vyaw + ay * h, -vmax, vmax);
        m_vpitch = h_clamp(m_vpitch + ap * h, -vmax, vmax);
        m_yaw += m_vyaw * h; m_pitch += m_vpitch * h;
    }
}

static void lean_update(float dt)
{
    const bool can = g_enabled && !g_walk.walking && interior() && !g_paused && sznt::game_in_front();
    const bool down = can && sznt::key_down(g_lean_key);
    if (g_lean_toggle) { if (down && !l_prev) l_on = !l_on; }
    else l_on = down;
    l_prev = down;
    if (!can && !g_paused) l_on = false;
    if (dt <= 0) return;
    const float target = l_on ? 1.f : 0.f;
    const float w = 2.0f / g_lean_time;
    const int n = dt > 0.004f ? (int)ceilf(dt / 0.004f) : 1;
    const float h = dt / n;
    for (int i = 0; i < n; i++) { const float a = w * w * (target - l_pos) - 2 * w * l_vel; l_vel += a * h; l_pos += l_vel * h; }
}

// ------------------------------------------------------------------ per frame
static void frame_update()
{
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    float dt = (float)(now.QuadPart - g_last.QuadPart) / (float)g_qpf.QuadPart;
    if (dt < 0.0005f) return;
    g_last = now;
    if (g_cfg_dirty) {
        EnterCriticalSection(&g_lock);
        g_cfg = g_cfg_pending;
        g_cam_keys = g_cam_keys_pending;
        memcpy(g_ovr, g_ovr_pending, sizeof(g_ovr)); g_novr = g_novr_pending;
        LeaveCriticalSection(&g_lock);
        InterlockedExchange(&g_cfg_dirty, 0);
        classify_substances();
        logf_(SCS_LOG_TYPE_message, "settings loaded (enabled=%d)", g_enabled);
    }
    if (g_paused) dt = 0;
    camera_keys_update(!g_paused && !g_walk.walking && sznt::game_in_front());
    wheels_to_input(g_in, dt);
    head_update(g_cfg, g_st, g_in, dt, g_out);
    mouse_update(dt);
    lean_update(dt);

    if (g_debug && dt > 0 && (g_dbg_t += dt) > 0.5f) {
        g_dbg_t = 0;
        logf_(SCS_LOG_TYPE_message,
              "dbg %s mouse %s yaw %+.1f pitch %+.1f | %.0f km/h rough %.2f | surface y %+.1f mm pitch %+.2f roll %+.2f deg"
              " | cabin pitch %+.2f roll %+.2f -> stab pitch %+.2f roll %+.2f deg"
              " | out yaw %+.1f pitch %+.2f roll %+.2f deg x %+.3f y %+.4f z %+.3f m walk %ld",
              interior() ? "inside" : "outside", mouse_owned() ? "head" : "game", m_yaw * 57.2958f, m_pitch * 57.2958f,
              fabsf(g_in.speed_ms) * 3.6f, g_st.rough, g_st.surf_y * 1000.0f, g_st.surf_pitch * 57.2958f, g_st.surf_roll * 57.2958f,
              g_in.cab_pitch * 57.2958f, g_in.cab_roll * 57.2958f, g_st.stab_p.p * 57.2958f, g_st.stab_r.p * 57.2958f,
              g_out.yaw * 57.2958f, g_out.pitch * 57.2958f, g_out.roll * 57.2958f, g_out.x, g_out.y, g_out.z, g_walk.walking);
    }
}

// ------------------------------------------------------------------ device
static const scs_input_device_input_t g_inputs[] = {
    { "sz_on", "Active", SCS_VALUE_TYPE_bool },
    { "sz_yaw", "Yaw", SCS_VALUE_TYPE_float },
    { "sz_pitch", "Pitch", SCS_VALUE_TYPE_float },
    { "sz_roll", "Roll", SCS_VALUE_TYPE_float },
    { "sz_x", "X", SCS_VALUE_TYPE_float },
    { "sz_y", "Y", SCS_VALUE_TYPE_float },
    { "sz_z", "Z", SCS_VALUE_TYPE_float },
    { "sz_mouse", "Mouse_owned", SCS_VALUE_TYPE_float },
};
static float g_sent[8];
static unsigned g_next = 0;

SCSAPI_VOID dev_active(const scs_u8_t a, const scs_context_t)
{
    for (float &s : g_sent) s = -99;
    logf_(SCS_LOG_TYPE_message, "head device %s", a ? "active" : "inactive");
}

SCSAPI_RESULT dev_event(scs_input_event_t *const ev, const scs_u32_t flags, const scs_context_t)
{
    if (flags & SCS_INPUT_EVENT_CALLBACK_FLAG_first_after_activation) for (float &s : g_sent) s = -99;
    if (flags & SCS_INPUT_EVENT_CALLBACK_FLAG_first_in_frame) { frame_update(); g_next = 0; }

    const bool on = g_enabled && !g_walk.walking;
    const bool mo = on && mouse_owned();
    const float ly = mo ? m_yaw : 0.f, lpi = mo ? m_pitch : 0.f;
    const float lf = l_pos * g_lean_fwd;
    float v[8];
    v[0] = on ? 1.f : 0.f;
    v[1] = on ? g_out.yaw + ly : 0.f;
    v[2] = on ? g_out.pitch + lpi - l_pos * g_lean_pitch * 0.0174533f : 0.f;
    v[3] = on ? g_out.roll : 0.f;
    v[4] = on ? g_out.x - lf * sinf(ly) * cosf(lpi) : 0.f;
    v[5] = on ? g_out.y - l_pos * g_lean_down + lf * sinf(lpi) * 0.6f : 0.f;
    v[6] = on ? g_out.z - lf * cosf(ly) * cosf(lpi) : 0.f;
    v[7] = mo ? 1.f : 0.f;
    while (g_next < 8) {
        const unsigned i = g_next++;
        if (fabsf(v[i] - g_sent[i]) > 0.00002f) {
            g_sent[i] = v[i];
            ev->input_index = i;
            if (i == 0) ev->value_bool.value = v[0] > 0.5f; else ev->value_float.value = v[i];
            return SCS_RESULT_ok;
        }
    }
    return SCS_RESULT_not_found;
}

extern "C" __declspec(dllexport)
SCSAPI_RESULT scs_input_init(const scs_u32_t version, const scs_input_init_params_t *const params)
{
    if (version != SCS_INPUT_VERSION_1_00) return SCS_RESULT_unsupported;
    const scs_input_init_params_v100_t *p = static_cast<const scs_input_init_params_v100_t *>(params);
    g_log = p->common.log;
    InitializeCriticalSection(&g_lock);
    QueryPerformanceFrequency(&g_qpf); QueryPerformanceCounter(&g_last);
    g_ini_path = sznt::module_dir() + L"\\sznt-view.ini";
    g_log_path = sznt::game_log_path(p->common.game_id);
    load_ini_now();

    scs_input_device_t d; memset(&d, 0, sizeof(d));
    d.name = "sznt_view"; d.display_name = "SZNT View";
    d.type = SCS_INPUT_DEVICE_TYPE_generic;
    d.input_count = 8; d.inputs = g_inputs;
    d.input_active_callback = dev_active; d.input_event_callback = dev_event;
    if (p->register_device(&d) != SCS_RESULT_ok) { logf_(SCS_LOG_TYPE_error, "could not register device"); return SCS_RESULT_generic_error; }

    g_stop = 0;
    g_thread = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
    g_hook_thread = CreateThread(nullptr, 0, hook_thread, nullptr, 0, &g_hook_tid);
    logf_(SCS_LOG_TYPE_message, "v%s ready", SZNT_VERSION);
    return SCS_RESULT_ok;
}

extern "C" __declspec(dllexport)
SCSAPI_VOID scs_input_shutdown(void)
{
    InterlockedExchange(&g_stop, 1);
    if (g_thread) { WaitForSingleObject(g_thread, 2000); CloseHandle(g_thread); g_thread = nullptr; }
    if (g_hook_thread) {
        PostThreadMessageW(g_hook_tid, WM_QUIT, 0, 0);
        WaitForSingleObject(g_hook_thread, 2000); CloseHandle(g_hook_thread); g_hook_thread = nullptr;
    }
}

// ------------------------------------------------------------------ telemetry
SCSAPI_VOID t_float(const scs_string_t, const scs_u32_t, const scs_value_t *const v, const scs_context_t c)
{
    const float f = v ? v->value_float.value : 0.f;
    if ((size_t)c == 0) g_in.speed_ms = f; else g_in.steer = f;
}
SCSAPI_VOID t_vec(const scs_string_t, const scs_u32_t, const scs_value_t *const v, const scs_context_t c)
{
    if (!v) return;
    const scs_value_fvector_t &q = v->value_fvector;
    const float TAU = 6.2831853f;
    switch ((size_t)c) {
    case 0: g_in.ax = q.x; g_in.ay = q.y; g_in.az = q.z; break;
    case 1: g_in.yaw_rate = q.y; g_in.body_pitch_rate = q.x * TAU; g_in.body_roll_rate = q.z * TAU; break;
    case 2: g_in.cab_alpha_pitch = q.x * TAU; g_in.cab_alpha_roll = q.z * TAU; break;
    }
}
SCSAPI_VOID t_cabin(const scs_string_t, const scs_u32_t, const scs_value_t *const v, const scs_context_t)
{
    const float TAU = 6.2831853f;
    g_in.cab_pitch = v ? v->value_fplacement.orientation.pitch * TAU : 0.f;
    g_in.cab_roll = v ? v->value_fplacement.orientation.roll * TAU : 0.f;
}
SCSAPI_VOID t_wground(const scs_string_t, const scs_u32_t i, const scs_value_t *const v, const scs_context_t)
{ if (i < (scs_u32_t)MAXW) g_wheels.ground[i] = v && v->value_bool.value; }
SCSAPI_VOID t_wsubst(const scs_string_t, const scs_u32_t i, const scs_value_t *const v, const scs_context_t)
{ if (i < (scs_u32_t)MAXW) g_subst[i] = v ? (int)v->value_u32.value : 0; }

SCSAPI_VOID t_config(const scs_event_t, const void *const info, const scs_context_t)
{
    const scs_telemetry_configuration_t *c = (const scs_telemetry_configuration_t *)info;
    if (!c || !c->id) return;
    if (!strcmp(c->id, SCS_TELEMETRY_CONFIG_substances)) {
        int n = 0;
        for (const scs_named_value_t *a = c->attributes; a && a->name; a++) {
            if (strcmp(a->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_id) || a->index >= (scs_u32_t)MAXS) continue;
            snprintf(g_sname[a->index], sizeof(g_sname[0]), "%s", a->value.value_string.value ? a->value.value_string.value : "");
            if ((int)a->index + 1 > n) n = a->index + 1;
        }
        g_nsubst = n;
        classify_substances();
        g_track.reset();
        if (g_debug) {
            std::string all;
            char b[64];
            for (int i = 0; i < n; i++) { snprintf(b, sizeof(b), "%s=%.2f ", g_sname[i], g_srough[i]); all += b; }
            logf_(SCS_LOG_TYPE_message, "surfaces: %s", all.c_str());
        }
    } else if (!strcmp(c->id, SCS_TELEMETRY_CONFIG_truck)) {
        int wheels = 0;
        float head_y = 1.2f;
        for (const scs_named_value_t *a = c->attributes; a && a->name; a++) {
            if (!strcmp(a->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_wheel_count)) wheels = (int)a->value.value_u32.value;
            else if (!strcmp(a->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_head_position))
                head_y = a->value.value_fvector.y;
            else if (!strcmp(a->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_wheel_position) && a->index < (scs_u32_t)MAXW) {
                g_wheels.x[a->index] = a->value.value_fvector.x;
                g_wheels.z[a->index] = a->value.value_fvector.z;
            }
        }
        g_wheels.n = wheels > MAXW ? MAXW : wheels;
        g_in.head_height = h_clamp(head_y, 0.3f, 2.5f);
        g_track.reset();
    }
}

SCSAPI_VOID t_pause(const scs_event_t ev, const void *const, const scs_context_t)
{
    InterlockedExchange(&g_paused, ev == SCS_TELEMETRY_EVENT_paused ? 1 : 0);
}

extern "C" __declspec(dllexport)
SCSAPI_RESULT scs_telemetry_init(const scs_u32_t version, const scs_telemetry_init_params_t *const params)
{
    if (version != SCS_TELEMETRY_VERSION_1_01 && version != SCS_TELEMETRY_VERSION_1_00) return SCS_RESULT_unsupported;
    const scs_telemetry_init_params_v100_t *p = static_cast<const scs_telemetry_init_params_v100_t *>(params);
    if (!g_log) g_log = p->common.log;
    p->register_for_event(SCS_TELEMETRY_EVENT_paused, t_pause, nullptr);
    p->register_for_event(SCS_TELEMETRY_EVENT_started, t_pause, nullptr);
    p->register_for_event(SCS_TELEMETRY_EVENT_configuration, t_config, nullptr);
    const scs_u32_t N = SCS_U32_NIL, F0 = SCS_TELEMETRY_CHANNEL_FLAG_none;
    p->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_speed, N, SCS_VALUE_TYPE_float, F0, t_float, (scs_context_t)0);
    p->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_effective_steering, N, SCS_VALUE_TYPE_float, F0, t_float, (scs_context_t)1);
    p->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_local_linear_acceleration, N, SCS_VALUE_TYPE_fvector, F0, t_vec, (scs_context_t)0);
    p->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_local_angular_velocity, N, SCS_VALUE_TYPE_fvector, F0, t_vec, (scs_context_t)1);
    p->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_cabin_angular_acceleration, N, SCS_VALUE_TYPE_fvector, F0, t_vec, (scs_context_t)2);
    p->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_cabin_offset, N, SCS_VALUE_TYPE_fplacement, F0, t_cabin, nullptr);
    for (scs_u32_t i = 0; i < (scs_u32_t)MAXW; i++) {
        p->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_wheel_on_ground, i, SCS_VALUE_TYPE_bool, F0, t_wground, nullptr);
        p->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_wheel_substance, i, SCS_VALUE_TYPE_u32, F0, t_wsubst, nullptr);
    }
    return SCS_RESULT_ok;
}

extern "C" __declspec(dllexport)
SCSAPI_VOID scs_telemetry_shutdown(void) {}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
