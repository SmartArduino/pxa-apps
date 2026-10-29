/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 * PXA adaptation of esp_xiaozhi at 12ba34b0d5db3f6811f80c2de2dd292ddc29ef1b.
 */
#pragma once
#include "vendor/cjson/cJSON.h"
#include <memory>
#include <string>
#include <string_view>
namespace xiaozhi {
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
inline Json parse(std::string_view input) {
  const char *end = nullptr;
  Json root{cJSON_ParseWithLengthOpts(input.data(), input.size(), &end, 0),
            cJSON_Delete};
  while (end && end < input.data() + input.size() &&
         (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
    ++end;
  if (end != input.data() + input.size() || !cJSON_IsObject(root.get()))
    root.reset();
  return root;
}
inline std::string_view text(const cJSON *root, const char *key) {
  auto value = cJSON_GetObjectItemCaseSensitive(root, key);
  return cJSON_IsString(value) ? std::string_view(value->valuestring)
                               : std::string_view{};
}
inline std::string encode(const cJSON *root) {
  char *raw = cJSON_PrintUnformatted(root);
  if (!raw)
    return {};
  std::string out(raw);
  cJSON_free(raw);
  return out;
}
inline bool safe_header(std::string_view value, std::size_t limit) {
  if (value.size() > limit)
    return false;
  for (unsigned char c : value)
    if (c < 32 || c == 127)
      return false;
  return true;
}
inline bool session_valid(std::string_view id) {
  if (id.empty() || id.size() > 127)
    return false;
  for (unsigned char c : id)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '_'))
      return false;
  return true;
}
inline constexpr std::string_view hello =
    R"({"type":"hello","version":1,"transport":"websocket","features":{"mcp":false},"audio_params":{"format":"opus","sample_rate":16000,"channels":1,"frame_duration":20}})";
inline std::string hello_for(unsigned frame_ms) {
  if (frame_ms != 20 && frame_ms != 40 && frame_ms != 60)
    return {};
  auto root = parse(hello);
  if (!root)
    return {};
  auto params = cJSON_GetObjectItemCaseSensitive(root.get(), "audio_params");
  cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(params, "frame_duration"), frame_ms);
  return encode(root.get());
}
inline std::string command(std::string_view session, const char *type,
                           const char *state = nullptr,
                           const char *content = nullptr) {
  if (!session_valid(session))
    return {};
  Json root{cJSON_CreateObject(), cJSON_Delete};
  if (!root)
    return {};
  std::string id(session);
  cJSON_AddStringToObject(root.get(), "session_id", id.c_str());
  cJSON_AddStringToObject(root.get(), "type", type);
  if (state)
    cJSON_AddStringToObject(root.get(), "state", state);
  if (std::string_view(type) == "listen" && state &&
      std::string_view(state) == "start")
    cJSON_AddStringToObject(root.get(), "mode", "manual");
  if (content)
    cJSON_AddStringToObject(root.get(), "text", content);
  return encode(root.get());
}
inline std::string abort_command(std::string_view session, bool wake_word = false) {
  auto root = parse(command(session, "abort"));
  if (!root) return {};
  if (wake_word)
    cJSON_AddStringToObject(root.get(), "reason", "wake_word_detected");
  return encode(root.get());
}
} // namespace xiaozhi
