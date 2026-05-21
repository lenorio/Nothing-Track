cmake_minimum_required(VERSION 3.15)
project(NothingTray LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Включаем современный C++/WinRT
add_compile_definitions(WINRT_LEAN_AND_MEAN)

include_directories(include)

# Собираем только необходимые файлы. Файл tray_app_custom.cpp больше не нужен!
file(GLOB SOURCES 
    src/main.cpp
    src/ble_monitor.cpp
    src/spp_client.cpp
    src/tray_app.cpp
    src/resources.rc
)

add_executable(NothingTray WIN32 ${SOURCES})
if(MSVC)
    target_compile_options(NothingTray PRIVATE /utf-8)
endif()
target_link_libraries(NothingTray PRIVATE dwmapi.lib gdiplus.lib pathcch.lib runtimeobject.lib)