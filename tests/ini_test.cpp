#include <cstdio>
#include <fstream>
#include <sstream>
#include "ini_merge.h"

static int fails = 0;
static void check(bool ok, const std::string &what)
{
    printf("  [%s] %s\n", ok ? " ok " : "FAIL", what.c_str());
    if (!ok) fails++;
}

static std::string load(const std::string &p)
{
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}

int main(int argc, char **argv)
{
    const std::string src = argc > 1 ? argv[1] : "../src";
    printf("All 4 languages have the same keys and values:\n");
    for (const char *mod : {"drive", "view"}) {
        const std::string base = src + "/" + mod + "/sznt-" + mod;
        const ini::Values en = ini::parse(load(base + ".en.ini"));
        check(en.size() > 20, std::string(mod) + ": english has " + std::to_string(en.size()) + " keys");
        for (const char *lang : {"pt", "es", "de"}) {
            const ini::Values other = ini::parse(load(base + "." + lang + ".ini"));
            check(other == en, std::string(mod) + "." + lang + " matches english");
        }
    }

    printf("Upgrading an .ini keeps the user's settings:\n");
    const std::string tmpl = "[a]\nalpha = 1.0       ; comment\nbeta = 2   ; other\n[new]\ngamma = 3 ; new\n";
    const std::string old = "alpha = 0.5\nbeta=7\nremoved = 9\nrough_mud = 0.4\n";
    const std::string out = ini::merge(tmpl, ini::parse(old));
    const ini::Values v = ini::parse(out);
    check(v.at("alpha") == "0.5" && v.at("beta") == "7", "user values kept");
    check(v.at("gamma") == "3", "new key comes with its default");
    check(!v.count("removed"), "removed key goes away");
    check(v.count("rough_mud") && v.at("rough_mud") == "0.4", "surface overrides (rough_*) kept");
    check(out.find("; comment") != std::string::npos && out.find("[new]") != std::string::npos, "new comments and sections present");
    check(ini::merge(out, ini::parse(out)) == out, "running again changes nothing");

    printf("%s\n", fails ? "FAILURES!" : "All good.");
    return fails;
}
