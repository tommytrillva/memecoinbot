cmake_minimum_required(VERSION 3.16)

# When building on Windows the dependencies (libcurl/OpenSSL) are typically
# sourced through vcpkg. If the environment variable is present we
# automatically wire up the toolchain to save developers from editing their
# cache manually inside Visual Studio.
if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if (DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE
        "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file" FORCE)
  endif()
endif()

project(memecoinbot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_library(common_logging STATIC
    src/common/logging.cpp
)

target_include_directories(common_logging
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(common_logging PUBLIC cxx_std_17)

add_library(trading_engine STATIC
    src/trading/engine.cpp
    src/trading/pumpfun_bridge.cpp
)

target_include_directories(trading_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_compile_features(trading_engine PUBLIC cxx_std_17)
target_link_libraries(trading_engine
    PUBLIC
        Threads::Threads
        common_logging
)

add_library(pumpfun_client_lib STATIC
    src/market_data/pumpfun_client.cpp
)

target_include_directories(pumpfun_client_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_compile_features(pumpfun_client_lib PUBLIC cxx_std_17)

target_link_libraries(pumpfun_client_lib
    PUBLIC
        CURL::libcurl
        Threads::Threads
        common_logging
)

add_library(trading_ui STATIC
    src/ui/imgui_trading_app.cpp
)

target_include_directories(trading_ui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(trading_ui
    PUBLIC
        trading_engine
)

target_compile_features(trading_ui PUBLIC cxx_std_17)

add_executable(trading_engine_app
    src/trading/engine_main.cpp
)

target_link_libraries(trading_engine_app PRIVATE trading_engine)

add_executable(trading_ui_demo
    src/ui/imgui_main.cpp
)

target_link_libraries(trading_ui_demo PRIVATE trading_ui)

enable_testing()

add_executable(pumpfun_client_tests
    tests/market_data/test_pumpfun_client.cpp
)

target_link_libraries(pumpfun_client_tests
    PRIVATE
        pumpfun_client_lib
)

target_compile_features(pumpfun_client_tests PRIVATE cxx_std_17)

add_test(NAME pumpfun_client_tests COMMAND pumpfun_client_tests)

add_executable(security_tests
    tests/security/test_security.cpp
    src/security/secret_store.cpp
    src/security/totp.cpp
    src/security/solana_signer.cpp
)

target_include_directories(security_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(security_tests
    PRIVATE
        OpenSSL::Crypto
        common_logging
)

target_compile_features(security_tests PRIVATE cxx_std_17)

add_test(NAME security_tests COMMAND security_tests)

add_executable(trading_engine_tests
    tests/trading/test_engine.cpp
)

target_include_directories(trading_engine_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(trading_engine_tests
    PRIVATE
        trading_engine
        pumpfun_client_lib
)

target_compile_features(trading_engine_tests PRIVATE cxx_std_17)

add_test(NAME trading_engine_tests COMMAND trading_engine_tests)

