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
#ifndef GAMEAPP_DIALOG_CURSOR_HPP
#define GAMEAPP_DIALOG_CURSOR_HPP

#include <string>
#include <vector>

#include <nomlib/graphics.hpp>
#include <nomlib/math.hpp>
#include <nomlib/system.hpp>

#include "config.hpp"

namespace tt {

class DialogCursor: public nom::SpriteBatch
{
  public:
    DialogCursor();
    virtual ~DialogCursor();

    /// \brief Set a positioning map for this game cursor to use.
    void set_position_map( const std::vector<nom::IntRect>& map );

    /// \brief Get the current positioning index, relative to the element index
    /// order of the positioning map.
    int cursor_position();

    /// \brief Get the beginning element position index for the map.
    int first();

    /// \brief Get the last element position index for the map.
    int last();

    /// \brief Move the game cursor to the previous mapped item.
    ///
    /// \remarks This method updates the internal rendering coordinates of the
    /// sprite.
    bool prev();

    /// \brief Move the game cursor to the next mapped item.
    ///
    /// \remarks This method updates the internal rendering coordinates of the
    /// sprite.
    bool next();

    /// \brief Set the position of the game cursor.
    ///
    /// \param pos The position index of the game cursor, relative to the
    /// element index order of the positioning map.
    void set_cursor_position(int pos);

  private:
    std::vector<nom::IntRect> position_map_;
    int cursor_position_;
};

} // namespace tt

#endif // include guard defined
