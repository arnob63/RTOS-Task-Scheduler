cmake_minimum_required(VERSION 3.15)
project(RTOSSchedulerSim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)

add_executable(rtos_scheduler
    src/main.cpp
    src/Scheduler.cpp
    src/Logger.cpp
    src/GuiLayout.cpp

    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(rtos_scheduler PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(rtos_scheduler PRIVATE
    ${SDL2_LIBRARIES}
    OpenGL::GL
)
