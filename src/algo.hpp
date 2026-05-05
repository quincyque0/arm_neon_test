#pragma once
#include <vector>
#include <cstddef>

bool HasNeonSupport();
float DotProductScalar(const std::vector<float>& a, const std::vector<float>& b);
float DotProductNeon(const std::vector<float>& a, const std::vector<float>& b);
