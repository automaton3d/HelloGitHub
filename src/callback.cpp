/*
 * callback.cpp - UI callbacks (mouse, keyboard, scroll)
 * Scroll: perspective → setScrollDirection, orthographic → ortho_scale
 */

#include "GUI.h"
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <mouse_helper.h>
#include <windows.h>
#include "hslider.h"
#include "vslider.h"
#include "radio.h"
#include "tickbox.h"
#include "projection.h"
#include "text_renderer.h"
#include "globals.h"
#include "menubar.h"
#include "model/simulation.h"
#include "animator.h"
#include "replay_progress.h"
#include "tomography.h"
#include "app_context.h"
#include "render_pipeline.h"

void toggleFullscreen(GLFWwindow* window);

namespace framework {

    // Feedback cube variables (gizmo)
    bool showDragCube = false;
    float dragCubeX = 0.0f;
    float dragCubeY = 0.0f;
    int gizmoHoverAxis = -1;

    extern bool showExitDialog;
    extern bool showHelp;
    extern HSlider hslider;
    extern VSlider vslider;
    extern Tickbox *scenarioHelpToggle;
    extern bool showAboutDialog;
    extern float axisLength;
    extern ReplayProgressBar *replayProgress;

    void onDelayToggled(Tickbox* toggled);
    void updateProjection();
    bool isIn3DZone(double xpos, double ypos, int windowWidth, int windowHeight);

    // -----------------------------------------------------------------
    // View radio helper
    // -----------------------------------------------------------------
    void setViewFromRadio(OrbitCamera& cam, int viewIndex) {
        switch (viewIndex) {
            case 0: cam.yaw = 45.0f; cam.pitch = 30.0f; break;
            case 1: cam.yaw = 0.0f;  cam.pitch = 90.0f; break;
            case 2: cam.yaw = 90.0f; cam.pitch = 0.0f; break;
            case 3: cam.yaw = 0.0f;  cam.pitch = 0.0f; break;
        }
    }

    // -----------------------------------------------------------------
    // Window resize
    // -----------------------------------------------------------------
    void sizeCallback(GLFWwindow *window, int width, int height) {
        glViewport(0, 0, width, height);
        ProjectionManager::instance().setViewport(width, height);
        gViewport[0] = 0;
        gViewport[1] = 0;
        gViewport[2] = width;
        gViewport[3] = height;

        resize(width, height);
        setScreenSize(width, height);

        if (replayProgress)
            replayProgress->setPosition(width, height, 100);
        if (menuBar)
            menuBar->Resize((float)width, (float)height);

        updateProjection();
    }

