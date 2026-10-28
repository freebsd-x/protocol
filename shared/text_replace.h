#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Text replacements: ~/.config/freebsdx/text-replacements, one
// "shortcut<TAB>phrase" per line. The compositor's com.freebsdx.TextReplacement
// service edits the file; xkit text fields read it and expand a shortcut when
// it's followed by a space or punctuation. Dependency-free; no I/O.
namespace fbx::shared::textreplace {

struct Entry {
    std::string shortcut;
    std::string phrase;
};

constexpr size_t kMaxShortcut = 31;    // TextReplacementAddRequest::shortcut_[32]
constexpr size_t kMaxPhrase   = 127;   // ::phrase_[128]

// A shortcut is one "word": no whitespace, so it can be matched before a
// boundary; no tab/newline so the file stays line-oriented.
inline bool validShortcut(std::string_view s) {
    if (s.empty() || s.size() > kMaxShortcut) return false;
    for (char c : s)
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\0') return false;
    return true;
}
inline bool validPhrase(std::string_view p) {
    if (p.empty() || p.size() > kMaxPhrase) return false;
    for (char c : p)
        if (c == '\t' || c == '\n' || c == '\r' || c == '\0') return false;
    return true;
}

// Malformed lines are skipped; a repeated shortcut keeps its last phrase.
inline std::vector<Entry> parse(std::string_view text) {
    std::vector<Entry> out;
    size_t start = 0;
    while (start < text.size()) {
        size_t nl = text.find('\n', start);
        if (nl == std::string_view::npos) nl = text.size();
        std::string_view line = text.substr(start, nl - start);
        start = nl + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.empty() || line[0] == '#') continue;
        size_t tab = line.find('\t');
        if (tab == std::string_view::npos) continue;
        std::string_view k = line.substr(0, tab), v = line.substr(tab + 1);
        if (!validShortcut(k) || !validPhrase(v)) continue;
        bool replaced = false;
        for (auto& e : out)
            if (e.shortcut == k) { e.phrase = std::string(v); replaced = true; break; }
        if (!replaced) out.push_back({std::string(k), std::string(v)});
    }
    return out;
}

inline std::string serialize(const std::vector<Entry>& entries) {
    std::string out = "# FreeBSD-X text replacements (managed by Settings): shortcut<TAB>phrase\n";
    for (const auto& e : entries) out += e.shortcut + "\t" + e.phrase + "\n";
    return out;
}

// Stable id from the shortcut (FNV-1a); never 0.
inline uint32_t entryId(std::string_view shortcut) {
    uint32_t h = 2166136261u;
    for (char c : shortcut) { h ^= static_cast<uint8_t>(c); h *= 16777619u; }
    return h ? h : 1u;
}

// Characters that end a word and trigger expansion of the word before them.
inline bool isBoundary(char c) {
    switch (c) {
    case ' ': case '\t': case '.': case ',': case '!': case '?': case ';': case ':':
    case ')': case ']': case '}': case '"': case '\'':
        return true;
    default:
        return false;
    }
}

// The word ending at `cursor` in `text` (back to the previous whitespace), if
// it's a shortcut: returns its phrase and sets `word_begin`. Exact,
// case-sensitive match.
inline const std::string* match(const std::vector<Entry>& entries, std::string_view text,
                                size_t cursor, size_t& word_begin) {
    if (cursor > text.size() || cursor == 0) return nullptr;
    size_t b = cursor;
    while (b > 0 && text[b - 1] != ' ' && text[b - 1] != '\t' && text[b - 1] != '\n') --b;
    std::string_view word = text.substr(b, cursor - b);
    if (word.empty()) return nullptr;
    for (const auto& e : entries)
        if (e.shortcut == word) { word_begin = b; return &e.phrase; }
    return nullptr;
}

inline std::string filePath(std::string_view home) {
    return std::string(home) + "/.config/freebsdx/text-replacements";
}

}  // namespace fbx::shared::textreplace
