#pragma once
#include <array>
#include <cstdint>
#include <algorithm>
#include "core/board.h"
#include "utils/config.h"

constexpr int MAX_PRECOMPUTE = Config4D::BASIS_MAX_RANGE; 
constexpr int FREQ_LIMIT = Config4D::BASIS_MAX_RANGE;

struct NumberData {
    uint8_t len;
    uint8_t digits[6]; // 역순 저장 (Little Endian)
};

// 컴파일 타임 테이블 생성
consteval auto generate_digit_table() {
    std::array<NumberData, MAX_PRECOMPUTE> table{};
    for (int i = 1; i < MAX_PRECOMPUTE; ++i) {
        int temp = i;
        int idx = 0;
        while (temp > 0) {
            table[i].digits[idx++] = static_cast<uint8_t>(temp % 10);
            temp /= 10;
        }
        table[i].len = static_cast<uint8_t>(idx);
    }
    return table;
}
constexpr auto DIGIT_TABLE = generate_digit_table();

struct SearchTarget {
    int num;
    int weight; // 1 or 2
};

struct SumTarget {
    int num;
    int value; // The value to add to the score (num, or num + rev_num)
};

// 헬퍼: 숫자 뒤집기
constexpr int reverse_int(int n) {
    int r = 0;
    while (n > 0) {
        r = r * 10 + n % 10;
        n /= 10;
    }
    return r;
}

// 헬퍼: 필요한 배열 크기 계산
consteval int count_needed_targets() {
    int count = 0;
    for (int i = 1; i < FREQ_LIMIT; ++i) {
        int rev = reverse_int(i);
        if (rev < 1) count++; // Should not happen for i >= 1
        else if (rev >= FREQ_LIMIT) count++; // Reverse out of range
        else if (i < rev) count++;     // Pair
        else if (i == rev) count++;    // Palindrome
    }
    return count;
}

constexpr int OPTIMIZED_TARGET_COUNT = count_needed_targets();

// 실제 테이블 생성
consteval auto generate_freq_targets() {
    std::array<SearchTarget, OPTIMIZED_TARGET_COUNT> table{};
    int idx = 0;
    for (int i = 1; i < FREQ_LIMIT; ++i) {
        int rev = reverse_int(i);
        if (rev >= FREQ_LIMIT) {
             table[idx++] = {i, 1};
        } else {
            if (i < rev) {
                table[idx++] = {i, 2};
            } else if (i == rev) {
                table[idx++] = {i, 1};
            }
        }
    }
    return table;
}

// 전역 접근 가능한 컴파일 타임 상수
constexpr auto FREQ_TARGETS = generate_freq_targets();


// Helper: Count needed targets for Sum Score (1 ~ BASIS_MAX_RANGE-1)
consteval int count_sum_targets() {
    int count = 0;
    for (int i = 1; i < Config4D::BASIS_MAX_RANGE; ++i) {
        int rev = reverse_int(i);
        if (rev >= Config4D::BASIS_MAX_RANGE) count++;      
        else if (i < rev) count++;     
        else if (i == rev) count++;
        else { // i > rev
            if (reverse_int(rev) != i) count++; // Asymmetric (e.g. 20 -> 2)
        }
    }
    return count;
}

constexpr int OPTIMIZED_SUM_TARGET_COUNT = count_sum_targets();

// Generate SUM_TARGETS table
consteval auto generate_sum_targets() {
    std::array<SumTarget, OPTIMIZED_SUM_TARGET_COUNT> table{};
    int idx = 0;
    for (int i = 1; i < Config4D::BASIS_MAX_RANGE; ++i) {
        int rev = reverse_int(i);
        if (rev >= Config4D::BASIS_MAX_RANGE) {
            table[idx++] = {i, 1000000 / i}; // Single: Non-linear weight
        } else {
            if (i < rev) {
                table[idx++] = {i, (1000000 / i) + (1000000 / rev)}; 
            } else if (i == rev) {
                table[idx++] = {i, 1000000 / i}; // Palindrome
            } else {
                // i > rev
                if (reverse_int(rev) != i) {
                    // Asymmetric (e.g. 20 -> 2). Treat as Single.
                    table[idx++] = {i, 1000000 / i};
                }
            }
        }
    }
    return table;
}

constexpr auto SUM_TARGETS = generate_sum_targets();

