// Installer logic (no UI): games, files, profiles, Windows registration and updates.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cwchar>

#include "sznt_version.h"
#include "controls_patch.h"
#include "ini_merge.h"
#include "setup_res.h"
#include "i18n.h"
#include "payload_hash.h"

#ifndef SZNT_EDITION
#define SZNT_EDITION 0
#endif
// 0 = Drive + View, 1 = Drive only, 2 = View only
static const bool HAS_DRIVE = SZNT_EDITION != 2;
static const bool HAS_VIEW = SZNT_EDITION != 1;
static const wchar_t *const EDITION_NAME = SZNT_EDITION == 1 ? L"SZNT Drive" : SZNT_EDITION == 2 ? L"SZNT View" : L"SZNT Drive & View";
static const wchar_t *const EDITION_EXE = SZNT_EDITION == 1 ? L"SZNT-Drive-Setup.exe" : SZNT_EDITION == 2 ? L"SZNT-View-Setup.exe" : L"SZNT-Setup.exe";

enum NoteKind { NOTE_INFO, NOTE_OK, NOTE_WARN, NOTE_ERROR, NOTE_HEADER };
typedef void (*NoteFn)(NoteKind, const std::wstring &);
extern NoteFn g_note;

namespace inst {

inline std::wstring widen(const std::string &s)
{
    if (s.empty()) return L"";
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

inline std::string narrow(const std::wstring &w)
{
    if (w.empty()) return "";
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

inline std::wstring fmt(const wchar_t *f, ...)
{
    wchar_t b[2048];
    va_list ap; va_start(ap, f); _vsnwprintf(b, 2047, f, ap); va_end(ap);
    b[2047] = 0;
    return b;
}

inline void note(NoteKind k, const std::wstring &s) { if (g_note) g_note(k, s); }

// ------------------------------------------------------------------ files
inline bool exists(const std::wstring &p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }

inline bool read_file(const std::wstring &p, std::string &out)
{
    HANDLE h = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz;
    bool ok = GetFileSizeEx(h, &sz) && sz.QuadPart < (1 << 28);
    if (ok) {
        out.resize((size_t)sz.QuadPart);
        DWORD got = 0;
        ok = out.empty() || (ReadFile(h, &out[0], (DWORD)out.size(), &got, nullptr) && got == out.size());
    }
    CloseHandle(h);
    return ok;
}

// Writes to a temporary file and swaps it in at once (never leaves a half-written file).
inline DWORD write_file(const std::wstring &p, const std::string &data)
{
    const std::wstring tmp = p + L".sznt-tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return GetLastError();
    DWORD put = 0;
    const bool ok = data.empty() || (WriteFile(h, data.data(), (DWORD)data.size(), &put, nullptr) && put == data.size());
    FlushFileBuffers(h);
    CloseHandle(h);
    if (!ok) { DeleteFileW(tmp.c_str()); return ERROR_WRITE_FAULT; }
    if (!MoveFileExW(tmp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD e = GetLastError();
        DeleteFileW(tmp.c_str());
        return e;
    }
    return 0;
}

inline std::string sha256_hex(const std::string &data)
{
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE h = nullptr;
    unsigned char digest[32] = {0};
    std::string out;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0) {
        if (BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0) == 0) {
            BCryptHashData(h, (PUCHAR)data.data(), (ULONG)data.size(), 0);
            BCryptFinishHash(h, digest, 32, 0);
            BCryptDestroyHash(h);
            char b[3];
            for (unsigned char c : digest) { snprintf(b, 3, "%02x", c); out += b; }
        }
        BCryptCloseAlgorithmProvider(alg, 0);
    }
    return out;
}

// Embedded file, checked against the SHA-256 recorded at build time.
inline bool payload(int id, std::string &out)
{
    out.clear();
    HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(id), (LPCWSTR)RT_RCDATA);
    HGLOBAL g = r ? LoadResource(nullptr, r) : nullptr;
    const void *p = g ? LockResource(g) : nullptr;
    if (!p) return false;
    out.assign((const char *)p, SizeofResource(nullptr, r));
    for (const PayloadItem &it : SZNT_PAYLOAD)
        if (it.id == id) return sha256_hex(out) == it.sha256;
    return false;
}

inline std::wstring known_folder(REFKNOWNFOLDERID id)
{
    PWSTR p = nullptr;
    std::wstring s;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p)) && p) s = p;
    if (p) CoTaskMemFree(p);
    return s;
}

