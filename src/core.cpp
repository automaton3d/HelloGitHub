// core.cpp - Thread Safety Improvements


#include <glad/glad.h>
#include <GLFW/glfw3.h>
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>

#endif
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <string>
#include <map>
#include <array>
#include <atomic>

#include "app_context.h"
#include "scene.h"
#include "input.h"
#include "hud.h"
#include "tickbox.h"
#include "shader.h"
#include "model/simulation.h"
#include "text_renderer.h"
#include "globals.h"
#include "GUI.h"
#include "recorder.h"
#include "replay_progress.h"
#include "replay.h"
#include "tinyfiledialogs.h"
#include "projection.h"
#include "hslider.h"
#include "projection_manager.h"
#include "tomography.h" // This should be at the top of core.cpp

// Note: If you create thread_safety.h, include it here:
// #include "thread_safety.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

using namespace std;

// =============================================================================
// Global Application Context
// =============================================================================
extern AppContext ctx;

// =============================================================================
// Thread-Safe State Management
// =============================================================================

// Mutex for voxel buffer access
std::mutex gVoxelBufferMutex;

// Condition variable for thread readiness
static std::condition_variable gThreadReadyCV;
static std::mutex gThreadReadyMutex;
static bool gThreadReady = false;

// =============================================================================
// Framework Namespace - Thread-Safe State
// =============================================================================
namespace framework
{
    // Note: replayFrames and recordFrames are declared in GUI.h and recorder.h
    // stopSimThread and pendingExit need to be declared in a header
    std::atomic<bool> stopSimThread{false};
    std::atomic<bool> pendingExit{false};

    // Non-atomic bools (only accessed from main thread)
    bool showExitDialog = false;
    bool savePopup = false;
    bool loadPopup = false;

    // Toast - needs mutex since set from sim thread
    std::mutex toastMutex;
    std::string toastMessage;
    double toastStartTime = 0.0;
    bool toastActive = false;

    // Replay
    size_t replayIndex = 0;
    unsigned long long replayTimer = 0;

    // Recorder synchronization mutex
    std::mutex recorderMutex;
    const float fullSize = 0.5f;

    constexpr int BOTTOM_UI_ZONE = 120;
    constexpr int LEFT_UI_ZONE   = 200;

    bool isIn3DZone(double xpos, double ypos, int windowWidth, int windowHeight)
    {
        double bottomY = static_cast<double>(windowHeight) - ypos;
        return (bottomY > BOTTOM_UI_ZONE && xpos > LEFT_UI_ZONE);
    }

    void onDelayToggled(Tickbox* toggled)
    {
        int i = 0;
        for (const auto &box : delays)
        {
            switch (i)
            {
                case 0: automaton::convol_delay = box.getState(); break;
                case 1: automaton::diffuse_delay = box.getState(); break;
                case 2: automaton::reloc_delay = box.getState(); break;
            }
            i++;
        }
    }

    // Helper function to show toast safely
    void showToast(const std::string& message)
    {
        std::lock_guard<std::mutex> lock(toastMutex);
        toastMessage = message;
        toastStartTime = glfwGetTime();
        toastActive = true;
    }
}

// =============================================================================
// Clear voxel buffer - now thread-safe
// =============================================================================
void clearVoxels()
{
    std::lock_guard<std::mutex> lock(gVoxelBufferMutex);
    for (unsigned i = 0; i < automaton::EL * automaton::EL * automaton::EL; ++i)
        voxels[i] = 0x000000;
}

