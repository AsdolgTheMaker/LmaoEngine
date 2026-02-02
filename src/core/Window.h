#pragma once
#include <cstdint>
#include <functional>

struct GLFWwindow;

namespace lmao {

struct WindowConfig {
    uint32_t width = 1600;
    uint32_t height = 900;
    const char* title = "LmaoEngine";
    bool resizable = true;
};

class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool init(const WindowConfig& config);
    void shutdown();

    bool shouldClose() const;
    void pollEvents();

    GLFWwindow* handle() const { return m_window; }
    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    bool wasResized() const { return m_resized; }
    void clearResizedFlag() { m_resized = false; }

    using ResizeCallback = std::function<void(uint32_t, uint32_t)>;
    void setResizeCallback(ResizeCallback cb) { m_resizeCb = std::move(cb); }

private:
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    GLFWwindow* m_window = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_resized = false;
    ResizeCallback m_resizeCb;
};

} // namespace lmao
