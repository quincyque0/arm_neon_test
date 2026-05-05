#include "algo.hpp"

#if defined(__ARM_NEON) || defined(__aarch64__)
    #include <arm_neon.h>
    #define ARMP_HAVE_NEON 1
#endif

#if defined(__linux__)
#include <sys/auxv.h>
#ifndef HWCAP_ASIMD
#define HWCAP_ASIMD (1 << 1)
#endif
#ifndef HWCAP_NEON
#define HWCAP_NEON (1 << 12)
#endif
#endif

bool HasNeonSupport() {
#if defined(__aarch64__)
    return true;
#elif defined(__ARM_NEON)
    #if defined(__linux__)
        unsigned long hwcap = getauxval(AT_HWCAP);
        return (hwcap & HWCAP_NEON) != 0;
    #else
        return true;
    #endif
#else
    return false;
#endif
}

// ──────────────────────────────────────────────────────────────
// DotProductScalar вынесен в algo_scalar.cpp и компилируется
// с флагом -fno-tree-vectorize, чтобы компилятор не мог
// автовекторизировать скалярный цикл.
// ──────────────────────────────────────────────────────────────

float DotProductNeon(const std::vector<float>& a, const std::vector<float>& b) {
#ifdef ARMP_HAVE_NEON
    if (!HasNeonSupport()) return DotProductScalar(a, b);

    float sum = 0.0f;
    size_t n = a.size();
    size_t i = 0;

    float32x4_t sum_vec = vdupq_n_f32(0.0f);
    for (; i + 3 < n; i += 4) {
        float32x4_t va = vld1q_f32(&a[i]);
        float32x4_t vb = vld1q_f32(&b[i]);
        sum_vec = vmlaq_f32(sum_vec, va, vb);
    }

    float temp[4];
    vst1q_f32(temp, sum_vec);
    sum += temp[0] + temp[1] + temp[2] + temp[3];

    for (; i < n; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
#else
    return DotProductScalar(a, b);
#endif
}
