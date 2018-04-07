#pragma once

#include <chrono>

#define INIT_TIMER auto start_time = std::chrono::high_resolution_clock::now()
#define START_TIMER start_time = std::chrono::high_resolution_clock::now()
#define ELAPSED_TIME std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()-start_time).count()

#define PRINT_ELAPSED_TIME(str, microseconds) { if (microseconds == 0) printf(" %s : < 15ms\n", str); else printf(" %s : %gms\n", str, microseconds / (double)1000);}