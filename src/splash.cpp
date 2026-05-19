/*
 * splash.cpp - Single-window version
 * Runs inside existing GLFW window passed from main.cpp
 */

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <atomic>
#include <windows.h>

#include "model/simulation.h"
#include "button.h"
#include "draw_utils.h"
#include "cortina.h"
#include "globals.h"
#include "logo.h"
#include "text_renderer.h"
#include "tickbox.h"
#include "shader.h"
#include "projection_manager.h"
#include "splash.h"
#include "config.h"
#include "Renderer2D.h"

// Declaration of updateProjection from the framework namespace
namespace framework {
    void updateProjection();
}

// External globals from main.cpp
extern TextRenderer* textRenderer;
extern unsigned int textProgram;

// ---------------------------------------------------------------------
// Forward declarations of input callbacks
// ---------------------------------------------------------------------
static int winW();
static int winH();
static void getMousePos(int& mx, int& my);
static int myTopDown(int my_bottom);

void mouseButtonCallback(GLFWwindow*, int, int, int);
void scrollCallback(GLFWwindow*, double, double);
void keyCallback(GLFWwindow*, int, int, int, int);
void passiveMotionCallback(GLFWwindow*, double, double);

// ---------------------------------------------------------------------
// Helpers (2D projection, panels, title)
// ---------------------------------------------------------------------
static const glm::mat4& proj2D() { return ProjectionManager::instance().get2DOrtho(); }
static int winW() { return ProjectionManager::instance().getWidth(); }
static int winH() { return ProjectionManager::instance().getHeight(); }

int Cortina::getSelectedIndex() const
{
    return selectedIndex;
}

namespace splash {

static void setupUI();

} // namespace splash

// ---------------------------------------------------------------------
// UI drawing utilities
// ---------------------------------------------------------------------
static void drawDarkPanel(float x, float y, float w, float h)
{
    float vertices[] = {
        x,     y,
        x + w, y,
        x + w, y + h,
        x,     y + h
    };

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    Renderer2D::use();
    Renderer2D::setMVP(proj2D());

    // Dark background
    Renderer2D::setColor(glm::vec3(0.28f, 0.28f, 0.28f));
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // Border
    Renderer2D::setColor(glm::vec3(0.35f, 0.35f, 0.40f));
    glLineWidth(2.0f);
    glDrawArrays(GL_LINE_LOOP, 0, 4);
    glLineWidth(1.0f);

    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}

static void drawRaisedPanel(float x, float y, float w, float h)
{
    float vertices[] = {
        x,     y,
        x + w, y,
        x + w, y + h,
        x,     y + h
    };

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    Renderer2D::use();
    Renderer2D::setMVP(proj2D());

    // Light background
    Renderer2D::setColor(glm::vec3(0.85f, 0.85f, 0.90f));
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // Border
    Renderer2D::setColor(glm::vec3(0.70f, 0.70f, 0.80f));
    glLineWidth(2.0f);
    glDrawArrays(GL_LINE_LOOP, 0, 4);
    glLineWidth(1.0f);

    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}

static void drawTitle()
{
    if (!textRenderer) return;

    const std::string title = "It from bit: a concrete attempt";
    float scale = 0.6f;

    float textW = textRenderer->measureTextWidth(title, scale);
    float x = (winW() - textW) * 0.5f;
    float y = winH() - 40.0f;

    glUseProgram(textProgram);
    const glm::mat4& P = proj2D();
    glUniformMatrix4fv(glGetUniformLocation(textProgram, "projection"), 1,
                       GL_FALSE, glm::value_ptr(P));

    textRenderer->RenderText(title, x, y, scale, glm::vec3(0.2f, 0.4f, 0.9f),
                             winW(), winH());
    glUseProgram(0);
}

// ---------------------------------------------------------------------
// Namespace splash: state, setup, API
// ---------------------------------------------------------------------
namespace splash {
    int lattice_size = 21;
    int numLayers = 10;
    int selection = -1;
    bool shouldExit = false;
    bool helpHover = false;

