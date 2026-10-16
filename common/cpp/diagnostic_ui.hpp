#pragma once

#include "pxa_ui.h"
#include <algorithm>
#include <array>
#include <cstring>

// Optional retained view for callback-based ABI probes. Test traffic and its
// assertions stay unchanged; the UI makes results readable without a logcat.
namespace pxa_examples {
class DiagnosticUi {
public:
    constexpr DiagnosticUi(const char *title, const char *coverage)
        : title_(title), coverage_(coverage) {}
    bool start(const uint8_t *config, uint32_t size) {
        pxa_ui_environment_t environment{};
        if (pxa_ui_parse_start_environment(config, size, &environment)) {
            const auto density = environment.density_q16 ? environment.density_q16 : 65536u;
            for (unsigned i = 0; i < 4; ++i)
                insets_[i] = uint16_t(std::min<uint64_t>(65535,
                    (uint64_t(environment.safe_insets[i]) * 65536 + density - 1) / density));
        }
        return render();
    }
    bool passed(const char *result) { result_ = result; return render(); }
    bool handle(const uint8_t *bytes, uint32_t size) {
        pxa_event_t event{}; pxa_ui_event_data_t ui{};
        if (!pxa_parse_event(bytes, size, &event) || !pxa_ui_parse_event(&event, &ui) ||
            ui.kind != PXA_UI_EVENT_CLICK_KIND || ui.node < 100 || ui.node > 102) return false;
        page_ = uint8_t(ui.node - 100);
        (void)render();
        return true;
    }
private:
    bool render() {
        pxa_ui_transaction_t tx{};
        if (!pxa_ui_transaction_begin(&tx, ++generation_, PXA_UI_TRANSACTION_REPLACE_SURFACE,
                                      packet_.data(), packet_.size())) return false;
        auto text = [&](uint32_t id, uint32_t parent, const char *value, uint8_t role) {
            return pxa_ui_create(&tx, id, parent, 0, PXA_UI_NODE_TEXT) &&
                pxa_ui_set_length(&tx, id, PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_FILL, 0) &&
                pxa_ui_set_text(&tx, id, value, std::strlen(value)) &&
                pxa_ui_set_font_role(&tx, id, role) &&
                pxa_ui_set_theme_color(&tx, id, PXA_UI_PROPERTY_FOREGROUND, PXA_UI_THEME_TEXT);
        };
        bool ok = pxa_ui_create(&tx, 1, 0, 0, PXA_UI_NODE_ROOT) &&
            pxa_ui_set_u8(&tx, 1, PXA_UI_PROPERTY_LAYOUT, PXA_UI_LAYOUT_COLUMN) &&
            pxa_ui_set_padding(&tx, 1, std::max<uint16_t>(12, insets_[3]),
                std::max<uint16_t>(12, insets_[0]), std::max<uint16_t>(12, insets_[1]),
                std::max<uint16_t>(12, insets_[2])) &&
            pxa_ui_set_dp(&tx, 1, PXA_UI_PROPERTY_GAP, 12) &&
            pxa_ui_set_theme_color(&tx, 1, PXA_UI_PROPERTY_BACKGROUND, PXA_UI_THEME_BACKGROUND) &&
            text(2, 1, title_, PXA_UI_FONT_ROLE_TITLE) &&
            pxa_ui_create(&tx, 3, 1, 0, PXA_UI_NODE_BOX) &&
            pxa_ui_set_length(&tx, 3, PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_FILL, 0) &&
            pxa_ui_set_u8(&tx, 3, PXA_UI_PROPERTY_LAYOUT, PXA_UI_LAYOUT_ROW) &&
            pxa_ui_set_dp(&tx, 3, PXA_UI_PROPERTY_GAP, 6);
        constexpr const char *labels[] = {"结果", "服务", "ABI"};
        for (unsigned i = 0; ok && i < 3; ++i) {
            const auto id = 100u + i;
            ok = pxa_ui_create_typed(&tx, id, 3, 0, PXA_UI_NODE_CONTROL, PXA_UI_CONTROL_BUTTON) &&
                pxa_ui_set_u16(&tx, id, PXA_UI_PROPERTY_GROW, 1) &&
                pxa_ui_set_padding(&tx, id, 6, 10, 6, 10) &&
                pxa_ui_set_theme_color(&tx, id, PXA_UI_PROPERTY_BACKGROUND,
                    page_ == i ? PXA_UI_THEME_PRIMARY_CONTAINER : PXA_UI_THEME_SURFACE) &&
                pxa_ui_set_event_mask(&tx, id, PXA_UI_EVENT_MASK_CLICK) &&
                text(200u + i, id, labels[i], PXA_UI_FONT_ROLE_LABEL);
        }
        constexpr const char *abi = "Core 2.0\n\n64 位请求 token 与资源句柄\n控制通道与数据 IO 分离\n按组件验证服务和运行目标\n\n详细结果可通过 PXADB 日志查看。";
        ok = ok && text(4, 1, page_ == 0 ? result_ : page_ == 1 ? coverage_ : abi,
                        PXA_UI_FONT_ROLE_BODY);
        if (ok && pxa_ui_transaction_commit(&tx)) return true;
        if (tx.active) (void)pxa_ui_transaction_cancel(&tx);
        return false;
    }
    const char *title_;
    const char *coverage_;
    const char *result_ = "正在运行诊断…";
    std::array<uint8_t, 1536> packet_{};
    std::array<uint16_t, 4> insets_{};
    uint32_t generation_{};
    uint8_t page_{};
};
} // namespace pxa_examples
