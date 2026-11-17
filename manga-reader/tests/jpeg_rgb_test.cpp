#include "../jpeg_rgb.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
int main(int n,char** a){assert(n==4);std::ifstream f(a[1],std::ios::binary);std::vector<unsigned char> rgb((std::istreambuf_iterator<char>(f)),{});unsigned w=std::stoul(a[2]),h=std::stoul(a[3]);assert(rgb.size()==w*h*3);manga::JpegRgb image;assert(image.begin(rgb.data(),w,h));unsigned rows=0;while(!image.done()){assert(image.step());++rows;}auto data=image.take();assert(!data.empty()&&data.size()<524288&&rows==((h+7)/8)*((w+7)/8));std::ofstream o(std::string(a[1])+".jpg",std::ios::binary);o.write((char*)data.data(),data.size());std::cout<<w<<"x"<<h<<" 4:4:4 JPEG95 bytes="<<data.size()<<", cooperative MCU steps="<<rows<<"\n";}
