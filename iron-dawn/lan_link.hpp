// SPDX-License-Identifier: MIT
#pragma once
#include <pxa/app.hpp>
#include "lan_protocol.hpp"

namespace iron::lan {
class Link {
 std::array<std::byte,header_size+max_payload> rx{},tx{};
 size_t received=0,sent=0,queued=0;
 Header incoming{};
public:
 pxa::Socket socket;
 bool idle()const{return queued==0;}

 template<class Encode> bool send(Message type,Encode encode){
  if(!idle())return false;
  size_t size=encode(std::span{tx}.subspan(header_size));
  if(size>max_payload||(size==0&&type!=Message::hello&&type!=Message::ping))return false;
  write_header(tx,type,size);sent=0;queued=header_size+size;
  return true;
 }
 bool signal(Message type){return send(type,[](auto){return size_t(0);});}

 template<class Receive> bool pump(Receive receive){
  // One simulation tick can service a few chunks without monopolizing app events.
  for(int i=0;i<4&&!idle();++i){
   auto wrote=socket.write(std::span{tx}.subspan(sent,std::min(size_t(4096),queued-sent)));
   if(!wrote){if(wrote.error()==pxa::Error::would_block)break;return false;}
   if(!*wrote)break;
   sent+=*wrote;
   if(sent==queued)queued=sent=0;
  }
  for(int i=0;i<4;++i){
   const size_t end=received<header_size?header_size:header_size+incoming.size;
   auto read=socket.read(std::span{rx}.subspan(received,std::min(size_t(4096),end-received)));
   if(!read){if(read.error()==pxa::Error::would_block)break;return false;}
   if(!*read)return false;
   received+=*read;
   if(received<header_size)continue;
   if(received==header_size&&!read_header(rx,incoming))return false;
   if(received==header_size+incoming.size){
    if(!receive(incoming.type,std::span{rx}.subspan(header_size,incoming.size)))return false;
    received=0;
   }
  }
  return true;
 }
};
}
