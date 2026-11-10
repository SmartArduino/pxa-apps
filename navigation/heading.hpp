// SPDX-License-Identifier: MIT
#pragma once

#include "../compass/model.hpp"
#include <array>
#include <cstdint>
#include <string_view>

namespace navigation {

// Gyroscope samples rotate vectors in the device frame. Magnetic observations
// remain the absolute reference, including while angular samples are arriving.
struct HeadingFusion {
    compass::Vec field{}, gravity{}, last_velocity{};
    std::uint64_t pose_time = 0, angular_time = 0;
    bool valid = false;

    void reset() { *this = {}; }

    static compass::Vec rotate_body(compass::Vec value, compass::Vec angular,
                                    float seconds) {
        const float speed = compass::norm(angular);
        if (speed < 1e-6f) return value;
        const auto axis = angular * (1 / speed);
        const float angle = -speed * seconds;
        const float cosine = std::cos(angle), sine = std::sin(angle);
        return value * cosine + compass::cross(axis, value) * sine +
               axis * (compass::dot(axis, value) * (1 - cosine));
    }

    void absolute(compass::Vec magnetic, compass::Vec up, std::uint64_t stamp) {
        const auto separation = stamp > angular_time ? stamp - angular_time : angular_time - stamp;
        const bool assisted = valid && angular_time && separation <= 150000;
        if (assisted) {
            // A coalesced observation can arrive after a newer angular sample.
            // Bring it into that pose before correcting the predicted vectors.
            if (stamp < pose_time) {
                const float seconds = std::min<std::uint64_t>(pose_time - stamp, 150000) / 1000000.f;
                magnetic = rotate_body(magnetic, last_velocity, seconds);
                up = rotate_body(up, last_velocity, seconds);
            }
            field = field * .75f + magnetic * .25f;
            gravity = compass::unit(gravity * .75f + compass::unit(up) * .25f);
            pose_time = std::max(pose_time, stamp);
        } else {
            field = magnetic;
            gravity = compass::unit(up);
            pose_time = stamp;
            angular_time = 0;
        }
        valid = true;
    }

    bool angular(compass::Vec velocity, std::uint64_t stamp) {
        if (!valid || !compass::finite(velocity) || compass::norm(velocity) > 40 ||
            stamp <= pose_time || stamp <= angular_time) return false;
        const auto elapsed = stamp - pose_time;
        if (elapsed > 150000) {
            valid = false;
            angular_time = 0;
            return false;
        }
        angular_time = stamp;
        last_velocity = velocity;
        const float seconds = elapsed / 1000000.f;
        field = rotate_body(field, velocity, seconds);
        gravity = compass::unit(rotate_body(gravity, velocity, seconds));
        pose_time = stamp;
        return true;
    }

    bool readable() const {
        if (!valid || !compass::finite(field) || !compass::finite(gravity)) return false;
        const auto reference = compass::forward_reference(gravity);
        if (compass::norm(reference) < .26f) return false;
        const auto top = compass::unit(reference);
        return std::hypot(compass::dot(field, top),
                          compass::dot(field, compass::cross(gravity, top))) >= 5;
    }

    float bearing() const {
        const auto top = compass::unit(compass::forward_reference(gravity));
        const auto left = compass::cross(gravity, top);
        return compass::wrap(std::atan2(compass::dot(field, left),
                                       compass::dot(field, top)) * compass::DEG);
    }
};

struct Heading {
    compass::Model model;
    HeadingFusion fusion;
    std::uint64_t absolute_time = 0, display_time = 0;
    float shown = 0;
    bool visible = false;

    void suspend() {
        model.suspend();
        model.has_heading = false;
        fusion.reset();
        absolute_time = display_time = 0;
        visible = false;
    }

    bool gyroscope_active(std::uint64_t now) const {
        return visible && !model.system_heading && fusion.angular_time &&
               now >= fusion.angular_time && now - fusion.angular_time <= 250000;
    }

    bool accept_time(std::uint64_t stamp, std::uint64_t now) const {
        return stamp && stamp <= now && now - stamp < 1500000;
    }

    void magnetic(const std::array<std::int32_t, 3>& values,
                  std::uint64_t stamp, std::uint64_t now) {
        if (accept_time(stamp, now)) model.tilt.acceleration(values.data(), stamp, now);
    }

    void acceleration(const std::array<std::int32_t, 3>& values,
                      std::uint64_t stamp, std::uint64_t now) {
        if (accept_time(stamp, now)) model.tilt.gyroscope(values.data(), stamp);
    }

    void angular(const std::array<std::int32_t, 3>& values,
                 std::uint64_t stamp, std::uint64_t now) {
        model.now = now;
        if (!accept_time(stamp, now) || now - stamp > 250000 ||
            model.system_heading || !model.has_heading || !model.fresh()) return;
        fusion.angular({values[0] / 1000000.f, values[1] / 1000000.f,
                        values[2] / 1000000.f}, stamp);
    }

    void update(std::uint64_t now) {
        model.now = now;
        model.update();
        if (!model.has_heading) {
            fusion.reset();
            visible = false;
            display_time = 0;
            return;
        }
        float target = model.heading;
        if (!model.system_heading) {
            const auto stamp = std::max(model.tilt.stamp, model.tilt.gyro_time);
            if (stamp != absolute_time && model.paired()) {
                const auto& calibration = model.saved[model.channel];
                auto field = compass::rotate(model.tilt.raw - calibration.bias,
                                              calibration.rotation);
                auto gravity = model.tilt.gravity;
                if (gyroscope_recent(now)) {
                    field = HeadingFusion::rotate_body(field, fusion.last_velocity,
                        (stamp - model.tilt.stamp) / 1000000.f);
                    gravity = HeadingFusion::rotate_body(gravity, fusion.last_velocity,
                        (stamp - model.tilt.gyro_time) / 1000000.f);
                }
                fusion.absolute(field, gravity, stamp);
                absolute_time = stamp;
            }
            if (fusion.valid && gyroscope_recent(now)) {
                if (!fusion.readable()) {
                    model.quality = compass::Quality::Vertical;
                    visible = false;
                    display_time = 0;
                    return;
                }
                target = compass::wrap(fusion.bearing() + model.declination);
            }
        }
        const float elapsed = display_time && now > display_time
            ? std::min(.25f, (now - display_time) / 1000000.f) : .1f;
        shown = visible ? compass::wrap(shown + compass::delta(target, shown) *
                                       (-std::expm1(-elapsed / .1f))) : target;
        display_time = now;
        visible = true;
    }

    bool gyroscope_recent(std::uint64_t now) const {
        return fusion.angular_time && now >= fusion.angular_time &&
               now - fusion.angular_time <= 250000;
    }

    unsigned degrees() const { return unsigned(std::lround(shown)) % 360; }
    std::string_view cardinal() const {
        static constexpr std::string_view labels[] = {"北", "东北", "东", "东南", "南", "西南", "西", "西北"};
        return labels[unsigned((shown + 22.5f) / 45.f) % 8];
    }
};

} // namespace navigation
