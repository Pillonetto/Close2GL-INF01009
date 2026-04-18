#include "open_gl_matrices.hpp"

#include "camera_gui.hpp"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace {

constexpr float kZNearMin = 1e-4f;
constexpr float kZNearMax = 1e5f;
constexpr float kZFarMinSep = 1e-3f;
constexpr float kZFarMax = 1e7f;

glm::vec3 worldUpForCamera(const glm::vec3 &forward) {
  glm::vec3 worldUp(0.f, 1.f, 0.f);
  if (std::fabs(glm::dot(forward, worldUp)) > 0.98f)
    worldUp = glm::vec3(0.f, 0.f, 1.f);
  return worldUp;
}

void cameraRolledBasis(const CameraData &cam, glm::vec3 &outRight,
                       glm::vec3 &outUp) {
  const glm::vec3 f = glm::normalize(cam.lookAtTarget - cam.position);
  const glm::vec3 wup = worldUpForCamera(f);
  outRight = glm::normalize(glm::cross(f, wup));
  outUp = glm::cross(outRight, f);
}

} // namespace

void sanitizeOpenGlPerspectiveClipPlanes(float zNearIn, float zFarIn,
                                         float &zNearOut, float &zFarOut) {
  zNearOut = zNearIn;
  zFarOut = zFarIn;
  if (zNearOut < kZNearMin)
    zNearOut = kZNearMin;
  if (zNearOut > kZNearMax)
    zNearOut = kZNearMax;
  if (zFarOut <= zNearOut + kZFarMinSep)
    zFarOut = zNearOut + kZFarMinSep;
  if (zFarOut > kZFarMax)
    zFarOut = kZFarMax;
}

glm::vec3 openGlCameraForward(const CameraData &cam) {
  return glm::normalize(cam.lookAtTarget - cam.position);
}

glm::vec3 openGlCameraRight(const CameraData &cam) {
  glm::vec3 r, u;
  cameraRolledBasis(cam, r, u);
  return r;
}

glm::vec3 openGlCameraUp(const CameraData &cam) {
  glm::vec3 r, u;
  cameraRolledBasis(cam, r, u);
  return u;
}

glm::mat4 openGlViewMatrix(const CameraData &cam) {
  return glm::lookAt(cam.position, cam.lookAtTarget, openGlCameraUp(cam));
}

glm::mat4 openGlProjectionMatrix(float aspect, const CameraData &cam) {
  float zn = 0.f, zf = 0.f;
  sanitizeOpenGlPerspectiveClipPlanes(cam.zNear, cam.zFar, zn, zf);
  return glm::perspective(glm::radians(45.f), aspect, zn, zf);
}

glm::mat4 openGlModelMatrix(float centerX, float centerY, float centerZ,
                            float scale) {
  return glm::scale(glm::mat4(1.f), glm::vec3(scale)) *
         glm::translate(glm::mat4(1.f),
                        glm::vec3(-centerX, -centerY, -centerZ));
}
