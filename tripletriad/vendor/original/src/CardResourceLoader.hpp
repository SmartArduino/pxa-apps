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
#ifndef TTCARDS_CARD_RESOURCE_LOADER_HPP
#define TTCARDS_CARD_RESOURCE_LOADER_HPP

#include <memory>

#include <nomlib/graphics.hpp>

#include "config.hpp"
#include "types.hpp"

namespace tt {

// Forward declarations
class GameConfig;

class CardResourceLoader
{
  public:
    /// \brief Load the resources used in rendering cards from a collection.
    bool init(  const nom::Font& card_font,
                const GameConfig* config,
                const std::string& extra_textures_dir );

    std::unique_ptr<nom::Gradient> card_backgrounds_[PlayerID::TOTAL_PLAYER_IDS];
    std::unique_ptr<nom::SpriteBatch> card_faces_;
    std::unique_ptr<nom::SpriteBatch> card_elements_;
    std::unique_ptr<nom::Text> card_text_;

    std::string extra_textures_dir_;
};

} // namespace tt

#endif // include guard defined