    // -----------------------------------------------------------------
    // Mouse button callback (clicks, gizmo, UI)
    // -----------------------------------------------------------------
    void buttonCallback(GLFWwindow *window, int button, int action, int mods) {
        if (showExitDialog) return;
        AppContext* ctx = static_cast<AppContext*>(glfwGetWindowUserPointer(window));
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        switch (action) {
            case GLFW_PRESS:
                switch (button) {
                    case GLFW_MOUSE_BUTTON_LEFT: {
                        // Menu bar
                        if (menuBar && (ypos <= 30.0 || menuBar->IsMenuOpen())) {
                            menuBar->HandleMouse(xpos, ypos, gViewport[2], gViewport[3], true);
                            if (menuBar->IsMenuOpen() || ypos <= 30.0) return;
                        }

                        // Gizmo thumb detection (highest priority)
                        if (showGizmo && (pause || currentMode == REPLAY)) {
                            float mx = (float)xpos, my = (float)ypos;
                            const float hitRadius = 15.0f;
                            int bestAxis = -1;
                            float bestDist = hitRadius;
                            for (int axis = 0; axis < 3; ++axis) {
                                float dx = mx - gGizmoProj.ex[axis];
                                float dy = my - gGizmoProj.ey[axis];
                                float dist = std::hypot(dx, dy);
                                if (dist < bestDist) {
                                    bestDist = dist;
                                    bestAxis = axis;
                                }
                            }
                            if (bestAxis != -1) {
                                thumb.active = true;
                                thumb.axis = bestAxis;
                                thumb.dragging = true;
                                thumb.startMouseX = xpos;
                                thumb.startMouseY = ypos;
                                thumb.initialPosition = 0.5f;
                                thumb.position = 0.5f * gGizmoProj.radius;
                                thumb.startOffset[0] = gConfig.view.vis_dx;
                                thumb.startOffset[1] = gConfig.view.vis_dy;
                                thumb.startOffset[2] = gConfig.view.vis_dz;
                                thumb.startOffsetF[0] = (float)gConfig.view.vis_dx;
                                thumb.startOffsetF[1] = (float)gConfig.view.vis_dy;
                                thumb.startOffsetF[2] = (float)gConfig.view.vis_dz;
                                return;
                            }
                        }

                        // Other UI
                        vslider.onMouseButton(button, action, xpos, ypos);
                        hslider.onMouseButton(button, action, xpos, ypos);
                        if (hslider.isDragging() || vslider.isDragging()) {
                            tomography::setSlicePosition(hslider.getValue());
                            tomography::update();
                            return;
                        }

                        int mouseX = (int)xpos, mouseY = (int)ypos;
                        for (Tickbox& cb : data3D) {
                            cb.onClick(mouseX, mouseY);
                            if (cb.contains(mouseX, mouseY)) return;
                        }
                        for (Tickbox& cb : delays) {
                            if (cb.contains(mouseX, mouseY)) {
                                cb.onClick(mouseX, mouseY);
                                onDelayToggled(&cb);
                                return;
                            }
                        }
                        if (scenarioHelpToggle->contains(mouseX, mouseY)) {
                            scenarioHelpToggle->onClick(mouseX, mouseY);
                            return;
                        }
                        for (size_t i = 0; i < views.size(); ++i) {
                            if (views[i].contains(mouseX, mouseY)) {
                                for (Radio& r : views) r.setSelected(false);
                                views[i].setSelected(true);
                                if (ctx) setViewFromRadio(ctx->camera, (int)i);
                                return;
                            }
                        }
                        if (layerList) layerList->poll(mouseX, mouseY);
                        for (size_t i = 0; i < projectRads.size(); ++i) {
                            if (projectRads[i].contains(mouseX, mouseY)) {
                                for (Radio& r : projectRads) r.setSelected(false);
                                projectRads[i].setSelected(true);
                                updateProjection();
                                return;
                            }
                        }
                        // Tomography
                        if (tomoEnable && tomoEnable->contains(mouseX, mouseY)) {
                            tomoEnable->onClick(mouseX, mouseY);
                            if (!tomoEnable->getState()) {
                                setPipelineState(RenderPipelineState::FULL_VOLUME);
                                return;
                            }
                            for (Radio& r : tomoDirs) r.setSelected(false);
                            if (!tomoDirs.empty()) {
                                tomoDirs[0].setSelected(true);
                                setPipelineState(RenderPipelineState::TOMOGRAPHY_XY);
                            }
                            return;
                        } else if (tomoEnable && tomoEnable->getState()) {
                            for (size_t i = 0; i < tomoDirs.size(); ++i) {
                                if (tomoDirs[i].contains(mouseX, mouseY)) {
                                    for (Radio& r : tomoDirs) r.setSelected(false);
                                    tomoDirs[i].setSelected(true);
                                    switch (i) {
                                        case 0: setPipelineState(RenderPipelineState::TOMOGRAPHY_XY); break;
                                        case 1: setPipelineState(RenderPipelineState::TOMOGRAPHY_YZ); break;
                                        case 2: setPipelineState(RenderPipelineState::TOMOGRAPHY_ZX); break;
                                    }
                                    return;
                                }
                            }
                        }
                        if (gHelpLink) {
                            int mx = (int)xpos, my = (int)ypos;
                            int flippedY = height - my;
                            if (gHelpLink->contains(mx, flippedY, height)) {
                                system("start https://github.com/automaton3d/automaton/blob/master/help.md");
                                return;
                            }
                        }
                        // Orbit
                        if (!thumb.active && isIn3DZone(xpos, ypos, gViewport[2], gViewport[3])) {
                            setLeftClicked(true);
                            setClickPoint(xpos, ypos);
                        }
                        // About dialog
                        if (showAboutDialog) {
                            int closeX = gViewport[2]/2 - 40, closeY = gViewport[3] - 180;
                            if (xpos >= closeX && xpos <= closeX+100 && ypos >= closeY-30 && ypos <= closeY+10)
                                showAboutDialog = false;
                        }
                        break;
                    }
                    case GLFW_MOUSE_BUTTON_MIDDLE:
                        if (isIn3DZone(xpos, ypos, gViewport[2], gViewport[3]) &&
                            !hslider.isDragging() && !vslider.isDragging()) {
                            setMiddleClicked(true);
                            setClickPoint(xpos, ypos);
                        }
                        break;
                    case GLFW_MOUSE_BUTTON_RIGHT:
                        if (isIn3DZone(xpos, ypos, gViewport[2], gViewport[3]) &&
                            !hslider.isDragging() && !vslider.isDragging()) {
                            setRightClicked(true);
                            setClickPoint(xpos, ypos);
                        }
                        break;
                }
                break;

            case GLFW_RELEASE:
                switch (button) {
                    case GLFW_MOUSE_BUTTON_LEFT: {
                        bool wasSlider = hslider.isDragging() || vslider.isDragging();
                        hslider.onMouseButton(button, action, xpos, ypos);
                        vslider.onMouseButton(button, action, xpos, ypos);
                        if (!wasSlider) setLeftClicked(false);
                        thumb.active = false;
                        thumb.dragging = false;
                        showDragCube = false;
                        break;
                    }
                    case GLFW_MOUSE_BUTTON_MIDDLE:
                        setMiddleClicked(false);
                        break;
                    case GLFW_MOUSE_BUTTON_RIGHT:
                        setRightClicked(false);
                        break;
                }
                break;
        }
    }

