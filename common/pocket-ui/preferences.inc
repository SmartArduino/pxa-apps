// SPDX-License-Identifier: CC0-1.0
// Included inside each runtime's anonymous namespace after now_us().
char preferences_key[96];
bool preferences_loaded, preferences_loading, preferences_dirty, preferences_pending, language_changed;
uint64_t preferences_load_retry, preferences_write_retry;
uint8_t preferences_payload[4];
void language_log() {
    pxa_log_write(2, pocket::english() ? "Pocket: language en" : "Pocket: language zh");
}
void service_preferences() {
    uint64_t now = now_us();
    if (!preferences_loaded && !preferences_loading && now >= preferences_load_retry) {
        int32_t status = pxa_storage_request_get(PREFERENCES_LOAD, preferences_key, uint32_t(strlen(preferences_key)));
        if (!status) preferences_loading = true;
        else preferences_load_retry = now + 1000000;
    }
    if (preferences_dirty && !preferences_pending && now >= preferences_write_retry) {
        pocket_ui::encode_locale(preferences_payload, pocket::locale);
        int32_t status = pxa_storage_request_set(packet, sizeof(packet), PREFERENCES_SAVE,
            preferences_key, uint32_t(strlen(preferences_key)), preferences_payload, sizeof(preferences_payload));
        if (!status) { preferences_pending = true; preferences_dirty = false; }
        else preferences_write_retry = now + 1000000;
    }
}
void toggle_language() {
    pocket::set_english(!pocket::english());
    language_changed = true; preferences_dirty = true;
    language_log(); pocket::redraw(); service_preferences();
}
int32_t handle_preferences(const pxa_event_t &e) {
    if (e.service != PXA_STORAGE_SERVICE) return PXA_EVENT_UNHANDLED;
    if (e.token == PREFERENCES_LOAD) {
        pxa_storage_get_result_t r;
        if (!pxa_storage_parse_get(&e, PREFERENCES_LOAD, &r)) return PXA_STATUS_INTERNAL;
        preferences_loading = false;
        if (preferences_loaded) return PXA_EVENT_HANDLED;
        if (!r.status || r.status == PXA_STATUS_NOT_FOUND) {
            pocket::Locale value = pocket::Locale::ZH;
            if (!language_changed && !r.status && pocket_ui::decode_locale(r.value.data, r.value.size, value)) pocket::locale = value;
            preferences_loaded = true; language_log(); pocket::redraw();
        } else {
            preferences_load_retry = now_us()+1000000;
            pxa_log_write(3, "Pocket: language load failed; retrying");
        }
        return PXA_EVENT_HANDLED;
    }
    if (e.token == PREFERENCES_SAVE) {
        int32_t status;
        if (!pxa_storage_parse_status(&e, PREFERENCES_SAVE, PXA_STORAGE_SET, &status)) return PXA_STATUS_INTERNAL;
        if (!preferences_pending) return PXA_EVENT_HANDLED;
        preferences_pending = false;
        if (status) {
            preferences_dirty = true; preferences_write_retry = now_us()+1000000;
            pxa_log_write(3, "Pocket: language save failed; retrying");
        } else preferences_write_retry = 0;
        return PXA_EVENT_HANDLED;
    }
    return PXA_EVENT_UNHANDLED;
}
