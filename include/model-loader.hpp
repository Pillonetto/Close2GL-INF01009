#ifndef MODEL_LOADER_HPP
#define MODEL_LOADER_HPP

#include <string>
#include <vector>

struct Vec3 {
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
};

struct Material {
  Vec3 ambient;
  Vec3 diffuse;
  Vec3 specular;
  float shine = 0.f;
};

extern int NumTris;
extern std::vector<Material> materials;
// array with (x,y,z) for each vertex (3 per triangle) (9 floats per triangle)
extern float *Vert;
// array with (x,y,z) for each vertex normal (3 per triangle) (9 floats per
// triangle)
extern float *Vert_Normal;

void loadModel(const std::string &fileName);
void freeModelBuffers();

#endif