    Button* simBtn = nullptr;
    Button* statBtn = nullptr;
    Button* replayBtn = nullptr;
    Button* helpLink = nullptr;
    Tickbox* startPausedBox = nullptr;
    Cortina* sizeDropdown = nullptr;
    Cortina* layerDropdown = nullptr;
    Cortina* scenarioDropdown = nullptr;
    framework::Logo* logo_splash = nullptr;

    GLFWwindow* window = nullptr;

    std::vector<std::string> scenarioOptions = {
        "Wrapping test", "Relocate test", "Orphan test", "Contraction test",
        "Hunting test", "Reissue test", "Dispersion test", "Full simulation"
    };

    void close_drops_callback(int) {
        if (sizeDropdown) sizeDropdown->close();
        if (layerDropdown) layerDropdown->close();
        if (scenarioDropdown) scenarioDropdown->close();
    }

    static void setupUI()
    {
        int rightX = winW() - 250;
        int screenH = winH();

        simBtn    = new Button(rightX, screenH - 260, 200, 40, "Simulation");
        statBtn   = new Button(rightX, screenH - 180, 200, 40, "Statistics");
        replayBtn = new Button(rightX, screenH - 110, 200, 40, "Replay");
        helpLink  = new Button((winW() - 100)/2, screenH - 45, 100, 20, "Help");

        std::vector<std::string> sizes, layers;
        for (int s = 5; s <= 89; s += 2) sizes.push_back(std::to_string(s));
        for (int l : {10,12,14,16,18,20,24,28,32,38,44,52,60,70,82,96,112,130,
                      150,174,200,230,264,300,320,340,360,364})
            layers.push_back(std::to_string(l));

        // ============================================================
        // ORIGINAL DROPDOWN POSITIONS (restored)
        // ============================================================
        sizeDropdown      = new Cortina(50, screenH - 280, 200, 30, sizes, 0, close_drops_callback);
        layerDropdown     = new Cortina(50, screenH - 230, 200, 30, layers, 0, close_drops_callback);
        scenarioDropdown  = new Cortina(50, screenH - 180, 200, 30, scenarioOptions, 0, close_drops_callback);
        startPausedBox    = new Tickbox(rightX, 315, "Start Paused");

        logo_splash = new framework::Logo("logo_bar.png");

        sizeDropdown->setSelectedIndex(8);   // 21
        layerDropdown->setSelectedIndex(0);  // 10

        if (gConfig.simulation.scenario >= 0 &&
            gConfig.simulation.scenario < (int)scenarioOptions.size())
        {
            scenarioDropdown->setSelectedIndex(gConfig.simulation.scenario);
        }
        else
        {
            scenarioDropdown->setSelectedIndex(0);
        }
    }

    int initialize(GLFWwindow* win)
    {
        window = win;
        shouldExit = false;
        helpHover = false;

        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetScrollCallback(window, scrollCallback);
        glfwSetKeyCallback(window, keyCallback);
        glfwSetCursorPosCallback(window, passiveMotionCallback);

        setupUI();
        return 0;
    }

    void cleanup()
    {
        delete simBtn; simBtn = nullptr;
        delete statBtn; statBtn = nullptr;
        delete replayBtn; replayBtn = nullptr;
        delete helpLink; helpLink = nullptr;
        delete startPausedBox; startPausedBox = nullptr;
        delete sizeDropdown; sizeDropdown = nullptr;
        delete layerDropdown; layerDropdown = nullptr;
        delete scenarioDropdown; scenarioDropdown = nullptr;
        delete logo_splash; logo_splash = nullptr;
    }

