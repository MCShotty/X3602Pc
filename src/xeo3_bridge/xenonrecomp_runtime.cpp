#include <smmintrin.h>

extern "C" __attribute__((target("sse4.1")))
float roundevenf(const float value) noexcept
{
    const auto input = _mm_set_ss(value);
    const auto rounded = _mm_round_ss(
        input,
        input,
        _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
    return _mm_cvtss_f32(rounded);
}
