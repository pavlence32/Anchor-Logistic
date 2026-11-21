#pragma once

#include <string>

struct GLFWwindow;

class ImGuiApp {
public:
    ImGuiApp(int width, int height, std::string title);
    ~ImGuiApp();

    ImGuiApp(const ImGuiApp&) = delete;
    ImGuiApp& operator=(const ImGuiApp&) = delete;

    bool init();
    void run();
    void shutdown();

private:
    bool loadCyrillicFont();
    void renderFrame();

    int width_;
    int height_;
    std::string title_;
    GLFWwindow* window_ = nullptr;
    bool initialized_ = false;
};