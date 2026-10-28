#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// XDG autostart entries (~/.config/autostart/*.desktop). The compositor's
// com.freebsdx.StartupItems service lists and toggles them; xshell launches
// the enabled ones once per session. Only the [Desktop Entry] group is read or
// rewritten. Dependency-free; no I/O.
namespace fbx::shared::autostart {

// Our name for OnlyShowIn= / NotShowIn= (XDG_CURRENT_DESKTOP).
constexpr std::string_view kDesktopName = "FreeBSD-X";

struct Entry {
    std::string name;
    std::string exec;
    bool        hidden        = false;  // Hidden=true
    bool        gnome_enabled = true;   // X-GNOME-Autostart-enabled=false → off
    std::string only_show_in;           // raw ;-list
    std::string not_show_in;
};

namespace detail {
inline std::vector<std::string_view> lines(std::string_view s) {
    std::vector<std::string_view> out;
    size_t start = 0;
    while (start < s.size()) {
        size_t nl = s.find('\n', start);
        if (nl == std::string_view::npos) nl = s.size();
        std::string_view l = s.substr(start, nl - start);
        if (!l.empty() && l.back() == '\r') l.remove_suffix(1);
        out.push_back(l);
        start = nl + 1;
    }
    return out;
}
inline std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
    return s;
}
inline bool listHas(std::string_view list, std::string_view item) {
    size_t start = 0;
    while (start <= list.size()) {
        size_t semi = list.find(';', start);
        if (semi == std::string_view::npos) semi = list.size();
        if (trim(list.substr(start, semi - start)) == item) return true;
        start = semi + 1;
    }
    return false;
}
// Calls fn(key, value, line_index) for each key=value in [Desktop Entry].
template <typename Fn>
void forEachKey(const std::vector<std::string_view>& ls, Fn fn) {
    bool in_group = false;
    for (size_t i = 0; i < ls.size(); ++i) {
        std::string_view l = trim(ls[i]);
        if (l.empty() || l[0] == '#') continue;
        if (l[0] == '[') { in_group = (l == "[Desktop Entry]"); continue; }
        if (!in_group) continue;
        size_t eq = l.find('=');
        if (eq == std::string_view::npos) continue;
        fn(trim(l.substr(0, eq)), trim(l.substr(eq + 1)), i);
    }
}
}  // namespace detail

// False if there's no [Desktop Entry] Exec= (not launchable).
inline bool parse(std::string_view text, Entry& e) {
    auto ls = detail::lines(text);
    detail::forEachKey(ls, [&](std::string_view k, std::string_view v, size_t) {
        if (k == "Name")                           e.name = std::string(v);  // unlocalized only
        else if (k == "Exec")                      e.exec = std::string(v);
        else if (k == "Hidden")                    e.hidden = (v == "true");
        else if (k == "X-GNOME-Autostart-enabled") e.gnome_enabled = (v != "false");
        else if (k == "OnlyShowIn")                e.only_show_in = std::string(v);
        else if (k == "NotShowIn")                 e.not_show_in = std::string(v);
    });
    return !e.exec.empty();
}

inline bool enabled(const Entry& e) { return !e.hidden && e.gnome_enabled; }

// Enabled, and meant for this desktop.
inline bool shouldLaunch(const Entry& e) {
    if (!enabled(e)) return false;
    if (!e.only_show_in.empty() && !detail::listHas(e.only_show_in, kDesktopName)) return false;
    if (detail::listHas(e.not_show_in, kDesktopName)) return false;
    return true;
}

// Rewrite the entry enabled/disabled: Hidden= is set (added if missing), and
// an existing X-GNOME-Autostart-enabled= is made to agree. False if there's no
// [Desktop Entry] group.
inline bool setEnabled(std::string_view text, bool on, std::string& out) {
    auto ls = detail::lines(text);
    std::vector<std::string> owned(ls.begin(), ls.end());
    long group = -1, hidden = -1, gnome = -1;
    for (size_t i = 0; i < ls.size(); ++i)
        if (detail::trim(ls[i]) == "[Desktop Entry]") { group = static_cast<long>(i); break; }
    if (group < 0) return false;
    detail::forEachKey(ls, [&](std::string_view k, std::string_view, size_t i) {
        if (k == "Hidden") hidden = static_cast<long>(i);
        else if (k == "X-GNOME-Autostart-enabled") gnome = static_cast<long>(i);
    });
    std::string hv = on ? "Hidden=false" : "Hidden=true";
    if (gnome >= 0) owned[gnome] = on ? "X-GNOME-Autostart-enabled=true" : "X-GNOME-Autostart-enabled=false";
    if (hidden >= 0) owned[hidden] = hv;
    else owned.insert(owned.begin() + group + 1, hv);
    out.clear();
    for (const auto& l : owned) out += l + "\n";
    return true;
}

// Exec= → a /bin/sh command line: field codes (%f %U …) dropped, %% → %.
inline std::string commandLine(std::string_view exec) {
    std::string out;
    for (size_t i = 0; i < exec.size(); ++i) {
        if (exec[i] != '%' || i + 1 >= exec.size()) { out += exec[i]; continue; }
        char c = exec[++i];
        if (c == '%') out += '%';
        // every other field code expands to nothing at autostart time
    }
    return std::string(detail::trim(out));
}

// Basename of the program, for display.
inline std::string program(std::string_view exec) {
    std::string_view t = detail::trim(exec);
    if (!t.empty() && t.front() == '"') {
        size_t q = t.find('"', 1);
        t = t.substr(1, q == std::string_view::npos ? std::string_view::npos : q - 1);
    } else {
        t = t.substr(0, t.find_first_of(" \t"));
    }
    size_t slash = t.rfind('/');
    return std::string(slash == std::string_view::npos ? t : t.substr(slash + 1));
}

// Stable id from the file name (FNV-1a), so ids survive edits and reorders.
inline uint32_t itemId(std::string_view file_name) {
    uint32_t h = 2166136261u;
    for (char c : file_name) { h ^= static_cast<uint8_t>(c); h *= 16777619u; }
    return h ? h : 1u;
}

// A file under the autostart dir that we'll manage: visible *.desktop, no '/'.
inline bool isEntryFile(std::string_view name) {
    constexpr std::string_view kExt = ".desktop";
    return name.size() > kExt.size() && name[0] != '.' &&
           name.find('/') == std::string_view::npos &&
           name.substr(name.size() - kExt.size()) == kExt;
}

inline std::string dirPath(std::string_view home) {
    return std::string(home) + "/.config/autostart";
}

}  // namespace fbx::shared::autostart