inline std::wstring self_path()
{
    wchar_t p[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, p, MAX_PATH);
    return p;
}

// SZNT_DOCUMENTS overrides the Documents folder (automated tests never touch real profiles).
inline bool test_mode()
{
    wchar_t b[8];
    return GetEnvironmentVariableW(L"SZNT_DOCUMENTS", b, 8) > 0;
}

inline std::wstring documents_dir()
{
    wchar_t buf[MAX_PATH] = {0};
    if (GetEnvironmentVariableW(L"SZNT_DOCUMENTS", buf, MAX_PATH)) return buf;
    return known_folder(FOLDERID_Documents);
}

// ------------------------------------------------------------------ games
struct Game {
    std::wstring name, root, exe, docs;
    unsigned steam_app = 0;
    bool selected = true;
};

inline std::wstring plugins_dir(const Game &g) { return g.root + L"\\bin\\win_x64\\plugins"; }

inline bool add_game(std::vector<Game> &games, const std::wstring &root_in)
{
    std::wstring root = root_in;
    while (!root.empty() && (root.back() == L'\\' || root.back() == L'/')) root.pop_back();
    struct { const wchar_t *exe, *name; unsigned app; } kinds[] = {
        {L"eurotrucks2.exe", L"Euro Truck Simulator 2", 227300},
        {L"amtrucks.exe", L"American Truck Simulator", 270880},
    };
    for (auto &k : kinds) {
        if (!exists(root + L"\\bin\\win_x64\\" + k.exe)) continue;
        for (auto &g : games) if (!_wcsicmp(g.root.c_str(), root.c_str())) return true;
        games.push_back({k.name, root, k.exe, documents_dir() + L"\\" + k.name, k.app, true});
        return true;
    }
    return false;
}

inline std::wstring reg_string(HKEY root, const wchar_t *key, const wchar_t *value)
{
    wchar_t buf[1024] = {0};
    DWORD sz = sizeof(buf);
    if (RegGetValueW(root, key, value, RRF_RT_REG_SZ, nullptr, buf, &sz) != ERROR_SUCCESS) return L"";
    return buf;
}

