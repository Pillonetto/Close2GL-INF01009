#include "software_rasterizer.hpp"

#include <GL3/gl3w.h>
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

namespace {

// get area of palelogram using cross product
float edgeFunction(const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &p) {
  return (p.x - a.x) * (b.y - a.y) - (p.y - a.y) * (b.x - a.x);
}

void shadeAndWritePixel(RasterFrame &frame, int x, int y, float ndcDepth,
                        const glm::vec3 &rgb) {
  // check if pixel is out of image
  if (x < 0 || x >= frame.width || y < 0 || y >= frame.height)
    return;

  // check if pixel is out of near and far planes
  if (ndcDepth < -1.f || ndcDepth > 1.f)
    return;

  // normalize depth to [0, 1]
  const float depth01 = glm::clamp(ndcDepth * 0.5f + 0.5f, 0.f, 1.f);
  const size_t pixelIndex =
      static_cast<size_t>(y) * static_cast<size_t>(frame.width) +
      static_cast<size_t>(x);
  if (depth01 >= frame.depth[pixelIndex])
    return;

  frame.depth[pixelIndex] = depth01;
  const glm::vec3 c = clamp01(rgb);
  const size_t cidx = pixelIndex * 4u;
  // get color from floating point values
  frame.color[cidx + 0] = static_cast<std::uint8_t>(c.r * 255.f);
  frame.color[cidx + 1] = static_cast<std::uint8_t>(c.g * 255.f);
  frame.color[cidx + 2] = static_cast<std::uint8_t>(c.b * 255.f);
  frame.color[cidx + 3] = 255;
}

// v = three vertices for triangles
// a = attributes of eadh vertices
// b = barycentric weights
glm::vec3 interpolatePerspectiveVec3(const RasterVertex &v0,
                                     const RasterVertex &v1,
                                     const RasterVertex &v2,
                                     const glm::vec3 &a0, const glm::vec3 &a1,
                                     const glm::vec3 &a2, float b0, float b1,
                                     float b2) {

  // check if denominator too close to zero
  const float denom = b0 * v0.invW + b1 * v1.invW + b2 * v2.invW;
  if (std::abs(denom) < 1e-8f)
    return a0;
  // interpolate attributes using barycentric weights
  const glm::vec3 num =
      b0 * a0 * v0.invW + b1 * a1 * v1.invW + b2 * a2 * v2.invW;
  return num / denom;
}

// rasterize a line segment between two vertices
void rasterizeWireEdge(const RasterVertex &v0, const RasterVertex &v1,
                       int shadingMode, const glm::vec3 &baseColor,
                       const LightingParams &light, RasterFrame &frame) {
  const float dx = v1.screen.x - v0.screen.x;
  const float dy = v1.screen.y - v0.screen.y;
  const int steps = glm::max(
      1, static_cast<int>(std::ceil(glm::max(std::abs(dx), std::abs(dy)))));

  for (int i = 0; i <= steps; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(steps);
    const float x = v0.screen.x + t * dx;
    const float y = v0.screen.y + t * dy;
    const int px = static_cast<int>(std::lround(x));
    const int py = static_cast<int>(std::lround(y));

    const float depthNdc = v0.ndc.z + t * (v1.ndc.z - v0.ndc.z);

    glm::vec3 pixelColor = baseColor;
    if (shadingMode == 1 || shadingMode == 2) {
      const float invW = (1.f - t) * v0.invW + t * v1.invW;
      if (std::abs(invW) > 1e-8f) {
        const glm::vec3 num = (1.f - t) * v0.gouraudColor * v0.invW +
                              t * v1.gouraudColor * v1.invW;
        pixelColor = num / invW;
      }
    } else if (shadingMode == 3) {
      const float invW = (1.f - t) * v0.invW + t * v1.invW;
      if (std::abs(invW) > 1e-8f) {
        const glm::vec3 normalNum =
            (1.f - t) * v0.normalEye * v0.invW + t * v1.normalEye * v1.invW;
        const glm::vec3 posNum =
            (1.f - t) * v0.posEye * v0.invW + t * v1.posEye * v1.invW;
        pixelColor =
            evaluatePhongLighting(baseColor, glm::normalize(normalNum / invW),
                                  posNum / invW, light, true);
      }
    }

    shadeAndWritePixel(frame, px, py, depthNdc, pixelColor);
  }
}

} // namespace

