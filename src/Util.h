#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace insight::util {

[[nodiscard]] inline std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

inline std::string& replaceAll(std::string& s, std::string_view from, std::string_view to) {
    if (from.empty()) {
        return s;
    }
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.length(), to);
        pos += to.length();
    }
    return s;
}

inline std::string& trimInPlace(std::string& s) {
    auto isNotSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), isNotSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), isNotSpace).base(), s.end());
    return s;
}

// Convert the classic `&code` color codes (e.g. &a &l &r) into `§code`,
// leaving `&&` as a literal ampersand. Existing § characters are untouched.
[[nodiscard]] inline std::string colorizeAmpersand(std::string s) {
    static constexpr std::string_view codes = "0123456789abcdefklmnor";
    std::string                       out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '&' && i + 1 < s.size()) {
            char n = s[i + 1];
            if (n == '&') {
                out += '&';
                ++i;
                continue;
            }
            if (codes.find(n) != std::string_view::npos) {
                out += '\xA7';
                out += n;
                ++i;
                continue;
            }
        }
        out += c;
    }
    return out;
}

// Format a distance / coordinate nicely, trimming trailing zeros: 3.50 -> 3.5, 10.0 -> 10
[[nodiscard]] inline std::string trimNumber(double v, int maxDecimals = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(maxDecimals) << v;
    std::string s = oss.str();
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') {
            s.pop_back();
        }
        if (!s.empty() && s.back() == '.') {
            s.pop_back();
        }
    }
    return s;
}

// Simple wrapped newline join for multi-line display text.
[[nodiscard]] inline std::vector<std::string> splitLines(std::string const& text) {
    std::vector<std::string> lines;
    size_t                   start = 0;
    while (true) {
        auto pos = text.find('\n', start);
        if (pos == std::string::npos) {
            lines.emplace_back(text.substr(start));
            break;
        }
        lines.emplace_back(text.substr(start, pos - start));
        start = pos + 1;
    }
    return lines;
}

} // namespace insight::util
