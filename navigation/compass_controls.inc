// SPDX-License-Identifier: MIT
// Included in NavigationApp. Sensor resources belong to this foreground session.
    struct CompassChannel {
        SensorDescriptor descriptor;
        Permission permission;
        SensorSubscription subscription;
        void reset() {
            subscription.reset();
            permission.reset();
        }
    };
    nav::Heading heading;
    TaskScope compass_jobs;
    CompassChannel magnetic_channel, gravity_channel, angular_channel;
    std::array<SensorDescriptor, 2> magnetic_descriptors{};
    SensorDescriptor gravity_descriptor, angular_descriptor, absolute_descriptor;
    bool compass_probed = false, compass_connecting = false;
    bool compass_settings_loaded = false, compass_saving = false;
    unsigned compass_return = 0;
    int compass_visual_degrees = -1;
    std::uint64_t compass_visual_state = 0;

    bool compass_page_visible() const { return map_screen() || screen == 6; }
    bool compass_available() const {
        return absolute_descriptor.id ||
               ((magnetic_descriptors[0].id || magnetic_descriptors[1].id) && gravity_descriptor.id);
    }
    bool compass_streaming() const {
        return bool(magnetic_channel.subscription) && heading.model.status == compass::Status::Streaming;
    }
    void close_compass() {
        if (compass_saving) compass_settings_loaded = false;
        compass_jobs.cancel();
        compass_connecting = false;
        compass_saving = false;
        magnetic_channel.reset();
        gravity_channel.reset();
        angular_channel.reset();
        heading.suspend();
        heading.model.capture.collecting = false;
        heading.model.capture.candidate = {};
    }
    Task<void> open_compass_channel(CompassChannel& channel,
                                    const SensorDescriptor& descriptor,
                                    std::uint32_t desired_period) {
        auto granted = co_await ctx->permissions().acquire("sensor.read", descriptor.semantic.view());
        if (!granted) co_return std::unexpected(granted.error());
        const auto period = std::clamp(desired_period, descriptor.min_period_ms, descriptor.max_period_ms);
        auto subscribed = co_await ctx->sensors().subscribe(descriptor, period, *granted);
        if (!subscribed) co_return std::unexpected(subscribed.error());
        channel.descriptor = descriptor;
        channel.permission = std::move(*granted);
        channel.subscription = std::move(*subscribed);
        co_return Result<void>{};
    }
    void compass_connection_failed(Error error) {
        magnetic_channel.reset();
        gravity_channel.reset();
        angular_channel.reset();
        heading.suspend();
        heading.model.status = error == Error::denied ? compass::Status::Denied : compass::Status::Error;
        compass_connecting = false;
        ui_dirty = true;
        draw();
    }
    Task<void> connect_compass() {
        if (!compass_settings_loaded) {
            std::array<std::byte, compass::SAVE_SIZE> saved{};
            auto loaded = co_await ctx->storage().get("compass.v2", saved);
            if (loaded) (void)compass::decode(heading.model,
                reinterpret_cast<const std::uint8_t*>(saved.data()), *loaded);
            compass_settings_loaded = true;
        }
        std::array<SensorDescriptor, 32> descriptors{};
        auto listed = co_await ctx->sensors().list(descriptors);
        compass_probed = true;
        magnetic_descriptors = {};
        gravity_descriptor = angular_descriptor = absolute_descriptor = {};
        if (!listed) {
            heading.model.status = compass::Status::Unavailable;
            compass_connecting = false;
            ui_dirty = true;
            draw();
            co_return Result<void>{};
        }
        for (const auto& descriptor : std::span(descriptors).first(*listed)) {
            if (!descriptor.min_period_ms || descriptor.min_period_ms > descriptor.max_period_ms) continue;
            const auto semantic = descriptor.semantic.view();
            if (semantic == "orientation.heading" && descriptor.dimensions == 1 &&
                descriptor.unit == SensorUnit::micro_degrees) absolute_descriptor = descriptor;
            if (descriptor.dimensions != 3) continue;
            if (descriptor.unit == SensorUnit::nano_tesla) {
                if (semantic == "magnetic-field.0") magnetic_descriptors[0] = descriptor;
                if (semantic == "magnetic-field.1") magnetic_descriptors[1] = descriptor;
            }
            if (semantic == "motion.acceleration" &&
                descriptor.unit == SensorUnit::micro_meters_per_second_squared) gravity_descriptor = descriptor;
            if (semantic == "motion.angular-velocity" &&
                descriptor.unit == SensorUnit::micro_radians_per_second) angular_descriptor = descriptor;
        }
        if (!compass_available()) {
            heading.model.status = magnetic_descriptors[0].id || magnetic_descriptors[1].id
                ? compass::Status::NoGravity : compass::Status::Unavailable;
            compass_connecting = false;
            ui_dirty = true;
            draw();
            co_return Result<void>{};
        }
        heading.model.system_heading = absolute_descriptor.id != 0;
        heading.model.status = compass::Status::Permission;
        if (!heading.model.system_heading && !magnetic_descriptors[heading.model.channel].id)
            heading.model.channel = magnetic_descriptors[0].id ? 0 : 1;
        const auto& primary = heading.model.system_heading
            ? absolute_descriptor : magnetic_descriptors[heading.model.channel];
        auto magnetic = co_await open_compass_channel(magnetic_channel, primary, 100);
        if (!magnetic) {
            compass_connection_failed(magnetic.error());
            co_return Result<void>{};
        }
        if (!heading.model.system_heading) {
            auto gravity = co_await open_compass_channel(gravity_channel, gravity_descriptor, 100);
            if (!gravity) {
                compass_connection_failed(gravity.error());
                co_return Result<void>{};
            }
            if (angular_descriptor.id) {
                // A denied or resource-limited auxiliary stream leaves the
                // magnetic compass available with its ordinary smoothing.
                (void)co_await open_compass_channel(angular_channel, angular_descriptor, 50);
            }
        }
        heading.suspend();
        heading.model.status = compass::Status::Streaming;
        compass_connecting = false;
        ui_dirty = true;
        draw();
        (void)ctx->log().write(LogLevel::info, heading.model.system_heading
            ? "Navigation compass: system heading connected"
            : angular_channel.subscription ? "Navigation compass: magnetic, gravity and angular streams connected"
                                           : "Navigation compass: magnetic and gravity streams connected");
        co_return Result<void>{};
    }
    void refresh_compass() {
        if (!ctx || !initialized) return;
        const bool wanted = ctx->foreground() && compass_page_visible();
        if (!wanted) {
            if (compass_connecting || magnetic_channel.subscription) {
                close_compass();
                compass_probed = false;
            }
            return;
        }
        if (!compass_connecting && !magnetic_channel.subscription && !compass_probed) {
            compass_connecting = true;
            heading.model.status = compass::Status::Connecting;
            if (!compass_jobs.start(connect_compass())) compass_connection_failed(Error::resource_limit);
        }
    }
    void retry_compass() {
        close_compass();
        compass_probed = false;
        refresh_compass();
    }
    bool compass_event(const Event& event) {
        if (event.service == 11 && event.opcode == 0x8001) {
            const auto revoked = decode_permission_revoked(event);
            if (!revoked || revoked->name != "sensor.read") return false;
            const std::string_view scope(reinterpret_cast<const char*>(revoked->scope.data()), revoked->scope.size());
            if (scope == "motion.angular-velocity") {
                angular_channel.reset();
                heading.fusion.reset();
            } else if (scope.empty() || scope == magnetic_channel.descriptor.semantic.view() ||
                       scope == gravity_channel.descriptor.semantic.view()) {
                close_compass();
                compass_probed = true;
                heading.model.status = compass::Status::Denied;
            }
            ui_dirty = true;
            draw();
            return true;
        }
        if (event.service != 8 || event.opcode != 0x8001) return false;
        if (!ctx->foreground() || !compass_page_visible()) return true;
        const auto sample = decode_sensor_sample(event);
        if (!sample) return true;
        const auto now = now_us();
        heading.model.now = now;
        if (sample->handle == magnetic_channel.subscription.handle() && sample->dimensions ==
            (heading.model.system_heading ? 1 : 3)) {
            if (heading.model.system_heading) {
                if (heading.accept_time(sample->timestamp_us, now))
                    heading.model.heading_sample(sample->values[0], sample->timestamp_us);
            } else heading.magnetic(sample->values, sample->timestamp_us, now);
        } else if (sample->handle == gravity_channel.subscription.handle() && sample->dimensions == 3) {
            heading.acceleration(sample->values, sample->timestamp_us, now);
        } else if (sample->handle == angular_channel.subscription.handle() && sample->dimensions == 3) {
            heading.angular(sample->values, sample->timestamp_us, now);
        }
        update_compass_visual(now);
        return true;
    }
    void update_compass_visual(std::uint64_t now) {
        heading.update(now);
        const auto& model = heading.model;
        const auto state = std::uint64_t(model.status) | (std::uint64_t(model.quality) << 4) |
            (std::uint64_t(heading.visible) << 8) | (std::uint64_t(compass_available()) << 9) |
            (std::uint64_t(heading.gyroscope_active(now)) << 10) |
            (std::uint64_t(model.capture.collecting) << 11) |
            (std::uint64_t(model.capture.faces) << 12) |
            (std::uint64_t(compass::valid_cal(model.capture.candidate)) << 16) |
            (std::uint64_t(compass_saving) << 17);
        const int degrees = heading.visible ? int(heading.degrees()) : -1;
        if (state != compass_visual_state || degrees != compass_visual_degrees) {
            compass_visual_state = state;
            compass_visual_degrees = degrees;
            if (compass_page_visible()) {
                if (!ui_dirty) compass_only_dirty = true;
                ui_dirty = true;
                if (ctx && !in_tick) clock();
            }
        }
    }
    Task<void> save_compass_calibration(bool apply_candidate = true) {
        const auto channel = heading.model.channel;
        const auto candidate = heading.model.capture.candidate;
        const auto previous = heading.model.saved[channel];
        std::array<std::uint8_t, compass::SAVE_SIZE> bytes{};
        // Encode the candidate without making it the active calibration until
        // the private-storage write succeeds. No suspension occurs in this span.
        if (apply_candidate) heading.model.saved[channel] = candidate;
        (void)compass::encode(heading.model, bytes.data(), bytes.size());
        heading.model.saved[channel] = previous;
        heading.model.capture.collecting = false;
        std::array<std::byte, 256> packet{};
        auto saved = co_await ctx->storage().set("compass.v2", std::as_bytes(std::span(bytes)), packet);
        if (saved) {
            if (apply_candidate) {
                heading.model.saved[channel] = candidate;
                heading.model.capture.candidate = {};
                heading.suspend();
            }
            message(apply_candidate ? "指南针校准已保存" : "指南针来源已保存");
        } else message("指南针设置保存失败，请重试");
        compass_saving = false;
        ui_dirty = true;
        draw();
        co_return Result<void>{};
    }
    std::string_view compass_status_text() const {
        using Status = compass::Status;
        using Quality = compass::Quality;
        if (compass_saving) return "正在保存校准…";
        if (compass::valid_cal(heading.model.capture.candidate)) return "采样合格，可以保存校准";
        if (heading.model.capture.collecting) {
            return heading.model.capture.faces == 6 ? "继续缓慢转动，直至可保存" : "正反、上下、左右缓慢翻转";
        }
        switch (heading.model.status) {
        case Status::Unavailable: return angular_descriptor.id ? "仅有陀螺仪，缺少指北来源" : "当前设备未提供方位传感器";
        case Status::Connecting: return "正在检测传感器…";
        case Status::Permission: return "等待传感器授权";
        case Status::Denied: return "传感器权限未授权";
        case Status::Error: return "传感器暂不可用，请重新检测";
        case Status::NoGravity: return "缺少用于倾斜补偿的加速度计";
        case Status::Streaming: break;
        }
        switch (heading.model.quality) {
        case Quality::Calibrate: return "需要六面校准";
        case Quality::Moving: return "请暂时保持设备平稳";
        case Quality::Vertical: return "请调整握持角度";
        case Quality::Interference: return "磁场异常，请远离干扰";
        case Quality::Good:
            if (safe.h < 260 && model_gyro_assisted()) return "磁北 · 陀螺仪辅助";
            return safe.h < 260 ? (heading.model.system_heading ? "磁北 · 系统方位" : "实时磁北") : "实时方向";
        default: return "等待有效方位数据";
        }
    }
    bool model_gyro_assisted() const { return heading.gyroscope_active(now_us()); }
    navui::Rect compass_badge() const {
        if (live) return {safe.x + safe.w - 51, safe.y + (layout.compact ? 21 : 28), 32, 32};
        navui::Rect box{safe.x + 10, safe.y + (map_failed ? 99 : 65), 38, 38};
        auto locate = layout.recenter;
        if (layout.compact) locate.x = safe.x + 10;
        if (box.y < locate.y + locate.h && locate.y < box.y + box.h)
            box = {locate.x + 46, locate.y, 38, 38};
        return box;
    }
    void draw_compass_needle(int x, int y, int radius, float degrees) {
        const float angle = -degrees / compass::DEG;
        const float sine = std::sin(angle), cosine = std::cos(angle);
        paint.line(x, y, x + int(sine * radius), y - int(cosine * radius), colors.danger, 3);
        paint.line(x, y, x - int(sine * (radius - 3)), y + int(cosine * (radius - 3)), colors.muted, 2);
        const int tx = x + int(sine * radius), ty = y - int(cosine * radius);
        paint.line(tx, ty, tx - int(sine * 5 + cosine * 3), ty + int(cosine * 5 - sine * 3), colors.danger, 2);
        paint.line(tx, ty, tx - int(sine * 5 - cosine * 3), ty + int(cosine * 5 + sine * 3), colors.danger, 2);
        paint.circle(x, y, 3, colors.surface);
        paint.circle(x + int(sine * radius), y - int(cosine * radius), 2, colors.danger);
    }
    void compass_badge_draw() {
        if (!compass_available()) return;
        const auto box = compass_badge();
        if (!live && box.y + box.h > layout.map_h - 20) return;
        paint.card(box, colors, box.w / 2);
        const int x = box.x + box.w / 2, y = box.y + box.h / 2;
        if (heading.visible) draw_compass_needle(x, y, box.w / 2 - 7, heading.shown);
        else paint.icon("compass", x, y, colors.muted, box.w / 2);
        paint.hit(box, navui::OpenCompass);
    }
    void compass_draw() {
        using namespace navui;
        title("指南针");
        const auto& model = heading.model;
        const bool calibrating = model.capture.collecting || compass::valid_cal(model.capture.candidate);
        const bool dual = !model.system_heading && magnetic_descriptors[0].id && magnetic_descriptors[1].id;
        const int controls = safe.y + safe.h - 46;
        const int footer_top = controls - (calibrating ? 24 : dual ? 32 : 0);
        const bool spacious = safe.h >= 400;
        const int below = footer_top - (spacious ? 60 : 44);
        const int dial_top = safe.y + (safe.h < 260 ? 48 : 64);
        const int radius = std::clamp((below - 8 - dial_top) / 2, 16, std::clamp((safe.w - 32) / 2, 16, 140));
        const int cx = safe.x + safe.w / 2, cy = (dial_top + below - 8) / 2;
        paint.circle(cx, cy, radius, colors.surface);
        paint.ring(cx, cy, radius, colors.line);
        for (int i = 0; i < 24; ++i) {
            const float angle = (i * 15.f - (heading.visible ? heading.shown : 0.f)) / compass::DEG;
            const int outer = radius - 5, inner = outer - (i % 6 == 0 ? 9 : 4);
            paint.line(cx + int(std::sin(angle) * inner), cy - int(std::cos(angle) * inner),
                       cx + int(std::sin(angle) * outer), cy - int(std::cos(angle) * outer), colors.line);
        }
        if (heading.visible) {
            draw_compass_needle(cx, cy, std::max(7, radius - 20), heading.shown);
            constexpr std::string_view cardinals[] = {"N", "E", "S", "W"};
            for (unsigned i = 0; i < 4; ++i) {
                const float angle = (i * 90.f - heading.shown) / compass::DEG;
                paint.text({cx + int(std::sin(angle) * (radius - 15)) - 9,
                            cy - int(std::cos(angle) * (radius - 15)) - 8, 18, 18},
                           cardinals[i], i ? colors.muted : colors.danger, 12, 1);
            }
        } else paint.text({cx - 36, cy - 13, 72, 30}, "—", colors.muted, 28, 1);
        const auto value = heading.visible ? nav::unsigned_text(heading.degrees()) + "° " + std::string(heading.cardinal()) : std::string("方向待就绪");
        paint.text({safe.x + 16, below, safe.w - 32, spacious ? 36 : 24}, value, colors.ink, spacious ? 30 : 21, 1);
        paint.text({safe.x + 12, below + (spacious ? 43 : 27), safe.w - 24, 16}, compass_status_text(), colors.muted, 12, 1);
        std::string source = model.system_heading ? "系统绝对方位 · 磁北" : "磁力计 " + nav::unsigned_text(model.channel + 1) + " · 磁北";
        if (heading.gyroscope_active(now_us())) source += " · 陀螺仪辅助";
        if (model.capture.collecting) source = "六面采样 · 远离磁铁和金属";
        if (compass_available() && safe.h >= 260) paint.text({safe.x + 12, safe.y + 45, safe.w - 24, 18}, source, colors.muted, 11, 1);
        if (calibrating) {
            const auto progress_text = "覆盖 " + nav::unsigned_text(model.capture.faces) + " / 6 面";
            paint.text({safe.x + 16, controls - 23, safe.w - 32, 18}, progress_text, colors.primary, 12, 1);
            paint.button({safe.x + 16, controls, (safe.w - 40) / 2, 34}, "取消", colors, CancelCompassCalibration, false, !compass_saving);
            paint.button({safe.x + 24 + (safe.w - 40) / 2, controls, (safe.w - 40) / 2, 34}, "保存校准", colors, SaveCompassCalibration, true,
                         compass::valid_cal(model.capture.candidate) && !compass_saving);
        } else {
            if (!model.system_heading && magnetic_descriptors[0].id && magnetic_descriptors[1].id) {
                const int tab_y = controls - 32;
                for (unsigned i = 0; i < 2; ++i)
                    paint.button({safe.x + 16 + int(i) * (safe.w - 32) / 2, tab_y, (safe.w - 40) / 2, 27},
                        i ? "磁力计 2" : "磁力计 1", colors, i ? SecondMagnetometer : FirstMagnetometer, model.channel == i);
            }
            const bool can_calibrate = compass_streaming() && !model.system_heading;
            const unsigned action = can_calibrate ? CalibrateCompass : RetryCompass;
            paint.button({safe.x + 16, controls, safe.w - 32, 34}, can_calibrate ? "六面校准" : "重新检测", colors, action, true,
                         !compass_connecting && !compass_saving);
        }
    }
