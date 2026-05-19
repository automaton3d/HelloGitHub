// test_globals.h
#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "text_renderer.h"

// Minimal global variables required for the Dropdown/TextRenderer test
// (These replace the externs from globals.h for this specific test)

extern GLFWwindow* window;
extern GLuint textProgram;
extern TextRenderer* textRenderer;

extern int gViewport[4];

// Control variable (optional, but good to have)
extern bool gAppShouldExit;