#pragma once
#include <array>
#include <cstdint>

struct GLFWwindow;

namespace lmao {

class Input {
public:
    static void init(GLFWwindow* window);

    static bool keyDown(int key);
    static bool keyPressed(int key);
    static bool mouseDown(int button);
    static bool mousePressed(int button);

    static float mouseX();
    static float mouseY();
    static float mouseDX();
    static float mouseDY();
    static float scrollDY();

    static void setCursorLocked(bool locked);
    static bool isCursorLocked();

    // Call at end of frame to reset per-frame deltas
    static void endFrame();

private:
    static void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* w, double x, double y);
    static void scrollCallback(GLFWwindow* w, double xoff, double yoff);

    static inline GLFWwindow* s_window = nullptr;
    static inline std::array<bool, 512> s_keys{};
    static inline std::array<bool, 512> s_keysPressed{};
    static inline std::array<bool, 8> s_mouseButtons{};
    static inline std::array<bool, 8> s_mousePressed{};
    static inline float s_mouseX = 0, s_mouseY = 0;
    static inline float s_mouseDX = 0, s_mouseDY = 0;
    static inline float s_scrollDY = 0;
    static inline bool s_cursorLocked = false;
    static inline bool s_firstMouse = true;
};

} // namespace lmao
