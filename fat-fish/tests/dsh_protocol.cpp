// SPDX-License-Identifier: MIT
#include "../dsh_protocol.hpp"
#include "../voice_protocol.hpp"
#include <cassert>
#include <string>

int main() {
    using namespace fish::dsh;
    Config config;
    const char *raw = R"({"url":"http://192.168.5.226:7341/dsh-pet-7340/","pet":"main.pet","token":"abc_123"})";
    assert(parse_config((const uint8_t *)raw, std::strlen(raw), config));
    assert(!std::strcmp(config.url, "http://192.168.5.226:7341/dsh-pet-7340"));
    char encoded[1024], url[513];
    size_t size = encode_config(config, encoded, sizeof(encoded));
    Config decoded;
    assert(size && parse_config((const uint8_t *)encoded, size, decoded));
    assert(!std::strcmp(config.token, decoded.token));
    assert(address(config, "chat", url, sizeof(url)));
    assert(std::strstr(url, "/chat?pet=main.pet&surface=any"));
    assert(!address(config, "chat", url, 8));
    for (const char *bad : {"http://", "file:///tmp", "http://user@host/api", "http://host/api?token=secret", "http://host/a b", "http://host/a\\b"}) {
        std::snprintf(decoded.url, sizeof(decoded.url), "%s", bad);
        assert(!valid(decoded));
    }
    std::snprintf(decoded.url, sizeof(decoded.url), "https://example.org/dsh-pet-7340");
    assert(valid(decoded));
    std::snprintf(decoded.token, sizeof(decoded.token), "bad\nheader");
    assert(!valid(decoded));
    const char *text = "你好，\"小鱼\"！\n\\bye\t";
    assert(quote(text, encoded, sizeof(encoded)));
    char restored[128];
    assert((media::json::Value{(const uint8_t *)encoded, std::strlen(encoded)}.string(restored, sizeof(restored))));
    assert(!std::strcmp(restored, text));
    assert(!quote(text, encoded, 5));
    Reply reply;
    for (const char *good : {R"({"ok":true,"text":"你好！"})", R"({"ok":true,"text":"\u4f60\u597d\ud83d\udc1f"})"}) {
        assert(parse_reply((const uint8_t *)good, std::strlen(good), reply));
        assert(reply.ok && reply.text[0]);
    }
    const char *missing = R"({"ok":false,"reason":"provider-missing","message":"当前对话未配置模型"})";
    assert(parse_reply((const uint8_t *)missing, std::strlen(missing), reply));
    assert(!reply.ok && !std::strcmp(reply.reason, "provider-missing"));
    for (const char *bad : {"{}", "{", R"({"ok":true})", R"({"ok":"true","text":"hi"})", R"({"ok":true,"text":""})", R"({"ok":true,"text":"\ud800"})", R"({"ok":true,"text":"hi"}junk)"})
        assert(!parse_reply((const uint8_t *)bad, std::strlen(bad), reply));
    std::string long_reply = R"({"ok":true,"text":")";
    for (int i = 0; i < 1000; ++i) long_reply += "鱼";
    long_reply += "\"}";
    assert(parse_reply((const uint8_t *)long_reply.data(), long_reply.size(), reply));
    assert(reply.truncated && std::strlen(reply.text) % 3 == 0);
    const char *state = R"({"pets":{"main.pet":{"anim":{"counter":123,"data":{"name":"轻快摇摆舞","playbackId":"abc"}}}}})";
    char name[64];
    assert(animation((const uint8_t *)state, std::strlen(state), "main.pet", name, sizeof(name)) == 123);
    assert(!std::strcmp(name, "轻快摇摆舞"));
    assert(animation((const uint8_t *)state, std::strlen(state), "main", name, sizeof(name)) == 0);
    const std::string bundled = std::string("{\"ok\":true,\"text\":\"hello\",\"pxa_state\":") + state + "}";
    const auto snapshot = bundled_state((const uint8_t *)bundled.data(), bundled.size(), "main.pet");
    assert(snapshot && animation(snapshot.p, snapshot.n, "main.pet", name, sizeof(name)) == 123);
    assert(!bundled_state((const uint8_t *)bundled.data(), bundled.size(), "missing"));
    for (const char *legacy : {"{\"ok\":true,\"text\":\"hello\"}",
            "{\"pxa_state\":null}", "{\"pxa_state\":{\"pets\":{\"main\":false}}}", "{"})
        assert(!bundled_state((const uint8_t *)legacy, std::strlen(legacy), "main"));
    for (const auto &sample : {std::pair{"f", "Zg=="}, std::pair{"fo", "Zm8="}, std::pair{"foo", "Zm9v"}}) {
        assert(fish::voice::audio_json((const uint8_t *)sample.first, std::strlen(sample.first), encoded, sizeof(encoded)));
        assert((media::json::Value{(const uint8_t *)encoded, std::strlen(encoded)}.get("audio").string(restored, sizeof(restored))));
        assert(!std::strcmp(restored, sample.second));
    }
    assert(!fish::voice::audio_json(nullptr, 10, encoded, sizeof(encoded)));
    assert(!fish::voice::audio_json((const uint8_t *)"foo", 3, encoded, 8));
    std::string maximum(128 * 1024, 'x'), request(176 * 1024, '\0');
    assert(fish::voice::audio_json((const uint8_t *)maximum.data(), maximum.size(), request.data(), request.size()));
    assert(!fish::voice::audio_json((const uint8_t *)maximum.data(), maximum.size() + 1, request.data(), request.size()));
    std::puts("dsh-pet protocol tests passed");
}
