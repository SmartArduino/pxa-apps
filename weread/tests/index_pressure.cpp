// Optional Guest-only regression, never linked into the reader application.
// In an isolated copy of weread/, copy this file to main.cpp and build with
// the normal package/CMake files (maximum linear memory must remain 2 MiB).
#include <pxa/app.hpp>
#include "hydration.hpp"
#include <array>
#include <cstdlib>
#include <cstdio>
struct App {
    pxa::Task<void> run(pxa::Context& context){
        std::string source;source.reserve(100000);
        source="window.__NUXT__=(function(";
        for(unsigned i=0;i<9000;++i){if(i)source+=',';source+='a';source+=std::to_string(i);}
        source+="){return {state:{reader:{bookInfo:{bookId:'1'},curChapter:{chapterUid:1},contentText:'ready',chapterIndexes:[],readerUrlParams:[]}}}}(";
        for(unsigned i=0;i<9000;++i){if(i)source+=',';source+='0';}source+="));";
        (void)context.log().write(pxa::LogLevel::info,"WEREAD INDEX PRESSURE BEGIN");
        std::array<void*,128> pressure{};unsigned blocks=0;
        for(;blocks<pressure.size();++blocks){
            pressure[blocks]=std::malloc(16384);if(!pressure[blocks])break;
        }
        weread::Hydration parser;bool rejected=false;
        {
            auto job=parser.preparation(source);
            while(!job.done())job.step();
            rejected=!job.result()&&parser.capacity_limited;
        }
        for(auto* p:pressure)std::free(p);
        if(!blocks||blocks==pressure.size()||!rejected)co_return std::unexpected(pxa::Error::bad_state);
        auto result=parser.decode_reader(source);
        if(!result||weread::Json(*result).get("contentText").string()!="ready")
            co_return std::unexpected(pxa::Error::protocol_error);
        char line[96];std::snprintf(line,sizeof(line),"WEREAD INDEX PRESSURE COMPLETE blocks=%u",blocks);
        (void)context.log().write(pxa::LogLevel::info,line);
        co_return pxa::Result<void>{};
    }
    void on_error(pxa::Context& context,pxa::Error error)noexcept{
        char line[96];std::snprintf(line,sizeof(line),"WEREAD INDEX PRESSURE FAILED code=%d",int(error));
        (void)context.log().write(pxa::LogLevel::error,line);
    }
    pxa::Result<void> on_start(pxa::Context& context,std::span<const std::byte>){return context.tasks().start(run(context));}
};
PXA_APPLICATION(App)
