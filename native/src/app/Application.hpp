#pragma once
#include "sokol_app.h"
#include "../ui/MainWindow.hpp"

class Application {
public:
    static Application& instance();

    void init();
    void frame();
    void cleanup();
    void event(const sapp_event* ev);

    MainWindow& mainWindow() { return m_mainWindow; }

private:
    Application() = default;
    MainWindow m_mainWindow;
    bool m_initialized = false;
};
