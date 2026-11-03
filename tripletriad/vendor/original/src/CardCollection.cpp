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
#include "CardCollection.hpp"

// Private headers
#include <nomlib/serializers.hpp>

using namespace nom;

namespace tt {

CardCollection::CardCollection()
{
  NOM_LOG_TRACE ( TTCARDS_LOG_CATEGORY_TRACE );
}

CardCollection::~CardCollection()
{
  NOM_LOG_TRACE ( TTCARDS_LOG_CATEGORY_TRACE );
}

void CardCollection::clear()
{
  this->cards_.clear();
}

nom::size_type CardCollection::size() const
{
  return this->cards_.size();
}

const Card& CardCollection::front() const
{
  if( this->cards_.size() > 0 ) {
    return this->cards_.front();
  } else {
    return Card::null;
  }
}

bool CardCollection::save(const std::string& filename)
{
  nom::Value objects;
  nom::Value card;

  auto fp = nom::make_unique_json_serializer();
  if( fp == nullptr ) {
    NOM_LOG_ERR(  TTCARDS,
                  "Could not load input file: failure to allocate memory!" );
    return false;
  }

  for( auto itr = this->cards_.begin(); itr != this->cards_.end(); ++itr ) {

    card = tt::serialize_card(*itr);
    objects["cards"].push_back(card);
  }

  if( fp->save(objects, filename) == false ) {
    NOM_LOG_ERR(TTCARDS, "Unable to save JSON file: " + filename);
    return false;
  }

  return true;
}

bool CardCollection::load(const std::string& filename)
{
  Card card;
  Cards cards_buffer;
  nom::Value value;

  auto fp = nom::make_unique_json_deserializer();
  if( fp == nullptr ) {
    NOM_LOG_ERR(  TTCARDS,
                  "Could not load input file: failure to allocate memory!" );
    return false;
  }

  if( fp->load(filename, value) == false ) {
    NOM_LOG_ERR(TTCARDS, "Unable to parse JSON input file: " + filename);
    return false;
  }

  nom::Value deck = value["cards"];
  for( auto itr = deck.begin(); itr != deck.end(); ++itr ) {

    nom::Value attr = itr->ref();
    card = tt::deserialize_card(attr);

    // Additional attributes
    card.player_id = PlayerID::PLAYER_ID_INVALID;
    card.player_owner = PlayerID::PLAYER_ID_INVALID;

    cards_buffer.push_back(card);
  }

  // All is well, let us make our freshly loaded data permanent
  this->cards_ = cards_buffer;

  return true;
}

const Card& CardCollection::find(const std::string& card_name) const
{
  for( auto itr = this->cards_.begin(); itr != this->cards_.end(); ++itr ) {

    if( (*itr).name == card_name ) {
      // Successful match
      return *itr;
    }
  }

  // No match
  return Card::null;
}

const Card& CardCollection::find(CardID card_id) const
{
  for( auto itr = this->cards_.begin(); itr != this->cards_.end(); ++itr ) {

    if( (*itr).id == card_id ) {
      // Successful match
      return *itr;
    }
  }

  // No match
  return Card::null;
}

void CardCollection::append_card(const Card& card)
{
  for( auto itr = this->cards_.begin(); itr != this->cards_.end(); ++itr ) {

    // Existing card
    if( *itr == card ) {
      if( (*itr).num < 99 ) {
        (*itr).num = (*itr).num + 1;
      }

      return;
    }
  }

  // Card does not exist, so we add it to the deck
  Card new_card = card;
  new_card.num = 1;
  this->cards_.push_back(new_card);
}

void CardCollection::append_cards(const Cards& cards)
{
  for( auto itr = cards.begin(); itr != cards.end(); ++itr ) {
    this->cards_.push_back(*itr);
  }
}

void CardCollection::erase_card(const Card& card)
{
  for( auto itr = this->cards_.begin(); itr != this->cards_.end(); ++itr ) {

    // Existing card
    if( (*itr) == card ) {

      if( (*itr).num > 0 ) {
        (*itr).num = (*itr).num - 1;
      }

      if( (*itr).num == 0 ) {
        this->cards_.erase(itr);
      }

      return;
    }
  }
}

ConstCardsIterator CardCollection::begin() const
{
  return this->cards_.begin();
}

ConstCardsIterator CardCollection::end() const
{
  return this->cards_.end();
}

CardsIterator CardCollection::begin()
{
  return this->cards_.begin();
}

CardsIterator CardCollection::end()
{
  return this->cards_.end();
}

nom::Value
serialize_deck(const CardCollection* deck)
{
  nom::Value objects(nom::Value::ValueType::Null);

  NOM_ASSERT(deck != nullptr);
  if( deck == nullptr ) {
    return objects;
  }

  auto deck_end = deck->end();
  for( auto itr = deck->begin(); itr != deck_end; ++itr ) {
    // Serialize each card as an object
    objects.push_back( tt::serialize_card(*itr) );
  }

  return objects;
}

tt::Cards
deserialize_deck(const nom::Value& objects)
{
  Card card;
  Cards cards;

  // Reconstruct hand data
  auto object_end = objects.end();
  for( auto itr = objects.begin(); itr != object_end; ++itr ) {

    card = tt::deserialize_card(*itr);
    cards.push_back(card);
  }

  return cards;
}

} // namespace tt
