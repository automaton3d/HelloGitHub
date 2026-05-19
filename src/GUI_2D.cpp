// GUI_2D.cpp
// No per-frame allocations, core-profile safe

#include "GUI.h"
#include "globals.h"
#include "layers.h"
#include "progress.h"
#include "replay_progress.h"
#include "logo.h"
#include "splash.h"
#include "hslider.h"
#include "vslider.h"
#include "text_renderer.h"
#include "model/simulation.h"
#include "color_utils.h"
#include "draw_utils.h"
#include "help.h"
#include "projection_manager.h"
#include "config.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <sstream>
#include <vector>

namespace automaton
{
    extern unsigned EL;
    extern unsigned W_USED;
}

namespace splash
{
    extern std::vector<std::string> scenarioOptions;
}

extern Mode currentMode;

namespace framework
{
    using namespace automaton;

    extern TextRenderer hudText;

    // References to variables declared in GUI.cpp and globals.h
    extern bool showHelp;
    extern std::vector<std::string> scenarioHelpTexts;
    extern HSlider hslider;
    extern VSlider vslider;
    extern int windowWidth;
    extern int windowHeight;
    extern std::unique_ptr<LayerList> layerList;

    void renderSimulationStats()
    {
        char s[64];

        if (currentMode == REPLAY) {
            std::snprintf(s, sizeof(s),
                          "Light: %llu",
                          timer / automaton::FRAME);
        } else {
            std::snprintf(s, sizeof(s),
                          "Light: %llu Tick: %llu",
                          timer / automaton::FRAME,
                          timer);
        }

        hudText.RenderText(
            s,
            1050.0f,
            gViewport[3] - 80.0f,
            1.0f,
            glm::vec3(1.0f),
            gViewport[2],
            gViewport[3]
        );
    }

    void renderComputeStats()
    {
        float x = 650.0f;
        float y = gViewport[3] - 80.0f;

        const glm::vec3 strongColor(1.0f);
        const glm::vec3 weakColor(0.5f);

        hudText.RenderText(
            "Compute:",
            x,
            y,
            1.0f,
            strongColor,
            gViewport[2],
            gViewport[3]
        );

        float offset = 120.0f;

        hudText.RenderText(
            "GPU",
            x + offset,
            y,
            1.0f,
            GPUEnabled ? strongColor : weakColor,
            gViewport[2],
            gViewport[3]
        );

        offset += 60.0f;

        hudText.RenderText(
            "CPU",
            x + offset,
            y,
            1.0f,
            GPUEnabled ? weakColor : strongColor,
            gViewport[2],
            gViewport[3]
        );
    }

    void renderLayerInfo()
    {
        if (gConfig.simulation.scenario < 0)
            return;

        char s[64];

        // L e W
        std::snprintf(s, sizeof(s),
                      "L = %u  W = %u",
                      EL, W_USED);

        hudText.RenderText(
            s,
            1700.0f,
            gViewport[3] - 95.0f,
            1.0f,
            glm::vec3(1.0f),
            gViewport[2],
            gViewport[3]
        );

        // Current layer
        if (layerList) {
            int w = layerList->getSelected();

            std::snprintf(s, sizeof(s),
                          "(Current layer = %u)",
                          w);

            if (textRenderer) {
                hudText.RenderText(
                    s,
                    1730.0f,
                    gViewport[3] - 120.0f,
                    0.5f,
                    glm::vec3(1.0f),
                    gViewport[2],
                    gViewport[3]
                );
            }
        }

        // Scenario name
        if (gConfig.simulation.scenario >= 0 &&
            gConfig.simulation.scenario <
            (int)splash::scenarioOptions.size())
        {
            std::snprintf(
                s,
                sizeof(s),
                "%s",
                splash::scenarioOptions[
                    gConfig.simulation.scenario
                ].c_str()
            );

            hudText.RenderText(
                s,
                230.0f,
                gViewport[3] - 80.0f,
                1.0f,
                glm::vec3(1.0f),
                gViewport[2],
                gViewport[3]
            );
        }

        // Mode
        std::snprintf(
            s,
            sizeof(s),
            "Mode: %s",
            currentMode == REPLAY ?
            "Replay" : "Simulation"
        );

        hudText.RenderText(
            s,
            1400.0f,
            gViewport[3] - 80.0f,
            1.0f,
            glm::vec3(1.0f),
            gViewport[2],
            gViewport[3]
        );
    }

    void renderLayers()
    {
        if (layerList) {
            layerList->render(hudText);
            layerList->update(hudText);
        }
    }

