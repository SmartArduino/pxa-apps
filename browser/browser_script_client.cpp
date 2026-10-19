#include "browser_script_client.hpp"
#include <wasi/api.h>
#include <ctime>

namespace browser::script {
std::uint64_t Client::now() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
Client::Client(pxa::Transport& transport,Cookies& cookies):transport_(transport),cookies_(cookies) {
    key_ready_=__wasi_random_get(reinterpret_cast<std::uint8_t*>(key_.data()),key_.size())==0;
}
Client::~Client() {
    if(token_ && !call_id_) {
        std::array<std::byte,8> data;pxa::wire::put64(data.data(),token_);
        (void)transport_.send(1,1,0,data);
    }
}
void Client::fail(std::string_view message) {
    token_=0;call_id_=0;
    summary_.active=false;summary_.error=message;
    open_html_.reset();std::string{}.swap(upload_);responses_.clear();fields_.clear();click_count_=0;
    draining_=false;
}
bool Client::open(std::string html,std::string url,unsigned module,Metrics metrics,Limits limits) {
    ++generation_;summary_={};summary_.generation=generation_;summary_.active=true;
    closing_=false;paused_=false;draining_=false;
    open_html_.reset();std::string{}.swap(upload_);upload_offset_=0;
    responses_.clear();requests_.clear();fields_.clear();click_count_=0;limit_.reset();markup_.reset();navigation_.reset();
    std::string{}.swap(output_);output_kind_=wire::Output::none;
    if(!key_ready_ || module>unsigned(Module::page) || html.size()>limits.html_bytes || !same_origin(url,url)) {
        fail("脚本服务初始化失败");return false;
    }
    open_html_=std::move(html);url_=std::move(url);metrics_=std::move(metrics);limits_=limits;module_=module;
    return true;
}
void Client::close() {
    if(!generation_)return;
    closing_=worker_known_;summary_.active=false;
    open_html_.reset();std::string{}.swap(upload_);responses_.clear();requests_.clear();fields_.clear();click_count_=0;
    markup_.reset();navigation_.reset();draining_=false;std::string{}.swap(output_);
    drive(false);
}
void Client::clear_storage(){clearing_=worker_known_;close();}
void Client::complete(Response response) {
    if(!active() || !pending(response.id))return;
    if(response.body.size()>std::max(limits_.response_bytes,limits_.script_bytes)) {response.body.clear();response.failed=true;}
    if(responses_.size()<4)responses_.push_back(std::move(response));
}
std::optional<Request> Client::take_request() {
    if(requests_.empty() || !active())return {};
    auto result=std::move(requests_.front());requests_.erase(requests_.begin());return result;
}
bool Client::click(std::uint32_t id) {
    if(!active() || click_count_==clicks_.size())return false;
    clicks_[click_count_++]=id;return true;
}
void Client::change_field(std::uint32_t id,std::string value,bool checked) {
    if(!active() || !id || value.size()>4096)return;
    auto old=std::ranges::find(fields_,id,&Field::id);
    if(old!=fields_.end()){old->value=std::move(value);old->checked=checked;}
    else if(fields_.size()<24)fields_.push_back({id,std::move(value),checked});
}
bool Client::set_heap_limit(std::size_t bytes) {
    if(bytes<128*1024 || bytes>4*1024*1024 || summary_.counters.current_bytes>bytes)return false;
    limits_.heap_bytes=bytes;limit_=bytes;return true;
}
void Client::upload(wire::Op op,std::string data) {
    if(data.size()>wire::max_blob){fail("脚本传输超过限制");return;}
    upload_op_=op;upload_=std::move(data);upload_offset_=0;
}
void Client::maintain(){drive(false);}
void Client::step(bool project){drive(true,project);}
void Client::drive(bool execute,bool project) {
    if(token_) {
        if(now()-started_us_>15000000)fail("脚本服务响应超时");
        return;
    }
    if(clearing_){clearing_=false;closing_=false;send(wire::Op::clear);return;}
    if(closing_){closing_=false;send(wire::Op::close);return;}
    if(!active())return;
    if(open_html_ && upload_.empty()) {
        wire::Encoder e;e.string(url_);e.string(metrics_.user_agent);e.u32(metrics_.width);e.u32(metrics_.height);
        e.u32(std::uint32_t(std::clamp(metrics_.density,0.25,8.0)*65536));e.u8(module_);wire::limits(e,limits_);wire::cookies(e,cookies_);
        auto html=std::move(*open_html_);open_html_.reset();html.insert(0,e.data);upload(wire::Op::open,std::move(html));
    }
    if(!upload_.empty()){send(upload_op_);return;}
    if(draining_){send(wire::Op::poll);return;}
    if(!responses_.empty()) {
        auto response=std::move(responses_.front());responses_.erase(responses_.begin());
        wire::Encoder e;e.u32(response.id);e.u32(response.status);e.u8(response.failed);wire::cookies(e,cookies_);
        response.body.insert(0,e.data);upload(wire::Op::response,std::move(response.body));send(upload_op_);return;
    }
    if(!fields_.empty()) {
        auto field=std::move(fields_.front());fields_.erase(fields_.begin());
        wire::Encoder e;e.u32(field.id);e.u8(field.checked);e.data.append(field.value);
        upload(wire::Op::field,std::move(e.data));send(upload_op_);return;
    }
    if(click_count_) {
        wire::Encoder e;e.u32(clicks_[0]);std::move(clicks_.begin()+1,clicks_.begin()+click_count_,clicks_.begin());--click_count_;
        upload(wire::Op::click,std::move(e.data));send(upload_op_);return;
    }
    if(limit_) {wire::Encoder e;e.u32(std::uint32_t(*limit_));limit_.reset();upload(wire::Op::limit,std::move(e.data));send(upload_op_);return;}
    if(execute && !paused_ && (!project || !markup_) && now()>=summary_.wake)send(wire::Op::poll,true,project);
}
void Client::send(wire::Op op,bool execute,bool project) {
    constexpr auto prefix=8+wire::endpoint.size();
    auto body=std::span(packet_).subspan(prefix,wire::packet_bytes);
    wire::Header h;h.op=op;h.mode=execute?(project?wire::PollMode::update:wire::PollMode::callbacks):wire::PollMode::drain;h.generation=generation_;h.key=key_;
    const bool uploading=op!=wire::Op::poll && op!=wire::Op::close && op!=wire::Op::clear;
    const auto amount=uploading?std::min(wire::packet_bytes-wire::header_bytes,upload_.size()-upload_offset_):0;
    if(uploading){h.offset=std::uint32_t(upload_offset_);h.total=std::uint32_t(upload_.size());}
    h.encode(body);
    if(amount)std::memcpy(body.data()+wire::header_bytes,upload_.data()+upload_offset_,amount);
    auto size=pxa::encode_ipc_call_in_place(wire::endpoint,wire::header_bytes+amount,packet_);
    if(!size){fail("脚本请求编码失败");return;}
    token_=transport_.next_token();call_id_=0;flight_generation_=generation_;flight_op_=op;flight_bytes_=amount;started_us_=now();
    auto sent=transport_.send(7,1,token_,std::span(packet_).first(*size));
    if(!sent){token_=0;fail("脚本服务不可用");return;}
    if(op==wire::Op::open)worker_known_=true;
}
bool Client::event(const pxa::Event& event) {
    if(event.service!=7)return false;
    if(event.opcode==1 && token_ && event.token==token_) {
        auto accepted=pxa::wire::result_body(event.payload);
        if(!accepted || accepted->size()!=4 || !(call_id_=pxa::wire::get32(accepted->data()))) {
            token_=0;fail("脚本服务启动失败");
        }
        return true;
    }
    if(event.opcode!=0x8002 || !call_id_ || event.token!=call_id_)return false;
    auto result=pxa::decode_ipc_result(event,reply_);
    const bool current=flight_generation_==generation_ && !closing_ && !clearing_;
    token_=0;call_id_=0;
    if(current) {
        if(!result || !receive(std::span(reply_).first(result?*result:0)))fail("脚本服务协议错误");
        else if(flight_op_!=wire::Op::poll && flight_op_!=wire::Op::close && flight_op_!=wire::Op::clear) {
            upload_offset_+=flight_bytes_;
            if(upload_offset_==upload_.size()){std::string{}.swap(upload_);upload_offset_=0;}
        }
    }
    drive(false);return true;
}
bool Client::receive(std::span<const std::byte> payload) {
    wire::Reader r{wire::text(payload)};auto summary=wire::Summary::decode(r);
    if(!summary || summary->generation!=generation_)return false;
    summary_=std::move(*summary);
    if(open_html_)summary_.active=true;
    auto kind=wire::Output(r.u8());auto more=r.u8();auto offset=r.u32(),total=r.u32();
    if(!r.valid || kind>wire::Output::markup || more>1 || total>wire::max_blob)return false;
    draining_=more;
    if(kind==wire::Output::none)return offset==0 && total==0 && r.done();
    if(offset==0){output_kind_=kind;output_total_=total;output_.clear();}
    if(kind!=output_kind_ || total!=output_total_ || offset!=output_.size() || r.data.size()>total-output_.size())return false;
    output_.append(r.data);
    if(output_.size()==total){auto data=std::exchange(output_,{});output_kind_=wire::Output::none;return deliver(kind,std::move(data));}
    return true;
}
bool Client::deliver(wire::Output kind,std::string data) {
    wire::Reader r{data};
    switch(kind) {
    case wire::Output::cookie: cookies_.receive(url_,data,now()/1000000,std::time(nullptr),true);return true;
    case wire::Output::markup: markup_=std::move(data);return true;
    case wire::Output::navigation: {
        Navigation n;n.post=r.u8();n.url=r.string(1024);n.body=r.string(8192);
        if(!r.done() || !script_url_allowed(url_,n.url,true))return false;
        navigation_=std::move(n);return true;
    }
    case wire::Output::request: {
        Request q;q.id=r.u32();auto flags=r.u8();q.script=flags&1;q.post=flags&2;q.timeout_ms=r.u32();
        q.url=r.string(1024);q.content_type=r.string(256);q.body=r.string(8192);
        if(!r.done() || !q.id || flags>3 || q.timeout_ms>60000 || !script_url_allowed(url_,q.url,q.script) || requests_.size()>=4)return false;
        requests_.push_back(std::move(q));return true;
    }
    default:return false;
    }
}
} // namespace browser::script
