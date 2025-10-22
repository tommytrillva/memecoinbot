cmake_minimum_required(VERSION 3.20)

project(memecoinbot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(memecoinbot-ui
    src/ui/data_subscription.cpp
    src/ui/layout.cpp
    src/ui/main_window.cpp
    src/ui/theming.cpp
    src/ui/views/positions_view.cpp
    src/ui/views/price_chart_view.cpp
    src/ui/views/trade_history_view.cpp
)

target_include_directories(memecoinbot-ui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(memecoinbot-ui PUBLIC cxx_std_20)

add_executable(memecoinbot-demo src/app/main.cpp)
target_link_libraries(memecoinbot-demo PRIVATE memecoinbot-ui)
