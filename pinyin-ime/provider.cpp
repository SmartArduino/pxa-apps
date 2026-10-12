#include <pxa/app.hpp>
#include <pxa/startup.hpp>
#include "session.hpp"
#include "google_engine.hpp"
#include "port/dictionary.hpp"

struct PinyinProvider {
    PinyinSession<GooglePinyinEngine> session;
    bool open = false;
    pxa::Result<void> on_start(pxa::Context& ctx, std::span<const std::byte> config) {
        auto dictionary = pxa::detail::configuration_record(config, 13);
        if (!dictionary || dictionary->size() != 16) return std::unexpected(pxa::Error::protocol_error);
        ime_bind_dictionary(ctx.transport(), pxa::wire::get64(dictionary->data()), pxa::wire::get64(dictionary->data()+8));
        open = ime_engine_open("system.dict", "");
        if (!open) return std::unexpected(pxa::Error::io_error);
        ime_engine_set_max_lens(IME_ENGINE_MAX_PINYIN_LEN, IME_ENGINE_MAX_PINYIN_LEN);
        ime_t9_init(); return {};
    }
    pxa::Result<bool> on_event(pxa::Context& ctx, const pxa::Event& event) {
        auto input = pxa::ime::decode_input(event);
        if (!input) return input.error() == pxa::Error::not_found ? pxa::Result<bool>(false) : std::unexpected(input.error());
        auto result = session.process(*input, ctx.transport());
        if (!result && result.error() != pxa::Error::bad_state && result.error() != pxa::Error::denied)
            return std::unexpected(result.error());
        return true;
    }
    void on_stop(pxa::StopReason) { if(open) ime_engine_close(); open = false; ime_t9_deinit(); }
};
PXA_SERVICE(PinyinProvider)
