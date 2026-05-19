// scene.cpp

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdexcept>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "GUI.h"
#include "scene.h"
#include "shader.h"
#include "app_context.h"
#include "globals.h"
#include "model/simulation.h"
#include "tomography.h"
#include "GUI3D.h"

using namespace SceneConstants;

// ============================================================
// Globals (transparent shader - kept for compatibility)
// ============================================================

GLuint transparentProgram = 0;
GLint transparentMvpLoc = -1;
GLint transparentColorLoc = -1;
GLint transparentAlphaLoc = -1;

// ============================================================
// Init
// ============================================================

void initScene(AppContext& ctx)
{
    // Initialize tomography (compiles internal shader)
    tomography::init();

    // Main shader
    ctx.shader = compileShader(vertexShaderSource, fragmentShaderSource);
    if (ctx.shader == 0) {
        throw std::runtime_error("Failed to compile scene shaders");
    }
}

// ============================================================
// Render
// ============================================================

void renderScene(AppContext& ctx)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    if (ctx.shader != 0)
    {
        glUseProgram(ctx.shader);

        // ====================================================
        // MATRICES
        // ====================================================

        glm::mat4 model = glm::mat4(1.0f);

        // IMPORTANT: get current projection (already updated via config + GUI)
        extern glm::mat4 framework::mProjection_;

        // View matrix (replace with your global camera if available)
        glm::mat4 view = glm::mat4(1.0f);

        // ====================================================
        // Uniforms
        // ====================================================

        GLint modelLoc = glGetUniformLocation(ctx.shader, "model");
        if (modelLoc != -1)
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        GLint viewLoc = glGetUniformLocation(ctx.shader, "view");
        if (viewLoc != -1)
            glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

        GLint projLoc = glGetUniformLocation(ctx.shader, "projection");
        if (projLoc != -1)
            glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(framework::mProjection_));

        // ====================================================
        // Render 3D
        // ====================================================

        framework::render3DObjects();
    }

    // ========================================================
    // Tomografia
    // ========================================================

    tomography::renderTomoPlane();

    // ========================================================
    // Restore state for GUI
    // ========================================================

    glEnable(GL_BLEND);

    if (ctx.shader != 0)
        glUseProgram(ctx.shader);
}

// ============================================================
// Cleanup
// ============================================================

void cleanupScene(AppContext& ctx)
{
    if (ctx.shader != 0) {
        glDeleteProgram(ctx.shader);
        ctx.shader = 0;
    }
}