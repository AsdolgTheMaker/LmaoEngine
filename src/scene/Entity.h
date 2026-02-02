#pragma once
#include "scene/Transform.h"
#include <memory>
#include <string>

namespace lmao {

class Mesh;
class Material;

struct Entity {
    std::string name;
    Transform transform;
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;

    Entity(const std::string& n = "Entity") : name(n) {}
};

} // namespace lmao
