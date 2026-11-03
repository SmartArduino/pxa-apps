/* SPDX-License-Identifier: MIT
 * Core-only render handle. It never submits or draws graphics.
 * Retains original model/hand renderer ownership without importing SDL.
 */
#pragma once
#include <nomlib/math.hpp>
namespace tt {
struct Card;class CardResourceLoader;
class CardRenderer {
 nom::Point2i position_{nom::Point2i::zero};
 public:
 bool valid()const{return true;}
 const nom::Point2i& position()const{return position_;}
 void set_position(const nom::Point2i& pos){position_=pos;}
 bool render(nom::RenderTarget&)const{return true;}
};
inline CardRenderer* create_card_renderer(const CardResourceLoader*,const Card&){return new CardRenderer;}
inline CardRenderer* create_placeholder_card_renderer(){return new CardRenderer;}
}

