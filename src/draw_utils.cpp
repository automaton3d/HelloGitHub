#include "draw_utils.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <sstream>

#include "render_pipeline.h"
#include "Renderer2D.h"
#include "projection_manager.h"

// External shader (should already exist in the project)
extern GLint uProjLoc;
extern GLint uColorLoc;

static GLuint vao = 0;
static GLuint vbo = 0;

static void init()
{
    if (vao) return;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          sizeof(glm::vec2), (void*)0);

    glBindVertexArray(0);
}

static void upload(const std::vector<glm::vec2>& verts)
{
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 verts.size() * sizeof(glm::vec2),
                 verts.data(),
                 GL_DYNAMIC_DRAW);
}

// =========================
// TRIANGLE FAN
// =========================
void drawTriangleFan2D(
    const std::vector<glm::vec2>& verts,
    const glm::vec3& color,
    const glm::mat4& proj)
{
    if (verts.empty()) return;

    init();

    Renderer2D::use();
    Renderer2D::setMVP(
        ProjectionManager::instance().get2DOrtho()
    );    
    glUniform3fv(uColorLoc, 1, &color[0]);

    glBindVertexArray(vao);
    upload(verts);

    glDrawArrays(GL_TRIANGLE_FAN, 0, (GLsizei)verts.size());

    glBindVertexArray(0);
}

// =========================
// LINE LOOP
// =========================
void drawLineLoop2D(
    const std::vector<glm::vec2>& pts,
    const glm::vec3& color,
    const glm::mat4& proj,
    float thickness)
{
    if (pts.empty()) return;
    init();
    glLineWidth(thickness);
    glUseProgram(0);
    glBindVertexArray(vao);
    upload(pts);
    glDrawArrays(GL_LINE_LOOP, 0, (GLsizei)pts.size());
    glBindVertexArray(0);
}

void drawLine2D_new(
    float x1, float y1,
    float x2, float y2,
    const glm::vec3& c1,
    const glm::vec3& c2,
    const glm::mat4& mvp)
{
    (void)c2;

    struct V {
        glm::vec3 pos;
    };

    V data[2] = {
        {{x1, y1, 0.0f}},
        {{x2, y2, 0.0f}}
    };

    Renderer2D::use();
    Renderer2D::setMVP(mvp);

    // uses only ONE uniform color
    Renderer2D::setColor(c1);

    GLuint vao, vbo;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(V),
        (void*)0
    );

    glEnableVertexAttribArray(0);

    glDrawArrays(GL_LINES, 0, 2);

    glBindVertexArray(0);

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}

// Note: x1,y1 is the top-left corner and x2,y2 the bottom-right
void drawQuad2D(float x1, float y1, float x2, float y2, const glm::vec3& color, const glm::mat4& projection) {
    init();
    std::vector<glm::vec2> verts = { {x1, y1}, {x2, y1}, {x1, y2}, {x2, y2} };
    Renderer2D::use();
    Renderer2D::setMVP(ProjectionManager::instance().get2DOrtho());
    Renderer2D::setColor(color);      // <-- using the header method
    glBindVertexArray(vao);
    upload(verts);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

std::vector<std::string> wrapText(const std::string& text, size_t width) {
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string word;
    std::string currentLine;

    while (ss >> word) {
        if (currentLine.length() + word.length() + 1 <= width) {
            if (!currentLine.empty()) currentLine += " ";
            currentLine += word;
        } else {
            lines.push_back(currentLine);
            currentLine = word;
        }
    }
    if (!currentLine.empty()) lines.push_back(currentLine);
    return lines;
}