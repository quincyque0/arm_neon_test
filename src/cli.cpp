#include "cli.hpp"
#include "algo.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>

namespace AppCli {

int Run() {
    std::cout << "=== ARM NEON Vector Dot Product (CLI) ===\n";
    if (HasNeonSupport()) {
        std::cout << "NEON support: YES\n";
    } else {
        std::cout << "NEON support: NO (Fallback to scalar)\n";
    }

    std::vector<size_t> sizes = {1000000, 5000000, 10000000, 50000000};
    
    for (size_t N : sizes) {
        std::cout << "\n--- Size N = " << N << " ---\n";
        std::vector<float> a(N), b(N);
        std::mt19937 gen(42);
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (size_t i = 0; i < N; ++i) {
            a[i] = dist(gen);
            b[i] = dist(gen);
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        float res_scalar = DotProductScalar(a, b);
        auto t2 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> ms_scalar = t2 - t1;

        auto t3 = std::chrono::high_resolution_clock::now();
        float res_neon = DotProductNeon(a, b);
        auto t4 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> ms_neon = t4 - t3;

        std::cout << "Scalar result: " << res_scalar << " in " << ms_scalar.count() << " ms\n";
        std::cout << "NEON result:   " << res_neon << " in " << ms_neon.count() << " ms\n";
        if (ms_neon.count() > 0) {
            std::cout << "Speedup:       x" << (ms_scalar.count() / ms_neon.count()) << "\n";
        }
    }

    return 0;
}

}
