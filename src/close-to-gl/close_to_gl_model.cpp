#include "close_to_gl_model.hpp"

#include <glm/glm.hpp>

glm::mat4 buildModelMatrix(float centerX, float centerY, float centerZ,
                           float scale) {
  glm::mat4 output(1.f);
  output[0][0] = scale;
  output[1][1] = scale;
  output[2][2] = scale;

  output[3][0] = -scale * centerX;
  output[3][1] = -scale * centerY;
  output[3][2] = -scale * centerZ;

  return output;
}
