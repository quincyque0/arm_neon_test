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
#include <future>
#include <algorithm>

namespace AppGui {

struct ChartPoint { size_t n; double ms_scalar; double ms_neon; };

static std::atomic<bool>  chart_running{false};
static std::atomic<float> chart_progress{0.0f};
static std::vector<ChartPoint> chart_data;
static std::future<std::vector<ChartPoint>> chart_future;
static const int chart_repeats = 3;

static std::vector<size_t> BuildChartSizes() {
    std::vector<size_t> sizes;
    const double lo = std::log2(1000.0), hi = std::log2(10000000.0);
    for (int i = 0; i < 80; ++i) {
        double t = (double)i / 79.0;
        size_t s = (size_t)std::round(std::pow(2.0, lo + t * (hi - lo)));
        s = (s + 15) & ~size_t(15);
        sizes.push_back(s);
    }
    return sizes;
}

static void RunChartAsync() {
    if (chart_running.load()) return;
    chart_running.store(true);
    chart_progress.store(0.0f);
    chart_data.clear();
    int repeats = chart_repeats;
    chart_future = std::async(std::launch::async, [repeats]() {
        auto sizes = BuildChartSizes();
        std::vector<ChartPoint> out;
        out.reserve(sizes.size());
        int total = (int)sizes.size(), idx = 0;
        for (size_t n : sizes) {
            std::vector<float> a(n), b(n);
            std::mt19937 gen(42);
            std::uniform_real_distribution<float> dist(-1.f, 1.f);
            for (size_t i = 0; i < n; ++i) { a[i] = dist(gen); b[i] = dist(gen); }

            double ms_s = 0, ms_n = 0;
            for (int r = 0; r < repeats; ++r) {
                auto t0 = std::chrono::high_resolution_clock::now();
                DotProductScalar(a, b);
                auto t1 = std::chrono::high_resolution_clock::now();
                DotProductNeon(a, b);
                auto t2 = std::chrono::high_resolution_clock::now();
                ms_s += std::chrono::duration<double, std::milli>(t1 - t0).count();
                ms_n += std::chrono::duration<double, std::milli>(t2 - t1).count();
            }
            out.push_back({n, ms_s / repeats, ms_n / repeats});
            chart_progress.store((float)(++idx) / total);
        }
        chart_running.store(false);
        return out;
    });
}

static void DrawChart() {
    if (chart_future.valid() && !chart_running.load() &&
        chart_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        chart_data = chart_future.get();

    if (chart_running.load()) {
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "  Замер...");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
        ImGui::ProgressBar(chart_progress.load(), ImVec2(-1, 0));
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f, 0.30f, 0.60f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.45f, 0.85f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.20f, 0.55f, 1.00f, 1.0f));
        if (ImGui::Button("  Построить  ")) RunChartAsync();
        ImGui::PopStyleColor(3);
    }
    ImGui::Spacing();

    if (chart_data.size() < 2) {
        ImGui::TextDisabled("Нажмите «Построить» — 80 точек от 1K до 10M.");
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin  = ImGui::GetCursorScreenPos();
    float avail    = ImGui::GetContentRegionAvail().x;
    float chartH   = std::max(300.0f, ImGui::GetContentRegionAvail().y - 8.f);
    float padL = 62, padR = 20, padT = 32, padB = 42;
    float W = avail - padL - padR, H = chartH - padT - padB;
    ImVec2 tl(origin.x + padL, origin.y + padT);
    ImVec2 br(origin.x + padL + W, origin.y + padT + H);

    // --- фон с тёмным градиентом через два прямоугольника ---
    dl->AddRectFilled(ImVec2(origin.x, origin.y),
                      ImVec2(origin.x + avail, origin.y + chartH),
                      IM_COL32(8, 10, 18, 255), 10.0f);
    dl->AddRectFilled(ImVec2(origin.x, origin.y),
                      ImVec2(origin.x + avail, origin.y + chartH * 0.5f),
                      IM_COL32(12, 15, 26, 120), 10.0f);
    // светящийся бордер
    dl->AddRect(ImVec2(origin.x, origin.y),
                ImVec2(origin.x + avail, origin.y + chartH),
                IM_COL32(30, 60, 120, 200), 10.0f, 0, 1.5f);
    dl->AddRect(ImVec2(origin.x + 1, origin.y + 1),
                ImVec2(origin.x + avail - 1, origin.y + chartH - 1),
                IM_COL32(60, 120, 220, 60), 9.0f, 0, 1.0f);

    double maxMs = 0;
    for (auto& p : chart_data) maxMs = std::max(maxMs, std::max(p.ms_scalar, p.ms_neon));
    if (maxMs <= 0) maxMs = 1.0;

    // --- горизонтальные сетки ---
    double yStep = 0.5;
    if (maxMs > 8) yStep = 2.0;
    else if (maxMs > 4) yStep = 1.0;
    for (double y = 0; y <= maxMs + 1e-9; y += yStep) {
        float fy = tl.y + H * (1.f - (float)(y / maxMs));
        if (fy < tl.y - 1 || fy > br.y + 1) continue;
        bool isZero = (y < 1e-9);
        ImU32 gridCol = isZero ? IM_COL32(50, 80, 140, 200) : IM_COL32(25, 35, 65, 180);
        dl->AddLine(ImVec2(tl.x, fy), ImVec2(br.x, fy), gridCol, isZero ? 1.5f : 1.0f);
        char lbl[32];
        if (isZero)           std::snprintf(lbl, sizeof(lbl), "0");
        else if (yStep < 1.0) std::snprintf(lbl, sizeof(lbl), "%.1f", y);
        else                  std::snprintf(lbl, sizeof(lbl), "%.0f", y);
        ImVec2 ts = ImGui::CalcTextSize(lbl);
        dl->AddText(ImVec2(tl.x - ts.x - 6, fy - 7), IM_COL32(100, 120, 170, 220), lbl);
    }

    const double logXMin   = std::log10(1000.0);
    const double logXMax   = std::log10((double)chart_data.back().n);
    const double logXRange = logXMax - logXMin;
    auto fx = [&](double sz) { return tl.x + (float)((std::log10(sz) - logXMin) / logXRange * W); };

    // --- вертикальные сетки ---
    const double ticks[] = {1e3,2e3,5e3,1e4,2e4,5e4,1e5,2e5,5e5,1e6,2e6,5e6,1e7};
    for (double sz : ticks) {
        float x = fx(sz);
        if (x < tl.x - 1 || x > br.x + 1) continue;
        dl->AddLine(ImVec2(x, tl.y), ImVec2(x, br.y), IM_COL32(25, 35, 65, 180), 1.0f);
        char lbl[16];
        if (sz >= 1e6) std::snprintf(lbl, sizeof(lbl), "%.0fM", sz/1e6);
        else           std::snprintf(lbl, sizeof(lbl), "%.0fK", sz/1e3);
        ImVec2 ts = ImGui::CalcTextSize(lbl);
        dl->AddText(ImVec2(x - ts.x*0.5f, br.y + 7), IM_COL32(90, 110, 160, 200), lbl);
    }

    // --- оси ---
    dl->AddLine(ImVec2(tl.x, br.y), ImVec2(br.x, br.y), IM_COL32(50, 80, 140, 255), 1.5f);
    dl->AddLine(ImVec2(tl.x, tl.y), ImVec2(tl.x, br.y), IM_COL32(50, 80, 140, 255), 1.5f);

    // --- подписи осей ---
    {
        const char* yLabel = "мс";
        ImVec2 ys = ImGui::CalcTextSize(yLabel);
        dl->AddText(ImVec2(tl.x - ys.x * 0.5f - 14, tl.y - 20), IM_COL32(120, 150, 210, 200), yLabel);
        const char* xLabel = "Размер массива";
        ImVec2 xs = ImGui::CalcTextSize(xLabel);
        dl->AddText(ImVec2(tl.x + W * 0.5f - xs.x * 0.5f, br.y + 24), IM_COL32(100, 130, 190, 180), xLabel);
    }

    // цвета линий: scalar = тёплый оранж, neon = холодный голубой
    struct LineStyle { ImU32 colCore; ImU32 colMid; ImU32 colGlow; ImU32 colFill; const char* name; };
    LineStyle styles[2] = {
        { IM_COL32(255, 130,  60, 255), IM_COL32(255, 100, 40, 100), IM_COL32(255, 80, 20, 30),
          IM_COL32(255, 100, 40, 18), "Scalar" },
        { IM_COL32( 60, 220, 255, 255), IM_COL32( 40, 180, 255, 100), IM_COL32(20, 140, 255, 30),
          IM_COL32( 40, 180, 255, 18), "NEON"   },
    };

    for (int a = 0; a < 2; ++a) {
        std::vector<ImVec2> pts;
        pts.reserve(chart_data.size());
        for (auto& p : chart_data) {
            double ms = (a == 0) ? p.ms_scalar : p.ms_neon;
            float px = fx((double)p.n);
            float py = tl.y + H * (1.f - (float)(ms / maxMs));
            py = std::max(tl.y, std::min(br.y, py));
            pts.push_back(ImVec2(px, py));
        }

        // полигон заливки под кривой
        if (pts.size() >= 2) {
            std::vector<ImVec2> fill;
            fill.reserve(pts.size() + 2);
            fill.push_back(ImVec2(pts.front().x, br.y));
            for (auto& p : pts) fill.push_back(p);
            fill.push_back(ImVec2(pts.back().x, br.y));
            dl->AddConvexPolyFilled(fill.data(), (int)fill.size(), styles[a].colFill);
        }

        // три слоя для эффекта свечения
        dl->AddPolyline(pts.data(), (int)pts.size(), styles[a].colGlow, ImDrawFlags_None, 8.0f);
        dl->AddPolyline(pts.data(), (int)pts.size(), styles[a].colMid,  ImDrawFlags_None, 3.5f);
        dl->AddPolyline(pts.data(), (int)pts.size(), styles[a].colCore, ImDrawFlags_None, 1.8f);
    }

    // --- легенда с тёмным фоном ---
    float legPad = 10.f, legH = 28.f, legW = 180.f;
    float legX = tl.x + 10, legY = tl.y + 10;
    dl->AddRectFilled(ImVec2(legX - legPad, legY - 6),
                      ImVec2(legX + legW, legY + legH),
                      IM_COL32(8, 12, 22, 210), 6.0f);
    dl->AddRect(ImVec2(legX - legPad, legY - 6),
                ImVec2(legX + legW, legY + legH),
                IM_COL32(40, 60, 110, 180), 6.0f);
    for (int a = 0; a < 2; ++a) {
        // маленькая светящаяся черта
        float lx1 = legX + a * 90.f, lx2 = lx1 + 20.f, ly = legY + 7.f;
        dl->AddLine(ImVec2(lx1, ly), ImVec2(lx2, ly), styles[a].colGlow,  5.0f);
        dl->AddLine(ImVec2(lx1, ly), ImVec2(lx2, ly), styles[a].colCore,  2.0f);
        dl->AddText(ImVec2(lx2 + 5, legY), IM_COL32(200, 210, 230, 230), styles[a].name);
    }

    ImGui::Dummy(ImVec2(avail, chartH + 4));
}

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
    
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 0.0f;
    style.FrameRounding     = 6.0f;
    style.GrabRounding      = 6.0f;
    style.TabRounding       = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.PopupRounding     = 6.0f;
    style.FramePadding      = ImVec2(8, 5);
    style.ItemSpacing       = ImVec2(10, 8);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]          = ImVec4(0.06f, 0.07f, 0.10f, 1.00f);
    colors[ImGuiCol_ChildBg]           = ImVec4(0.08f, 0.09f, 0.13f, 1.00f);
    colors[ImGuiCol_FrameBg]           = ImVec4(0.10f, 0.12f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]    = ImVec4(0.15f, 0.18f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive]     = ImVec4(0.18f, 0.22f, 0.34f, 1.00f);
    colors[ImGuiCol_TitleBg]           = ImVec4(0.04f, 0.05f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgActive]     = ImVec4(0.06f, 0.08f, 0.14f, 1.00f);
    colors[ImGuiCol_Tab]               = ImVec4(0.08f, 0.10f, 0.16f, 1.00f);
    colors[ImGuiCol_TabHovered]        = ImVec4(0.15f, 0.40f, 0.70f, 1.00f);
    colors[ImGuiCol_TabSelected]       = ImVec4(0.10f, 0.30f, 0.60f, 1.00f);
    colors[ImGuiCol_Header]            = ImVec4(0.10f, 0.25f, 0.45f, 1.00f);
    colors[ImGuiCol_HeaderHovered]     = ImVec4(0.15f, 0.35f, 0.60f, 1.00f);
    colors[ImGuiCol_HeaderActive]      = ImVec4(0.20f, 0.45f, 0.75f, 1.00f);
    colors[ImGuiCol_Button]            = ImVec4(0.10f, 0.25f, 0.50f, 1.00f);
    colors[ImGuiCol_ButtonHovered]     = ImVec4(0.15f, 0.35f, 0.65f, 1.00f);
    colors[ImGuiCol_ButtonActive]      = ImVec4(0.20f, 0.45f, 0.80f, 1.00f);
    colors[ImGuiCol_SliderGrab]        = ImVec4(0.20f, 0.55f, 0.90f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]  = ImVec4(0.30f, 0.65f, 1.00f, 1.00f);
    colors[ImGuiCol_CheckMark]         = ImVec4(0.30f, 0.65f, 1.00f, 1.00f);
    colors[ImGuiCol_Text]              = ImVec4(0.88f, 0.90f, 0.96f, 1.00f);
    colors[ImGuiCol_TextDisabled]      = ImVec4(0.40f, 0.45f, 0.55f, 1.00f);
    colors[ImGuiCol_Separator]         = ImVec4(0.15f, 0.18f, 0.28f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.20f, 0.25f, 0.38f, 1.00f);
    colors[ImGuiCol_TableBorderLight]  = ImVec4(0.12f, 0.15f, 0.24f, 1.00f);
    colors[ImGuiCol_TableRowBg]        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]     = ImVec4(1.00f, 1.00f, 1.00f, 0.04f);

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

        if (ImGui::BeginTabBar("##tabs")) {

        if (ImGui::BeginTabItem("Демо")) {
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
        
        ImGui::EndTabItem(); // Демо
        }

        if (ImGui::BeginTabItem("График")) {
            DrawChart();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
        } // BeginTabBar

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
