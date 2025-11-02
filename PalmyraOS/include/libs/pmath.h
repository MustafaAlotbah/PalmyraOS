
#pragma once


namespace PalmyraOS::math {
    constexpr int table_size = 360;
    extern const double sin_table[table_size];
    extern const double cos_table[table_size];

    // Function to fetch sine value based on degree
    inline double sin(int degree) {
        // Normalize degree to [0, 359]
        int index = degree % table_size;
        if (index < 0) index += table_size;  // Handle negative degrees
        return sin_table[index];
    }

    // Function to fetch cosine value based on degree
    inline double cos(int degree) {
        // Normalize degree to [0, 359]
        int index = degree % table_size;
        if (index < 0) index += table_size;  // Handle negative degrees
        return cos_table[index];
    }
}  // namespace PalmyraOS::math