// =============================================================================
// Simulation Thread (runs forever until program exit)
// =============================================================================
void SimulateThread()
{
    std::puts("Simulation thread launched...");

    {
        std::lock_guard<std::mutex> lock(gThreadReadyMutex);
        gThreadReady = true;
    }
    gThreadReadyCV.notify_one();

        std::cout << "Waiting a bit for initialization...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));  // give time for splash/init to run

        std::cout << "BLOCK = " << automaton::BLOCK 
                  << " | draft.size = " << automaton::lattice_draft.size() << std::endl;

        if (automaton::BLOCK > 1000 && automaton::lattice_draft.size() >= automaton::BLOCK) {
            std::cout << "Doing initial swap...\n";
            automaton::swap_lattices();
        } else {
            std::cout << "Still skipping initial swap (lattices not ready)\n";
        }

        // Safer version - avoids accessing uninitialized variables
        bool canSwap = false;
        try {
            if (automaton::BLOCK > 100 && 
                automaton::lattice_draft.size() >= automaton::BLOCK &&
                automaton::lattice_curr.size() >= automaton::BLOCK) {
                canSwap = true;
            }
        } catch (...) {
            std::cout << "Exception checking lattice sizes\n";
        }

        if (canSwap) {
            std::cout << "Doing initial swap...\n";
            automaton::swap_lattices();
            std::cout << "Initial lattice swap performed.\n";
        } else {
            std::cout << "Skipping initial swap - lattices not ready (BLOCK=" 
                      << automaton::BLOCK 
                      << ", draft.size=" << automaton::lattice_draft.size() 
                      << ", curr.size=" << automaton::lattice_curr.size() << ")\n";
        }

    while (!framework::stopSimThread.load(std::memory_order_acquire))
    {
        if (!pause)
        {
            if (framework::replayFrames)
            {
                if (framework::updateReplay()) {
                    std::lock_guard<std::mutex> lock(timerMutex);
                    timer++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(250));
                } else {
                    framework::replayFrames = false;
                }
            }
            else if (currentMode == SIMULATION)
            {
                // Physics evolution
                bool simResult = automaton::simulation();

                // Frame recording (if active)
                if (simResult && framework::recordFrames) 
                {
                    std::lock_guard<std::mutex> lock(framework::recorderMutex);
                    unsigned long long currentTimer;
                    {
                        std::lock_guard<std::mutex> tLock(timerMutex);
                        currentTimer = timer;
                    }
                    framework::recorder.recordFrame(automaton::lattice_curr, currentTimer, gConfig.simulation.scenario);
                }

                // Buffer synchronization and signaling
                {
                    std::lock_guard<std::mutex> lock(gVoxelBufferMutex);
                    automaton::updateBuffer();
                    
                    // Just raise the flag.
                    // The heavy copy work will be done in the render thread.
                    tomography::requestUpdate();
                }

                {
                    std::lock_guard<std::mutex> lock(timerMutex);
                    timer++;
                }
            }
        }
        else
        {
            // If tomography changes during pause, update once
            static bool prevTomoState = false;
            bool currentTomoState = (tomoEnable && tomoEnable->getState());
            if (currentTomoState != prevTomoState)
            {
                std::lock_guard<std::mutex> lock(gVoxelBufferMutex);
                automaton::updateBuffer();
                tomography::requestUpdate();
            }
            prevTomoState = currentTomoState;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::puts("Simulation thread terminated cleanly.");
}

// =============================================================================
// Start Simulation Thread (call ONCE, early!)
// =============================================================================
void StartSimulationThread()
{
    if (gSimulationThreadRunning.load(std::memory_order_acquire))
        return;

    framework::stopSimThread.store(false, std::memory_order_release);

    // Reset ready flag
    {
        std::lock_guard<std::mutex> lock(gThreadReadyMutex);
        gThreadReady = false;
    }

    gSimThread = std::thread(SimulateThread);

    // Wait for thread to be ready using condition variable (max 2 seconds)
    {
        std::unique_lock<std::mutex> lock(gThreadReadyMutex);
        if (!gThreadReadyCV.wait_for(lock, std::chrono::seconds(2), []{ return gThreadReady; }))
        {
            std::cerr << "[ERROR] Simulation thread failed to start in time!\n";
            return;
        }
    }

    gSimulationThreadRunning.store(true, std::memory_order_release);
}

// =============================================================================
// Stop Simulation Thread (call before cleanup)
// =============================================================================
void StopSimulationThread()
{
    if (!gSimulationThreadRunning.load(std::memory_order_acquire))
        return;

    framework::stopSimThread.store(true, std::memory_order_release);

    // Join the thread instead of detaching
    if (gSimThread.joinable())
    {
        gSimThread.join();
    }

    gSimulationThreadRunning.store(false, std::memory_order_release);
}

// =============================================================================
// Main Render Loop - Consolidated
// =============================================================================
void renderFrame(GLFWwindow* window, int& width, int& height)
{
    if (framework::pendingExit.load(std::memory_order_acquire))
    {
        framework::pendingExit.store(false, std::memory_order_release);
        framework::showExitDialog = true;
    }

    processInput(window, ctx);

    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(ctx.shader);

    glfwGetFramebufferSize(window, &width, &height);
    if (width != gViewport[2] || height != gViewport[3])
    {
        gViewport[0] = 0;
        gViewport[1] = 0;
        gViewport[2] = width;
        gViewport[3] = height;
        glViewport(0, 0, width, height);
    }

    float aspect = (float)width / (float)height;

    glUniformMatrix4fv(glGetUniformLocation(ctx.shader, "projection"), 1, GL_FALSE,
        glm::value_ptr(ctx.camera.GetProjectionMatrix(aspect)));
    glUniformMatrix4fv(glGetUniformLocation(ctx.shader, "view"), 1, GL_FALSE,
        glm::value_ptr(ctx.camera.GetViewMatrix()));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Lock voxel buffer during scene rendering
    {
        std::lock_guard<std::mutex> lock(gVoxelBufferMutex);


        framework::updateProjection();


        renderScene(ctx);
    }

    framework::renderHUD(width, height);

    if (framework::menuBar)
    	framework::menuBar->Render();

    // Toast handling - thread-safe
    if (framework::savePopup)
    {
        framework::savePopup = false;
        framework::showToast("Frames saved successfully.");
    }
    if (framework::loadPopup)
    {
        framework::loadPopup = false;
        framework::showToast("Frames loaded successfully.");
    }
}

void toggleFullscreen(GLFWwindow* window) {
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    if (!isFullscreen) {
        // Save current window state
        glfwGetWindowPos(window, &windowedPosX, &windowedPosY);
        glfwGetWindowSize(window, &windowedWidth, &windowedHeight);

        // Go fullscreen
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        isFullscreen = true;
    } else {
        // Go windowed
        glfwSetWindowMonitor(window, nullptr,
                           windowedPosX,
                           windowedPosY,
                           windowedWidth,
                           windowedHeight,
                           0);
        isFullscreen = false;
    }

    // Update viewport
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    ProjectionManager::instance().setViewport(width, height);

    if (framework::menuBar) {
        framework::menuBar->Resize((float)width, (float)height);
    }
}
