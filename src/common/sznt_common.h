#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include "sznt_version.h"

namespace sznt {

inline std::wstring module_dir()
{
    HMODULE h = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)&module_dir, &h);
    wchar_t p[MAX_PATH] = {0};
    GetModuleFileNameW(h, p, MAX_PATH);
    std::wstring s(p);
    const size_t k = s.find_last_of(L"\\/");
    return k == std::wstring::npos ? s : s.substr(0, k);
}

inline std::wstring game_log_path(const char *game_id)
{
    PWSTR docs = nullptr;
    std::wstring p;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docs)) && docs) {
        p = docs;
        p += (game_id && !strcmp(game_id, "ats")) ? L"\\American Truck Simulator" : L"\\Euro Truck Simulator 2";
        p += L"\\game.log.txt";
    }
    if (docs) CoTaskMemFree(docs);
    return p;
}

inline FILETIME file_time(const std::wstring &p)
{
    WIN32_FILE_ATTRIBUTE_DATA d;
    FILETIME z = {0, 0};
    return GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &d) ? d.ftLastWriteTime : z;
}

inline bool game_in_front()
{
    HWND w = GetForegroundWindow();
    DWORD pid = 0;
    if (w) GetWindowThreadProcessId(w, &pid);
    return pid == GetCurrentProcessId();
}

inline bool key_down(int vk) { return vk > 0 && (GetAsyncKeyState(vk) & 0x8000) != 0; }

// "key = value"; sections and comments (; #) are ignored.
template <class F> void read_ini(const std::wstring &path, F on_value)
{
    FILE *f = _wfopen(path.c_str(), L"rb");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ';' || *p == '#' || *p == '[' || *p == '\r' || *p == '\n' || !*p) continue;
        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = 0;
        char *e = eq - 1;
        while (e >= p && (*e == ' ' || *e == '\t')) *e-- = 0;
        char *v = eq + 1;
        while (*v == ' ' || *v == '\t') v++;
        char *c = v;
        while (*c && *c != ';' && *c != '#' && *c != '\r' && *c != '\n') c++;
        *c = 0;
        on_value(p, v);
    }
    fclose(f);
}

inline float to_f(const char *v) { return (float)strtod(v, nullptr); }
inline int to_i(const char *v) { return (int)strtol(v, nullptr, 0); }

class LogTail {
public:
    template <class F> void poll(const std::wstring &path, F on_line)
    {
        if (f_ == INVALID_HANDLE_VALUE && !path.empty()) {
            f_ = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                             nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (f_ != INVALID_HANDLE_VALUE) { LARGE_INTEGER sz; pos_ = GetFileSizeEx(f_, &sz) ? sz.QuadPart : 0; pend_.clear(); }
        }
        if (f_ == INVALID_HANDLE_VALUE) return;
        LARGE_INTEGER sz;
        if (!GetFileSizeEx(f_, &sz)) { close(); return; }
        if (sz.QuadPart < pos_) { pos_ = 0; pend_.clear(); }
        while (sz.QuadPart > pos_) {
            LARGE_INTEGER li; li.QuadPart = pos_;
            SetFilePointerEx(f_, li, nullptr, FILE_BEGIN);
            const LONGLONG left = sz.QuadPart - pos_;
            DWORD want = (DWORD)(left > (LONGLONG)sizeof(buf_) ? sizeof(buf_) : left), got = 0;
            if (!ReadFile(f_, buf_, want, &got, nullptr) || !got) break;
            pos_ += got;
            pend_.append(buf_, got);
            size_t nl;
            while ((nl = pend_.find('\n')) != std::string::npos) { on_line(pend_.substr(0, nl)); pend_.erase(0, nl + 1); }
            if (pend_.size() > (1u << 20)) pend_.clear();
        }
    }
    void close() { if (f_ != INVALID_HANDLE_VALUE) CloseHandle(f_); f_ = INVALID_HANDLE_VALUE; }
    ~LogTail() { close(); }

private:
    HANDLE f_ = INVALID_HANDLE_VALUE;
    LONGLONG pos_ = 0;
    std::string pend_;
    char buf_[65536];
};

// Compatibility with TM Real Walk (third-party walking plugin): it writes to game.log.txt when
// the player leaves the truck and when the camera changes.
struct WalkCompat {
    volatile LONG walking = 0;
    volatile LONG interior = 1;
    volatile LONG camera_known = 0;

    void on_line(const std::string &l)
    {
        if (l.find("[TMRealWalk]") == std::string::npos) return;
        const size_t cp = l.find("[camera] fields changed: +0x10=");
        if (cp != std::string::npos) {
            const size_t a = l.find('(', cp), b = l.find(')', cp);
            if (a != std::string::npos && b != std::string::npos && b > a) {
                InterlockedExchange(&interior, l.substr(a + 1, b - a - 1).find("interior") != std::string::npos ? 1 : 0);
                InterlockedExchange(&camera_known, 1);
            }
            return;
        }
        if (l.find("[walker] enter at") != std::string::npos) InterlockedExchange(&walking, 1);
        else if (l.find("[walker] exit at") != std::string::npos || l.find("walk mode ended") != std::string::npos ||
                 l.find("] unloading") != std::string::npos || l.find("the game is quitting") != std::string::npos)
            InterlockedExchange(&walking, 0);
    }
};

// Game surfaces (substance names) -> roughness (0 smooth .. 1 grass) and grip (1 = dry asphalt).
struct Surface { float rough, grip; };

inline Surface surface_of(const char *n)
{
    static const struct { const char *name; float rough, grip; } known[] = {
        {"road", 0.00f, 1.00f}, {"road_smooth", 0.00f, 1.00f}, {"static", 0.00f, 1.00f}, {"invis", 0.00f, 1.00f},
        {"rubber", 0.00f, 1.00f}, {"plastic", 0.00f, 0.90f}, {"glass", 0.00f, 0.80f}, {"soft", 0.05f, 0.90f},
        {"road_coarse", 0.12f, 1.00f}, {"concrete", 0.18f, 1.00f}, {"metal", 0.22f, 0.85f},
        {"rumble_stripe", 0.30f, 0.95f}, {"wood", 0.35f, 0.80f}, {"ice", 0.08f, 0.20f}, {"road_snow", 0.25f, 0.50f},
        {"snow", 0.50f, 0.40f}, {"road_dirt", 0.55f, 0.75f}, {"gravel", 0.70f, 0.70f}, {"dirt", 0.80f, 0.65f},
        {"grass", 0.90f, 0.60f},
    };
    for (auto &k : known) if (!strcmp(n, k.name)) return {k.rough, k.grip};
    auto has = [&](const char *k) { return strstr(n, k) != nullptr; };
    if (has("ice")) return {0.08f, 0.20f};
    if (has("snow")) return {0.50f, 0.40f};
    if (has("grass") || has("field") || has("meadow") || has("offroad")) return {0.90f, 0.60f};
    if (has("mud")) return {0.80f, 0.50f};
    if (has("dirt") || has("soil") || has("earth") || has("sand")) return {0.80f, 0.65f};
    if (has("gravel")) return {0.70f, 0.70f};
    if (has("cobble") || has("pav") || has("brick") || has("sett")) return {0.45f, 0.90f};
    if (has("wood") || has("plank")) return {0.35f, 0.80f};
    if (has("rumble")) return {0.30f, 0.95f};
    if (has("metal") || has("steel") || has("iron") || has("bridge")) return {0.22f, 0.85f};
    if (has("concrete")) return {0.18f, 1.00f};
    if (has("coarse") || has("rough")) return {0.12f, 1.00f};
    return {0.0f, 1.0f};
}

}
