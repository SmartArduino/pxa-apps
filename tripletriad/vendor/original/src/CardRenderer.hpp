/******************************************************************************

  TTcards - Triple Triad remake

Copyright (c) 2013, 2014 Jeffrey Carpenter <i8degrees@gmail.com>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/
#ifndef TTCARDS_CARD_RENDERER_HPP
#define TTCARDS_CARD_RENDERER_HPP

#include <memory>

#include <nomlib/graphics.hpp>

#include "config.hpp"

namespace tt {

// Forward declarations
struct Card;
class CardResourceLoader;

class CardRenderer
{
  public:
    // TODO (?): Should we pre-render an invalid and/or face down sprite for
    // every card rendering instance, or keep with rendering on the fly?
    // static std::unique_ptr<nom::Sprite> INVALID_CARD;
    // static std::unique_ptr<nom::Sprite> FACE_DOWN_CARD;

    // ...Card::null && face down cards...
    //
    // TODO: Consider using another ID for the face down ID! We currently pass
    // Card::null (BAD_CARD_ID) to signify to the renderer to draw the face
    // down sprite for us -- I'm worried about future usage conflicts; i.e.:
    // what if we wish to start rendering Card::null with a debug rendering
    // texture?

    // TODO: Object-pooling?

    CardRenderer();
    ~CardRenderer();

    CardRenderer(std::unique_ptr<nom::Sprite> sprite);

    bool valid() const;

    std::unique_ptr<nom::Sprite> rendered_card();

    const nom::Point2i& position() const;
    const nom::Size2i& size() const;

    void set_position(const nom::Point2i& pos);
    void set_size(const nom::Size2i& dims);

    void set_rendered_card(std::unique_ptr<nom::Sprite> sprite);

    bool render(nom::RenderTarget& target) const;

  private:
    std::unique_ptr<nom::Sprite> rendered_card_;
    nom::Point2i position_;
    nom::Size2i size_;
};

CardRenderer* create_placeholder_card_renderer();

CardRenderer*
create_card_renderer(const CardResourceLoader* res, const Card& card);

} // namespace tt

#endif // include guard defined
