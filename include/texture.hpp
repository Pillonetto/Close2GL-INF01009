#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <vector>

enum TextureFilter {
  TEXTURE_FILTER_NEAREST = 0,
  TEXTURE_FILTER_BILINEAR = 1,
  TEXTURE_FILTER_TRILINEAR = 2,
};

struct Texture {
  struct Level {
    int width = 0;
    int height = 0;
    std::vector<glm::vec4> texels;
  };

  std::vector<Level> levels;
  bool valid = false;

  bool loadFromFile(const std::string &path);

  void clear();

  int baseWidth() const { return levels.empty() ? 0 : levels[0].width; }
  int baseHeight() const { return levels.empty() ? 0 : levels[0].height; }
  int maxLevel() const { return static_cast<int>(levels.size()) - 1; }

  glm::vec4 sampleLevelNearest(int level, float s, float t) const;
  glm::vec4 sampleLevelBilinear(int level, float s, float t) const;

  glm::vec4 sampleNearest(float s, float t) const {
    return sampleLevelNearest(0, s, t);
  }
  glm::vec4 sampleBilinear(float s, float t) const {
    return sampleLevelBilinear(0, s, t);
  }

  glm::vec4 sampleTrilinear(float s, float t, float lod) const;

  glm::vec4 sample(float s, float t, int filter, float lod) const;
};

#endif // TEXTURE_HPP
