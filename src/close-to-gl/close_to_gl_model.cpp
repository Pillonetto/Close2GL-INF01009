#include "close_to_gl_model.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace {
glm::mat4 scaleMatrix(float scale) {
  glm::mat4 matrix = glm::mat4(0.f);

  matrix[0][0] = scale;
  matrix[1][1] = scale;
  matrix[2][2] = 1.f;

  return matrix;
}

glm::mat4 translateMatrix(float x, float y, float z) {
  glm::mat4 matrix = glm::mat4(0.f);

  matrix[0][3] = x;
  matrix[1][3] = y;
  matrix[2][3] = z;
  matrix[3][3] = 1.f;
  return matrix;
}
} // namespace

// center model in origin
glm::mat4 buildModelMatrix(float centerX, float centerY, float centerZ,
                           float scale) {
  return scaleMatrix(scale) * translateMatrix(centerX, centerY, centerZ);
}