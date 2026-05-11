// hud.cpp - FIXED to work with ProjectionManager
#include "hud.h"
#include "text_renderer.h"
#include "shader.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <iostream>
#include <sstream>
#include "GUI.h"
#include "globals.h"
#include "logo.h"
#include "replay_progress.h"
#include "help.h"
#include "projection_manager.h"
#include "tomography.h"

extern int textureSamplerLoc;


namespace framework {
    TextRenderer hudText;

extern bool showAboutDialog;

double tbegin = 0;
std::unique_ptr<LayerList> layerList;
Logo* logo = nullptr;
ReplayProgressBar *replayProgress = nullptr;
ProgressBar* progress = nullptr;

int windowWidth = 800;
int windowHeight = 600;

bool helpHover = false;
bool showHelp = false;

int barWidths[3] = {0, 0, 0};

extern Tickbox* scenarioHelpToggle;

// Helper for 2D projection
const glm::mat4& proj2D() { return ProjectionManager::instance().get2DOrtho(); }

// ------------------------------------------------------------
// Init HUD
// ------------------------------------------------------------
bool initHUD(AppContext& ctx, const std::string& fontPath, int fontSize, unsigned int shader,
             int screenW, int screenH)
{
    if (!hudText.init(fontPath, fontSize, shader)) {
        std::cerr << "Failed to initialize HUD text renderer\n";
        return false;
    }
    initializeWidgets(ctx);

    tbegin = glfwGetTime();

    textureProgram2D = compileTextureShader();


    if (textureProgram2D == 0)
        std::cerr << "TEXTURE SHADER FAILED TO COMPILE/LINK!" << std::endl;

    textureMvpLoc    = glGetUniformLocation(textureProgram2D, "uMVP");
    textureSamplerLoc= glGetUniformLocation(textureProgram2D, "uTexture");

    colorProgram2D = compileColorShader();
    colorMvpLoc2D  = glGetUniformLocation(colorProgram2D, "uMVP");
    colorColorLoc2D= glGetUniformLocation(colorProgram2D, "uColor");

    logo = new Logo("logo_bar.png");

    int w = screenW;
    int h = screenH;
    float linkY = h - 45.0f;
    gHelpLink = new Button((w - 200)/2.0f, linkY, 200, 30, "Help", false);

    return true;
}

// ------------------------------------------------------------
// Render elapsed time
// ------------------------------------------------------------
void renderElapsedTime()
{
    double now = glfwGetTime();
    double seconds = now - tbegin;

    char buf[64];
    snprintf(buf, sizeof(buf), "Elapsed %.1f s", seconds);

    hudText.RenderText(buf, 20.0f, 40.0f, 0.5f, glm::vec3(1.0f));
}

void renderAboutDialog()
{
    if (!showAboutDialog) return;

    const int paneW = 520;
    const int paneH = 320;
    const int paneX = (windowWidth  - paneW) / 2;
    const int paneY = (windowHeight - paneH) / 2;

    const glm::mat4& proj = proj2D();

    // Background panel (reuses same style as scenario help)
    drawPanel((float)paneX, (float)paneY,
              (float)paneW, (float)paneH,
              glm::vec3(0.05f, 0.05f, 0.1f),
              glm::vec3(0.4f, 0.4f, 1.0f),
              2.0f, proj);

    const std::string about =
        "Cellular Automaton Visualizer\n\n"
        "Version 1.0\n\n"
        "Real-time 3D cellular automaton\n"
        "with recording, replay & tomography\n\n"
        "Built with OpenGL + GLFW\n"
        "(c) 2025";

    std::istringstream iss(about);
    std::string line;
    int lineY = paneY + 40;

    while (std::getline(iss, line))
    {
        auto wrapped = wrapText(line, 48);

        for (const auto& wline : wrapped)
        {
            int flippedY = windowHeight - lineY;

            hudText.RenderText(
                wline,
                (float)(paneX + 20),
                (float)flippedY,
                0.7f,
                glm::vec3(1.0f),
                windowWidth,
                windowHeight
            );

            lineY += 24;
        }
    }

    // Close button (reusing same positioning logic)
    hudText.RenderText(
        "[ Close ]",
        (float)(paneX + paneW / 2 - 45),
        (float)(windowHeight - (paneY + paneH - 35)),
        0.7f,
        glm::vec3(1.0f, 0.3f, 0.3f),
        windowWidth,
        windowHeight
    );
}

// ------------------------------------------------------------
// Render HUD
// ------------------------------------------------------------
void renderHUD(int screenW, int screenH)
{
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const glm::mat4& P = proj2D();

    // Set uniforms for 2D shaders
    glUseProgram(colorProgram2D);
    glUniformMatrix4fv(colorMvpLoc2D, 1, GL_FALSE, &P[0][0]);

    glUseProgram(textureProgram2D);
    glUniformMatrix4fv(textureMvpLoc, 1, GL_FALSE, &P[0][0]);

    renderAboutDialog();

    if (scenarioHelpToggle && scenarioHelpToggle->getState()) {
        renderScenarioHelpPane();
    }

    renderElapsedTime();

    // Panels
    int leftH  = screenH - 170;
    int rightH = screenH - 170;

    drawPanel(35, 60, 170, leftH, glm::vec3(0.05f, 0.05f, 0.1f), glm::vec3(0.4f, 0.4f, 1.0f), 2.0f, P);
    drawPanel(screenW - 260, 60, 250, rightH, glm::vec3(0.05f, 0.05f, 0.1f), glm::vec3(0.4f, 0.4f, 1.0f), 2.0f, P);

    // Logo
    if (logo && logo->valid())
        logo->draw(screenW - 250, screenH - 350, 1.0f);

    renderSectionLabels();
    render3Dboxes();
    renderDelays();
    renderViewpointRadios();
    renderProjectionRadios();
    renderTomoControls();
    renderTomoRadios();
    renderSimulationStats();
    renderComputeStats();
    renderLayerInfo();
    renderLayers();
    renderHelpText();
    renderSliders();
    tomography::renderControls();
    renderPauseOverlay();
    renderHyperlink();
    renderScenarioHelpToggle();
    renderGizmo();

    if (currentMode == SIMULATION)
    {
        if (gConfig.simulation.scenario >= 0 && progress)
        {
            unsigned long long safeTimer;
            {
                std::lock_guard<std::mutex> lock(timerMutex);
                safeTimer = timer;
            }

            if (progress->getWidth() > 0)
            {
                progress->resize(screenW, screenH);
                progress->update(safeTimer, automaton::FRAME);
                progress->draw(safeTimer, automaton::FRAME, screenW, screenH);
            }
        }
    }


    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void renderHyperlink()
{
    if (!gHelpLink || !textRenderer) return;

    int w = gViewport[2];
    int h = gViewport[3];
    // Use the same beautiful hyperlink rendering as Splash
    gHelpLink->drawAsHyperlink(*textRenderer, helpHover, w, h);
}

void cleanup()
{
	delete gHelpLink;
	gHelpLink = nullptr;
}

} // namespace framework
