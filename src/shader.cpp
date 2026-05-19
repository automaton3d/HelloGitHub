// shader.cpp - Unified Shader Compilation Implementation
#include "shader.h"
#include <glad/glad.h>
#include <iostream>
#include <vector>
#include <cassert>

// ============================================================================
// NEW VERT SHADER DEBUG
// ============================================================================

const char* shader2DVertex = R"(
#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

uniform mat4 uMVP;

out vec3 vColor;

void main()
{
    vColor = aColor;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

// ============================================================================
// NEW FRAG SHADER DEBUG
// ============================================================================

const char* shader2DFragment = R"(
#version 330 core

in vec3 vColor;
out vec4 FragColor;

void main()
{
    FragColor = vec4(vColor, 1.0);
}
)";

// ============================================================================
// 3D Scene Shaders
// ============================================================================
const char* vertexShaderSource = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec3 fragColor;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    fragColor = aColor;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
in vec3 fragColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(fragColor, 1.0);
}
)";

// ============================================================================
// Text Rendering Shaders
// ============================================================================
const char* textVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec4 vertex;
out vec2 TexCoords;
uniform mat4 projection;
void main() {
    gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
    TexCoords = vertex.zw;
}
)";

const char* textFragmentShaderSource = R"(
#version 330 core
in vec2 TexCoords;
out vec4 FragColor;
uniform sampler2D text;
uniform vec3 textColor;
void main() {
    float alpha = texture(text, TexCoords).a;
    FragColor = vec4(textColor, alpha);
}
)";

// ============================================================================
// 2D Color Shaders (for primitives like lines, quads)
// ============================================================================
const char* colorVertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

const char* colorFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;
void main() {
    FragColor = vec4(uColor, 1.0);
}
)";

// ============================================================================
// OIT (Order Independent Transparency) - can be kept even if unused
// ============================================================================
const char* oitFragmentShader = R"(
#version 330 core
out vec4 accum;
layout(location = 1) out float reveal;
uniform vec3 uColor;
uniform float uAlpha;
void main() {
    vec4 color = vec4(uColor, uAlpha);
    float weight = max(0.01, pow(color.a + 0.01, 4.0));
    accum = vec4(color.rgb * color.a, color.a) * weight;
    reveal = color.a;
}
)";

// ============================================================================
// 2D Texture Shaders
// ============================================================================
const char* textureVertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
uniform mat4 uMVP;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
}
)";

const char* textureFragmentShaderSource = R"(
#version 330 core
in vec2 vUV;
uniform sampler2D uTexture;
out vec4 FragColor;
void main() {
    FragColor = texture(uTexture, vUV);
}
)";

// ============================================================================
// Transparent Plane Shader (single definition, no duplicate)
// ============================================================================
const char* transparentVertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

const char* transparentFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;
uniform float uAlpha;
void main() {
    FragColor = vec4(uColor, uAlpha);
}
)";

// ============================================================================
// Internal Helper Functions
// ============================================================================
static bool checkShaderCompileStatus(unsigned int shader, const char* type) {
    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        int logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> infoLog(logLength > 1 ? logLength : 1);
        glGetShaderInfoLog(shader, logLength, nullptr, infoLog.data());
        std::cerr << "ERROR: " << type << " shader compilation failed:\n"
                  << infoLog.data() << '\n';
        return false;
    }
    return true;
}

static bool checkProgramLinkStatus(unsigned int program) {
    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        int logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> infoLog(logLength > 1 ? logLength : 1);
        glGetProgramInfoLog(program, logLength, nullptr, infoLog.data());
        std::cerr << "ERROR: Shader program linking failed:\n"
                  << infoLog.data() << '\n';
        return false;
    }
    return true;
}

// ============================================================================
// Generic Shader Compiler
// ============================================================================
unsigned int compileShader(const char* vs, const char* fs) {
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vertexShader, 1, &vs, nullptr);
    glCompileShader(vertexShader);
    if (!checkShaderCompileStatus(vertexShader, "VERTEX")) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return 0;
    }

    glShaderSource(fragmentShader, 1, &fs, nullptr);
    glCompileShader(fragmentShader);
    if (!checkShaderCompileStatus(fragmentShader, "FRAGMENT")) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return 0;
    }

    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    if (!checkProgramLinkStatus(program)) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(program);
        return 0;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return program;
}

// ============================================================================
// Convenience Functions
// ============================================================================
unsigned int compileSceneShader() {
    return compileShader(vertexShaderSource, fragmentShaderSource);
}

unsigned int compileTextShader() {
    return compileShader(textVertexShaderSource, textFragmentShaderSource);
}

unsigned int compileColorShader() {
    return compileShader(colorVertexShaderSource, colorFragmentShaderSource);
}

unsigned int compileTextureShader() {
    return compileShader(textureVertexShaderSource, textureFragmentShaderSource);
}

unsigned int compileOITShader() {
    return compileShader(colorVertexShaderSource, oitFragmentShader);
}

unsigned int compileTransparentShader() {
    return compileShader(transparentVertexShaderSource, transparentFragmentShaderSource);
}

// ============================================================================
// NEW COMPILER DEBUG
// ============================================================================

unsigned int compileShader2D()
{
    return compileShader(shader2DVertex, shader2DFragment);
}