    void render()
    {
        glClearColor(0.95f, 0.95f, 0.97f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        drawTitle();

        if (logo_splash) {
            logo_splash->draw((winW() - 200) / 2, 60, 1.0f);
        }

        int w = winW(), h = winH();

        if (helpLink) helpLink->drawAsHyperlink(*textRenderer, helpHover, w, h);

        drawRaisedPanel(30, 295, w - 60, 180);
        drawDarkPanel(w - 260, w - 290, 220, 80);

        if (simBtn)    simBtn->draw(*textRenderer, w, h);
        if (statBtn)   statBtn->draw(*textRenderer, w, h);
        if (replayBtn) replayBtn->draw(*textRenderer, w, h);

        if (scenarioDropdown) scenarioDropdown->render(textRenderer);
        if (layerDropdown)    layerDropdown->render(textRenderer);
        if (sizeDropdown)     sizeDropdown->render(textRenderer);

        if (startPausedBox) {
            startPausedBox->setFontScale(0.32f);
            startPausedBox->draw(*textRenderer);
        }
    }
} // namespace splash

// ---------------------------------------------------------------------
// Mouse, scroll, key, and motion callbacks
// ---------------------------------------------------------------------
static void getMousePos(int& mx, int& my)
{
    double x, y;
    glfwGetCursorPos(splash::window, &x, &y);
    mx = static_cast<int>(x);
    my = winH() - static_cast<int>(y);
}

static int myTopDown(int my_bottom)
{
    return winH() - my_bottom;
}

void mouseButtonCallback(GLFWwindow*, int button, int action, int)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;

    double xpos, ypos;
    glfwGetCursorPos(splash::window, &xpos, &ypos);
    int mx = static_cast<int>(xpos);
    int my_raw = static_cast<int>(ypos);
    int my_dropdown = winH() - my_raw;
    int my_button = winH() - my_raw;

    bool handled = false;
    if (splash::sizeDropdown) handled = splash::sizeDropdown->handleMouseClick(mx, my_dropdown);
    if (!handled && splash::layerDropdown) handled = splash::layerDropdown->handleMouseClick(mx, my_dropdown);
    if (!handled && splash::scenarioDropdown) handled = splash::scenarioDropdown->handleMouseClick(mx, my_dropdown);

    if (!handled) {
        if (splash::sizeDropdown) splash::sizeDropdown->close();
        if (splash::layerDropdown) splash::layerDropdown->close();
        if (splash::scenarioDropdown) splash::scenarioDropdown->close();
    }

    if (splash::simBtn && splash::simBtn->contains(mx, my_button, winH())) {
        int sizeIndex = splash::sizeDropdown->getSelectedIndex();
        splash::lattice_size = 5 + (sizeIndex * 2);
        int layerIndex = splash::layerDropdown->getSelectedIndex();
        std::vector<int> layerOptions = {10,12,14,16,18,20,24,28,32,38,44,52,60,70,82,96,112,130,150,174,200,230,264,300,320,340,360,364};
        splash::numLayers = layerOptions[layerIndex];
        automaton::calculateParameters(splash::lattice_size, splash::numLayers);
        if (automaton::tryAllocate(splash::lattice_size, splash::numLayers)) {
            currentMode = SIMULATION;
            glfwMaximizeWindow(splash::window);
            int w, h;
            glfwGetFramebufferSize(splash::window, &w, &h);
            ProjectionManager::instance().setViewport(w, h);
            latticeSize = splash::lattice_size;
            numLayers = splash::numLayers;
            gConfig.simulation.scenario = splash::scenarioDropdown ? splash::scenarioDropdown->getSelectedIndex() : gConfig.simulation.scenario;
            std::cout << "[Splash] scenario = " << gConfig.simulation.scenario << std::endl;
            initPaused = splash::startPausedBox ? splash::startPausedBox->getState() : false;
            splash::shouldExit = true;
        } else {
            MessageBoxA(NULL, "Failed to allocate lattice.\nPlease try other parameters.", "Allocation Error", MB_OK | MB_ICONERROR);
        }
    }
    else if (splash::statBtn && splash::statBtn->contains(mx, my_button, winH())) {
        int sizeIndex = splash::sizeDropdown->getSelectedIndex();
        splash::lattice_size = 5 + (sizeIndex * 2);
        int layerIndex = splash::layerDropdown->getSelectedIndex();
        std::vector<int> layerOptions = {10,12,14,16,18,20,24,28,32,38,44,52,60,70,82,96,112,130,150,174,200,230,264,300,320,340,360,364};
        splash::numLayers = layerOptions[layerIndex];
        automaton::calculateParameters(splash::lattice_size, splash::numLayers);
        if (automaton::tryAllocate(splash::lattice_size, splash::numLayers)) {
            currentMode = STATISTICS;
            gConfig.simulation.scenario = 7;
            splash::shouldExit = true;
        } else {
            MessageBoxA(NULL, "Failed to allocate lattice.\nPlease try other parameters.", "Allocation Error", MB_OK | MB_ICONERROR);
        }
    }
    else if (splash::replayBtn && splash::replayBtn->contains(mx, my_button, winH())) {
        int sizeIndex = splash::sizeDropdown->getSelectedIndex();
        splash::lattice_size = 5 + (sizeIndex * 2);
        int layerIndex = splash::layerDropdown->getSelectedIndex();
        std::vector<int> layerOptions = {10,12,14,16,18,20,24,28,32,38,44,52,60,70,82,96,112,130,150,174,200,230,264,300,320,340,360,364};
        splash::numLayers = layerOptions[layerIndex];
        automaton::calculateParameters(splash::lattice_size, splash::numLayers);
        if (automaton::tryAllocate(splash::lattice_size, splash::numLayers)) {
            currentMode = REPLAY;
            glfwMaximizeWindow(splash::window);
            int w, h;
            glfwGetFramebufferSize(splash::window, &w, &h);
            ProjectionManager::instance().setViewport(w, h);
            splash::shouldExit = true;
        } else {
            MessageBoxA(NULL, "Failed to allocate lattice.\nPlease try other parameters.", "Allocation Error", MB_OK | MB_ICONERROR);
        }
    }
    else if (splash::helpLink && splash::helpLink->contains(mx, my_button, winH())) {
        system("start https://github.com/automaton3d/automaton/blob/master/help.md");
    }
    if (splash::startPausedBox) {
        int myTopDown = winH() - my_button;
        splash::startPausedBox->onClick(mx, myTopDown);
    }
}

