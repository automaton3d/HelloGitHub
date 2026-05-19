/*
 * projection.h (Corrected Version)
 */

#ifndef PROJECTION_H_
#define PROJECTION_H_

#include <glad/glad.h>
#include <glm/glm.hpp>

namespace framework
{
    bool projectPoint(const float obj[3],
                    const glm::mat4& modelview,
                    const glm::mat4& projection,
                    const glm::vec4& viewport,
                    float &winX, float &winY);

    void map3DTo2D(const glm::vec3& pt,
                 double mouseX, double mouseY,
                 const glm::mat4& modelMat,
                 const glm::mat4& projMat,
                 const glm::vec4& viewport,
                 float depth,
                 float axisLength);

    float distanceToSegment(float px, float py,
                          float ax, float ay,
                          float bx, float by);

    float closestPointOnSegmentToRay(const glm::vec3& rayO, const glm::vec3& rayD,
                                   const glm::vec3& segA, const glm::vec3& segB,
                                   float& outDist);

    void updateProjection();
}

#endif