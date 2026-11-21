#include "ImGuiApp.h"

#include <iostream>

int main() {
    ImGuiApp app(1024, 700, "Anchor Logistic");

    if (!app.init()) {
        std::cerr << "Не удалось инициализировать интерфейс ImGui\n";
        return 1;
    }

    app.run();
    return 0;
}