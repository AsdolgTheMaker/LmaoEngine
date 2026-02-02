#pragma once
#include "math/MathUtils.h"

namespace lmao {

struct DirectionalLight {
    vec3 direction{0, -1, 0};
    float _pad0 = 0;
    vec3 color{1, 1, 1};
    float intensity = 1.0f;
};

struct PointLight {
    vec3 position{0, 0, 0};
    float range = 10.0f;
    vec3 color{1, 1, 1};
    float intensity = 1.0f;
};

} // namespace lmao
