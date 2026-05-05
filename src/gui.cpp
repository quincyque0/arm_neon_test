#include "gui.hpp"
#include "algo.hpp"

#ifdef HAVE_GUI

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <string>
#include <thread>
#include <atomic>
#include <cmath>

namespace AppGui {

static int demo_N = 1024;
static float step_size_A = 0.5f;
static float step_size_B = 0.5f;
static std::vector<float> demo_vecA;
static std::vector<float> demo_vecB;
static float demo_res_scalar = 0, demo_res_neon = 0;
static double demo_time_scalar = 0, demo_time_neon = 0;
static bool demo_has_run = false;

static void RegenerateDemo() {
    demo_vecA.assign(demo_N, 0.0f);
    demo_vecB.assign(demo_N, 0.0f);
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> noiseA(-step_size_A, step_size_A);
    std::uniform_real_distribution<float> noiseB(-step_size_B, step_size_B);
    
    float valA = 0.0f;
    float valB = 0.0f;
    for (int i = 0; i < demo_N; ++i) {
        valA += noiseA(gen);
        valB += noiseB(gen);
        if (valA > 10.0f) valA -= step_size_A * 2.0f;
        if (valA < -10.0f) valA += step_size_A * 2.0f;
        if (valB > 10.0f) valB -= step_size_B * 2.0f;
        if (valB < -10.0f) valB += step_size_B * 2.0f;
        
        demo_vecA[i] = valA;
        demo_vecB[i] = valB;
    }
}

static void ComputeDemo() {
    auto t1 = std::chrono::high_resolution_clock::now();
    for (int k = 0; k < 64; ++k) demo_res_scalar = DotProductScalar(demo_vecA, demo_vecB);
    auto t2 = std::chrono::high_resolution_clock::now();
    demo_time_scalar = std::chrono::duration<double, std::milli>(t2 - t1).count() / 64.0;

    auto t3 = std::chrono::high_resolution_clock::now();
    for (int k = 0; k < 64; ++k) demo_res_neon = DotProductNeon(demo_vecA, demo_vecB);
    auto t4 = std::chrono::high_resolution_clock::now();
    demo_time_neon = std::chrono::duration<double, std::milli>(t4 - t3).count() / 64.0;

    demo_has_run = true;
}

struct BenchResult {
    size_t n;
    double time_scalar;
    double time_neon;
};

static std::atomic<bool> bench_running{false};
static std::atomic<float> bench_progress{0.0f};
static std::vector<BenchResult> bench_results;
static std::thread bench_thread;

static void RunBenchmarkAsync() {
    if (bench_running.load()) return;
    if (bench_thread.joinable()) bench_thread.join();
    
    bench_running.store(true);
    bench_progress.store(0.0f);
    bench_results.clear();
    
    bench_thread = std::thread([]() {
        std::vector<size_t> sizes = {1000000, 5000000, 10000000, 50000000};
        int total = sizes.size();
        int done = 0;
        
        std::vector<BenchResult> res;
        for (size_t N : sizes) {
            std::vector<float> a(N), b(N);
            std::mt19937 gen(123);
            std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
            for (size_t i = 0; i < N; ++i) {
                a[i] = dist(gen);
                b[i] = dist(gen);
            }
            
            auto t1 = std::chrono::high_resolution_clock::now();
            DotProductScalar(a, b);
            auto t2 = std::chrono::high_resolution_clock::now();
            double ms_scalar = std::chrono::duration<double, std::milli>(t2 - t1).count();

            auto t3 = std::chrono::high_resolution_clock::now();
            DotProductNeon(a, b);
            auto t4 = std::chrono::high_resolution_clock::now();
            double ms_neon = std::chrono::duration<double, std::milli>(t4 - t3).count();
            
            res.push_back({N, ms_scalar, ms_neon});
            done++;
            bench_progress.store(static_cast<float>(done) / total);
        }
        bench_results = res;
        bench_running.store(false);
    });
}

int Run(const Options& opts) {
    if (opts.forceSoftware) {
#if defined(__linux__)
        setenv("LIBGL_ALWAYS_SOFTWARE", "1", 1);
        setenv("GALLIUM_DRIVER", "llvmpipe", 0);
#endif
    }

    if (!glfwInit()) {
        std::cerr << "[GUI] Failed to initialize GLFW.\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1024, 600, "NEON Computation Space", nullptr, nullptr);
    if (!window) {
        std::cerr << "[GUI] Failed to create window.\n";
        glfwTerminate();
        return 2;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    io.Fonts->AddFontFromFileTTF("assets/fonts/NotoSans-Bold.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesCyrillic());
    
    ImGui::StyleColorsClassic();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.FrameRounding = 4.0f;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    RegenerateDemo();
    ComputeDemo();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImVec2 display_size = ImGui::GetIO().DisplaySize;
        float footer_height = 50.0f;

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(display_size.x, display_size.y - footer_height));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 20.0f));
        ImGui::Begin("Content", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        ImGui::Columns(2, "MainColumns", false);
        ImGui::SetColumnWidth(0, display_size.x * 0.55f);

        ImGui::Text("Векторное Пространство (Абстракция)");
        ImGui::TextDisabled("Моделирование случайных блужданий.");
        ImGui::Spacing();
        ImGui::Spacing();
        
        bool dirty = false;
        dirty |= ImGui::SliderInt("Элементы N", &demo_N, 256, 4096);
        dirty |= ImGui::SliderFloat("Шаг блуждания A", &step_size_A, 0.1f, 2.0f);
        dirty |= ImGui::SliderFloat("Шаг блуждания B", &step_size_B, 0.1f, 2.0f);
        
        if (dirty) RegenerateDemo();
        
        ImGui::Spacing();
        if (ImGui::Button("Обновить данные", ImVec2(180, 35))) RegenerateDemo();
        ImGui::SameLine();
        if (ImGui::Button("Рассчитать Dot Product", ImVec2(220, 35))) ComputeDemo();
        
        ImGui::Spacing();
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = avail.x - 10.0f;
        float h = 180;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(10, 15, 20, 255), 5.0f);
        dl->AddRect(p, ImVec2(p.x + w, p.y + h), IM_COL32(80, 90, 100, 255), 5.0f);
        
        if (demo_vecA.size() == static_cast<size_t>(demo_N)) {
            float max_val = 12.0f;
            for (int i = 0; i < demo_N - 1; ++i) {
                float x1 = p.x + (static_cast<float>(i) / demo_N) * w;
                float y1 = p.y + h/2 - (demo_vecA[i] / max_val) * (h/2);
                float x2 = p.x + (static_cast<float>(i+1) / demo_N) * w;
                float y2 = p.y + h/2 - (demo_vecA[i+1] / max_val) * (h/2);
                dl->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(200, 180, 50, 255), 1.5f);
                
                y1 = p.y + h/2 - (demo_vecB[i] / max_val) * (h/2);
                y2 = p.y + h/2 - (demo_vecB[i+1] / max_val) * (h/2);
                dl->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(50, 180, 200, 255), 1.5f);
            }
        }
        ImGui::Dummy(ImVec2(w, h + 15));
        
        if (demo_has_run) {
            ImGui::Columns(2, nullptr, false);
            ImGui::TextColored(ImVec4(0.8f, 0.7f, 0.2f, 1.0f), "Scalar Engine");
            ImGui::Text("Значение: %.2f", demo_res_scalar);
            ImGui::Text("Задержка: %.4f ms", demo_time_scalar);
            ImGui::NextColumn();
            ImGui::TextColored(ImVec4(0.2f, 0.7f, 0.8f, 1.0f), "NEON Engine");
            ImGui::Text("Значение: %.2f", demo_res_neon);
            ImGui::Text("Задержка: %.4f ms", demo_time_neon);
            ImGui::Columns(1);
        }

        ImGui::NextColumn();
        ImGui::Text("Глобальный Стресс-Тест");
        ImGui::TextDisabled("Анализ производительности скалярных и\nвекторизованных (NEON) вычислений.");
        ImGui::Spacing();
        ImGui::Spacing();
        
        if (bench_running.load()) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Обработка...");
            ImGui::ProgressBar(bench_progress.load(), ImVec2(-1, 0), "");
        } else {
            if (ImGui::Button("СТАРТ АНАЛИЗА", ImVec2(-1, 50))) {
                RunBenchmarkAsync();
            }
        }
        
        ImGui::Spacing();
        
        if (!bench_results.empty() && !bench_running.load()) {
            if (ImGui::BeginTable("perf_table", 4, ImGuiTableFlags_BordersInner | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Массив");
                ImGui::TableSetupColumn("Scalar");
                ImGui::TableSetupColumn("NEON");
                ImGui::TableSetupColumn("Boost");
                ImGui::TableHeadersRow();
                
                for (const auto& res : bench_results) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("%zu", res.n);
                    ImGui::TableNextColumn(); ImGui::Text("%.2f", res.time_scalar);
                    ImGui::TableNextColumn(); ImGui::Text("%.2f", res.time_neon);
                    ImGui::TableNextColumn();
                    if (res.time_neon > 0) {
                        float boost = static_cast<float>(res.time_scalar / res.time_neon);
                        ImGui::TextColored(boost > 1.2f ? ImVec4(0,1,0,1) : ImVec4(1,1,1,1), "x%.2f", boost);
                    } else {
                        ImGui::Text("-");
                    }
                }
                ImGui::EndTable();
            }
        }
        
        ImGui::End();
        ImGui::PopStyleVar();

        ImGui::SetNextWindowPos(ImVec2(0, display_size.y - footer_height));
        ImGui::SetNextWindowSize(ImVec2(display_size.x, footer_height));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 15.0f));
        ImGui::Begin("Footer", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
        
        bool neon = HasNeonSupport();
        ImVec2 footer_p = ImGui::GetCursorScreenPos();
        ImDrawList* footer_dl = ImGui::GetWindowDrawList();
        float radius = 8.0f;
        ImVec2 center = ImVec2(footer_p.x + radius, footer_p.y + radius + 2.0f);
        
        if (neon) {
            footer_dl->AddCircleFilled(center, radius, IM_COL32(50, 255, 100, 255));
            footer_dl->AddCircleFilled(center, radius * 1.5f, IM_COL32(50, 255, 100, 80));
        } else {
            footer_dl->AddCircleFilled(center, radius, IM_COL32(255, 50, 50, 255));
        }
        
        ImGui::SetCursorScreenPos(ImVec2(footer_p.x + radius * 3.5f, footer_p.y));
        if (neon) {
            ImGui::Text("СИСТЕМА: АКТИВНЫ ВЕКТОРНЫЕ ВЫЧИСЛЕНИЯ (NEON)");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "СИСТЕМА: СТАНДАРТНЫЙ РЕЖИМ (СКАЛЯРНЫЙ ФОЛБЭК)");
        }

        float decor_x = display_size.x - 300.0f;
        ImGui::SetCursorScreenPos(ImVec2(decor_x, footer_p.y));
        ImGui::TextDisabled("BACKEND: %s", opts.forceSoftware ? "Software Rasterizer" : "Hardware OpenGL");

        ImGui::End();
        ImGui::PopStyleVar();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        static auto glViewportPtr = reinterpret_cast<void(*)(int,int,int,int)>(glfwGetProcAddress("glViewport"));
        static auto glClearColorPtr = reinterpret_cast<void(*)(float,float,float,float)>(glfwGetProcAddress("glClearColor"));
        static auto glClearPtr = reinterpret_cast<void(*)(unsigned)>(glfwGetProcAddress("glClear"));
        
        if (glViewportPtr && glClearColorPtr && glClearPtr) {
            glViewportPtr(0, 0, display_w, display_h);
            glClearColorPtr(0.05f, 0.05f, 0.07f, 1.0f);
            glClearPtr(0x4000);
        }
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    if (bench_thread.joinable()) bench_thread.join();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

}

#else

namespace AppGui {
    int Run(const Options&) {
        return 2;
    }
}

#endif
