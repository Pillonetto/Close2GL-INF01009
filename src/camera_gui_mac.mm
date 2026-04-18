// macOS: secondary GLFW/NSOpenGL windows often stay hidden behind the main
// window or on another Space. Bring the NSWindow forward explicitly.

#include "camera_gui.hpp"

#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_COCOA
#define GLFW_EXPOSE_NATIVE_NSGL
#include <GLFW/glfw3native.h>

#import <Cocoa/Cocoa.h>

void cameraGuiMacRaiseWindow(GLFWwindow *w) {
  if (!w)
    return;
  id handle = glfwGetCocoaWindow(w);
  if (!handle)
    return;
  NSWindow *nsw = (NSWindow *)handle;
  // Follow the user to the current Space and stack above other app windows.
  if ([nsw respondsToSelector:@selector(setCollectionBehavior:)]) {
    NSWindowCollectionBehavior b = [nsw collectionBehavior];
    b |= NSWindowCollectionBehaviorMoveToActiveSpace;
    [nsw setCollectionBehavior:b];
  }
  [nsw orderFrontRegardless];
  [nsw makeKeyWindow];
}