inline std::vector<Game> find_games()
{
    std::vector<Game> games;
    std::vector<std::wstring> steams;
    for (auto s : {reg_string(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath"),
                   reg_string(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath")}) {
        for (auto &c : s) if (c == L'/') c = L'\\';
        if (!s.empty()) steams.push_back(s);
    }
    std::vector<std::wstring> libs = steams;
    for (auto &s : steams) {
        std::string vdf;
        if (!read_file(s + L"\\steamapps\\libraryfolders.vdf", vdf)) continue;
        size_t p = 0;
        while ((p = vdf.find("\"path\"", p)) != std::string::npos) {
            const size_t a = vdf.find('"', p + 6), b = a == std::string::npos ? a : vdf.find('"', a + 1);
            if (b == std::string::npos) break;
            std::string path = vdf.substr(a + 1, b - a - 1), clean;
            for (size_t i = 0; i < path.size(); i++) { if (path[i] == '\\' && i + 1 < path.size() && path[i + 1] == '\\') i++; clean += path[i]; }
            libs.push_back(widen(clean));
            p = b + 1;
        }
    }
    for (auto &l : libs)
        for (const wchar_t *g : {L"Euro Truck Simulator 2", L"American Truck Simulator"})
            add_game(games, l + L"\\steamapps\\common\\" + g);
    return games;
}

inline bool process_running(const std::wstring &exe)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe; pe.dwSize = sizeof(pe);
    bool found = false;
    for (BOOL ok = Process32FirstW(snap, &pe); ok && !found; ok = Process32NextW(snap, &pe))
        found = !_wcsicmp(pe.szExeFile, exe.c_str());
    CloseHandle(snap);
    return found;
}

inline std::wstring file_version(const std::wstring &path)
{
    DWORD dummy = 0;
    const DWORD n = GetFileVersionInfoSizeW(path.c_str(), &dummy);
    if (!n) return L"";
    std::vector<char> buf(n);
    if (!GetFileVersionInfoW(path.c_str(), 0, n, buf.data())) return L"";
    wchar_t *v = nullptr; UINT len = 0;
    if (VerQueryValueW(buf.data(), L"\\StringFileInfo\\040904B0\\ProductVersion", (void **)&v, &len) && v) return v;
    return L"";
}

struct GameStatus { std::wstring drive, view; bool legacy = false; };

inline GameStatus status_of(const Game &g)
{
    const std::wstring d = plugins_dir(g);
    GameStatus s;
    s.drive = file_version(d + L"\\sznt-drive.dll");
    s.view = file_version(d + L"\\sznt-view.dll");
    s.legacy = (HAS_DRIVE && exists(d + L"\\tm-handling.dll")) || (HAS_VIEW && exists(d + L"\\tm-head.dll"));
    return s;
}

// A profile folder is its name in hex (UTF-8).
inline std::wstring profile_name(const std::wstring &folder)
{
    std::string s;
    for (size_t i = 0; i + 1 < folder.size(); i += 2) {
        wchar_t h[3] = {folder[i], folder[i + 1], 0};
        wchar_t *end = nullptr;
        const long v = wcstol(h, &end, 16);
        if (*end) return folder;
        s += (char)v;
    }
    return folder.size() % 2 ? folder : widen(s);
}

struct Profile { std::wstring name, controls; };

inline std::vector<Profile> profiles(const Game &g, std::vector<std::wstring> &without_controls)
{
    std::vector<Profile> out;
    for (const wchar_t *sub : {L"profiles", L"steam_profiles"}) {
        const std::wstring base = g.docs + L"\\" + sub;
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((base + L"\\*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.') continue;
            const std::wstring c = base + L"\\" + fd.cFileName + L"\\controls.sii";
            if (exists(c)) out.push_back({profile_name(fd.cFileName), c});
            else without_controls.push_back(profile_name(fd.cFileName));
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    return out;
}

// ------------------------------------------------------------------ Windows: Installed apps + Start menu
inline std::wstring app_dir() { return known_folder(FOLDERID_LocalAppData) + L"\\Programs\\SZNT"; }
inline std::wstring app_exe() { return app_dir() + L"\\" + EDITION_EXE; }
inline std::wstring start_menu_link() { return known_folder(FOLDERID_Programs) + L"\\" + EDITION_NAME + L".lnk"; }
inline std::wstring uninstall_key(bool drive) { return std::wstring(L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SZNT.") + (drive ? L"Drive" : L"View"); }

inline void reg_set(HKEY k, const wchar_t *name, const std::wstring &v)
{
    RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
}

inline void register_mod(bool drive)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, uninstall_key(drive).c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    reg_set(k, L"DisplayName", drive ? L"SZNT Drive" : L"SZNT View");
    reg_set(k, L"DisplayVersion", widen(SZNT_VERSION));
    reg_set(k, L"Publisher", widen(SZNT_PUBLISHER));
    reg_set(k, L"DisplayIcon", app_exe());
    reg_set(k, L"InstallLocation", app_dir());
    reg_set(k, L"UninstallString", L"\"" + app_exe() + L"\" --uninstall " + (drive ? L"--only-drive" : L"--only-view"));
    reg_set(k, L"URLInfoAbout", widen(drive ? SZNT_URL_DRIVE : SZNT_URL_VIEW));
    reg_set(k, L"HelpLink", widen(SZNT_REPO_URL));
    const DWORD one = 1, size_kb = 1600;
    RegSetValueExW(k, L"NoModify", 0, REG_DWORD, (const BYTE *)&one, sizeof(one));
    RegSetValueExW(k, L"NoRepair", 0, REG_DWORD, (const BYTE *)&one, sizeof(one));
    RegSetValueExW(k, L"EstimatedSize", 0, REG_DWORD, (const BYTE *)&size_kb, sizeof(size_kb));
    RegCloseKey(k);
}

inline bool mod_registered(bool drive)
{
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, uninstall_key(drive).c_str(), 0, KEY_READ, &k) != ERROR_SUCCESS) return false;
    RegCloseKey(k);
    return true;
}

inline void register_app(bool drive, bool view)
{
    if (test_mode()) return;
    SHCreateDirectoryExW(nullptr, app_dir().c_str(), nullptr);
    if (_wcsicmp(self_path().c_str(), app_exe().c_str())) CopyFileW(self_path().c_str(), app_exe().c_str(), FALSE);
    if (drive) register_mod(true);
    if (view) register_mod(false);
    IShellLinkW *link = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void **)&link))) {
        link->SetPath(app_exe().c_str());
        link->SetDescription(EDITION_NAME);
        IPersistFile *pf = nullptr;
        if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, (void **)&pf))) { pf->Save(start_menu_link().c_str(), TRUE); pf->Release(); }
        link->Release();
    }
}

