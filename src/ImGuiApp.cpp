#include "ImGuiApp.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <utility>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#if defined(_WIN32)
#include <GL/gl.h>
#endif

#include "LogisticService.h"
#include "events.h"
#include "interface.h"
#include "models.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

namespace {

void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW ошибка (" << error << "): "
              << (description ? description : "неизвестно") << '\n';
}

} 

ImGuiApp::ImGuiApp(int width, int height, std::string title,
                   LogisticService& logistics, IRepository& repo,
                   SimulationEngine* simulation)
    : width_(width),
      height_(height),
      title_(std::move(title)),
      logistics_(logistics),
      repo_(repo),
      simulation_(simulation) {}

ImGuiApp::~ImGuiApp() {
    shutdown();
}

bool ImGuiApp::init() {
    if (initialized_) {
        return true;
    }

    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        std::cerr << "Не удалось инициализировать GLFW\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    window_ = glfwCreateWindow(width_, height_, title_.c_str(), nullptr, nullptr);
    if (!window_) {
        std::cerr << "Не удалось создать окно GLFW\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    if (!loadCyrillicFont()) {
        std::cerr << "Предупреждение: шрифт с кириллицей не загружен, "
                     "используется шрифт по умолчанию\n";
    }

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    initialized_ = true;
    return true;
}

bool ImGuiApp::loadCyrillicFont() {
    ImGuiIO& io = ImGui::GetIO();

    const char* candidates[] = {
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf"
    };

    for (const char* path : candidates) {
        if (std::filesystem::exists(path)) {
            ImFont* font = io.Fonts->AddFontFromFileTTF(
                path, 18.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
            if (font != nullptr) {
                io.FontDefault = font;
                return true;
            }
        }
    }

    return false;
}

void ImGuiApp::pushStatusLog(const std::string& message) {
    std::lock_guard<std::mutex> lock(events_mutex_);
    recent_events_.push_back(message);
    if (recent_events_.size() > 50) {
        recent_events_.erase(recent_events_.begin());
    }
}

void ImGuiApp::renderClientSection() {
    ImGui::Text("1. Клиент (поиск по ID или регистрация)");
    ImGui::Separator();

    ImGui::InputInt("ID клиента", &client_id_input_);
    ImGui::SameLine();
    if (ImGui::Button("Найти клиента")) {
        Client client;
        if (logistics_.findClient(client_id_input_, client)) {
            active_client_id_ = client.Get_id();
            active_client_info_ =
                "ID: " + std::to_string(client.Get_id()) +
                " | " + client.Get_name() +
                " | " + client.Get_phone() +
                " | " + client.Get_address();
            client_status_message_ = "Клиент найден и выбран.";
            client_status_is_error_ = false;
        } else {
            active_client_id_ = 0;
            active_client_info_.clear();
            client_status_message_ =
                "Клиент с таким ID не найден. Можно зарегистрировать нового ниже.";
            client_status_is_error_ = true;
        }
    }

    if (!active_client_info_.empty()) {
        ImGui::TextWrapped("Активный клиент: %s", active_client_info_.c_str());
    }

    ImGui::Spacing();
    ImGui::Text("Регистрация нового клиента");
    ImGui::InputText("Имя", reg_name_, sizeof(reg_name_));
    ImGui::InputText("Телефон", reg_phone_, sizeof(reg_phone_));
    ImGui::InputText("Адрес", reg_address_, sizeof(reg_address_));

    if (ImGui::Button("Зарегистрировать")) {
        try {
            const int new_id = logistics_.registerClient(
                reg_name_, reg_phone_, reg_address_);
            client_id_input_ = new_id;
            active_client_id_ = new_id;

            Client client;
            if (logistics_.findClient(new_id, client)) {
                active_client_info_ =
                    "ID: " + std::to_string(client.Get_id()) +
                    " | " + client.Get_name() +
                    " | " + client.Get_phone() +
                    " | " + client.Get_address();
            } else {
                active_client_info_ = "ID: " + std::to_string(new_id);
            }

            reg_name_[0] = '\0';
            reg_phone_[0] = '\0';
            reg_address_[0] = '\0';
            client_status_message_ =
                "Клиент зарегистрирован, ID: " + std::to_string(new_id);
            client_status_is_error_ = false;
        } catch (const std::invalid_argument& e) {
            client_status_message_ = std::string("Ошибка ввода: ") + e.what();
            client_status_is_error_ = true;
        } catch (const std::runtime_error& e) {
            client_status_message_ = std::string("Ошибка регистрации: ") + e.what();
            client_status_is_error_ = true;
        }
    }

    if (!client_status_message_.empty()) {
        ImVec4 color = client_status_is_error_
                           ? ImVec4(0.95f, 0.35f, 0.35f, 1.0f)
                           : ImVec4(0.35f, 0.85f, 0.45f, 1.0f);
        ImGui::TextColored(color, "%s", client_status_message_.c_str());
    }
}

void ImGuiApp::renderOrderSection() {
    ImGui::Spacing();
    ImGui::Text("2. Заказы (создание и отмена до начала доставки)");
    ImGui::Separator();

    if (active_client_id_ <= 0) {
        ImGui::TextDisabled(
            "Сначала найдите или зарегистрируйте клиента в блоке выше.");
        return;
    }

    ImGui::Text("Клиент для заказа: ID %d", active_client_id_);
    ImGui::InputDouble("Вес груза (кг)", &order_weight_kg_, 0.5, 5.0, "%.2f");

    if (ImGui::Button("Создать заказ")) {
        try {
            const int order_id =
                logistics_.createOrder(active_client_id_, order_weight_kg_);

            if (simulation_ != nullptr) {
                simulation_->enqueue(order_id);
            }

            cancel_order_id_input_ = order_id;
            order_status_message_ =
                "Заказ создан, ID: " + std::to_string(order_id);
            order_status_is_error_ = false;
            pushStatusLog("Создан заказ #" + std::to_string(order_id));
        } catch (const std::invalid_argument& e) {
            order_status_message_ =
                std::string("Ошибка данных заказа: ") + e.what();
            order_status_is_error_ = true;
        } catch (const std::runtime_error& e) {
            order_status_message_ =
                std::string("Не удалось создать заказ: ") + e.what();
            order_status_is_error_ = true;
        }
    }

    ImGui::Spacing();
    ImGui::InputInt("ID заказа для отмены", &cancel_order_id_input_);
    ImGui::SameLine();
    if (ImGui::Button("Отменить заказ")) {
        try {
            logistics_.cancelOrder(cancel_order_id_input_);
            order_status_message_ =
                "Заказ #" + std::to_string(cancel_order_id_input_) + " отменён.";
            order_status_is_error_ = false;
            pushStatusLog("Отменён заказ #" + std::to_string(cancel_order_id_input_));
        } catch (const std::invalid_argument& e) {
            order_status_message_ = std::string("Ошибка отмены: ") + e.what();
            order_status_is_error_ = true;
        } catch (const std::logic_error& e) {
            order_status_message_ =
                std::string("Нельзя отменить заказ: ") + e.what();
            order_status_is_error_ = true;
        } catch (const std::runtime_error& e) {
            order_status_message_ =
                std::string("Ошибка при отмене заказа: ") + e.what();
            order_status_is_error_ = true;
        }
    }

    if (!order_status_message_.empty()) {
        ImVec4 color = order_status_is_error_
                           ? ImVec4(0.95f, 0.35f, 0.35f, 1.0f)
                           : ImVec4(0.35f, 0.85f, 0.45f, 1.0f);
        ImGui::TextColored(color, "%s", order_status_message_.c_str());
    }
}

void ImGuiApp::renderOrdersTableSection() {
    ImGui::Spacing();
    ImGui::Text("3. Заказы активного клиента и статусы");
    ImGui::Separator();

    if (active_client_id_ <= 0) {
        ImGui::TextDisabled("Выберите клиента, чтобы просмотреть его заказы.");
        return;
    }

    if (ImGui::Button("Обновить список заказов")) {
        order_status_message_ = "Список заказов обновлён.";
        order_status_is_error_ = false;
    }

    const std::vector<Order> orders = repo_.getOrdersByClient(active_client_id_);

    if (ImGui::BeginTable("ClientOrdersTable", 6,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_ScrollY,
                          ImVec2(0.0f, 170.0f))) {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Вес (кг)");
        ImGui::TableSetupColumn("Транспорт");
        ImGui::TableSetupColumn("Дистанция (км)");
        ImGui::TableSetupColumn("Статус");
        ImGui::TableSetupColumn("Действие");
        ImGui::TableHeadersRow();

        for (const Order& order : orders) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", order.Get_id());

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.2f", order.Get_weight_kg());

            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(toString(order.Get_transport()));

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2f", order.Get_route().distance_m / 1000.0);

            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(toString(order.Get_status()));

            ImGui::TableSetColumnIndex(5);
            const std::string label =
                "Выбрать##" + std::to_string(order.Get_id());
            if (ImGui::SmallButton(label.c_str())) {
                cancel_order_id_input_ = order.Get_id();
            }
        }

        ImGui::EndTable();
    }
}

