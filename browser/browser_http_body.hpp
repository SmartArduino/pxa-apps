// SPDX-License-Identifier: MIT
#pragma once
#include "browser_content_encoding.hpp"
#include <pxa/http.hpp>
#include <pxa/clock.hpp>
#include <array>

namespace browser {
// Adds one bounded inflate step per read. The caller's existing receive buffer
// is also the output buffer; compressed input uses a lazy 1 KiB scratch block.
class HttpBodyReader {
public:
    HttpBodyReader(pxa::HttpExchange& body,ContentEncoding encoding,std::size_t limit,pxa::ClockService clock)
        :body_(body),decoder_(encoding,limit),clock_(clock){}
    std::size_t decoded()const{return decoder_.size();}
    std::size_t wire_bytes()const{return wire_bytes_;}
    std::size_t peak_heap()const{return decoder_.peak_heap()+(input_?sizeof(Input):0);}
    void release(){decoder_.release();input_.reset();}
    pxa::Task<std::size_t> read_some(std::span<std::byte> output) {
        if(!decoder_.supported())co_return std::unexpected(pxa::Error::unsupported);
        if(output.empty())co_return std::unexpected(pxa::Error::invalid_argument);
        // Each nested Task completion can add native AOT call frames when
        // the network buffer is already ready. Bound both bytes and read calls
        // for identity AND compressed bodies, including one-byte fragments.
        if(++turn_reads_>4 || decoder_.size()-yielded_>=16384) {
            yielded_=decoder_.size();turn_reads_=1;
            auto yielded=co_await clock_.yield();
            if(!yielded)co_return std::unexpected(yielded.error());
        }
        if(!decoder_.compressed()) {
            auto read=co_await body_.read_some(output);
            if(!read)co_return std::unexpected(read.error());
            wire_bytes_+=*read;
            auto decoded=decoder_.step(output.first(*read),output);
            if(decoded.error!=ContentDecoder::Error::none)co_return std::unexpected(error(decoded.error));
            co_return decoded.produced;
        }
        if(!input_)input_.reset(new(std::nothrow) Input{});
        if(!input_)co_return std::unexpected(pxa::Error::resource_limit);
        for(;;) {
            if(input_->at==input_->size&&!decoder_.pending()) {
                auto read=co_await body_.read_some(input_->bytes);
                if(!read)co_return std::unexpected(read.error());
                wire_bytes_+=*read;input_->at=0;input_->size=*read;
                if(!*read) {
                    auto status=decoder_.finish();
                    if(status!=ContentDecoder::Error::none)co_return std::unexpected(error(status));
                    co_return std::size_t(0);
                }
            }
            auto step=decoder_.step(std::span{input_->bytes}.subspan(input_->at,input_->size-input_->at),output);
            if(step.error!=ContentDecoder::Error::none)co_return std::unexpected(error(step.error));
            input_->at+=step.consumed;
            if(step.produced)co_return step.produced;
        }
    }
private:
    static pxa::Error error(ContentDecoder::Error value) {
        if(value==ContentDecoder::Error::limit)return pxa::Error::limit_exceeded;
        if(value==ContentDecoder::Error::memory)return pxa::Error::resource_limit;
        return pxa::Error::protocol_error;
    }
    struct Input {std::array<std::byte,1024> bytes;std::size_t at=0,size=0;};
    pxa::HttpExchange& body_;
    ContentDecoder decoder_;
    std::unique_ptr<Input> input_;
    pxa::ClockService clock_;
    std::size_t wire_bytes_=0,yielded_=0;
    unsigned turn_reads_=0;
};
}
