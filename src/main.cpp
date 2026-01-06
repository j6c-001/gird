// main.cpp (gird) - Native (GLFW) + Web (Emscripten) with the same ImGui grid code.

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "GridFramework.h"
#include "GridViewImGui.h"
#include "SimpleRowSource.h"

#include <stdio.h>
#include <string>

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

struct AppState
{
    GLFWwindow* window = nullptr;

    gird::SimpleRowSource src;
    gird::GridDocument doc;
    gird::GridViewModel vm;
    gird::GridController ctl;

    ImVec4 clear_color = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
};

static AppState g;

static void BuildDummyDocument(gird::GridDocument& doc, gird::SimpleRowSource& src)
{
    using namespace gird;

    src.rows.clear();
    src.rows.reserve(10000);
    for (int i = 0; i < 10000; ++i)
    {
        src.rows.push_back({
            "ROW-" + std::to_string(i),
            "Item " + std::to_string(i % 100),
            std::to_string(100000 - i)
        });
    }

    doc.source = &src;

    auto get_i64 = [](int64_t x){ return gird::Value(x); };

    doc.columns.insert(doc.columns.begin(), {
        gird::ColumnDef{
            .id="year", .label="Year", .type=gird::ValueType::Int64, .visible=true,
            .getValue = [](const gird::SimpleRow& r){
                int i = std::stoi(r[0].substr(4));        // from "ROW-123"
                int64_t year = 2024 + (i % 4);
                return gird::Value(year);
            },
            .format = [](const gird::Value& v){ return std::to_string(std::get<int64_t>(v)); },
        },
        gird::ColumnDef{
            .id="quarter", .label="Q", .type=gird::ValueType::Int64, .visible=true,
            .getValue = [](const gird::SimpleRow& r){
                int i = std::stoi(r[0].substr(4));
                int64_t q = 1 + (i % 4);
                return gird::Value(q);
            },
            .format = [](const gird::Value& v){ return "Q" + std::to_string(std::get<int64_t>(v)); },
        },
        gird::ColumnDef{
            .id="month", .label="Month", .type=gird::ValueType::Int64, .visible=true,
            .getValue = [](const gird::SimpleRow& r){
                int i = std::stoi(r[0].substr(4));
                int64_t m = 1 + (i % 12);
                return gird::Value(m);
            },
            .format = [](const gird::Value& v){ return std::to_string(std::get<int64_t>(v)); },
        },
        gird::ColumnDef{
            .id="week", .label="Week", .type=gird::ValueType::Int64, .visible=true,
            .getValue = [](const gird::SimpleRow& r){
                int i = std::stoi(r[0].substr(4));
                int64_t w = 1 + (i % 52);
                return gird::Value(w);
            },
            .format = [](const gird::Value& v){ return std::to_string(std::get<int64_t>(v)); },
        },
    });


    // Framework: mark derived data dirty so first frame builds indices
    // (implementation lives in GridController::rebuild_indices()).
    // Keeping behavior identical: no sort/filter/group yet.
}

static void Frame()
{
    glfwPollEvents();
    if (glfwGetWindowAttrib(g.window, GLFW_ICONIFIED) != 0)
    {
        ImGui_ImplGlfw_Sleep(10);
        return;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Full-screen "app window" inside ImGui.
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(vp->WorkSize, ImGuiCond_Always);

    ImGuiWindowFlags win_flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar;

    ImGui::Begin("GirdMain", nullptr, win_flags);

    // Ensure controller points at the current doc/vm
    g.ctl.doc = &g.doc;
    g.ctl.vm  = &g.vm;

    if (g.vm.dirtyIndices)
        g.ctl.RebuildIndices();

    // Draw grid to fill remaining content region
    gird::DrawGridImGui(g.doc, g.vm, g.ctl, ImGui::GetContentRegionAvail());

    ImGui::End();

    // Render
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(g.window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(g.clear_color.x, g.clear_color.y, g.clear_color.z, g.clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(g.window);
}

int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
    const char* glsl_version = "#version 300 es";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    g.window = glfwCreateWindow((int)(1280 * main_scale), (int)(800 * main_scale), "gird", nullptr, nullptr);
    if (g.window == nullptr)
        return 1;

    glfwMakeContextCurrent(g.window);
    glfwSwapInterval(1);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    // Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(g.window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(g.window, "#canvas"); // works with ImGui's GLFW backend on web [web:125]
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Build framework document + dummy rows
    BuildDummyDocument(g.doc, g.src);
    g.vm.groupByColumnIds = {"year", "month"};
    g.vm.dirtyGroups = true;
    g.vm.dirtyIndices = true;
    g.vm.dirtyGroups  = true;

#ifdef __EMSCRIPTEN__
    // Typically disable ini on web; persistence can be added later.
    io.IniFilename = nullptr;
    emscripten_set_main_loop(Frame, 0, true);
#else
    while (!glfwWindowShouldClose(g.window))
        Frame();
#endif

    // Cleanup (native only, web path won't reach here due to simulate_infinite_loop=true)
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(g.window);
    glfwTerminate();
    return 0;
}
