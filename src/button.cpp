// button.cpp
#include "button.h"

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "globals.h"
#include "projection_manager.h"
#include "Renderer2D.h"

#include <iostream>

Button::Button(float x, float y, float w, float h,
               const std::string& label,
               bool isDefault)
    : x_(x),
      y_(y),
      w_(w),
      h_(h),
      label_(label),
      isDefault_(isDefault),
      shadowVAO(0),
      shadowVBO(0),
      bgVAO(0),
      bgVBO(0),
      borderVAO(0),
      borderVBO(0),
      underlineVAO(0),
      underlineVBO(0),
      underlineInitialized(false)
{
    setupGeometry();
}

Button::~Button()
{
    cleanup();
}

void Button::setupGeometry()
{
    // =========================================================
    // Shadow geometry
    // =========================================================
    float shadowVertices[] = {
        x_ + 2.0f,      y_ - 2.0f,
        x_ + w_ + 2.0f, y_ - 2.0f,
        x_ + w_ + 2.0f, y_ + h_ - 2.0f,
        x_ + 2.0f,      y_ + h_ - 2.0f
    };

    glGenVertexArrays(1, &shadowVAO);
    glGenBuffers(1, &shadowVBO);

    glBindVertexArray(shadowVAO);
    glBindBuffer(GL_ARRAY_BUFFER, shadowVBO);

    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(shadowVertices),
                 shadowVertices,
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          2 * sizeof(float),
                          (void*)0);

    glEnableVertexAttribArray(0);

    // =========================================================
    // Background geometry
    // =========================================================
    float bgVertices[] = {
        x_,      y_,
        x_ + w_, y_,
        x_ + w_, y_ + h_,
        x_,      y_ + h_
    };

    glGenVertexArrays(1, &bgVAO);
    glGenBuffers(1, &bgVBO);

    glBindVertexArray(bgVAO);
    glBindBuffer(GL_ARRAY_BUFFER, bgVBO);

    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(bgVertices),
                 bgVertices,
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          2 * sizeof(float),
                          (void*)0);

    glEnableVertexAttribArray(0);

    // =========================================================
    // Border geometry
    // =========================================================
    glGenVertexArrays(1, &borderVAO);
    glGenBuffers(1, &borderVBO);

    glBindVertexArray(borderVAO);
    glBindBuffer(GL_ARRAY_BUFFER, borderVBO);

    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(bgVertices),
                 bgVertices,
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          2 * sizeof(float),
                          (void*)0);

    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Button::cleanup()
{
    if (shadowVAO) glDeleteVertexArrays(1, &shadowVAO);
    if (shadowVBO) glDeleteBuffers(1, &shadowVBO);

    if (bgVAO) glDeleteVertexArrays(1, &bgVAO);
    if (bgVBO) glDeleteBuffers(1, &bgVBO);

    if (borderVAO) glDeleteVertexArrays(1, &borderVAO);
    if (borderVBO) glDeleteBuffers(1, &borderVBO);

    if (underlineVAO) glDeleteVertexArrays(1, &underlineVAO);
    if (underlineVBO) glDeleteBuffers(1, &underlineVBO);
}

bool Button::contains(int mouseX,
                      int mouseY,
                      int screenHeight) const
{
    const float padding = 6.0f;

    float drawnY    = screenHeight - (y_ + h_);
    float drawnYTop = screenHeight - y_;

    return mouseX >= (x_ - padding) &&
           mouseX <= (x_ + w_ + padding) &&
           mouseY >= (drawnY - padding) &&
           mouseY <= (drawnYTop + padding);
}

