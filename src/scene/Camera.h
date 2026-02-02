#pragma once
#include "math/MathUtils.h"

namespace lmao {

class Camera {
public:
    enum class Mode { FPS, Orbit };

    Camera() = default;

    void update(float dt);

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    void setPosition(const vec3& pos) { m_position = pos; }
    void setYawPitch(float yaw, float pitch);

    void setTarget(const vec3& target) { m_target = target; }
    void setDistance(float dist) { m_distance = dist; }

    void setPerspective(float fovYDeg, float aspect, float nearPlane, float farPlane);
    void setAspect(float aspect);

    vec3 position() const;
    vec3 forward() const;
    vec3 right() const;
    vec3 up() const;
    mat4 viewMatrix() const;
    mat4 projectionMatrix() const { return m_projection; }

    float moveSpeed = 5.0f;
    float lookSensitivity = 0.002f;
    float scrollSpeed = 2.0f;

private:
    void updateProjection();

    Mode m_mode = Mode::FPS;

    vec3 m_position{0, 2, 5};
    float m_yaw = -HALF_PI;
    float m_pitch = 0.0f;

    vec3 m_target{0, 0, 0};
    float m_distance = 5.0f;
    float m_orbitYaw = 0.0f;
    float m_orbitPitch = 0.3f;

    mat4 m_projection{1};
    float m_fovY = 60.0f;
    float m_aspect = 16.0f / 9.0f;
    float m_near = 0.1f;
    float m_far = 1000.0f;
};

} // namespace lmao
