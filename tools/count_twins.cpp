#include <iostream>
#include <vector>
#include <string>
#include <unordered_set>
#include <iomanip>
#include <sstream>

// Mock Config for local tool
namespace Config {
    bool BASIS_USE_PADDING = false;
    int BASIS_MAX_RANGE = 100000;
    int BASIS_PADDING_WIDTH = 5;
}

static std::unordered_set<std::string> get_span_cpp(const std::string& start_s, int max_len) {
    std::unordered_set<std::string> results;
    std::vector<std::pair<std::string, int>> q;
    int n = (int)start_s.length();
    for(int i=0; i<n; ++i) q.push_back({std::string(1, start_s[i]), i});
    int head = 0;
    while(head < (int)q.size()) {
        auto [s, idx] = q[head++];
        results.insert(s);
        int neighbors[2] = {idx - 1, idx + 1};
        for (int ni : neighbors) {
            if (ni >= 0 && ni < n) {
                std::string next_s = s + start_s[ni];
                if ((int)next_s.length() <= max_len) q.push_back({next_s, ni});
            }
        }
    }
    return results;
}

bool has_twin(const std::string& s) {
    for (size_t i = 0; i < s.length() - 1; ++i) {
        if (s[i] == s[i+1]) return true;
    }
    return false;
}

int main() {
    std::unordered_set<std::string> covered_set;
    std::unordered_set<std::string> basis_set;
    int limit_len = 5;

    for (int i = 1; i < 100000; ++i) {
        std::string s = std::to_string(i);
        if (covered_set.count(s)) continue;
        auto span = get_span_cpp(s, limit_len);
        for (const auto& item : span) covered_set.insert(item);
        for (auto it = basis_set.begin(); it != basis_set.end(); ) {
            if (span.count(*it)) it = basis_set.erase(it);
            else ++it;
        }
        basis_set.insert(s);
    }

    int count3 = 0, count4 = 0, count5 = 0;
    int twin3 = 0, twin4 = 0, twin5 = 0;

    for (const auto& s : basis_set) {
        int len = s.length();
        if (len == 3) { count3++; if (has_twin(s)) twin3++; }
        else if (len == 4) { count4++; if (has_twin(s)) twin4++; }
        else if (len == 5) { count5++; if (has_twin(s)) twin5++; }
    }

    std::cout << "Total Basis Strings: " << basis_set.size() << std::endl;
    std::cout << "Len 3: " << count3 << " (Twins: " << twin3 << ")" << std::endl;
    std::cout << "Len 4: " << count4 << " (Twins: " << twin4 << ")" << std::endl;
    std::cout << "Len 5: " << count5 << " (Twins: " << twin5 << ")" << std::endl;
    std::cout << "Total Twins: " << (twin3 + twin4 + twin5) << std::endl;

    return 0;
}
