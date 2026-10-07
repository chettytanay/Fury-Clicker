cmake_minimum_required(VERSION 3.20)
project(mc_clicker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(mc_clicker
    src/main.cpp
    src/clicker.cpp
    src/memory.cpp
    src/input.cpp
    src/config.cpp
    src/ui.cpp
)

target_include_directories(mc_clicker PRIVATE
    src
    third_party
    third_party/imgui
    third_party/imgui/backends
)

# Dear ImGui sources (user must place imgui sources under third_party/imgui)
file(GLOB IMGUI_SRC
    third_party/imgui/*.cpp
    third_party/imgui/backends/imgui_impl_win32.cpp
    third_party/imgui/backends/imgui_impl_dx11.cpp
)
target_sources(mc_clicker PRIVATE ${IMGUI_SRC})

target_link_libraries(mc_clicker PRIVATE d3d11 dxgi user32 gdi32)

if(MSVC)
    set_target_properties(mc_clicker PROPERTIES WIN32_EXECUTABLE TRUE)
    target_compile_options(mc_clicker PRIVATE /W4 /permissive- /EHsc)
    target_compile_definitions(mc_clicker PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    set_property(TARGET mc_clicker APPEND_STRING PROPERTY LINK_FLAGS " /MANIFESTUAC:\"level='asInvoker' uiAccess='false'\"")

endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(click_scheduler_tests tests/click_scheduler_tests.cpp)
    target_include_directories(click_scheduler_tests PRIVATE src)
    if(MSVC)
        target_compile_options(click_scheduler_tests PRIVATE /W4 /permissive- /EHsc)
        target_compile_definitions(click_scheduler_tests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    endif()
    add_test(NAME click_scheduler_tests COMMAND click_scheduler_tests)
    add_executable(input_tracking_tests tests/input_tracking_tests.cpp src/input.cpp)
    target_include_directories(input_tracking_tests PRIVATE src)
    target_link_libraries(input_tracking_tests PRIVATE user32)
    target_compile_definitions(input_tracking_tests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    add_test(NAME input_tracking_tests COMMAND input_tracking_tests)
    set_tests_properties(input_tracking_tests PROPERTIES TIMEOUT 15)
    add_executable(config_randomization_tests tests/config_randomization_tests.cpp src/config.cpp)
    target_include_directories(config_randomization_tests PRIVATE src)
    target_link_libraries(config_randomization_tests PRIVATE user32)
    target_compile_definitions(config_randomization_tests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    add_test(NAME config_randomization_tests COMMAND config_randomization_tests)
    add_executable(slot_cps_tests tests/slot_cps_tests.cpp)
    target_include_directories(slot_cps_tests PRIVATE src)
    target_link_libraries(slot_cps_tests PRIVATE user32)
    target_compile_definitions(slot_cps_tests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    add_test(NAME slot_cps_tests COMMAND slot_cps_tests)
    add_executable(click_rate_window_tests tests/click_rate_window_tests.cpp)
    target_include_directories(click_rate_window_tests PRIVATE src)
    target_link_libraries(click_rate_window_tests PRIVATE user32)
    target_compile_definitions(click_rate_window_tests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    add_test(NAME click_rate_window_tests COMMAND click_rate_window_tests)
    add_executable(left_activation_tests tests/left_activation_tests.cpp)
    target_include_directories(left_activation_tests PRIVATE src)
    add_test(NAME left_activation_tests COMMAND left_activation_tests)
endif()


# Optional: nlohmann/json single header expected at third_party/nlohmann/json.hpp
# If missing, config falls back to a minimal internal serialiser.
