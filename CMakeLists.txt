cmake_minimum_required(VERSION 3.16)
project(memecoinbot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(trading_engine STATIC
    src/trading/engine.cpp
)

target_include_directories(trading_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(trading_engine PUBLIC cxx_std_17)

add_executable(trading_engine_app
    src/trading/engine_main.cpp
)

target_link_libraries(trading_engine_app PRIVATE trading_engine)
