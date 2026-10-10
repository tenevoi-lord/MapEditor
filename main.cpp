#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include "imgui_internal.h"

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <gl/GL.h>
#endif

#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <ctime>
#include "stb_image.h"
#include "EditorWorkspace.h"
#include "TextureManager.h"
#include "ObjectManager.h"
#include "ResourceManager.h"
#include "MapParser.h"

/*
#ifdef _WIN32
extern "C" {
    // Подсказка драйверу NVIDIA использовать дискретную GPU
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    // Подсказка драйверу AMD использовать дискретную GPU
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif
*/
#ifndef _WIN32
#include "imfilebrowser.h"
#else
static std::filesystem::path OpenFileDialogWindows(GLFWwindow* window) {
    OPENFILENAMEW ofn;
    wchar_t szFile[260] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = glfwGetWin32Window(window);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"Файлы сценариев (*.DTm)\0*.DTm\0Все файлы\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn) == TRUE) return std::filesystem::path(ofn.lpstrFile);
    return std::filesystem::path();
}
#endif

static void SetAppWindowIcon(GLFWwindow* window, const char* path) {
    GLFWimage icon{};
    int channels;
    // Пытаемся загрузить PNG
    icon.pixels = stbi_load(path, &icon.width, &icon.height, &channels, 4);
    if (icon.pixels) {
        glfwSetWindowIcon(window, 1, &icon);
        stbi_image_free(icon.pixels);
    }
    else {
#ifdef _WIN32
        // Если файла PNG нет, загружаем иконку прямо из скомпилированного .exe
        HWND hwnd = glfwGetWin32Window(window);
        HINSTANCE hInst = GetModuleHandle(NULL);

        // Компиляторы ресурсов обычно присваивают иконке ID = 101 или 1
        HICON hIcon = LoadIcon(hInst, MAKEINTRESOURCE(101));
        if (!hIcon) hIcon = LoadIcon(hInst, MAKEINTRESOURCE(1));

        if (hIcon) {
            SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        }
#endif
    }
}

int main() {
    srand((unsigned int)time(NULL));

#ifdef _WIN32
#if defined(_DEBUG)
    int dbgFlags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    dbgFlags |= _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF;
    _CrtSetDbgFlag(dbgFlags);
#endif
#endif

    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Кроссплатформенный Редактор Карт", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    SetAppWindowIcon(window, "Graphics/icon.png");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

#ifdef _WIN32
    if (std::filesystem::exists("C:\\Windows\\Fonts\\arial.ttf"))
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 16.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
#else
    if (std::filesystem::exists("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"))
        io.Fonts->AddFontFromFileTTF("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 16.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
#endif

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    TextureManager tex_manager;
    tex_manager.LoadAll();

    ObjectManager obj_manager;
    obj_manager.Load(".");

    ResourceManager res_manager;
    res_manager.LoadResources(".");

    EditorWorkspace workspace;
    workspace.tex_manager = &tex_manager;
    workspace.obj_manager = &obj_manager;
    workspace.res_manager = &res_manager;

#ifndef _WIN32
    ImGui::FileBrowser fileDialog;
    fileDialog.SetTitle("Выберите файл сценария");
    fileDialog.SetTypeFilters({ ".DTm", ".*" });
#endif

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents(); // Опрашиваем события без остановок для стабильных 60+ FPS
        /*if (ImGui::IsAnyItemActive()) {
            glfwPollEvents();
        }
        else if (ImGui::GetIO().WantTextInput) {
            glfwWaitEventsTimeout(0.1);
        }
        else {
            glfwWaitEventsTimeout(0.25);
        }*/
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        // Создаем невидимый корневой узел докинга поверх всего окна
        // Создаем док-пространство и получаем его ID
        ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // Настройка дефолтного расположения окон (выполнится только 1 раз)
       // Настройка дефолтного расположения окон (выполнится только 1 раз)
        static bool first_layout = true;
        if (first_layout) {
            first_layout = false;

            ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
            // Проверяем, есть ли уже сохраненный макет
            if (!node || !node->IsSplitNode()) {
                ImGui::DockBuilderRemoveNode(dockspace_id);
                ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
                ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

                ImGuiID dock_main_id = dockspace_id;

                // Разделяем экран: правая часть занимает ~13.6% экрана (261px / 1920px)
                ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.136f, nullptr, &dock_main_id);

                // --- Расстановка окон как в INI ---

                // В правый док (0x00000002) кидаем "Инструменты"
                ImGui::DockBuilderDockWindow("Инструменты", dock_id_right);

                // В центральный док (0x00000001) кидаем "Карту" и "Панель управления" (они станут вкладками)
                // 1. Сначала привязываем панель управления (уйдет на задний план)
                ImGui::DockBuilderDockWindow("Карта", dock_main_id);
                ImGui::DockBuilderDockWindow("Панель управления", dock_main_id);

                // 2. Вторым привязываем карту — она станет АКТИВНОЙ открытой вкладкой
                ImGui::DockBuilderDockWindow("Карта", dock_main_id);
                // <-- ДОБАВЬТЕ ЭТИ СТРОКИ:
                ImGuiDockNode* central_node = ImGui::DockBuilderGetNode(dock_main_id);
                if (central_node) {
                    central_node->SelectedTabId = ImGui::GetID("Карта"); // Принудительно выбираем "Карту"
                }
                // Применяем HiddenTabBar=1 для правого узла, как в вашем файле
                ImGuiDockNode* right_node = ImGui::DockBuilderGetNode(dock_id_right);
                if (right_node) {
                    right_node->LocalFlags |= ImGuiDockNodeFlags_HiddenTabBar;
                }

                ImGui::DockBuilderFinish(dockspace_id);
            }
        }

        bool trigger_open = (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O));
        bool trigger_save = (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S));

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Файл")) {
                if (ImGui::MenuItem("Открыть...", "Ctrl+O")) { trigger_open = true; }
                if (ImGui::MenuItem("Сохранить", "Ctrl+S")) { trigger_save = true; }
                if (ImGui::MenuItem("Сохранить как...")) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Параметры редактора")) { workspace.open_settings_modal = true; }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Правка")) { ImGui::EndMenu(); }
            if (ImGui::BeginMenu("Сценарий")) {
                workspace.RenderScenarioMenu();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Помощь")) { ImGui::EndMenu(); }
            ImGui::EndMainMenuBar();
        }

        if (trigger_open) {
#ifdef _WIN32
            std::filesystem::path selected_path = OpenFileDialogWindows(window);
            if (!selected_path.empty()) {
                workspace.LoadMap(ParseMapData(selected_path));
            }
#else
            fileDialog.Open();
#endif
        }
        if (trigger_save) { std::cout << "Saving map..." << std::endl; }

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        workspace.Render(viewport->WorkPos, viewport->WorkSize);
        // ============================================
        // СЧЕТЧИК FPS 
        // Чтобы скрыть счетчик, просто закомментируйте блок ниже /* ... */
        // ============================================
         /*
        {
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.35f); // Полупрозрачный фон
            ImGuiWindowFlags overlayFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
            ImGui::Begin("FPS_Overlay", nullptr, overlayFlags);
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::End();
        }
         */
         // ============================================

#ifndef _WIN32
        fileDialog.Display();
        if (fileDialog.HasSelected()) {
            workspace.LoadMap(ParseMapData(fileDialog.GetSelected()));
            fileDialog.ClearSelected();
        }
#endif

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
        glfwSwapBuffers(window);
    }

    tex_manager.Cleanup();
    obj_manager.Cleanup();
    res_manager.Cleanup();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}