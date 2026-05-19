// GUI_InitWidgets.cpp
#include "config.h"
#include "GUI.h"
#include "tickbox.h"
#include "radio.h"
#include "layers.h"
#include "progress.h"
#include "replay_progress.h"
#include "hslider.h"
#include "globals.h"
#include "callbacks.h"
#include "model/simulation.h"
#include "hud.h"
#include "tomography.h"
#include <vector>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <GLFW/glfw3.h>

namespace framework
{
    using namespace automaton;

    extern HSlider hslider;
    extern ReplayProgressBar* replayProgress;
    extern double tbegin;
    extern Tickbox* scenarioHelpToggle;
    extern ProgressBar* progress;
    extern std::unique_ptr<LayerList> layerList;
    extern int barWidths[3];
    extern bool showAboutDialog;

    void onDelayToggled(Tickbox* toggled);

    void initializeWidgets(AppContext& ctx)
    {
        assert(L3 > 0);
        voxels.assign(L3, 0);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // ------------------------------------------------------------
        // HSlider – centered horizontally, 80px from top
        // ------------------------------------------------------------
        hslider = HSlider(
            (gViewport[2] - 400.0f) * 0.5f,   // x
            gViewport[3] - 80.0f,                           // y (top-left origin)
            400.0f,                          // width
            20.0f,                           // height
            30.0f                            // thumb width
        );
        hslider.setValue(0.5f);   // <-- this is the correct modern call

        // Assuming you have a slider instance called tomoSlider or similar
        hslider.setOnValueChanged([](float value) {
            tomography::setSlicePosition(value);
        });

        // ------------------------------------------------------------
        // Progress bar (simulation)
        // ------------------------------------------------------------
        if (!progress) {
            int w = gViewport[2] / 4;
            int barHeight = 20;
            progress = new ProgressBar(
                (gViewport[2] - w) * 0.5f,
                130,          // 130px from top
                w,
                barHeight
            );
            progress->init(gViewport[2], 100, 100, 100, automaton::FRAME);
        }

        // ------------------------------------------------------------
        // Replay progress bar
        // ------------------------------------------------------------
        if (!replayProgress) {
            replayProgress = new ReplayProgressBar(gViewport[2], gViewport[3]);
        }

        tbegin = glfwGetTime();
        layerList = std::make_unique<LayerList>(W_USED);

        // ------------------------------------------------------------
        // Optional tomography toggle
        // ------------------------------------------------------------

        if (tomoEnable) {
            tomoEnable->onToggle = [](bool state) {
            	// std::cout << "Tomography mode: " << (state ? "Enabled" : "Disabled") << '\n';
            };
        }
        // ------------------------------------------------------------
        // 3D data tickboxes (starting at Y = 180, top-left origin)
        // ------------------------------------------------------------
        data3D = {
            Tickbox(50, d3Dpos, "Wavefront"),
            Tickbox(50, d3Dpos + 1*RAD_SEP, "Momentum"),
            Tickbox(50, d3Dpos + 2*RAD_SEP, "Spin"),
            Tickbox(50, d3Dpos + 3*RAD_SEP, "Sine mask"),
            Tickbox(50, d3Dpos + 4*RAD_SEP, "Hunting"),
            Tickbox(50, d3Dpos + 5*RAD_SEP, "Centers"),
            Tickbox(50, d3Dpos + 6*RAD_SEP, "Lattice"),
            Tickbox(50, d3Dpos + 7*RAD_SEP, "Axes"),
            Tickbox(50, d3Dpos + 8*RAD_SEP, "Plane")
        };
        /*
        data3D[0].setState(true);
        data3D[5].setState(true);
        data3D[7].setState(true);
        */
        
        for (int i = 0; i < 9; ++i)
        {
            data3D[i].setState(gConfig.data3D[i]);
        }

        // ------------------------------------------------------------
        // Delay tickboxes
        // ------------------------------------------------------------
        delays = {
            Tickbox(50, delaysPos, "Convolution"),
            Tickbox(50, delaysPos + 1*RAD_SEP, "Diffusion"),
            Tickbox(50, delaysPos + 2*RAD_SEP, "Relocation")
        };
        for (Tickbox& box : delays) {
            box.onToggle = [&box](bool) { onDelayToggled(&box); };
        }

        // ------------------------------------------------------------
        // View radios
        // ------------------------------------------------------------
        views = {
            Radio(60, viewsPos, "Isometric"),
            Radio(60, viewsPos + 1*RAD_SEP, "XY"),
            Radio(60, viewsPos + 2*RAD_SEP, "YZ"),
            Radio(60, viewsPos + 3*RAD_SEP, "ZX")
        };
        views[0].setSelected(true);

        // >>> Adjust camera to initial isometric view
        setViewFromRadio(ctx.camera, 0);

        // ------------------------------------------------------------
        // Projection radios
        // ------------------------------------------------------------
        projectRads = {
            Radio(60, projPos, "Ortho"),
            Radio(60, projPos + RAD_SEP, "Perspective")
        };
        projectRads[1].setSelected(true);

        // ------------------------------------------------------------
        // Tomography direction radios
        // ------------------------------------------------------------
        tomoEnable = new Tickbox(50, tomoPos, "Enable");

        tomoDirs = {
            Radio(80, tomoPos + 1*RAD_SEP + 10, "XY"),
            Radio(80, tomoPos + 2*RAD_SEP + 10, "YZ"),
            Radio(80, tomoPos + 3*RAD_SEP + 10, "ZX")
        };

        // Ensure a direction is always selected
        if (!tomoDirs.empty()) {
            tomoDirs[0].setSelected(true);
        }

        // Tomography tickbox configuration
        if (tomoEnable) {
            tomoEnable->onToggle = [](bool state) {
                if (state) {
                    tomography::requestUpdate();
                }
            };
        }
        // ------------------------------------------------------------
        // Progress bar segment widths
        // ------------------------------------------------------------
        int barWidth = gViewport[2] / 4;
        double total = static_cast<double>(FRAME);
        barWidths[0] = static_cast<int>(barWidth * static_cast<double>(CONVOL) / total);
        barWidths[1] = static_cast<int>(barWidth * static_cast<double>(DIFFUSION - CONVOL) / total);
        barWidths[2] = static_cast<int>(barWidth * static_cast<double>(RELOC - DIFFUSION) / total);

        // ------------------------------------------------------------
        // Scenario help toggle
        // ------------------------------------------------------------

        scenarioHelpToggle = new Tickbox(230, 95, "Scenario Help");
        scenarioHelpToggle->setState(false);


        scenarioHelpToggle->onToggle = [](bool) {
            // Checkbox controls scenario help pane only
            // (checked directly via scenarioHelpToggle->getState())
            // F1 key controls UI help text independently
        };

        Radio::setFontScale(0.35f);
        // Set scale for individual pointers initialized late
        scenarioHelpToggle->setFontScale(0.35f);

        // Apply colors globally
        Radio::setColors(
            glm::vec3(0.20f, 0.80f, 0.40f),
            glm::vec3(0.95f, 0.95f, 0.95f),
            glm::vec3(0.80f, 0.80f, 0.80f),
            glm::vec3(0.90f, 0.90f, 0.90f)
        );
        scenarioHelpToggle->setColors(
            glm::vec3(0.20f, 0.80f, 0.40f),
            glm::vec3(0.95f, 0.95f, 0.95f),
            glm::vec3(0.80f, 0.80f, 0.80f),
            glm::vec3(0.90f, 0.90f, 0.90f)
        );
        tomoEnable->setColors(
            glm::vec3(0.20f, 0.80f, 0.40f),
            glm::vec3(0.95f, 0.95f, 0.95f),
            glm::vec3(0.80f, 0.80f, 0.80f),
            glm::vec3(0.90f, 0.90f, 0.90f)
        );
        // Create the menu bar
        menuBar = new MenuBar(textRenderer, gViewport[2], gViewport[3]);

        // Create and add a File menu
        auto fileMenu = std::make_unique<Menu>("File");
        fileMenu->AddItem("New", []() {
            std::cout << "New file" << std::endl;
        });
        fileMenu->AddItem("Open replay", []() {
        });
        fileMenu->AddItem("Save replay", []() {
            std::cout << "Save file" << std::endl;
        });
        fileMenu->AddItem("Exit", [&]() {
            glfwSetWindowShouldClose(window, true);
        });
        menuBar->AddMenu(std::move(fileMenu));

        // Create and add an Edit menu
        auto editMenu = std::make_unique<Menu>("Help");
        editMenu->AddItem("GUI help", []() {
        });
        editMenu->AddItem("About", []() {
        	showAboutDialog = true;
        });
        menuBar->AddMenu(std::move(editMenu));





        tomography::init();
std::cout << "[Init] Tomography initialized\n";
    }

} // namespace framework
