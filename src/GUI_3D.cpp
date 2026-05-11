/*
 * GUI_3D.cpp
 */

#include "GUI.h"
#include "model/simulation.h"
#include "color_utils.h"
#include "layers.h"
#include "text_renderer.h"
#include "dropdown.h"
#include "globals.h"
#include "projection.h"
#include "tomography.h"
#include "app_context.h"
#include "render_pipeline.h"
#include "draw_utils.h"
#include "projection_manager.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <map>
#include <iostream>
#include <cmath>
#include <mutex>

extern AppContext ctx;
extern std::mutex gVoxelBufferMutex;

namespace automaton {
  extern unsigned EL;
  extern unsigned L2;
  extern unsigned W_USED;
  extern unsigned CENTER;
  extern std::vector<std::array<unsigned,3>> lcenters;
  extern std::vector<Cell> lattice_curr;
}

// From core (shared color shader)
extern GLuint colorProgram3D;
extern GLint colorMvpLoc3D, colorColorLoc3D;
extern Mode currentMode;

namespace framework {
  extern TextRenderer hudText;
  extern std::unique_ptr<LayerList> layerList;
  extern std::vector<Tickbox> data3D;

  using namespace automaton;

  inline int mod(int a, int b) {
      return ((a % b) + b) % b;
  }



