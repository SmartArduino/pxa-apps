// Host CPU microbenchmark, not device FPS or network/page-load throughput.
// Override BROWSER_BENCH_HEADER to compare an unchanged source snapshot.
#ifndef BROWSER_BENCH_HEADER
#define BROWSER_BENCH_HEADER "../browser_core.hpp"
#endif
#include BROWSER_BENCH_HEADER
#include <chrono>
#include <ctime>
#include <iostream>
#include <map>

int main(int argc, char **argv) {
    const int rounds = argc > 1 ? std::clamp(std::stoi(argv[1]), 1, 1000) : 40;
    std::map<std::string, std::string> pages;
    for (int i = 0; i < 1400; ++i) pages["article"] += "<p>Chinese 中文阅读 paragraph with a link <a href='/next'>next</a>.</p>";
    for (int i = 0; i < 1700; ++i) pages["plain"] += "中文阅读 ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz\n";
    pages["raw-prefix"] = "<style>" + std::string(1500000, 'x') + "</style><p>Visible 中文</p>";
    pages["comment-prefix"] = "<!--" + std::string(500000, 'x') + "--><p>Visible 中文</p>";
    for (int i = 0; i < 250; ++i) pages["attributes"] += "<div class='card' data-a=1 data-b=2 data-c=3><a title='link' href='/next'>阅读</a></div>";
    std::string sheet = "<style>.article{color:#2468ac;font-size:120%}.note{white-space:pre-line}";
    for (int i = 0; i < 62; ++i) sheet += ".unused" + std::to_string(i) + "{color:red}";
    sheet += "</style>";
    pages["styled-article"] = sheet;
    pages["unique-classes"] = sheet;
    for (int i = 0; i < 500; ++i) {
        pages["styled-article"] += "<p class='article'>中文阅读 paragraph <a href='/next'>next</a> tail.</p>";
        pages["inline-article"] += "<p style='color:#2468ac;font-size:120%;text-align:left'>中文阅读 paragraph.</p>";
        pages["unique-classes"] += "<p class='entry" + std::to_string(i) + "'>中文阅读 paragraph.</p>";
    }
    for (const auto &[name, html] : pages) {
        std::uint64_t checksum = 0;
        const auto start = std::chrono::steady_clock::now();
        const auto cpu_start = std::clock();
        for (int round = 0; round < rounds; ++round) {
            browser::Document d;
            d.clear("https://bench.test/dir/page");
            browser::Parser parser(d);
            for (std::size_t at = 0; at < html.size(); at += 4096) {
                const auto chunk = std::string_view(html).substr(at, 4096);
                if (name == "plain") parser.feed_plain(chunk);
                else parser.feed(chunk);
            }
            parser.finish();
            browser::Layout layout;
            layout.build(d, 272, 15);
            checksum += d.text.size() + layout.height + d.links.size();
            for (const auto &span : d.spans)
                checksum += span.text_style.color + span.text_style.scale;
        }
        const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start).count();
        const auto cpu_us = std::uint64_t(double(std::clock()-cpu_start) * 1000000.0 / CLOCKS_PER_SEC);
        std::cout << "{\"scene\":\"" << name << "\",\"rounds\":" << rounds
                  << ",\"microseconds\":" << us << ",\"cpu_microseconds\":" << cpu_us
                  << ",\"checksum\":" << checksum << "}\n";
    }
}
