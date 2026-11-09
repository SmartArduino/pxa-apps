// SPDX-License-Identifier: MIT
#pragma once
#include "multiplayer_model.hpp"
#include <array>
#include <cstdio>
#include <string_view>

namespace dewfield_net::wire {
constexpr unsigned Header=8, Buffer=2048;
inline unsigned frame(Message type,const uint8_t*body,unsigned n,uint8_t*out){
  if(n>MaxPacket)return 0;
  std::memcpy(out,"DWFN",4);out[4]=1;out[5]=uint8_t(type);put16(out+6,n);
  if(n)std::memcpy(out+Header,body,n);return n+Header;
}
inline int packet(const uint8_t*data,unsigned n,Message&type,unsigned&size){
  if(n<Header)return 0;
  if(std::memcmp(data,"DWFN",4)||data[4]!=1||data[5]<1||data[5]>unsigned(Message::Goodbye))return -1;
  size=get16(data+6);type=Message(data[5]);if(size>MaxPacket)return -1;
  return n<Header+size?0:int(Header+size);
}
inline uint32_t rotate(uint32_t v,unsigned n){return (v<<n)|(v>>(32-n));}
// SHA-1 is used only for the RFC6455 public handshake, never authentication.
inline void sha1(const uint8_t*in,unsigned n,uint8_t*out){
  uint32_t h[]={0x67452301,0xefcdab89,0x98badcfe,0x10325476,0xc3d2e1f0};
  unsigned blocks=(n+9+63)/64;
  for(unsigned block=0;block<blocks;++block){
    uint32_t w[80]{};
    for(unsigned i=0;i<64;++i){
      unsigned at=block*64+i;uint8_t b=at<n?in[at]:at==n?128:0;
      if(at>=blocks*64-4)b=uint8_t((uint64_t(n)*8)>>((blocks*64-1-at)*8));
      w[i/4]|=uint32_t(b)<<(24-(i%4)*8);
    }
    for(unsigned i=16;i<80;++i)w[i]=rotate(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
    uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4];
    for(unsigned i=0;i<80;++i){
      uint32_t f=i<20?(b&c)|(~b&d):i<40?b^c^d:i<60?(b&c)|(b&d)|(c&d):b^c^d;
      uint32_t k=i<20?0x5a827999:i<40?0x6ed9eba1:i<60?0x8f1bbcdc:0xca62c1d6;
      uint32_t t=rotate(a,5)+f+e+k+w[i];e=d;d=c;c=rotate(b,30);b=a;a=t;
    }
    h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;
  }
  for(unsigned i=0;i<20;++i)out[i]=uint8_t(h[i/4]>>(24-i%4*8));
}
inline void base64(const uint8_t*in,unsigned n,char*out){
  constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  unsigned at=0;
  for(unsigned i=0;i<n;i+=3){
    uint32_t v=uint32_t(in[i])<<16;if(i+1<n)v|=uint32_t(in[i+1])<<8;if(i+2<n)v|=in[i+2];
    out[at++]=alphabet[v>>18];out[at++]=alphabet[(v>>12)&63];
    out[at++]=i+1<n?alphabet[(v>>6)&63]:'=';out[at++]=i+2<n?alphabet[v&63]:'=';
  }
  out[at]=0;
}
inline bool equal(std::string_view a,std::string_view b){
  if(a.size()!=b.size())return false;
  for(unsigned i=0;i<a.size();++i){auto c=a[i],d=b[i];if(c>='A'&&c<='Z')c+=32;if(d>='A'&&d<='Z')d+=32;if(c!=d)return false;}return true;
}
inline std::string_view trim(std::string_view s){
  while(!s.empty()&&(s.front()==' '||s.front()=='\t'))s.remove_prefix(1);
  while(!s.empty()&&(s.back()==' '||s.back()=='\t'))s.remove_suffix(1);return s;
}
inline bool token(std::string_view value,std::string_view wanted){
  while(!value.empty()){auto comma=value.find(',');auto t=trim(value.substr(0,comma));if(equal(t,wanted))return true;if(comma==value.npos)break;value.remove_prefix(comma+1);}return false;
}
// Return consumed HTTP bytes, 0 for a partial request, -1 for invalid input.
inline int upgrade(const uint8_t*data,unsigned n,char*reply,unsigned capacity){
  std::string_view http(reinterpret_cast<const char*>(data),n);
  auto end=http.find("\r\n\r\n");if(end==http.npos)return n>=1536?-1:0;
  if(end>1532)return -1;
  auto line=http.find("\r\n");auto request=http.substr(0,line);
  if(!request.starts_with("GET /dewfield ")||!request.ends_with(" HTTP/1.1"))return -1;
  std::string_view key;bool connection=false,websocket=false,version=false,host=false;
  unsigned at=unsigned(line+2);
  while(at<end){
    auto next=http.find("\r\n",at),colon=http.find(':',at);
    if(next==http.npos||colon==http.npos||colon>=next)return -1;
    auto name=http.substr(at,colon-at),value=trim(http.substr(colon+1,next-colon-1));
    if(equal(name,"upgrade"))websocket=equal(value,"websocket");
    else if(equal(name,"connection"))connection=token(value,"upgrade");
    else if(equal(name,"sec-websocket-key")){if(!key.empty())return -1;key=value;}
    else if(equal(name,"sec-websocket-version"))version=value=="13";
    else if(equal(name,"host"))host=!value.empty();
    at=unsigned(next+2);
  }
  if(!connection||!websocket||!version||!host||key.size()!=24||!key.ends_with("=="))return -1;
  for(unsigned i=0;i<22;++i)if(!((key[i]>='A'&&key[i]<='Z')||(key[i]>='a'&&key[i]<='z')||(key[i]>='0'&&key[i]<='9')||key[i]=='+'||key[i]=='/'))return -1;
  char input[60];std::memcpy(input,key.data(),24);std::memcpy(input+24,"258EAFA5-E914-47DA-95CA-C5AB0DC85B11",36);
  uint8_t digest[20];char accept[29];sha1(reinterpret_cast<uint8_t*>(input),60,digest);base64(digest,20,accept);
  int size=std::snprintf(reply,capacity,"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n",accept);
  return size<0||unsigned(size)>=capacity?-1:int(end+4);
}
inline unsigned websocket(const uint8_t*data,unsigned n,uint8_t*out,unsigned opcode=2){
  if(n>MaxPacket+Header)return 0;
  out[0]=uint8_t(128|opcode);unsigned header=n<126?2:4;
  if(header==2)out[1]=uint8_t(n);else{out[1]=126;out[2]=uint8_t(n>>8);out[3]=uint8_t(n);}
  if(n)std::memcpy(out+header,data,n);return n+header;
}
struct WebFrame {unsigned size=0,header=0,opcode=0,length=0;bool fin=false;uint8_t mask[4]{};};
inline int websocket_frame(const uint8_t*data,unsigned n,WebFrame&f){
  if(n<2)return 0;f={};f.fin=data[0]&128;f.opcode=data[0]&15;f.length=data[1]&127;f.header=2;
  if((data[0]&112)||!(data[1]&128)||(f.opcode!=0&&f.opcode!=2&&f.opcode!=8&&f.opcode!=9&&f.opcode!=10))return -1;
  if(f.length==127)return -1;
  if(f.length==126){if(n<4)return 0;f.length=(unsigned(data[2])<<8)|data[3];f.header=4;if(f.length<126)return -1;}
  if(f.length>MaxPacket+Header||(f.opcode>=8&&(!f.fin||f.length>125)))return -1;
  if(n<f.header+4)return 0;
  std::memcpy(f.mask,data+f.header,4);f.header+=4;f.size=f.header+f.length;
  return n<f.size?0:int(f.size);
}
inline bool address(std::string_view ip,std::string_view port,uint16_t&value){
  unsigned part=0,digits=0,parts=0;
  for(char c:ip){if(c=='.'){if(!digits||++parts>3)return false;part=digits=0;}else{if(c<'0'||c>'9'||++digits>3)return false;part=part*10+unsigned(c-'0');if(part>255)return false;}}
  if(parts!=3||!digits||ip=="0.0.0.0"||ip=="255.255.255.255")return false;
  unsigned p=0;if(port.empty()||port.size()>5)return false;
  for(char c:port){if(c<'0'||c>'9')return false;p=p*10+unsigned(c-'0');if(p>65535)return false;}
  if(!p)return false;value=uint16_t(p);return true;
}
} // namespace dewfield_net::wire
