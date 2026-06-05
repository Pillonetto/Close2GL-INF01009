#pragma once

#include "texture.hpp"

#include <GL3/gl3.h>
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

struct LightingParams {
  glm::vec3 lightPosEye{2.f, 2.f, 2.f};
  glm::vec3 lightColor{1.f, 1.f, 1.f};
  float ambient = 0.12f;
  float kd = 0.85f;
  float ks = 0.45f;
  float shininess = 48.f;
};

struct RasterVertex {
  glm::vec2 screen{0.f};
  glm::vec3 ndc{0.f};
  glm::vec3 normalEye{0.f};
  glm::vec3 posEye{0.f};
  glm::vec3 gouraudColor{0.f};
  glm::vec3 gouraudSpecular{0.f};
  glm::vec2 texCoord{0.f};
  float invW = 0.f;
};

struct TextureState {
  const Texture *texture = nullptr;
  bool enabled = false;
  int filter = TEXTURE_FILTER_NEAREST;
};

struct RasterFrame {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> color;
  std::vector<float> depth;
  GLuint textureId = 0;
};

glm::vec3 clamp01(const glm::vec3 &c);

glm::vec2 ndcToScreen(const glm::vec3 &ndc, int width, int height);

glm::vec3 evaluatePhongLighting(const glm::vec3 &baseColor,
                                const glm::vec3 &normalEye,
                                const glm::vec3 &posEye,
                                const LightingParams &light,
                                bool includeSpecular);

void evaluatePhongComponents(const glm::vec3 &baseColor,
                             const glm::vec3 &normalEye,
                             const glm::vec3 &posEye,
                             const LightingParams &light, bool includeSpecular,
                             glm::vec3 &ambientDiffuse, glm::vec3 &specular);

void ensureRasterFrameSize(RasterFrame &frame, int width, int height);
void clearRasterFrame(RasterFrame &frame, const glm::vec4 &clearColor);

void rasterizeSolidTriangle(const RasterVertex &v0, const RasterVertex &v1,
                            const RasterVertex &v2, int shadingMode,
                            const glm::vec3 &baseColor,
                            const LightingParams &light,
                            const TextureState &tex, RasterFrame &frame);

void rasterizeWireTriangle(const RasterVertex &v0, const RasterVertex &v1,
                           const RasterVertex &v2, int shadingMode,
                           const glm::vec3 &baseColor,
                           const LightingParams &light, const TextureState &tex,
                           RasterFrame &frame);

void rasterizeVertexPoint(const RasterVertex &v, int shadingMode,
                          const glm::vec3 &baseColor,
                          const LightingParams &light, const TextureState &tex,
                          float pointSize, RasterFrame &frame);

bool trianglePassesClipZ(const glm::vec3 &n0, const glm::vec3 &n1,
                         const glm::vec3 &n2);

bool isFrontFacingInNdc(const glm::vec3 &n0, const glm::vec3 &n1,
                        const glm::vec3 &n2, bool frontFaceClockwise);
