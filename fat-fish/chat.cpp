// SPDX-License-Identifier: MIT
#include "chat.hpp"
#include "dsh_protocol.hpp"
#include "dsh_http.hpp"
#include "voice.hpp"
#include "voice_protocol.hpp"
#include "animation_data.hpp"
#include "../common/pocket-play/runtime.hpp"
#include "pxa_assets.h"
#include "pxa_permission.h"
#include "pxa_storage.h"
#include "pxa_ui.h"

namespace fish::chat {
namespace {
using pocket::tr;
constexpr uint64_t LOAD = UINT64_C(1) << 48, BOOTSTRAP = LOAD + 1, SAVE = LOAD + 2;
constexpr char STORAGE_KEY[] = "pxa-fat-fish.dsh.v1";
enum Node : unsigned {
    Root = 40, Header, Back, Title, Settings, Status, History, Transcript,
    Presets, Hello, Topic, Composer, Input, Send, Cancel,
    Mic, Form = 70, UrlLabel, Url, PetLabel, Pet, TokenLabel, Token, Save
};
enum class Job { NONE, BASELINE, CHAT, STATE, HEAR, LISTEN, SPEAK };
dsh::Config config, editing;
dsh::Http http;
dsh::Reply last_reply;
struct Message { bool pet; char text[2049]; } messages[6];
unsigned message_count;
char draft[513]{}, submitted[513]{}, draft_snapshot[513]{}, body[2048]{}, address[513]{};
char status_text[256]{}, transcript[8192]{}, storage_value[1024]{};
char audio_request[176 * 1024];
uint8_t scratch[4096];
uint64_t sequence = LOAD + 16, permission, permission_token, now_ms, save_retry;
uint64_t animation_counter;
uint32_t generation = 100, mode_generation;
Job job = Job::NONE;
Job permission_job = Job::BASELINE;
int next_animation = -1;
bool opened, configuring, created, created_form, shown, dirty, clear_input;
bool loading, config_dirty, saving, reply_ready, previous_english;
bool sent_draft;
bool scroll_to_end;
bool transcript_dirty;
bool ai_mode;
uint64_t ai_session, ai_origin;
uint64_t listen_after;
bool was_playing;

bool busy() { return job != Job::NONE || permission_token || voice::busy(); }
void copy_text(char *out, size_t capacity, const char *text) {
    size_t n = std::min(capacity - 1, std::strlen(text));
    while (n && ((unsigned char)text[n] & 0xc0) == 0x80) --n;
    std::memcpy(out, text, n); out[n] = 0;
}
void status(const char *en, const char *zh) {
    copy_text(status_text, sizeof(status_text), tr(en, zh)); dirty = true;
}
void add_message(bool pet, const char *text) {
    transcript_dirty = true;
    if (message_count == 6) {
        std::memmove(messages, messages + 1, sizeof(messages) - sizeof(messages[0]));
        --message_count;
    }
    auto &message = messages[message_count++];
    message.pet = pet; copy_text(message.text, sizeof(message.text), text);
}
void cancel() {
    ai_mode = false;
    voice::cancel();
    http.cancel(); job = Job::NONE;
    if (permission_token) (void)pxa_cancel(permission_token);
    permission_token = 0;
    status("Cancelled. Your draft is kept.", "已取消，可以修改后重新发送。");
    pocket::log("FatFish: chat cancelled");
}
void fail_request() {
    if (ai_mode) { ai_mode = false; voice::cancel(); }
    char line[128];
    std::snprintf(line, sizeof(line), "FatFish: chat transport status=%u error=%d", http.status, http.error);
    pocket::log(line);
    if (http.status == 401 || http.status == 403)
        status("Check the service token in Settings.", "服务拒绝访问，请检查服务令牌。");
    else if (http.error == PXA_STATUS_TIMED_OUT)
        status("Request timed out. You can retry.", "等待回复超时，可以重新发送。");
    else if (http.status == 404)
        status("Check the URL, including /dsh-pet-7340.", "地址未找到，请包含 /dsh-pet-7340 路径。");
    else status("Cannot reach dsh-pet. Check Wi-Fi and server.", "连接失败，请检查 Wi-Fi 和电脑端服务。");
    job = Job::NONE;
}
void begin(Job next) {
    job = next;
    const char *endpoint = next == Job::HEAR || next == Job::LISTEN ? "voice/hear" : next == Job::SPEAK ? "voice/speak" :
                           next == Job::CHAT ? "chat" : "state";
    if (!dsh::address(config, endpoint, address, sizeof(address))) {
        job = Job::NONE;
        status("Check the service address.", "请检查服务地址。"); return;
    }
    (void)http.begin(++sequence, permission, now_ms, address, config.token,
                     next == Job::HEAR || next == Job::LISTEN ? audio_request : next == Job::CHAT || next == Job::SPEAK ? body : nullptr);
}
void network(Job next) {
    if (permission) { begin(next); return; }
    permission_job = next;
    uint32_t written;
    permission_token = ++sequence;
    if (!pxa_permission_build(scratch, sizeof(scratch), PXA_PERMISSION_ACQUIRE,
            permission_token, "net.client", 10, (const uint8_t *)"web", 3, &written) ||
        pxa_control_packet(scratch, written)) {
        permission_token = 0;
        if (ai_mode) { ai_mode = false; voice::cancel(); }
        status("Cannot request network access.", "暂时无法申请网络权限，请重试。");
    }
}
void send(const char *text) {
    if (busy() || loading) return;
    sent_draft = text == draft;
    copy_text(draft_snapshot, sizeof(draft_snapshot), draft);
    if (!dsh::valid(config)) {
        editing = config; configuring = true; dirty = true;
        status("Enter your computer's dsh-pet address.", "请先填写电脑端 dsh-pet 服务地址。"); return;
    }
    while (*text == ' ' || *text == '\n' || *text == '\t') ++text;
    if (!*text) { status("Write something first.", "先写一句想说的话吧。"); return; }
    copy_text(submitted, sizeof(submitted), text);
    char quoted[2038];
    if (!dsh::quote(submitted, quoted, sizeof(quoted))) {
        status("Please shorten this message.", "这句话有点长，请缩短后再发送。"); return;
    }
    std::snprintf(body, sizeof(body), "{\"text\":%s}", quoted);
    status("Waiting for Fat Fish...", "正在等大肥鱼回复……");
    pocket::log("FatFish: chat sending");
    network(Job::BASELINE);
}
uint64_t state_animation(char *name, size_t capacity) {
    return dsh::animation(http.data, http.used, config.pet, name, capacity);
}
void speak_reply(const uint8_t *state_data, size_t state_size) {
    char name[128]{};
    uint64_t counter = dsh::animation(state_data, state_size, config.pet, name, sizeof(name));
    if (counter && counter != animation_counter) {
        const char *names[] = {"待机呼吸休闲", "点击回应-元气挥手", "点击回应-开心跃动",
            "点击回应-挠痒咯咯笑", "点击回应-害羞惊讶", "大口吃零食", "涮火锅",
            "原地专心玩魔方", "轻快摇摆舞", "鲸鱼吐泡泡特效", "超大伸懒腰",
            "深度思考碎碎念", "睡觉-开始", "睡觉-循环", "睡觉-起来", "原地左转奔跑"};
        for (unsigned i = 0; i < art::COUNT; ++i)
            if (!std::strcmp(name, names[i])) next_animation = int(i);
        animation_counter = counter;
    }
    char spoken[241], quoted[1500];
    copy_text(spoken, sizeof(spoken), last_reply.text);
    if (dsh::quote(spoken, quoted, sizeof(quoted))) {
        std::snprintf(body, sizeof(body), "{\"text\":%s}", quoted);
        status("Preparing local speech...", "正在准备本地语音……"); begin(Job::SPEAK);
    } else { job = Job::NONE; dirty = true; }
}
void finish_request() {
    http.done = false;
    if (job == Job::SPEAK) {
        job = Job::NONE;
        if (http.error || http.status != 200 || !http.used || dsh::document(http.data, http.used)) {
            if (ai_mode) { ai_mode = false; voice::cancel(); }
            status("Speech unavailable. Your reply is kept.", "语音暂不可用，可以继续查看文字回复。"); return;
        }
        voice::play(http.data, http.used); dirty = true; return;
    }
    if (job == Job::STATE) {
        speak_reply(!http.error && http.status == 200 ? http.data : nullptr, http.used);
        return;
    }
    if (http.error || http.status != 200) {
        if (ai_mode) { ai_mode = false; voice::cancel(); }
        fail_request(); return;
    }
    if (job == Job::LISTEN) {
        job = Job::NONE;
        media::json::Value document{http.data, http.used};
        char heard[513]{};
        bool truncated = false;
        if (!dsh::document(http.data, http.used) || !document.get("ok").boolean() ||
            !document.get("text").string(heard, sizeof(heard), &truncated) || truncated) {
            ai_mode = false; voice::cancel();
            status("Speech recognition failed.", "语音识别暂不可用，点击 AI 语音重试。"); return;
        }
        if (!document.get("finished").boolean()) return;
        if (!heard[0]) { ai_session = ++sequence; return; }
        voice::cancel(); ai_session = ++sequence;
        pocket::log("FatFish: room speech recognized"); send(heard); return;
    }
    if (job == Job::HEAR) {
        job = Job::NONE;
        media::json::Value document{http.data, http.used};
        bool truncated = false;
        if (!dsh::document(http.data, http.used) || !document.get("ok").boolean() ||
            !document.get("text").string(draft, sizeof(draft), &truncated) || truncated) {
            status("Speech recognition failed. Try again.", "语音没有识别成功，请再试一次。"); return;
        }
        clear_input = true;
        if (!draft[0]) { status("I couldn't hear clearly. Hold to try again.", "没听清楚，再按住说一句吧。"); return; }
        pocket::log("FatFish: local speech recognized"); send(draft); return;
    }
    if (job == Job::BASELINE) {
        if (!dsh::document(http.data, http.used)) { fail_request(); return; }
        char name[128]; animation_counter = state_animation(name, sizeof(name));
        begin(Job::CHAT); return;
    }
    dsh::Reply reply;
    if (!dsh::parse_reply(http.data, http.used, reply)) {
        if (ai_mode) { ai_mode = false; voice::cancel(); }
        job = Job::NONE;
        status("The service returned an invalid chat response.", "服务返回了无法识别的聊天响应。"); return;
    }
    if (!reply.ok) {
        if (ai_mode) { ai_mode = false; voice::cancel(); }
        job = Job::NONE;
        if (!std::strcmp(reply.reason, "provider-missing"))
            status("Connected. Configure a model on your computer.", "服务已连接，请在电脑端配置聊天模型。");
        else if (reply.text[0]) { copy_text(status_text, sizeof(status_text), reply.text); dirty = true; }
        else status("dsh-pet could not generate a reply.", "dsh-pet 暂时无法生成回复，请稍后重试。");
        char line[128];
        std::snprintf(line, sizeof(line), "FatFish: chat rejected reason=%s", reply.reason);
        pocket::log(line); return;
    }
    last_reply = reply;
    add_message(false, submitted); add_message(true, last_reply.text);
    scroll_to_end = true;
    if (sent_draft && !std::strcmp(draft, draft_snapshot)) { draft[0] = 0; clear_input = true; }
    reply_ready = true; next_animation = -1;
    status(last_reply.truncated ? "Reply received. Long text is shortened." : "Connected to dsh-pet",
           last_reply.truncated ? "已收到回复，长文本已截短显示。" : "已连接 dsh-pet");
    pocket::log("FatFish: chat reply received");
    const auto snapshot = dsh::bundled_state(http.data, http.used, config.pet);
    if (snapshot) speak_reply(snapshot.p, snapshot.n);
    else begin(Job::STATE);
}

bool node(pxa_ui_transaction_t &tx, unsigned id, unsigned parent, unsigned kind) {
    return pxa_ui_create(&tx, id, parent, 0, uint8_t(kind));
}
bool width(pxa_ui_transaction_t &tx, unsigned id) {
    return pxa_ui_set_length(&tx, id, PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_FILL, 0);
}
bool text(pxa_ui_transaction_t &tx, unsigned id, const char *value) {
    return pxa_ui_set_text(&tx, id, value, std::strlen(value));
}
bool label(pxa_ui_transaction_t &tx, unsigned id, unsigned parent, const char *value) {
    return node(tx, id, parent, PXA_UI_NODE_TEXT) && width(tx, id) && text(tx, id, value) &&
        pxa_ui_set_font_role(&tx, id, PXA_UI_FONT_ROLE_CAPTION) &&
        pxa_ui_set_rgba(&tx, id, PXA_UI_PROPERTY_FOREGROUND, 0x384355ff);
}
bool box(pxa_ui_transaction_t &tx, unsigned id, unsigned parent, unsigned layout) {
    return node(tx, id, parent, PXA_UI_NODE_BOX) && width(tx, id) &&
        pxa_ui_set_u8(&tx, id, PXA_UI_PROPERTY_LAYOUT, uint8_t(layout)) &&
        pxa_ui_set_dp(&tx, id, PXA_UI_PROPERTY_GAP, 6);
}
bool button(pxa_ui_transaction_t &tx, unsigned id, unsigned parent, const char *value) {
    return pxa_ui_create_typed(&tx, id, parent, 0, PXA_UI_NODE_CONTROL, PXA_UI_CONTROL_BUTTON) &&
        text(tx, id, value) && pxa_ui_set_font_role(&tx, id, PXA_UI_FONT_ROLE_CAPTION) &&
        pxa_ui_set_length(&tx, id, PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX, 30) &&
        pxa_ui_set_padding(&tx, id, 8, 4, 8, 4) &&
        pxa_ui_set_dp(&tx, id, PXA_UI_PROPERTY_RADIUS, 9) &&
        pxa_ui_set_dp(&tx, id, PXA_UI_PROPERTY_BORDER_WIDTH, 1) &&
        pxa_ui_set_rgba(&tx, id, PXA_UI_PROPERTY_BORDER_COLOR, 0xa3cab9ff) &&
        pxa_ui_set_rgba(&tx, id, PXA_UI_PROPERTY_BACKGROUND, 0xe0efe9ff) &&
        pxa_ui_set_rgba(&tx, id, PXA_UI_PROPERTY_FOREGROUND, 0x384355ff) &&
        pxa_ui_set_event_mask(&tx, id, PXA_UI_EVENT_MASK_CLICK);
}
bool input(pxa_ui_transaction_t &tx, unsigned id, unsigned parent, const char *value,
           unsigned maximum, bool password = false) {
    return pxa_ui_create_typed(&tx, id, parent, 0, PXA_UI_NODE_CONTROL, PXA_UI_CONTROL_TEXT_INPUT) &&
        text(tx, id, value) && pxa_ui_set_font_role(&tx, id, PXA_UI_FONT_ROLE_CAPTION) &&
        pxa_ui_set_u8(&tx, id, PXA_UI_PROPERTY_TEXT_SINGLE_LINE, 1) &&
        pxa_ui_set_u8(&tx, id, PXA_UI_PROPERTY_TEXT_PASSWORD, password) &&
        pxa_ui_set_u32(&tx, id, PXA_UI_PROPERTY_TEXT_MAX_BYTES, maximum) &&
        pxa_ui_set_length(&tx, id, PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX, 36) &&
        pxa_ui_set_padding(&tx, id, 7, 4, 7, 4) &&
        pxa_ui_set_dp(&tx, id, PXA_UI_PROPERTY_RADIUS, 8) &&
        pxa_ui_set_dp(&tx, id, PXA_UI_PROPERTY_BORDER_WIDTH, 1) &&
        pxa_ui_set_rgba(&tx, id, PXA_UI_PROPERTY_BORDER_COLOR, 0xa3cab9ff) &&
        pxa_ui_set_rgba(&tx, id, PXA_UI_PROPERTY_BACKGROUND, 0xfffcf6ff) &&
        pxa_ui_set_rgba(&tx, id, PXA_UI_PROPERTY_FOREGROUND, 0x384355ff) &&
        pxa_ui_set_event_mask(&tx, id, PXA_UI_EVENT_MASK_TEXT | PXA_UI_EVENT_MASK_CLICK);
}
bool scroll(pxa_ui_transaction_t &tx, unsigned id) {
    return node(tx, id, Root, PXA_UI_NODE_SCROLL) && width(tx, id) &&
        pxa_ui_set_u8(&tx, id, PXA_UI_PROPERTY_SCROLL_AXIS, 2) &&
        pxa_ui_set_u16(&tx, id, PXA_UI_PROPERTY_GROW, 1) &&
        pxa_ui_set_length(&tx, id, PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX, 0);
}
void make_transcript() {
    transcript[0] = 0;
    if (!message_count) {
        copy_text(transcript, sizeof(transcript), tr("Tell me about your day.\n\nChat memory is kept by dsh-pet on your computer.",
            "今天有什么想和我说的呀？\n\n聊天记忆保存在电脑端 dsh-pet。"));
        return;
    }
    size_t used = 0;
    for (unsigned i = 0; i < message_count; ++i) {
        const auto &message = messages[i];
        const char *name = message.pet ? tr("Fat Fish", "大肥鱼") : tr("You", "你");
        int n = std::snprintf(transcript + used, sizeof(transcript) - used, "%s%s\n%s", i ? "\n\n" : "", name, message.text);
        if (n < 0 || size_t(n) >= sizeof(transcript) - used) break;
        used += size_t(n);
    }
}
bool create_ui(pxa_ui_transaction_t &tx) {
    if (!box(tx, Root, 1, PXA_UI_LAYOUT_COLUMN) ||
        // Native controls share the alpha plane above the pet's bitmap Surface.
        !pxa_ui_set_u8(&tx, Root, PXA_UI_PROPERTY_COMPOSITION, PXA_UI_COMPOSITION_ALPHA_OVERLAY) ||
        !pxa_ui_set_u8(&tx, Root, PXA_UI_PROPERTY_POSITION, 1) ||
        !pxa_ui_set_length(&tx, Root, PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_FILL, 0) ||
        !pxa_ui_set_padding(&tx, Root, 12, 10, 12, 10) ||
        !pxa_ui_set_rgba(&tx, Root, PXA_UI_PROPERTY_BACKGROUND, 0xfff8e8f5) ||
        !box(tx, Header, Root, PXA_UI_LAYOUT_ROW) ||
        !button(tx, Back, Header, tr("Back", "返回")) ||
        !label(tx, Title, Header, configuring ? tr("Service", "服务设置") : tr("Chat", "和大肥鱼聊天")) ||
        !pxa_ui_set_length(&tx, Title, PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_PX, 0) ||
        !pxa_ui_set_u16(&tx, Title, PXA_UI_PROPERTY_GROW, 1) ||
        !pxa_ui_set_u8(&tx, Header, PXA_UI_PROPERTY_ALIGN, PXA_UI_ALIGN_CENTER) ||
        !button(tx, Settings, Header, tr("Setup", "设置")) ||
        !label(tx, Status, Root, status_text)) return false;
    if (configuring) {
        return scroll(tx, Form) &&
            pxa_ui_set_u8(&tx, Form, PXA_UI_PROPERTY_LAYOUT, PXA_UI_LAYOUT_COLUMN) &&
            pxa_ui_set_dp(&tx, Form, PXA_UI_PROPERTY_GAP, 5) &&
            label(tx, UrlLabel, Form, tr("Service URL (ends in /dsh-pet-7340)", "服务地址（包含 /dsh-pet-7340）")) &&
            input(tx, Url, Form, editing.url, sizeof(editing.url) - 1) && width(tx, Url) &&
            label(tx, PetLabel, Form, tr("Pet ID", "宠物 ID")) &&
            input(tx, Pet, Form, editing.pet, sizeof(editing.pet) - 1) && width(tx, Pet) &&
            label(tx, TokenLabel, Form, tr("LAN bridge token", "局域网转发令牌")) &&
            input(tx, Token, Form, editing.token, sizeof(editing.token) - 1, true) && width(tx, Token) &&
            button(tx, Save, Root, tr("Save service", "保存服务设置")) && width(tx, Save);
    }
    return scroll(tx, History) &&
        pxa_ui_set_padding(&tx, History, 10, 10, 10, 10) &&
        pxa_ui_set_rgba(&tx, History, PXA_UI_PROPERTY_BACKGROUND, 0xfffcf6ff) &&
        pxa_ui_set_dp(&tx, History, PXA_UI_PROPERTY_RADIUS, 12) &&
        pxa_ui_set_dp(&tx, History, PXA_UI_PROPERTY_BORDER_WIDTH, 1) &&
        pxa_ui_set_rgba(&tx, History, PXA_UI_PROPERTY_BORDER_COLOR, 0xd5c3a6ff) &&
        label(tx, Transcript, History, transcript) &&
        box(tx, Presets, Root, PXA_UI_LAYOUT_ROW) &&
        button(tx, Mic, Presets, tr("Talk", "说话")) &&
        pxa_ui_set_event_mask(&tx, Mic, PXA_UI_EVENT_MASK_POINTER) &&
        button(tx, Hello, Presets, tr("Hi", "你好")) &&
        button(tx, Topic, Presets, tr("Today", "今天")) &&
        button(tx, Cancel, Presets, tr("Stop", "取消")) &&
        box(tx, Composer, Root, PXA_UI_LAYOUT_ROW) &&
        input(tx, Input, Composer, draft, sizeof(draft) - 1) &&
        pxa_ui_set_length(&tx, Input, PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_PX, 0) &&
        pxa_ui_set_u16(&tx, Input, PXA_UI_PROPERTY_GROW, 1) &&
        button(tx, Send, Composer, tr("Send", "发送"));
}
void present(bool foreground) {
    const bool visible = foreground && opened;
    const bool rebuild = visible && (!created || created_form != configuring || previous_english != pocket::english());
    if (!rebuild && !dirty && shown == visible && (created || !visible)) return;
    if (!created && !visible) return;
    const bool replacing = rebuild && created;
    // A subtree replacement can only address that subtree. Restore the room
    // Canvas separately when resuming into a rebuilt language/view.
    if (replacing && shown != visible) {
        pxa_ui_transaction_t companion{};
        if (!pxa_ui_transaction_begin(&companion, ++generation, PXA_UI_TRANSACTION_PATCH, scratch, sizeof(scratch)) ||
            !pxa_ui_set_u8(&companion, 2, PXA_UI_PROPERTY_VISIBLE, !visible) ||
            !pxa_ui_transaction_commit(&companion)) {
            if (companion.active) (void)pxa_ui_transaction_cancel(&companion);
            return;
        }
    }
    pxa_ui_transaction_t tx{};
    uint32_t next = ++generation;
    if (!pxa_ui_transaction_begin_target(&tx, next, next, PXA_UI_PRIMARY_SURFACE,
            replacing ? unsigned(Root) : 0u,
            replacing ? PXA_UI_TRANSACTION_REPLACE_SUBTREE : PXA_UI_TRANSACTION_PATCH,
            scratch, sizeof(scratch))) return;
    bool ok = true;
    if (!opened) {
        ok = pxa_ui_remove(&tx, Root);
    } else if (rebuild) {
        make_transcript();
        ok = create_ui(tx);
    } else {
        ok = pxa_ui_set_u8(&tx, Root, PXA_UI_PROPERTY_VISIBLE, visible);
        if (dirty) {
            ok = ok && text(tx, Status, status_text);
            if (!configuring) {
                if (transcript_dirty) {
                    make_transcript();
                    ok = ok && text(tx, Transcript, transcript);
                }
                if (clear_input) ok = ok && text(tx, Input, draft);
            }
        }
    }
    // The room's retained text Canvas is a separate alpha-plane root.
    // Hide it while the native chat panel supplies the foreground UI.
    if (!replacing) ok = ok && pxa_ui_set_u8(&tx, 2, PXA_UI_PROPERTY_VISIBLE, !visible);
    if (opened) {
        ok = ok && pxa_ui_set_u8(&tx, Settings, PXA_UI_PROPERTY_ENABLED, !busy() && !loading);
        if (!configuring)
            ok = ok && pxa_ui_set_u8(&tx, Send, PXA_UI_PROPERTY_ENABLED, !busy() && !loading) &&
                pxa_ui_set_u8(&tx, Hello, PXA_UI_PROPERTY_ENABLED, !busy() && !loading) &&
                pxa_ui_set_u8(&tx, Topic, PXA_UI_PROPERTY_ENABLED, !busy() && !loading) &&
                pxa_ui_set_u8(&tx, Hello, PXA_UI_PROPERTY_VISIBLE, !busy()) &&
                pxa_ui_set_u8(&tx, Topic, PXA_UI_PROPERTY_VISIBLE, !busy()) &&
                pxa_ui_set_u8(&tx, Mic, PXA_UI_PROPERTY_ENABLED, job == Job::NONE && !permission_token && !loading) &&
                pxa_ui_set_rgba(&tx, Mic, PXA_UI_PROPERTY_BACKGROUND, voice::recording() ? 0xf5c8c4ff : 0xe0efe9ff) &&
                pxa_ui_set_u8(&tx, Cancel, PXA_UI_PROPERTY_VISIBLE, busy());
        else ok = ok && pxa_ui_set_u8(&tx, Save, PXA_UI_PROPERTY_ENABLED, !loading);
        if (!configuring && message_count && (rebuild || scroll_to_end))
            ok = ok && pxa_ui_set_dp(&tx, History, PXA_UI_PROPERTY_SCROLL_POSITION, 32767);
    }
    if (!ok || !pxa_ui_transaction_commit(&tx)) {
        if (tx.active) (void)pxa_ui_transaction_cancel(&tx);
        return;
    }
    if (rebuild) mode_generation = next;
    created = opened; created_form = configuring; shown = visible;
    if (rebuild) previous_english = pocket::english();
    dirty = clear_input = false;
    if (!configuring) transcript_dirty = false;
    if (visible) scroll_to_end = false;
    pocket::redraw();
}
void click(unsigned id) {
    if (id == Back) {
        if (configuring) { configuring = false; dirty = true; }
        else close();
    } else if (id == Settings && !busy() && !loading) {
        editing = config; configuring = !configuring; dirty = true;
    } else if (id == Save && configuring && !loading) {
        size_t n = std::strlen(editing.url);
        while (n && editing.url[n - 1] == '/') editing.url[--n] = 0;
        if (!dsh::valid(editing)) { status("Enter a valid HTTP(S) URL and pet ID.", "请填写完整的 HTTP(S) 地址和宠物 ID。"); return; }
        config = editing; configuring = false; config_dirty = true;
        if (permission) (void)pxa_close_handle(permission);
        permission = 0;
        status("Saving service settings...", "正在保存服务设置……");
    } else if (!configuring) {
        if (id == Send || id == Input) send(draft);
        if (id == Hello) send(tr("Hello, Fat Fish!", "你好呀，大肥鱼！"));
        if (id == Topic) send(tr("How are you today?", "今天过得怎么样呀？"));
        if (id == Cancel && busy()) cancel();
    }
}
void loaded() {
    loading = false;
    if (configuring) editing = config;
    status(dsh::valid(config) ? "Hold Talk, then release to send." : "Set up your computer's dsh-pet service first.",
           dsh::valid(config) ? "按住说话，松开发送。" : "先在设置里填写电脑端 dsh-pet 地址。");
}
} // namespace
void init() {
    loading = true;
    if (pxa_storage_request_get(LOAD, STORAGE_KEY, sizeof(STORAGE_KEY) - 1)) loaded();
}
void open() { stop_voice(); opened = true; configuring = false; dirty = true; pocket::redraw(); }
void close() { voice::cancel(); opened = false; dirty = true; pocket::redraw(); }
bool is_open() { return opened; }
void stop_voice() {
    if (ai_mode) {
        cancel(); status("AI voice is off.", "AI 语音已关闭。");
    }
}
void toggle_voice() {
    if (ai_mode) { stop_voice(); return; }
    if (loading) return;
    if (!dsh::valid(config)) {
        open(); editing = config; configuring = true;
        status("Set up the computer service first.", "请先设置电脑端服务，再开启 AI 语音。"); return;
    }
    cancel(); ai_mode = true; ai_origin = now_ms; ai_session = ++sequence; listen_after = 0;
    status("Listening. Speak naturally.", "我在听，直接和我说话吧。");
    pocket::log("FatFish: room voice enabled");
}
bool voice_mode() { return ai_mode; }
unsigned voice_stage() {
    if (!ai_mode) return 0;
    if (voice::playing() || job == Job::SPEAK) return 3;
    if (now_ms < listen_after) return 2;
    return job != Job::NONE && job != Job::LISTEN ? 2 : 1;
}
const char *voice_status() { return status_text; }
const char *spoken_reply() { return last_reply.text[0] ? last_reply.text : nullptr; }
bool event(const pxa_event_t &event, bool foreground, uint64_t now_us) {
    now_ms = now_us / 1000;
    if (voice::event(event, foreground, now_us)) return true;
    if (event.service == PXA_STORAGE_SERVICE && event.token == LOAD) {
        pxa_storage_get_result_t result{};
        if (pxa_storage_parse_get(&event, LOAD, &result) && !result.status &&
            dsh::parse_config(result.value.data, result.value.size, config)) loaded();
        else if (pxa_assets_read(BOOTSTRAP, "assets/dsh.local.json", 0, sizeof(storage_value))) loaded();
        return true;
    }
    if (event.service == PXA_ASSETS_SERVICE && event.token == BOOTSTRAP) {
        pxa_asset_read_result_t result{};
        if (pxa_assets_parse_read(&event, BOOTSTRAP, &result) && !result.status && !result.offset &&
            result.total_bytes == result.bytes && dsh::parse_config(result.data, result.bytes, config)) config_dirty = true;
        loaded(); return true;
    }
    if (event.service == PXA_STORAGE_SERVICE && event.token == SAVE) {
        int32_t result;
        if (!pxa_storage_parse_status(&event, SAVE, PXA_STORAGE_SET, &result)) result = PXA_STATUS_PROTOCOL_ERROR;
        saving = false;
        if (result) {
            config_dirty = true; save_retry = now_ms + 1000;
            status("Service settings will be saved again.", "服务设置保存失败，稍后重试。");
        } else if (!busy()) status("Service settings saved.", "服务设置已保存。");
        return true;
    }
    if (event.service == PXA_PERMISSION_SERVICE && event.opcode == PXA_PERMISSION_ACQUIRE && event.token > LOAD) {
        pxa_permission_acquire_result_t result{};
        if (!pxa_permission_parse_acquire(&event, event.token, &result)) return true;
        if (event.token != permission_token) {
            if (result.handle) (void)pxa_close_handle(result.handle);
            return true;
        }
        permission_token = 0;
        if (result.status) {
            if (ai_mode) { ai_mode = false; voice::cancel(); }
            char line[96]; std::snprintf(line, sizeof(line), "FatFish: chat permission status=%d", result.status);
            pocket::log(line);
            status("Network access was not granted.", "联网权限未获允许，可在发送时重试。");
        }
        else { permission = result.handle; begin(permission_job); }
        return true;
    }
    pxa_permission_revoked_t revoked{};
    if (pxa_permission_parse_revoked(&event, &revoked) && revoked.name_size == 10 &&
        !std::memcmp(revoked.name, "net.client", 10)) {
        cancel(); permission = 0;
        status("Network access was revoked.", "联网权限已撤销。"); return true;
    }
    if (http.event(event)) return true;
    // Native controls receive their own ACTION/TEXT events. Their bubbled
    // root pointers must not activate the room's menu underneath the panel.
    pxa_ui_pointer_data_t pointer{};
    if (opened && foreground && pxa_ui_parse_pointer(&event, &pointer)) {
        if (!configuring && pointer.node == Mic) {
            if (pointer.phase == 0 && job == Job::NONE && !permission_token && !loading) {
                if (dsh::valid(config)) voice::press();
                else { editing = config; configuring = true; dirty = true; }
            }
            if (pointer.phase == 2) voice::release(true);
            if (pointer.phase == 3) voice::release(false);
            if (pointer.phase != PXA_POINTER_MOVE) dirty = true;
        }
        return true;
    }
    pxa_ui_event_data_t ui{};
    if (event.service != PXA_UI_SERVICE || !pxa_ui_parse_event(&event, &ui) ||
        !created || !opened || ui.generation < mode_generation || ui.generation > generation) return false;
    if (ui.kind == PXA_UI_EVENT_TEXT_KIND) {
        if (!configuring && ui.node == Input) pxa_ui_event_text(&ui, draft, sizeof(draft));
        if (configuring && ui.node == Url) pxa_ui_event_text(&ui, editing.url, sizeof(editing.url));
        if (configuring && ui.node == Pet) pxa_ui_event_text(&ui, editing.pet, sizeof(editing.pet));
        if (configuring && ui.node == Token) pxa_ui_event_text(&ui, editing.token, sizeof(editing.token));
        return true;
    }
    if (foreground && ui.kind == PXA_UI_EVENT_ACTION_KIND && ui.node >= Back && ui.node <= Save) {
        click(ui.node); return true;
    }
    return false;
}
void update(bool foreground, uint64_t now_us) {
    now_ms = now_us / 1000;
    if (ai_mode && !foreground) stop_voice();
    http.pump(now_ms);
    if (http.done) finish_request();
    voice::update(foreground, now_us);
    if (ai_mode && was_playing && !voice::playing()) listen_after = now_ms + 600;
    if (const char *message = voice::notice()) { copy_text(status_text, sizeof(status_text), message); dirty = true; }
    if (voice::take_failure() && ai_mode) { ai_mode = false; voice::cancel(); }
    const uint8_t *audio; size_t audio_size;
    if (voice::take_recording(audio, audio_size)) {
        if (voice::audio_json(audio, audio_size, audio_request, sizeof(audio_request))) {
            status("Recognizing speech...", "正在识别语音……"); network(Job::HEAR);
        } else status("Recording is too large. Try a shorter sentence.", "录音有点长，请分成短句再试。");
    }
    if (ai_mode && job == Job::NONE && !permission_token && now_ms >= listen_after) {
        if (!voice::busy()) voice::listen();
        const uint8_t *chunk; size_t bytes;
        if (voice::take_chunk(chunk, bytes) && voice::audio_json(chunk, bytes, audio_request, sizeof(audio_request))) {
            size_t size = std::strlen(audio_request);
            std::snprintf(audio_request + size - 1, sizeof(audio_request) - size + 1,
                          ",\"session\":\"%llx-%llx\"}",
                          (unsigned long long)ai_origin, (unsigned long long)ai_session);
            network(Job::LISTEN);
        }
    }
    was_playing = voice::playing();
    if (config_dirty && !saving && !loading && now_ms >= save_retry) {
        size_t n = dsh::encode_config(config, storage_value, sizeof(storage_value));
        int result = n ? pxa_storage_request_set(scratch, sizeof(scratch), SAVE, STORAGE_KEY,
            sizeof(STORAGE_KEY) - 1, (const uint8_t *)storage_value, n) : PXA_STATUS_INVALID_ARGUMENT;
        if (result) save_retry = now_ms + 1000;
        else { saving = true; config_dirty = false; }
    }
    present(foreground);
}
const char *take_reply() {
    if (!reply_ready) return nullptr;
    reply_ready = false; return last_reply.text;
}
int take_animation() { int result = next_animation; next_animation = -1; return result; }
} // namespace fish::chat
