#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

struct GLFWwindow;

struct CameraData {
  glm::vec3 position{0.f, 0.f, 3.5f};
  glm::vec3 lookAtTarget{0.f, 0.f, 0.f};
  // Roll around the forward axis
  float roll = 0.f;
  float zNear = 1.0f;
  float zFar = 3000.f;
};

// Single RGB + rasterization style
struct ModelAppearance {
  glm::vec3 colorsRgb{0.7f, 0.7f, 0.7f};
  // 0 = flat (uniform color), 1 = Gouraud A+D, 2 = Gouraud A+D+S, 3 = Phong A+D+S.
  int shadingMode = 0;
  // 0 = filled triangles, 1 = wireframe, 2 = points at each vertex position.
  int drawMode = 0;
  float pointSize = 8.f;
  // false = GL_CCW (default), true = GL_CW
  bool frontFaceClockwise = false;
  // false = OpenGL pipeline, true = Close2GL pipeline (toggle only; wire in main).
  bool close2GlMode = false;
};

#include "open_gl_matrices.hpp"

// Dedicated GLFW window for UI (shared OpenGL context with the main window).
// Places the window to the right of the main window so it is not covered.
// Call after the main window exists and its context is current; gl3w must be
// initialized.
GLFWwindow *cameraGuiCreateWindow(GLFWwindow *shareContextWindow);
void cameraGuiDestroyWindow(GLFWwindow *uiWindow);

#if defined(__APPLE__)
void cameraGuiMacRaiseWindow(GLFWwindow *uiWindow);
#endif

// Draw the full-window control panel (call with the UI window’s GL context
// current). `framesPerSecond` is shown at the top (smoothed by the caller).
void drawCameraTranslationGui(CameraData &cam, ModelAppearance &appearance,
                                float framesPerSecond);
