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

          // NDC -> screen (without inverting Y)
          float screenX = (ndc.x + 1.0f) * 0.5f * gViewport[2];
          float screenY = (1.0f - ndc.y) * 0.5f * gViewport[3];

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
    const float a = 0.25f;                    // half cube size
    const float sphereRadius = a;             // inscribed sphere (fits inside cube)

    glm::mat4 view = ctx.camera.GetViewMatrix();
    glm::mat4 projection = framework::mProjection_;
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 mvp = projection * view * model;

    // =============================================
    // 1. Wireframe Cube (12 edges only)
    // =============================================
    std::vector<glm::vec3> edges;

    // Bottom face
    edges.push_back({-a, -a, -a}); edges.push_back({ a, -a, -a});
    edges.push_back({ a, -a, -a}); edges.push_back({ a, -a,  a});
    edges.push_back({ a, -a,  a}); edges.push_back({-a, -a,  a});
    edges.push_back({-a, -a,  a}); edges.push_back({-a, -a, -a});
    // Top face
    edges.push_back({-a,  a, -a}); edges.push_back({ a,  a, -a});
    edges.push_back({ a,  a, -a}); edges.push_back({ a,  a,  a});
    edges.push_back({ a,  a,  a}); edges.push_back({-a,  a,  a});
    edges.push_back({-a,  a,  a}); edges.push_back({-a,  a, -a});
    // Verticals
    edges.push_back({-a, -a, -a}); edges.push_back({-a,  a, -a});
    edges.push_back({ a, -a, -a}); edges.push_back({ a,  a, -a});
    edges.push_back({ a, -a,  a}); edges.push_back({ a,  a,  a});
    edges.push_back({-a, -a,  a}); edges.push_back({-a,  a,  a});

    drawLines(edges, glm::vec3(0.75f, 0.22f, 0.22f), mvp, 1.8f);

    // =============================================
    // 2. Isotropic Sphere (Fibonacci Spiral)
    // =============================================
    std::vector<glm::vec3> spherePoints;
    const int numPoints = 850;                    // increased for better appearance
    const float goldenAngle = glm::pi<float>() * (3.0f - sqrtf(5.0f)); // ~2.39996

    for (int i = 0; i < numPoints; ++i)
    {
        float y = 1.0f - (i / float(numPoints - 1)) * 2.0f;           // from -1 to 1
        float radius = sqrtf(1.0f - y * y);                           // circle radius at this latitude

        float theta = goldenAngle * i;                                // golden angle

        float x = radius * cosf(theta);
        float z = radius * sinf(theta);

        spherePoints.emplace_back(
            sphereRadius * x,
            sphereRadius * y,
            sphereRadius * z
        );
    }

    drawPoints(spherePoints, glm::vec3(0.95f, 0.45f, 0.45f), mvp, 2.4f);
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
// ---------------------------------------------------------------------
// renderGizmo() implementation
// ---------------------------------------------------------------------
void renderGizmo()
{
    if (!showGizmo) return;

    const glm::mat4& P = ProjectionManager::instance().get2DOrtho();

    const float gizmoRadius = 70.0f;
    const float cx = (float)gViewport[2] - 285.0f - 60.0f;
    const float cy = 85.0f + 70.0f;

    glm::mat3 rot(ctx.camera.GetViewMatrix());

    glm::vec3 rawDirs[3] = {
        rot * glm::vec3(1, 0, 0),
        rot * glm::vec3(0, 1, 0),
        rot * glm::vec3(0, 0, 1)
    };

    glm::vec3 colors[3] = {
        {1.0f, 0.25f, 0.25f},  // X
        {0.25f, 1.0f, 0.25f},  // Y
        {0.25f, 0.5f, 1.0f}    // Z
    };

    const char* labels[3] = {"X", "Y", "Z"};

    gGizmoProj.cx = cx;
    gGizmoProj.cy = cy;
    gGizmoProj.radius = gizmoRadius + 10.0f;

    const float r = 8.5f;   // handle bubble radius

    // Helper to draw outlined circle (contour only)
    auto drawCircleOutline = [&](float centerX, float centerY, float radius, const glm::vec3& color)
    {
        const int segments = 40;
        for (int i = 0; i < segments; ++i)
        {
            float a1 = 2.0f * 3.14159265f * i / segments;
            float a2 = 2.0f * 3.14159265f * (i + 1) / segments;
            float x1 = centerX + cosf(a1) * radius;
            float y1 = centerY + sinf(a1) * radius;
            float x2 = centerX + cosf(a2) * radius;
            float y2 = centerY + sinf(a2) * radius;
            drawLine2D_new(x1, y1, x2, y2, color, color, P);
        }
    };

    // 1. Axes and handle bubbles (outline only)
    for (int axis = 0; axis < 3; ++axis)
    {
        glm::vec3 dir = glm::normalize(rawDirs[axis]);
        glm::vec3 col = colors[axis];
        glm::vec3 faded = col * 0.6f;   // faded tone for negative axis

        // Positive
        float centerX_pos = cx + dir.x * (gizmoRadius - r);
        float centerY_pos = cy - dir.y * (gizmoRadius - r);
        float tipX_pos = cx + dir.x * (gizmoRadius - 2.0f * r);
        float tipY_pos = cy - dir.y * (gizmoRadius - 2.0f * r);
        drawLine2D_new(cx, cy, tipX_pos, tipY_pos, col, col, P);
        drawCircleOutline(centerX_pos, centerY_pos, r, col);

        // NEGATIVE (same length)
        float centerX_neg = cx - dir.x * (gizmoRadius - r);
        float centerY_neg = cy + dir.y * (gizmoRadius - r);
        float tipX_neg = cx - dir.x * (gizmoRadius - 2.0f * r);
        float tipY_neg = cy + dir.y * (gizmoRadius - 2.0f * r);
        drawLine2D_new(cx, cy, tipX_neg, tipY_neg, faded, faded, P);
        drawCircleOutline(centerX_neg, centerY_neg, r, faded);

        // ============================================================
        // IMPORTANT: store positive bubble coords for hit test
        // ============================================================
        gGizmoProj.ex[axis] = centerX_pos;
        gGizmoProj.ey[axis] = centerY_pos;

        // Label (centered, offset 2px down)
        float lx = centerX_pos;
        float ly_screen = centerY_pos;
        float ly_text = (float)gViewport[3] - ly_screen;
        hudText.RenderText(labels[axis], lx - 5.0f, ly_text - 5.0f,
                           0.50f, col, gViewport[2], gViewport[3]);
    }

    // 2. Central point (solid, with border)
    {
        const float originR = 4.0f;
        std::vector<glm::vec2> originDot;
        originDot.reserve(20);
        originDot.push_back({cx, cy});
        for (int i = 0; i < 18; ++i)
        {
            float a = 2.0f * 3.14159265f * i / 18.0f;
            originDot.push_back({ cx + cosf(a) * originR, cy + sinf(a) * originR });
        }
        drawTriangleFan2D(originDot, glm::vec3(0.8f), P); // fill
        for (int i = 0; i < 18; ++i)
        {
            int j = (i + 1) % 18;
            glm::vec2 p1 = originDot[i+1], p2 = originDot[j+1];
            drawLine2D_new(p1.x, p1.y, p2.x, p2.y, glm::vec3(0.5f), glm::vec3(0.5f), P);
        }
    }

    // ==================================================================
    // 3. DRAW FEEDBACK CUBE (hover or drag)
    // ==================================================================
    if (framework::showDragCube && showGizmo)
    {
        float cubeX = framework::dragCubeX;
        float cubeY = framework::dragCubeY;
        const float size = 18.0f;   // size in pixels

        std::vector<glm::vec2> cubeVerts = {
            {cubeX - size/2, cubeY - size/2},
            {cubeX + size/2, cubeY - size/2},
            {cubeX + size/2, cubeY + size/2},
            {cubeX - size/2, cubeY + size/2}
        };

        // Orange fill
        drawTriangleFan2D(cubeVerts, glm::vec3(1.0f, 0.5f, 0.2f), P);
        // Black border
        for (int i = 0; i < 4; ++i)
        {
            int j = (i + 1) % 4;
            drawLine2D_new(cubeVerts[i].x, cubeVerts[i].y,
                           cubeVerts[j].x, cubeVerts[j].y,
                           glm::vec3(0.0f, 0.0f, 0.0f),
                           glm::vec3(0.0f, 0.0f, 0.0f), P);
        }
    }
}

  /**
   * Renders 3D objects
   * The MVP transform must be managed by GUIrenderer before calling this function
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
//      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      renderCube();
  //    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    if (data3D[7].getState()) renderAxes();
    if (data3D[8].getState()) renderGrid();
  }

} // namespace framework
