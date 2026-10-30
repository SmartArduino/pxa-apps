// SPDX-License-Identifier: CC0-1.0
#pragma once
#include "../common/pocket-imu/service.hpp"
#include "motion.hpp"
namespace marble_motion {
class Service : public pocket_imu::Service<Device> {
public:
  explicit Service(Device &d)
      : pocket_imu::Service<Device>(d, "Marble motion", "calibrated") {}
};
} // namespace marble_motion
