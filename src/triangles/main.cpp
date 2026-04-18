#include "../../include/close_to_gl_camera.hpp"
#include "../../include/close_to_gl_model.hpp"
#include <GL3/gl3.h>
#include <GL3/gl3w.h>
#include <GLFW/glfw3.h>
#include <camera_gui.hpp>
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
  std::vector<float> close2GlUploadMesh(objectSpaceMesh.size());

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
  glUseProgram(shaderProgram);

  // we will feed these values to the vertex and fragment shader
  GLint modelViewUniformLocation =
      glGetUniformLocation(shaderProgram, "uModelView");
  GLint projectionUniformLocation =
      glGetUniformLocation(shaderProgram, "uProjection");
  GLint normalMatrixUniformLocation =
      glGetUniformLocation(shaderProgram, "uNormalMatrix");
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

  ModelAppearance appearance;
  CameraData camera;

  // World origin is the model center after openGlModelMatrix (translate by
  // -bounds center).
  camera.lookAtTarget = glm::vec3(0.f);

  glUseProgram(shaderProgram);
  glBindVertexArray(vertexArrayObject);

  FpsCounter fpsCounter;
  static bool prevClose2GlMode = false;

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

    printf("MODEL centers: (%f, %f, %f), scale: %f\n", centerX, centerY,
           centerZ, modelScale);

    // Gwt MVP matrices
    if (appearance.close2GlMode) {
      model = buildModelMatrix(centerX, centerY, centerZ, modelScale);
      view = cameraViewMatrix(camera);
      projection = cameraProjectionMatrix(aspect, camera);
    } else {
      model = openGlModelMatrix(centerX, centerY, centerZ, modelScale);
      view = openGlViewMatrix(camera);
      projection = openGlProjectionMatrix(aspect, camera);
    }

    const glm::mat4 modelView = view * model;
    // Apr 16th lecture
    const glm::mat3 normalMatrix =
        glm::transpose(glm::inverse(glm::mat3(modelView)));

    const int numVerts = NumTris * 3;

    if (appearance.close2GlMode) {
      for (int vi = 0; vi < numVerts; ++vi) {
        const size_t o = static_cast<size_t>(vi) * 7u;
        const glm::vec3 position(objectSpaceMesh[o], objectSpaceMesh[o + 1],
                                 objectSpaceMesh[o + 2]);
        const glm::vec4 clip =
            projection * modelView * glm::vec4(position, 1.f);
        const float w = clip.w;
        const glm::vec3 ndc =
            (w != 0.f) ? glm::vec3(clip.x / w, clip.y / w, clip.z / w)
                       : glm::vec3(clip.x, clip.y, clip.z);
        close2GlUploadMesh[o + 0] = ndc.x;
        close2GlUploadMesh[o + 1] = ndc.y;
        close2GlUploadMesh[o + 2] = ndc.z;
        close2GlUploadMesh[o + 3] = w;
        close2GlUploadMesh[o + 4] = objectSpaceMesh[o + 4];
        close2GlUploadMesh[o + 5] = objectSpaceMesh[o + 5];
        close2GlUploadMesh[o + 6] = objectSpaceMesh[o + 6];
      }
      glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
      glBufferSubData(
          GL_ARRAY_BUFFER, 0,
          static_cast<GLsizeiptr>(close2GlUploadMesh.size() * sizeof(float)),
          close2GlUploadMesh.data());
      glUniform1i(close2GlCpuClipVertexUniformLocation, 1);
      const glm::mat4 identity(1.f);
      glUniformMatrix4fv(modelViewUniformLocation, 1, GL_FALSE,
                         glm::value_ptr(identity));
    } else {
      glUniform1i(close2GlCpuClipVertexUniformLocation, 0);
      if (prevClose2GlMode) {
        glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
        glBufferSubData(
            GL_ARRAY_BUFFER, 0,
            static_cast<GLsizeiptr>(objectSpaceMesh.size() * sizeof(float)),
            objectSpaceMesh.data());
      }
      glUniformMatrix4fv(modelViewUniformLocation, 1, GL_FALSE,
                         glm::value_ptr(modelView));
    }
    prevClose2GlMode = appearance.close2GlMode;

    glUniformMatrix4fv(projectionUniformLocation, 1, GL_FALSE,
                       glm::value_ptr(projection));
    glUniformMatrix3fv(normalMatrixUniformLocation, 1, GL_FALSE,
                       glm::value_ptr(normalMatrix));

    const glm::vec3 lightWorld(4.f, 6.f, 5.f);
    const glm::vec3 lightPosEye = glm::vec3(view / glm::vec4(lightWorld, 1.f));
    glUniform3f(lightPosEyeUniformLocation, lightPosEye.x, lightPosEye.y,
                lightPosEye.z);
    glUniform3f(lightColorUniformLocation, 1.f, 1.f, 1.f);
    glUniform1f(ambientUniformLocation, 0.12f);
    glUniform1f(kdUniformLocation, 0.85f);
    glUniform1f(ksUniformLocation, 0.45f);
    glUniform1f(shininessUniformLocation, 48.f);

    glUniform1i(shadingModeUniformLocation, appearance.shadingMode);
    glUniform3f(colorUniformLocation, appearance.colorsRgb.x,
                appearance.colorsRgb.y, appearance.colorsRgb.z);

    // set drawing mode
    const bool pointsMode = (appearance.drawMode == 2);
    const bool wireframeMode = (appearance.drawMode == 1);

    glFrontFace(appearance.frontFaceClockwise ? GL_CW : GL_CCW);

    // set polygon mode and coloring mode
    glPolygonMode(GL_FRONT_AND_BACK, wireframeMode ? GL_LINE : GL_FILL);
    glLineWidth(1.f);
    glUniform1f(pointSizeUniformLocation,
                pointsMode ? appearance.pointSize : 1.f);

    glClearColor(0.08f, 0.08f, 0.1f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const GLenum primitive = pointsMode ? GL_POINTS : GL_TRIANGLES;
    glDrawArrays(primitive, 0, NumTris * 3);

    // reset polygon mode
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

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

  cameraGuiDestroyWindow(cameraUiWindow);
  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