void ImGuiApp::renderSimulationSection() {
    if (simulation_ == nullptr) {
        return;
    }

    ImGui::Spacing();
    ImGui::Text("4. Симуляция доставки и журнал событий");
    ImGui::Separator();

    const bool is_running = simulation_->running();
    ImGui::Text("Состояние симуляции: %s",
                is_running ? "Запущена" : "Остановлена");

    if (!is_running && ImGui::Button("Запустить симуляцию")) {
        simulation_->start();
        pushStatusLog("Симуляция запущена.");
    }
    ImGui::SameLine();
    if (is_running && ImGui::Button("Остановить симуляцию")) {
        simulation_->stop();
        pushStatusLog("Симуляция остановлена.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Возобновить активные заказы")) {
        simulation_->resumeActiveOrders();
        pushStatusLog("Запрошено возобновление активных заказов.");
    }

    ImGui::BeginChild("StatusEventsLog", ImVec2(0.0f, 110.0f), true);
    {
        std::lock_guard<std::mutex> lock(events_mutex_);
        for (const auto& line : recent_events_) {
            ImGui::TextUnformatted(line.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();
}

void ImGuiApp::renderFrame() {
    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(960.0f, 720.0f), ImGuiCond_FirstUseEver);

    ImGui::Begin("Anchor Logistic");
    renderClientSection();
    renderOrderSection();
    renderOrdersTableSection();
    renderSimulationSection();
    ImGui::End();
}

void ImGuiApp::run() {
    if (!initialized_ && !init()) {
        return;
    }

    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        renderFrame();

        ImGui::Render();
        int display_w = 0;
        int display_h = 0;
        glfwGetFramebufferSize(window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.10f, 0.12f, 0.16f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_);
    }
}

void ImGuiApp::shutdown() {
    if (!initialized_) {
        return;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    glfwTerminate();
    initialized_ = false;
}