// ------------------------------------------------------------------
// Hybrid Score Targets: (1,000,000 / i) + 803
// ------------------------------------------------------------------
consteval auto generate_hybrid_targets() {
    std::array<SumTarget, OPTIMIZED_SUM_TARGET_COUNT> table{};
    int idx = 0;
    // We can use integer arithmetic directly.
    auto weight = [](int n) -> int {
        return (1000000 / n) + 803;
    };

    for (int i = 1; i < Config4D::BASIS_MAX_RANGE; ++i) {
        int rev = reverse_int(i);
        if (rev >= Config4D::BASIS_MAX_RANGE) {
            table[idx++] = {i, weight(i)}; 
        } else {
            if (i < rev) {
                // Symmetric pair
                table[idx++] = {i, weight(i) + weight(rev)}; 
            } else if (i == rev) {
                // Palindrome
                table[idx++] = {i, weight(i)}; 
            } else {
                // i > rev
                if (reverse_int(rev) != i) {
                    // Asymmetric (e.g. 20 -> 2)
                    table[idx++] = {i, weight(i)};
                }
            }
        }
    }
    return table;
}

constexpr auto HYBRID_TARGETS = generate_hybrid_targets();


// ------------------------------------------------------------------
// SQRT Score Targets: 10,000 / sqrt(i)
// ------------------------------------------------------------------
// We need a consteval sqrt or just a simple loop for integer sqrt since this is compile time.
constexpr int integer_sqrt(int n) {
    if (n < 0) return -1;
    if (n == 0) return 0;
    long long left = 1, right = n;
    while (left <= right) {
        long long mid = left + (right - left) / 2;
        if (mid * mid == n) return static_cast<int>(mid);
        if (mid * mid < n) left = mid + 1;
        else right = mid - 1;
    }
    return static_cast<int>(right);
}

consteval auto generate_hybrid_sqrt_targets() {
    std::array<SumTarget, OPTIMIZED_SUM_TARGET_COUNT> table{};
    int idx = 0;
    
    auto weight = [](int n) -> int {
        // 10000 / sqrt(n) + 185
        int sq = integer_sqrt(n);
        if (sq == 0) return 10000 + 185; // Should not happen for i >= 1
        return (10000 / sq) + 185;
    };

    for (int i = 1; i < Config4D::BASIS_MAX_RANGE; ++i) {
        int rev = reverse_int(i);
        if (rev >= Config4D::BASIS_MAX_RANGE) {
            table[idx++] = {i, weight(i)}; 
        } else {
            if (i < rev) {
                table[idx++] = {i, weight(i) + weight(rev)}; 
            } else if (i == rev) {
                table[idx++] = {i, weight(i)}; 
            } else {
                if (reverse_int(rev) != i) {
                    table[idx++] = {i, weight(i)};
                }
            }
        }
    }
    return table;
}

constexpr auto HYBRID_SQRT_TARGETS = generate_hybrid_sqrt_targets();

// Temperature Scaling Constants
// Goal: Normalize Max Score to BASIS_MAX_RANGE - 1
// Constant = MaxScore / (BASIS_MAX_RANGE - 1)

consteval double calculate_max_freq_score() {
    double total = 0;
    for (int i = 1; i < FREQ_LIMIT; ++i) total += 1.0;
    return total;
}
constexpr double MAX_FREQ_SCORE = calculate_max_freq_score();
constexpr double TEMP_SCALE_FREQ = MAX_FREQ_SCORE / (double)(Config4D::BASIS_MAX_RANGE - 1);

consteval double calculate_max_sum_score() {
    double total = 0;
    for (int i = 1; i < Config4D::BASIS_MAX_RANGE; ++i) total += (1000000.0 / i);
    return total;
}
constexpr double MAX_SUM_SCORE = calculate_max_sum_score();
constexpr double TEMP_SCALE_SUM = MAX_SUM_SCORE / (double)(Config4D::BASIS_MAX_RANGE - 1);

consteval double calculate_max_hybrid_score() {
    double total = 0;
    for (int i = 1; i < Config4D::BASIS_MAX_RANGE; ++i) total += ((1000000.0 / i) + 803.0);
    return total;
}
constexpr double MAX_HYBRID_SCORE = calculate_max_hybrid_score();
constexpr double TEMP_SCALE_HYBRID = MAX_HYBRID_SCORE / (double)(Config4D::BASIS_MAX_RANGE - 1);

consteval double calculate_max_hybrid_sqrt_score() {
    double total = 0;
    for (int i = 1; i < Config4D::BASIS_MAX_RANGE; ++i) {
        int sq = integer_sqrt(i);
        total += ((10000.0 / (sq > 0 ? sq : 1)) + 185.0);
    }
    return total;
}
constexpr double MAX_HYBRID_SQRT_SCORE = calculate_max_hybrid_sqrt_score();
constexpr double TEMP_SCALE_HYBRID_SQRT = MAX_HYBRID_SQRT_SCORE / (double)(Config4D::BASIS_MAX_RANGE - 1);