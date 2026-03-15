cmake_minimum_required(VERSION 3.15)
project(keystroke-counter)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
    GIT_TAG main
)
FetchContent_MakeAvailable(ftxui)

add_executable(keystroke main.cpp)
target_link_libraries(keystroke sqlite3)

add_executable(keystroke-stats stats.cpp)
target_link_libraries(keystroke-stats
    sqlite3
    ftxui::screen
    ftxui::dom
    ftxui::component
)
