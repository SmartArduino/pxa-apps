#include "../browser_comic.hpp"
#include "../browser_image_view.hpp"
#include <cassert>
#include <iostream>
#include <limits>
using namespace browser;

int main() {
    ImageView view;
    view.open(2);
    view.fit_width(320, 900, 240, 120);
    assert(view.width_fit && (view.rectangle(320, 900, 240, 120) == std::array{0, 0, 240, 675}));
    assert(view.pan(0, -100, 320, 900, 240, 120));
    assert(view.rectangle(320, 900, 240, 120)[1] == -100);
    view.pan(0, -10000, 320, 900, 240, 120);
    assert(view.rectangle(320, 900, 240, 120)[1] == -555);
    assert(!view.pan(0, -100, 320, 900, 240, 120));
    view.fit();
    assert(!view.width_fit && (view.rectangle(320, 900, 240, 120) == std::array{99, 0, 42, 120}));
    for (int w : {1, 96, 640, 4096}) for (int h : {1, 180, 1200, 4096})
        for (int vw : {64, 240, 480, 1024}) {
            view.fit_width(w, h, vw, 240);
            auto box = view.rectangle(w, h, vw, 240);
            assert(box[2] > 0 && box[2] <= 16384 && box[3] > 0 && box[3] <= 16384);
            if (box[3] > 240) assert(box[1] == 0);
            view.pan(-100000, -100000, w, h, vw, 240);
            box = view.rectangle(w, h, vw, 240);
            if (box[3] > 240) assert(box[1] + box[3] >= 240);
        }
    Document d;d.clear("https://comic.test/chapter/");Parser parser(d);
    parser.feed("<img src='/logo.SVG'><img src='/icon.png' width=25 height=25>"
                "<img class='lazy' src='/loading.gif' data-original='/page1.jpg'>"
                "<img class='lazyload' src='/loading.gif' data-src='/page2.png'>"
                "<img data-url='/page3.jpg'><img src='/unsupported.WEBP?key=1'>"
                "<img src='/real.jpg' data-src='/not-this.jpg'>");
    parser.finish();
    assert(d.images.size() == 7 && !d.images_limited);
    assert(d.images[2].source == "https://comic.test/page1.jpg");
    assert(d.images[3].source == "https://comic.test/page2.png");
    assert(d.images[4].source == "https://comic.test/page3.jpg");
    assert(d.images[6].source == "https://comic.test/real.jpg");
    assert((comic_position(d, 3) == std::pair{2, 4}));
    assert(comic_neighbor(d, -1, 1) == 2 && comic_neighbor(d, 2, -1) == -1);
    assert(comic_neighbor(d, 4, 1) == 6 && comic_neighbor(d, 6, 1) == -1);
    assert(comic_neighbor(d, 0, 0) == -1);
    assert(comic_neighbor(d, std::numeric_limits<int>::max(), 1) == -1);
    d.clear("https://comic.test/chapter1");Parser marked(d);
    marked.feed("<head><link rel='next alternate' href='/chapter2'><link rel='prev' href='#'></head>"
                "<img class='comicpage' src='/chapter/cover.jpg'>"
                "<img src='/chapter/page1.jpg'><img src='/ads/banner.jpg'>"
                "<a rel='prev' href='/chapter0'>Previous</a>");marked.finish();
    assert(d.comic_folder == "https://comic.test/chapter/");
    assert((comic_position(d, 1) == std::pair{2, 2}));
    assert(d.next_page == "https://comic.test/chapter2" && d.previous_page == "https://comic.test/chapter0");
    assert(comic_neighbor(d, 1, 1) == -1);
    d.clear("https://comic.test/");Parser chapter(d);
    for (std::size_t i = 0; i < max_images + 5; ++i) chapter.feed("<img src='/page.jpg'>");
    chapter.finish();
    assert(d.images.size() == max_images && d.images_limited);
    assert(comic_position(d, int(max_images - 1)).first == int(max_images));
    d.clear("https://comic.test/new");assert(!d.images_limited && d.comic_folder.empty() && d.next_page.empty());
    std::cout << "comic reader PASS: width fit, pan bounds, image sequence, lazy placeholders and chapter limits\n";
}