inline void unregister_app(bool drive, bool view)
{
    if (test_mode()) return;
    if (drive) RegDeleteKeyW(HKEY_CURRENT_USER, uninstall_key(true).c_str());
    if (view) RegDeleteKeyW(HKEY_CURRENT_USER, uninstall_key(false).c_str());
    if (mod_registered(true) || mod_registered(false)) return;
    for (const wchar_t *n : {L"SZNT Drive & View", L"SZNT Drive", L"SZNT View"})
        DeleteFileW((known_folder(FOLDERID_Programs) + L"\\" + n + L".lnk").c_str());
    for (const wchar_t *e : {L"SZNT-Setup.exe", L"SZNT-Drive-Setup.exe", L"SZNT-View-Setup.exe"}) {
        const std::wstring p = app_dir() + L"\\" + e;
        if (!DeleteFileW(p.c_str())) MoveFileExW(p.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    }
    RemoveDirectoryW(app_dir().c_str());
}

// ------------------------------------------------------------------ install / remove
struct Result { bool ok = true, need_admin = false; };

inline bool can_write(const std::wstring &dir)
{
    SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
    const std::wstring probe = dir + L"\\.sznt-write-test";
    HANDLE h = CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    CloseHandle(h);
    return true;
}

inline void retire_legacy(const std::wstring &dir, bool drive, bool view, ini::Values &drive_old, ini::Values &view_old)
{
    std::string text;
    std::vector<const wchar_t *> files;
    if (drive) {
        if (read_file(dir + L"\\tm-handling.ini", text)) drive_old = ini::parse(text);
        files.insert(files.end(), {L"tm-handling.dll", L"tm-handling.ini", L"tm-walk-gate.dll"});
    }
    if (view) {
        if (read_file(dir + L"\\tm-head.ini", text)) view_old = ini::parse(text);
        files.insert(files.end(), {L"tm-head.dll", L"tm-head.ini"});
    }
    for (const wchar_t *f : files) {
        const std::wstring p = dir + L"\\" + f, off = p + L".old";
        if (!exists(p)) continue;
        DeleteFileW(off.c_str());
        if (MoveFileW(p.c_str(), off.c_str())) note(NOTE_INFO, fmt(tr(txt::m_old_disabled), f));
    }
}

inline int ini_resource(bool drive)
{
    return (drive ? IDR_DRIVE_INI_EN : IDR_VIEW_INI_EN) + (int)g_lang;
}

inline bool install_mod(const std::wstring &dir, bool drive, const ini::Values &legacy, Result &res)
{
    const wchar_t *base = drive ? L"sznt-drive" : L"sznt-view";
    const wchar_t *name = drive ? L"SZNT Drive" : L"SZNT View";
    const std::wstring dll_path = dir + L"\\" + base + L".dll", ini_path = dir + L"\\" + base + L".ini";
    std::string dll, tmpl, current;
    ini::Values user = legacy;
    if (read_file(ini_path, current) || read_file(ini_path + L".bak", current)) user = ini::parse(current);
    if (!payload(drive ? IDR_DRIVE_DLL : IDR_VIEW_DLL, dll) || !payload(ini_resource(drive), tmpl)) {
        note(NOTE_ERROR, tr(txt::m_corrupt));
        return false;
    }
    DWORD e = write_file(dll_path, dll);
    if (e == ERROR_ACCESS_DENIED) { res.need_admin = true; return false; }
    if (e) { note(NOTE_ERROR, fmt(tr(txt::m_copy_err), (std::wstring(base) + L".dll").c_str(), e)); return false; }
    std::string check;
    if (!read_file(dll_path, check) || check != dll) { note(NOTE_ERROR, fmt(tr(txt::m_verify_err), (std::wstring(base) + L".dll").c_str())); return false; }
    e = write_file(ini_path, user.empty() ? tmpl : ini::merge(tmpl, user));
    if (e) { note(NOTE_ERROR, fmt(tr(txt::m_copy_err), (std::wstring(base) + L".ini").c_str(), e)); return false; }
    DeleteFileW((ini_path + L".bak").c_str());
    note(NOTE_OK, fmt(tr(user.empty() ? txt::m_installed : txt::m_kept), name, widen(SZNT_VERSION).c_str()));
    return true;
}

inline void patch_profiles(const Game &g, bool install, bool drive, bool view, Result &res)
{
    std::vector<std::wstring> missing;
    const std::vector<Profile> list = profiles(g, missing);
    for (const Profile &p : list) {
        std::string text;
        if (!read_file(p.controls, text) || !ctl::looks_like_controls(text)) {
            note(NOTE_WARN, fmt(tr(txt::m_profile_bad), p.name.c_str()));
            continue;
        }
        const std::string before = text;
        const ctl::Report r = install ? ctl::apply(text, drive, view) : ctl::remove(text, drive, view);
        for (auto &pr : r.problems) {
            note(NOTE_WARN, p.name + L": " + tr(pr.rfind("no_free_slot", 0) == 0 ? txt::m_no_slot : txt::m_bad_format));
            res.ok = false;
        }
        if (text == before) { note(NOTE_OK, fmt(tr(txt::m_profile_same), p.name.c_str())); continue; }
        const std::wstring bak = p.controls + L".sznt-backup";
        if (install && !exists(bak)) CopyFileW(p.controls.c_str(), bak.c_str(), TRUE);
        const DWORD e = write_file(p.controls, text);
        if (e) { note(NOTE_ERROR, fmt(tr(txt::m_profile_err), p.name.c_str(), e)); res.ok = false; }
        else note(NOTE_OK, fmt(tr(install ? txt::m_profile_ok : txt::m_profile_restored), p.name.c_str()));
    }
    if (list.empty() && install) note(NOTE_WARN, tr(txt::m_no_profiles));
    if (install)
        for (auto &m : missing) note(NOTE_WARN, fmt(tr(txt::m_profile_missing), m.c_str()));
}

inline void install_game(const Game &g, bool drive, bool view, Result &res)
{
    note(NOTE_HEADER, g.name);
    if (process_running(g.exe)) { note(NOTE_ERROR, fmt(tr(txt::m_running), g.name.c_str())); res.ok = false; return; }
    const std::wstring dir = plugins_dir(g);
    if (!can_write(dir)) { res.need_admin = true; res.ok = false; return; }
    ini::Values drive_old, view_old;
    retire_legacy(dir, drive, view, drive_old, view_old);
    bool ok = true;
    if (drive) ok &= install_mod(dir, true, drive_old, res);
    if (view) ok &= install_mod(dir, false, view_old, res);
    if (res.need_admin) { res.ok = false; return; }
    if (!ok) res.ok = false;
    patch_profiles(g, true, drive, view, res);
}

inline void uninstall_game(const Game &g, bool drive, bool view, Result &res)
{
    note(NOTE_HEADER, g.name);
    if (process_running(g.exe)) { note(NOTE_ERROR, fmt(tr(txt::m_running), g.name.c_str())); res.ok = false; return; }
    const std::wstring dir = plugins_dir(g);
    if (exists(dir) && !can_write(dir)) { res.need_admin = true; res.ok = false; return; }
    for (int i = 0; i < 2; i++) {
        if ((i == 0 && !drive) || (i == 1 && !view)) continue;
        const std::wstring base = i == 0 ? L"sznt-drive" : L"sznt-view";
        const std::wstring dll = dir + L"\\" + base + L".dll", ini_path = dir + L"\\" + base + L".ini";
        if (exists(dll)) {
            if (DeleteFileW(dll.c_str())) note(NOTE_OK, fmt(tr(txt::m_removed), (base + L".dll").c_str()));
            else { note(NOTE_ERROR, fmt(tr(txt::m_remove_err), (base + L".dll").c_str(), GetLastError())); res.ok = false; }
        }
        if (exists(ini_path)) { DeleteFileW((ini_path + L".bak").c_str()); MoveFileW(ini_path.c_str(), (ini_path + L".bak").c_str()); }
    }
    patch_profiles(g, false, drive, view, res);
}

inline bool installed_anywhere(const std::vector<Game> &games, bool drive)
{
    for (auto &g : games) if (exists(plugins_dir(g) + (drive ? L"\\sznt-drive.dll" : L"\\sznt-view.dll"))) return true;
    return false;
}

// ------------------------------------------------------------------ updates (latest release tag on GitHub)
inline int compare_versions(const std::string &a, const std::string &b)
{
    auto parse = [](const std::string &s, int n[3], std::string &pre) {
        size_t i = (s.size() && (s[0] == 'v' || s[0] == 'V')) ? 1 : 0;
        for (int k = 0; k < 3; k++) {
            n[k] = 0;
            while (i < s.size() && isdigit((unsigned char)s[i])) n[k] = n[k] * 10 + (s[i++] - '0');
            if (i < s.size() && s[i] == '.') i++;
        }
        pre = (i < s.size() && s[i] == '-') ? s.substr(i + 1) : "";
    };
    int x[3], y[3]; std::string px, py;
    parse(a, x, px); parse(b, y, py);
    for (int k = 0; k < 3; k++) if (x[k] != y[k]) return x[k] < y[k] ? -1 : 1;
    if (px == py) return 0;
    if (px.empty()) return 1;
    if (py.empty()) return -1;
    return px < py ? -1 : 1;
}

inline std::string json_string(const std::string &j, const char *key)
{
    const std::string k = std::string("\"") + key + "\"";
    size_t p = j.find(k);
    if (p == std::string::npos) return "";
    p = j.find('"', j.find(':', p) + 1);
    if (p == std::string::npos) return "";
    const size_t e = j.find('"', p + 1);
    return e == std::string::npos ? "" : j.substr(p + 1, e - p - 1);
}

inline std::string http_get(const std::wstring &url)
{
    URL_COMPONENTS u; memset(&u, 0, sizeof(u)); u.dwStructSize = sizeof(u);
    wchar_t host[256], path[1024];
    u.lpszHostName = host; u.dwHostNameLength = 256; u.lpszUrlPath = path; u.dwUrlPathLength = 1024;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &u)) return "";
    std::string body;
    HINTERNET s = WinHttpOpen((L"SZNT-Setup/" + widen(SZNT_VERSION)).c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    HINTERNET c = s ? WinHttpConnect(s, host, u.nPort, 0) : nullptr;
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path, nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE) : nullptr;
    if (r) WinHttpSetTimeouts(r, 4000, 4000, 4000, 6000);
    if (r && WinHttpSendRequest(r, L"Accept: application/vnd.github+json\r\n", (DWORD)-1, nullptr, 0, 0, 0) && WinHttpReceiveResponse(r, nullptr)) {
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(r, &avail) && avail && body.size() < (1 << 20)) {
            std::string chunk(avail, 0); DWORD got = 0;
            if (!WinHttpReadData(r, &chunk[0], avail, &got) || !got) break;
            body.append(chunk, 0, got);
        }
    }
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    if (s) WinHttpCloseHandle(s);
    return body;
}

// Returns the newest published version, or empty when up to date / offline.
inline std::wstring newer_version()
{
    const std::string tag = json_string(http_get(widen(SZNT_RELEASES_API)), "tag_name");
    return !tag.empty() && compare_versions(SZNT_VERSION, tag) < 0 ? widen(tag) : L"";
}

inline const char *download_page()
{
    return SZNT_EDITION == 1 ? SZNT_URL_DRIVE : SZNT_EDITION == 2 ? SZNT_URL_VIEW : SZNT_URL_HOME;
}

}
