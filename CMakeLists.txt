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
    src/events.cpp
    src/models.cpp
    src/LogisticService.cpp
)

target_include_directories(anchor_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(anchor_core PUBLIC ${SQLITE3_TARGET} Threads::Threads)

add_executable(anchor_logistic src/main.cpp)
target_link_libraries(anchor_logistic PRIVATE anchor_core)

include(CTest)

if(BUILD_TESTING)
    find_package(Catch2 3 QUIET)

    if(Catch2_FOUND)
        include(Catch)

        add_executable(anchor_tests
            test/test_service.cpp
            test/test_simulation.cpp
            test/test_events.cpp
        )
        target_include_directories(anchor_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test)
        target_link_libraries(anchor_tests PRIVATE anchor_core Catch2::Catch2WithMain)
        catch_discover_tests(anchor_tests)
    else()
        message(WARNING "Catch2 3 не найден: тесты отключены "
                        "(brew install catch2)")
    endif()
endif()
