#include "../../include/close_to_gl_camera.hpp"
#include "../../include/close_to_gl_model.hpp"
#include "../../include/software_rasterizer.hpp"
#include <GL3/gl3.h>
#include <GL3/gl3w.h>
#include <GLFW/glfw3.h>
#include <camera_gui.hpp>
#include <cstdlib>
#include <fps_counter.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <model-loader.hpp>
#include <open_gl_matrices.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

std::string readFile(const char *path) {
  std::ifstream inputStream(path, std::ios::binary);
  if (!inputStream)
    return {};
  return std::string(std::istreambuf_iterator<char>(inputStream), {});
}

// link vertex and fragment shaders to a program
GLuint linkProgram(const char *vertexShaderPath,
                   const char *fragmentShaderPath) {
  std::string vertexShaderSource = readFile(vertexShaderPath);
  std::string fragmentShaderSource = readFile(fragmentShaderPath);
  if (vertexShaderSource.empty() || fragmentShaderSource.empty()) {
    std::cerr << "Cant find shader files (vertex/fragment): "
              << vertexShaderPath << " / " << fragmentShaderPath << std::endl;
    return 0;
  }

  auto compileShader = [](GLenum shaderType,
                          const std::string &source) -> GLuint {
    GLuint shader = glCreateShader(shaderType);
    const char *sourcePointer = source.c_str();
    GLint sourceLength = static_cast<GLint>(source.size());
    glShaderSource(shader, 1, &sourcePointer, &sourceLength);
    glCompileShader(shader);

    GLint compileSucceeded = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileSucceeded);
    // error handling, delete shader and log error.
    if (!compileSucceeded) {
      char log[512];
      glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
      std::cerr << log << std::endl;
      glDeleteShader(shader);
      return 0;
    }
    return shader;
  };

  GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
  GLuint fragmentShader =
      compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
  if (!vertexShader || !fragmentShader) {
    if (vertexShader)
      glDeleteShader(vertexShader);
    if (fragmentShader)
      glDeleteShader(fragmentShader);
    return 0;
  }

  GLuint program = glCreateProgram();
  // attach shaders to program
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  // bind parameters to shader variables
  glBindAttribLocation(program, 0, "vPosition");
  glBindAttribLocation(program, 1, "vNormal");
  // attach program to gpu
  glLinkProgram(program);
  // delete shaders from gpu (linked to program already)
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  GLint linkSucceeded = 0;
  // check program link status
  glGetProgramiv(program, GL_LINK_STATUS, &linkSucceeded);
  if (!linkSucceeded) {
    char log[512];
    glGetProgramInfoLog(program, sizeof(log), nullptr, log);
    std::cerr << log << std::endl;
    glDeleteProgram(program);
    return 0;
  }
  return program;
}

// helps to center model in origin
void modelBounds(float &centerX, float &centerY, float &centerZ,
                 float &extent) {
  glm::vec3 minimum(Vert[0], Vert[1], Vert[2]);
  glm::vec3 maximum = minimum;
  const int floatsPerTriangle = 9;
  const int totalPositionFloats = NumTris * floatsPerTriangle;
  // starts at 3 because minimum is already set to first vertex
  for (int floatIndex = 3; floatIndex < totalPositionFloats; floatIndex += 3) {
    glm::vec3 position(Vert[floatIndex], Vert[floatIndex + 1],
                       Vert[floatIndex + 2]);
    minimum = glm::min(minimum, position);
    maximum = glm::max(maximum, position);
  }
  // center of model bounding box
  glm::vec3 center = 0.5f * (minimum + maximum);
  centerX = center.x;
  centerY = center.y;
  centerZ = center.z;
  // extent of model bounding box
  glm::vec3 boxDiagonal = maximum - minimum;
  extent = glm::max(glm::max(boxDiagonal.x, boxDiagonal.y), boxDiagonal.z);
}

} // namespace

