#include "Animate.h"
#include "config.h"

#include <stddef.h>

#if Animate_Choice < 0 || Animate_Choice > 3
#error "Animate_Choice must be in the range 0..3"
#endif

#if Animate_Choice == 1
#include "img/astronaut.h"
#define ANIMATION_FRAMES astronaut
#define ANIMATION_SIZES astronaut_size
#elif Animate_Choice == 2
#include "img/hutao.h"
#define ANIMATION_FRAMES hutao
#define ANIMATION_SIZES hutao_size
#elif Animate_Choice == 3
#include "img/miku.h"
#define ANIMATION_FRAMES miku
#define ANIMATION_SIZES miku_size
#endif

#if Animate_Choice != 0
namespace
{
constexpr size_t kAnimationFrameCount = sizeof(ANIMATION_FRAMES) / sizeof(ANIMATION_FRAMES[0]);
constexpr size_t kAnimationSizeCount = sizeof(ANIMATION_SIZES) / sizeof(ANIMATION_SIZES[0]);
static_assert(kAnimationFrameCount > 0, "animation must contain at least one frame");
static_assert(kAnimationFrameCount == kAnimationSizeCount,
              "animation frame and size tables must have the same length");

size_t animationFrameIndex = 0;
}
#endif

void imgAnim(const uint8_t **Animate_value, uint32_t *Animate_size)
{
    if (Animate_value == nullptr || Animate_size == nullptr)
        return;

#if Animate_Choice == 0
    *Animate_value = nullptr;
    *Animate_size = 0;
#else
    *Animate_value = static_cast<const uint8_t *>(pgm_read_ptr(&ANIMATION_FRAMES[animationFrameIndex]));
    *Animate_size = pgm_read_dword(&ANIMATION_SIZES[animationFrameIndex]);
    animationFrameIndex = (animationFrameIndex + 1U) % kAnimationFrameCount;
#endif
}

#if Animate_Choice != 0
#undef ANIMATION_FRAMES
#undef ANIMATION_SIZES
#endif
