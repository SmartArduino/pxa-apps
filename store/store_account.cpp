#include "store_account.hpp"
#include "store_json.h"
#include "pxa_net.h"
#include "pxa_storage.h"
#include <cstdio>
#include <cstring>

namespace store {
namespace {
constexpr char key[] = "account.session.v1";
void erase(char *p, size_t n) { volatile char *v = p; while (n--) *v++ = 0; }
bool credential(const char *s, const char *prefix) {
    if (std::strlen(s) != 48 || std::strncmp(s, prefix, 5)) return false;
    for (size_t i = 5; i < 48; ++i)
        if (!((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z') ||
              (s[i] >= '0' && s[i] <= '9') || s[i] == '_' || s[i] == '-')) return false;
    return true;
}
bool code_valid(const char *s) {
    constexpr char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    if (std::strlen(s) != 9 || s[4] != '-') return false;
    for (size_t i = 0; i < 9; ++i) if (i != 4 && !std::strchr(alphabet, s[i])) return false;
    return true;
}
}
void Account::start() {
    store_client_init(&client);
    if (pxa_storage_request_get(++storage_id, key, sizeof(key) - 1) != PXA_STATUS_OK)
        restored = true;
}
void Account::forget() {
    erase(token, sizeof(token));
    erase(display_name, sizeof(display_name));
    session_checked = false;
    (void)pxa_storage_request_remove(++storage_id, key, sizeof(key) - 1);
    phase = AccountPhase::guest;
    liked = false; like_ready = false; want_likes = app_id[0] != 0;
    changed = true;
}
void Account::network(uint64_t permission, uint64_t now) {
    client.network_permission = permission;
    schedule(now);
}
void Account::schedule(uint64_t now) {
    if (paused || !restored || busy() || !client.network_permission || now < next_attempt) return;
    if (token[0] && !session_checked) {
        phase = AccountPhase::restoring;
        submit(Op::session, now);
    } else if (want_likes && app_id[0] && phase != AccountPhase::starting && phase != AccountPhase::waiting) {
        want_likes = false;
        submit(Op::likes_get, now);
    }
}
bool Account::submit(Op op, uint64_t now) {
    if (busy() || !client.network_permission) return false;
    char url[512], payload[128]{}, authorization[56]{};
    const bool likes = op == Op::likes_get || op == Op::likes_put || op == Op::likes_delete;
    if (likes) {
        // IDs in a signed PXA manifest are restricted to these ASCII bytes.
        // Validate before interpolation; no user-provided URL reaches bearer transport.
        if (!app_id[0] || std::strlen(lineage) != 64) return false;
        for (const char *p = app_id; *p; ++p)
            if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                  (*p >= '0' && *p <= '9') || *p == '.' || *p == '_' || *p == '-')) return false;
        for (const char *p = lineage; *p; ++p)
            if (!((*p >= 'a' && *p <= 'f') || (*p >= '0' && *p <= '9'))) return false;
        std::snprintf(url, sizeof(url), PXA_STORE_ORIGIN "/api/v2/apps/%s/likes?lineage_root_key_id=%s", app_id, lineage);
    } else {
        const char *path = op == Op::authorize ? "authorize" : op == Op::poll ? "token" : "session";
        std::snprintf(url, sizeof(url), PXA_STORE_ORIGIN "/api/v1/device/%s", path);
    }
    if (op == Op::authorize) std::snprintf(payload, sizeof(payload), "{\"device_name\":\"PXA Store\"}");
    if (op == Op::poll) std::snprintf(payload, sizeof(payload), "{\"device_code\":\"%s\"}", device_code);
    pxa_net_header_t headers[3] = {
        {"accept", 6, reinterpret_cast<const uint8_t *>("application/json"), 16},
        {"content-type", 12, reinterpret_cast<const uint8_t *>("application/json"), 16},
        {"authorization", 13, reinterpret_cast<const uint8_t *>(authorization), 0},
    };
    pxa_net_request_t req{};
    req.method = op == Op::authorize || op == Op::poll ? PXA_NET_POST :
        op == Op::revoke || op == Op::likes_delete ? PXA_NET_DELETE :
        op == Op::likes_put ? PXA_NET_PUT : PXA_NET_GET;
    req.url = url; req.url_size = std::strlen(url);
    req.permission_handle = client.network_permission;
    req.max_response_bytes = sizeof(body); req.timeout_ms = 15000;
    req.body = reinterpret_cast<const uint8_t *>(payload); req.body_size = std::strlen(payload);
    req.headers = headers; req.header_count = 2;
    if (token[0] && op != Op::authorize && op != Op::poll) {
        std::snprintf(authorization, sizeof(authorization), "Bearer %s", token);
        headers[2].value_size = std::strlen(authorization); req.header_count = 3;
    }
    operation = op; request_revision = target_revision;
    client.response_size = 0; client.response_flags = 0; client.http_status = 0;
    client.response_length = 0; client.stream_waiting = 0;
    request_id = ++sequence;
    const int32_t status = pxa_net_submit(packet, sizeof(packet), PXA_NET_HTTP_REQUEST, request_id, &req);
    erase(authorization, sizeof(authorization)); erase(payload, sizeof(payload));
    // The Host consumes the control packet synchronously; do not retain a
    // second copy of either bearer or grant secret in its encoding buffer.
    erase(reinterpret_cast<char *>(packet), sizeof(packet));
    if (status != PXA_STATUS_OK) { finish(now, false); return false; }
    return true;
}
void Account::login(uint64_t now) {
    if (!restored || busy()) return;
    if (!client.network_permission) { phase = AccountPhase::failed; changed = true; return; }
    cancel();
    phase = AccountPhase::starting; changed = true; next_attempt = 0;
    submit(Op::authorize, now);
}
void Account::cancel() {
    if (operation == Op::authorize || operation == Op::poll) {
        if (request_id) (void)pxa_cancel(request_id);
        (void)store_client_close_body(&client); request_id = 0; operation = Op::none;
    }
    erase(device_code, sizeof(device_code)); user_code[0] = 0; deadline = poll_at = 0;
    if (!token[0]) phase = AccountPhase::guest;
    changed = true;
}
void Account::logout(uint64_t now) {
    if (request_id) (void)pxa_cancel(request_id);
        (void)store_client_close_body(&client); request_id = 0; operation = Op::none;
    cancel(); restored = true;
    if (token[0] && client.network_permission) submit(Op::revoke, now);
    forget();
}
void Account::retry(uint64_t now) {
    next_attempt = 0;
    if (token[0]) { session_checked = false; schedule(now); }
    else login(now);
}
void Account::target(const store_app_t *item, uint64_t now) {
    const char *id = item ? item->app_id : "", *root = item ? item->lineage_root_key_id : "";
    if (!std::strcmp(id, app_id) && !std::strcmp(root, lineage)) return;
    std::snprintf(app_id, sizeof(app_id), "%s", id);
    std::snprintf(lineage, sizeof(lineage), "%s", root);
    ++target_revision; changed = true;
    like_ready = liked = like_error = false; like_count = 0;
    want_likes = id[0] != 0;
    schedule(now);
}
void Account::toggle_like(uint64_t now) {
    if (!authenticated() || busy()) return;
    if (!like_ready || like_error) { want_likes = true; next_attempt = 0; schedule(now); return; }
    changed = true;
    submit(liked ? Op::likes_delete : Op::likes_put, now);
}
void Account::finish(uint64_t now, bool valid) {
    const Op op = operation;
    if (request_id) (void)pxa_cancel(request_id);
        (void)store_client_close_body(&client); request_id = 0; operation = Op::none;
    store_account_response_t result{};
    valid = valid && store_json_parse_account(body, client.response_size, &result);
    erase(reinterpret_cast<char *>(body), sizeof(body));
    if (op == Op::revoke) { /* Local logout already removed the credential. */ }
    else if (client.http_status == 401 && token[0]) { forget(); }
    else if (op == Op::authorize) {
        if (valid && client.http_status == 200 && credential(result.device_code, "pxac_") &&
            code_valid(result.user_code) && result.expires_in > 0 && result.expires_in <= 600 &&
            result.interval >= 5 && result.interval <= 60) {
            std::memcpy(device_code, result.device_code, sizeof(device_code));
            std::memcpy(user_code, result.user_code, sizeof(user_code));
            interval_ms = result.interval * 1000;
            deadline = now + result.expires_in * 1000; poll_at = now + interval_ms;
            phase = AccountPhase::waiting;
        } else phase = AccountPhase::failed;
        changed = true;
    } else if (op == Op::poll) {
        if (valid && client.http_status == 200 && credential(result.access_token, "pxad_") && (result.fields & 8)) {
            std::memcpy(token, result.access_token, sizeof(token));
            std::memcpy(display_name, result.display_name, sizeof(display_name));
            erase(device_code, sizeof(device_code)); user_code[0] = 0; deadline = 0;
            phase = AccountPhase::signed_in; session_checked = true;
            persistent = pxa_storage_request_set(packet, sizeof(packet), ++storage_id, key, sizeof(key) - 1,
                reinterpret_cast<const uint8_t *>(token), 48) == PXA_STATUS_OK;
            erase(reinterpret_cast<char *>(packet), sizeof(packet));
            want_likes = app_id[0] != 0; changed = true;
        } else if (valid && !std::strcmp(result.error, "access_denied")) {
            cancel(); phase = AccountPhase::denied; changed = true;
        } else if (valid && !std::strcmp(result.error, "expired_token")) {
            cancel(); phase = AccountPhase::expired; changed = true;
        } else {
            if (valid && !std::strcmp(result.error, "slow_down") && interval_ms < 60000) interval_ms += 5000;
            // Transient failures retain the human code and retry at the paced
            // interval. The local deadline also works without connectivity.
            poll_at = now + interval_ms;
        }
    } else if (op == Op::session) {
        if (valid && client.http_status == 200 && (result.fields & 8)) {
            std::memcpy(display_name, result.display_name, sizeof(display_name));
            phase = AccountPhase::signed_in; session_checked = true;
            want_likes = app_id[0] != 0;
        } else { phase = AccountPhase::failed; next_attempt = UINT64_MAX; }
        changed = true;
    } else if (op == Op::likes_get || op == Op::likes_put || op == Op::likes_delete) {
        if (request_revision == target_revision) {
            like_error = !(valid && client.http_status == 200 && (result.fields & 384) == 384);
            if (!like_error) { like_ready = true; like_count = result.count; liked = result.liked; }
            changed = true;
        }
    }
    erase(reinterpret_cast<char *>(&result), sizeof(result));
    if (op != Op::revoke) schedule(now);
}
void Account::continue_body(uint64_t now) {
    const int result = store_client_consume_body(&client, body, sizeof(body));
    if (result == STORE_BODY_WAITING) return;
    const bool length_ok = !(client.response_flags & PXA_NET_BODY_LENGTH_KNOWN) || client.response_length == client.response_size;
    finish(now, result == STORE_BODY_DONE && length_ok);
}
bool Account::event(const pxa_event_t &event, uint64_t now) {
    if (event.service == PXA_STORAGE_SERVICE && event.token == storage_id) {
        if (event.opcode == PXA_STORAGE_GET) {
            pxa_storage_get_result_t result{};
            restored = true;
            if (pxa_storage_parse_get(&event, storage_id, &result) && result.status == PXA_STATUS_OK && result.value.size == 48) {
                std::memcpy(token, result.value.data, 48); token[48] = 0;
                if (!credential(token, "pxad_")) forget();
                else { phase = AccountPhase::restoring; changed = true; }
            }
        } else if (event.opcode == PXA_STORAGE_SET) {
            int32_t status;
            persistent = pxa_storage_parse_status(&event, storage_id, PXA_STORAGE_SET, &status) && status == PXA_STATUS_OK;
            changed = true;
        }
        schedule(now); return true;
    }
    if (event.service != PXA_NET_SERVICE || event.opcode != PXA_NET_HTTP_REQUEST || (event.token >> 56) != 0x41) return false;
    pxa_net_result_t result{};
    const bool parsed = pxa_net_parse_result(&event, event.token, PXA_NET_HTTP_REQUEST, &result);
    if (event.token != request_id) {
        if (parsed && result.body_handle) { store_client_t stale{}; stale.body_handle = result.body_handle; store_client_close_body(&stale); }
        return true;
    }
    client.body_handle = parsed ? result.body_handle : 0; client.http_status = parsed ? result.status_code : 0;
    client.response_flags = result.flags; client.response_length = result.body_length;
    if (!parsed || result.status != PXA_STATUS_OK || !result.body_handle) finish(now, false);
    else continue_body(now);
    return true;
}
void Account::tick(uint64_t now) {
    if (phase == AccountPhase::waiting && now >= deadline) { cancel(); phase = AccountPhase::expired; changed = true; }
    if (client.stream_waiting && client.body_handle) { client.stream_waiting = 0; continue_body(now); }
    if (!paused && phase == AccountPhase::waiting && !busy() && now >= poll_at) submit(Op::poll, now);
}
uint16_t Account::clock_period() const {
    return client.stream_waiting ? 50 : !paused && phase == AccountPhase::waiting ? 1000 : 0;
}
void Account::stop() {
    if (request_id) (void)pxa_cancel(request_id);
    (void)store_client_close_body(&client);
    erase(token, sizeof(token)); erase(device_code, sizeof(device_code));
    erase(reinterpret_cast<char *>(packet), sizeof(packet)); erase(reinterpret_cast<char *>(body), sizeof(body));
}
}
