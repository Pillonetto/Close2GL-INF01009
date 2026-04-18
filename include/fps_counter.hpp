#pragma once

#include <chrono>

/// Averages frame rate over fixed time windows (default 0.25 s).
class FpsCounter {
public:
  explicit FpsCounter(double averageIntervalSeconds = 0.25);

  /// Call once per frame; returns smoothed FPS (last computed value until the
  /// next window completes).
  float tick();

  float value() const { return displayFps_; }

private:
  using Clock = std::chrono::steady_clock;

  Clock::time_point lastTime_;
  double accumSeconds_ = 0.;
  int frameCount_ = 0;
  float displayFps_ = 0.f;
  double intervalSeconds_;
};
