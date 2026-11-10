// SPDX-License-Identifier: MIT
#pragma once
#include "pxa_http.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace fish::dsh {
// Bounded, single-flight version of the callback HTTP loop used by Translator.
struct Http {
    uint8_t packet[4096]{}, header[1024]{}, data[131072]{};
    uint64_t token = 0, handle = 0, deadline = 0;
    size_t used = 0;
    unsigned status = 0;
    int error = 0;
    bool headers = false, active = false, done = false;
    const uint8_t *upload = nullptr;
    size_t upload_size = 0, upload_offset = 0;
    bool upload_finished = true;

    void cancel() {
        if (token) (void)pxa_cancel(token);
        if (handle) (void)pxa_close_handle(handle);
        token = handle = 0; active = done = false;
    }
    void finish(int result = 0) {
        cancel(); error = result; done = true;
    }
    bool begin(uint64_t next, uint64_t permission, uint64_t now,
               const char *url, const char *key, const char *body = nullptr) {
        cancel();
        char authorization[140];
        std::snprintf(authorization, sizeof(authorization), "Bearer %s", key);
        static const char json[] = "application/json";
        pxa_net_header_t fields[] = {
            {"accept", 6, (const uint8_t *)json, sizeof(json) - 1},
            {"content-type", 12, (const uint8_t *)json, sizeof(json) - 1},
            {"authorization", 13, (const uint8_t *)authorization, uint16_t(std::strlen(authorization))}
        };
        auto request = pxa_http_request_defaults();
        request.url = url; request.url_size = uint16_t(std::strlen(url));
        request.permission_handle = permission;
        request.method = body ? PXA_NET_POST : PXA_NET_GET;
        upload = (const uint8_t *)body;
        upload_size = body ? std::strlen(body) : 0;
        upload_offset = 0;
        upload_finished = upload_size <= 2048;
        request.body = upload_finished ? upload : nullptr;
        request.body_size = upload_finished ? uint16_t(upload_size) : 0;
        request.upload_bytes = upload_size;
        request.max_body_bytes = sizeof(data);
        request.timeout_ms = 60000;
        request.header_bytes = sizeof(header);
        request.headers = fields;
        request.header_count = key[0] ? 3 : 2;
        used = status = 0; error = 0; headers = false;
        deadline = now + 65000;
        token = next; active = true;
        int result = pxa_http_open(token, &request, packet, sizeof(packet));
        if (result) { finish(result); return false; }
        return true;
    }
    bool event(const pxa_event_t &event) {
        if (event.service != PXA_NET_SERVICE || event.opcode != PXA_HTTP_OPEN) return false;
        pxa_http_open_result_t result{};
        if (!pxa_http_parse_open(&event, event.token, &result)) return false;
        if (event.token != token || !active) {
            if (result.handle) (void)pxa_close_handle(result.handle);
            return true;
        }
        token = 0;
        if (result.status) finish(result.status);
        else handle = result.handle;
        return true;
    }
    void pump(uint64_t now) {
        if (!active) return;
        if (now >= deadline) { finish(PXA_STATUS_TIMED_OUT); return; }
        if (!handle) return;
        if (!upload_finished) {
            for (unsigned work = 0; work < 4 && upload_offset < upload_size; ++work) {
                uint32_t bytes = uint32_t(std::min<size_t>(2048, upload_size - upload_offset));
                int n = pxa_http_write(handle, upload + upload_offset, bytes);
                if (n == PXA_STATUS_WOULD_BLOCK) return;
                if (n <= 0 || unsigned(n) > bytes) { finish(n < 0 ? n : PXA_STATUS_PROTOCOL_ERROR); return; }
                upload_offset += unsigned(n);
            }
            if (upload_offset != upload_size) return;
            int result = pxa_http_finish_upload(handle);
            if (result == PXA_STATUS_WOULD_BLOCK) return;
            // A small response can complete after all bytes are accepted but
            // before this marker reaches the Host. Its headers/body are ready.
            if (result && result != PXA_STATUS_BAD_STATE) { finish(result); return; }
            upload_finished = true;
        }
        if (!headers) {
            int n = pxa_http_read_response(handle, header, sizeof(header));
            if (n == PXA_STATUS_WOULD_BLOCK) return;
            pxa_http_response_t response{};
            if (n < 0 || !pxa_http_parse_response(header, size_t(n), &response)) {
                finish(n < 0 ? n : PXA_STATUS_PROTOCOL_ERROR); return;
            }
            status = response.status_code; headers = true;
            if (response.body_length_known && response.body_length > sizeof(data)) {
                finish(PXA_STATUS_PROTOCOL_ERROR); return;
            }
        }
        for (unsigned work = 0; work < 4; ++work) {
            uint8_t overflow;
            uint8_t *out = used < sizeof(data) ? data + used : &overflow;
            uint32_t bytes = uint32_t(used < sizeof(data) ? std::min<size_t>(2048, sizeof(data) - used) : 1);
            int n = pxa_http_read(handle, out, bytes);
            if (n == PXA_STATUS_WOULD_BLOCK) return;
            if (n < 0 || size_t(n) > bytes || (n && used == sizeof(data))) {
                finish(n < 0 ? n : PXA_STATUS_PROTOCOL_ERROR); return;
            }
            if (!n) { finish(); return; }
            used += size_t(n);
        }
    }
};
} // namespace fish::dsh
