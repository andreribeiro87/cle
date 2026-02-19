#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "stats.h"

// ---------------------------------------------------------------------------
// HashMap type with heterogeneous lookup: lets us use std::string_view as key
// against a map whose stored keys are std::string — zero allocation on the
// hot lookup path.
// ---------------------------------------------------------------------------
struct TransparentHash
{
    using is_transparent = void;
    std::size_t operator()(std::string_view sv) const noexcept
    {
        return std::hash<std::string_view>{}(sv);
    }
};

using StationMap = std::unordered_map<
    std::string,
    ValuesFor,
    TransparentHash,
    std::equal_to<>>; // equal_to<void> enables heterogeneous find()

// ---------------------------------------------------------------------------
// Fast temperature parser.
//
// The spec guarantees the format is  [-][0-9]{1,2}.[0-9]
// We exploit the fixed structure to avoid any branch on digit count beyond
// the one check for the decimal point position.
//
// Returns the temperature * 10 as int32_t.
// *end_out is set to one past the last consumed character (the digit AFTER
// the '.').  The caller should skip one '\n' after that.
// ---------------------------------------------------------------------------
[[nodiscard]]
inline int32_t parse_temp_x10(const char *__restrict__ s,
                              const char **__restrict__ end_out) noexcept
{
    const bool neg = (*s == '-');
    if (neg)
        ++s;

    int32_t v = static_cast<uint8_t>(*s++) - '0';
    if (*s != '.')
        v = v * 10 + (static_cast<uint8_t>(*s++) - '0');

    ++s; // skip '.'

    v = v * 10 + (static_cast<uint8_t>(*s++) - '0');

    *end_out = s;
    return neg ? -v : v;
}

// ---------------------------------------------------------------------------
// Process one decompressed block in-place.
//
// Scans the raw character buffer byte-by-byte, avoiding any heap allocation
// for individual lines.  Only allocates a std::string when a truly new
// station name is first inserted into the map.
// ---------------------------------------------------------------------------
inline void process_block(const char *__restrict__ buf,
                          int size,
                          StationMap &data) noexcept
{
    const char *p = buf;
    const char *end = buf + size;

    while (p < end)
    {
        const char *semi = p;
        while (semi < end && *semi != ';')
            ++semi;
        if (semi >= end)
            break;

        std::string_view key(p, static_cast<std::size_t>(semi - p));

        const char *after_temp;
        int32_t temp_x10 = parse_temp_x10(semi + 1, &after_temp);

        p = after_temp;
        if (p < end && *p == '\n')
            ++p;

        auto it = data.find(key); // ignore the linter error. It will work 
        if (it == data.end()) [[unlikely]]
        {
            auto [ins, _] = data.emplace(std::string(key), ValuesFor{});
            it = ins;
        }
        it->second.update(temp_x10);
    }
}
