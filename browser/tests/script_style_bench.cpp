// Host-only paired benchmark. Pass a runtime bytecode asset and plain/styled.
// Compare checksums and CPU time; this is not an FPS or device timing test.
#include "../browser_script.hpp"
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

static std::string markup(browser::script::Page& page) {
    for (unsigned i=0; i<1024 && page.active(); ++i) {
        page.step(); if (auto out=page.take_markup()) return std::move(*out);
    }
    return {};
}
static std::uint64_t hash(std::uint64_t value, const std::string& text) {
    for (unsigned char c : text) { value ^= c; value *= 1099511628211ull; }
    return value;
}
static double cpu_ms(std::clock_t start) { return 1000.0*double(std::clock()-start)/CLOCKS_PER_SEC; }
int main(int argc, char** argv) {
    if (argc!=3 || (std::string(argv[2])!="plain" && std::string(argv[2])!="styled")) return 2;
    std::ifstream file(argv[1],std::ios::binary);
    browser::script::Bootstrap bootstrap{{std::istreambuf_iterator<char>(file),{}}};
    if (bootstrap.runtime.empty()) return 2;
    const bool styled=std::string(argv[2])=="styled";
    std::string html="<html><body>";
    for (unsigned i=0; i<48; ++i) {
        html+="<p id=p"+std::to_string(i);
        if (styled) html+=" style='color:#2468ac;font-size:16px;line-height:1.4;text-align:left;"
            "white-space:normal;display:block;margin:0;padding:0;border:0;width:100%;height:auto;opacity:1'";
        html+=">Styled paragraph</p>";
    }
    html+="<button onclick=update()>Update</button><script>var counter=0;function update(){"
        "document.getElementById('p0').textContent='Update '+(++counter)}</script></body></html>";
    browser::script::Limits limits;limits.heap_bytes=1024*1024;limits.turn_us=200000;
    browser::script::Page page;
    const auto begin=std::clock();
    if (!page.open(std::move(html),"https://example.test/",std::move(bootstrap),{},{},limits)) return 1;
    auto output=markup(page);
    const auto initialization=cpu_ms(begin);
    if (output.empty() || !page.ready() || page.stats().errors) return 1;
    auto checksum=hash(14695981039346656037ull,output);
    const auto repaint=std::clock();
    for (unsigned i=0; i<24; ++i) {
        if (!page.click(1)) return 1;
        output=markup(page);
        if (output.empty() || page.stats().errors) return 1;
        checksum=hash(checksum,output);
    }
    const auto updates=cpu_ms(repaint);
    const auto stats=page.stats();
    std::cout<<"{\"scene\":\""<<argv[2]<<"\",\"updates\":24,\"initialization_cpu_ms\":"<<initialization
        <<",\"updates_cpu_ms\":"<<updates<<",\"vm_current_bytes\":"<<stats.current_bytes
        <<",\"vm_peak_bytes\":"<<stats.peak_bytes<<",\"checksum\":\""<<checksum<<"\"}\n";
}