void scrollCallback(GLFWwindow*, double xoffset, double yoffset)
{
    int mx, my;
    getMousePos(mx, my);
    if (splash::sizeDropdown && splash::sizeDropdown->isExpanded())
        splash::sizeDropdown->handleMouseScroll(mx, my, yoffset);
    if (splash::layerDropdown && splash::layerDropdown->isExpanded())
        splash::layerDropdown->handleMouseScroll(mx, my, yoffset);
    if (splash::scenarioDropdown && splash::scenarioDropdown->isExpanded())
        splash::scenarioDropdown->handleMouseScroll(mx, my, yoffset);
}

void keyCallback(GLFWwindow*, int key, int, int action, int)
{
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
        automaton::calculateParameters(splash::lattice_size, splash::numLayers);
        if (automaton::tryAllocate(splash::lattice_size, splash::numLayers)) {
            currentMode = SIMULATION;
            
            // ====================================================
            // MAXIMIZA A JANELA E AJUSTA O VIEWPORT
            // ====================================================
            glfwMaximizeWindow(splash::window);
            
            int w, h;
            glfwGetFramebufferSize(splash::window, &w, &h);
            ProjectionManager::instance().setViewport(w, h);
            
            // Update projection for the correct viewport
            framework::updateProjection();
            
            gConfig.simulation.scenario = splash::scenarioDropdown ? splash::scenarioDropdown->getSelectedIndex() : gConfig.simulation.scenario;
            pause = splash::startPausedBox ? splash::startPausedBox->getState() : false;
            splash::shouldExit = true;
        }
    }
}

void passiveMotionCallback(GLFWwindow*, double xpos, double ypos)
{
    int mx = (int)xpos;
    int my = winH() - (int)ypos;
    int my_top = myTopDown(my);
    if (splash::helpLink) splash::helpHover = splash::helpLink->contains(mx, my_top, winH());
    if (splash::sizeDropdown) splash::sizeDropdown->updateHover(mx, my);
    if (splash::layerDropdown) splash::layerDropdown->updateHover(mx, my);
    if (splash::scenarioDropdown) splash::scenarioDropdown->updateHover(mx, my);
}