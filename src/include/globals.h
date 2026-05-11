// globals.h
#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <atomic>
#include <vector>
#include <array>
#include <thread>
#include <mutex>
#include <glm/glm.hpp>
#include "radio.h"
#include "menubar.h"
#include "config.h"

#define RAD_SEP 25

#define DEBUG

class TextRenderer;
struct AxisProjection;

// ============================================================================
// Declarações externas (extern) — DEFINIDAS em globals.cpp
// ============================================================================

class Button;
class Radio;
class Tickbox;

extern unsigned tomo_x;
extern unsigned tomo_y;
extern unsigned tomo_z;

extern std::vector<uint32_t> fullVolumeVoxels;
extern std::vector<uint32_t> sliceVoxels;

extern std::vector<Radio> tomoDirs;

extern GLFWwindow* window;

extern GLuint textProgram;
extern GLuint colorProgram2D;
extern GLuint textureProgram2D;
extern GLuint colorProgram3D;

extern GLint colorMvpLoc2D;
extern GLint colorColorLoc2D;
extern GLint textureMvpLoc;
extern GLint textureSamplerLoc;

extern GLuint colorProgram3D;
extern GLint  colorMvpLoc3D;
extern GLint  colorColorLoc3D;

extern TextRenderer* textRenderer;

extern int gViewport[4];

// Estado da aplicação
extern unsigned long long timer;
extern int latticeSize;
extern int numLayers;
extern bool initPaused;

// Flags de controle
extern std::atomic<bool> gSimulationThreadRunning;
extern std::thread gSimThread;

enum Mode {
    SPLASH,
    SIMULATION,
    STATISTICS,
    REPLAY
};

// Variável global que guarda o modo atual
extern Mode currentMode;

// Para a thread de simulação
extern std::atomic<bool> pause;
extern std::atomic<bool> active;

// Buffers
extern std::vector<unsigned int> voxels;

// Projeção e vista
extern glm::mat4 modelview;
extern glm::mat4 projection;

// Mutexes
extern std::mutex timerMutex;
extern std::mutex gVoxelBufferMutex;

// Outros
extern bool gAppShouldExit;

extern int d3Dpos;
extern int delaysPos;
extern int viewsPos;
extern int projPos;
extern int tomoPos;

extern Tickbox* tomoEnable;

// globals.h or GUI.h
extern Button* gHelpLink;

// globals.h
extern bool tomographyNeedsUpdate;

std::vector<glm::vec3> makeCube(float cx, float cy, float cz, float size);

// Axis projection for thumb dragging
struct AxisProjection {
    float x0, y0;  // Start point in screen coordinates
    float x1, y1;  // End point in screen coordinates
};

extern AxisProjection gAxisProj[3];
extern bool gAxisProjValid;


extern bool isFullscreen;
extern int windowedWidth;
extern int windowedHeight;
extern int windowedPosX;
extern int windowedPosY;


namespace framework {
    struct AxisThumb {
        bool active = false;
        bool dragging = false;
        int startMouseX = 0;
        int startMouseY = 0;

        int axis = -1;               // 0=X, 1=Y, 2=Z
        float position = 0.0f;       // Current position along axis
        float initialPosition = 0.0f;
        int startOffset[3] = {0, 0, 0};  // Starting vis_dx, vis_dy, vis_dz
        float startOffsetF[3] = {0.0f, 0.0f, 0.0f};
    };

    extern AxisThumb thumb;
    extern float axisLength;
    extern float vis_offset_x;
    extern float vis_offset_y;
    extern float vis_offset_z;
    extern MenuBar* menuBar;

}

extern bool helpHover;

extern AxisProjection gAxisProj[3];
extern bool gAxisProjValid;
extern bool helpHover;

// Gizmo (miniature axis widget in top-right corner)
extern bool showGizmo;

struct GizmoProjection {
    float cx, cy;           // center of gizmo in screen (GLFW) coordinates
    float ex[3], ey[3];     // endpoint of each axis in screen coords
    float radius;           // gizmo radius in pixels
};
extern GizmoProjection gGizmoProj;

namespace framework {
    extern float axisLength;
}

extern double debugClickX;
extern double debugClickY;
extern bool showDebugClick;
extern bool GPUEnabled;

extern GLuint transparentProgram;   // programa para o plano transparente
extern GLint transparentMvpLoc, transparentColorLoc, transparentAlphaLoc;

namespace automaton
{
    extern bool convol_delay;
    extern bool diffuse_delay;
    extern bool reloc_delay;
}