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
#ifndef GAMEAPP_CARD_HEADERS
#define GAMEAPP_CARD_HEADERS

#include <string>
#include <array>

#include <nomlib/config.hpp>

#include "config.hpp"
#include "types.hpp"

namespace tt {

// Forward declarations
class CardRenderer;

/// \brief Minimum card level.
const nom::uint32 MIN_LEVEL = 1;

/// \brief Maximum card level.
const nom::uint32 MAX_LEVEL = 10;

/// Minimum rank a card can contain
const nom::uint32 MIN_RANK = 1;

/// Maximum rank a card can contain
const nom::uint32 MAX_RANK = 10;

/// \brief Maximum card name length
const nom::uint32 MAX_NAME = 24;

/// \brief The minimum number of cards of a type you can collect.
const nom::uint32 MIN_CARD_NUM = 0;

/// \brief The maximum number of cards of a type you can collect.
const nom::uint32 MAX_CARD_NUM = 99;

struct Card
{
  static Card null;

  /// \brief Default constructor; constructs an invalid Card object.
  Card();

  ~Card();

  /// \see Resources/cards.json
  CardID id;

  nom::uint32 level;

  /// \see tt::CardType enumeration values.
  nom::uint32 type;

  /// \see tt::CardElement enumeration values.
  nom::uint32 element;

  /// \see tt::CardRank
  std::array<nom::uint32, CardRank::TOTAL_RANKS> ranks;

  // The current owner of this card.
  PlayerID player_id;

  // The player whom first owned this card.
  PlayerID player_owner;

  std::string name;

  /// \brief The number of cards of this instance type that has been
  /// collected.
  int num;

  /// \brief Internal state flag for whether or not the card face is to be
  /// shown to the player.
  bool face_down;

  std::shared_ptr<CardRenderer> card_renderer;

  /// \brief The absolute file path to a custom texture.
  ///
  /// \todo Change this to an integer ID and implement a texture lookup table
  /// for texture paths?
  std::string texture_path;
};

nom::Value serialize_card(const Card& card);

Card deserialize_card(const nom::Value& obj);

/// \todo Rename to CardList..?
typedef std::vector<Card> Cards;

/// TODO: Rename to Card::iterator
typedef Cards::iterator CardsIterator;

/// TODO: Rename to Card::const_iterator
typedef Cards::const_iterator ConstCardsIterator;

/// Pretty print the the card attributes.
std::ostream& operator <<(std::ostream& os, const Card& rhs);

/// Compare two cards for equality
bool operator ==(const Card& lhs, const Card& rhs);

/// Compare two cards for less-than equality
bool operator <(const Card& lhs, const Card& rhs);

/// Compare two cards for greater-than equality
bool operator >(const Card& lhs, const Card& rhs);

/// Compare two cards for in-equality
bool operator !=(const Card& lhs, const Card& rhs);

/// Compare two cards for less-than or equal to equality
bool operator <=(const Card& lhs, const Card& rhs);

/// Compare two cards for greater-than or equal to equality
bool operator >=(const Card& lhs, const Card& rhs);

/// The total strength value of any given card is determined by adding the
/// sum of all the card rank values together.
///
/// Note that this does not take into account game rules that may be in
/// effect!
nom::int32 strength(const Card& rhs);

/// \brief Card strength greater-than comparison.
///
/// \see CardHand
bool strongest_card(const Card& lhs, const Card& rhs);

/// \brief Card strength lesser-than comparison.
///
/// \see CardHand
bool weakest_card(const Card& lhs, const Card& rhs);

} // namespace tt

#endif // GAMEAPP_CARD_HEADERS defined
