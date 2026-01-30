#include <iostream>
#include "shared_mem.h"

int main() {
    std::cout << "sizeof(ThreadStatus): " << sizeof(ThreadStatus) << std::endl;
    std::cout << "sizeof(ControlCommand): " << sizeof(ControlCommand) << std::endl;
    std::cout << "sizeof(MonitorData): " << sizeof(MonitorData) << std::endl;
    
    MonitorData md;
    std::cout << "Offset of num_threads: " << (uint8_t*)&md.num_threads - (uint8_t*)&md << std::endl;
    std::cout << "Offset of global_best_score: " << (uint8_t*)&md.global_best_score - (uint8_t*)&md << std::endl;
    std::cout << "Offset of cmd: " << (uint8_t*)&md.cmd - (uint8_t*)&md << std::endl;
    std::cout << "Offset of status: " << (uint8_t*)&md.status - (uint8_t*)&md << std::endl;

    ThreadStatus ts;
    std::cout << "Offset of thread_id: " << (uint8_t*)&ts.thread_id - (uint8_t*)&ts << std::endl;
    std::cout << "Offset of current_score: " << (uint8_t*)&ts.current_score - (uint8_t*)&ts << std::endl;
    std::cout << "Offset of best_score: " << (uint8_t*)&ts.best_score - (uint8_t*)&ts << std::endl;
    std::cout << "Offset of temperature: " << (uint8_t*)&ts.temperature - (uint8_t*)&ts << std::endl;
    std::cout << "Offset of total_iter: " << (uint8_t*)&ts.total_iter - (uint8_t*)&ts << std::endl;
    std::cout << "Offset of mode: " << (uint8_t*)&ts.mode - (uint8_t*)&ts << std::endl;
    std::cout << "Offset of trial_id: " << (uint8_t*)&ts.trial_id - (uint8_t*)&ts << std::endl;
    std::cout << "Offset of reheat_factor: " << (uint8_t*)&ts.reheat_factor - (uint8_t*)&ts << std::endl;
    std::cout << "Offset of current_board: " << (uint8_t*)&ts.current_board - (uint8_t*)&ts << std::endl;

    return 0;
}
