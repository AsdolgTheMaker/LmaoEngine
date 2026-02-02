#include "scene/Camera.h"
#include "core/Input.h"
#include "core/Log.h"
#include <GLFW/glfw3.h>
#include <algorithm>

namespace lmao {

void Camera::update(float dt) {
    if (Input::keyPressed(GLFW_KEY_TAB)) {
        if (m_mode == Mode::FPS) {
            setMode(Mode::Orbit);
        } else {
            setMode(Mode::FPS);
        }
    }

    if (m_mode == Mode::FPS) {
        // Mouse look (only when cursor locked)
        if (Input::mouseDown(GLFW_MOUSE_BUTTON_RIGHT)) {
            if (!Input::isCursorLocked()) Input::setCursorLocked(true);
            m_yaw += Input::mouseDX() * lookSensitivity;
            m_pitch -= Input::mouseDY() * lookSensitivity;
            m_pitch = std::clamp(m_pitch, -HALF_PI + 0.01f, HALF_PI - 0.01f);
        } else if (Input::isCursorLocked()) {
            Input::setCursorLocked(false);
        }

        // WASD movement
        vec3 fwd = forward();
        vec3 rt = right();
        vec3 move{0};
        if (Input::keyDown(GLFW_KEY_W)) move += fwd;
        if (Input::keyDown(GLFW_KEY_S)) move -= fwd;
        if (Input::keyDown(GLFW_KEY_D)) move += rt;
        if (Input::keyDown(GLFW_KEY_A)) move -= rt;
        if (Input::keyDown(GLFW_KEY_E) || Input::keyDown(GLFW_KEY_SPACE)) move.y += 1.0f;
        if (Input::keyDown(GLFW_KEY_Q) || Input::keyDown(GLFW_KEY_LEFT_CONTROL)) move.y -= 1.0f;

        if (glm::length(move) > 0.001f) {
            float speed = moveSpeed;
            if (Input::keyDown(GLFW_KEY_LEFT_SHIFT)) speed *= 3.0f;
            m_position += glm::normalize(move) * speed * dt;
        }
    } else {
        // Orbit mode
        if (Input::mouseDown(GLFW_MOUSE_BUTTON_RIGHT) || Input::mouseDown(GLFW_MOUSE_BUTTON_LEFT)) {
            if (!Input::isCursorLocked()) Input::setCursorLocked(true);
            m_orbitYaw += Input::mouseDX() * lookSensitivity;
            m_orbitPitch -= Input::mouseDY() * lookSensitivity;
            m_orbitPitch = std::clamp(m_orbitPitch, -HALF_PI + 0.05f, HALF_PI - 0.05f);
        } else if (Input::isCursorLocked()) {
            Input::setCursorLocked(false);
        }

        m_distance -= Input::scrollDY() * scrollSpeed;
        m_distance = std::clamp(m_distance, 0.5f, 100.0f);
    }
}

void Camera::setMode(Mode mode) {
    if (m_mode == mode) return;
    if (m_mode == Mode::FPS && mode == Mode::Orbit) {
        m_target = m_position + forward() * m_distance;
        m_orbitYaw = m_yaw;
        m_orbitPitch = m_pitch;
    } else if (m_mode == Mode::Orbit && mode == Mode::FPS) {
        m_position = position();
        m_yaw = m_orbitYaw;
        m_pitch = m_orbitPitch;
    }
    m_mode = mode;
    LOG(Input, Debug, "Camera mode: %s", mode == Mode::FPS ? "FPS" : "Orbit");
}

void Camera::setYawPitch(float yaw, float pitch) {
    m_yaw = yaw;
    m_pitch = std::clamp(pitch, -HALF_PI + 0.01f, HALF_PI - 0.01f);
}

void Camera::setPerspective(float fovYDeg, float aspect, float nearPlane, float farPlane) {
    m_fovY = fovYDeg;
    m_aspect = aspect;
    m_near = nearPlane;
    m_far = farPlane;
    updateProjection();
}

void Camera::setAspect(float aspect) {
    m_aspect = aspect;
    updateProjection();
}

void Camera::updateProjection() {
    // Reversed-Z: swap near/far
    m_projection = glm::perspective(glm::radians(m_fovY), m_aspect, m_far, m_near);
}

vec3 Camera::position() const {
    if (m_mode == Mode::FPS) return m_position;
    // Orbit: compute position from target + spherical coords
    float x = std::cos(m_orbitPitch) * std::cos(m_orbitYaw);
    float y = std::sin(m_orbitPitch);
    float z = std::cos(m_orbitPitch) * std::sin(m_orbitYaw);
    return m_target + vec3(x, y, z) * m_distance;
}

vec3 Camera::forward() const {
    if (m_mode == Mode::FPS) {
        return vec3(std::cos(m_pitch) * std::cos(m_yaw),
                    std::sin(m_pitch),
                    std::cos(m_pitch) * std::sin(m_yaw));
    }
    return glm::normalize(m_target - position());
}

vec3 Camera::right() const {
    return glm::normalize(glm::cross(forward(), vec3(0, 1, 0)));
}

vec3 Camera::up() const {
    return glm::normalize(glm::cross(right(), forward()));
}

mat4 Camera::viewMatrix() const {
    vec3 pos = position();
    return glm::lookAt(pos, pos + forward(), vec3(0, 1, 0));
}

} // namespace lmao
