#pragma once
#include <vector>
#include <algorithm>
#include <cmath>

namespace PhysicsLookup {
    struct DataPoint {
        double bad_ar;
        double temp;
    };

    // Data from Cycle 0 of physics_logs_20260128_001853
    // Sorted by Bad AR descending (0.41 -> 0.00)
    static const std::vector<DataPoint> lookup_table = {
        {0.4123, 61.95}, {0.3949, 58.04}, {0.3768, 54.37}, {0.3593, 50.94},
        {0.3427, 47.72}, {0.3264, 44.70}, {0.3106, 41.88}, {0.2953, 39.23},
        {0.2805, 36.76}, {0.2667, 34.43}, {0.2532, 32.26}, {0.2405, 30.22},
        {0.2284, 28.31}, {0.2165, 26.52}, {0.2057, 24.84}, {0.1955, 23.29},
        {0.1862, 21.82}, {0.1765, 20.45}, {0.1676, 19.14}, {0.1596, 17.93},
        {0.1520, 16.80}, {0.1446, 15.73}, {0.1376, 14.74}, {0.1313, 13.81},
        {0.1253, 12.94}, {0.1197, 12.12}, {0.1143, 11.35}, {0.1092, 10.63},
        {0.1055, 10.10}, {0.0996, 9.34},  {0.0952, 8.75},  {0.0927, 8.19},
        {0.0889, 7.67},  {0.0850, 7.19},  {0.0817, 6.73},  {0.0777, 6.31},
        {0.0754, 5.91},  {0.0726, 5.54},  {0.0452, 5.17},  {0.0426, 4.86},
        {0.0406, 4.55},  {0.0389, 4.27},  {0.0367, 4.00},  {0.0352, 3.74},
        {0.0336, 3.51},  {0.0318, 3.28},  {0.0308, 3.08},  {0.0295, 2.88},
        {0.0281, 2.70},  {0.0268, 2.53},  {0.0257, 2.37},  {0.0246, 2.22},
        {0.0238, 2.08},  {0.0222, 1.95},  {0.0211, 1.83},  {0.0204, 1.71},
        {0.0197, 1.60},  {0.0189, 1.50},  {0.0176, 1.41},  {0.0174, 1.32},
        {0.0167, 1.23},  {0.0160, 1.16},  {0.0159, 1.08},  {0.0155, 1.01},
        {0.0143, 0.95},  {0.0140, 0.89},  {0.0138, 0.83},  {0.0136, 0.78},
        {0.0128, 0.73},  {0.0121, 0.69},  {0.0122, 0.64},  {0.0118, 0.60},
        {0.0116, 0.56},  {0.0114, 0.53},  {0.0112, 0.49},  {0.0113, 0.46}
    };

    inline double get_temp_for_bad_ar(double target_bad_ar) {
        // Handle out of bounds
        if (target_bad_ar >= lookup_table.front().bad_ar) return lookup_table.front().temp;
        if (target_bad_ar <= lookup_table.back().bad_ar) return lookup_table.back().temp;

        // Linear Interpolation
        for (size_t i = 0; i < lookup_table.size() - 1; ++i) {
            const auto& p1 = lookup_table[i];
            const auto& p2 = lookup_table[i + 1];

            // Because table is sorted descending by Bad AR
            if (target_bad_ar <= p1.bad_ar && target_bad_ar > p2.bad_ar) {
                double t = (target_bad_ar - p2.bad_ar) / (p1.bad_ar - p2.bad_ar);
                return p2.temp + t * (p1.temp - p2.temp);
            }
        }
        return lookup_table.back().temp;
    }
}