    // -----------------------------------------------------------------
    // Mouse move callback (gizmo drag, hover)
    // -----------------------------------------------------------------
    void moveCallback(GLFWwindow* window, double xpos, double ypos) {
        int width, height;
        glfwGetWindowSize(window, &width, &height);

        hslider.onMouseDrag((int)xpos, (int)ypos);
        hslider.onMouseMove((int)xpos, (int)ypos);
        if (hslider.isDragging() && tomoEnable && tomoEnable->getState()) {
            tomography::setSlicePosition(hslider.getValue());
            tomography::update();
        }

        if (gHelpLink) {
            int mx = (int)xpos, my = (int)ypos;
            int flippedY = height - my;
            helpHover = gHelpLink->contains(mx, flippedY, height);
        }

        if (!hslider.isDragging() && !vslider.isDragging() &&
            isIn3DZone(xpos, ypos, width, height)) {
            setClickPoint(xpos, ypos);
        }

        // Gizmo dragging
        if ((pause || currentMode == REPLAY) && thumb.active && thumb.dragging && showGizmo) {
            float mx = (float)xpos, my = (float)ypos;
            float dMx = mx - (float)thumb.startMouseX;
            float dMy = my - (float)thumb.startMouseY;
            float dAx = gGizmoProj.ex[thumb.axis] - gGizmoProj.cx;
            float dAy = gGizmoProj.ey[thumb.axis] - gGizmoProj.cy;
            float fullProjLength = std::hypot(dAx, dAy);
            if (fullProjLength > 1e-6f) {
                float projDistance = (dMx * dAx + dMy * dAy) / fullProjLength;
                float cellDelta = (projDistance / fullProjLength) * (float)automaton::EL;
                float newOffset = (float)thumb.startOffset[thumb.axis] + cellDelta;
                int newVisOffset = static_cast<int>(std::round(newOffset));
                if (thumb.axis == 0)      gConfig.view.vis_dx = newVisOffset;
                else if (thumb.axis == 1) gConfig.view.vis_dy = newVisOffset;
                else if (thumb.axis == 2) gConfig.view.vis_dz = newVisOffset;
            }
            showDragCube = true;
            dragCubeX = (float)xpos;
            dragCubeY = (float)ypos;
            gizmoHoverAxis = thumb.axis;
        } else {
            // Hover detection for gizmo
            if (showGizmo && (pause || currentMode == REPLAY)) {
                float mx = (float)xpos, my = (float)ypos;
                const float hoverRadius = 14.0f;
                int bestAxis = -1;
                float bestDist = hoverRadius;
                for (int axis = 0; axis < 3; ++axis) {
                    float dx = mx - gGizmoProj.ex[axis];
                    float dy = my - gGizmoProj.ey[axis];
                    float dist = std::hypot(dx, dy);
                    if (dist < bestDist) {
                        bestDist = dist;
                        bestAxis = axis;
                    }
                }
                gizmoHoverAxis = bestAxis;
                if (bestAxis != -1) {
                    showDragCube = true;
                    dragCubeX = gGizmoProj.ex[bestAxis];
                    dragCubeY = gGizmoProj.ey[bestAxis];
                } else {
                    showDragCube = false;
                }
            } else {
                gizmoHoverAxis = -1;
                showDragCube = false;
            }
        }

        // Remove resize cursor (no more <->)
        glfwSetCursor(window, nullptr);
    }