  // ---------------------------------------------------------------------
  // Helpers: shader-based primitive drawing
  // ---------------------------------------------------------------------
  static void drawPoints(const std::vector<glm::vec3>& pts,
                         const glm::vec3& color,
                         const glm::mat4& mvp,
                         float size = 2.0f)
  {
    if (pts.empty()) return;
    glUseProgram(colorProgram3D);
    glUniformMatrix4fv(colorMvpLoc3D, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform3f(colorColorLoc3D, color.r, color.g, color.b);

    GLuint vao=0, vbo=0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, pts.size()*sizeof(glm::vec3), pts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glPointSize(size);
    glDrawArrays(GL_POINTS, 0, (GLsizei)pts.size());
    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
  }

  static void drawLines(const std::vector<glm::vec3>& verts,
                        const glm::vec3& color,
                        const glm::mat4& mvp,
                        float width = 1.0f)
  {
    if (verts.empty()) return;
    glUseProgram(colorProgram3D);
    glUniformMatrix4fv(colorMvpLoc3D, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform3f(colorColorLoc3D, color.r, color.g, color.b);

    GLuint vao=0, vbo=0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(glm::vec3), verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glLineWidth(width);
    glDrawArrays(GL_LINES, 0, (GLsizei)verts.size());
    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
  }

  void drawQuads(const std::vector<glm::vec3>& verts,
                        const glm::vec3& color,
                        const glm::mat4& mvp)
  {
    if (verts.empty()) return;
    glUseProgram(colorProgram3D);
    glUniformMatrix4fv(colorMvpLoc3D, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform3f(colorColorLoc3D, color.r, color.g, color.b);

    GLuint vao=0, vbo=0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(glm::vec3), verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    // Each face provided as 4 vertices → draw with TRIANGLE_FAN per face
    // Here we assume verts.size() is multiple of 4 and faces are contiguous
    for (size_t i = 0; i + 3 < verts.size(); i += 4) {
      glDrawArrays(GL_TRIANGLE_FAN, (GLint)i, 4);
    }
    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
  }

  void renderMomentum(AppContext& ctx)
  {
    const float GRID_SIZE = 0.5f / EL;
    const int CENTER_INT = EL / 2;
    std::vector<glm::vec3> pts;

    for (unsigned x=0;x<EL;x++)
      for (unsigned y=0;y<EL;y++)
        for (unsigned z=0;z<EL;z++) {
          int wx=(x+gConfig.view.vis_dx+EL)%EL;
          int wy=(y+gConfig.view.vis_dy+EL)%EL;
          int wz=(z+gConfig.view.vis_dz+EL)%EL;
          if (getCell(lattice_curr,wx,wy,wz,layerList->getSelected()).pB) {
            float px,py,pz;
            if (currentMode==REPLAY) {
              auto& c=lcenters[layerList->getSelected()];
              px=(int)(x-c[0])*GRID_SIZE;
              py=(int)(y-c[1])*GRID_SIZE;
              pz=(int)(z-c[2])*GRID_SIZE;
            } else {
              px=(int)(x-CENTER_INT)*GRID_SIZE;
              py=(int)(y-CENTER_INT)*GRID_SIZE;
              pz=(int)(z-CENTER_INT)*GRID_SIZE;
            }
            pts.emplace_back(px,py,pz);
          }
        }

    glm::mat4 view = ctx.camera.GetViewMatrix();
    glm::mat4 projection = framework::mProjection_;
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 mvp = projection * view * model;

    drawPoints(pts, glm::vec3(1.0f,1.0f,0.0f), mvp, 4.0f);
  }

  void renderSpin()
  {
    const float GRID_SIZE=0.5f/EL;
    const int CENTER_INT=EL/2;
    std::vector<glm::vec3> pts;

    for (unsigned x=0;x<EL;x++)
      for (unsigned y=0;y<EL;y++)
        for (unsigned z=0;z<EL;z++) {
          int wx=(x+gConfig.view.vis_dx+EL)%EL;
          int wy=(y+gConfig.view.vis_dy+EL)%EL;
          int wz=(z+gConfig.view.vis_dz+EL)%EL;
          if (getCell(lattice_curr,wx,wy,wz,layerList->getSelected()).sB) {
            float px=(int)(x-CENTER_INT)*GRID_SIZE;
            float py=(int)(y-CENTER_INT)*GRID_SIZE;
            float pz=(int)(z-CENTER_INT)*GRID_SIZE;
            pts.emplace_back(px,py,pz);
          }
        }

    glm::mat4 view = ctx.camera.GetViewMatrix();
    glm::mat4 projection = framework::mProjection_;
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 mvp = projection * view * model;



    drawPoints(pts, glm::vec3(0.0f,1.0f,1.0f), mvp, 4.0f);
  }

  void renderSineMask()
  {
      const float GRID_SIZE=0.5f/EL;
      const int CENTER_INT=EL/2;
      std::vector<glm::vec3> pts;

      for (unsigned x=0;x<EL;x++)
        for (unsigned y=0;y<EL;y++)
          for (unsigned z=0;z<EL;z++) {
            // Use tomography::isVoxelVisible instead of framework::isVoxelVisible
            if (tomoEnable && tomoEnable->getState() && !tomography::isVoxelVisible(x,y,z)) continue;

            int wx=(x+gConfig.view.vis_dx+EL)%EL;
            int wy=(y+gConfig.view.vis_dy+EL)%EL;
            int wz=(z+gConfig.view.vis_dz+EL)%EL;
            if (getCell(lattice_curr,wx,wy,wz,layerList->getSelected()).phiB) {
              float px=(int)(x-CENTER_INT)*GRID_SIZE;
              float py=(int)(y-CENTER_INT)*GRID_SIZE;
              float pz=(int)(z-CENTER_INT)*GRID_SIZE;
              pts.emplace_back(px,py,pz);
            }
          }

      glm::mat4 view = ctx.camera.GetViewMatrix();
      glm::mat4 projection = framework::mProjection_;
      glm::mat4 model = glm::mat4(1.0f);
      glm::mat4 mvp = projection * view * model;

      drawPoints(pts, glm::vec3(1.0f,1.0f,0.0f), mvp, 1.7f);
  }

  void renderHunting()
  {
    const float GRID_SIZE=0.5f/EL;
    const int CENTER_INT=EL/2;
    std::vector<glm::vec3> pts;

    for (unsigned x=0;x<EL;x++)
      for (unsigned y=0;y<EL;y++)
        for (unsigned z=0;z<EL;z++) {
          int wx=(x+gConfig.view.vis_dx+EL)%EL;
          int wy=(y+gConfig.view.vis_dy+EL)%EL;
          int wz=(z+gConfig.view.vis_dz+EL)%EL;
          if (getCell(lattice_curr,wx,wy,wz,layerList->getSelected()).pB) {
            float px=(int)(x-CENTER_INT)*GRID_SIZE;
            float py=(int)(y-CENTER_INT)*GRID_SIZE;
            float pz=(int)(z-CENTER_INT)*GRID_SIZE;
            pts.emplace_back(px,py,pz);
          }
        }

    glm::mat4 view = ctx.camera.GetViewMatrix();
    glm::mat4 projection = framework::mProjection_;
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 mvp = projection * view * model;


    drawPoints(pts, glm::vec3(1.0f,1.0f,0.0f), mvp, 5.0f);
  }

void renderWavefront()
{
    // NOTE: do NOT lock gVoxelBufferMutex here — this function is called
    // from render3DObjects() → renderScene() → renderFrame(), which already
    // holds the lock. Re-locking a non-recursive std::mutex is UB (deadlock).

    if (EL == 0)
        return;

    if (voxels.empty())
        return;

    // ============================================================
    // VOLUME PATH (with optional tomography filtering)
    // ============================================================

    const bool tomoActive = (tomoEnable && tomoEnable->getState());

    const float CELL_SPACING =
        0.5f / static_cast<float>(EL);

    const float VOXEL_SIZE =
        CELL_SPACING / 4.0f;

    const int CENTER_INT =
        EL / 2;

    glm::mat4 view =
        ctx.camera.GetViewMatrix();

    glm::mat4 projection =
        framework::mProjection_;

    glm::mat4 model(1.0f);

    glm::mat4 mvp =
        projection * view * model;

    // Group voxels by color so each scenario renders with correct colors
    std::map<uint32_t, std::vector<glm::vec3>> colorBuckets;

    for (unsigned x = 0; x < EL; ++x)
    for (unsigned y = 0; y < EL; ++y)
    for (unsigned z = 0; z < EL; ++z)
    {
        if (tomoActive && !tomography::isVoxelVisible(x, y, z))
            continue;

        size_t idx =
            static_cast<size_t>(x) * automaton::L2 +
            static_cast<size_t>(y) * EL + z;

        if (idx >= voxels.size())
            continue;

        uint32_t color = voxels[idx];
        if (color == 0)
            continue;

        float px = (mod(static_cast<int>(x) + gConfig.view.vis_dx, EL) - CENTER_INT) * CELL_SPACING;
        float py = (mod(static_cast<int>(y) + gConfig.view.vis_dy, EL) - CENTER_INT) * CELL_SPACING;
        float pz = (mod(static_cast<int>(z) + gConfig.view.vis_dz, EL) - CENTER_INT) * CELL_SPACING;

        auto cube = makeCube(px, py, pz, VOXEL_SIZE);
        colorBuckets[color].insert(
            colorBuckets[color].end(), cube.begin(), cube.end());
    }

    // Format: (a << 24) | (r << 16) | (g << 8) | b
    for (auto& [packed, faces] : colorBuckets)
    {
        float r = static_cast<float>((packed >> 16) & 0xFF) / 255.0f;
        float g = static_cast<float>((packed >>  8) & 0xFF) / 255.0f;
        float b = static_cast<float>( packed        & 0xFF) / 255.0f;
        drawQuads(faces, glm::vec3(r, g, b), mvp);
    }
}

  void renderCenters()
  {
    screenPositions_.clear();

    const float GRID_SIZE = 0.5f / EL;  // Changed from 1.0f / EL
    const int CENTER_INT = EL / 2;
    std::vector<glm::vec3> pts;
    std::vector<glm::vec3> colors;

    for (unsigned w = 0; w < W_USED; w++)
    {
      const auto& center = automaton::lcenters[w];
      unsigned cx = center[0];
      unsigned cy = center[1];
      unsigned cz = center[2];
      Cell &cell = getCell(lattice_curr, cx, cy, cz, w);

      float px = ((int)(((int)cell.x[0] + gConfig.view.vis_dx) % EL) - CENTER_INT) * GRID_SIZE;
      float py = ((int)(((int)cell.x[1] + gConfig.view.vis_dy) % EL) - CENTER_INT) * GRID_SIZE;
      float pz = ((int)(((int)cell.x[2] + gConfig.view.vis_dz) % EL) - CENTER_INT) * GRID_SIZE;

      float r = 0.7f + (w & 1) * 0.3f;
      float g = 0.7f + ((w >> 1) & 1) * 0.3f;
      float b = 0.7f + ((w >> 2) & 1) * 0.3f;

      pts.emplace_back(px,py,pz);
      colors.emplace_back(r,g,b);
    }

    // Draw centers per color
    glm::mat4 view = ctx.camera.GetViewMatrix();
    glm::mat4 projection = framework::mProjection_;
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 mvp = projection * view * model;



    for (size_t i=0;i<pts.size();++i) {
      drawPoints(std::vector<glm::vec3>{pts[i]}, colors[i], mvp, 8.0f);
    }
  }

  /**
   * Renders the cartesian positive axes with labels and thumb indicator.
   * Also caches axis projections for click detection.
   */
  void renderAxes()
  {
      const float fullSize = 0.5f;
      const float axisLength = fullSize * 0.75f;
      const float labelOffset = 0.02f;

      // === 1. PREPARE MATRICES ===
      glm::mat4 view = ctx.camera.GetViewMatrix();
      glm::mat4 projection = framework::mProjection_;
      glm::mat4 model = glm::mat4(1.0f);
      glm::mat4 mvp = projection * view * model;

      // === 2. RENDER AXIS LINES ===
      struct Vertex {
          glm::vec3 pos;
          glm::vec3 color;
      };

      std::vector<Vertex> axisVerts = {
          // X-axis (red)
          {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
          {{axisLength, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
          // Y-axis (green)
          {{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
          {{0.0f, axisLength, 0.0f}, {0.0f, 1.0f, 0.0f}},
          // Z-axis (blue)
          {{0.0f, 0.0f, 0.0f}, {0.3f, 0.3f, 0.8f}},
          {{0.0f, 0.0f, axisLength}, {0.3f, 0.3f, 0.8f}}
      };

      glUseProgram(ctx.shader);
      glUniformMatrix4fv(glGetUniformLocation(ctx.shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
      glUniformMatrix4fv(glGetUniformLocation(ctx.shader, "view"), 1, GL_FALSE, glm::value_ptr(view));
      glUniformMatrix4fv(glGetUniformLocation(ctx.shader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

      GLuint vao, vbo;
      glGenVertexArrays(1, &vao);
      glGenBuffers(1, &vbo);
      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);
      glBufferData(GL_ARRAY_BUFFER, axisVerts.size() * sizeof(Vertex), axisVerts.data(), GL_STATIC_DRAW);

      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
      glEnableVertexAttribArray(1);

      glLineWidth(2.0f);
      glDrawArrays(GL_LINES, 0, (GLsizei)axisVerts.size());
      glLineWidth(1.0f);

      glBindVertexArray(0);
      glDeleteBuffers(1, &vbo);
      glDeleteVertexArrays(1, &vao);

      // === 3. RENDER AXIS LABELS ===
      hudText.RenderText("x", axisLength + labelOffset, -labelOffset, 0.7f,
                         glm::vec3(1.0f, 0.0f, 0.0f), gViewport[2], gViewport[3]);
      hudText.RenderText("y", -labelOffset, axisLength + labelOffset, 0.7f,
                         glm::vec3(0.0f, 1.0f, 0.0f), gViewport[2], gViewport[3]);
      hudText.RenderText("z", 0.0f, -labelOffset + axisLength, 0.7f,
                         glm::vec3(0.3f, 0.3f, 0.8f), gViewport[2], gViewport[3]);

      // === 4. CACHE AXIS PROJECTIONS ===
      // Helper lambda to project 3D world position to 2D screen coordinates
      auto project3Dto2D = [&](const glm::vec3& worldPos) -> glm::vec2 {
          glm::vec4 clipSpace = mvp * glm::vec4(worldPos, 1.0f);
          if (std::abs(clipSpace.w) < 1e-6f) return glm::vec2(0.0f);

          glm::vec3 ndc = glm::vec3(clipSpace) / clipSpace.w;

          // NDC -> screen (agora SEM inverter Y!)
          float screenX = (ndc.x + 1.0f) * 0.5f * gViewport[2];
          float screenY = (1.0f - ndc.y) * 0.5f * gViewport[3];  // ← AQUI ESTÁ A CORREÇÃO!

          return glm::vec2(screenX, screenY);
      };

      // Project origin and all three axis endpoints
      glm::vec2 origin = project3Dto2D(glm::vec3(0.0f, 0.0f, 0.0f));
      glm::vec2 xEnd = project3Dto2D(glm::vec3(axisLength, 0.0f, 0.0f));
      glm::vec2 yEnd = project3Dto2D(glm::vec3(0.0f, axisLength, 0.0f));
      glm::vec2 zEnd = project3Dto2D(glm::vec3(0.0f, 0.0f, axisLength));

      // Store in global axis projection array for click detection
      // X axis
      gAxisProj[0].x0 = origin.x;
      gAxisProj[0].y0 = origin.y;
      gAxisProj[0].x1 = xEnd.x;
      gAxisProj[0].y1 = xEnd.y;

      // Y axis
      gAxisProj[1].x0 = origin.x;
      gAxisProj[1].y0 = origin.y;
      gAxisProj[1].x1 = yEnd.x;
      gAxisProj[1].y1 = yEnd.y;

      // Z axis
      gAxisProj[2].x0 = origin.x;
      gAxisProj[2].y0 = origin.y;
      gAxisProj[2].x1 = zEnd.x;
      gAxisProj[2].y1 = zEnd.y;

      // Mark projections as valid
      gAxisProjValid = true;
  }

  void renderCube()
  {
    const float a = 0.25f;
    std::vector<glm::vec3> faces;

    // Right face
    faces.push_back({ a,-a,-a}); faces.push_back({ a, a,-a});
    faces.push_back({ a, a, a}); faces.push_back({ a,-a, a});
    // Left face
    faces.push_back({-a,-a,-a}); faces.push_back({-a, a,-a});
    faces.push_back({-a, a, a}); faces.push_back({-a,-a, a});
    // Top face
    faces.push_back({-a, a,-a}); faces.push_back({ a, a,-a});
    faces.push_back({ a, a, a}); faces.push_back({-a, a, a});
    // Bottom face
    faces.push_back({-a,-a,-a}); faces.push_back({ a,-a,-a});
    faces.push_back({ a,-a, a}); faces.push_back({-a,-a, a});
    // Front face
    faces.push_back({-a,-a, a}); faces.push_back({ a,-a, a});
    faces.push_back({ a, a, a}); faces.push_back({-a, a, a});
    // Back face
    faces.push_back({-a,-a,-a}); faces.push_back({ a,-a,-a});
    faces.push_back({ a, a,-a}); faces.push_back({-a, a,-a});

    glm::mat4 view = ctx.camera.GetViewMatrix();
    glm::mat4 projection = framework::mProjection_;
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 mvp = projection * view * model;


    drawQuads(faces, glm::vec3(0.45f,0.13f,0.13f), mvp);
  }

  /**
   * Horizontal reference grid.
   */
  void renderGrid()
  {
    const float GRID_SIZE = 0.5f / EL;
    const float half = EL * GRID_SIZE / 2.0f;
    const float eps = -1e-4f;

    std::vector<glm::vec3> grid;
    for (unsigned i = 0; i <= EL; ++i)
    {
      float p = -half + i * GRID_SIZE;
      grid.push_back({ p, eps, -half });
      grid.push_back({ p, eps,  half });
      grid.push_back({ -half, eps, p });
      grid.push_back({  half, eps, p });
    }

    glm::mat4 view = ctx.camera.GetViewMatrix();
    glm::mat4 projection = framework::mProjection_;
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 mvp = projection * view * model;



    drawLines(grid, glm::vec3(1.0f,1.0f,1.0f), mvp, 1.0f);
  }

  void enhanceVoxel()
  {
    unsigned w = layerList->getSelected();
    const float GRID_SIZE = 0.5f / EL;

    float cx, cy, cz;

    if (currentMode == REPLAY)
    {
      const auto& center = automaton::lcenters[w];
      cx = ((center[0] + gConfig.view.vis_dx) - EL / 2) * GRID_SIZE;
      cy = ((center[1] + gConfig.view.vis_dy) - EL / 2) * GRID_SIZE;
      cz = ((center[2] + gConfig.view.vis_dz) - EL / 2) * GRID_SIZE;
    }
    else
    {
      Cell &cell = getCell(lattice_curr, CENTER, CENTER, CENTER, w);
      cx = ((cell.x[0] + gConfig.view.vis_dx) - EL / 2) * GRID_SIZE;
      cy = ((cell.x[1] + gConfig.view.vis_dy) - EL / 2) * GRID_SIZE;
      cz = ((cell.x[2] + gConfig.view.vis_dz) - EL / 2) * GRID_SIZE;
    }

    const int POINT_COUNT = 12;
    const float MAX_OFFSET = 0.005f;

    std::vector<glm::vec3> pts;
    pts.reserve(POINT_COUNT);
    for (int i = 0; i < POINT_COUNT; ++i)
    {
      float dx = ((std::rand() / (float)RAND_MAX) * 2.0f - 1.0f) * MAX_OFFSET;
      float dy = ((std::rand() / (float)RAND_MAX) * 2.0f - 1.0f) * MAX_OFFSET;
      float dz = ((std::rand() / (float)RAND_MAX) * 2.0f - 1.0f) * MAX_OFFSET;
      pts.emplace_back(cx + dx, cy + dy, cz + dz);
    }

    glm::mat4 view = ctx.camera.GetViewMatrix();
    glm::mat4 projection = framework::mProjection_;
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 mvp = projection * view * model;



    drawPoints(pts, glm::vec3(0.70f,0.70f,0.70f), mvp, 2.0f);
  }

  /**
   * Renders the XYZ gizmo in the top-right corner.
   * A miniature axis cross that follows camera rotation, with draggable
   * handles controlling vis_dx/dy/dz (visual wrapping offset).
   * Called from renderHUD() in 2D overlay mode.
   */
  void renderGizmo()
  {
      if (!showGizmo) return;

      const glm::mat4& P = ProjectionManager::instance().get2DOrtho();

      // Gizmo position and size (top-right, left of right panel)
      // Ortho uses top-left origin: y=0 at top, same as GLFW
      const float gizmoRadius = 55.0f;
      const float cx = (float)gViewport[2] - 285.0f;
      const float cy = 85.0f;  // near top

      // Camera rotation (extract 3x3 from view matrix, no translation)
      glm::mat3 rot(ctx.camera.GetViewMatrix());

      // Axis directions in view space
      // rot * worldAxis: x maps to screen-right, y maps to screen-up
      // But screen y increases DOWNWARD (top-left origin), so negate y
      glm::vec3 rawDirs[3] = {
          rot * glm::vec3(1, 0, 0),
          rot * glm::vec3(0, 1, 0),
          rot * glm::vec3(0, 0, 1),
      };

      glm::vec3 colors[3] = {
          {1.0f, 0.3f, 0.3f},  // X red
          {0.3f, 1.0f, 0.3f},  // Y green
          {0.4f, 0.4f, 1.0f},  // Z blue
      };
      const char* labels[3] = {"X", "Y", "Z"};

      // Store gizmo projections for click detection (in screen/GLFW coords)
      gGizmoProj.cx = cx;
      gGizmoProj.cy = cy;
      gGizmoProj.radius = gizmoRadius;

      // Draw background circle
      {
          const int seg = 32;
          std::vector<glm::vec2> circle;
          circle.reserve(seg + 2);
          circle.push_back({cx, cy});
          for (int i = 0; i <= seg; ++i)
          {
              float a = 2.0f * 3.14159265f * (float)i / (float)seg;
              circle.push_back({
                  cx + cosf(a) * (gizmoRadius + 5.0f),
                  cy + sinf(a) * (gizmoRadius + 5.0f)
              });
          }
          drawTriangleFan2D(circle, glm::vec3(0.05f, 0.05f, 0.12f), P);
      }

      // Draw axes and handles
      for (int axis = 0; axis < 3; ++axis)
      {
          // Screen-space endpoint (negate y for top-left origin)
          float endX = cx + rawDirs[axis].x * gizmoRadius;
          float endY = cy - rawDirs[axis].y * gizmoRadius;

          // Store for click detection (already in GLFW coords)
          gGizmoProj.ex[axis] = endX;
          gGizmoProj.ey[axis] = endY;

          // Draw axis line
          drawThickLine2D(cx, cy, endX, endY, 2.0f, colors[axis], P);

          // Draw axis label at endpoint
          // RenderText uses y-up (0=bottom), while drawing uses y-down (0=top)
          float lx = endX + rawDirs[axis].x * 10.0f;
          float ly_screen = endY - rawDirs[axis].y * 10.0f;
          float ly_text = (float)gViewport[3] - ly_screen;
          hudText.RenderText(labels[axis], lx, ly_text, 0.35f,
              colors[axis], gViewport[2], gViewport[3]);

          // Handle position based on current vis offset
          int visOffset = 0;
          if (axis == 0) visOffset = gConfig.view.vis_dx;
          else if (axis == 1) visOffset = gConfig.view.vis_dy;
          else visOffset = gConfig.view.vis_dz;

          // Map offset to parametric t [-1, 1] on the gizmo axis
          float t = 0.0f;
          if (EL > 0)
              t = glm::clamp((float)visOffset / ((float)EL * 0.5f), -1.0f, 1.0f);

          float handleX = cx + rawDirs[axis].x * gizmoRadius * t;
          float handleY = cy - rawDirs[axis].y * gizmoRadius * t;

          // Handle: small filled circle
          const int hseg = 12;
          const float handleR = 5.0f;
          std::vector<glm::vec2> handle;
          handle.reserve(hseg + 2);
          handle.push_back({handleX, handleY});
          for (int i = 0; i <= hseg; ++i)
          {
              float a = 2.0f * 3.14159265f * (float)i / (float)hseg;
              handle.push_back({
                  handleX + cosf(a) * handleR,
                  handleY + sinf(a) * handleR
              });
          }
          drawTriangleFan2D(handle, colors[axis], P);
      }

      // Draw origin dot
      {
          const int oseg = 8;
          std::vector<glm::vec2> originDot;
          originDot.reserve(oseg + 2);
          originDot.push_back({cx, cy});
          for (int i = 0; i <= oseg; ++i)
          {
              float a = 2.0f * 3.14159265f * (float)i / (float)oseg;
              originDot.push_back({
                  cx + cosf(a) * 3.0f,
                  cy + sinf(a) * 3.0f
              });
          }
          drawTriangleFan2D(originDot, glm::vec3(0.8f), P);
      }
  }

  /**
   * Renders 3D objects
   * A transformação MVP deve ser gerenciada pelo GUIrenderer antes de chamar esta função
   */
  void render3DObjects()
  {
    glEnable(GL_DEPTH_TEST);

    // Draw tomo plane FIRST (before voxels) so voxels render on top.
    // glDepthMask(GL_FALSE) prevents the plane from writing to the depth
    // buffer, so subsequent voxels always pass the depth test.
    if (tomoEnable && tomoEnable->getState() && EL > 0)
    {
      const float CELL_SPACING = 0.5f / static_cast<float>(EL);
      const int CENTER_INT = EL / 2;
      float half = (EL / 2) * CELL_SPACING;

      float pos;
      int plane = 0;
      if (tomoDirs.size() >= 3) {
        if (tomoDirs[0].isSelected())      { plane = 0; pos = ((int)::tomo_z - CENTER_INT) * CELL_SPACING; }
        else if (tomoDirs[1].isSelected()) { plane = 1; pos = ((int)::tomo_x - CENTER_INT) * CELL_SPACING; }
        else                               { plane = 2; pos = ((int)::tomo_y - CENTER_INT) * CELL_SPACING; }
      } else {
        pos = 0.0f;
      }

      std::vector<glm::vec3> quad;
      if (plane == 0) {
        quad = { {-half,-half,pos}, {half,-half,pos}, {half,half,pos}, {-half,half,pos} };
      } else if (plane == 1) {
        quad = { {pos,-half,-half}, {pos,half,-half}, {pos,half,half}, {pos,-half,half} };
      } else {
        quad = { {-half,pos,-half}, {half,pos,-half}, {half,pos,half}, {-half,pos,half} };
      }

      glm::mat4 view = ctx.camera.GetViewMatrix();
      glm::mat4 projection = framework::mProjection_;
      glm::mat4 mvp = projection * view * glm::mat4(1.0f);

      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDepthMask(GL_FALSE);

      drawQuads(quad, glm::vec3(0.3f, 0.6f, 1.0f), mvp);

      glDepthMask(GL_TRUE);
    }

    if (gConfig.simulation.scenario >= 0)
    {
      if (data3D[0].getState()) renderWavefront();
      if (data3D[1].getState()) renderMomentum(ctx);
      if (data3D[2].getState()) renderSpin();
      if (data3D[3].getState()) renderSineMask();
      if (data3D[4].getState()) renderHunting();
      if (data3D[5].getState()) renderCenters();
    }
    if (data3D.size() > 6 && data3D[6].getState())
    {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      renderCube();
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    if (data3D[7].getState()) renderAxes();
    if (data3D[8].getState()) renderGrid();
  }

} // namespace framework