int main(int argc, char **argv) {
  const char *modelPath = (argc > 1) ? argv[1] : "./models/cow_up.in";

  // loads NumTris, Vert, Vert_Normal
  loadModel(modelPath);
  if (NumTris <= 0 || !Vert || !Vert_Normal) {
    std::cerr << "Failed to load: " << modelPath << std::endl;
    return 1;
  }

  const int floatsPerVertex = 7; // position xyzw (w=1 object) + normal xyz
  // this array will have all the vertices and their normals (position + normal)
  std::vector<float> interleavedVertexData(static_cast<size_t>(NumTris) * 3 *
                                           floatsPerVertex);
  for (int triangleIndex = 0; triangleIndex < NumTris; ++triangleIndex) {
    // 9 floats per triangle (3 corners * 3 floats per corner)
    const int sourceFloatBase = triangleIndex * 9;
    // 21 floats per triangle (3 corners * 7 floats: xyzw + normal)
    const int destinationFloatBase = triangleIndex * 21;
    // 3 corners per triangle
    for (int cornerIndex = 0; cornerIndex < 3; ++cornerIndex) {
      const int sourceFloatOffset = sourceFloatBase + cornerIndex * 3;
      const int bufferFloatOffset =
          destinationFloatBase + cornerIndex * floatsPerVertex;
      std::memcpy(&interleavedVertexData[bufferFloatOffset],
                  &Vert[sourceFloatOffset], 3 * sizeof(float));
      interleavedVertexData[static_cast<size_t>(bufferFloatOffset) + 3u] = 1.f;
      std::memcpy(&interleavedVertexData[bufferFloatOffset + 4],
                  &Vert_Normal[sourceFloatOffset], 3 * sizeof(float));
    }
  }

  const std::vector<float> objectSpaceMesh = interleavedVertexData;

  // center of the models
  float centerX, centerY, centerZ, boundingExtent;
  // compute centers and extent radius
  modelBounds(centerX, centerY, centerZ, boundingExtent);
  // scale is arbitrary here
  const float modelScale = 1.8f / boundingExtent;

  // window control, ai-generated
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_DEPTH_BITS, 24);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  GLFWwindow *window = glfwCreateWindow(1024, 768, "CMP143", nullptr, nullptr);
  if (!window) {
    freeModelBuffers();
    return 1;
  }
  glfwMakeContextCurrent(window);
  if (gl3wInit() != 0) {
    freeModelBuffers();
    return 1;
  }

  GLFWwindow *cameraUiWindow = cameraGuiCreateWindow(window);
  if (!cameraUiWindow) {
    std::cerr << "Failed to create camera control window\n";
    freeModelBuffers();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  // initialize ImGui
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

  // Platform backend is bound to the dedicated control window (separate OS
  // window).
  glfwMakeContextCurrent(cameraUiWindow);
  ImGui_ImplGlfw_InitForOpenGL(cameraUiWindow, true);
  ImGui_ImplOpenGL3_Init("#version 150");
#ifdef __APPLE__
  // Realize the Cocoa drawable so the auxiliary window is composited (macOS).
  {
    glfwMakeContextCurrent(cameraUiWindow);
    int uw = 0, uh = 0;
    glfwGetFramebufferSize(cameraUiWindow, &uw, &uh);
    if (uw > 0 && uh > 0) {
      glViewport(0, 0, uw, uh);
      glClearColor(0.15f, 0.15f, 0.17f, 1.f);
      glClear(GL_COLOR_BUFFER_BIT);
      glfwSwapBuffers(cameraUiWindow);
    }
    cameraGuiMacRaiseWindow(cameraUiWindow);
    glfwPollEvents();
  }
#endif
  glfwMakeContextCurrent(window);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_PROGRAM_POINT_SIZE);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);

  // VAOs and VBOs boilerplate
  GLuint vertexArrayObject, vertexBufferObject;
  glGenVertexArrays(1, &vertexArrayObject);
  glGenBuffers(1, &vertexBufferObject);
  glBindVertexArray(vertexArrayObject);
  glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
  glBufferData(
      GL_ARRAY_BUFFER,
      static_cast<GLsizeiptr>(interleavedVertexData.size() * sizeof(float)),
      interleavedVertexData.data(), GL_STATIC_DRAW);

  // data is not useful anymore, we can clear it and free memory
  freeModelBuffers();
  interleavedVertexData.clear();
  interleavedVertexData.shrink_to_fit();

  // set up the vertex attributes
  const GLsizei vertexAttributeStrideBytes =
      static_cast<GLsizei>(floatsPerVertex * sizeof(float));

  // position (vec4: xyz + w; w=1 in object space, clip xyzw in Close2GL CPU
  // path)
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, vertexAttributeStrideBytes,
                        nullptr);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertexAttributeStrideBytes,
                        reinterpret_cast<const void *>(
                            static_cast<uintptr_t>(4 * sizeof(float))));
  glEnableVertexAttribArray(1);

  // load shader source from file
  GLuint shaderProgram = linkProgram("./triangles.vert", "./triangles.frag");
  if (!shaderProgram)
    return 1;
  GLuint close2GLProgram =
      linkProgram("./close_to_gl.vert", "./close_to_gl.frag");
  if (!close2GLProgram)
    return 1;

  // we will feed these values to the vertex and fragment shader
  GLint modelUniformLocation = glGetUniformLocation(shaderProgram, "uModel");
  GLint viewUniformLocation = glGetUniformLocation(shaderProgram, "uView");
  GLint projectionUniformLocation =
      glGetUniformLocation(shaderProgram, "uProjection");
  GLint colorUniformLocation = glGetUniformLocation(shaderProgram, "uColor");
  GLint pointSizeUniformLocation =
      glGetUniformLocation(shaderProgram, "uPointSize");
  GLint shadingModeUniformLocation =
      glGetUniformLocation(shaderProgram, "uShadingMode");
  GLint close2GlCpuClipVertexUniformLocation =
      glGetUniformLocation(shaderProgram, "uClose2GlCpuClipVertex");
  GLint lightPosEyeUniformLocation =
      glGetUniformLocation(shaderProgram, "uLightPosEye");
  GLint lightColorUniformLocation =
      glGetUniformLocation(shaderProgram, "uLightColor");
  GLint ambientUniformLocation =
      glGetUniformLocation(shaderProgram, "uAmbient");
  GLint kdUniformLocation = glGetUniformLocation(shaderProgram, "uKd");
  GLint ksUniformLocation = glGetUniformLocation(shaderProgram, "uKs");
  GLint shininessUniformLocation =
      glGetUniformLocation(shaderProgram, "uShininess");
  GLint rasterTextureLocation =
      glGetUniformLocation(close2GLProgram, "uRasterTexture");

  ModelAppearance appearance;
  CameraData camera;
  LightingParams lighting;

  // World origin is the model center after openGlModelMatrix (translate by
  // -bounds center).
  camera.lookAtTarget = glm::vec3(0.f);

  GLuint close2GLVao = 0;
  GLuint close2GLVertexVbo = 0;
  GLuint close2GLTexcoordVbo = 0;
  const float close2GLVertices[] = {
      -1.f, -1.f, 1.f, -1.f, 1.f, 1.f, 1.f, 1.f, -1.f, 1.f, -1.f, -1.f,
  };
  const float close2GLTexcoords[] = {
      0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 1.f, 1.f, 0.f, 1.f, 0.f, 0.f,
  };
  glGenVertexArrays(1, &close2GLVao);
  glBindVertexArray(close2GLVao);
  glGenBuffers(1, &close2GLVertexVbo);
  glBindBuffer(GL_ARRAY_BUFFER, close2GLVertexVbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(close2GLVertices), close2GLVertices,
               GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(0);
  glGenBuffers(1, &close2GLTexcoordVbo);
  glBindBuffer(GL_ARRAY_BUFFER, close2GLTexcoordVbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(close2GLTexcoords), close2GLTexcoords,
               GL_STATIC_DRAW);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(1);
  glBindVertexArray(0);

  glUseProgram(close2GLProgram);
  glUniform1i(rasterTextureLocation, 1);

  RasterFrame rasterFrame;

  FpsCounter fpsCounter;

  // draw loop
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    if (glfwWindowShouldClose(cameraUiWindow))
      glfwSetWindowShouldClose(window, 1);

    const float fpsDisplay = fpsCounter.tick();

    // Configiration window
    glfwMakeContextCurrent(cameraUiWindow);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    drawCameraTranslationGui(camera, appearance, fpsDisplay);
    ImGui::Render();

    int uiFbW = 0, uiFbH = 0;
    glfwGetFramebufferSize(cameraUiWindow, &uiFbW, &uiFbH);
    glViewport(0, 0, uiFbW, uiFbH);
    glClearColor(0.12f, 0.12f, 0.14f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(cameraUiWindow);

    // Main window (openGL context)
    glfwMakeContextCurrent(window);
    int framebufferWidth = 0, framebufferHeight = 0;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    float aspect = framebufferHeight > 0
                       ? float(framebufferWidth) / float(framebufferHeight)
                       : 1.f;

    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;

    if (appearance.close2GlMode) {
      model = buildModelMatrix(centerX, centerY, centerZ, modelScale);
      view = cameraViewMatrix(camera);
      projection = cameraProjectionMatrix(aspect, camera);

      if (framebufferWidth > 0 && framebufferHeight > 0) {
        ensureRasterFrameSize(rasterFrame, framebufferWidth, framebufferHeight);
        clearRasterFrame(rasterFrame, glm::vec4(0.08f, 0.08f, 0.1f, 1.f));

        const glm::mat4 modelView = view * model;
        const glm::mat3 normalMatrix =
            glm::mat3(glm::transpose(glm::inverse(modelView)));
        const bool wireframeMode = (appearance.drawMode == 1);
        const bool pointsMode = (appearance.drawMode == 2);
        const glm::vec3 baseColor = clamp01(appearance.colorsRgb);

        for (int tri = 0; tri < NumTris; ++tri) {
          RasterVertex rv[3];
          bool discardTriangle = false;
          for (int corner = 0; corner < 3; ++corner) {
            const size_t vi = static_cast<size_t>(tri * 3 + corner);
            const size_t o = vi * 7u;
            const glm::vec3 positionObject(objectSpaceMesh[o + 0],
                                           objectSpaceMesh[o + 1],
                                           objectSpaceMesh[o + 2]);
            const glm::vec3 normalObject(objectSpaceMesh[o + 4],
                                         objectSpaceMesh[o + 5],
                                         objectSpaceMesh[o + 6]);

            const glm::vec4 posEye4 =
                modelView * glm::vec4(positionObject, 1.f);
            const glm::vec4 clip = projection * posEye4;
            if (clip.w <= 0.f) {
              discardTriangle = true;
              break;
            }
            rv[corner].invW = 1.f / clip.w;
            rv[corner].ndc = glm::vec3(clip) * rv[corner].invW;
            rv[corner].screen = ndcToScreen(rv[corner].ndc, rasterFrame.width,
                                            rasterFrame.height);
            rv[corner].posEye = glm::vec3(posEye4) / posEye4.w;
            rv[corner].normalEye = glm::normalize(normalMatrix * normalObject);
          }
          if (discardTriangle)
            continue;
          if (!trianglePassesClipZ(rv[0].ndc, rv[1].ndc, rv[2].ndc))
            continue;
          if (!isFrontFacingInNdc(rv[0].ndc, rv[1].ndc, rv[2].ndc,
                                  appearance.frontFaceClockwise))
            continue;

          if (appearance.shadingMode == 1 || appearance.shadingMode == 2) {
            const bool includeSpec = (appearance.shadingMode == 2);
            rv[0].gouraudColor =
                evaluatePhongLighting(baseColor, rv[0].normalEye, rv[0].posEye,
                                      lighting, includeSpec);
            rv[1].gouraudColor =
                evaluatePhongLighting(baseColor, rv[1].normalEye, rv[1].posEye,
                                      lighting, includeSpec);
            rv[2].gouraudColor =
                evaluatePhongLighting(baseColor, rv[2].normalEye, rv[2].posEye,
                                      lighting, includeSpec);
          }

          if (pointsMode) {
            rasterizeVertexPoint(rv[0], appearance.shadingMode, baseColor,
                                 lighting, appearance.pointSize, rasterFrame);
            rasterizeVertexPoint(rv[1], appearance.shadingMode, baseColor,
                                 lighting, appearance.pointSize, rasterFrame);
            rasterizeVertexPoint(rv[2], appearance.shadingMode, baseColor,
                                 lighting, appearance.pointSize, rasterFrame);
          } else if (wireframeMode) {
            rasterizeWireTriangle(rv[0], rv[1], rv[2], appearance.shadingMode,
                                  baseColor, lighting, rasterFrame);
          } else {
            rasterizeSolidTriangle(rv[0], rv[1], rv[2], appearance.shadingMode,
                                   baseColor, lighting, rasterFrame);
          }
        }
      }

      glDisable(GL_DEPTH_TEST);
      glDisable(GL_CULL_FACE);
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      glClearColor(0.08f, 0.08f, 0.1f, 1.f);
      glClear(GL_COLOR_BUFFER_BIT);

      if (rasterFrame.textureId != 0 && rasterFrame.width > 0 &&
          rasterFrame.height > 0) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, rasterFrame.textureId);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rasterFrame.width,
                        rasterFrame.height, GL_RGBA, GL_UNSIGNED_BYTE,
                        rasterFrame.color.data());
      }
      glUseProgram(close2GLProgram);
      glBindVertexArray(close2GLVao);
      glDrawArrays(GL_TRIANGLES, 0, 6);
    } else {
      model = openGlModelMatrix(centerX, centerY, centerZ, modelScale);
      view = openGlViewMatrix(camera);
      projection = openGlProjectionMatrix(aspect, camera);

      glUseProgram(shaderProgram);
      glBindVertexArray(vertexArrayObject);
      glUniform1i(close2GlCpuClipVertexUniformLocation, 0);
      glUniformMatrix4fv(modelUniformLocation, 1, GL_FALSE,
                         glm::value_ptr(model));
      glUniformMatrix4fv(viewUniformLocation, 1, GL_FALSE,
                         glm::value_ptr(view));
      glUniformMatrix4fv(projectionUniformLocation, 1, GL_FALSE,
                         glm::value_ptr(projection));
      glUniform3f(lightPosEyeUniformLocation, lighting.lightPosEye.x,
                  lighting.lightPosEye.y, lighting.lightPosEye.z);
      glUniform3f(lightColorUniformLocation, lighting.lightColor.x,
                  lighting.lightColor.y, lighting.lightColor.z);
      glUniform1f(ambientUniformLocation, lighting.ambient);
      glUniform1f(kdUniformLocation, lighting.kd);
      glUniform1f(ksUniformLocation, lighting.ks);
      glUniform1f(shininessUniformLocation, lighting.shininess);

      glUniform1i(shadingModeUniformLocation, appearance.shadingMode);
      glUniform3f(colorUniformLocation, appearance.colorsRgb.x,
                  appearance.colorsRgb.y, appearance.colorsRgb.z);

      const bool pointsMode = (appearance.drawMode == 2);
      const bool wireframeMode = (appearance.drawMode == 1);
      glFrontFace(appearance.frontFaceClockwise ? GL_CW : GL_CCW);
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_CULL_FACE);
      glPolygonMode(GL_FRONT_AND_BACK, wireframeMode ? GL_LINE : GL_FILL);
      glLineWidth(1.f);
      glUniform1f(pointSizeUniformLocation,
                  pointsMode ? appearance.pointSize : 1.f);

      glClearColor(0.08f, 0.08f, 0.1f, 1.f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      const GLenum primitive = pointsMode ? GL_POINTS : GL_TRIANGLES;
      glDrawArrays(primitive, 0, NumTris * 3);
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    char mainTitle[80];
    std::snprintf(mainTitle, sizeof(mainTitle), "CMP143 | %.1f FPS",
                  static_cast<double>(fpsDisplay));
    glfwSetWindowTitle(window, mainTitle);

    glfwSwapBuffers(window);
  }

  glfwMakeContextCurrent(cameraUiWindow);
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  if (rasterFrame.textureId != 0)
    glDeleteTextures(1, &rasterFrame.textureId);
  glDeleteBuffers(1, &close2GLVertexVbo);
  glDeleteBuffers(1, &close2GLTexcoordVbo);
  glDeleteVertexArrays(1, &close2GLVao);
  glDeleteProgram(close2GLProgram);
  glDeleteProgram(shaderProgram);
  glDeleteBuffers(1, &vertexBufferObject);
  glDeleteVertexArrays(1, &vertexArrayObject);

  cameraGuiDestroyWindow(cameraUiWindow);
  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
