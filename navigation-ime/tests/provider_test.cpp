// SPDX-License-Identifier: Apache-2.0
#include "../provider.cpp"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
static std::vector<uint8_t> response;
extern "C" int32_t pxa_call(uint32_t route,uint64_t token,const uint8_t* p,uint32_t n){assert(route==(PXA_IME_SERVICE|(PXA_IME_PRESENT<<16))&&!token);response.assign(p,p+n);return 0;}
static uint32_t seq=0;
static void send(uint16_t action,std::string_view text={},bool sensitive=false,uint8_t lang=PXA_IME_LANGUAGE_ENGLISH,uint32_t value=0){
 uint8_t p[512]={},wire[768];uint32_t n=0;
 pxa_store_u64(p,1);pxa_store_u32(p+8,++seq);pxa_store_u16(p+12,action);p[14]=sensitive;p[17]=lang;pxa_store_u32(p+20,value);
 if(!text.empty())memcpy(p+24,text.data(),text.size());assert(pxa_build_message(wire,sizeof(wire),PXA_IME_SERVICE,PXA_IME_INPUT,0,p,24+text.size(),&n));
 assert(pxa_app_on_event(wire,n)==PXA_EVENT_HANDLED);
}
static std::string preedit(){return {reinterpret_cast<char*>(response.data()+32),pxa_load_u16(response.data()+20)};}
static std::string committed(){return {reinterpret_cast<char*>(response.data()+32+pxa_load_u16(response.data()+20)),pxa_load_u16(response.data()+22)};}
int main(){
 assert(pxa_app_start(nullptr,0)==0);
 send(PXA_IME_ACTION_RESET);send(PXA_IME_ACTION_TEXT,"abc");assert(committed()=="abc"&&preedit().empty());
 send(PXA_IME_ACTION_TEXT,"a1b2",true,PXA_IME_LANGUAGE_CHINESE);assert(committed()=="a1b2"&&preedit().empty()&&!pxa_load_u16(response.data()+24));
 send(PXA_IME_ACTION_RESET,{},false,PXA_IME_LANGUAGE_CHINESE);
 send(PXA_IME_ACTION_TEXT,"shi",false,PXA_IME_LANGUAGE_CHINESE);assert(preedit()=="shi"&&pxa_load_u16(response.data()+24)>0);
 send(PXA_IME_ACTION_PAGE,{},false,PXA_IME_LANGUAGE_CHINESE,12);assert(pxa_load_u32(response.data()+16)==12);
 send(PXA_IME_ACTION_CHOOSE,{},false,PXA_IME_LANGUAGE_CHINESE,12);assert(committed().size()==3&&preedit().empty());
 send(PXA_IME_ACTION_TEXT,"shimin",false,PXA_IME_LANGUAGE_CHINESE);
 send(PXA_IME_ACTION_CHOOSE,{},false,PXA_IME_LANGUAGE_CHINESE,0);assert(committed().size()==3&&preedit()=="min");
 send(PXA_IME_ACTION_CHOOSE,{},false,PXA_IME_LANGUAGE_CHINESE,0);assert(committed().size()==3&&preedit().empty());
 send(PXA_IME_ACTION_BACKSPACE);assert(pxa_load_u16(response.data()+14)==PXA_IME_EDIT_BACKSPACE);
 send(PXA_IME_ACTION_CONFIRM,{},false,PXA_IME_LANGUAGE_ENGLISH,1);assert(pxa_load_u16(response.data()+14)==PXA_IME_EDIT_SUBMIT&&(pxa_load_u16(response.data()+12)&2));
 uint8_t bad[8]={};assert(pxa_app_on_event(bad,sizeof(bad))==PXA_EVENT_UNHANDLED);
 std::cout<<"PASS: English, sensitive inputs, pinyin paging, composition continuation, backspace, submit, malformed events\n";
}
