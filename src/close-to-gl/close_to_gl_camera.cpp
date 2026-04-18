#include "close_to_gl_camera.hpp"

#include <cmath>

#include "camera_gui.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

glm::mat4 cameraViewMatrix(const CameraData &cam) {
  glm::vec3 forward = glm::normalize(cam.lookAtTarget - cam.position);

  glm::vec3 worldUp = glm::vec3(0.f, 1.f, 0.f);
  if (std::abs(glm::dot(forward, worldUp)) > 0.98f)
    worldUp = glm::vec3(0.f, 0.f, 1.f);

  glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
  glm::vec3 up = glm::cross(right, forward);

  glm::mat4 viewMatrix = glm::mat4(1.0f);

  viewMatrix[0][0] = right.x;
  viewMatrix[0][1] = right.y;
  viewMatrix[0][2] = right.z;
  viewMatrix[0][3] = -glm::dot(right, cam.position);

  viewMatrix[1][0] = up.x;
  viewMatrix[1][1] = up.y;
  viewMatrix[1][2] = up.z;
  viewMatrix[1][3] = -glm::dot(up, cam.position);

  viewMatrix[2][0] = -forward.x;
  viewMatrix[2][1] = -forward.y;
  viewMatrix[2][2] = -forward.z;
  viewMatrix[2][3] = -glm::dot(forward, cam.position);

  viewMatrix[3][0] = 0;
  viewMatrix[3][1] = 0;
  viewMatrix[3][2] = 0;
  viewMatrix[3][3] = 1.0f;

  return viewMatrix;
}

// Symmetric perspective from vertical FOV (Song Ho Ahn):
// https://www.songho.ca/opengl/gl_projectionmatrix.html
glm::mat4 cameraProjectionMatrix(float aspect, const CameraData &cam) {
  constexpr float kFovYDegrees = 45.f;
  float front = 0.f, back = 0.f;

  constexpr float kEps = 1e-8f;
  if (std::fabs(aspect) <= kEps)
    return glm::mat4(1.f);

  // calculo de left e right extraidos de
  // https://www.songho.ca/opengl/gl_projectionmatrix.html
  const float fovYRad = glm::radians(kFovYDegrees);
  const float tangent = std::tan(fovYRad * 0.5f);

  const float top = front * tangent;
  const float right = top * aspect;

  // simplificado para campo de visao simetrico
  // https://www.songho.ca/opengl/gl_projectionmatrix.html
  glm::mat4 P(0.f);
  P[0][0] = front / right;
  P[1][1] = front / top;
  P[2][2] = -(back + front) / (back - front);
  P[2][3] = -1.f;
  P[3][2] = -(2.f * back * front) / (back - front);
  P[3][3] = 0.f;
  return P;
}
