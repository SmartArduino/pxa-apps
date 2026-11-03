/* SPDX-License-Identifier: MIT
 * Geometry preserves the documented nomlib negative default/null semantics.
 * References: i8degrees/nomlib include/nomlib/math/{Point2,Rect}.hpp.
 */
#pragma once
#include "config.hpp"
namespace nom {
template<class T> struct Point2 {
 T x,y;
 constexpr Point2():x(-1),y(-1){}
 constexpr Point2(T a,T b):x(a),y(b){}
 static const Point2 zero,null;
};
template<class T> inline const Point2<T> Point2<T>::zero{0,0};
template<class T> inline const Point2<T> Point2<T>::null{-1,-1};
template<class T> inline bool operator==(const Point2<T>&a,const Point2<T>&b){return a.x==b.x&&a.y==b.y;}
template<class T> inline bool operator!=(const Point2<T>&a,const Point2<T>&b){return !(a==b);}
template<class T> inline std::ostream& operator<<(std::ostream&o,const Point2<T>&p){return o<<p.x<<", "<<p.y;}
using Point2i=Point2<int32>;
template<class T> struct Size2 {
 T w,h;
 constexpr Size2():w(-1),h(-1){}
 constexpr Size2(T a,T b):w(a),h(b){}
};
using Size2i=Size2<int32>;
template<class T> struct Rect {
 T x,y,w,h;
 constexpr Rect():x(-1),y(-1),w(-1),h(-1){}
 constexpr Rect(T a,T b,T c,T d):x(a),y(b),w(c),h(d){}
 Point2<T> position()const{return {x,y};}
 Size2<T> size()const{return {w,h};}
 static const Rect null,zero;
};
template<class T> inline const Rect<T> Rect<T>::null{-1,-1,-1,-1};
template<class T> inline const Rect<T> Rect<T>::zero{0,0,0,0};
template<class T> inline bool operator==(const Rect<T>&a,const Rect<T>&b){return a.x==b.x&&a.y==b.y&&a.w==b.w&&a.h==b.h;}
template<class T> inline bool operator!=(const Rect<T>&a,const Rect<T>&b){return !(a==b);}
template<class T> inline std::ostream& operator<<(std::ostream&o,const Rect<T>&r){return o<<r.x<<", "<<r.y<<", "<<r.w<<", "<<r.h;}
using IntRect=Rect<int32>;
template<class T> inline T minimum(T a,T b){return a<b?a:b;}
template<class T> inline T maximum(T a,T b){return a>b?a:b;}
inline uint32 random_state=0x54c4a19du;
inline void seed_random(uint32 seed){random_state=seed?seed:0x54c4a19du;}
inline uint32 next_random(){uint32 n=random_state;n^=n<<13;n^=n>>17;n^=n<<5;return random_state=n;}
template<class T> inline T uniform_int_rand(T low,T high){
 assert(high>=low);
 const uint32 bound=static_cast<uint32>(high-low)+1u;
 if(bound==0)return static_cast<T>(next_random());
 const uint32 threshold=(0u-bound)%bound;
 uint32 n;do{n=next_random();}while(n<threshold);
 return static_cast<T>(low+static_cast<T>(n%bound));
}
struct Color4i {
 int32 r,g,b,a;
 constexpr Color4i(int32 red=0,int32 green=0,int32 blue=0,int32 alpha=255):r(red),g(green),b(blue),a(alpha){}
};
using Color4iColors=std::array<Color4i,2>;
}

