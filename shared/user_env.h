#pragma once

#include <string>
#include <string_view>
#include <vector>

// The user's session environment overrides: ~/.config/freebsdx/environment.
// One `KEY=value` per line; the value runs to end of line, taken literally (no
// quoting or expansion). Blank lines and `#` comments are ignored. Written by
// the compositor's com.freebsdx.EnvVars service, read by xshell before it
// launches an app so the app inherits the overrides. Dependency-free; no I/O.
namespace fbx::shared::userenv {

struct Var {
    std::string key;
    std::string value;
};

constexpr size_t kMaxKey   = 63;   // EnvSetRequest::key_ is 64 with the NUL
constexpr size_t kMaxValue = 191;  // EnvSetRequest::value_ is 192

// POSIX-portable name: [A-Za-z_][A-Za-z0-9_]*.
inline bool validKey(std::string_view k) {
    if (k.empty() || k.size() > kMaxKey) return false;
    if (k[0] >= '0' && k[0] <= '9') return false;
    for (char c : k) {
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') || c == '_';
        if (!ok) return false;
    }
    return true;
}

// One line per var, so no line breaks; NUL can't live in environ.
inline bool validValue(std::string_view v) {
    if (v.size() > kMaxValue) return false;
    for (char c : v)
        if (c == '\n' || c == '\r' || c == '\0') return false;
    return true;
}

// Malformed lines are dropped; a repeated key keeps its first position and its
// last value.
inline std::vector<Var> parse(std::string_view text) {
    std::vector<Var> vars;
    size_t start = 0;
    while (start < text.size()) {
        size_t nl = text.find('\n', start);
        if (nl == std::string_view::npos) nl = text.size();
        std::string_view line = text.substr(start, nl - start);
        start = nl + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string_view::npos) continue;
        std::string_view k = line.substr(0, eq), v = line.substr(eq + 1);
        if (!validKey(k) || !validValue(v)) continue;
        bool replaced = false;
        for (auto& e : vars)
            if (e.key == k) { e.value = std::string(v); replaced = true; break; }
        if (!replaced) vars.push_back({std::string(k), std::string(v)});
    }
    return vars;
}

inline std::string serialize(const std::vector<Var>& vars) {
    std::string out = "# FreeBSD-X session environment (managed by Settings)\n";
    for (const auto& e : vars) out += e.key + "=" + e.value + "\n";
    return out;
}

// Add or update; false if the key or value is invalid.
inline bool set(std::vector<Var>& vars, std::string_view key, std::string_view value) {
    if (!validKey(key) || !validValue(value)) return false;
    for (auto& e : vars)
        if (e.key == key) { e.value = std::string(value); return true; }
    vars.push_back({std::string(key), std::string(value)});
    return true;
}

// False if the key wasn't there.
inline bool unset(std::vector<Var>& vars, std::string_view key) {
    for (auto it = vars.begin(); it != vars.end(); ++it)
        if (it->key == key) { vars.erase(it); return true; }
    return false;
}

inline std::string filePath(std::string_view home) {
    return std::string(home) + "/.config/freebsdx/environment";
}

}  // namespace fbx::shared::userenv
