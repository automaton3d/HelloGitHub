/*
 * projection.cpp
 */

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>

#include "GUI.h"
#include "projection.h"
#include "projection_manager.h"
#include "config.h"
#include "model/simulation.h" // for automaton::L3

namespace framework
{
  extern AxisThumb thumb;
  extern glm::mat4 mProjection_;

  void map3DTo2D(const glm::vec3& pt,
                 double mouseX, double mouseY,
                 const glm::mat4& modelMat,
                 const glm::mat4& projMat,
                 const glm::vec4& viewport,
                 float depth,
                 float axisLength)
  {
      glm::vec3 winPos(mouseX, viewport[3] - mouseY, depth);
      glm::vec3 world = glm::unProject(winPos, modelMat, projMat, viewport);

      float raw_pos = 0.0f;
      if (thumb.axis == 0)      raw_pos = world.x;
      else if (thumb.axis == 1) raw_pos = world.y;
      else if (thumb.axis == 2) raw_pos = world.z;

      float delta_world = raw_pos - thumb.initialPosition;
      thumb.position = thumb.initialPosition + delta_world;

      float wrapped = std::fmod(thumb.position, axisLength);
      if (wrapped < 0.0f) wrapped += axisLength;
      thumb.position = wrapped;

      float laps = delta_world / axisLength;

      int delta_cells = static_cast<int>(std::round(laps * automaton::L3));

      if (thumb.axis == 0) gConfig.view.vis_dx = thumb.startOffset[0] + delta_cells;
      if (thumb.axis == 1) gConfig.view.vis_dy = thumb.startOffset[1] + delta_cells;
      if (thumb.axis == 2) gConfig.view.vis_dz = thumb.startOffset[2] + delta_cells;

      auto wrap = [](int& v) {
          v = (v % automaton::L3 + automaton::L3) % automaton::L3;
      };

      wrap(gConfig.view.vis_dx);
      wrap(gConfig.view.vis_dy);
      wrap(gConfig.view.vis_dz);
  }

  bool projectPoint(const float obj[3],
                    const glm::mat4& mv,
                    const glm::mat4& pj,
                    const glm::vec4& vp,
                    float &winX, float &winY)
  {
    glm::vec3 screen = glm::project(glm::vec3(obj[0], obj[1], obj[2]), mv, pj, vp);
    winX = screen.x;
    winY = screen.y;
    return screen.z >= 0.0f && screen.z <= 1.0f;
  }

  float distanceToSegment(float px, float py, float ax, float ay, float bx, float by)
  {
    float dx = bx - ax;
    float dy = by - ay;
    float denom = dx*dx + dy*dy;

    if (denom < 1e-8f)
        return sqrtf(powf(px - ax, 2) + powf(py - ay, 2));

    float t = std::max(0.0f, std::min(1.0f,
        ((px - ax) * dx + (py - ay) * dy) / denom));

    return sqrtf(powf(px - (ax + t * dx), 2) +
                 powf(py - (ay + t * dy), 2));
  }

  float closestPointOnSegmentToRay(const glm::vec3& rayO,
                                   const glm::vec3& rayD,
                                   const glm::vec3& segA,
                                   const glm::vec3& segB,
                                   float& outDist)
  {
    glm::vec3 u = rayD;
    glm::vec3 v = segB - segA;
    glm::vec3 w0 = rayO - segA;

    float a = glm::dot(u,u);
    float b = glm::dot(u,v);
    float c = glm::dot(v,v);
    float d = glm::dot(u,w0);
    float e = glm::dot(v,w0);

    float denom = a*c - b*b;
    float sc, tc;

    if (denom < 1e-8f) {
      sc = 0.0f;
      tc = glm::clamp(e / c, 0.0f, 1.0f);
    } else {
      sc = (b*e - c*d) / denom;
      tc = glm::clamp((a*e - b*d) / denom, 0.0f, 1.0f);
    }

    outDist = glm::length((rayO + sc*u) - (segA + tc*v));
    return tc;
  }

  void updateProjection()
  {
    if (projectRads.empty()) return;

    float ratio = gViewport[2] > 0
        ? (float)gViewport[2] / gViewport[3]
        : 1.0f;

    // GUI has priority
    bool usePerspective;

    if (projectRads[0].isSelected())
        usePerspective = false;
    else if (projectRads[1].isSelected())
        usePerspective = true;
    else
        usePerspective = gConfig.projection.perspective;

    if (usePerspective)
    {
        mProjection_ = ProjectionManager::instance().get3DPerspective(
            gConfig.projection.fov,
            gConfig.projection.near_plane,
            gConfig.projection.far_plane
        );
    }
else
{
    float scale = gConfig.view.ortho_scale;
    mProjection_ = glm::ortho(
        -scale * ratio,
         scale * ratio,
        -scale,
         scale,
        gConfig.projection.near_plane,
        gConfig.projection.far_plane
    );
}  }

} // namespace framework