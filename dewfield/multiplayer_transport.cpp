// SPDX-License-Identifier: MIT
#include "multiplayer_transport.hpp"
#include "multiplayer.hpp"
#include "multiplayer_wire.hpp"
#include "pxa_permission.h"
#include "pxa_lan.h"
#include "pxa_websocket.h"
#include "pxa_window.h"
#include <cstdio>
#include <cstring>

namespace dewfield_transport {
namespace {
using namespace dewfield_net;
constexpr char Service[]="_dewfield._tcp";
enum class Request { None, Caps, ListenGrant, Listener, DiscoverGrant, Advertise, Browse, SocketGrant, Connect };
struct Link {
  uint64_t handle=0;
  uint8_t rx[wire::Buffer]{},tx[wire::Buffer]{},fragment[MaxPacket+wire::Header]{};
  unsigned received=0,length=0,sent=0,fragments=0;
  uint32_t opened=0,last_read=0;
  bool detected=false,websocket=false,native_websocket=false,connected=false,fragmented=false;
};
Link links[MaxPlayers];
uint64_t listener=0,advertisement=0,browser_handle=0,grant=0,discovery_grant=0;
uint64_t sequence=0x44570000,pending_token=0;
Request pending=Request::None;
bool advertising=false;
uint32_t now=0,connecting_at=0;
uint8_t scratch[1536];
char target[96]{};

void log(const char*s){pxa_log_write(2,s);}
uint64_t token(Request kind){pending=kind;pending_token=++sequence;return pending_token;}
void drop(unsigned id,bool notify=true){
  if(id>=MaxPlayers)return;
  auto &l=links[id];if(l.handle)pxa_close_handle(l.handle);l=Link{};
  if(notify)room.disconnected(id);
}
void clear(){
  (void)pxa_window_keep_screen_on(0);
  if(pending_token)pxa_cancel(pending_token);
  pending=Request::None;pending_token=0;
  for(unsigned i=0;i<MaxPlayers;++i)drop(i,false);
  for(auto*h:{&listener,&advertisement,&browser_handle,&grant,&discovery_grant}){
    if(*h)pxa_close_handle(*h);*h=0;
  }
  connecting_at=0;advertising=false;
}
bool queue(unsigned id,Message type,const uint8_t*body,unsigned size){
  if(id>=MaxPlayers||size>MaxPacket)return false;
  auto &l=links[id];if(!l.handle||!l.connected||l.length)return false;
  uint8_t packet[MaxPacket+wire::Header];unsigned n=wire::frame(type,body,size,packet);
  if(l.native_websocket){l.tx[0]=2;std::memcpy(l.tx+1,packet,n);l.length=n+1;}
  else if(l.websocket)l.length=wire::websocket(packet,n,l.tx);
  else{std::memcpy(l.tx,packet,n);l.length=n;}
  l.sent=0;return l.length!=0;
}
void fail(const char*text){log(text);room.leave(text);}
int permission(Request kind,const char*name,const char*scope){
  auto t=token(kind);uint32_t n=0;
  if(!pxa_permission_build(scratch,sizeof scratch,PXA_PERMISSION_ACQUIRE,t,name,std::strlen(name),
      reinterpret_cast<const uint8_t*>(scope),std::strlen(scope),&n))return -1;
  return pxa_control_packet(scratch,n);
}
int local_open(Request kind,unsigned resource,uint64_t permission_handle){
  pxa_local_request_t r{};r.permission=permission_handle;r.kind=uint16_t(resource);
  r.bind.family=4;r.buffer_bytes=4096;r.max_items=3;
  if(resource==PXA_LOCAL_LISTENER){r.flags=PXA_LOCAL_NO_DELAY;r.bind.port=room.port;}
  else{r.type=Service;r.type_size=sizeof(Service)-1;}
  if(resource==PXA_LOCAL_ADVERTISE){
    r.name="Dewfield farm";r.name_size=13;r.bind.port=room.port;
    uint8_t endpoint[24];if(pxa_io(listener,PXA_LOCAL_IO_ENDPOINT,endpoint,sizeof endpoint)==24)
      std::memcpy(r.bind.address,endpoint+8,16);
    static const uint8_t txt[]={3,'v','=','1'};r.txt=txt;r.txt_size=sizeof txt;
  }
  return pxa_local_open(token(kind),&r,scratch,sizeof scratch);
}
int socket_open(){
  auto t=token(Request::Connect);
  if(room.browser)return pxa_websocket_open(t,grant,target,uint16_t(std::strlen(target)),8192,MaxPacket+wire::Header,10000,scratch,sizeof scratch);
  unsigned at=0;
  auto record=[&](unsigned tag,const void*data,unsigned n){put16(scratch+at,tag);put16(scratch+at+2,n);at+=4;std::memcpy(scratch+at,data,n);at+=n;};
  uint8_t number[8];record(1,target,unsigned(std::strlen(target)));put32(number,10000);
  put16(scratch+at,3);put16(scratch+at+2,8);at+=4;pxa_store_u64(scratch+at,grant);at+=8;
  record(8,number,4);put32(number,4096);record(14,number,4);
  return pxa_control_call(9,5,t,scratch,at);
}
void start(bool host,const char*ip,uint16_t port){
  clear();connecting_at=now;
  (void)pxa_window_keep_screen_on(1);
  if(host){if(!room.can_host){fail("Host cannot listen. Update firmware.");return;}
    if(permission(Request::ListenGrant,"net.listen","network"))fail("Cannot request listener permission");
  }else{
    std::snprintf(target,sizeof target,room.browser?(room.secure?"wss://%s:%u/dewfield":"ws://%s:%u/dewfield"):"tcp://%s:%u",ip,port);
    if(permission(Request::SocketGrant,"net.socket","outbound"))fail("Cannot request connection permission");
  }
}
void read_browser(){
  if(!browser_handle)return;
  for(unsigned budget=0;budget<3;++budget){
    uint8_t data[512];int n=pxa_io(browser_handle,1,data,sizeof data);
    if(n==-7)return;
    if(n<32){pxa_close_handle(browser_handle);browser_handle=0;return;}
    unsigned name=get16(data+2),type=get16(data+4),txt=get16(data+6);
    if(unsigned(n)!=32+name+type+txt||name>54||get16(data+8)!=4)continue;
    char ip[24];std::snprintf(ip,sizeof ip,"%u.%u.%u.%u",data[16],data[17],data[18],data[19]);
    unsigned index=room.found_count;
    for(unsigned i=0;i<room.found_count;++i)
      if(!std::strcmp(room.found[i].ip,ip)&&room.found[i].port==get16(data+10))index=i;
    if(data[0]==3){
      if(index<room.found_count){for(unsigned i=index;i+1<room.found_count;++i)room.found[i]=room.found[i+1];--room.found_count;room.changed=true;}continue;
    }
    if(index>=3)continue;
    if(index==room.found_count)++room.found_count;
    char message[80];std::snprintf(message,sizeof message,"Dewfield NET FOUND %s:%u",ip,get16(data+10));if(index+1==room.found_count)log(message);
    auto &f=room.found[index];std::memcpy(f.name,data+32,name);f.name[name]=0;
    std::snprintf(f.ip,sizeof f.ip,"%s",ip);f.port=get16(data+10);room.changed=true;
  }
}
bool deliver(unsigned id,const uint8_t*data,unsigned size){
  Message type;unsigned body;int n=wire::packet(data,size,type,body);
  if(n<=0||unsigned(n)!=size)return false;
  bool was_ready=room.ready;
  if(!room.receive(id,type,data+wire::Header,body))return false;
  if(type==Message::Hello||type==Message::Snapshot){char message[96];std::snprintf(message,sizeof message,"Dewfield NET %s peer=%u players=%u day=%u gold=%u",type==Message::Hello?"JOIN":"SYNC",id,room.players(),room.state->day,room.state->gold);
    // A single join log; ordinary snapshots stay quiet.
    if(type==Message::Hello||!was_ready)log(message);
  }
  return true;
}
bool parse(unsigned id){
  auto &l=links[id];unsigned consumed=0;
  if(!l.detected){
    if(l.received<4)return true;
    if(!std::memcmp(l.rx,"DWFN",4)){l.detected=true;}
    else if(!std::memcmp(l.rx,"GET ",4)){
      if(l.length)return true;
      int n=wire::upgrade(l.rx,l.received,reinterpret_cast<char*>(l.tx),sizeof l.tx);
      if(n<0)return false;if(!n)return true;
      l.length=unsigned(std::strlen(reinterpret_cast<char*>(l.tx)));l.sent=0;
      consumed=unsigned(n);l.detected=l.websocket=true;
    }else return false;
  }
  while(consumed<l.received){
    if(l.websocket){
      wire::WebFrame frame;int n=wire::websocket_frame(l.rx+consumed,l.received-consumed,frame);
      if(n<0)return false;if(!n)break;
      auto *payload=l.rx+consumed+frame.header;
      for(unsigned i=0;i<frame.length;++i)payload[i]^=frame.mask[i%4];
      if(frame.opcode==8)return false;
      if(frame.opcode==9){
        if(l.length)return false;
        l.length=wire::websocket(payload,frame.length,l.tx,10);l.sent=0;
      }else if(frame.opcode!=10){
        if(frame.opcode==2){if(l.fragmented)return false;l.fragments=0;}
        else if(!l.fragmented)return false;
        if(frame.length>sizeof(l.fragment)-l.fragments)return false;
        std::memcpy(l.fragment+l.fragments,payload,frame.length);l.fragments+=frame.length;
        l.fragmented=!frame.fin;
        if(frame.fin&&!deliver(id,l.fragment,l.fragments))return false;
      }
      consumed+=unsigned(n);
    }else{
      Message type;unsigned size;int n=wire::packet(l.rx+consumed,l.received-consumed,type,size);
      if(n<0)return false;if(!n)break;
      if(!deliver(id,l.rx+consumed,unsigned(n)))return false;
      consumed+=unsigned(n);
    }
  }
  if(consumed){l.received-=consumed;std::memmove(l.rx,l.rx+consumed,l.received);}
  return l.received<sizeof l.rx;
}
void service_link(unsigned id){
  auto &l=links[id];if(!l.handle)return;
  if(!l.connected){
    int status=pxa_io(l.handle,4,nullptr,0);
    if(status==-7){if(now-l.opened>10000)drop(id);return;}
    if(status!=1){drop(id);return;}
    l.connected=true;l.detected=true;
    uint8_t hello[4];put32(hello,ProtocolVersion);queue(id,Message::Hello,hello,sizeof hello);
  }
  if(l.length){
    int n=pxa_io(l.handle,2,l.tx+l.sent,l.length-l.sent);
    if(n<0&&n!=-7){drop(id);return;}
    if(n>0){l.sent+=unsigned(n);if(l.sent==l.length)l.length=l.sent=0;}
  }
  for(unsigned budget=0;budget<3;++budget){
    if(l.native_websocket){
      uint8_t data[MaxPacket+wire::Header+1];int n=pxa_io(l.handle,1,data,sizeof data);
      if(n==-7)break;
      if(n<1||data[0]!=2||!deliver(id,data+1,unsigned(n-1))){drop(id);return;}
      l.last_read=now;continue;
    }
    unsigned capacity=unsigned(sizeof l.rx)-l.received;
    int n=pxa_io(l.handle,1,l.rx+l.received,std::min(512u,capacity));
    if(n==-7)break;
    if(n<=0){drop(id);return;}
    l.last_read=now;l.received+=unsigned(n);
    if(!parse(id)){drop(id);return;}
  }
  if((!room.farmers[id].pose.active&&room.mode==Mode::Hosting&&now-l.opened>5000)||now-l.last_read>10000)drop(id);
}
} // namespace

void init(){
  room.send=queue;room.start=start;room.close=clear;room.discover=browse;
  if(pxa_control_call(9,8,token(Request::Caps),nullptr,0)){
    pending=Request::None;pending_token=0;room.message("Network Host update required");
  }
}
void browse(){
  if(!room.can_discover){room.message("Use IP and port to join");return;}
  if(pending!=Request::None)return;
  if(browser_handle){pxa_close_handle(browser_handle);browser_handle=0;}
  room.found_count=0;advertising=false;room.message("Searching local farms...");
  if(discovery_grant){if(local_open(Request::Browse,PXA_LOCAL_BROWSE,discovery_grant))room.message("Discovery failed");}
  else if(permission(Request::DiscoverGrant,"net.discovery","local-network"))room.message("Discovery permission failed");
}
bool event(const pxa_event_t&e){
  if(!pending_token||e.token!=pending_token){
    if(e.token>0x44570000&&e.token<=sequence){
      if(e.payload_size==12&&!pxa_load_u32(e.payload))pxa_close_handle(pxa_load_u64(e.payload+4));
      return true;
    }
    return false;
  }
  auto kind=pending;pending=Request::None;pending_token=0;
  if(e.payload_size<4){fail("Invalid network response");return true;}
  int32_t status=int32_t(pxa_load_u32(e.payload));
  if(kind==Request::Caps){
    if(!status&&e.payload_size==36){unsigned flags=pxa_load_u32(e.payload+4);
      room.can_host=flags&32;room.can_discover=flags&128;room.browser=flags&512;room.changed=true;
    }
    return true;
  }
  if(status){
    if(kind==Request::DiscoverGrant||kind==Request::Advertise||kind==Request::Browse){room.message("Discovery unavailable. Use IP and port.");return true;}
    char error[96];std::snprintf(error,sizeof error,"Connection error %ld. Check address, Wi-Fi and permission.",long(status));fail(error);return true;
  }
  if(e.payload_size!=12){fail("Invalid network handle");return true;}
  uint64_t handle=pxa_load_u64(e.payload+4);if(!(handle>>32)){fail("Invalid network resource");return true;}
  if(kind==Request::ListenGrant){grant=handle;if(local_open(Request::Listener,PXA_LOCAL_LISTENER,grant))fail("Cannot create listener");}
  else if(kind==Request::Listener){
    // Native bind/listen is asynchronous; the handle is still connecting.
    listener=handle;
  }else if(kind==Request::DiscoverGrant){
    discovery_grant=handle;
    if(local_open(advertising?Request::Advertise:Request::Browse,advertising?PXA_LOCAL_ADVERTISE:PXA_LOCAL_BROWSE,discovery_grant))room.message("Discovery unavailable");
  }else if(kind==Request::Advertise)advertisement=handle;
  else if(kind==Request::Browse)browser_handle=handle;
  else if(kind==Request::SocketGrant){grant=handle;connecting_at=now;if(socket_open())fail("Cannot open connection");}
  else if(kind==Request::Connect){
    auto &l=links[0];l.handle=handle;l.opened=l.last_read=now;l.native_websocket=room.browser;
  }
  return true;
}
void update(uint32_t timestamp){
  now=timestamp;room.now=timestamp;
  read_browser();
  if(listener&&room.mode==Mode::Hosting&&!room.ready){
    uint8_t endpoint[24];int n=pxa_io(listener,PXA_LOCAL_IO_ENDPOINT,endpoint,sizeof endpoint);
    if(n==-7)return;
    if(n!=24){fail("Cannot read listener address");return;}
    char ip[24];std::snprintf(ip,sizeof ip,"%u.%u.%u.%u",endpoint[8],endpoint[9],endpoint[10],endpoint[11]);
    room.port=get16(endpoint+2);room.host_ready(ip);connecting_at=0;
    char message[96];std::snprintf(message,sizeof message,"Dewfield NET HOST %s:%u",ip,room.port);log(message);
    advertising=true;if(permission(Request::DiscoverGrant,"net.discovery","local-network"))room.message("Host ready. Join by IP and port.");
  }
  if(listener&&room.mode==Mode::Hosting){
    for(unsigned i=1;i<MaxPlayers;++i)if(!links[i].handle){
      uint8_t handle[8];int n=pxa_io(listener,PXA_LOCAL_IO_ACCEPT,handle,sizeof handle);
      if(n==8){auto &l=links[i];l.handle=pxa_load_u64(handle);l.opened=l.last_read=now;l.connected=true;}
      else if(n!=-7&&n!=-9){fail("Host network changed");return;}
      break;
    }
  }
  for(unsigned i=0;i<MaxPlayers;++i)service_link(i);
  room.service(timestamp);
  if(room.mode==Mode::Joining&&pending!=Request::SocketGrant&&connecting_at&&now-connecting_at>60000)fail("Connection timed out");
}
void shutdown(){room.leave();}
} // namespace dewfield_transport
