#pragma once
#include "math/MathUtils.h"

namespace lmao {

struct Transform {
    vec3 position{0, 0, 0};
    quat rotation{1, 0, 0, 0};
    vec3 scale{1, 1, 1};

    mat4 modelMatrix() const {
        mat4 T = glm::translate(mat4(1), position);
        mat4 R = glm::mat4_cast(rotation);
        mat4 S = glm::scale(mat4(1), scale);
        return T * R * S;
    }
};

} // namespace lmao
