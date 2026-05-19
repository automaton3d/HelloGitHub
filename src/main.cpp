// main.cpp - Single-window Toy Universe Application


//#define GLFW_INCLUDE_NONE
#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>

#include "globals.h"
#include "config.h"
#include "projection_manager.h"
#include "text_renderer.h"
#include "shader.h"
#include "app_context.h"
#include "scene.h"
#include "input.h"
#include "hud.h"
#include "model/simulation.h"
#include "splash.h"
#include "tickbox.h"
#include <atomic>
#include "cuda/cuda_api.h"
#include "Renderer2D.h"

#include <limits.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

#include "core.h"
#include "callbacks.h"
#include "tomography.h"

// =============================================================================
// Global Variables
// =============================================================================
AppContext ctx;
extern std::atomic<bool> gSimulationThreadRunning;

extern GLuint textProgram;
Mode previousMode = SPLASH;

bool modeInitialized = false;

// =============================================================================
// Forward Declarations
// =============================================================================

// Splash functions (from splash.cpp)
namespace splash {
    void render();
    void cleanup();
}

// Stats functions (from stats.cpp)
namespace stats {
    extern void display(GLFWwindow* window);
    extern void stop();
    extern bool start(GLFWwindow* window, TextRenderer* renderer);
}

// Framework functions
namespace framework {
    void sizeCallback(GLFWwindow *window, int width, int height);
}

// Core functions (from core.cpp)
void StartSimulationThread();
void StopSimulationThread();
void renderFrame(GLFWwindow* window, int& width, int& height);

// Scene functions
void initScene(AppContext& ctx);
void cleanupScene(AppContext& ctx);
void setupInputCallbacks(GLFWwindow* window, AppContext* ctx);

// Shader compilation functions
unsigned int compileTextShader();
unsigned int compileColorShader();
unsigned int compileTextureShader();
unsigned int compileShader(const char* vertexSrc, const char* fragmentSrc);

extern GLuint textureProgram2D;
extern GLint textureMvpLoc;
extern GLint textureSamplerLoc;

extern const char* textVertexShaderSource;
extern const char* textFragmentShaderSource;

void glfwErrorCallback(int error, const char* description)
{
    std::cerr << "[GLFW ERROR] (" << error << "): " << description << std::endl;
}

// =============================================================================
// Mode Initialization Functions
// =============================================================================

int initializeSimulation(GLFWwindow* window)
{
    currentMode = SIMULATION;
    int step = 0;
    while (!automaton::initSimulation(step++));

    // Initialize HUD FIRST (this calls initializeWidgets and fills data3D, views, etc.)
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    unsigned int textShader = compileShader(textVertexShaderSource, textFragmentShaderSource);

    if (!framework::initHUD(ctx, "fonts/arial.ttf", 24, textShader, width, height)) {
        std::cerr << "[Mode] Warning: HUD init failed\n";
    }

    // Initialize scene
    initScene(ctx);

    // Setup input callbacks
    setupInputCallbacks(window, &ctx);

    // Try to enable CUDA acceleration
    GPUEnabled = automaton::tryEnableCuda();
    if (GPUEnabled) {
        std::cout << "[CUDA] ✓ GPU acceleration enabled" << std::endl;
    } else {
        std::cout << "[CUDA] ⚠ Running on CPU" << std::endl;
    }

    // Start simulation thread
    StartSimulationThread();

    if (!gSimulationThreadRunning.load(std::memory_order_acquire)) {
        std::cerr << "[Mode] Simulation thread failed to start!\n";
        return -1;
    }

    timer = 0;
    return 0;
}

int initializeStatistics(GLFWwindow* window)
{
    if (!stats::start(window, textRenderer)) {
        std::cerr << "[Mode] Failed to start statistics mode!\n";
        return -1;
    }

    return 0;
}

int initializeReplay(GLFWwindow* window)
{
    currentMode = REPLAY;
    initScene(ctx);
    setupInputCallbacks(window, &ctx);
    StartSimulationThread();

    return 0;
}

// =============================================================================
// Mode Rendering Functions
// =============================================================================

void renderSimulation(GLFWwindow* window, int width, int height)
{
    // Use the renderFrame from core.cpp
    renderFrame(window, width, height);
}

void renderStatistics(GLFWwindow* window)
{
    stats::display(window);
}

void renderReplay(GLFWwindow* window, int width, int height)
{
    // Similar to simulation rendering
    renderFrame(window, width, height);
}

// =============================================================================
// Mode Cleanup Functions
// =============================================================================

void cleanupSimulation()
{
    StopSimulationThread();
    cleanupScene(ctx);
}

void cleanupStatistics()
{
    stats::stop();
}

void cleanupReplay()
{
    StopSimulationThread();
    cleanupScene(ctx);
}

