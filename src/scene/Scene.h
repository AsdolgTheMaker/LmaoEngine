#pragma once
#include "scene/Camera.h"
#include "scene/Light.h"
#include "scene/Entity.h"
#include <vector>

namespace lmao {

class Scene {
public:
    Scene() = default;

    Entity& createEntity(const std::string& name = "Entity");
    const std::vector<Entity>& entities() const { return m_entities; }
    std::vector<Entity>& entities() { return m_entities; }

    Camera& camera() { return m_camera; }
    const Camera& camera() const { return m_camera; }

    DirectionalLight& directionalLight() { return m_dirLight; }
    const DirectionalLight& directionalLight() const { return m_dirLight; }

private:
    Camera m_camera;
    DirectionalLight m_dirLight;
    std::vector<Entity> m_entities;
};

} // namespace lmao
