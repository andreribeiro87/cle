#pragma once

#include <cmath>
#include <cstdint>

/**
 * Online (Welford) running statistics for a station.
 *
 * Temperatures are stored *10 as int32_t to avoid floating-point comparisons
 * in the hot path.  The Welford mean/M2 accumulation uses double.
 */
struct ValuesFor
{
    int64_t count = 0;
    double mean = 0.0;
    double M2 = 0.0;
    int32_t min_x10 = INT32_MAX;
    int32_t max_x10 = INT32_MIN;

    /// Update statistics with a new temperature (supplied as value * 10).
    inline void update(int32_t val_x10) noexcept
    {
        ++count;
        // Welford online algorithm
        double val = val_x10 * 0.1;
        double delta = val - mean;
        mean += delta / static_cast<double>(count);
        double delta2 = val - mean;
        M2 += delta * delta2;

        if (val_x10 < min_x10)
            min_x10 = val_x10;
        if (val_x10 > max_x10)
            max_x10 = val_x10;
    }

    double variance() const noexcept { return M2 / static_cast<double>(count); }
    double stddev() const noexcept { return std::sqrt(variance()); }
    double min() const noexcept { return min_x10 * 0.1; }
    double max() const noexcept { return max_x10 * 0.1; }
};
