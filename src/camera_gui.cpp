#include "camera_gui.hpp"

#include "imgui.h"

#include <GLFW/glfw3.h>
#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace {

constexpr float kMinEyeToTarget = 0.05f;

void enforceMinDistance(CameraData &cam) {
  glm::vec3 toTarget = cam.lookAtTarget - cam.position;
  float d = glm::length(toTarget);
  if (d < kMinEyeToTarget) {
    cam.position = cam.lookAtTarget - toTarget * (kMinEyeToTarget / d);
  }
}

void rotateViewAroundAxis(CameraData &cam, const glm::vec3 &axisUnit,
                          float radians) {
  glm::vec3 toTarget = cam.lookAtTarget - cam.position;
  const glm::vec3 front = glm::normalize(toTarget);

  // Rotate the front vector around the axis unit vector by the given angle.
  const glm::vec3 newFront = glm::vec3(
      glm::rotate(glm::mat4(1.f), radians, axisUnit) * glm::vec4(front, 0.f));
  // New front vector is the new look-at target.
  cam.lookAtTarget = cam.position + glm::normalize(newFront);
}

} // namespace

GLFWwindow *cameraGuiCreateWindow(GLFWwindow *shareContextWindow) {
  glfwWindowHint(GLFW_VISIBLE, 1);
#if defined(__APPLE__)
  GLFWwindow *w =
      glfwCreateWindow(440, 560, "Camera controls", nullptr, nullptr);
#else
  GLFWwindow *w = glfwCreateWindow(440, 560, "Camera controls", nullptr,
                                   shareContextWindow);
#endif
  if (!w)
    return nullptr;

  int mainX = 0, mainY = 0, mainW = 0, mainH = 0;
  glfwGetWindowPos(shareContextWindow, &mainX, &mainY);
  glfwGetWindowSize(shareContextWindow, &mainW, &mainH);
  constexpr int kGapPx = 48;
  glfwSetWindowPos(w, mainX + mainW + kGapPx, mainY);

  glfwShowWindow(w);
  glfwPollEvents();

#if defined(__APPLE__)
  cameraGuiMacRaiseWindow(w);
  glfwPollEvents();
#endif

  return w;
}

void cameraGuiDestroyWindow(GLFWwindow *uiWindow) {
  if (uiWindow)
    glfwDestroyWindow(uiWindow);
}

namespace {

void clampRgb01(glm::vec3 &rgb) {
  rgb.x = glm::clamp(rgb.x, 0.f, 1.f);
  rgb.y = glm::clamp(rgb.y, 0.f, 1.f);
  rgb.z = glm::clamp(rgb.z, 0.f, 1.f);
}

} // namespace

