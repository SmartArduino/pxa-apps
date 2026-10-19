#pragma once
#include "browser_script_wire.hpp"
#include <pxa/ipc.hpp>
#include <chrono>
#include <utility>

namespace browser::script {
// Stable UI-owned transport. Closing/replacing a page never invalidates an
// in-flight packet. There is no timer or VM allocation on static-only runs.
class Client {
public:
    Client(pxa::Transport&,Cookies&);
    ~Client();
    bool open(std::string html,std::string url,unsigned module,Metrics,Limits);
    void close();
    void clear_storage();
    // Defer DOM projection while an image overlay borrows the current page.
    // Timers/network still run, without another markup buffer or serialization.
    void step(bool project=true);
    void maintain();
    bool event(const pxa::Event&);
    void complete(Response);
    std::optional<Request> take_request();
    std::optional<Navigation> take_navigation(){return std::exchange(navigation_,{});}
    std::optional<std::string> take_markup(){return std::exchange(markup_,{});}
    bool click(std::uint32_t);
    void change_field(std::uint32_t,std::string,bool);
    void pause(bool p){paused_=p;}
    bool pending(std::uint32_t id) const {return id && std::ranges::find(summary_.pending,id)!=summary_.pending.end();}
    bool active() const {return summary_.active;}
    bool initialized() const {return summary_.initialized;}
    bool ready() const {return summary_.ready;}
    std::string_view error() const {return summary_.error;}
    Stats stats() const {return summary_.counters;}
    bool set_heap_limit(std::size_t);
private:
    pxa::Transport& transport_;
    Cookies& cookies_;
    wire::Key key_{};
    bool key_ready_=false,worker_known_=false,closing_=false,clearing_=false,paused_=false,draining_=false;
    std::uint64_t generation_=0,token_=0,flight_generation_=0,started_us_=0;
    std::uint32_t call_id_=0;
    wire::Op flight_op_=wire::Op::poll,upload_op_=wire::Op::open;
    std::size_t flight_bytes_=0,upload_offset_=0;
    std::string upload_;
    std::optional<std::string> open_html_;
    std::string url_;
    Metrics metrics_;
    Limits limits_;
    unsigned module_=0;
    std::optional<std::size_t> limit_;
    struct Field {std::uint32_t id;std::string value;bool checked;};
    std::vector<Field> fields_;
    std::array<std::uint32_t,4> clicks_{};
    unsigned click_count_=0;
    std::vector<Response> responses_;
    std::vector<Request> requests_;
    std::optional<Navigation> navigation_;
    std::optional<std::string> markup_;
    wire::Summary summary_;
    wire::Output output_kind_=wire::Output::none;
    std::string output_;
    std::size_t output_total_=0;
    std::array<std::byte,1100> packet_{};
    std::array<std::byte,1024> reply_{};
    static std::uint64_t now();
    void fail(std::string_view);
    void drive(bool,bool=true);
    void upload(wire::Op,std::string);
    void send(wire::Op,bool=false,bool=true);
    bool receive(std::span<const std::byte>);
    bool deliver(wire::Output,std::string);
};
} // namespace browser::script
