cmake_minimum_required(VERSION 3.16)

project(AnchorLogistic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# На macOS Homebrew ставит зависимости в /opt/homebrew (Apple Silicon)
# или /usr/local (Intel); CMake не всегда ищет их автоматически.
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew /usr/local)
endif()

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

# HTTPS-запросы к Nominatim и OSRM (cpp-httplib работает через OpenSSL).
find_package(OpenSSL REQUIRED)

# Зависимости для оконного интерфейса Dear ImGui (GLFW + OpenGL).
include(FetchContent)
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

set(IMGUI_BUILD_DEMO_WINDOWS OFF CACHE BOOL "" FORCE)
set(IMGUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.5
)
FetchContent_MakeAvailable(imgui)

# Разные версии CMake экспортируют разные имена цели SQLite3.
if(TARGET SQLite3::SQLite3)
    set(SQLITE3_TARGET SQLite3::SQLite3)
else()
    set(SQLITE3_TARGET SQLite::SQLite3)
endif()

add_library(anchor_core STATIC
    src/database.cpp
    src/database_clients.cpp
    src/database_couriers.cpp
    src/database_orders.cpp
    src/database_vehicles.cpp
    src/events.cpp
    src/models.cpp
    src/LogisticService.cpp
    src/GeocodingService.cpp
    src/RoutingService.cpp
)

target_include_directories(anchor_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_compile_definitions(anchor_core PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
# cpp-httplib использует функции OpenSSL, помеченные устаревшими в OpenSSL 4.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(anchor_core PRIVATE -Wno-deprecated-declarations)
endif()
target_link_libraries(anchor_core PUBLIC
    ${SQLITE3_TARGET}
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

# cpp-httplib использует Winsock и хранилище сертификатов Windows.
if(WIN32)
    target_link_libraries(anchor_core PUBLIC ws2_32 crypt32)
endif()

add_library(imgui_ui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui_ui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui_ui PUBLIC
    glfw
    OpenGL::GL
)

add_executable(anchor_logistic src/main.cpp)
target_link_libraries(anchor_logistic PRIVATE
    anchor_core
    imgui_ui
)

include(CTest)

if(BUILD_TESTING)
    find_package(Catch2 3 QUIET)

    if(Catch2_FOUND)
        include(Catch)

        add_executable(anchor_tests
            test/test_database.cpp
            test/test_service.cpp
            test/test_simulation.cpp
            test/test_events.cpp
            test/test_geocoding.cpp
        )
        target_include_directories(anchor_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test)
        target_link_libraries(anchor_tests PRIVATE anchor_core Catch2::Catch2WithMain)
        catch_discover_tests(anchor_tests)
    else()
        message(WARNING "Catch2 3 не найден: тесты отключены "
                        "(brew install catch2)")
    endif()
endif()