#pragma once

#include <glm/mat4x4.hpp>

struct CameraData;

glm::mat4 cameraViewMatrix(const CameraData &cam);
glm::mat4 cameraProjectionMatrix(float aspect, const CameraData &cam);
