cmake_minimum_required(VERSION 3.20)
project(N4X-ProcessOptimizer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /EHsc /MP /utf-8 /permissive-)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options($<$<CONFIG:Release>:/O2>)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
set(IMGUI_BACKENDS_DIR ${IMGUI_DIR}/backends)
set(IMGUI_DX11_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui_dx11)
set(JSON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json)

# ImGui sources - prefer submodule backends, fallback to third_party/imgui_dx11
set(IMGUI_CORE_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
)

if(EXISTS ${IMGUI_BACKENDS_DIR}/imgui_impl_win32.cpp)
    list(APPEND IMGUI_CORE_SOURCES ${IMGUI_BACKENDS_DIR}/imgui_impl_win32.cpp)
    list(APPEND IMGUI_CORE_SOURCES ${IMGUI_BACKENDS_DIR}/imgui_impl_dx11.cpp)
    set(IMGUI_BACKEND_INCLUDE ${IMGUI_BACKENDS_DIR})
elseif(EXISTS ${IMGUI_DX11_DIR}/imgui_impl_win32.cpp)
    list(APPEND IMGUI_CORE_SOURCES ${IMGUI_DX11_DIR}/imgui_impl_win32.cpp)
    list(APPEND IMGUI_CORE_SOURCES ${IMGUI_DX11_DIR}/imgui_impl_dx11.cpp)
    set(IMGUI_BACKEND_INCLUDE ${IMGUI_DX11_DIR})
else()
    message(WARNING "ImGui backends not found. Ensure git submodules are initialized: git submodule update --init --recursive")
    set(IMGUI_BACKEND_INCLUDE ${IMGUI_BACKENDS_DIR})
endif()

set(APP_SOURCES
    src/main.cpp
    src/core/Logger.cpp
    src/core/Config.cpp
    src/core/SafetyGuard.cpp
    src/core/ProcessManager.cpp
    src/utils/WinUtil.cpp
    src/ui/MainWindow.cpp
    src/ui/ProcessTab.cpp
    src/ui/SettingsTab.cpp
    src/ui/LogTab.cpp
    resources/app.rc
)

add_executable(N4XProcessOptimizer WIN32 ${APP_SOURCES} ${IMGUI_CORE_SOURCES})

target_include_directories(N4XProcessOptimizer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${IMGUI_DIR}
    ${IMGUI_BACKEND_INCLUDE}
    ${JSON_DIR}
)

target_link_libraries(N4XProcessOptimizer PRIVATE
    d3d11
    dxgi
    d3dcompiler
    psapi
    version
    shell32
    advapi32
    user32
    gdi32
    ole32
    uuid
    comctl32
)

if(MSVC)
    target_link_options(N4XProcessOptimizer PRIVATE /SUBSYSTEM:WINDOWS /MANIFEST:NO)
    # Embed manifest explicitly to avoid conflicting auto-manifest level
    set_target_properties(N4XProcessOptimizer PROPERTIES
        WIN32_EXECUTABLE TRUE
        OUTPUT_NAME "N4XProcessOptimizer"
    )
    add_custom_command(TARGET N4XProcessOptimizer POST_BUILD
        COMMAND mt.exe -nologo -manifest "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.manifest" "-outputresource:$<TARGET_FILE:N4XProcessOptimizer>;#1"
        VERBATIM
    )
endif()

# Copy runtime files next to exe
add_custom_command(TARGET N4XProcessOptimizer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config.json
        $<TARGET_FILE_DIR:N4XProcessOptimizer>/config.json
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
        $<TARGET_FILE_DIR:N4XProcessOptimizer>/README.md
)

# Optional console build for debugging
option(N4X_BUILD_CONSOLE "Build with console subsystem" OFF)
if(N4X_BUILD_CONSOLE AND MSVC)
    target_link_options(N4XProcessOptimizer PRIVATE /SUBSYSTEM:CONSOLE)
    target_compile_definitions(N4XProcessOptimizer PRIVATE N4X_CONSOLE_BUILD)
endif()

install(TARGETS N4XProcessOptimizer RUNTIME DESTINATION .)
install(FILES config.json README.md DESTINATION .)