    void renderHelpText()
    {
        if (!showHelp)
            return;

        const float scale = 0.4f;
        int totalLines = (int)ui_help.size() + 1 + (int)record_help.size();
        int lineHeight = 18;
        int bottomMargin = 40;

        // Measure longest line to right-align block before the right panel
        float maxWidth = 0;
        for (auto& s : ui_help)
            maxWidth = std::max(maxWidth, hudText.measureTextWidth(s, scale));
        for (auto& s : record_help)
            maxWidth = std::max(maxWidth, hudText.measureTextWidth(s, scale));

        // Position so text block ends just before the right panel (screenW - 260)
        int rightX = gViewport[2] - 270 - (int)maxWidth;

        glm::vec3 color(0.6f);

        int line = 0;

        // UI help (keyboard shortcuts)
        for (size_t i = 0; i < ui_help.size(); ++i) {
            int y = bottomMargin + (totalLines - 1 - line) * lineHeight;
            hudText.RenderText(
                ui_help[i],
                (float)rightX,
                (float)y,
                scale,
                color,
                gViewport[2],
                gViewport[3]
            );
            line++;
        }

        line++; // blank separator

        // Record/replay help
        for (size_t i = 0; i < record_help.size(); ++i) {
            int y = bottomMargin + (totalLines - 1 - line) * lineHeight;
            hudText.RenderText(
                record_help[i],
                (float)rightX,
                (float)y,
                scale,
                color,
                gViewport[2],
                gViewport[3]
            );
            line++;
        }
    }

    void drawPanel(float x, float y,
                   float w, float h,
                   const glm::vec3& bgColor,
                   const glm::vec3& borderColor,
                   float borderThickness,
                   const glm::mat4& proj)
    {
        drawQuad2D(
            x, y,
            x + w, y + h,
            bgColor,
            proj
        );

        drawLine2D_new(
            x, y,
            x + w, y,
            borderColor,
            borderColor,
            proj
        );

        drawLine2D_new(
            x + w, y,
            x + w, y + h,
            borderColor,
            borderColor,
            proj
        );

        drawLine2D_new(
            x + w, y + h,
            x, y + h,
            borderColor,
            borderColor,
            proj
        );

        drawLine2D_new(
            x, y + h,
            x, y,
            borderColor,
            borderColor,
            proj
        );
    }

    void renderScenarioHelpPane()
    {
        if (gConfig.simulation.scenario < 0 ||
            gConfig.simulation.scenario >=
            (int)scenarioHelpTexts.size())
            return;

        const int paneX = 230;
        const int paneY = 150;
        const int paneW = 500;
        const int paneH = 300;

        const glm::mat4& proj =
            ProjectionManager::instance().get2DOrtho();

        drawPanel(
            (float)paneX,
            (float)paneY,
            (float)paneW,
            (float)paneH,
            glm::vec3(0.05f, 0.05f, 0.1f),
            glm::vec3(0.4f, 0.4f, 1.0f),
            2.0f,
            proj
        );

        std::istringstream iss(
            scenarioHelpTexts[
                gConfig.simulation.scenario
            ]
        );

        std::string line;

        int lineY = paneY + 40;

        while (std::getline(iss, line))
        {
            auto wrapped = wrapText(line, 60);

            for (const auto& wline : wrapped)
            {
                int flippedY =
                    windowHeight - lineY;

                hudText.RenderText(
                    wline,
                    (float)(paneX + 20),
                    (float)flippedY,
                    0.8f,
                    glm::vec3(1.0f),
                    windowWidth,
                    windowHeight
                );

                lineY += 25;
            }
        }
    }




  void drawRoundedRect(float x, float y, float w, float h, float radius)
  {
    const int seg = 20;
    const float PI = 3.14159265358979323846f;

    std::vector<glm::vec2> pts;
    pts.reserve(seg * 4 + 4);

    // TL
    for (int i = 0; i <= seg; ++i) {
      float a = (PI / 2.0f) * (float)i / seg;
      pts.emplace_back(
        x + radius - radius * cosf(a),
        y + radius - radius * sinf(a)
      );
    }

    // TR
    for (int i = 0; i <= seg; ++i) {
      float a = (PI / 2.0f) * (float)i / seg + PI / 2.0f;
      pts.emplace_back(
        x + w - radius + radius * cosf(a),
        y + radius - radius * sinf(a)
      );
    }

    // BR
    for (int i = 0; i <= seg; ++i) {
      float a = (PI / 2.0f) * (float)i / seg + PI;
      pts.emplace_back(
        x + w - radius + radius * cosf(a),
        y + h - radius + radius * sinf(a)
      );
    }

    // BL
    for (int i = 0; i <= seg; ++i) {
      float a = (PI / 2.0f) * (float)i / seg + 3.0f * PI / 2.0f;
      pts.emplace_back(
        x + radius - radius * cosf(a),
        y + h - radius + radius * sinf(a)
      );
    }

    pts.push_back(pts[0]);

    const glm::mat4& proj =
      ProjectionManager::instance().get2DOrtho();

    drawTriangleFan2D(
      pts,
      glm::vec3(0.18f, 0.18f, 0.18f),
      proj
    );
  }

