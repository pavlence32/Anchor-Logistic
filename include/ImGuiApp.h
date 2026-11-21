#pragma once

#include <string>

class LogisticService;
struct GLFWwindow;

class ImGuiApp {
public:
    ImGuiApp(int width, int height, std::string title, LogisticService& logistics);
    ~ImGuiApp();

    ImGuiApp(const ImGuiApp&) = delete;
    ImGuiApp& operator=(const ImGuiApp&) = delete;

    bool init();
    void run();
    void shutdown();

private:
    bool loadCyrillicFont();
    void renderFrame();
    void renderClientSection();

    int width_;
    int height_;
    std::string title_;
    LogisticService& logistics_;

    GLFWwindow* window_ = nullptr;
    bool initialized_ = false;

    int client_id_input_ = 1;
    int active_client_id_ = 0;
    std::string active_client_info_;

    char reg_name_[128]{};
    char reg_phone_[64]{};
    char reg_address_[256]{};
    std::string client_status_message_;
    bool client_status_is_error_ = false;
};