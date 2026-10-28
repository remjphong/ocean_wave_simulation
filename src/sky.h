#pragma once
#include "render_types.h"
namespace Sky {
    bool init();
    Lighting computeLighting(float phase);
    void render(const FrameContext& frame);
    void destroy();
}