// =============================================================================
// Main Function
// =============================================================================
int main()
{
    if (!loadConfig("automaton.cfg"))
        std::cerr << "[Config] Using default settings\n";
	glfwSetErrorCallback(glfwErrorCallback);

    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "[FATAL] Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    window = glfwCreateWindow(600, 600, "Toy Universe - It from Bit", nullptr, nullptr);
    if (!window) {
        std::cerr << "[FATAL] Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable VSync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "[FATAL] Failed to initialize GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    Renderer2D::init();

    // Initialize viewport and projection
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    gViewport[0] = 0;
    gViewport[1] = 0;
    gViewport[2] = width;
    gViewport[3] = height;
    glViewport(0, 0, width, height);
    ProjectionManager::instance().setViewport(width, height);

    // Enable rendering features
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Set framebuffer callback
    glfwSetFramebufferSizeCallback(window, framework::sizeCallback);

    // Compile shaders
    textProgram = compileTextShader();
    if (textProgram == 0) {
        std::cerr << "[FATAL] Failed to compile text shader\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    textureProgram2D = compileTextureShader();
    if (textureProgram2D == 0) {
        std::cerr << "[FATAL] Failed to compile 2D texture shader\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    // Get uniform locations
    textureMvpLoc = glGetUniformLocation(textureProgram2D, "uMVP");
    textureSamplerLoc = glGetUniformLocation(textureProgram2D, "uTexture");

    if (textureMvpLoc == -1 || textureSamplerLoc == -1) {
        std::cerr << "[WARNING] Texture shader missing required uniforms!\n";
    }

    // Verify shader linkage
    GLint success;
    // After compiling
    colorProgram3D = compileColorShader();

    // Get uniform locations ONCE, right after linking the program
    colorMvpLoc3D   = glGetUniformLocation(colorProgram3D, "uMVP");
    colorColorLoc3D = glGetUniformLocation(colorProgram3D, "uColor");

    // Simple verification (optional for debug)
    assert(colorMvpLoc3D   != -1 && "uMVP not found in colorProgram3D");
    assert(colorColorLoc3D != -1 && "uColor not found in colorProgram3D");


    //////////////////////
    // Initialize text renderer
    textRenderer = new TextRenderer();
    if (!textRenderer->init("fonts/arial.ttf", 48, textProgram)) {
        std::cerr << "[FATAL] Could not load fonts/arial.ttf\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Start in splash screen
    currentMode = SPLASH;
    previousMode = SPLASH;

    // Initialize splash
    if (splash::initialize(window) != 0) {
        std::cerr << "[FATAL] Failed to initialize splash\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    modeInitialized = true;

    // =============================================================================
    // Main Application Loop
    // =============================================================================
    while (!glfwWindowShouldClose(window) && !gAppShouldExit)
    {
        // Detect mode change
        if (currentMode != previousMode)
        {
            // Cleanup previous mode
            if (modeInitialized) {
                switch (previousMode) {
                    case SPLASH:
                        splash::cleanup();
                        break;
                    case SIMULATION:
                        cleanupSimulation();
                        break;
                    case STATISTICS:
                        cleanupStatistics();
                        break;
                    case REPLAY:
                        cleanupReplay();
                        break;
                }
                modeInitialized = false;
            }

            // Initialize new mode
            int initResult = 0;
            switch (currentMode) {
                case SPLASH:
                    initResult = splash::initialize(window);
                    break;
                case SIMULATION:
                    initResult = initializeSimulation(window);
                    break;
                case STATISTICS:
                    initResult = initializeStatistics(window);
                    break;
                case REPLAY:
                    initResult = initializeReplay(window);
                    break;
            }

            if (initResult != 0) {
                std::cerr << "[App] Mode initialization failed! Exiting...\n";
                gAppShouldExit = true;
                continue;
            }

            modeInitialized = true;
            previousMode = currentMode;
        }

        // Get current window size
        glfwGetFramebufferSize(window, &width, &height);

        // Render current mode (ONE FRAME ONLY)
        switch (currentMode)
        {
            case SPLASH:
            	splash::render();
                break;

            case SIMULATION:
                renderSimulation(window, width, height);
                break;

            case STATISTICS:
                renderStatistics(window);
                break;

            case REPLAY:
                renderReplay(window, width, height);
                break;
        }

        // <-- Put the tomography update check right here
        if (tomographyNeedsUpdate) {
            tomography::update();
            tomographyNeedsUpdate = false;
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // =============================================================================
    // Clean Shutdown
    // =============================================================================
    if (modeInitialized) {
        switch (currentMode) {
            case SPLASH:
                splash::cleanup();
                break;
            case SIMULATION:
                cleanupSimulation();
                break;
            case STATISTICS:
                cleanupStatistics();
                break;
            case REPLAY:
                cleanupReplay();
                break;
        }
    }

    if (textRenderer) {
        delete textRenderer;
        textRenderer = nullptr;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    std::cout << "Exited cleanly.\n";
    return 0;
}
