#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

class IRepository;
class LogisticService;
class SimulationEngine;
struct GLFWwindow;

class ImGuiApp {
public:
    using ErrorDiagFn = std::function<std::string()>;

    ImGuiApp(int width, int height, std::string title,
             LogisticService& logistics, IRepository& repo,
             SimulationEngine* simulation = nullptr,
             ErrorDiagFn route_error_diag = nullptr);
    ~ImGuiApp();

    ImGuiApp(const ImGuiApp&) = delete;
    ImGuiApp& operator=(const ImGuiApp&) = delete;

    bool init();
    void run();
    void shutdown();
    void pushStatusLog(const std::string& message);

private:
    bool loadCyrillicFont();
    void refreshOrdersCache(bool force = false);
    void renderFrame();
    void renderClientSection();
    void renderOrderSection();
    void renderOrdersTableSection();
    void renderSimulationSection();

    int width_;
    int height_;
    std::string title_;
    LogisticService& logistics_;
    IRepository& repo_;
    SimulationEngine* simulation_ = nullptr;
    ErrorDiagFn route_error_diag_;

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

    std::vector<std::string> cached_order_rows_;
    std::vector<int> cached_order_ids_;
    int cached_orders_client_id_ = 0;
    std::chrono::steady_clock::time_point last_orders_refresh_{};

    mutable std::mutex events_mutex_;
    std::vector<std::string> recent_events_;
};