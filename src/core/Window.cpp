#include "core/Window.h"
#include "core/Log.h"
#include <GLFW/glfw3.h>

namespace lmao {

Window::~Window() {
    shutdown();
}

bool Window::init(const WindowConfig& config) {
    if (!glfwInit()) {
        LMAO_ERROR("Failed to initialize GLFW");
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

    m_window = glfwCreateWindow(
        static_cast<int>(config.width),
        static_cast<int>(config.height),
        config.title, nullptr, nullptr
    );
    if (!m_window) {
        LMAO_ERROR("Failed to create GLFW window");
        glfwTerminate();
        return false;
    }

    m_width = config.width;
    m_height = config.height;

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);

    LMAO_INFO("Window created: %ux%u", m_width, m_height);
    return true;
}

void Window::shutdown() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
        glfwTerminate();
    }
}

bool Window::shouldClose() const {
    return m_window && glfwWindowShouldClose(m_window);
}

void Window::pollEvents() {
    glfwPollEvents();
}

void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    self->m_width = static_cast<uint32_t>(width);
    self->m_height = static_cast<uint32_t>(height);
    self->m_resized = true;
    if (self->m_resizeCb) {
        self->m_resizeCb(self->m_width, self->m_height);
    }
}

} // namespace lmao
