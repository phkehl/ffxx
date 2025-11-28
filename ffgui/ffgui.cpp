/* ****************************************************************************************************************** */
// flipflip's gui (ffgui)
//
// Copyright (c) Philippe Kehl (flipflip at oinkzwurgl dot org)
// https://oinkzwurgl.org/projaeggd/ffxx/
//
// This program is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, either version 3 of the
// License.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
// even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program.
// If not, see <https://www.gnu.org/licenses/>.

// This is a UBX, NMEA and RTCM3 message parser. It only parses the frames, not the content
// of the message (it does not decode any message fields).
// The parser will pass-through all data that is input. Unknown parts (other protocols,
// spurious data, incorrect messages, etc.) are output as GARBAGE type messages. GARBAGE messages
// are not guaranteed to be combined and can be split arbitrarily (into several GARBAGE messages).

#include "ffgui_inc.hpp"
//
#include <cstdio>
#include <regex>
#include <unistd.h>
//
// #if defined(IMGUI_IMPL_OPENGL_ES2)
// #  include <GLES2/gl2.h>
// #endif
// https://www.khronos.org/opengl/wiki/OpenGL_Loading_Library#GL3W
#include "GL/gl3w.h"
#define GLFW_INCLUDE_GLCOREARB
#include <curl/curl.h>
#include <GLFW/glfw3.h>
#include <freetype/freetype.h>
#include <ft2build.h>
//
#include <fpsdk_common/app.hpp>
#include <fpsdk_common/utils.hpp>
#include <fpsdk_common/time.hpp>
//
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
//
#include "ffgui_icon.hpp"
#include "gui_app.hpp"

namespace ffgui {
/* ****************************************************************************************************************** */

using namespace fpsdk::common::utils;
using namespace fpsdk::common::app;
using namespace fpsdk::common::time;

static int sWindowActivity;  // boost framerate temporarily

static void sGlfwErrorCallback(int error, const char* description);
static void sGlfwCursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
static void sGlfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
static void sGlfwScScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
static void sGlfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
static void sGlfwCharCallback(GLFWwindow* window, unsigned int codepoint);
static void sGlfwWindowResizeCallback(GLFWwindow* window, int width, int height);
static void sGlfwWindowPosCallback(GLFWwindow* window, int xpos, int ypos);


#ifndef NDEBUG
static void sGlHandleDebug(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
    const GLchar* message, const void* userParam);
#endif

// ---------------------------------------------------------------------------------------------------------------------

class AppOptions : public fpsdk::common::app::ProgramOptions
{
   public:
    AppOptions(const std::string& app_name)  // clang-format off
        : ProgramOptions(app_name, {
        { 'c', true,  "config"       },
        { 'x', false, "no-docking"   },
        { 'X', true,  "viewports"    },
        { 'r', false, "reset-config" } })  // clang-format on
    {
    }

    void PrintVersion() override final
    {
        std::fprintf(stdout, "%s (%s, %s)\n", app_name_.c_str(), FF_VERSION_STRING, IF_DEBUG("debug", "release"));
        std::fputs(
            "Copyright (c) Philippe Kehl (flipflip at oinkzwurgl dot org)\n"
            "Licenses: See the LICENSE files included in the source distribution, or the 'About' window in the app\n",
            stdout);
    }

    void PrintHelp() final
    {
        // clang-format off
        std::fprintf(stdout,
            "\n"
            "Usage:\n"
            "\n"
            "    %s [flags]\n", app_name_.c_str());
        std::fputs("\n"
            "Where the [flags] are:\n"
            "\n", stdout);
        // clang-format on
        std::fputs(COMMON_FLAGS_HELP, stdout);
        std::fputs(
            "    -c <config>        -- Use an alternative config (a name)\n"
            "    -x, --no-docking   -- Disable docking in ImGui\n"
            "    -X, --viewports    -- Enable viewports in ImGui\n"
            "    -r, --reset-config -- Reset configuration to defaults\n"
            "\n"
            "Notes:\n"
            "\n"
            "- Debugging: LIBGL_SHOW_FPS=1 LIBGL_DEBUG=verbose ffgui\n"
            "- If you get a *GLFW error 65543: GLX: Failed to create context: GLXBadFBConfig* error, try starting it\n"
            "  with: MESA_GL_VERSION_OVERRIDE=4.3 ffgui\n"
            "- See https://docs.mesa3d.org/envvars.html, https://mesa-docs.readthedocs.io/en/latest/envvars.html\n",
            stdout);
            std::fprintf(stdout, "- Config files are in: %s\n- Cache files are in: %s\n",
                GuiGlobal::cache_.configDir.c_str(), GuiGlobal::cache_.cacheDir.c_str());
        std::fputs(
            "\n"
            "\n", stdout);
    }