glm::vec3 clamp01(const glm::vec3 &c) {
  return glm::clamp(c, glm::vec3(0.f), glm::vec3(1.f));
}

glm::vec2 ndcToScreen(const glm::vec3 &ndc, int width, int height) {
  const float sx = (ndc.x * 0.5f + 0.5f) * static_cast<float>(width - 1);
  const float sy = (ndc.y * 0.5f + 0.5f) * static_cast<float>(height - 1);
  return glm::vec2(sx, sy);
}

glm::vec3 evaluatePhongLighting(const glm::vec3 &baseColor,
                                const glm::vec3 &normalEye,
                                const glm::vec3 &posEye,
                                const LightingParams &light,
                                bool includeSpecular) {
  const glm::vec3 N = glm::normalize(normalEye);
  const glm::vec3 L = glm::normalize(light.lightPosEye - posEye);
  const float ndotl = glm::max(glm::dot(N, L), 0.f);
  glm::vec3 output = light.ambient * light.lightColor * baseColor +
                     light.kd * ndotl * light.lightColor * baseColor;

  if (includeSpecular && ndotl > 0.f) {
    const glm::vec3 V = glm::normalize(-posEye);
    const glm::vec3 R = glm::reflect(-L, N);
    const float spec = std::pow(glm::max(glm::dot(R, V), 0.f), light.shininess);
    output += light.ks * spec * light.lightColor;
  }
  return clamp01(output);
}

