#pragma once

#include <array>

// 8 routing tables. Index 0..3 = op1..op4. mod[i][j] == true means op(j+1) modulates op(i+1).
// Modulators always have a higher index than what they modulate, so rendering op4 -> op1 is sufficient.
namespace queen
{
struct Algorithm
{
    std::array<bool, 4> carrier;
    std::array<std::array<bool, 4>, 4> mod;
};

inline constexpr std::array<Algorithm, 8> kAlgorithms { {
    // 1: 4->3->2->1
    { { 1, 0, 0, 0 }, { { { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 }, { 0, 0, 0, 0 } } } },
    // 2: 4->3->(2,1 parallel carriers)
    { { 1, 1, 0, 0 }, { { { 0, 0, 1, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 }, { 0, 0, 0, 0 } } } },
    // 3: (4->2),(3->1)
    { { 1, 1, 0, 0 }, { { { 0, 0, 1, 0 }, { 0, 0, 0, 1 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } } } },
    // 4: (4,3)->2->1
    { { 1, 0, 0, 0 }, { { { 0, 1, 0, 0 }, { 0, 0, 1, 1 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } } } },
    // 5: 4->3->1, 2->1
    { { 1, 0, 0, 0 }, { { { 0, 1, 1, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 1 }, { 0, 0, 0, 0 } } } },
    // 6: 4->(3,2,1)
    { { 1, 1, 1, 0 }, { { { 0, 0, 0, 1 }, { 0, 0, 0, 1 }, { 0, 0, 0, 1 }, { 0, 0, 0, 0 } } } },
    // 7: 4->3, 2, 1 (three carriers)
    { { 1, 1, 1, 0 }, { { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 1 }, { 0, 0, 0, 0 } } } },
    // 8: all four carriers
    { { 1, 1, 1, 1 }, { { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } } } },
} };

constexpr int numCarriers (const Algorithm& a)
{
    int n = 0;
    for (auto c : a.carrier)
        n += c ? 1 : 0;
    return n;
}
} // namespace queen
