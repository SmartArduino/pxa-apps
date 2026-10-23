#pragma once
#include "store_client.h"
#include "store_model.h"

namespace store {
enum class AccountPhase : uint8_t { guest, restoring, starting, waiting, signed_in, failed, expired, denied };

/* One bounded transport for profile/likes. Catalog/media never receive bearer
 * credentials. No allocation or polling outside an active authorization. */
class Account {
public:
    AccountPhase phase = AccountPhase::guest;
    char user_code[10]{}, display_name[128]{};
    bool changed = false, persistent = true, like_ready = false, liked = false, like_error = false;
    uint64_t like_count = 0;
    void start();
    void network(uint64_t permission, uint64_t now);
    void login(uint64_t now);
    void cancel();
    void logout(uint64_t now);
    void retry(uint64_t now);
    void target(const store_app_t *item, uint64_t now);
    void toggle_like(uint64_t now);
    bool event(const pxa_event_t &, uint64_t now);
    void tick(uint64_t now);
    uint16_t clock_period() const;
    bool authenticated() const { return phase == AccountPhase::signed_in; }
    bool busy() const { return request_id != 0; }
    bool has_credential() const { return token[0] != 0; }
    void stop();
    void pause(bool value) { paused = value; }
private:
    enum class Op : uint8_t { none, authorize, poll, session, revoke, likes_get, likes_put, likes_delete };
    store_client_t client{};
    uint8_t packet[1024]{}, body[2048]{};
    char token[49]{}, device_code[49]{}, app_id[STORE_MAX_APP_ID]{}, lineage[65]{};
    uint64_t request_id = 0, sequence = UINT64_C(0x4100000000000000), storage_id = UINT64_C(0x4200000000000000);
    uint64_t deadline = 0, poll_at = 0, next_attempt = 0;
    uint32_t target_revision = 0, request_revision = 0;
    uint32_t interval_ms = 5000;
    bool paused = false;
    bool restored = false, session_checked = false, want_likes = false;
    Op operation = Op::none;
    bool submit(Op, uint64_t now);
    void continue_body(uint64_t now);
    void finish(uint64_t now, bool valid);
    void forget();
    void schedule(uint64_t now);
};
}
