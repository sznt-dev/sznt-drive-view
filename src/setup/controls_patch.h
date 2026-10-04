// Connects the plugins to the game by editing the profile's controls.sii. Fully reversible:
// every added term has a unique name (".sz_*?0") and is removed precisely.
#pragma once
#include <cctype>
#include <string>
#include <vector>

namespace ctl {

static const char *DRIVE_DEV = "sdk.sznt_drive";
static const char *VIEW_DEV = "sdk.sznt_view";
static const char *SLOTS[] = {"joy6", "joy5", "joy4", "joy3", "joy2", "joy7", "joy8", "joy9"};

struct Report {
    int changes = 0;
    bool legacy_drive = false, legacy_view = false;
    std::string drive_slot, view_slot;
    std::vector<std::string> problems;
};

inline bool replace_once(std::string &s, const std::string &a, const std::string &b)
{
    const size_t p = s.find(a);
    if (p == std::string::npos) return false;
    s.replace(p, a.size(), b);
    return true;
}

inline bool looks_like_controls(const std::string &t)
{
    return t.find("input_config") != std::string::npos && t.find("\"mix steering `") != std::string::npos;
}

inline bool device_line(const std::string &t, const std::string &slot, size_t &v0, size_t &v1)
{
    const std::string key = "\"device " + slot + " `";
    const size_t p = t.find(key);
    if (p == std::string::npos) return false;
    v0 = p + key.size();
    v1 = t.find('`', v0);
    return v1 != std::string::npos;
}

inline std::string device_of(const std::string &t, const std::string &slot)
{
    size_t a, b;
    return device_line(t, slot, a, b) ? t.substr(a, b - a) : std::string("?");
}

inline std::string slot_of(const std::string &t, const std::string &dev)
{
    for (const char *s : SLOTS) if (device_of(t, s) == dev) return s;
    return "";
}

inline void set_device(std::string &t, const std::string &slot, const std::string &dev, Report &r)
{
    size_t a, b;
    if (!device_line(t, slot, a, b) || t.substr(a, b - a) == dev) return;
    t.replace(a, b - a, dev);
    r.changes++;
}

inline std::string free_slot(const std::string &t, const std::string &avoid)
{
    for (const char *s : SLOTS) if (s != avoid && device_of(t, s).empty()) return s;
    return "";
}

template <class F> bool edit_mix(std::string &t, const std::string &name, Report &r, F f)
{
    const std::string key = "\"mix " + name + " `";
    const size_t p = t.find(key);
    if (p == std::string::npos) return false;
    const size_t e0 = p + key.size(), e1 = t.find("`\"", e0);
    if (e1 == std::string::npos) return false;
    std::string expr = t.substr(e0, e1 - e0);
    const std::string orig = expr;
    f(expr);
    if (expr != orig) { t.replace(e0, e1 - e0, expr); r.changes++; }
    return true;
}

inline std::string mix_of(const std::string &t, const std::string &name)
{
    const std::string key = "\"mix " + name + " `";
    const size_t p = t.find(key);
    if (p == std::string::npos) return "";
    const size_t e0 = p + key.size(), e1 = t.find("`\"", e0);
    return e1 == std::string::npos ? "" : t.substr(e0, e1 - e0);
}

// Removes "<op><slot>.<input>?0" (e.g. " - joy6.sz_steer?0"); empty slot = any slot.
inline bool remove_term(std::string &e, const std::string &op, const std::string &slot, const std::string &input)
{
    const std::string tail = "." + input + "?0";
    size_t from = 0;
    while (true) {
        const size_t p = e.find(tail, from);
        if (p == std::string::npos) return false;
        size_t s = p;
        while (s > 0 && (isalnum((unsigned char)e[s - 1]) || e[s - 1] == '_')) s--;
        const std::string sl = e.substr(s, p - s);
        if ((slot.empty() || sl == slot) && s >= op.size() && e.compare(s - op.size(), op.size(), op) == 0) {
            e.erase(s - op.size(), op.size() + (p - s) + tail.size());
            return true;
        }
        from = p + tail.size();
    }
}

inline void set_relative_steering(std::string &t, const char *val, Report &r)
{
    const std::string key = "\"constant c_relatsteer ";
    const size_t p = t.find(key);
    if (p == std::string::npos) return;
    const size_t v0 = p + key.size(), v1 = t.find('"', v0);
    if (v1 != std::string::npos && t.substr(v0, v1 - v0) != val) { t.replace(v0, v1 - v0, val); r.changes++; }
}

inline void clear_ui_joy(std::string &t, Report &r)
{
    const std::string dev = device_of(t, "ui_joy");
    if (dev.rfind("sdk.sznt_", 0) == 0 || dev.rfind("sdk.tm_", 0) == 0) set_device(t, "ui_joy", "", r);
}

// W A S D stop driving the truck directly (SZNT Drive reads those keys); the arrow keys keep working.
inline void remove_wasd(std::string &t, Report &r)
{
    edit_mix(t, "dsteerleft", r, [](std::string &e) { replace_once(e, " | keyboard.a?0", ""); });
    edit_mix(t, "dsteerright", r, [](std::string &e) { replace_once(e, " | keyboard.d?0", ""); });
    edit_mix(t, "dforward", r, [](std::string &e) { replace_once(e, " | keyboard.w?0", ""); });
    auto no_s = [](std::string &e) {
        while (replace_once(e, "(keyboard.darrow?0 | keyboard.s?0)", "keyboard.darrow?0")) {}
        while (replace_once(e, "keyboard.darrow?0 | keyboard.s?0", "keyboard.darrow?0")) {}
    };
    edit_mix(t, "dbackward", r, no_s);
    edit_mix(t, "abackward", r, no_s);
}

inline void restore_wasd(std::string &t, Report &r)
{
    auto add = [](const char *arrow, const char *key) {
        return [=](std::string &e) {
            if (e.find(key) == std::string::npos) replace_once(e, arrow, std::string(arrow) + " | " + key);
        };
    };
    edit_mix(t, "dsteerleft", r, add("keyboard.larrow?0", "keyboard.a?0"));
    edit_mix(t, "dsteerright", r, add("keyboard.rarrow?0", "keyboard.d?0"));
    edit_mix(t, "dforward", r, add("keyboard.uarrow?0", "keyboard.w?0"));
    auto s = [](std::string &e) {
        if (e.find("keyboard.s?0") != std::string::npos) return;
        size_t p = 0;
        while ((p = e.find("keyboard.darrow?0", p)) != std::string::npos) {
            e.replace(p, 17, "(keyboard.darrow?0 | keyboard.s?0)");
            p += 34;
        }
    };
    edit_mix(t, "dbackward", r, s);
    edit_mix(t, "abackward", r, s);
}

static const char *HEAD_AXES[][2] = {
    {"headtryaw", "yaw"}, {"headtrpitch", "pitch"}, {"headtrroll", "roll"}, {"headtrx", "x"},
    {"headtry", "y"}, {"headtrz", "z"}, {"headtrwmyaw", "yaw"}, {"headtrwmpitc", "pitch"},
    {"headtrwmroll", "roll"}, {"headtrwmx", "x"}, {"headtrwmy", "y"}, {"headtrwmz", "z"},
};

inline void strip_drive_terms(std::string &t, const std::string &slot, const std::string &prefix, Report &r)
{
    edit_mix(t, "steering", r, [&](std::string &e) { while (remove_term(e, " - ", slot, prefix + "steer")) {} });
    edit_mix(t, "aforward", r, [&](std::string &e) { while (remove_term(e, " + ", slot, prefix + "throttle")) {} });
    edit_mix(t, "abackward", r, [&](std::string &e) { while (remove_term(e, " + ", slot, prefix + "brake")) {} });
}

inline void strip_view_terms(std::string &t, const std::string &slot, const std::string &prefix, Report &r)
{
    for (const char *m : {"headtron", "headtrwmon"})
        edit_mix(t, m, r, [&](std::string &e) { while (remove_term(e, " | ", slot, prefix + "on")) {} });
    for (auto &a : HEAD_AXES)
        edit_mix(t, a[0], r, [&](std::string &e) { while (remove_term(e, " + ", slot, prefix + a[1])) {} });
    for (const char *m : {"camlr", "camud"})
        edit_mix(t, m, r, [&](std::string &e) {
            const std::string tail = "." + prefix + "mouse?0)";
            size_t p;
            while ((p = e.find(tail)) != std::string::npos) {
                const size_t s = e.rfind(" * (1 - ", p);
                if (s == std::string::npos) break;
                e.erase(s, p + tail.size() - s);
            }
        });
}

// Previous version (TM Handling / TM Head): removes its terms and frees its slots.
inline void remove_legacy(std::string &t, Report &r, bool drive, bool view)
{
    for (const char *dev : {"sdk.tm_handling_joy", "sdk.tm_handling_sem"}) {
        if (!drive) break;
        const std::string s = slot_of(t, dev);
        if (s.empty()) continue;
        r.legacy_drive = true;
        strip_drive_terms(t, s, "", r);
        set_device(t, s, "", r);
    }
    const std::string hs = view ? slot_of(t, "sdk.tm_head_joy") : std::string();
    if (!hs.empty()) {
        r.legacy_view = true;
        strip_view_terms(t, hs, "", r);
        set_device(t, hs, "", r);
    }
    const std::string gs = drive ? slot_of(t, "sdk.tm_walk_gate") : std::string();
    if (!gs.empty()) set_device(t, gs, "", r);
}

inline bool install_drive(std::string &t, Report &r, const std::string &avoid_slot = "")
{
    std::string slot = slot_of(t, DRIVE_DEV);
    if (slot.empty()) slot = free_slot(t, avoid_slot);
    if (slot.empty()) { r.problems.push_back("no_free_slot_drive"); return false; }
    r.drive_slot = slot;
    set_device(t, slot, DRIVE_DEV, r);
    clear_ui_joy(t, r);
    set_relative_steering(t, "0.000000", r);
    remove_wasd(t, r);
    strip_drive_terms(t, "", "sz_", r);
    bool ok = edit_mix(t, "steering", r, [&](std::string &e) { e += " - " + slot + ".sz_steer?0"; });
    ok &= edit_mix(t, "aforward", r, [&](std::string &e) { e += " + " + slot + ".sz_throttle?0"; });
    ok &= edit_mix(t, "abackward", r, [&](std::string &e) {
        if (!replace_once(e, "semantical.abackward?0", "semantical.abackward?0 + " + slot + ".sz_brake?0"))
            e += " + " + slot + ".sz_brake?0";
    });
    if (!ok) r.problems.push_back("missing_drive_mix");
    return ok;
}

inline bool install_view(std::string &t, Report &r, const std::string &avoid_slot = "")
{
    std::string slot = slot_of(t, VIEW_DEV);
    if (slot.empty()) slot = free_slot(t, avoid_slot);
    if (slot.empty()) { r.problems.push_back("no_free_slot_view"); return false; }
    r.view_slot = slot;
    set_device(t, slot, VIEW_DEV, r);
    clear_ui_joy(t, r);
    strip_view_terms(t, "", "sz_", r);
    bool ok = true;
    for (const char *m : {"headtron", "headtrwmon"})
        ok &= edit_mix(t, m, r, [&](std::string &e) {
            if (!replace_once(e, "eyeposon)", "eyeposon | " + slot + ".sz_on?0)")) e = "(" + e + ") | " + slot + ".sz_on?0";
        });
    for (auto &a : HEAD_AXES)
        ok &= edit_mix(t, a[0], r, [&](std::string &e) { e += " + " + slot + ".sz_" + a[1] + "?0"; });
    const std::string gate = " * (1 - " + slot + ".sz_mouse?0)";
    edit_mix(t, "camlr", r, [&](std::string &e) { replace_once(e, "* c_msens", "* c_msens" + gate); });
    edit_mix(t, "camud", r, [&](std::string &e) { replace_once(e, "sel(c_minvert, -c_msens, c_msens)", "sel(c_minvert, -c_msens, c_msens)" + gate); });
    edit_mix(t, "camzoom", r, [](std::string &e) { replace_once(e, "mouse.button_middle?0 | ", ""); });
    if (!ok) r.problems.push_back("missing_view_mix");
    return ok;
}

inline void uninstall_drive(std::string &t, Report &r)
{
    const std::string slot = slot_of(t, DRIVE_DEV);
    strip_drive_terms(t, "", "sz_", r);
    if (!slot.empty()) set_device(t, slot, "", r);
    restore_wasd(t, r);
    set_relative_steering(t, "1.000000", r);
}

inline void uninstall_view(std::string &t, Report &r)
{
    const std::string slot = slot_of(t, VIEW_DEV);
    strip_view_terms(t, "", "sz_", r);
    if (!slot.empty()) set_device(t, slot, "", r);
    edit_mix(t, "camzoom", r, [](std::string &e) {
        if (e.find("mouse.button_middle?0") == std::string::npos) e = "mouse.button_middle?0 | " + e;
    });
}

// Installs/updates only the chosen mods (an older version of the same mod is replaced).
inline Report apply(std::string &t, bool drive, bool view)
{
    Report r;
    remove_legacy(t, r, drive, view);
    if (drive) install_drive(t, r, slot_of(t, VIEW_DEV));
    if (view) install_view(t, r, slot_of(t, DRIVE_DEV));
    return r;
}

inline Report remove(std::string &t, bool drive, bool view)
{
    Report r;
    remove_legacy(t, r, drive, view);
    if (drive) uninstall_drive(t, r);
    if (view) uninstall_view(t, r);
    return r;
}
}
