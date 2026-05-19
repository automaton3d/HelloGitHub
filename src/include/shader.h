// shader.h - Unified Shader Compilation Header
#pragma once
#include <glad/glad.h>

// ============================================================================
// Shader Source Strings
// ============================================================================

// 3D Scene shaders
extern const char* vertexShaderSource;
extern const char* fragmentShaderSource;

// Text rendering shaders
extern const char* textVertexShaderSource;
extern const char* textFragmentShaderSource;

// 2D Color shaders
extern const char* colorVertexShaderSource;
extern const char* colorFragmentShaderSource;

// 2D Texture shaders
extern const char* textureVertexShaderSource;
extern const char* textureFragmentShaderSource;

// ============================================================================
// Shader Compilation Functions
// ============================================================================

// Generic shader compiler with error checking
unsigned int compileShader(const char* vertexSrc, const char* fragmentSrc);

// Convenience functions for specific shader types
unsigned int compileSceneShader();      // 3D scene rendering
unsigned int compileTextShader();       // Text rendering
unsigned int compileColorShader();      // 2D colored primitives
unsigned int compileTextureShader();    // 2D textured quads

// NEW: Shader for semi-transparent planes
unsigned int compileTransparentShader();