void Button::draw(TextRenderer& renderer,
                  int screenWidth,
                  int screenHeight)
{
    glDisable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,
                GL_ONE_MINUS_SRC_ALPHA);

    // =========================================================
    // Activate Renderer2D
    // =========================================================
    Renderer2D::use();

    Renderer2D::setMVP(
        ProjectionManager::instance().get2DOrtho()
    );

    // =========================================================
    // Shadow
    // =========================================================
    Renderer2D::setColor(
        glm::vec4(0.0f,
                  0.0f,
                  0.0f,
                  0.35f)
    );

    glBindVertexArray(shadowVAO);

    glDrawArrays(GL_TRIANGLE_FAN,
                 0,
                 4);

    // =========================================================
    // Background
    // =========================================================
    Renderer2D::setColor(
        glm::vec4(0.2f,
                  0.4f,
                  0.8f,
                  1.0f)
    );

    glBindVertexArray(bgVAO);

    glDrawArrays(GL_TRIANGLE_FAN,
                 0,
                 4);

    // =========================================================
    // Border
    // =========================================================
    Renderer2D::setColor(
        glm::vec4(1.0f,
                  1.0f,
                  1.0f,
                  1.0f)
    );

    glLineWidth(2.0f);

    glBindVertexArray(borderVAO);

    glDrawArrays(GL_LINE_LOOP,
                 0,
                 4);

    glLineWidth(1.0f);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // =========================================================
    // Text
    // =========================================================
    float textScale = 0.35f;

float textWidth =
    renderer.measureTextWidth(label_,
                              textScale);

// Approximate visual font height
float textHeight = 24.0f * textScale;

float textX =
    x_ + (w_ - textWidth) * 0.5f;

// Actual vertical centering
float textY =
    screenHeight -
    (y_ + (h_ + textHeight) * 0.5f);
    
    renderer.RenderText(label_,
                        textX,
                        textY,
                        textScale,
                        glm::vec3(1.0f),
                        screenWidth,
                        screenHeight);
}

void Button::drawAsHyperlink(TextRenderer& renderer,
                             bool hovered,
                             int screenWidth,
                             int screenHeight)
{
    glDisable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,
                GL_ONE_MINUS_SRC_ALPHA);

    glm::vec3 color =
        hovered
        ? glm::vec3(0.1f, 0.6f, 1.0f)
        : glm::vec3(0.0f, 0.3f, 1.0f);

    float textScale = 0.35f;

    float textWidth =
        renderer.measureTextWidth(label_,
                                  textScale);

    float tx =
        x_ + (w_ - textWidth) * 0.5f;

    float ty =
        screenHeight -
        (y_ + h_ * 0.5f) -
        4.0f;

    // =========================================================
    // Draw text
    // =========================================================
    renderer.RenderText(label_,
                        tx,
                        ty,
                        textScale,
                        color,
                        screenWidth,
                        screenHeight);

    // =========================================================
    // Underline geometry
    // =========================================================
    float underlineY = ty - 2.0f;

    float underlineVertices[] = {
        tx,             underlineY,
        tx + textWidth, underlineY
    };

    if (!underlineVAO)
    {
        glGenVertexArrays(1, &underlineVAO);
        glGenBuffers(1, &underlineVBO);
    }

    glBindVertexArray(underlineVAO);

    glBindBuffer(GL_ARRAY_BUFFER,
                 underlineVBO);

    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(underlineVertices),
                 underlineVertices,
                 GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          2 * sizeof(float),
                          (void*)0);

    glEnableVertexAttribArray(0);

    // =========================================================
    // Draw underline
    // =========================================================
    Renderer2D::use();

    Renderer2D::setMVP(
        glm::ortho(0.0f,
                   (float)screenWidth,
                   0.0f,
                   (float)screenHeight)
    );

    Renderer2D::setColor(
        glm::vec4(color,
                  1.0f)
    );

    glLineWidth(1.5f);

    glDrawArrays(GL_LINES,
                 0,
                 2);

    glLineWidth(1.0f);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Button::setPosition(float x, float y)
{
    x_ = x;
    y_ = y;

    cleanup();
    setupGeometry();
}

void Button::setSize(float w, float h)
{
    w_ = w;
    h_ = h;

    cleanup();
    setupGeometry();
}