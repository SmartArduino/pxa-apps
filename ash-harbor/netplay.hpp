// SPDX-License-Identifier: MIT
#pragma once
#include <pxa/lan.hpp>
#include "net_wire.hpp"
#include <cstdio>

namespace ash {
struct Netplay {
    enum class Status {idle,opening,hosting,searching,joining,loading,playing,failed};
    struct Room {pxa::NetworkEndpoint address;uint32_t id=0,seen=0;int character=0,stage=0;};
    Status status=Status::idle;
    pxa::Context* ctx=nullptr;
    std::optional<pxa::Permission> grant;
    std::optional<pxa::DatagramSocket> socket;
    std::optional<pxa::Permission> discoveryGrant;
    std::optional<pxa::ServiceBrowser> browser;
    std::optional<pxa::ServiceAdvertisement> advertisement;
    std::array<Room,4> rooms{};unsigned roomCount=0;
    pxa::NetworkEndpoint peer{},local{};
    lan::Inputs outgoing,incoming;
    uint32_t room=0,session=0,nonce=0,sequence=0,received=0,control=1,remoteControl=0;
    uint32_t now=0,lastRx=0,lastSend=0,lastFind=0,lastWelcome=0,opened=0,ping=0,echo=0;
    uint16_t match=1;
    int characters[2]{},stage=0,errorCode=0;
    bool host=false,connected=false,localReady=false,remoteReady=false,started=false;
    bool localPause=false,remotePause=false,localRematch=false,remoteRematch=false;
    bool remoteStall=false;
    bool resetWanted=false,snapshot=false,busy=false;
    const char* message="同一网络内双人对战";
    std::array<std::byte,lan::capacity+24> rx{},tx{};
    unsigned generation=0,draining=0;
    bool active()const{return status!=Status::idle&&status!=Status::failed;}
    bool playing()const{return status==Status::playing;}
    bool stalled()const{return connected&&now-lastRx>800;}
    bool paused()const{return localPause||remotePause||stalled()||remoteStall;}
    int player()const{return host?0:1;}
    static bool same(const pxa::NetworkEndpoint& a,const pxa::NetworkEndpoint& b){return a.family==b.family&&a.port==b.port&&a.address==b.address&&a.scope==b.scope;}
    void log(const char* event){if(!ctx)return;char b[192];std::snprintf(b,sizeof b,
        "Ash LAN %s host=%d status=%d match=%u peer=%u.%u.%u.%u:%u ping=%u ack=%u",event,int(host),int(status),match,
        unsigned(peer.address[0]),unsigned(peer.address[1]),unsigned(peer.address[2]),unsigned(peer.address[3]),peer.port,ping,incoming.ack);
        (void)ctx->log().write(pxa::LogLevel::info,b);}
    bool send(lan::Writer& packet,const pxa::NetworkEndpoint& to){
        if(!socket)return false;packet.finish();auto r=socket->send_to(to,packet.view(),tx);
        if(!r&&r.error()!=pxa::Error::would_block){errorCode=int(r.error());fail("网络连接不可用");}return bool(r);
    }
    void send(lan::Writer& packet){(void)send(packet,peer);}
    pxa::Task<void> drain(pxa::DatagramSocket old,pxa::Permission permission){
        auto began=co_await ctx->clock().now();
        if(began)for(unsigned i=0;i<256;++i){auto stats=old.stats();if(!stats||!stats->send_queued_bytes)break;
            auto time=co_await ctx->clock().now();if(!time||*time-*began>200000)break;
            if(auto r=co_await ctx->clock().yield();!r)break;}
        (void)old.close();--draining;co_return pxa::Result<void>{};
    }
    void leave(){if(socket&&connected){lan::Writer w(lan::Kind::leave,room,session);for(int i=0;i<3;++i)send(w);
            if(socket&&grant&&ctx){++draining;auto r=ctx->tasks().start(drain(std::move(*socket),std::move(*grant)));if(!r)--draining;}}
        ++generation;browser.reset();advertisement.reset();socket.reset();discoveryGrant.reset();grant.reset();connected=started=resetWanted=snapshot=false;status=Status::idle;}
    void fail(const char* text){message=text;log("failed");++generation;browser.reset();advertisement.reset();socket.reset();discoveryGrant.reset();grant.reset();connected=started=false;status=Status::failed;}
    void setInput(uint32_t mask){if(!connected||!started||paused())mask=0;
        if(!host&&!outgoing.push(mask))fail("网络延迟过高，请重新加入");}
    void pause(bool value){if(!connected)return;localPause=value;++control;setInput(0);log(value?"pause":"resume");}
    void ready(){if(!localReady){localReady=true;++control;log("loaded");}}
    void rematch(){localRematch=true;localPause=false;++control;log("rematch-ready");}
    void resetMatch(){++match;localReady=remoteReady=started=localPause=remotePause=localRematch=remoteRematch=remoteStall=false;
        outgoing={};incoming={};++control;remoteControl=0;resetWanted=true;status=Status::loading;lastWelcome=0;log("loading");}
    pxa::Task<void> open(pxa::Context& c,bool hosting,int character,int scenery,uint32_t seed){
        if(busy)co_return pxa::Result<void>{};leave();ctx=&c;busy=true;const unsigned g=generation;
        status=Status::opening;message="正在连接网络";
        while(draining&&g==generation){auto r=co_await c.clock().yield();if(!r)break;}
        if(g!=generation){busy=false;co_return pxa::Result<void>{};}
        auto caps=co_await c.network().capabilities();
        if(g!=generation){busy=false;co_return pxa::Result<void>{};}
        if(!caps||!caps->datagrams()||!caps->ipv4()||caps->max_datagram_bytes<lan::capacity){busy=false;fail("此设备暂不支持局域网对战");co_return pxa::Result<void>{};}
        auto permission=co_await c.permissions().acquire("net.datagram","local-network");
        if(g!=generation){busy=false;co_return pxa::Result<void>{};}
        if(!permission){busy=false;errorCode=int(permission.error());fail("请允许局域网通信后重试");co_return pxa::Result<void>{};}
        grant.emplace(std::move(*permission));pxa::NetworkOptions options;
        options.bind.port=hosting?lan::port:0;options.buffer_bytes=8192;options.max_items=8;
        auto made=co_await c.network().bind_datagram(*grant,options);
        if(g!=generation){busy=false;co_return pxa::Result<void>{};}
        if(!made){busy=false;errorCode=int(made.error());fail("无法建连，请检查网络或房间占用");co_return pxa::Result<void>{};}
        socket.emplace(std::move(*made));auto address=socket->local_endpoint();if(address)local=*address;
        host=hosting;characters[0]=character;characters[1]=character;stage=scenery;nonce=seed?seed:1;room=hosting?nonce:0;session=0;
        peer={};ping=echo=errorCode=0;
        roomCount=0;sequence=received=remoteControl=0;control=1;match=1;outgoing={};incoming={};
        localReady=remoteReady=started=localPause=remotePause=localRematch=remoteRematch=remoteStall=false;
        resetWanted=snapshot=connected=false;opened=lastRx=now;lastSend=lastFind=lastWelcome=0;
        status=hosting?Status::hosting:Status::searching;message=hosting?"等待对手加入":"正在搜索房间";log("opened");
        if(caps->discovery()){
            auto permission=co_await c.permissions().acquire("net.discovery","local-network");
            if(g!=generation){busy=false;co_return pxa::Result<void>{};}
            if(permission){discoveryGrant.emplace(std::move(*permission));pxa::NetworkOptions discoveryOptions;discoveryOptions.max_items=4;
                if(hosting){char name[32];std::snprintf(name,sizeof name,"AshHarbor-%08x",room);
                    std::array<std::byte,11> txt{std::byte{10},std::byte{'a'},std::byte{'='},std::byte{'A'},std::byte{lan::version},std::byte(character),std::byte(scenery)};
                    for(int i=0;i<4;++i)txt[7+i]=std::byte(room>>(8*i));
                    auto a=co_await c.network().advertise("_ashharbor._udp",name,local,txt,*discoveryGrant,discoveryOptions);
                    if(g!=generation){busy=false;co_return pxa::Result<void>{};}if(a)advertisement.emplace(std::move(*a));else errorCode=int(a.error());
                }else{auto b=co_await c.network().browse("_ashharbor._udp",*discoveryGrant,discoveryOptions);
                    if(g!=generation){busy=false;co_return pxa::Result<void>{};}if(b)browser.emplace(std::move(*b));else errorCode=int(b.error());}
            }
        }
        busy=false;log(hosting?(advertisement?"advertised":"direct-only"):(browser?"searching":"direct-only"));
        co_return pxa::Result<void>{};
    }
    void join(pxa::NetworkEndpoint address,uint32_t id=0){if(!socket||host)return;
        peer=address;room=id;session=nonce;opened=lastRx=now;status=Status::joining;message="正在加入房间";lastSend=0;log("joining");}
    void welcome(){lan::Writer w(lan::Kind::welcome,room,session);w.u16(match);w.u8(characters[0]);w.u8(characters[1]);w.u8(stage);send(w);lastWelcome=now;}
    void receive(lan::Packet& p,const pxa::NetworkEndpoint& from,Arena& arena){
        using K=lan::Kind;auto& r=p.body;
        if(p.kind==K::find&&host&&status==Status::hosting&&r.done()){
            lan::Writer w(K::offer,room,0);w.u8(characters[0]);w.u8(stage);send(w,from);return;}
        if(p.kind==K::offer&&!host&&status==Status::searching){unsigned c=r.u8(),s=r.u8();
            if(!r.done()||!p.room||c>=characterCount||s>=4)return;
            unsigned i=0;while(i<roomCount&&!same(rooms[i].address,from))++i;
            if(i<rooms.size()){if(i==roomCount)++roomCount;rooms[i]={from,p.room,now,int(c),int(s)};}return;}
        if(p.kind==K::join&&host){unsigned c=r.u8();if(!r.done()||c>=characterCount||!p.session||(p.room&&p.room!=room))return;
            if(connected){if(same(from,peer)&&p.session==session){welcome();}else{lan::Writer w(K::reject,room,p.session);send(w,from);}return;}
            if(status!=Status::hosting)return;peer=from;session=p.session;characters[1]=c;connected=true;advertisement.reset();lastRx=now;resetWanted=true;status=Status::loading;welcome();log("joined");return;}
        if(!same(from,peer)||p.session!=session)return;
        if(p.kind==K::reject&&status==Status::joining&&r.done()){fail("房间已满，请选择其他房间");return;}
        if(p.kind==K::welcome&&!host&&(status==Status::joining||connected)){
            unsigned m=r.u16(),a=r.u8(),b=r.u8(),s=r.u8();if(!r.done()||!p.room||(room&&p.room!=room)||!m||a>=characterCount||b>=characterCount||s>=4)return;
            if(connected&&m!=match&&uint16_t(m-match)!=1)return;
            if(!connected||m!=match){room=p.room;match=m;characters[0]=a;characters[1]=b;stage=s;connected=true;
                localReady=remoteReady=started=localPause=remotePause=localRematch=remoteRematch=remoteStall=false;
                outgoing={};incoming={};received=0;++control;resetWanted=true;status=Status::loading;lastRx=now;log("welcome");}
            // Repeated greetings cannot keep a match with no State alive.
            return;}
        if(!connected||p.room!=room)return;
        if(p.kind==K::leave&&r.done()){fail("对手已离开房间");return;}
        if(p.kind==K::input&&host){unsigned m=r.u16();uint32_t revision=r.u32(),stamp=r.u32();unsigned delay=r.u16(),flags=r.u8(),count=r.u8();
            if(m!=match||flags>15||count>32||r.b.size()-r.pos!=count*6)return;
            // Validate the complete message before changing any live state.
            std::array<lan::Input,32> events{};for(unsigned i=0;i<count;++i){events[i]={r.u32(),uint16_t(r.u16())};if(events[i].buttons&~lan::keys)return;}
            if(!r.done())return;
            if(lan::newer(p.seq,received)){
                received=p.seq;lastRx=now;echo=stamp;ping=std::min(delay,9999u);
                // Link status changes without a menu/control revision. Use
                // packet order so delayed reports cannot restore an old hold.
                remoteStall=flags&8;
                if(lan::newer(revision,remoteControl)){remoteControl=revision;remoteReady=flags&1;remotePause=flags&2;remoteRematch=flags&4;}
            }
            for(unsigned i=0;i<count;++i)if(!incoming.receive(events[i])){fail("网络延迟过高，请重新加入");return;}
            return;}
        if(p.kind==K::state&&!host){unsigned m=r.u16(),flags=r.u8();uint32_t ack=r.u32(),stamp=r.u32();
            if(m!=match||flags>63||!lan::newer(p.seq,received)||lan::newer(ack,outgoing.next-1))return;
            if(!lan::readArena(r,arena))return;received=p.seq;lastRx=now;
            if(stamp&&now-stamp<10000)ping=now-stamp;
            outgoing.acknowledge(ack);started=flags&1;remotePause=flags&2;remoteRematch=flags&8;remoteStall=flags&32;
            status=started?Status::playing:Status::loading;snapshot=true;return;}
    }
    void poll(uint32_t clock,Arena& arena){
        now=clock;if(!socket)return;
        if(browser&&status==Status::searching)for(unsigned count=0;count<8;++count){auto n=browser->read(rx);if(!n)break;
            if(*n<32||*n>rx.size())continue;lan::Reader r{std::span(rx).first(*n)};
            unsigned change=r.u8();r.u8();unsigned name=r.u16(),type=r.u16(),txt=r.u16();
            if(change<1||change>3||32+name+type+txt!=*n)continue;
            auto address=pxa::NetworkEndpoint::decode(std::span(rx).subspan(8,24));if(!address||address->family!=4)continue;
            unsigned i=0;while(i<roomCount&&!same(rooms[i].address,*address))++i;
            if(change==3){if(i<roomCount)rooms[i]=rooms[--roomCount];continue;}
            if(txt!=11)continue;r.pos=32+name+type;if(r.u8()!=10||r.u8()!='a'||r.u8()!='='||r.u8()!='A'||r.u8()!=lan::version)continue;
            unsigned c=r.u8(),s=r.u8();uint32_t id=r.u32();if(!r.done()||c>=characterCount||s>=4||!id)continue;
            if(i<rooms.size()){if(i==roomCount)++roomCount;rooms[i]={*address,id,now,int(c),int(s)};log("room-found");}}
        for(unsigned i=0;i<16&&socket;++i){auto n=socket->read(rx);if(!n){if(n.error()!=pxa::Error::would_block){errorCode=int(n.error());fail("网络连接不可用");}break;}
            if(*n<24||*n>rx.size())continue;auto endpoint=pxa::NetworkEndpoint::decode(std::span(rx).first(24));if(!endpoint||endpoint->family!=4)continue;
            lan::Packet p{};if(lan::parse(std::span(rx).subspan(24,*n-24),p))receive(p,*endpoint,arena);}
        if(!socket)return;
        if(connected&&now-lastRx>6000){fail("连接已中断，请重新加入");return;}
        if(status==Status::joining&&now-opened>8000){fail("未能加入，请检查地址与网络");return;}
        if(status==Status::joining&&now-lastSend>=250){lan::Writer w(lan::Kind::join,room,session);w.u8(characters[1]);send(w);lastSend=now;}
        if(!connected)return;
        if(host){
            if(localRematch&&remoteRematch&&arena.phase==Phase::match_end)resetMatch();
            if(localReady&&remoteReady&&!started){started=true;status=Status::playing;log("fight");}
            if(now-lastWelcome>=1000)welcome();
        }
        // The app resets Arena after poll returns. Never tag the old match's
        // final state with the new match number while that reset is pending.
        if(host&&resetWanted)return;
        if(now-lastSend<30)return;lastSend=now;
        if(host){lan::Writer w(lan::Kind::state,room,session,++sequence);w.u16(match);
            w.u8(unsigned(started)|(unsigned(localPause)<<1)|(unsigned(remotePause)<<2)|(unsigned(localRematch)<<3)|(unsigned(remoteRematch)<<4)|(unsigned(stalled()||remoteStall)<<5));
            w.u32(incoming.ack);w.u32(echo);lan::writeArena(w,arena);send(w);
        }else{lan::Writer w(lan::Kind::input,room,session,++sequence);w.u16(match);w.u32(control);w.u32(now);w.u16(ping);
            w.u8(unsigned(localReady)|(unsigned(localPause)<<1)|(unsigned(localRematch)<<2)|(unsigned(stalled())<<3));w.u8(outgoing.count);
            for(unsigned i=0;i<outgoing.count;++i){w.u32(outgoing.events[i].seq);w.u16(outgoing.events[i].buttons);}send(w);}
    }
};
} // namespace ash
