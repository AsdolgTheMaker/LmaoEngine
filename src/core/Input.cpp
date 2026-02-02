#include "core/Input.h"
#include <GLFW/glfw3.h>

namespace lmao {

void Input::init(GLFWwindow* window) {
    s_window = window;
    s_keys.fill(false);
    s_keysPressed.fill(false);
    s_mouseButtons.fill(false);
    s_mousePressed.fill(false);
    s_firstMouse = true;

    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);
}

bool Input::keyDown(int key) { return key >= 0 && key < 512 && s_keys[key]; }
bool Input::keyPressed(int key) { return key >= 0 && key < 512 && s_keysPressed[key]; }
bool Input::mouseDown(int button) { return button >= 0 && button < 8 && s_mouseButtons[button]; }
bool Input::mousePressed(int button) { return button >= 0 && button < 8 && s_mousePressed[button]; }

float Input::mouseX() { return s_mouseX; }
float Input::mouseY() { return s_mouseY; }
float Input::mouseDX() { return s_mouseDX; }
float Input::mouseDY() { return s_mouseDY; }
float Input::scrollDY() { return s_scrollDY; }

void Input::setCursorLocked(bool locked) {
    s_cursorLocked = locked;
    glfwSetInputMode(s_window, GLFW_CURSOR,
        locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (locked) s_firstMouse = true;
}

bool Input::isCursorLocked() { return s_cursorLocked; }

void Input::endFrame() {
    s_keysPressed.fill(false);
    s_mousePressed.fill(false);
    s_mouseDX = 0;
    s_mouseDY = 0;
    s_scrollDY = 0;
}

void Input::keyCallback(GLFWwindow*, int key, int, int action, int) {
    if (key < 0 || key >= 512) return;
    if (action == GLFW_PRESS) {
        s_keys[key] = true;
        s_keysPressed[key] = true;
    } else if (action == GLFW_RELEASE) {
        s_keys[key] = false;
    }
}

void Input::mouseButtonCallback(GLFWwindow*, int button, int action, int) {
    if (button < 0 || button >= 8) return;
    if (action == GLFW_PRESS) {
        s_mouseButtons[button] = true;
        s_mousePressed[button] = true;
    } else if (action == GLFW_RELEASE) {
        s_mouseButtons[button] = false;
    }
}

void Input::cursorPosCallback(GLFWwindow*, double x, double y) {
    auto fx = static_cast<float>(x);
    auto fy = static_cast<float>(y);
    if (s_firstMouse) {
        s_mouseX = fx;
        s_mouseY = fy;
        s_firstMouse = false;
    }
    s_mouseDX += fx - s_mouseX;
    s_mouseDY += fy - s_mouseY;
    s_mouseX = fx;
    s_mouseY = fy;
}

void Input::scrollCallback(GLFWwindow*, double, double yoff) {
    s_scrollDY += static_cast<float>(yoff);
}

} // namespace lmao
