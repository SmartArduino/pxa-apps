// SPDX-License-Identifier: MIT
#pragma once
#include "pxa_http.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
namespace translate {
struct Upload {
  const uint8_t *data = nullptr;
  size_t size = 0;
};
// A single bounded request. Cancellation revokes the pending token and
// resource; late OPEN completions are closed without exposing their body to
// another job.
struct Http {
  uint8_t packet[4096]{}, header[4096]{}, data[128 * 1024]{};
  char url[513]{}, content_type[128]{}, authorization[300]{};
  Upload upload[3]{};
  uint64_t token = 0, handle = 0, next_token = 1000, deadline = 0;
  uint64_t *tokens = nullptr;
  size_t used = 0, limit = 0, part = 0, part_offset = 0;
  unsigned status = 0;
  bool active = false, headers = false, finished_upload = false, done = false,
       failed = false;
  void cancel() {
    if (token)
      pxa_cancel(token);
    if (handle)
      pxa_close_handle(handle);
    token = handle = 0;
    active = false;
    done = failed = false;
  }
  bool begin(const char *address, uint64_t permission, uint64_t now,
             const char *type = nullptr, const Upload *body = nullptr,
             unsigned count = 0, const char *key = nullptr,
             size_t max_bytes = 32768) {
    cancel();
    if (!permission || std::strlen(address) > 512 || count > 3 ||
        max_bytes > sizeof(data))
      return false;
    std::strcpy(url, address);
    std::snprintf(content_type, sizeof(content_type), "%s", type ? type : "");
    std::snprintf(authorization, sizeof(authorization), "%s%s",
                  key && *key ? "Bearer " : "", key && *key ? key : "");
    uint64_t bytes = 0;
    for (unsigned i = 0; i < 3; ++i) {
      upload[i] = i < count ? body[i] : Upload{};
      bytes += upload[i].size;
    }
    pxa_net_header_t fields[4]{};
    unsigned fields_count = 0;
    auto add = [&](const char *name, const char *value) {
      fields[fields_count++] = {name, uint16_t(std::strlen(name)),
                                (const uint8_t *)value,
                                uint16_t(std::strlen(value))};
    };
    add("User-Agent", "PXA-Translator/0.1");
    add("Accept", type ? "application/json" : "*/*");
    if (type)
      add("Content-Type", content_type);
    if (authorization[0])
      add("Authorization", authorization);
    auto request = pxa_http_request_defaults();
    request.url = url;
    request.url_size = uint16_t(std::strlen(url));
    request.permission_handle = permission;
    request.method = count ? PXA_NET_POST : PXA_NET_GET;
    request.upload_bytes = bytes;
    request.max_body_bytes = max_bytes;
    request.buffer_bytes = 4096;
    request.header_bytes = 2048;
    request.timeout_ms = 60000;
    request.headers = fields;
    request.header_count = fields_count;
    used = part = part_offset = status = 0;
    limit = max_bytes;
    headers = false;
    done = failed = false;
    finished_upload = !bytes;
    deadline = now + 65000;
    active = true;
    token = tokens ? ++*tokens : ++next_token;
    if (pxa_http_open(token, &request, packet, sizeof(packet))) {
      token = 0;
      active = false;
      failed = true;
      return false;
    }
    return true;
  }
  bool event(const pxa_event_t &event) {
    if (event.service != PXA_NET_SERVICE || event.opcode != PXA_HTTP_OPEN)
      return false;
    pxa_http_open_result_t result{};
    if (!pxa_http_parse_open(&event, event.token, &result))
      return false;
    if (event.token != token || !active) {
      if (result.handle)
        pxa_close_handle(result.handle);
      return true;
    }
    token = 0;
    if (result.status) {
      failed = done = true;
      active = false;
    } else
      handle = result.handle;
    return true;
  }
  void fail() {
    if (handle)
      pxa_close_handle(handle);
    handle = 0;
    if (token)
      pxa_cancel(token);
    token = 0;
    failed = done = true;
    active = false;
  }
  void pump(uint64_t now) {
    if (!active)
      return;
    if (now >= deadline) {
      fail();
      return;
    }
    if (!handle)
      return;
    if (!finished_upload) {
      for (unsigned work = 0; work < 4; ++work) {
        while (part < 3 && part_offset == upload[part].size) {
          ++part;
          part_offset = 0;
        }
        if (part == 3) {
          int n = pxa_http_finish_upload(handle);
          if (n == PXA_STATUS_WOULD_BLOCK)
            return;
          if (n < 0) {
            fail();
            return;
          }
          finished_upload = true;
          break;
        }
        auto &p = upload[part];
        int n = pxa_http_write(
            handle, p.data + part_offset,
            uint32_t(std::min<size_t>(2048, p.size - part_offset)));
        if (n == PXA_STATUS_WOULD_BLOCK)
          return;
        if (n <= 0 || size_t(n) > p.size - part_offset) {
          fail();
          return;
        }
        part_offset += size_t(n);
      }
      if (!finished_upload)
        return;
    }
    if (!headers) {
      int n = pxa_http_read_response(handle, header, sizeof(header));
      if (n == PXA_STATUS_WOULD_BLOCK)
        return;
      pxa_http_response_t response{};
      if (n < 0 || !pxa_http_parse_response(header, n, &response)) {
        fail();
        return;
      }
      status = response.status_code;
      headers = true;
      if (status < 200 || status >= 300) {
        fail();
        return;
      }
      if (response.body_length_known && response.body_length > limit) {
        fail();
        return;
      }
    }
    for (unsigned work = 0; work < 4; ++work) {
      uint8_t overflow;
      uint8_t *out = used < limit ? data + used : &overflow;
      uint32_t bytes =
          uint32_t(used < limit ? std::min<size_t>(4096, limit - used) : 1);
      int n = pxa_http_read(handle, out, bytes);
      if (n == PXA_STATUS_WOULD_BLOCK)
        return;
      if (n < 0 || (n > 0 && used == limit)) {
        fail();
        return;
      }
      if (!n) {
        pxa_close_handle(handle);
        handle = 0;
        active = false;
        done = true;
        return;
      }
      used += size_t(n);
    }
  }
};
} // namespace translate
