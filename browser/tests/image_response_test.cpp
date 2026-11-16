#include "../browser_image_response.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace browser::image_response;
    for (auto value : {"image/jpeg", "image/jpg", " IMAGE/PJPEG ; charset=binary",
                       "image/x-png", "image/gif", "image/webp"})
        assert(declared_image(value));
    for (auto value : {"", "text/html", "application/octet-stream", "image"})
        assert(!declared_image(value));
    assert(mime(" IMAGE/JPG ; anything=1 ") == "image/jpg");
    assert(request_url("https://reader.test/chapter", "http://cdn.test/p.jpg?x=1#p") ==
           "https://cdn.test/p.jpg?x=1#p");
    assert(request_url("https://reader.test/", "http://cdn.test:80/p.jpg") ==
           "https://cdn.test/p.jpg");
    assert(request_url("https://reader.test/", "http://cdn.test:8080/p.jpg") ==
           "https://cdn.test:8080/p.jpg");
    assert(request_url("https://reader.test/", "http://cdn.test:80") ==
           "https://cdn.test");
    assert(request_url("https://reader.test/", "http://[::1]:80/p.jpg") ==
           "https://[::1]/p.jpg");
    for (auto page : {"http://reader.test/", "about:home"})
        assert(request_url(page, "http://cdn.test/p.jpg") == "http://cdn.test/p.jpg");
    assert(request_url("https://reader.test/", "https://cdn.test/p.jpg") ==
           "https://cdn.test/p.jpg");
    std::cout << "Image MIME and secure request URL PASS\n";
}