    // -----------------------------------------------------------------
    // Scroll callback (unified: perspective zoom vs ortho zoom)
    // -----------------------------------------------------------------
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    bool in3D = isIn3DZone(mouseX, mouseY, gViewport[2], gViewport[3]);

    if (!hslider.isDragging() && !vslider.isDragging() && in3D) {
        // Determine which projection is active
        bool usePerspective;
        if (projectRads.size() >= 2) {
            if (projectRads[0].isSelected())
                usePerspective = false;  // ortho
            else if (projectRads[1].isSelected())
                usePerspective = true;   // perspective
            else
                usePerspective = gConfig.projection.perspective;
        } else {
            usePerspective = gConfig.projection.perspective;
        }

        if (usePerspective) {
            // Retrieve the camera context
            AppContext* ctx = static_cast<AppContext*>(glfwGetWindowUserPointer(window));
            if (ctx) {
                ctx->camera.ProcessMouseScroll((float)yoffset);
            } else {
                // Fallback to setScrollDirection if no context
                setScrollDirection((xoffset + yoffset) > 0);
            }
        } else {
            // Orthographic zoom
            float delta = (yoffset > 0) ? 0.05f : -0.05f;
            gConfig.view.ortho_scale += delta;
            gConfig.view.ortho_scale = std::max(0.2f, std::min(6.0f, gConfig.view.ortho_scale));
            updateProjection();
        }
    }
}
    // -----------------------------------------------------------------
    // Keyboard callback
    // -----------------------------------------------------------------
    void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        static double lastKeyTime = 0.0;
        double now = glfwGetTime();
        if (now - lastKeyTime < 0.2) return;
        lastKeyTime = now;

        if (action == GLFW_PRESS) {
            switch (key) {
                case GLFW_KEY_ESCAPE: {
#ifdef _WIN32
                    int result = MessageBoxW(nullptr,
                        L"Do you really want to exit?",
                        L"Exit Confirmation",
                        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 | MB_SYSTEMMODAL);
                    if (result == IDYES)
                        glfwSetWindowShouldClose(window, GLFW_TRUE);
#else
                    showExitDialog = true;
#endif
                    break;
                }
                case GLFW_KEY_LEFT_CONTROL:
                case GLFW_KEY_RIGHT_CONTROL:
                    setSpeed(5.f);
                    break;
                case GLFW_KEY_LEFT_SHIFT:
                case GLFW_KEY_RIGHT_SHIFT:
                    setSpeed(0.1f);
                    break;
                case GLFW_KEY_T:
                    if (tomoEnable) {
                        tomoEnable->toggle();
                        if (!tomoEnable->getState())
                            setPipelineState(RenderPipelineState::FULL_VOLUME);
                        else {
                            for (auto& r : tomoDirs) r.setSelected(false);
                            if (!tomoDirs.empty()) {
                                tomoDirs[0].setSelected(true);
                                setPipelineState(RenderPipelineState::TOMOGRAPHY_XY);
                            }
                        }
                    }
                    break;
                case GLFW_KEY_M:
                    sound(true);
                    break;
                case GLFW_KEY_P:
                    pause = !pause;
                    showGizmo = !showGizmo;
                    if (!pause && currentMode == SIMULATION) {
                        gConfig.view.vis_dx = 0;
                        gConfig.view.vis_dy = 0;
                        gConfig.view.vis_dz = 0;
                    }
                    glfwPostEmptyEvent();
                    break;
                case GLFW_KEY_F1:
                    showHelp = !showHelp;
                    break;
                case GLFW_KEY_F8:
                    toggleFullscreen(window);
                    break;
                default:
                    break;
            }
        } else if (action == GLFW_RELEASE) {
            switch (key) {
                case GLFW_KEY_LEFT_CONTROL:
                case GLFW_KEY_RIGHT_CONTROL:
                case GLFW_KEY_LEFT_SHIFT:
                case GLFW_KEY_RIGHT_SHIFT:
                    setSpeed(1.f);
                    break;
                default:
                    break;
            }
        }
    }

    void errorCallback(int error, const char* description) {
        std::cerr << "[GLFW Error " << error << "] " << description << std::endl;
    }

} // namespace framework