#include <fps_counter.hpp>

FpsCounter::FpsCounter(double averageIntervalSeconds)
    : lastTime_(Clock::now()), intervalSeconds_(averageIntervalSeconds) {}

float FpsCounter::tick() {
  const auto now = Clock::now();
  const double dt = std::chrono::duration<double>(now - lastTime_).count();
  lastTime_ = now;
  accumSeconds_ += dt;
  frameCount_++;
  if (accumSeconds_ >= intervalSeconds_) {
    displayFps_ = static_cast<float>(
        static_cast<double>(frameCount_) / accumSeconds_);
    accumSeconds_ = 0.;
    frameCount_ = 0;
  }
  return displayFps_;
}
