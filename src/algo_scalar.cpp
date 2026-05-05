#include "algo.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("no-tree-vectorize")
#endif

#if defined(__GNUC__) || defined(__clang__)
__attribute__((optimize("no-tree-vectorize")))
#endif
float DotProductScalar(const std::vector<float>& a, const std::vector<float>& b) {
    float sum = 0.0f;
    size_t n = a.size();
    for (size_t i = 0; i < n; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}
