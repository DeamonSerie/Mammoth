#pragma once
#include "InputEvent.hpp"
#include "../ui/MainWindow.hpp"
#include <deque>
#include <mutex>

class Application {
public:
    static Application& instance();

    void init();
    void frame();
    void cleanup();
    void event(const sapp_event* ev);
    // Neutral input dispatch used by all backends (Sokol + SDL).
    void dispatchInput(const Input::Event& ev);

    // Thread-safe input injection used by the SDL backend (its event pump runs
    // on the main thread but ahead of frame()). Events are drained at the top
    // of frame() so drawing sees every move/press in order.
    void postInput(const Input::Event& ev);
    void drainInput();

    // Backend-neutral quit: sokol calls sapp_request_quit(), SDL sets a flag.
    void requestQuit();
    bool shouldQuit() const { return m_shouldQuit; }

    MainWindow& mainWindow() { return m_mainWindow; }

private:
    Application() = default;
    MainWindow m_mainWindow;
    bool m_initialized = false;
    bool m_shouldQuit = false;
    std::deque<Input::Event> m_inputQueue;
    std::mutex m_inputMutex;
};
