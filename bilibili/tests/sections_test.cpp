// SPDX-License-Identifier: MIT
#include "../sections.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
media::json::Value json(const std::string& s){return {reinterpret_cast<const uint8_t*>(s.data()),s.size()};}
int main(int argc,char **argv){using namespace bili;
    char duration[32];duration_text(24977,duration,sizeof(duration));assert(!std::strcmp(duration,"6:56:17"));duration_text(3599,duration,sizeof(duration));assert(!std::strcmp(duration,"59:59"));duration_text(3600,duration,sizeof(duration));assert(!std::strcmp(duration,"1:00:00"));
    assert(md5("")=="d41d8cd98f00b204e9800998ecf8427e");assert(md5("abc")=="900150983cd24fb0d6963f7d28e17f72");
    assert(md5("message digest")=="f96b697d7cb7938d525a2f31aaf161d0");assert(md5(std::string(1000,'a'))=="cabe45dcc9ae5b66ba86600cca6b8ba8");
    auto key=mixin_key("https://i0.hdslb.com/bfs/wbi/7cd084941338484aae1ad9425b84077c.png","https://i0.hdslb.com/bfs/wbi/4932caff0ff746eab6f01bf08b70ac45.png");assert(key=="ea1db124af3c7062474693fa704f4ff8");
    auto q=wbi_query({{"foo","114"},{"bar","514"},{"baz","1919810"}},key,1702204169);assert(q=="bar=514&baz=1919810&foo=114&wts=1702204169&w_rid=6149fdadf571698ca7e6a567265cd0ee");
    assert(url_decode("a%2Bb+c")=="a+b c");assert(url_decode("%0d%0a").empty());assert(url_decode("%xy").empty());
    Account a;assert(!a.accept_url("https://evil.example/crossDomain?DedeUserID=1&SESSDATA=fake&bili_jct=test"));
    assert(a.accept_url("https://passport.biligame.com/crossDomain?DedeUserID=123&SESSDATA=fake%252Ctoken&bili_jct=test"));
    Account from_headers;assert(from_headers.accept_cookie_headers("SESSDATA=fake%2Ctoken; bili_jct=test; DedeUserID=123;"));assert(from_headers.cookie()==a.cookie());
    auto cookie=a.cookie();assert(cookie=="SESSDATA=fake%2Ctoken; bili_jct=test; DedeUserID=123");auto bytes=a.encode();Account b;assert(b.decode(bytes.data(),bytes.size())&&b.cookie()==cookie);
    bytes[4]='\n';assert(!b.decode(bytes.data(),bytes.size()));assert(b.cookie()==cookie);
    assert(!a.accept_url("https://passport.biligame.com/crossDomain?DedeUserID=123&SESSDATA=bad%3Bheader&bili_jct=test"));assert(a.cookie()==cookie);
    std::string view=R"({"data":{"pages":[{"cid":1,"part":"第一集","duration":10},{"cid":2,"part":"第二集","duration":20}]}})";Video parent;std::snprintf(parent.bvid,sizeof(parent.bvid),"BV1J4411v7g6");auto parts=video_parts(json(view).get("data"),parent);assert(parts.size()==2&&parts[1].cid==2&&parts[1].duration==20);
    SectionData data;std::string h=R"({"data":{"list":[{"title":"历史视频","cover":"https://i0.hdslb.com/a.jpg","author_name":"UP","duration":99,"history":{"bvid":"BV1J4411v7g6","cid":22}}],"cursor":{"max":10,"view_at":20,"business":"archive"}}})";
    data.parse(json(h),Section::cloud_history,parent);assert(data.videos.size()==1&&data.videos[0].duration==99&&data.videos[0].cid==22&&data.more&&data.next=="max=10&view_at=20&business=archive");
    std::string c=R"({"data":{"replies":[{"rpid":1,"member":{"uname":"测试"},"content":{"message":"一条评论🙂"},"replies":[{"member":{"uname":"回复"},"content":{"message":"二级"}}]}],"page":{"num":1,"count":21}}})";
    data.parse(json(c),Section::comments,parent);assert(data.entries.size()==2&&data.more);
    std::string qr=R"({"url":"https://passport.bilibili.com/h5-app/passport/login/scan?navhide=1&qrcode_key=0123456789abcdef0123456789abcdef&from=main-fe-header","qrcode_key":"0123456789abcdef0123456789abcdef"})";
    LoginCode login;assert(login.generate(json(qr),1000));assert(login.valid&&login.expires==181000&&qrcodegen_getSize(login.pixels)<=49);
    if(argc>1){std::ifstream input(argv[1]);std::string body((std::istreambuf_iterator<char>(input)),{});assert(login.generate(json(body).get("data"),2000));}
    if(argc>2){int n=qrcodegen_getSize(login.pixels);std::ofstream f(argv[2],std::ios::binary);f<<"P5\n"<<(n+8)*6<<" "<<(n+8)*6<<"\n255\n";
    for(int y=-4;y<n+4;++y)for(int sy=0;sy<6;++sy)for(int x=-4;x<n+4;++x)for(int sx=0;sx<6;++sx)f.put(qrcodegen_getModule(login.pixels,x,y)?0:255);}
    std::cout<<"Account persistence, cookie isolation inputs, MD5/WBI, parts, history, comments and QR passed\n";
}
