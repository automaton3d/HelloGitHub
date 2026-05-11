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

// External globals from main.cpp
extern TextRenderer* textRenderer;
extern unsigned int colorProgram2D;
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

static void drawRaisedPanel(float x, float y, float w, float h)
{
    drawQuad2D(x, y, x + w, y + h, glm::vec3(0.85f, 0.85f, 0.9f), proj2D());
    drawLineLoop2D({{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}},
                   glm::vec3(0.7f, 0.7f, 0.8f), proj2D(), 2.0f);
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

    // window atual (para obter cursor, maximizar, etc.)
    GLFWwindow* window = nullptr;

    std::vector<std::string> scenarioOptions = {
        "Wrapping test", "Relocate test", "Orphan test", "Contraction test",
        "Hunting test", "Reissue test", "Dispersion test", "Full simulation"
    };

    void close_drops_callback(int i) {
        sizeDropdown->close();
        layerDropdown->close();
        scenarioDropdown->close();
    }

    // Criação de widgets, sem loop interno
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
            
        sizeDropdown      = new Cortina(50, screenH - 280, 200, 30, sizes, 0, close_drops_callback);
        layerDropdown     = new Cortina(50, screenH - 230, 200, 30, layers, 0, close_drops_callback);
        scenarioDropdown  = new Cortina(50, screenH - 180, 200, 30, scenarioOptions, 0, close_drops_callback);
        startPausedBox    = new Tickbox(rightX, 315, "Start Paused");

        // Logo depois de shader estar pronto (feito em main antes)
        logo_splash = new framework::Logo("logo_bar.png");

sizeDropdown->setSelectedIndex(8);   // 21
layerDropdown->setSelectedIndex(0);  // 10

// Initialize scenario dropdown from config
if (gConfig.simulation.scenario >= 0 &&
    gConfig.simulation.scenario < (int)scenarioOptions.size())
{
    scenarioDropdown->setSelectedIndex(gConfig.simulation.scenario);
}
else
{
    scenarioDropdown->setSelectedIndex(0);
}    }

    // -----------------------------------------------------------------
    // Public API: initialize, render, cleanup
    // -----------------------------------------------------------------
    int initialize(GLFWwindow* win)
    {
        window = win;
        shouldExit = false;
        helpHover = false;

        // Registrar callbacks
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetScrollCallback(window, scrollCallback);
        glfwSetKeyCallback(window, keyCallback);
        glfwSetCursorPosCallback(window, passiveMotionCallback);

        // Criar widgets
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

        // Título
        drawTitle();

        // Logo
        if (logo_splash) {
            logo_splash->draw((winW() - 200) / 2, 60, 1.0f);
        }

        int w = winW(), h = winH();

        // Link de ajuda
        if (helpLink) helpLink->drawAsHyperlink(*textRenderer, helpHover, w, h);

        // Painéis
        drawRaisedPanel(30, 295, w - 60, 180);
        drawRaisedPanel(w - 260, w - 290, 220, 80);

        // Botões
        if (simBtn)    simBtn->draw(colorProgram2D, *textRenderer, w, h);
        if (statBtn)   statBtn->draw(colorProgram2D, *textRenderer, w, h);
        if (replayBtn) replayBtn->draw(colorProgram2D, *textRenderer, w, h);

        // Dropdowns
        if (scenarioDropdown) scenarioDropdown->render(textRenderer);
        if (layerDropdown)    layerDropdown->render(textRenderer);
        if (sizeDropdown)     sizeDropdown->render(textRenderer);

        // Tickbox
        if (startPausedBox) {
            startPausedBox->setFontScale(0.32f);
            startPausedBox->draw(*textRenderer);
        }
    }
} // namespace splash

static void getMousePos(int& mx, int& my)
{
    double x, y;
    glfwGetCursorPos(splash::window, &x, &y);
    mx = static_cast<int>(x);
    my = winH() - static_cast<int>(y); // bottom-up for Button
}

static int myTopDown(int my_bottom)
{
    return winH() - my_bottom;
}

