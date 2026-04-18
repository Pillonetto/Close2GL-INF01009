#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

struct CameraData;

/// Clamp near/far for stable perspective (same rules as the camera UI).
void sanitizeOpenGlPerspectiveClipPlanes(float zNearIn, float zFarIn,
                                         float &zNearOut, float &zFarOut);

glm::vec3 openGlCameraForward(const CameraData &cam);
glm::vec3 openGlCameraRight(const CameraData &cam);
glm::vec3 openGlCameraUp(const CameraData &cam);

glm::mat4 openGlViewMatrix(const CameraData &cam);
glm::mat4 openGlProjectionMatrix(float aspect, const CameraData &cam);

/// Centers the model at the origin: scale then translate by \(-center\) (matches
/// previous `buildModelMatrix` in main).
glm::mat4 openGlModelMatrix(float centerX, float centerY, float centerZ,
                            float scale);
