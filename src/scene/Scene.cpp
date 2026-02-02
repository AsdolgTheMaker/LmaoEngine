#include "scene/Scene.h"
#include "core/Log.h"

namespace lmao {

Entity& Scene::createEntity(const std::string& name) {
    m_entities.emplace_back(name);
    LOG(Scene, Debug, "Entity created: %s", name.c_str());
    return m_entities.back();
}

PointLight& Scene::createPointLight() {
    m_pointLights.emplace_back();
    LOG(Scene, Debug, "Point light created (total: %zu)", m_pointLights.size());
    return m_pointLights.back();
}

} // namespace lmao