  void drawRoundedRectOutline(float x, float y,
                              float w, float h,
                              float radius)
  {
    const int seg = 20;
    const float PI = 3.14159265358979323846f;

    std::vector<glm::vec2> pts;
    pts.reserve(seg * 4 + 4);

    // TL
    for (int i = 0; i <= seg; ++i) {
      float a = (PI / 2.0f) * (float)i / seg;
      pts.emplace_back(
        x + radius - radius * cosf(a),
        y + radius - radius * sinf(a)
      );
    }

    // TR
    for (int i = 0; i <= seg; ++i) {
      float a = (PI / 2.0f) * (float)i / seg + PI / 2.0f;
      pts.emplace_back(
        x + w - radius + radius * cosf(a),
        y + radius - radius * sinf(a)
      );
    }

    // BR
    for (int i = 0; i <= seg; ++i) {
      float a = (PI / 2.0f) * (float)i / seg + PI;
      pts.emplace_back(
        x + w - radius + radius * cosf(a),
        y + h - radius + radius * sinf(a)
      );
    }

    // BL
    for (int i = 0; i <= seg; ++i) {
      float a = (PI / 2.0f) * (float)i / seg + 3.0f * PI / 2.0f;
      pts.emplace_back(
        x + radius - radius * cosf(a),
        y + h - radius + radius * sinf(a)
      );
    }

    pts.push_back(pts[0]);

    const glm::mat4& proj =
      ProjectionManager::instance().get2DOrtho();

    drawLineLoop2D(
      pts,
      glm::vec3(0.45f, 0.45f, 0.45f),
      proj,
      1.0f
    );
  }

  void renderSliders()
  {
    if (tomoEnable && tomoEnable->getState())
      hslider.draw();

    vslider.draw();
  }

  void renderPauseOverlay()
  {
    if (!pause)
      return;

    int vw = gViewport[2];
    int vh = gViewport[3];

    const glm::mat4& proj =
      ProjectionManager::instance().get2DOrtho();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float boxW = 200.0f;
    float boxH = 70.0f;

    float centerX = vw * 0.25f;
    float centerY_fromTop = vh * 0.25f;

    float boxTop    = centerY_fromTop - boxH * 0.5f;
    float boxBottom = centerY_fromTop + boxH * 0.5f;
    float boxLeft   = centerX - boxW * 0.5f;
    float boxRight  = centerX + boxW * 0.5f;

    drawQuad2D(
      boxLeft - 8.0f,
      boxTop - 8.0f,
      boxRight + 8.0f,
      boxBottom + 8.0f,
      glm::vec3(0.0f),
      proj
    );

    drawQuad2D(
      boxLeft - 4.0f,
      boxTop - 4.0f,
      boxRight + 4.0f,
      boxBottom + 4.0f,
      glm::vec3(0.4f, 0.6f, 1.0f),
      proj
    );

    drawQuad2D(
      boxLeft,
      boxTop,
      boxRight,
      boxBottom,
      glm::vec3(0.0f, 0.0f, 0.18f),
      proj
    );

    float textX = centerX - 50.0f;
    float textY = vh - centerY_fromTop - 9.0f;

    hudText.RenderText(
      "PAUSED",
      textX,
      textY,
      1.0f,
      glm::vec3(1.0f, 0.84f, 0.0f),
      vw,
      vh
    );
  }

  void renderSectionLabels()
  {
    glm::vec3 color(0.8f, 0.8f, 1.0f);

    if (textRenderer)
    {
      hudText.RenderText(
        "Data 3D",
        50.0f,
        gViewport[3] - d3Dpos + 15,
        0.8f,
        color,
        gViewport[2],
        gViewport[3]
      );

      hudText.RenderText(
        "Delays",
        50.0f,
        gViewport[3] - delaysPos + 15,
        0.8f,
        color,
        gViewport[2],
        gViewport[3]
      );

      hudText.RenderText(
        "View",
        50.0f,
        gViewport[3] - viewsPos + 20,
        0.8f,
        color,
        gViewport[2],
        gViewport[3]
      );

      hudText.RenderText(
        "Projection",
        50.0f,
        gViewport[3] - projPos + 20,
        0.8f,
        color,
        gViewport[2],
        gViewport[3]
      );

      hudText.RenderText(
        "Tomography",
        50.0f,
        gViewport[3] - tomoPos + 15,
        0.8f,
        color,
        gViewport[2],
        gViewport[3]
      );
    }
  }

} // namespace framework