void ensureRasterFrameSize(RasterFrame &frame, int width, int height) {
  if (width <= 0 || height <= 0)
    return;

  if (frame.width == width && frame.height == height && frame.textureId != 0)
    return;

  frame.width = width;
  frame.height = height;
  frame.color.resize(static_cast<size_t>(width) * static_cast<size_t>(height) *
                     static_cast<size_t>(4));
  frame.depth.resize(static_cast<size_t>(width) * static_cast<size_t>(height));

  if (frame.textureId != 0)
    glDeleteTextures(1, &frame.textureId);
  glActiveTexture(GL_TEXTURE1);
  glGenTextures(1, &frame.textureId);
  glBindTexture(GL_TEXTURE_2D, frame.textureId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
}

void clearRasterFrame(RasterFrame &frame, const glm::vec4 &clearColor) {
  const std::uint8_t r =
      static_cast<std::uint8_t>(glm::clamp(clearColor.r, 0.f, 1.f) * 255.f);
  const std::uint8_t g =
      static_cast<std::uint8_t>(glm::clamp(clearColor.g, 0.f, 1.f) * 255.f);
  const std::uint8_t b =
      static_cast<std::uint8_t>(glm::clamp(clearColor.b, 0.f, 1.f) * 255.f);
  const std::uint8_t a =
      static_cast<std::uint8_t>(glm::clamp(clearColor.a, 0.f, 1.f) * 255.f);

  for (size_t i = 0; i < frame.color.size(); i += 4) {
    frame.color[i + 0] = r;
    frame.color[i + 1] = g;
    frame.color[i + 2] = b;
    frame.color[i + 3] = a;
  }
  std::fill(frame.depth.begin(), frame.depth.end(), 1.f);
}

void rasterizeSolidTriangle(const RasterVertex &inV0, const RasterVertex &inV1,
                            const RasterVertex &inV2, int shadingMode,
                            const glm::vec3 &baseColor,
                            const LightingParams &light, RasterFrame &frame) {
  RasterVertex v0 = inV0;
  RasterVertex v1 = inV1;
  RasterVertex v2 = inV2;

  float area = edgeFunction(v0.screen, v1.screen, v2.screen);

  // check if triangle is degenerate
  if (area < 0.f) {
    std::swap(v1, v2);
    area = -area;
  }

  const float minXf =
      std::floor(glm::min(v0.screen.x, glm::min(v1.screen.x, v2.screen.x)));
  const float maxXf =
      std::ceil(glm::max(v0.screen.x, glm::max(v1.screen.x, v2.screen.x)));
  const float minYf =
      std::floor(glm::min(v0.screen.y, glm::min(v1.screen.y, v2.screen.y)));
  const float maxYf =
      std::ceil(glm::max(v0.screen.y, glm::max(v1.screen.y, v2.screen.y)));

  const int minX = glm::clamp(static_cast<int>(minXf), 0, frame.width - 1);
  const int maxX = glm::clamp(static_cast<int>(maxXf), 0, frame.width - 1);
  const int minY = glm::clamp(static_cast<int>(minYf), 0, frame.height - 1);
  const int maxY = glm::clamp(static_cast<int>(maxYf), 0, frame.height - 1);

  for (int y = minY; y <= maxY; ++y) {
    for (int x = minX; x <= maxX; ++x) {
      const glm::vec2 pixelCenter(static_cast<float>(x) + 0.5f,
                                  static_cast<float>(y) + 0.5f);
      const float w0 = edgeFunction(v1.screen, v2.screen, pixelCenter);
      const float w1 = edgeFunction(v2.screen, v0.screen, pixelCenter);
      const float w2 = edgeFunction(v0.screen, v1.screen, pixelCenter);
      if (w0 < 0.f || w1 < 0.f || w2 < 0.f)
        continue;

      const float b0 = w0 / area;
      const float b1 = w1 / area;
      const float b2 = w2 / area;
      const float depthNdc = b0 * v0.ndc.z + b1 * v1.ndc.z + b2 * v2.ndc.z;

      glm::vec3 pixelColor = baseColor;
      if (shadingMode == 1 || shadingMode == 2) {
        pixelColor = interpolatePerspectiveVec3(v0, v1, v2, v0.gouraudColor,
                                                v1.gouraudColor,
                                                v2.gouraudColor, b0, b1, b2);
      } else if (shadingMode == 3) {
        const glm::vec3 normalEye = glm::normalize(interpolatePerspectiveVec3(
            v0, v1, v2, v0.normalEye, v1.normalEye, v2.normalEye, b0, b1, b2));
        const glm::vec3 posEye = interpolatePerspectiveVec3(
            v0, v1, v2, v0.posEye, v1.posEye, v2.posEye, b0, b1, b2);
        pixelColor =
            evaluatePhongLighting(baseColor, normalEye, posEye, light, true);
      }

      shadeAndWritePixel(frame, x, y, depthNdc, pixelColor);
    }
  }
}

void rasterizeWireTriangle(const RasterVertex &v0, const RasterVertex &v1,
                           const RasterVertex &v2, int shadingMode,
                           const glm::vec3 &baseColor,
                           const LightingParams &light, RasterFrame &frame) {
  rasterizeWireEdge(v0, v1, shadingMode, baseColor, light, frame);
  rasterizeWireEdge(v1, v2, shadingMode, baseColor, light, frame);
  rasterizeWireEdge(v2, v0, shadingMode, baseColor, light, frame);
}

void rasterizeVertexPoint(const RasterVertex &v, int shadingMode,
                          const glm::vec3 &baseColor,
                          const LightingParams &light, float pointSize,
                          RasterFrame &frame) {
  glm::vec3 pixelColor = baseColor;
  if (shadingMode == 1 || shadingMode == 2) {
    pixelColor = v.gouraudColor;
  } else if (shadingMode == 3) {
    pixelColor =
        evaluatePhongLighting(baseColor, v.normalEye, v.posEye, light, true);
  }

  const int radius =
      glm::max(0, static_cast<int>(std::floor(pointSize * 0.5f)));
  const int cx = static_cast<int>(std::lround(v.screen.x));
  const int cy = static_cast<int>(std::lround(v.screen.y));
  for (int dy = -radius; dy <= radius; ++dy) {
    for (int dx = -radius; dx <= radius; ++dx) {
      shadeAndWritePixel(frame, cx + dx, cy + dy, v.ndc.z, pixelColor);
    }
  }
}

// z culling
bool trianglePassesClipZ(const glm::vec3 &n0, const glm::vec3 &n1,
                         const glm::vec3 &n2) {
  const bool allBehindNear = (n0.z < -1.f && n1.z < -1.f && n2.z < -1.f);
  const bool allBeyondFar = (n0.z > 1.f && n1.z > 1.f && n2.z > 1.f);
  return !allBehindNear && !allBeyondFar;
}

// backface culling
bool isFrontFacingInNdc(const glm::vec3 &n0, const glm::vec3 &n1,
                        const glm::vec3 &n2, bool frontFaceClockwise) {
  const float signedAreaXY =
      (n1.x - n0.x) * (n2.y - n0.y) - (n1.y - n0.y) * (n2.x - n0.x);
  return frontFaceClockwise ? (signedAreaXY < 0.f) : (signedAreaXY > 0.f);
}
