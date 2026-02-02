#pragma once
#include <chrono>

namespace lmao {

class Timer {
public:
    using Clock = std::chrono::high_resolution_clock;

    void reset() {
        m_start = Clock::now();
        m_last = m_start;
        m_frameCount = 0;
    }

    void tick() {
        auto now = Clock::now();
        m_dt = std::chrono::duration<float>(now - m_last).count();
        m_elapsed = std::chrono::duration<float>(now - m_start).count();
        m_last = now;
        m_frameCount++;
    }

    float dt() const { return m_dt; }
    float elapsed() const { return m_elapsed; }
    uint64_t frameCount() const { return m_frameCount; }
    float fps() const { return m_dt > 0.0f ? 1.0f / m_dt : 0.0f; }

private:
    Clock::time_point m_start{Clock::now()};
    Clock::time_point m_last{Clock::now()};
    float m_dt = 0.0f;
    float m_elapsed = 0.0f;
    uint64_t m_frameCount = 0;
};

} // namespace lmao
