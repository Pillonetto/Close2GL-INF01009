#include "close_to_gl_camera.hpp"

#include "camera_gui.hpp"

#include <cmath>
#include <glm/glm.hpp>

namespace {

constexpr float kZNearMin = 1e-4f;
constexpr float kZNearMax = 1e5f;
constexpr float kZFarMinSep = 1e-3f;
constexpr float kZFarMax = 1e7f;
constexpr float kFovYDegrees = 45.f;
constexpr float kAspectEps = 1e-8f;

void sanitizeClipPlanes(float zNearIn, float zFarIn, float &zNearOut,
                        float &zFarOut) {
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

glm::vec3 worldUpForForward(const glm::vec3 &forward) {
  glm::vec3 worldUp(0.f, 1.f, 0.f);
  if (std::fabs(glm::dot(forward, worldUp)) > 0.98f)
    worldUp = glm::vec3(0.f, 0.f, 1.f);
  return worldUp;
}

void cameraBasis(const CameraData &cam, glm::vec3 &outRight, glm::vec3 &outUp,
                 glm::vec3 &outForward) {
  outForward = glm::normalize(cam.lookAtTarget - cam.position);
  const glm::vec3 wup = worldUpForForward(outForward);
  outRight = glm::normalize(glm::cross(outForward, wup));
  outUp = glm::cross(outRight, outForward);
}

} // namespace

glm::mat4 cameraViewMatrix(const CameraData &cam) {
  glm::vec3 right, up, forward;
  cameraBasis(cam, right, up, forward);

  glm::mat4 view(1.f);
  view[0][0] = right.x;
  view[1][0] = right.y;
  view[2][0] = right.z;
  view[0][1] = up.x;
  view[1][1] = up.y;
  view[2][1] = up.z;
  view[0][2] = -forward.x;
  view[1][2] = -forward.y;
  view[2][2] = -forward.z;
  view[3][0] = -glm::dot(right, cam.position);
  view[3][1] = -glm::dot(up, cam.position);
  view[3][2] = glm::dot(forward, cam.position);
  view[3][3] = 1.f;
  return view;
}

glm::mat4 cameraProjectionMatrix(float aspect, const CameraData &cam) {
  if (std::fabs(aspect) <= kAspectEps)
    return glm::mat4(1.f);

  float zn = 0.f, zf = 0.f;
  sanitizeClipPlanes(cam.zNear, cam.zFar, zn, zf);

  const float fovy = glm::radians(kFovYDegrees);
  const float tanHalfFovy = std::tan(fovy * 0.5f);

  glm::mat4 p(0.f);
  p[0][0] = 1.f / (aspect * tanHalfFovy);
  p[1][1] = 1.f / tanHalfFovy;
  p[2][2] = -(zf + zn) / (zf - zn);
  p[2][3] = -1.f;
  p[3][2] = -(2.f * zf * zn) / (zf - zn);
  return p;
}
