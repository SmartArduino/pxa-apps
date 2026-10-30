// SPDX-License-Identifier: CC0-1.0
#pragma once
#include "pxa_clock.h"
#include "pxa_permission.h"
#include "pxa_sensor.h"
#include <stdio.h>
#include <string.h>
namespace pocket_imu {
template <class Device> class Service {
  using Status = decltype(Device{}.status);
  enum Stage {
    Idle,
    List,
    AccPermission,
    GyroPermission,
    AccSubscribe,
    GyroSubscribe
  } stage = Idle;
  static constexpr uint64_t PREFIX = UINT64_C(0x4d4f540000000000),
                            MASK = UINT64_C(0xffffff0000000000);
  Device &d;
  const char *log_prefix, *ready_suffix;
  uint32_t requested_period;
  uint64_t serial = 0, token = 0, submitted = 0;
  uint64_t acc_permission = 0, gyro_permission = 0, acc_stream = 0,
           gyro_stream = 0;
  uint64_t closing[8] = {};
  unsigned close_count = 0;
  uint16_t acc_id = 0, gyro_id = 0;
  uint32_t acc_period = 50, gyro_period = 50, last_retry = 0;
  bool engaged = false, sent = false, cancelled = false, cancel_sent = false,
       failed = false, denied = false, gyro_denied = false, quiet = false;
  static bool text(const uint8_t *p, size_t n, const char *s) {
    return strlen(s) == n && !memcmp(p, s, n);
  }
  void close(uint64_t &handle) {
    if (!handle)
      return;
    for (unsigned i = 0; i < close_count; ++i)
      if (closing[i] == handle) {
        handle = 0;
        return;
      }
    // No new request is started until this queue is empty; at most four
    // live resources and one late completion can be present.
    if (close_count < 8)
      closing[close_count++] = handle;
    handle = 0;
  }
  void pump_closes() {
    for (unsigned i = 0; i < close_count;) {
      if (pxa_close_handle(closing[i]) == PXA_STATUS_WOULD_BLOCK) {
        ++i;
        continue;
      }
      closing[i] = closing[--close_count];
    }
  }
  void clock(bool pause) {
    if (pause != quiet) {
      if (!pxa_clock_set_period(pause ? 0 : 20))
        quiet = pause;
    }
  }
  void next(Stage value) {
    stage = value;
    token = 0;
    sent = cancelled = cancel_sent = false;
    submitted = 0;
  }
  void stop() {
    close(acc_stream);
    close(gyro_stream);
    close(acc_permission);
    close(gyro_permission);
    if (token && sent) {
      cancelled = true;
      cancel_sent = false;
    } else
      next(Idle);
    d.gyro = false;
    d.tilt.suspend();
    engaged = false;
    clock(false);
    pump_closes();
  }
  void fail(Status status) {
    stop();
    failed = true;
    d.status = status;
  }
  void connected() {
    next(Idle);
    d.status = Status::Streaming;
    d.gyro = gyro_stream != 0;
    d.tilt.suspend();
    char text[96];
    snprintf(text, sizeof(text), "%s connected acc=1 gyro=%u", log_prefix,
             d.gyro ? 1u : 0u);
    pxa_log_write(2, text);
  }
  void submit() {
    if (stage == Idle || sent)
      return;
    if (!token)
      token = PREFIX | ++serial;
    int32_t result = -1;
    if (stage == List)
      result = pxa_sensor_request_list(token);
    else if (stage == AccPermission || stage == GyroPermission) {
      uint8_t packet[160];
      uint32_t n = 0;
      const char *scope = stage == AccPermission ? "motion.acceleration"
                                                 : "motion.angular-velocity";
      if (pxa_permission_build(packet, sizeof(packet), PXA_PERMISSION_ACQUIRE,
                               token, "sensor.read", 11,
                               reinterpret_cast<const uint8_t *>(scope),
                               strlen(scope), &n))
        result = pxa_control_packet(packet, n);
    } else
      result = pxa_sensor_request_subscribe(
          token, stage == AccSubscribe ? acc_id : gyro_id,
          stage == AccSubscribe ? acc_period : gyro_period,
          stage == AccSubscribe ? acc_permission : gyro_permission);
    if (!result) {
      sent = true;
      submitted = d.now;
    } else if (result != PXA_STATUS_WOULD_BLOCK) {
      if (stage == GyroPermission) {
        gyro_denied = true;
        next(AccSubscribe);
      } else if (stage == GyroSubscribe) {
        close(gyro_permission);
        connected();
      } else
        fail(Status::Error);
    }
  }

public:
  explicit Service(Device &value, const char *prefix = "IMU",
                   const char *ready = "ready", uint32_t period_ms = 50)
      : d(value), log_prefix(prefix), ready_suffix(ready),
        requested_period(period_ms ? period_ms : 50) {}
  void update(bool foreground, uint64_t now) {
    d.now = now;
    pump_closes();
    if (d.retry != last_retry) {
      last_retry = d.retry;
      stop();
      denied = gyro_denied = failed = false;
    }
    bool want = foreground && d.wanted;
    if (!want) {
      if (engaged) {
        stop();
        d.status = denied ? Status::Denied : Status::Unavailable;
      }
      clock(false);
      failed = false;
    }
    if (cancelled && token) {
      clock(false);
      if (!cancel_sent)
        cancel_sent = pxa_cancel(token) == 0;
      return;
    }
    if (!want)
      return;
    if (!engaged && !failed && !close_count) {
      if (denied) {
        d.status = Status::Denied;
        return;
      }
      engaged = true;
      acc_id = gyro_id = 0;
      d.tilt.suspend();
      d.status = Status::Connecting;
      next(List);
    }
    if (!engaged || failed)
      return;
    if (sent && stage != AccPermission && stage != GyroPermission &&
        now - submitted > 2000000) {
      fail(Status::Error);
      return;
    }
    clock(stage == AccPermission || stage == GyroPermission);
    submit();
  }
  bool event(const pxa_event_t &event, bool foreground, uint64_t now) {
    update(foreground, now);
    pxa_permission_revoked_t revoked;
    if (pxa_permission_parse_revoked(&event, &revoked) &&
        text(revoked.name, revoked.name_size, "sensor.read")) {
      if (text(revoked.scope, revoked.scope_size, "motion.acceleration") ||
          !revoked.scope_size) {
        denied = true;
        fail(Status::Denied);
      } else if (text(revoked.scope, revoked.scope_size,
                      "motion.angular-velocity")) {
        gyro_denied = true;
        close(gyro_stream);
        close(gyro_permission);
        d.gyro = false;
        d.tilt.gyro_time = 0;
      }
      return true;
    }
    if (event.service == PXA_SENSOR_SERVICE &&
        event.opcode == PXA_SENSOR_SAMPLE) {
      pxa_sensor_sample_t sample;
      if (!foreground || !d.wanted ||
          !pxa_sensor_parse_sample(&event, &sample) || sample.dimensions != 3 ||
          sample.timestamp_us > now + 100000)
        return true;
      bool was = d.tilt.calibrated;
      if (sample.handle == acc_stream)
        d.tilt.acceleration(sample.values, sample.timestamp_us, now);
      else if (sample.handle == gyro_stream)
        d.tilt.gyroscope(sample.values, sample.timestamp_us);
      if (!was && d.tilt.calibrated) {
        char text[96];
        snprintf(text, sizeof(text), "%s %s", log_prefix, ready_suffix);
        pxa_log_write(2, text);
      }
      return true;
    }
    if ((event.token & MASK) != PREFIX)
      return false;
    if (event.token != token || cancelled || !foreground || !d.wanted) {
      uint64_t handle = 0;
      pxa_permission_acquire_result_t p;
      pxa_sensor_subscribe_result_t s;
      if (pxa_permission_parse_acquire(&event, event.token, &p) && !p.status)
        handle = p.handle;
      if (pxa_sensor_parse_subscribe(&event, event.token, &s) && !s.status)
        handle = s.handle;
      close(handle);
      pump_closes();
      if (event.token == token)
        next(Idle);
      return true;
    }
    sent = false; // This completion owns no outstanding request any more.
    if (stage == List) {
      pxa_sensor_list_result_t list;
      if (!pxa_sensor_parse_list(&event, token, &list) || list.status) {
        fail(Status::Unavailable);
        return true;
      }
      size_t offset = 0;
      pxa_sensor_descriptor_t descriptor;
      unsigned count = 0;
      while (offset < list.size && count++ < PXA_SENSOR_MAX_DESCRIPTORS) {
        if (!pxa_sensor_descriptor_next(&list, &offset, &descriptor)) {
          fail(Status::Error);
          return true;
        }
        if (descriptor.dimensions != 3 || descriptor.min_period_ms > 100)
          continue;
        uint32_t period =
            descriptor.min_period_ms > requested_period   ? descriptor.min_period_ms
            : descriptor.max_period_ms < requested_period ? descriptor.max_period_ms
                                                          : requested_period;
        if (text(descriptor.semantic, descriptor.semantic_size,
                 "motion.acceleration") &&
            descriptor.unit ==
                PXA_SENSOR_UNIT_MICRO_METERS_PER_SECOND_SQUARED) {
          acc_id = descriptor.id;
          acc_period = period;
        }
        if (text(descriptor.semantic, descriptor.semantic_size,
                 "motion.angular-velocity") &&
            descriptor.unit == PXA_SENSOR_UNIT_MICRO_RADIANS_PER_SECOND) {
          gyro_id = descriptor.id;
          gyro_period = period;
        }
      }
      if (offset != list.size) {
        fail(Status::Error);
        return true;
      }
      if (!acc_id) {
        fail(Status::Unavailable);
        return true;
      }
      d.status = Status::Permission;
      next(AccPermission);
    } else if (stage == AccPermission || stage == GyroPermission) {
      pxa_permission_acquire_result_t permission;
      if (!pxa_permission_parse_acquire(&event, token, &permission)) {
        fail(Status::Error);
        return true;
      }
      if (stage == AccPermission) {
        if (permission.status) {
          denied = true;
          fail(Status::Denied);
          return true;
        }
        acc_permission = permission.handle;
        next(gyro_id && !gyro_denied ? GyroPermission : AccSubscribe);
      } else {
        if (!permission.status)
          gyro_permission = permission.handle;
        else
          gyro_denied = true;
        next(AccSubscribe);
      }
    } else if (stage == AccSubscribe || stage == GyroSubscribe) {
      pxa_sensor_subscribe_result_t subscription;
      if (!pxa_sensor_parse_subscribe(&event, token, &subscription)) {
        fail(Status::Error);
        return true;
      }
      if (stage == AccSubscribe) {
        if (subscription.status) {
          fail(Status::Error);
          return true;
        }
        acc_stream = subscription.handle;
        if (gyro_permission)
          next(GyroSubscribe);
        else
          connected();
      } else {
        if (!subscription.status)
          gyro_stream = subscription.handle;
        else
          close(gyro_permission);
        connected();
      }
    }
    return true;
  }
};
} // namespace pocket_imu
