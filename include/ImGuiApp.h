#pragma once

#include <mutex>
#include <string>
#include <vector>

class LogisticService;
class SimulationEngine;
struct GLFWwindow;

class ImGuiApp {
public:
    ImGuiApp(int width, int height, std::string title,
             LogisticService& logistics, SimulationEngine* simulation = nullptr);
    ~ImGuiApp();

    ImGuiApp(const ImGuiApp&) = delete;
    ImGuiApp& operator=(const ImGuiApp&) = delete;

    bool init();
    void run();
    void shutdown();
    void pushStatusLog(const std::string& message);
    
private:
    bool loadCyrillicFont();
    void renderFrame();
    void renderClientSection();
    void renderOrderSection();
    void renderOrdersTableSection();
    void renderSimulationSection();


    int width_;
    int height_;
    std::string title_;
    LogisticService& logistics_;
    SimulationEngine* simulation_ = nullptr;

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

    double order_weight_kg_ = 1.0;
    int cancel_order_id_input_ = 1;
    std::string order_status_message_;
    bool order_status_is_error_ = false;

    mutable std::mutex events_mutex_;
    std::vector<std::string> recent_events_;
};