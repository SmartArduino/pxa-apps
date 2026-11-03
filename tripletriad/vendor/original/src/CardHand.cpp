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
#include "CardHand.hpp"

// Private headers
#include "helpers.hpp"
#include "CardRenderer.hpp"

#include <nomlib/serializers.hpp>

// Forward declarations
#include "CardCollection.hpp"
#include "CardResourceLoader.hpp"

using namespace nom;

namespace tt {

CardHand::CardHand()
{
  NOM_LOG_TRACE(TTCARDS_LOG_CATEGORY_TRACE);

  this->set_position(0);
  this->selectedCard = Card();
}

CardHand::~CardHand()
{
  NOM_LOG_TRACE(TTCARDS_LOG_CATEGORY_TRACE);
}

bool CardHand::init(CardResourceLoader* res, PlayerIndex pid)
{
  this->cards.reserve(MAX_PLAYER_HAND);

  this->card_res_ = res;
  this->player_index_ = pid;

  NOM_ASSERT(this->card_res_ != nullptr);
  if( this->card_res_ != nullptr) {
    return true;
  }

  return false;
}

bool CardHand::update()
{
  NOM_ASSERT(this->card_res_ != nullptr);
  if( this->card_res_ == nullptr) {
    // TODO: logging && err handling
    return false;
  }

  for( auto itr = this->cards.begin(); itr != this->cards.end(); ++itr ) {

    Card& pcard = *itr;

    auto renderer = tt::create_card_renderer(this->card_res_, pcard);
    pcard.card_renderer.reset(renderer);

    NOM_ASSERT(pcard.card_renderer != nullptr);
    NOM_ASSERT(pcard.card_renderer->valid() == true);
    if( pcard.card_renderer == nullptr ) {
      // TODO: logging && err handling
      return false;
    }

    if( pcard.card_renderer->valid() == false ) {
      // TODO: logging && err handling
      return false;
    }
  } // end for loop

  // Reset the player's selected card to the top
  this->set_position(0);

  return true;
}

bool CardHand::push_back(const Card& card)
{
  Card pcard = card;

  // No go -- we are out of space!
  if( this->size() > (MAX_PLAYER_HAND - 1) ) {
    return false;
  }

  // IMPORTANT: To minimize rendering updates, the card's initial attributes
  // should be set before the rendering of the hand occurs
  tt::set_card_id(pcard, this->player_id() );

  if( pcard.face_down == true ) {
    // No card face
    auto renderer = tt::create_card_renderer(this->card_res_, Card::null);
    pcard.card_renderer.reset(renderer);
  } else {
    auto renderer = tt::create_card_renderer(this->card_res_, pcard);
    pcard.card_renderer.reset(renderer);
  }

  NOM_ASSERT(pcard.card_renderer != nullptr);
  if( pcard.card_renderer == nullptr ) {
    // TODO: logging && handle err
    return false;
  }

  NOM_ASSERT(pcard.card_renderer->valid() == true);
  if( pcard.card_renderer->valid() == false ) {
    // TODO: logging && handle err
    return false;
  }

  this->cards.push_back(pcard);

  // Reset the player's selected card to the top
  this->set_position(0);

  return true;
}

bool CardHand::push_back(const Cards& cards)
{
  if( cards.size() < 1 ) {
    // No cards to add
    return false;
  }

  for( auto itr = cards.begin(); itr != cards.end(); ++itr ) {
    this->push_back(*itr);
  }

  // Success!
  return true;
}

bool CardHand::erase(const Card& card)
{
  int32 position = 0;
  CardID previous_id = 0;
  std::string previous_name;

  position = this->at(card);

  if( position == -1 ) {
    return false;
  }

  previous_id = this->cards[position].id;
  previous_name = this->cards[position].name;

  auto itr = this->cards.begin() + position;
  this->cards.erase(itr);

  this->front();

  return true;
}

void CardHand::clearSelectedCard()
{
  this->selectedCard = Card();
}

const Card& CardHand::getSelectedCard()
{
  return this->selectedCard;
}

void CardHand::selectCard(const Card& card)
{
  if ( this->exists ( card ) )
  {
    this->selectedCard = card;
  }
}

bool CardHand::empty() const
{
  return this->cards.empty();
}

nom::uint32 CardHand::size() const
{
  return this->cards.size();
}

nom::int32 CardHand::at(const Card& card)
{
  nom::int32 pos = -1;

  if( this->size() < 1 ) {
    return pos;
  }

  for( nom::uint32 idx = 0; idx < this->size() && pos == -1; idx++ ) {

    if( this->cards[idx] == card ) {
      pos = idx;
      break;
    }
  }

  return pos;
}

void CardHand::front()
{
  this->set_position(0);
}

nom::size_type CardHand::position()
{
  return this->position_;
}

void CardHand::next()
{
  nom::size_type pos = this->position();

  if( pos >= this->cards.size() ) {
    this->set_position(0);
  }
  else {
    pos = pos + 1;
    this->set_position(pos);
  }
}

void CardHand::previous()
{
  nom::size_type pos = this->position();

  if( pos >= this->cards.size() || pos == 0) {
    this->set_position(0);
  }
  else {
    pos = pos - 1;
    this->set_position(pos);
  }
}

void CardHand::clear()
{
  this->cards.clear();
  this->clearSelectedCard();
}

bool CardHand::exists(const Card& card) const
{
  for( nom::uint32 idx = 0; idx < this->size(); idx++ ) {

    if( card == this->cards[idx] ) {
      // Matched
      return true;
    }
  }

  // Not found
  return false;
}

void
CardHand::add_random_card(  nom::uint32 min_level, nom::uint32 max_level,
                            const CardCollection* db )
{
  nom::size_type num_cards = 0;
  int32 random_card_id = 0;
  Card card(Card::null);

  if( db == nullptr ) {
    return;
  }

  // ...Pick a card at random
  num_cards = (db->size() - 1);
  random_card_id = nom::uniform_int_rand<nom::uint32>(0, num_cards);

  card = db->find(random_card_id);
  if( card != Card::null && card.num > 0 && card.level <= max_level &&
      card.level >= min_level )
  {
    this->push_back(card);
  }
}

Card CardHand::strongest()
{
  Cards strongest_cards(this->cards);

  std::sort(strongest_cards.begin(), strongest_cards.end(), strongest_card);

  return strongest_cards.front();
}

Card CardHand::weakest()
{
  Cards weakest_cards(this->cards);

  std::sort(weakest_cards.begin(), weakest_cards.end(), weakest_card);

  return weakest_cards.front();
}

void CardHand::set_position(nom::size_type pos)
{
  NOM_ASSERT( pos <= this->size() );

  this->position_ = pos;

  if( this->size() > 0 && pos < this->size() ) {
    this->selectedCard = this->cards[pos];
  }
}

CardsIterator CardHand::begin()
{
  return this->cards.begin();
}

CardsIterator CardHand::end()
{
  return this->cards.end();
}

ConstCardsIterator CardHand::begin() const
{
  return this->cards.begin();
}

ConstCardsIterator CardHand::end() const
{
  return this->cards.end();
}

PlayerID CardHand::player_id() const
{
  auto id = tt::player_id(this->player_index_);

  return id;
}

PlayerIndex CardHand::player_index() const
{
  return this->player_index_;
}

std::ostream& operator <<(std::ostream& os, const CardHand& rhs)
{
  os << "\n" << rhs.size() << " cards" << "\n";

  for( auto itr = rhs.begin(); itr != rhs.end(); ++itr ) {

    os << *itr;
    os << "\n\n";
  }

  return os;
}

nom::Value
serialize_hand(const CardHand* phand)
{
  nom::Value objects(nom::Value::ValueType::Null);

  NOM_ASSERT(phand != nullptr);
  if( phand == nullptr ) {
    return objects;
  }

  for( auto itr = phand->cards.begin(); itr != phand->cards.end(); ++itr ) {
    // Serialize each card as an object
    objects.push_back( tt::serialize_card(*itr) );
  }

  return objects;
}

tt::Cards
deserialize_hand(PlayerID player_id, const nom::Value& objects)
{
  Card card;
  Cards cards;

  // Reconstruct hand data
  for( auto itr = objects.begin(); itr != objects.end(); ++itr ) {

    card = tt::deserialize_card(*itr);

    // Set rendering color && ownership info
    tt::set_card_id(card, player_id);

    cards.push_back(card);
  }

  return cards;
}

} // namespace tt
