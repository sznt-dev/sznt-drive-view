#include <cstdio>
#include <fstream>
#include <sstream>
#include "controls_patch.h"

static std::string load(const char *p)
{
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}

static int fails = 0;
static void check(bool ok, const char *what)
{
    printf("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    if (!ok) fails++;
}

static int count(const std::string &t, const std::string &s)
{
    int n = 0;
    for (size_t p = t.find(s); p != std::string::npos; p = t.find(s, p + 1)) n++;
    return n;
}

static int diff_lines(const std::string &a, const std::string &b)
{
    std::istringstream x(a), y(b);
    std::string la, lb;
    int n = 0;
    while (std::getline(x, la) && std::getline(y, lb)) if (la != lb) n++;
    return n;
}

int main(int argc, char **argv)
{
    const std::string dir = argc > 1 ? argv[1] : "fixtures";
    const std::string vanilla = load((dir + "/controls_vanilla.sii").c_str());
    const std::string legacy = load((dir + "/controls_legacy_tm.sii").c_str());

    printf("Untouched profile:\n");
    std::string t = vanilla;
    ctl::Report r = ctl::apply(t, true, true);
    check(ctl::looks_like_controls(vanilla), "recognizes controls.sii");
    check(r.problems.empty(), "installs without problems");
    check(r.drive_slot == "joy6" && r.view_slot == "joy5", "slots joy6 (drive) and joy5 (view)");
    check(count(t, ".sz_") == 3 + 2 + 12 + 2, "19 bindings added");
    check(ctl::mix_of(t, "dforward") == "keyboard.uarrow?0" && ctl::mix_of(t, "dsteerleft") == "keyboard.larrow?0" && ctl::mix_of(t, "dsteerright") == "keyboard.rarrow?0", "W/A/D removed from the truck");
    check(ctl::mix_of(t, "dbackward").find("keyboard.s?0") == std::string::npos && ctl::mix_of(t, "abackward").find("keyboard.s?0") == std::string::npos, "S removed from the brake");
    check(t.find("c_relatsteer 0.000000") != std::string::npos, "absolute steering");
    std::string again = t;
    ctl::apply(again, true, true);
    check(again == t, "running again changes nothing");
    ctl::remove(t, true, true);
    check(t == vanilla, "uninstall restores the exact original");

    printf("View only, then Drive:\n");
    t = vanilla;
    ctl::apply(t, false, true);
    check(ctl::mix_of(t, "dforward") == ctl::mix_of(vanilla, "dforward") && count(t, ".sz_") == 16, "View alone leaves the keyboard alone");
    ctl::apply(t, true, true);
    check(count(t, ".sz_") == 19, "Drive added later");
    ctl::remove(t, true, false);
    check(count(t, ".sz_") == 16 && ctl::mix_of(t, "abackward") == ctl::mix_of(vanilla, "abackward") && ctl::mix_of(t, "dforward") == ctl::mix_of(vanilla, "dforward"), "removing only Drive gives WASD back");
    ctl::remove(t, false, true);
    check(t == vanilla, "removing the rest restores the original");

    printf("Profile with the previous version (TM):\n");
    t = legacy;
    r = ctl::apply(t, true, true);
    check(r.legacy_drive && r.legacy_view, "detects the previous version");
    check(t.find("sdk.tm_") == std::string::npos && t.find(".steer?0") == std::string::npos &&
          t.find("joy5.yaw?0") == std::string::npos && t.find("joy5.mouse") == std::string::npos, "nothing of the previous version is left");
    check(r.drive_slot == "joy6" && r.view_slot == "joy5", "reuses joy6/joy5");
    check(count(t, ".sz_") == 19, "19 new bindings");
    ctl::remove(t, true, true);
    printf("  lines different from the original profile after removing: %d\n", diff_lines(t, vanilla));

    printf("Previous version, installing View only (old Drive untouched):\n");
    t = legacy;
    ctl::apply(t, false, true);
    check(t.find("sdk.tm_handling_joy") != std::string::npos && t.find("joy6.steer?0") != std::string::npos, "old TM Handling stays connected");
    check(t.find("sdk.tm_head_joy") == std::string::npos && ctl::slot_of(t, ctl::VIEW_DEV) == "joy5" && count(t, ".sz_") == 16, "new View replaces TM Head");
    ctl::remove(t, false, true);
    check(count(t, ".sz_") == 0 && t.find("joy6.steer?0") != std::string::npos, "removing View leaves the old Drive alone");

    printf("All slots taken:\n");
    t = vanilla;
    for (const char *s : {"joy2", "joy3", "joy4", "joy5", "joy6"}) ctl::set_device(t, s, "sys.wheel", r);
    ctl::Report full = ctl::apply(t, true, true);
    check(!full.problems.empty() && count(t, ".sz_") == 0, "no free slot: reports it and changes nothing");

    printf("%s\n", fails ? "FAILURES!" : "All good.");
    return fails;
}
