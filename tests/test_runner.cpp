#include <iostream>
#include <print>
#include <cassert>
#include "logic.h"

// 테스트 헬퍼 함수
void test_digit_table_integrity() {
    std::println("[TEST] DIGIT_TABLE Integrity...");
    
    // 123 -> {3, 2, 1}
    auto d123 = DIGIT_TABLE[123];
    assert(d123.len == 3);
    assert(d123.digits[0] == 3); // 1의 자리
    assert(d123.digits[1] == 2); // 10의 자리
    assert(d123.digits[2] == 1); // 100의 자리
    
    // 127 -> {7, 2, 1}
    auto d127 = DIGIT_TABLE[127];
    assert(d127.digits[0] == 7);
    
    // 128 -> {8, 2, 1} (문제의 구간)
    auto d128 = DIGIT_TABLE[128];
    assert(d128.len == 3);
    assert(d128.digits[0] == 8); // 1의 자리
    assert(d128.digits[1] == 2); 
    assert(d128.digits[2] == 1);
    
    std::println(" -> PASS");
}

void test_simple_sequence() {
    std::println("[TEST] Simple Sequence (1, 2, 3)...");
    std::array<std::array<int, 14>, 8> board = {0};
    
    // (0,0)=1, (0,1)=2, (0,2)=3 심기
    board[0][0] = 1;
    board[0][1] = 2;
    board[0][2] = 3;
    
    // 1, 2, 3은 당연히 있어야 함
    // 12, 23, 123 등도 가능해야 함
    
    int score = get_score_param(board);
    std::println(" -> Score: {}", score);
    
    // 1, 2, 3은 이어져 있으니 최소 3점 이상
    // 4는 없으니 3점에서 멈추거나, 다른 조합으로 더 갈 수도 있음
    assert(score >= 3); 
    std::println(" -> PASS");
}

void test_127_boundary() {
    std::println("[TEST] 127 -> 128 Boundary Check...");
    std::array<std::array<int, 14>, 8> board = {0};
    
    // 시나리오: 127과 128을 동시에 만들 수 있는 배치
    // 공통 접두사: 1, 2
    // 분기점: 7, 8
    
    // (4,4)=1 -> (4,5)=2 -> (4,6)=7
    //                    -> (3,5)=8 (2의 위쪽)
    
    board[4][4] = 1;
    board[4][5] = 2;
    board[4][6] = 7;
    board[3][5] = 8;
    
    // 수동 검증: 127
    const auto& d127 = DIGIT_TABLE[127];
    bool make127 = can_make(board, d127.digits, d127.len, 1, 4, 4);
    std::println(" -> Can make 127? {}", make127);
    assert(make127 == true);

    // 수동 검증: 128
    const auto& d128 = DIGIT_TABLE[128];
    bool make128 = can_make(board, d128.digits, d128.len, 1, 4, 4);
    std::println(" -> Can make 128? {}", make128);
    assert(make128 == true);
    
    std::println(" -> PASS");
}

void test_get_endpoints_logic() {
    std::println("[TEST] get_endpoints Logic Check...");
    std::array<std::array<int, 14>, 8> board = {0};

    // 시나리오: (0,0)=1 -> (0,1)=2 -> (0,2)=3 경로 생성
    board[0][0] = 1;
    board[0][1] = 2;
    board[0][2] = 3;

    // 123의 끝점(3)인 (0,2)를 찾아내는지 확인
    const auto& d123 = DIGIT_TABLE[123]; // {3, 2, 1}
    std::vector<std::pair<int, int>> endpoints;

    // 시작점인 1(0,0)을 찾아서 호출한다고 가정
    // d123.digits[d123.len - 1] 은 '1'
    if (board[0][0] == 1) {
        get_endpoints(board, d123.digits, d123.len, d123.len, 0, 0, 0, endpoints);
    }

    std::println(" -> Found {} endpoints.", endpoints.size());
    assert(endpoints.size() > 0);

    bool correct_coord_found = false;
    for(auto [r, c] : endpoints) {
        std::println(" -> Endpoint candidate: ({}, {}) Val={}", r, c, board[r][c]);
        if (r == 0 && c == 2 && board[r][c] == 3) {
            correct_coord_found = true;
        }
    }
    
    assert(correct_coord_found == true);
    std::println(" -> PASS");
}


void test_smart_mutation_simulation() {
    std::println("[TEST] Smart Mutation Simulation (127 -> 128)...");
    std::array<std::array<int, 14>, 8> board = {0};
    
    // 127 경로: (2,0)=1 -> (2,1)=2 -> (2,2)=7
    board[2][0] = 1;
    board[2][1] = 2;
    board[2][2] = 7;
    
    const auto& d128 = DIGIT_TABLE[128]; // {8, 2, 1}, len=3
    int prefix_len = 2; // "1, 2" 찾기
    
    std::vector<std::pair<int, int>> endpoints;
    
    // [수정] origin_len 인자 추가! (d128.len = 3)
    get_endpoints(board, d128.digits, d128.len, prefix_len, 0, 2, 0, endpoints);
    
    std::println(" -> Prefix '1,2' endpoints found: {}", endpoints.size());
    
    bool found_prefix_end = false;
    for(auto [r, c] : endpoints) {
        if(r==2 && c==1) found_prefix_end = true;
    }
    
    if (found_prefix_end) {
        std::println(" -> PASS: Correctly handled Little Endian indexing!");
    } else {
        std::println(" -> FAIL: Still broken.");
    }
    assert(found_prefix_end);
}

int main() {
    std::println("=== UNIT TEST START ===");
    
    test_digit_table_integrity();
    test_simple_sequence();
    test_127_boundary();

    test_get_endpoints_logic();
    test_smart_mutation_simulation();
    
    std::println("=== ALL TESTS PASSED ===");
    return 0;
}