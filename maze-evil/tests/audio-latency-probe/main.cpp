#include <pxa/app.hpp>

#include <algorithm>
#include <optional>
#include "../../sfx_profiles.h"

using namespace pxa::ui;
using namespace pxa::ui::literals;

struct AudioApp {
    pxa::Context* context_ = nullptr;
    unsigned mode_ = 0;
    State<std::string> status{"Opening"};
    std::optional<pxa::Permission> permission;
    std::optional<pxa::AudioSession> session;

    pxa::Task<void> initialize(pxa::Context& context) {
        auto grant = co_await context.permissions().acquire(
            "audio.playback", "media");
        if (!grant) {
            status.set("Permission denied");
            co_return std::unexpected(grant.error());
        }
        permission.emplace(std::move(*grant));
        auto opened = co_await context.audio().open(*permission);
        if (!opened) {
            status.set("Open failed");
            co_return std::unexpected(opened.error());
        }
        session.emplace(std::move(*opened));
        auto committed = co_await session->graph(0);
        if (!committed) {
            status.set("Graph failed");
            co_return std::unexpected(committed.error());
        }
        status.set("Ready");
        co_return pxa::Result<void>{};
    }

    pxa::Result<void> on_start(pxa::Context& context) {
        context_ = &context;
        return context.tasks().start(initialize(context));
    }

    auto view() {
        return Column(
            Text("Audio").font(Font::title),
            Text(status),
            Button("Run next test").on_click([this] {
                if (!session) return;
                auto play = [this](const auto& specs) {
                    std::array<pxa::Tone, std::extent_v<std::remove_reference_t<decltype(specs)>>> tones{};
                    for (std::size_t i = 0; i < tones.size(); ++i) {
                        const auto& spec = specs[i];
                        tones[i] = {
                            .frequency_hz = std::max<std::uint16_t>(40, spec.frequency_hz),
                            .duration_ms = spec.duration_ms, .gain_db_q8 = -12*256,
                            .waveform = static_cast<pxa::Waveform>(spec.waveform-1),
                            .attack_ms = spec.attack_ms, .release_ms = spec.release_ms,
                            .delay_ms = spec.delay_ms};
                    }
                    auto result = session->effect(tones);
                    (void)context_->log().write(pxa::LogLevel::info,
                        result ? "Probe effect accepted" : "Probe effect rejected");
                };
                switch (mode_++ % 3) {
                    case 0:
                        (void)context_->log().write(pxa::LogLevel::info,"Probe case shotgun");
                        play(kShotgun); status.set("Shotgun"); break;
                    case 1:
                        (void)context_->log().write(pxa::LogLevel::info,"Probe case alert-shotgun");
                        play(kImpAlert); play(kShotgun); status.set("Alert + shotgun"); break;
                    default:
                        (void)context_->log().write(pxa::LogLevel::info,"Probe case door");
                        play(kDoorOpen); status.set("Door"); break;
                }
            })
        ).gap(12_dp).padding(16_dp);
    }
};

PXA_APPLICATION(AudioApp)
