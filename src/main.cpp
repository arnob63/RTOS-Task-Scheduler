//main.cpp


// -----------------------------------------------------------------------
// RTOS Task Scheduler Simulator
// Entry point: sets up SDL2 + OpenGL + Dear ImGui, then runs the main
// loop. All scheduling logic lives in Scheduler; all drawing lives in
// GuiLayout. This file only wires the two together each frame.
// -----------------------------------------------------------------------
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include "Scheduler.h"
#include "Logger.h"
#include "GuiLayout.h"

int main(int, char**) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return -1;
    }

    // OpenGL 3.3 core-ish context (kept simple; ImGui backend handles the rest).
    const char* glslVersion = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_WindowFlags windowFlags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                                     SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow(
        "RTOS Task Scheduler Simulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 800, windowFlags);

    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return -1;
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(1); // vsync

    // --- ImGui setup ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // allow panel docking, still simple to use

    // --- Font: load a clean 18px font (Roboto). Falls back to ImGui's
    // built-in default font if the file isn't found, so the app never
    // crashes just because the font asset is missing. ---
    const char* fontPath = "assets/fonts/Roboto-Regular.ttf";
    ImFont* loadedFont = io.Fonts->AddFontFromFileTTF(fontPath, 18.0f);
    if (loadedFont == nullptr) {
        SDL_Log("Could not load '%s', falling back to default ImGui font.", fontPath);
        io.Fonts->AddFontDefault();
    }

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    // --- Spacing / padding: roomier, easier to read on a 1080p display ---
    style.WindowPadding     = ImVec2(14.0f, 14.0f);
    style.FramePadding      = ImVec2(10.0f, 6.0f);
    style.CellPadding       = ImVec2(8.0f, 6.0f);
    style.ItemSpacing       = ImVec2(10.0f, 10.0f);
    style.ItemInnerSpacing  = ImVec2(8.0f, 6.0f);
    style.IndentSpacing     = 20.0f;
    style.ScrollbarSize     = 16.0f;
    style.GrabMinSize       = 10.0f;

    // --- Rounding: soft, modern look without being flashy ---
    style.WindowRounding    = 6.0f;
    style.ChildRounding     = 6.0f;
    style.FrameRounding     = 5.0f;
    style.PopupRounding     = 5.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding      = 5.0f;
    style.TabRounding       = 5.0f;

    // --- Slightly refined dark palette (kept close to ImGui's default
    // dark theme per the request, just a bit higher contrast) ---
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]        = ImVec4(0.11f, 0.11f, 0.13f, 1.00f);
    colors[ImGuiCol_ChildBg]         = ImVec4(0.13f, 0.13f, 0.15f, 1.00f);
    colors[ImGuiCol_FrameBg]         = ImVec4(0.18f, 0.18f, 0.21f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]  = ImVec4(0.24f, 0.24f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive]   = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);
    colors[ImGuiCol_Header]          = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
    colors[ImGuiCol_HeaderHovered]   = ImVec4(0.26f, 0.28f, 0.34f, 1.00f);
    colors[ImGuiCol_HeaderActive]    = ImVec4(0.30f, 0.32f, 0.38f, 1.00f);
    colors[ImGuiCol_Button]          = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
    colors[ImGuiCol_ButtonHovered]   = ImVec4(0.27f, 0.30f, 0.37f, 1.00f);
    colors[ImGuiCol_ButtonActive]    = ImVec4(0.22f, 0.45f, 0.75f, 1.00f);
    colors[ImGuiCol_TableRowBg]      = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    colors[ImGuiCol_TableRowBgAlt]   = ImVec4(0.17f, 0.17f, 0.20f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]   = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_Separator]       = ImVec4(0.30f, 0.30f, 0.34f, 1.00f);

    ImGui_ImplSDL2_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL3_Init(glslVersion);

    // --- Application state ---
    Logger logger;
    Scheduler scheduler(logger);
    GuiLayout::UiState uiState;

    Uint64 lastCounter = SDL_GetPerformanceCounter();
    const Uint64 freq = SDL_GetPerformanceFrequency();

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window)) {
                running = false;
            }
        }

        // Compute delta time for Auto Run pacing.
        Uint64 nowCounter = SDL_GetPerformanceCounter();
        float deltaTime = static_cast<float>(nowCounter - lastCounter) / static_cast<float>(freq);
        lastCounter = nowCounter;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Use a fullscreen "dockspace-like" layout by letting each panel
        // be a normal ImGui window; users can freely arrange them since
        // docking is enabled. Simpler than building a fixed dockspace,
        // and still looks clean/modern.
        GuiLayout::draw(scheduler, uiState, deltaTime);

        ImGui::Render();
        glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
        glClearColor(0.11f, 0.11f, 0.13f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
    }

    // --- Cleanup ---
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