void drawCameraTranslationGui(CameraData &cam, ModelAppearance &appearance,
                              float framesPerSecond) {

  const ImGuiIO &io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(0.f, 0.f));
  ImGui::SetNextWindowSize(io.DisplaySize);

  constexpr ImGuiWindowFlags kRootFlags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
      ImGuiWindowFlags_NoDecoration;

  ImGui::Begin("CameraControlsRoot", nullptr, kRootFlags);

  ImGui::TextColored(ImVec4(0.95f, 0.92f, 0.55f, 1.f), "FPS: %.1f",
                     framesPerSecond);
  ImGui::Separator();
  ImGui::TextUnformatted("Model color");
  ImGui::ColorEdit3("RGB", &appearance.colorsRgb.x,
                    ImGuiColorEditFlags_Float | ImGuiColorEditFlags_DisplayRGB);
  clampRgb01(appearance.colorsRgb);
  const char *shadingItems[] = {
      "Flat (uniform color)",
      "Gouraud: ambient + diffuse",
      "Gouraud: ambient + diffuse + specular",
      "Phong: ambient + diffuse + specular",
  };
  ImGui::Combo("Shading", &appearance.shadingMode, shadingItems,
               IM_ARRAYSIZE(shadingItems));
  if (appearance.shadingMode < 0)
    appearance.shadingMode = 0;
  if (appearance.shadingMode > 3)
    appearance.shadingMode = 3;
  const char *modeItems[] = {"Filled triangles", "Wireframe", "Points"};
  ImGui::Combo("Draw mode", &appearance.drawMode, modeItems,
               IM_ARRAYSIZE(modeItems));
  if (appearance.drawMode < 0)
    appearance.drawMode = 0;
  if (appearance.drawMode > 2)
    appearance.drawMode = 2;
  ImGui::SliderFloat("Point size (pixels)", &appearance.pointSize, 1.f, 32.f,
                     "%.0f");
  if (appearance.pointSize < 1.f)
    appearance.pointSize = 1.f;
  if (appearance.pointSize > 64.f)
    appearance.pointSize = 64.f;
  ImGui::TextDisabled(
      "Points: one sprite per buffered corner; wireframe: GL_LINE triangles.");
  ImGui::Checkbox("Front face: clockwise (GL_CW)",
                  &appearance.frontFaceClockwise);

  ImGui::Separator();
  ImGui::TextUnformatted("Rendering pipeline");
  if (ImGui::RadioButton("OpenGL", !appearance.close2GlMode))
    appearance.close2GlMode = false;
  ImGui::SameLine();
  if (ImGui::RadioButton("Close2GL", appearance.close2GlMode))
    appearance.close2GlMode = true;

  ImGui::Separator();
  ImGui::TextUnformatted("Camera (look-at model center)");
  ImGui::Separator();

  ImGui::TextUnformatted(
      "Look-at is the object center in world space (origin after centering).");
  ImGui::InputFloat3("Look-at (world)", &cam.lookAtTarget.x, "%.4f",
                     ImGuiInputTextFlags_None);

  ImGui::Spacing();
  ImGui::TextUnformatted("Eye position (world)");
  ImGui::InputFloat3("Eye##pos", &cam.position.x, "%.4f",
                     ImGuiInputTextFlags_None);
  enforceMinDistance(cam);

  const glm::vec3 right = openGlCameraRight(cam);
  const glm::vec3 up = openGlCameraUp(cam);
  const glm::vec3 forward = openGlCameraForward(cam);

  ImGui::Separator();
  ImGui::TextUnformatted("Camera axes (toward target = forward)");
  ImGui::Text("Right:   %.3f  %.3f  %.3f", right.x, right.y, right.z);
  ImGui::Text("Up:      %.3f  %.3f  %.3f", up.x, up.y, up.z);
  ImGui::Text("Forward: %.3f  %.3f  %.3f", forward.x, forward.y, forward.z);

  ImGui::Separator();
  ImGui::TextUnformatted("Translate along camera axes (keeps look-at)");
  static float step = 0.15f;
  ImGui::InputFloat("Move step", &step, 0.01f, 0.05f, "%.3f");
  if (step < 0.001f)
    step = 0.001f;

  auto localAxisRow = [&](const char *label, const glm::vec3 &axis) {
    ImGui::PushID(label);
    if (ImGui::SmallButton("-")) {
      cam.position -= axis * step;
      enforceMinDistance(cam);
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    if (ImGui::SmallButton("+")) {
      cam.position += axis * step;
      enforceMinDistance(cam);
    }
    ImGui::PopID();
  };

  localAxisRow("Right (local X)", right);
  localAxisRow("Up (local Y)", up);
  localAxisRow("Forward / back (local Z)", forward);

  ImGui::Separator();
  ImGui::TextUnformatted(
      "Rotate around camera axes (eye fixed; updates look-at direction)");
  static float rotStep = 0.08f;
  ImGui::InputFloat("Rotate step (rad)", &rotStep, 0.01f, 0.05f, "%.3f");
  if (rotStep < 0.001f)
    rotStep = 0.001f;

  auto rotAxisRow = [&](const char *label, const glm::vec3 &axis,
                        void (*apply)(CameraData &, const glm::vec3 &, float)) {
    ImGui::PushID(label);
    if (ImGui::SmallButton("-")) {
      apply(cam, axis, -rotStep);
      enforceMinDistance(cam);
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    if (ImGui::SmallButton("+")) {
      apply(cam, axis, rotStep);
      enforceMinDistance(cam);
    }
    ImGui::PopID();
  };

  rotAxisRow("Pitch (around local X / right)", right,
             [](CameraData &c, const glm::vec3 &ax, float a) {
               rotateViewAroundAxis(c, ax, a);
             });
  rotAxisRow("Yaw (around local Y / up)", up,
             [](CameraData &c, const glm::vec3 &ax, float a) {
               rotateViewAroundAxis(c, ax, a);
             });
  rotAxisRow("Roll (around local Z / forward)", forward,
             [](CameraData &c, const glm::vec3 &, float a) { c.roll += a; });

  ImGui::InputFloat("Roll angle (rad)", &cam.roll, 0.01f, 0.1f, "%.4f");

  ImGui::Separator();
  ImGui::TextUnformatted("Projection (perspective clipping)");
  ImGui::InputFloat("Near plane", &cam.zNear, 0.001f, 0.05f, "%.4f");
  ImGui::InputFloat("Far plane", &cam.zFar, 1.f, 50.f, "%.2f");
  sanitizeOpenGlPerspectiveClipPlanes(cam.zNear, cam.zFar, cam.zNear, cam.zFar);

  ImGui::Separator();
  if (ImGui::Button("Reset camera")) {
    cam.position = glm::vec3(0.f, 0.f, 3.5f);
    cam.lookAtTarget = glm::vec3(0.f, 0.f, 0.f);
    cam.roll = 0.f;
    cam.zNear = 1.0f;
    cam.zFar = 3000.f;
  }

  ImGui::End();
}
