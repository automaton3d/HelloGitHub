/*
 * globals.cpp
 */

#include <thread>
#include "globals.h"
#include "radio.h"
#include "tickbox.h"
#include "menubar.h"
#include "voxel.h"
#include "GUI.h"
#include "replay_progress.h"
#include "projection.h"
#include "hslider.h"
#include "text_renderer.h"
#include "app_context.h"
#include <iostream>

GLFWwindow* window = nullptr;

unsigned long long timer = 0;

unsigned tomo_x = 0;
unsigned tomo_y = 0;
unsigned tomo_z = 0;

// Splash state
int latticeSize = -1;
int numLayers = -1;
bool initPaused = false;

// Simulation state
std::vector<unsigned int> voxels;

// Shaders and uniforms
GLuint textProgram = 0;



GLuint colorProgram2D = 0;
GLint uProjLoc = -1;
GLint uColorLoc = -1;



GLuint textureProgram2D = 0;
GLint colorMvpLoc2D = -1;
GLint colorColorLoc2D = -1;

GLuint colorProgram3D = 0;
GLint  colorMvpLoc3D  = -1;
GLint  colorColorLoc3D = -1;

GLint textureMvpLoc = -1;
GLint textureSamplerLoc = -1;
TextRenderer* textRenderer = nullptr;

std::atomic<bool> gSimulationThreadRunning{false};
std::thread gSimThread;

// Viewport and matrices
GLint gViewport[4] = {0, 0, 0, 0};
glm::mat4 modelview = glm::mat4(1.0f);
glm::mat4 projection = glm::mat4(1.0f);

// Flags
std::atomic<bool> active{false};
std::atomic<bool> pause{false};
bool enable = true;

// UI elements
//std::vector<Radio> viewpoint;
//std::vector<Radio> projectionDirs;
std::vector<Radio> tomoDirs;
Tickbox* tomoEnable = nullptr;

Mode currentMode = SPLASH;

bool gAxisProjValid = false;
AxisProjection gAxisProj[3];

bool showGizmo = false;
GizmoProjection gGizmoProj = {};

std::mutex timerMutex;

VoxelMesh voxelMesh;

bool shouldExitApp = false;
bool gAppShouldExit = false;

int d3Dpos = 110;
int delaysPos = 390;
int viewsPos = 530;
int projPos = 680;
int tomoPos = 785;

Button* gHelpLink = nullptr;

bool isFullscreen = false;
int windowedWidth = 1920;
int windowedHeight = 1080;
int windowedPosX = 100;
int windowedPosY = 100;

namespace framework
{
	MenuBar* menuBar = nullptr;
	float vis_offset_x = 0;
	float vis_offset_y = 0;
	float vis_offset_z = 0;
	float axisLength = 0.375f;  // 0.5 * 0.75 to match the actual axis rendering
    std::vector<Radio> projectRads;
    std::vector<Radio> views;
    Tickbox* scenarioHelpToggle = nullptr;
    std::vector<Tickbox> delays;
    std::vector<Tickbox> data3D;
    VSlider vslider;

    AxisThumb thumb;  // Default initialization

    // Projection matrices used in camera/projection system
    glm::mat4 mProjection_ = glm::mat4(1.0f);
    glm::mat4 mOrtho       = glm::mat4(1.0f);
    glm::mat4 mPerspective = glm::mat4(1.0f);

    // Position tracking arrays
    std::vector<std::array<unsigned, 3>> lastPositions_;
    std::vector<std::array<float, 2>>    screenPositions_;
    std::map<std::pair<int,int>, int>    counts_;
    HSlider hslider;

}

bool tomographyNeedsUpdate = false;

std::vector<glm::vec3> makeCube(float cx, float cy, float cz, float size) {
    float h = size * 0.5f;
    std::vector<glm::vec3> cube;
    // 6 faces, each with 4 vertices
    cube.insert(cube.end(), {
        // Right face
        {cx+h, cy-h, cz-h}, {cx+h, cy+h, cz-h}, {cx+h, cy+h, cz+h}, {cx+h, cy-h, cz+h},
        // Left face
        {cx-h, cy-h, cz-h}, {cx-h, cy+h, cz-h}, {cx-h, cy+h, cz+h}, {cx-h, cy-h, cz+h},
        // Top face
        {cx-h, cy+h, cz-h}, {cx+h, cy+h, cz-h}, {cx+h, cy+h, cz+h}, {cx-h, cy+h, cz+h},
        // Bottom face
        {cx-h, cy-h, cz-h}, {cx+h, cy-h, cz-h}, {cx+h, cy-h, cz+h}, {cx-h, cy-h, cz+h},
        // Front face
        {cx-h, cy-h, cz+h}, {cx+h, cy-h, cz+h}, {cx+h, cy+h, cz+h}, {cx-h, cy+h, cz+h},
        // Back face
        {cx-h, cy-h, cz-h}, {cx+h, cy-h, cz-h}, {cx+h, cy+h, cz-h}, {cx-h, cy+h, cz-h}
    });
    return cube;
};

bool helpHover = false;
bool GPUEnabled = false;

namespace automaton
{
    bool convol_delay  = false;
    bool diffuse_delay = false;
    bool reloc_delay   = false;
}

#ifdef DEBUG
double debugClickX = -1;
double debugClickY = -1;
bool showDebugClick = false;
#endif
