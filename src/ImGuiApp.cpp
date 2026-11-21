#include "ImGuiApp.h"

#include <filesystem>
#include <iostream>
#include <utility>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#if defined(_WIN32)
#include <GL/gl.h>
#else
#include <GLFW/glfw3.h>
#endif

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

namespace {

void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW ошибка (" << error << "): "
              << (description ? description : "неизвестно") << '\n';
}

} 

ImGuiApp::ImGuiApp(int width, int height, std::string title)
    : width_(width), height_(height), title_(std::move(title)) {}

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

void ImGuiApp::renderFrame() {
    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(760.0f, 520.0f), ImGuiCond_FirstUseEver);

    ImGui::Begin("Anchor Logistic — UI");
    ImGui::Text("Окно Dear ImGui запущено и готово к подключению логики.");
    ImGui::Separator();
    ImGui::Text("Следующий шаг: клиент, создание заказа и таблица статусов.");
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