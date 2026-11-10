// SPDX-License-Identifier: Apache-2.0
#if defined(__wasm__)
#include <pxa/libcpp_diagnostics.hpp>
#endif
#include "pxa_ime.h"
#include "../pinyin-ime-lite/engine.hpp"
namespace {
pinyin_lite::CharacterEngine engine;
char spelling[64];size_t size=0,count=0,page=0;uint64_t session=0;uint8_t language=0,layout=0;
void reset(){size=count=page=0;engine.reset();}
void search(){count=size?engine.search({spelling,size}):0;page=0;}
size_t choose(size_t index,char *out,size_t capacity){
 if(!size)return 0;
 auto n=count?engine.candidate(index,{out,capacity}):0;
 size_t consumed=0;if(n){engine.choose(index);consumed=engine.consumed();}
 if(!consumed||consumed>size){if(size>capacity)return 0;memcpy(out,spelling,size);n=size;reset();return n;}
 memmove(spelling,spelling+consumed,size-consumed);size-=consumed;search();return n;
}
}
extern "C" int32_t pxa_app_start(const uint8_t *config,uint32_t length){(void)config;(void)length;reset();session=0;return PXA_STATUS_OK;}
extern "C" int32_t pxa_app_on_event(const uint8_t *bytes,uint32_t length){
 pxa_event_t event;pxa_ime_input_t input;
 if(!pxa_parse_event(bytes,length,&event)||!pxa_ime_parse_input(&event,&input))return PXA_EVENT_UNHANDLED;
 if(session!=input.session||input.action==PXA_IME_ACTION_RESET||language!=input.language||layout!=input.layout){reset();session=input.session;language=input.language;layout=input.layout;}
 if(input.text_size>256)return PXA_STATUS_LIMIT_EXCEEDED;
 char commit[512];size_t committed=0;uint16_t edit=PXA_IME_EDIT_NONE;
 switch(input.action){
 case PXA_IME_ACTION_RESET:break;
 case PXA_IME_ACTION_TEXT:{
  bool letters=!input.value&&!input.sensitive&&language==PXA_IME_LANGUAGE_CHINESE&&input.text_size;
  for(size_t i=0;i<input.text_size;++i)letters&=(input.text[i]>='a'&&input.text[i]<='z')||input.text[i]=='\'';
  if(letters){if(size+input.text_size<=sizeof(spelling)){memcpy(spelling+size,input.text,input.text_size);size+=input.text_size;search();}}
  else{
   while(size&&committed+64<=sizeof(commit)){size_t n=choose(0,commit+committed,sizeof(commit)-committed);if(!n)break;committed+=n;}
   if(input.text_size<=sizeof(commit)-committed){memcpy(commit+committed,input.text,input.text_size);committed+=input.text_size;}
   if(committed)edit=PXA_IME_EDIT_INSERT;
  }
  break;
 }
 case PXA_IME_ACTION_BACKSPACE:if(size){--size;search();}else edit=PXA_IME_EDIT_BACKSPACE;break;
 case PXA_IME_ACTION_CONFIRM:
  if(size){committed=choose(0,commit,sizeof(commit));if(committed)edit=PXA_IME_EDIT_INSERT;}
  else if(!input.value){commit[0]=' ';committed=1;edit=PXA_IME_EDIT_INSERT;}
  else edit=PXA_IME_EDIT_SUBMIT;
  break;
 case PXA_IME_ACTION_CHOOSE:if(input.value<count){committed=choose(input.value,commit,sizeof(commit));if(committed)edit=PXA_IME_EDIT_INSERT;}break;
 case PXA_IME_ACTION_PAGE:page=input.value<count?input.value:0;break;
 default:break;
 }
 uint8_t storage[1024];pxa_ime_frame_t frame;
 auto status=pxa_ime_frame_begin(&frame,storage,sizeof(storage),&input,spelling,size,edit,commit,committed);
 if(status!=PXA_STATUS_OK)return status;
 size_t shown=0;char candidate[4];
 for(size_t i=page;i<count&&shown<12;++i){auto n=engine.candidate(i,candidate);if(!n||pxa_ime_frame_candidate(&frame,candidate,n)!=PXA_STATUS_OK)break;++shown;}
 status=pxa_ime_frame_present(&frame,page,page+shown<count,input.action==PXA_IME_ACTION_CONFIRM&&input.value);
 return status==PXA_STATUS_OK?PXA_EVENT_HANDLED:status;
}
extern "C" void pxa_app_stop(uint32_t reason){(void)reason;reset();session=0;}