void mouseButtonCallback(GLFWwindow*, int button, int action, int)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;
    
    // Pegue as coordenadas RAW do GLFW (top-down, origem no canto superior esquerdo)
    double xpos, ypos;
    glfwGetCursorPos(splash::window, &xpos, &ypos);
    
    int mx = static_cast<int>(xpos);
    int my_raw = static_cast<int>(ypos);  // Top-down raw
    
    // Para dropdowns: converta para bottom-up baseado em onde eles REALMENTE estão
    int my_dropdown = winH() - my_raw;  // Bottom-up
    
    // Para botões: o código já espera bottom-up
    int my_button = winH() - my_raw;
    
    // Dropdowns recebem coordenadas bottom-up
    bool handled = false;
    if (splash::sizeDropdown)     handled = splash::sizeDropdown->handleMouseClick(mx, my_dropdown);
    if (!handled && splash::layerDropdown)    handled = splash::layerDropdown->handleMouseClick(mx, my_dropdown);
    if (!handled && splash::scenarioDropdown) handled = splash::scenarioDropdown->handleMouseClick(mx, my_dropdown);

    if (!handled) {
        if (splash::sizeDropdown)     splash::sizeDropdown->close();
        if (splash::layerDropdown)    splash::layerDropdown->close();
        if (splash::scenarioDropdown) splash::scenarioDropdown->close();
    }

    // Botões (usa conversão para top-down se necessário)
    if (splash::simBtn && splash::simBtn->contains(mx, my_button, winH())) {
        int sizeIndex = splash::sizeDropdown->getSelectedIndex();
        splash::lattice_size = 5 + (sizeIndex * 2);  // Formula for sizes: 5,7,9,...,89

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

            // FIXED: Set globals to actual values, not indices
            latticeSize = splash::lattice_size;
            numLayers   = splash::numLayers;
            gConfig.simulation.scenario = splash::scenarioDropdown ? splash::scenarioDropdown->getSelectedIndex() : gConfig.simulation.scenario;
            std::cout << "[Splash] scenario = " << gConfig.simulation.scenario << std::endl;
            initPaused  = splash::startPausedBox ? splash::startPausedBox->getState() : false;
            splash::shouldExit = true;
        }
        else {
            MessageBoxA(NULL,
              "Failed to allocate lattice.\nPlease try other parameters.",
              "Allocation Error",
              MB_OK | MB_ICONERROR);
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
            gConfig.simulation.scenario    = 7;
            splash::shouldExit = true;
        }
        else {
            MessageBoxA(NULL,
              "Failed to allocate lattice.\nPlease try other parameters.",
              "Allocation Error",
              MB_OK | MB_ICONERROR);
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
        }
        else {
            MessageBoxA(NULL,
              "Failed to allocate lattice.\nPlease try other parameters.",
              "Allocation Error",
              MB_OK | MB_ICONERROR);
        }
    }
    else if (splash::helpLink && splash::helpLink->contains(mx, my_button, winH())) {
      system("start https://github.com/automaton3d/automaton/blob/master/help.md");
    }
    if (splash::startPausedBox) {
        int myTopDown = winH() - my_button;  // Convert bottom-up to top-down
        splash::startPausedBox->onClick(mx, myTopDown);
    }
}

void scrollCallback(GLFWwindow*, double xoffset, double yoffset)
{
    int mx, my;
    getMousePos(mx, my);  // Get current mouse position
    
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
    if (key == GLFW_KEY_ESCAPE) {
    }
    else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
        automaton::calculateParameters(splash::lattice_size, splash::numLayers);
        if (automaton::tryAllocate(splash::lattice_size, splash::numLayers)) {
            currentMode = SIMULATION;
            gConfig.simulation.scenario    = splash::scenarioDropdown ? splash::scenarioDropdown->getSelectedIndex() : gConfig.simulation.scenario;
            pause       = splash::startPausedBox ? splash::startPausedBox->getState() : false;
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
    if (splash::sizeDropdown)     splash::sizeDropdown->updateHover(mx, my);
    if (splash::layerDropdown)    splash::layerDropdown->updateHover(mx, my);
    if (splash::scenarioDropdown) splash::scenarioDropdown->updateHover(mx, my);
}