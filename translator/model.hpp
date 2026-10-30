// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
namespace translate {
constexpr size_t InputBytes = 1537, OutputBytes = 4097, HistoryCount = 4;
struct Language {
  const char *name, *code, *libre, *speech;
};
inline constexpr Language Languages[] = {
    {"简体中文", "zh-CN", "zh", "zh"}, {"英语", "en", "en", "en"},
    {"日语", "ja", "ja", "ja"},        {"韩语", "ko", "ko", "ko"},
    {"法语", "fr", "fr", "fr"},        {"德语", "de", "de", "de"},
    {"西班牙语", "es", "es", "es"},    {"俄语", "ru", "ru", "ru"},
    {"繁体中文", "zh-TW", "zh", "zh"},
};
constexpr unsigned LanguageCount = sizeof(Languages) / sizeof(Languages[0]);
struct Settings {
  uint8_t source = 0, target = 1, provider = 0, asr_preset = 0;
  bool history = true;
  char libre_url[257]{}, libre_key[257]{};
  char asr_url[257] = "https://api.groq.com/openai/v1/audio/transcriptions";
  char asr_key[257]{};
  char asr_model[97] = "whisper-large-v3-turbo";
};
struct Record {
  uint8_t source = 0, target = 1;
  char input[InputBytes]{}, output[OutputBytes]{};
};
enum class Parse { Ok, Invalid, Quota, TooLong, Empty };
bool utf8(const char *, size_t, size_t *characters = nullptr);
bool valid_input(const char *);
bool endpoint(const char *, bool secret = false);
bool header_value(const char *);
bool model_name(const char *);
bool json_quote(const char *, char *, size_t);
bool translation_url(const char *, size_t, unsigned, unsigned, char *, size_t,
                     size_t &);
bool tts_url(const char *, size_t, unsigned, char *, size_t, size_t &);
bool libre_body(const char *, const Settings &, char *, size_t);
Parse parse_translation(const uint8_t *, size_t, bool, char *, size_t);
Parse parse_transcription(const uint8_t *, size_t, char *, size_t);
uint32_t hash(const uint8_t *, size_t);
size_t save_settings(const Settings &, uint8_t *, size_t);
bool load_settings(const uint8_t *, size_t, Settings &);
size_t save_record(const Record &, uint8_t *, size_t);
bool load_record(const uint8_t *, size_t, Record &);
void asr_preset(Settings &, unsigned);
} // namespace translate
