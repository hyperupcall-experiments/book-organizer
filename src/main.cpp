#include <iostream>
#include <string>
#include <filesystem>
#include <signal.h>
#include <sys/wait.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "BookOrganizer.h"

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

static void sigchld_handler(int sig) {
    // Clean up zombie processes
    while (waitpid(-1, nullptr, WNOHANG) > 0) {
        // Keep cleaning up children until no more are available
    }
}

int main(int argc, char* argv[]) {
    // Set up signal handler for zombie process cleanup
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, nullptr);

    // Initialize book organizer and load config
    BookOrganizer organizer;
    organizer.loadConfig();

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return 1;
    }

    // GL 3.0 + GLSL 130 (more compatible)
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // Don't force core profile for better compatibility
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Book Organizer", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Show the window
    glfwShowWindow(window);

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Load better sans-serif font
    const char* fontPaths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
        "/usr/share/fonts/opentype/noto/NotoSans-Regular.ttf"
    };

    bool fontLoaded = false;
    for (const char* fontPath : fontPaths) {
        if (std::filesystem::exists(fontPath)) {
            ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath, 15.0f);
            if (font != nullptr) {
                fontLoaded = true;
                std::cout << "Loaded font: " << fontPath << " (15pt)" << std::endl;
                break;
            }
        }
    }

    if (!fontLoaded) {
        std::cout << "Using default ImGui font" << std::endl;
    }

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        std::cerr << "Failed to initialize ImGui GLFW backend" << std::endl;
        glfwTerminate();
        return 1;
    }
    if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
        std::cerr << "Failed to initialize ImGui OpenGL3 backend" << std::endl;
        glfwTerminate();
        return 1;
    }

    std::cout << "ImGui initialized successfully" << std::endl;
    std::cout << "Book organizer initialized, starting main loop..." << std::endl;

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Poll and handle events
        glfwPollEvents();

        // Handle window minimized
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        if (display_w == 0 || display_h == 0) {
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Render our application
        organizer.render();

        // Rendering
        ImGui::Render();
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    organizer.shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
