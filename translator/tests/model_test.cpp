// SPDX-License-Identifier: MIT
#include "../model.hpp"
#include "../ogg.hpp"
#include "../../common/media/json.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
using namespace translate;
static std::string unquote(const char *s){std::string out;for(size_t i=0;s[i];++i){if(s[i]=='%'){unsigned v=0;assert(std::sscanf(s+i+1,"%2x",&v)==1);out+=char(v);i+=2;}else out+=s[i];}return out;}
static void chunks(const std::string &input,bool speech){size_t at=0;std::string restored;unsigned calls=0;while(at<input.size()){char url[513];size_t next=at;bool ok=speech?tts_url(input.c_str(),at,0,url,sizeof(url),next):translation_url(input.c_str(),at,0,1,url,sizeof(url),next);assert(ok&&next>at&&next<=input.size()&&std::strlen(url)<=512);const char *q=std::strstr(url,"&q=");assert(q);auto fragment=unquote(q+3);assert(utf8(fragment.data(),fragment.size()));restored+=fragment;at=next;assert(++calls<100);}assert(restored==input);}
int main(int argc,char **argv){
 assert(valid_input("你好，世界。"));assert(!valid_input("  \n\t"));assert(!utf8("\xc0\xaf",2));assert(!utf8("\xed\xa0\x80",3));assert(!utf8("\xf4\x90\x80\x80",4));
 std::string source;for(int i=0;i<100;++i)source+="你好 🌍 A & B?\n";chunks(source,false);chunks(source,true);chunks(std::string(500,'x'),false);
 char out[4097];const char *json="{\"responseStatus\":200,\"quotaFinished\":false,\"responseData\":{\"translatedText\":\"<g id=\\\"1\\\">Hello</g> &amp; world.\"}}";
 assert(parse_translation((const uint8_t*)json,std::strlen(json),false,out,sizeof(out))==Parse::Ok);assert(!std::strcmp(out,"Hello & world."));
 json="{\"translatedText\":\"你好，世界。\\n😀\"}";assert(parse_translation((const uint8_t*)json,std::strlen(json),true,out,sizeof(out))==Parse::Ok);assert(!std::strcmp(out,"你好，世界。\n😀"));
 assert(parse_translation((const uint8_t*)json,std::strlen(json),true,out,3)==Parse::TooLong);
 json="{\"quotaFinished\":true,\"responseStatus\":403}";assert(parse_translation((const uint8_t*)json,std::strlen(json),false,out,sizeof(out))==Parse::Quota);
 json="{\"translatedText\":\"result\"} trailing";assert(parse_translation((const uint8_t*)json,std::strlen(json),true,out,sizeof(out))==Parse::Invalid);
 json="{\"text\":\"Hello world\"}";assert(parse_transcription((const uint8_t*)json,std::strlen(json),out,sizeof(out))==Parse::Ok);assert(!std::strcmp(out,"Hello world"));
 assert(endpoint("https://example.org/v1/audio/transcriptions",true));assert(endpoint("http://192.168.1.2:8080/inference"));assert(!endpoint("http://example.org",true));assert(!endpoint("https://token@example.org"));assert(!endpoint("https://example.org\r\nX: y"));assert(!header_value("secret\r\nAuthorization: x"));assert(!model_name("bad\"model"));
 Settings settings;std::strcpy(settings.asr_key,"private-example-key");uint8_t saved[1800],corrupt[1800];size_t n=save_settings(settings,saved,sizeof(saved));assert(n&&n<2048);Settings loaded;assert(load_settings(saved,n,loaded));assert(!std::strcmp(loaded.asr_key,settings.asr_key));
 for(size_t i=0;i<n;++i){std::memcpy(corrupt,saved,n);corrupt[i]^=1;Settings original=loaded;assert(!load_settings(corrupt,n,loaded));assert(!std::strcmp(loaded.asr_key,original.asr_key));}
 asr_preset(loaded,1);assert(!loaded.asr_key[0]&&!std::strcmp(loaded.asr_model,"gpt-transcribe"));
 assert(json_quote("quote\"slash\\newline\n",out,sizeof(out)));char decoded[128];assert(media::json::Value{(const uint8_t*)out,std::strlen(out)}.string(decoded,sizeof(decoded)));assert(!std::strcmp(decoded,"quote\"slash\\newline\n"));
 OggOpus audio;audio.reset();const uint8_t silence[]={0xf8,0xff,0xfe};for(int i=0;i<50;++i)assert(audio.append(silence,sizeof(silence),20));assert(audio.finish()&&audio.granule==48000&&audio.frames==50);assert(!audio.append(silence,sizeof(silence),20));
 size_t offset=0;unsigned pages=0;while(offset<audio.size){auto *p=audio.data+offset;assert(!std::memcmp(p,"OggS",4));size_t bytes=27+p[26];for(unsigned i=0;i<p[26];++i)bytes+=p[27+i];uint32_t crc=p[22]|uint32_t(p[23])<<8|uint32_t(p[24])<<16|uint32_t(p[25])<<24;std::memset(p+22,0,4);assert(crc==OggOpus::crc(p,bytes));OggOpus::put(p+22,crc,4);offset+=bytes;++pages;}assert(pages==52&&offset==audio.size);
 if(argc==2){std::ofstream file(argv[1],std::ios::binary);file.write((const char*)audio.data,audio.size);}
 std::puts("PASS: UTF-8 segmentation, strict response parsing, service configuration, secret-header validation, settings corruption and Ogg Opus CRC.");
}
