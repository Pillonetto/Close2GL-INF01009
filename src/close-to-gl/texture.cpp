#include "texture.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

float clamp01(float v) { return clampInt(v, 0.f, 1.f); }

} // namespace

void Texture::clear() {
  levels.clear();
  valid = false;
}

bool Texture::loadFromFile(const std::string &path) {
  clear();

  // OpenGL (s,t)=(0,0) is bottom-left; stb_image rows are top-to-bottom.
  stbi_set_flip_vertically_on_load(1);

  int width = 0, height = 0, channels = 0;
  unsigned char *pixels =
      stbi_load(path.c_str(), &width, &height, &channels, 4);
  if (!pixels || width <= 0 || height <= 0) {
    std::cerr << "ERROR: failed to load texture image: " << path << std::endl;
    if (pixels)
      stbi_image_free(pixels);
    return false;
  }

  Level base;
  base.width = width;
  base.height = height;
  base.texels.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
  const float inv255 = 1.f / 255.f;
  for (size_t i = 0; i < base.texels.size(); ++i) {
    unsigned char *p = pixels + i * 4u;
    base.texels[i] =
        glm::vec4(p[0] * inv255, p[1] * inv255, p[2] * inv255, p[3] * inv255);
  }
  stbi_image_free(pixels);

  levels.push_back(std::move(base));

  while (levels.back().width > 1 || levels.back().height > 1) {
    const Level &src = levels.back();
    Level dst;
    dst.width = std::max(1, src.width / 2);
    dst.height = std::max(1, src.height / 2);
    dst.texels.resize(static_cast<size_t>(dst.width) *
                      static_cast<size_t>(dst.height));

    for (int y = 0; y < dst.height; ++y) {
      for (int x = 0; x < dst.width; ++x) {
        const int sx0 = std::min(2 * x, src.width - 1);
        const int sx1 = std::min(2 * x + 1, src.width - 1);
        const int sy0 = std::min(2 * y, src.height - 1);
        const int sy1 = std::min(2 * y + 1, src.height - 1);
        const glm::vec4 sum =
            src.texels[static_cast<size_t>(sy0) * src.width + sx0] +
            src.texels[static_cast<size_t>(sy0) * src.width + sx1] +
            src.texels[static_cast<size_t>(sy1) * src.width + sx0] +
            src.texels[static_cast<size_t>(sy1) * src.width + sx1];
        int index = y * dst.width + x;
        dst.texels[static_cast<size_t>(index)] = sum * 0.25f;
      }
    }
    levels.push_back(std::move(dst));
  }

  valid = true;
  return true;
}

glm::vec4 Texture::sampleLevelNearest(int level, float s, float t) const {
  if (levels.empty())
    return glm::vec4(1.f);
  level = clampInt(level, 0, maxLevel());
  const Level &lvl = levels[static_cast<size_t>(level)];

  s = clamp01(s);
  t = clamp01(t);
  int x = static_cast<int>(s * static_cast<float>(lvl.width));
  int y = static_cast<int>(t * static_cast<float>(lvl.height));
  x = clampInt(x, 0, lvl.width - 1);
  y = clampInt(y, 0, lvl.height - 1);
  return lvl.texels[static_cast<size_t>(y) * lvl.width + x];
}

glm::vec4 Texture::sampleLevelBilinear(int level, float s, float t) const {
  if (levels.empty())
    return glm::vec4(1.f);
  level = clampInt(level, 0, maxLevel());
  const Level &lvl = levels[static_cast<size_t>(level)];

  s = clamp01(s);
  t = clamp01(t);

  const float fx = s * static_cast<float>(lvl.width) - 0.5f;
  const float fy = t * static_cast<float>(lvl.height) - 0.5f;
  const int x0 = static_cast<int>(std::floor(fx));
  const int y0 = static_cast<int>(std::floor(fy));
  const float ax = fx - static_cast<float>(x0);
  const float ay = fy - static_cast<float>(y0);

  const int x0c = clampInt(x0, 0, lvl.width - 1);
  const int x1c = clampInt(x0 + 1, 0, lvl.width - 1);
  const int y0c = clampInt(y0, 0, lvl.height - 1);
  const int y1c = clampInt(y0 + 1, 0, lvl.height - 1);

  const glm::vec4 c00 = lvl.texels[static_cast<size_t>(y0c) * lvl.width + x0c];
  const glm::vec4 c10 = lvl.texels[static_cast<size_t>(y0c) * lvl.width + x1c];
  const glm::vec4 c01 = lvl.texels[static_cast<size_t>(y1c) * lvl.width + x0c];
  const glm::vec4 c11 = lvl.texels[static_cast<size_t>(y1c) * lvl.width + x1c];

  const glm::vec4 top = c00 * (1.f - ax) + c10 * ax;
  const glm::vec4 bottom = c01 * (1.f - ax) + c11 * ax;
  return top * (1.f - ay) + bottom * ay;
}

glm::vec4 Texture::sampleTrilinear(float s, float t, float lod) const {
  if (levels.empty())
    return glm::vec4(1.f);
  const int maxLvl = maxLevel();
  if (lod <= 0.f)
    return sampleLevelBilinear(0, s, t);
  if (lod >= static_cast<float>(maxLvl))
    return sampleLevelBilinear(maxLvl, s, t);

  const int lo = static_cast<int>(std::floor(lod));
  const int hi = std::min(lo + 1, maxLvl);
  const float frac = lod - static_cast<float>(lo);
  const glm::vec4 a = sampleLevelBilinear(lo, s, t);
  const glm::vec4 b = sampleLevelBilinear(hi, s, t);
  return a * (1.f - frac) + b * frac;
}

glm::vec4 Texture::sample(float s, float t, int filter, float lod) const {
  switch (filter) {
  case TEXTURE_FILTER_NEAREST:
    return sampleLevelNearest(0, s, t);
  case TEXTURE_FILTER_BILINEAR:
    return sampleLevelBilinear(0, s, t);
  case TEXTURE_FILTER_TRILINEAR:
    return sampleTrilinear(s, t, lod);
  default:
    return sampleLevelBilinear(0, s, t);
  }
}