    bool HandleOption(const Option& option, const std::string& argument) final
    {
        bool ok = true;
        switch (option.flag) {  // clang-format off
            case 'c': config_       = argument; break;
            case 'x': docking_      = false;    break;
            case 'X': viewports_    = true;     break;
            case 'r': reset_config_ = true;     break;
            default: ok = false; break;
        }  // clang-format on
        return ok;
    }
    bool CheckOptions(const std::vector<std::string>& /*args*/) override final
    {
        DEBUG("docking       = %s", ToStr(docking_));
        DEBUG("viewports     = %s", ToStr(viewports_));
        DEBUG("reset_config  = %s", ToStr(reset_config_));
        return true;
    }
    std::string config_ = "ffgui";
    bool docking_ = true;
    bool viewports_ = false;
    bool reset_config_ = false;
};

// Capture initial log output until GuiApp is taking over
std::vector<std::pair<fpsdk::common::logging::LoggingLevel, std::string> > sEarlyLogs;
static void sAddEarlyLog(const LoggingParams& params, const LoggingLevel level, const char* str)
{
    sEarlyLogs.push_back({ level, str });
    LoggingDefaultWriteFn(params, level, str);
}

// ---------------------------------------------------------------------------------------------------------------------

int RunApp(int argc, char** argv)
{
    // Setup logging, captuture "early logs"
    LoggingParams params = LoggingGetParams();
    params.fn_ = sAddEarlyLog;
    LoggingSetParams(params);

    if (!GuiGlobal::EarlyInit()) {
        ERROR("EarlyInit fail");
        return EXIT_FAILURE;
    }

    // Process command line options
    AppOptions opts("ffgui");
    if (!opts.LoadFromArgv(argc, argv)) {
        return EXIT_FAILURE;
    }

    /* ***** Setup low-level platform and renderer ****************************************************************** */

    const int kWindowMinWidth = 640;
    const int kWindowMinHeight = 384;
    std::string windowTitle = Sprintf("ffgui (%s) " FF_VERSION_STRING IF_DEBUG(" (debug)", ""), opts.config_.c_str());

    // We want to do some OpenGL stuff ourselves. Maybe.
    if (!gl3wInit()) {
        ERROR("gl3wInit() fail!");
        return EXIT_FAILURE;
    }

    glfwSetErrorCallback(sGlfwErrorCallback);
    if (glfwInit() != GLFW_TRUE) {
        return EXIT_FAILURE;
    }

    // https://en.wikipedia.org/wiki/OpenGL_Shading_Language
    // GL 3.3 + GLSL 330
    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    const char* openGlVersion = "OpenGL 3.3, GLSL 3.30";

#ifdef FF_BUILD_DEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4);

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(
        GuiGlobal::config_.windowGeometry[0], GuiGlobal::config_.windowGeometry[1], windowTitle.c_str(), NULL, NULL);
    if (window == NULL) {
        glfwTerminate();
        return EXIT_FAILURE;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // Enable vsync, but see below!
    glfwSetWindowSizeLimits(window, kWindowMinWidth, kWindowMinHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwSetWindowIcon(window, 1, appIcon());

    // User activity detection, for frame rate control
    glfwSetCursorPosCallback(window, sGlfwCursorPositionCallback);
    glfwSetMouseButtonCallback(window, sGlfwMouseButtonCallback);
    glfwSetScrollCallback(window, sGlfwScScrollCallback);
    glfwSetKeyCallback(window, sGlfwKeyCallback);
    glfwSetCharCallback(window, sGlfwCharCallback);
    glfwSetWindowSizeCallback(window, sGlfwWindowResizeCallback);
    glfwSetWindowPosCallback(window, sGlfwWindowPosCallback);

#ifndef NDEBUG
    glDebugMessageCallback(sGlHandleDebug, nullptr);
    // glDebugMessageCallbackARB(sGlHandleDebug, nullptr);
    // glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    // ImPlot3D::CreateContext();

    ImGuiIO& io = ImGui::GetIO();

    // Enable these early, the rest is in GuiSettings::Init();
    if (opts.docking_) {
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    }
    if (opts.viewports_) {
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    }

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    if (!GuiGlobal::Init(opts.config_.c_str(), opts.reset_config_)) {
        glfwTerminate();
        return EXIT_FAILURE;
    }
    WipeCache(GuiGlobal::cache_.tilesCacheDir, 365.25 / 4.0);

    /* ***** Create application ************************************************************************************* */

    // Collect some version infos
    std::vector<std::string> versionInfos;
    versionInfos.push_back(openGlVersion);
    versionInfos.push_back(Sprintf("GLFW %s", glfwGetVersionString()));
    versionInfos.push_back(
        Sprintf("ImGui %s (%d)"
#ifdef IMGUI_HAS_DOCK
                " (Docking branch)"
#endif
                " (%s, %s)",
            ImGui::GetVersion(), IMGUI_VERSION_NUM, io.BackendPlatformName ? io.BackendPlatformName : "?",
            io.BackendRendererName ? io.BackendRendererName : "?"));
    versionInfos.push_back("ImPlot " IMPLOT_VERSION);
    versionInfos.push_back(Sprintf("Fixposition SDK %s", GetVersionString()));
    versionInfos.push_back(Sprintf("libcurl %s", curl_version()));
    versionInfos.push_back(
        "FreeType " STRINGIFY(FREETYPE_MAJOR) "." STRINGIFY(FREETYPE_MINOR) "." STRINGIFY(FREETYPE_PATCH));
    versionInfos.push_back("GCC " STRINGIFY(__GNUC__) "." STRINGIFY(__GNUC_MINOR__) "." STRINGIFY(
        __GNUC_PATCHLEVEL__) " C++ " STRINGIFY(__cplusplus));
    versionInfos.push_back(
        Sprintf("GPU: %s %s %s (GLSL %s)", (const char*)glGetString(GL_VENDOR), (const char*)glGetString(GL_RENDERER),
            (const char*)glGetString(GL_VERSION), (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION)));

    for (const auto& info : versionInfos) {
        DEBUG("%s", info.c_str());
    }

    bool done = false;
    std::unique_ptr<GuiApp> app = std::make_unique<GuiApp>();
    for (const auto& entry : sEarlyLogs) {
        app->AddDebugLog(entry.first, entry.second.c_str());
    }
    sEarlyLogs.clear();
    if (!app->Init(versionInfos)) {
        ERROR("gui init fail");
        done = true;
    }

    // Restore previous window geometry
    if (app) {
        if ((GuiGlobal::config_.windowGeometry[0] > 0) && (GuiGlobal::config_.windowGeometry[1] > 0)) {
            glfwSetWindowSize(window, GuiGlobal::config_.windowGeometry[0], GuiGlobal::config_.windowGeometry[1]);
        }
        if ((GuiGlobal::config_.windowGeometry[2] > 0) && (GuiGlobal::config_.windowGeometry[3] > 0)) {
            glfwSetWindowPos(window, GuiGlobal::config_.windowGeometry[2], GuiGlobal::config_.windowGeometry[3]);
        }
    }

    /* ***** main loop ********************************************************************************************** */

    // Main loop
    Time lastDraw = Time::FromClockRealtime();
    Time lastMark = lastDraw;
    const Duration markInterval = Duration::FromSec(10.0);
    auto frame_sleep = Duration::FromSec(0.005);
    SigIntHelper sigint;
    bool restoreFocus = true;
    while (!done && !sigint.ShouldAbort()) {
        glfwPollEvents();

        const Time now = Time::FromClockRealtime();
        if ((now - lastMark) >= markInterval) {
            DEBUG("----- MARK -----");
            lastMark += markInterval;
        }

        if ((sWindowActivity > 0) || ((now - lastDraw) >= GUI_VAR.maxFrameDur)) {
            lastDraw = now;
            sWindowActivity--;

            app->PerfTic(GuiApp::Perf_e::TOTAL);
            app->PerfTic(GuiApp::Perf_e::NEWFRAME);

            // Update fonts if necessary
            GuiGlobal::UpdateFonts();

            // Start the Dear ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // Set default font and size. Must do this here, before UpdateSizes() amd app->DrawFrame().
            // FIXME: Not sure if this is the designated way of doing this.  Why isn't there a ImGui::SetDefaultFont() or something?
            GuiPushFontRegular();

            GuiGlobal::UpdateSizes();

            app->PerfToc(GuiApp::Perf_e::NEWFRAME);

            // Do things
            app->PerfTic(GuiApp::Perf_e::LOOP);
            app->Loop(now);
            app->PerfToc(GuiApp::Perf_e::LOOP);

            // Compose the frame
            app->PerfTic(GuiApp::Perf_e::DRAW);
            app->DrawFrame();
            app->PerfToc(GuiApp::Perf_e::DRAW);

            GuiPopFont();

            // Confirm close modal
            if (glfwWindowShouldClose(window)) {
                bool wantClose = true;
                app->ConfirmClose(wantClose, done);
                glfwSetWindowShouldClose(window, wantClose);
            }

            // Render
            app->PerfTic(GuiApp::Perf_e::RENDER_IM);
            // ImGui::EndFrame();
            ImGui::Render();
            app->PerfToc(GuiApp::Perf_e::RENDER_IM);
            app->PerfTic(GuiApp::Perf_e::RENDER_GL);
            glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
            const auto c = app->BackgroundColour();
            glClearColor(c.x, c.y, c.z, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
#if (GLFW_VERSION_MAJOR >= 3) && (GLFW_VERSION_MINOR >= 3)
            glfwSetWindowOpacity(window, c.w < 0.2f ? 0.2f : c.w);
#endif
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            app->PerfToc(GuiApp::Perf_e::RENDER_GL);

            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
                app->PerfTic(GuiApp::Perf_e::RENDER_PL);
                GLFWwindow* backup_current_context = glfwGetCurrentContext();
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
                glfwMakeContextCurrent(backup_current_context);
                app->PerfToc(GuiApp::Perf_e::RENDER_PL);
            }

            app->PerfToc(GuiApp::Perf_e::TOTAL);

            // https://github.com/ocornut/imgui/issues/1805
            const bool dragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
            switch (GUI_CFG.dragLagWorkaround) {
                case GuiConfig::DragLagWorkaround::SYNC_ALWAYS:
                    glfwSwapInterval(1);
                    glfwSwapBuffers(window);
                    break;
                case GuiConfig::DragLagWorkaround::SYNC_NEVER:
                    glfwSwapInterval(0);
                    glfwSwapBuffers(window);
                    break;
                case GuiConfig::DragLagWorkaround::NOSYNC_DRAG:
                    glfwSwapInterval(dragging ? 0 : 1);
                    glfwSwapBuffers(window);
                    break;
                case GuiConfig::DragLagWorkaround::NOSYNC_DRAG_LIMIT:
                    glfwSwapInterval(dragging ? 0 : 1);
                    glfwSwapBuffers(window);
                    if (dragging) {
                        frame_sleep.Sleep();
                    }
                    break;
            }

            if (restoreFocus) {
                GuiGlobal::RestoreFocusOrder();
                restoreFocus = false;
            }

            // glFlush();
        } else {
            frame_sleep.Sleep();
        }
    }

    if (app) {
        // Save window geometry
        glfwGetWindowSize(window, &GuiGlobal::config_.windowGeometry[0], &GuiGlobal::config_.windowGeometry[1]);
        glfwGetWindowPos(window, &GuiGlobal::config_.windowGeometry[2], &GuiGlobal::config_.windowGeometry[3]);

        // Tear down
        app.reset();
    }

    // Save settings
    GuiGlobal::Save();

    /* ***** Cleanup ************************************************************************************************ */

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    // ImPlot3D::DestroyContext();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    NOTICE("Adios!");

    return EXIT_SUCCESS;
}

// ---------------------------------------------------------------------------------------------------------------------

static void sGlfwErrorCallback(int error, const char* description)
{
    ERROR("GLFW error %d: %s", error, description);
}

static void sGlfwCursorPositionCallback(GLFWwindow* /*window*/, double /*xpos*/, double /*ypos*/)
{
    sWindowActivity = 10;
}

static void sGlfwMouseButtonCallback(GLFWwindow* /*window*/, int /*button*/, int /*action*/, int /*mods*/)
{
    sWindowActivity = 20;
}

static void sGlfwScScrollCallback(GLFWwindow* /*window*/, double /*xoffset*/, double /*yoffset*/)
{
    sWindowActivity = 20;
}

static void sGlfwKeyCallback(GLFWwindow* /*window*/, int /*key*/, int /*scancode*/, int /*action*/, int /*mods*/)
{
    sWindowActivity = 20;
}

static void sGlfwCharCallback(GLFWwindow* /*window*/, unsigned int /*codepoint*/)
{
    sWindowActivity = 20;
}

static void sGlfwWindowResizeCallback(GLFWwindow* /*window*/, int /*width*/, int /*height*/)
{
    sWindowActivity = 20;
}

static void sGlfwWindowPosCallback(GLFWwindow* /*window*/, int /*xpos*/, int /*ypos*/)
{
    sWindowActivity = 20;
}

#ifndef NDEBUG
static void sGlHandleDebug(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
    const GLchar* message, const void* userParam)
{
    const char* sourceStr = "?";
    switch (source) {  // clang-format off
        case GL_DEBUG_SOURCE_API:               sourceStr   = "API";                          break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:     sourceStr   = "WINDOW_SYSTEM";                break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER:   sourceStr   = "SHADER_COMPILER";              break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:       sourceStr   = "THIRD_PARTY";                  break;
        case GL_DEBUG_SOURCE_APPLICATION:       sourceStr   = "APPLICATION";                  break;
        case GL_DEBUG_SOURCE_OTHER:             sourceStr   = "OTHER";                        break;
    }  // clang-format on
    const char* typeStr = "?";
    switch (type) {  // clang-format off
        case GL_DEBUG_TYPE_ERROR:               typeStr     = "ERROR";                        break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeStr     = "DEPRECATED_BEHAVIOR";          break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  typeStr     = "UNDEFINED_BEHAVIOR";           break;
        case GL_DEBUG_TYPE_PORTABILITY:         typeStr     = "PORTABILITY";                  break;
        case GL_DEBUG_TYPE_PERFORMANCE:         typeStr     = "PERFORMANCE";                  break;
        case GL_DEBUG_TYPE_MARKER:              typeStr     = "MARKER";                       break;
        case GL_DEBUG_TYPE_PUSH_GROUP:          typeStr     = "PUSH_GROUP";                   break;
        case GL_DEBUG_TYPE_POP_GROUP:           typeStr     = "POP_GROUP";                    break;
        case GL_DEBUG_TYPE_OTHER:               typeStr     = "OTHER";                        break;
    }  // clang-format on
    const char* severityStr = "?";
    switch (severity) {  // clang-format off
        case GL_DEBUG_SEVERITY_HIGH:            severityStr = "HIGH";                         break;
        case GL_DEBUG_SEVERITY_MEDIUM:          severityStr = "MEDIUM";                       break;
        case GL_DEBUG_SEVERITY_LOW:             severityStr = "LOW";                          break;
        case GL_DEBUG_SEVERITY_NOTIFICATION:    severityStr = "NOTIFICATION";                 break;
    }  // clang-format on

    UNUSED(length);
    UNUSED(userParam);
    if ((severity == GL_DEBUG_SEVERITY_MEDIUM) || (severity == GL_DEBUG_SEVERITY_HIGH)) {
        WARNING("GL (%s, %s, %s, %d) %s", sourceStr, typeStr, severityStr, id, message);
    } else {
        DEBUG("GL (%s, %s, %s, %d) %s", sourceStr, typeStr, severityStr, id, message);
    }
}
#endif

/* ****************************************************************************************************************** */
}  // namespace ffgui

int main(int argc, char** argv)
{
#ifndef NDEBUG
    StacktraceHelper stacktrace;
#endif
    return ffgui::RunApp(argc, argv);
}
