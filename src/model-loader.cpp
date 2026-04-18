/*
Input example

Object name = <obj_name>
# triangles = <num_tri>
Material count = <material_count>
ambient color <r_a> <g_a> <b_a>
diffuse color <r_d> <g_d> <b_d>
specular color <r_s> <g_s> <b_s>
material shine <shine_coeff>
-- 3*[pos(x,y,z) normal(x,y,z) color_index] face_normal(x,y,z)
v0 <x> <y> <z> <Nx> <Ny> <Nz> <material_index>
v1 <x> <y> <z> <Nx> <Ny> <Nz> <material_index>
v2 <x> <y> <z> <Nx> <Ny> <Nz> <material_index>
face normal <FNx> <FNy> <FNz>
*/

// This model loader was built with AI-generated code. Input example was
// provided to LLM.

#include "model-loader.hpp"

#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Triangle {
  Vec3 A, B, C;
  Vec3 Norm[3];
  int mat[3]{0, 0, 0};
};

} // namespace

int NumTris = 0;
std::vector<Material> materials;
float *Vert = nullptr;
float *Vert_Normal = nullptr;

static std::string trim(std::string s) {
  size_t a = 0;
  while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a])))
    ++a;
  size_t b = s.size();
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])))
    --b;
  return s.substr(a, b - a);
}

static bool nextNonEmptyLine(std::ifstream &file, std::string &out) {
  std::string line;
  while (std::getline(file, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    line = trim(line);
    if (!line.empty()) {
      out = line;
      return true;
    }
  }
  return false;
}

static bool readVertexLine(const std::string &line, const char *prefix,
                           Vec3 &pos, Vec3 &nrm, int &matIdx) {
  std::istringstream iss(line);
  std::string tag;
  iss >> tag;
  if (tag != prefix)
    return false;
  if (!(iss >> pos.x >> pos.y >> pos.z >> nrm.x >> nrm.y >> nrm.z >> matIdx))
    return false;
  return true;
}

static bool readFaceNormalLine(const std::string &line, Vec3 &fn) {
  std::istringstream iss(line);
  std::string w1, w2;
  iss >> w1 >> w2;
  if (w1 != "face" || w2 != "normal")
    return false;
  return static_cast<bool>(iss >> fn.x >> fn.y >> fn.z);
}

void freeModelBuffers() {
  delete[] Vert;
  Vert = nullptr;
  delete[] Vert_Normal;
  Vert_Normal = nullptr;
}

void loadModel(const std::string &fileName) {
  freeModelBuffers();
  materials.clear();
  NumTris = 0;

  std::ifstream file(fileName);
  if (!file.is_open()) {
    std::cerr << "ERROR: unable to open model file: " << fileName << std::endl;
    return;
  }

  std::string line;
  int materialCount = -1;

  while (nextNonEmptyLine(file, line)) {
    if (line.find("Object name") != std::string::npos)
      continue;
    if (line.find("# triangles =") != std::string::npos) {
      size_t eq = line.find('=');
      if (eq != std::string::npos) {
        std::istringstream iss(line.substr(eq + 1));
        iss >> NumTris;
      }
      continue;
    }
    if (line.find("Material count =") != std::string::npos) {
      size_t eq = line.find('=');
      if (eq != std::string::npos) {
        std::istringstream iss(line.substr(eq + 1));
        iss >> materialCount;
      }
      break;
    }
  }

  if (materialCount < 0 || NumTris <= 0) {
    std::cerr << "ERROR: invalid model header in " << fileName << std::endl;
    NumTris = 0;
    return;
  }

  materials.reserve(static_cast<size_t>(materialCount));
  for (int mi = 0; mi < materialCount; ++mi) {
    Material m;
    auto readRgb = [&file, &line](const char *a, const char *b,
                                  Vec3 &out) -> bool {
      if (!nextNonEmptyLine(file, line))
        return false;
      std::istringstream iss(line);
      std::string w1, w2;
      iss >> w1 >> w2;
      if (w1 != a || w2 != b)
        return false;
      return static_cast<bool>(iss >> out.x >> out.y >> out.z);
    };
    if (!readRgb("ambient", "color", m.ambient) ||
        !readRgb("diffuse", "color", m.diffuse) ||
        !readRgb("specular", "color", m.specular) ||
        !nextNonEmptyLine(file, line)) {
      std::cerr << "ERROR: invalid material block in " << fileName << std::endl;
      materials.clear();
      NumTris = 0;
      return;
    }
    {
      std::istringstream iss(line);
      std::string w1, w2;
      iss >> w1 >> w2;
      if (w1 != "material" || w2 != "shine" || !(iss >> m.shine)) {
        std::cerr << "ERROR: invalid material block in " << fileName
                  << std::endl;
        materials.clear();
        NumTris = 0;
        return;
      }
    }
    materials.push_back(m);
  }

  while (nextNonEmptyLine(file, line)) {
    if (line.rfind("--", 0) == 0)
      break;
  }

  std::vector<Triangle> triangles(static_cast<size_t>(NumTris));
  for (int i = 0; i < NumTris; ++i) {
    Triangle &t = triangles[static_cast<size_t>(i)];
    if (!nextNonEmptyLine(file, line) ||
        !readVertexLine(line, "v0", t.A, t.Norm[0], t.mat[0]))
      goto parse_error;
    if (!nextNonEmptyLine(file, line) ||
        !readVertexLine(line, "v1", t.B, t.Norm[1], t.mat[1]))
      goto parse_error;
    if (!nextNonEmptyLine(file, line) ||
        !readVertexLine(line, "v2", t.C, t.Norm[2], t.mat[2]))
      goto parse_error;
    Vec3 faceNormal;
    if (!nextNonEmptyLine(file, line) || !readFaceNormalLine(line, faceNormal))
      goto parse_error;
    (void)faceNormal;

    for (int k = 0; k < 3; ++k) {
      int idx = t.mat[k];
      if (idx < 0 || idx >= materialCount) {
        std::cerr << "ERROR: material index out of range\n";
        goto parse_error;
      }
    }
  }

  Vert = new float[9 * NumTris];
  Vert_Normal = new float[9 * NumTris];
  for (int i = 0; i < NumTris; ++i) {
    const Triangle &tri = triangles[static_cast<size_t>(i)];
    Vert[9 * i] = tri.A.x;
    Vert[9 * i + 1] = tri.A.y;
    Vert[9 * i + 2] = tri.A.z;
    Vert[9 * i + 3] = tri.B.x;
    Vert[9 * i + 4] = tri.B.y;
    Vert[9 * i + 5] = tri.B.z;
    Vert[9 * i + 6] = tri.C.x;
    Vert[9 * i + 7] = tri.C.y;
    Vert[9 * i + 8] = tri.C.z;

    Vert_Normal[9 * i] = tri.Norm[0].x;
    Vert_Normal[9 * i + 1] = tri.Norm[0].y;
    Vert_Normal[9 * i + 2] = tri.Norm[0].z;
    Vert_Normal[9 * i + 3] = tri.Norm[1].x;
    Vert_Normal[9 * i + 4] = tri.Norm[1].y;
    Vert_Normal[9 * i + 5] = tri.Norm[1].z;
    Vert_Normal[9 * i + 6] = tri.Norm[2].x;
    Vert_Normal[9 * i + 7] = tri.Norm[2].y;
    Vert_Normal[9 * i + 8] = tri.Norm[2].z;
  }
  return;

parse_error:
  std::cerr << "ERROR: failed parsing triangle data in " << fileName
            << std::endl;
  freeModelBuffers();
  materials.clear();
  NumTris = 0;
}
