// Upgrades an .ini to the new template (new sections and comments) while keeping the user's values.
#pragma once
#include <map>
#include <string>

namespace ini {

typedef std::map<std::string, std::string> Values;

inline std::string trim(const std::string &s)
{
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) b--;
    return s.substr(a, b - a);
}

inline Values parse(const std::string &text)
{
    Values v;
    size_t p = 0;
    while (p < text.size()) {
        size_t e = text.find('\n', p);
        if (e == std::string::npos) e = text.size();
        const std::string line = trim(text.substr(p, e - p));
        p = e + 1;
        if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string val = line.substr(eq + 1);
        const size_t c = val.find_first_of(";#");
        if (c != std::string::npos) val = val.substr(0, c);
        v[trim(line.substr(0, eq))] = trim(val);
    }
    return v;
}

inline std::string merge(const std::string &tmpl, const Values &user)
{
    std::string out;
    std::map<std::string, bool> used;
    size_t p = 0;
    while (p < tmpl.size()) {
        size_t e = tmpl.find('\n', p);
        const bool last = e == std::string::npos;
        if (last) e = tmpl.size();
        std::string line = tmpl.substr(p, e - p);
        p = e + 1;
        const std::string t = trim(line);
        const size_t eq = line.find('=');
        if (!t.empty() && t[0] != ';' && t[0] != '#' && t[0] != '[' && eq != std::string::npos) {
            const std::string key = trim(line.substr(0, eq));
            auto it = user.find(key);
            if (it != user.end()) {
                used[key] = true;
                size_t v0 = eq + 1;
                while (v0 < line.size() && line[v0] == ' ') v0++;
                size_t c = line.find(';', v0);
                if (c == std::string::npos) c = line.size();
                size_t v1 = c;
                while (v1 > v0 && (line[v1 - 1] == ' ' || line[v1 - 1] == '\r')) v1--;
                const std::string gap = line.substr(v1, c - v1);
                const int pad = (int)(v1 - v0) + (int)gap.size() - (int)it->second.size();
                const std::string spaces = c < line.size() ? std::string(pad > 1 ? pad : 1, ' ')
                                                           : (!line.empty() && line.back() == '\r' ? "\r" : "");
                line = line.substr(0, v0) + it->second + spaces + line.substr(c);
            }
        }
        out += line;
        if (!last) out += "\n";
    }
    for (auto &kv : user)
        if (!used.count(kv.first) && kv.first.rfind("rough_", 0) == 0) out += kv.first + " = " + kv.second + "\n";
    return out;
}

}
