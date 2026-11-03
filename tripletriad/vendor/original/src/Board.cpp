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
#include "Board.hpp"

// Private helpers
#include "helpers.hpp"

#include <nomlib/serializers.hpp>

// Forward declarations
#include "CardRules.hpp"
#include "CardResourceLoader.hpp"
#include "CardRenderer.hpp"

using namespace nom;

namespace tt {

Board::Board() :
  rules_(nullptr),
  card_res_(nullptr)
{
  NOM_LOG_TRACE(TTCARDS_LOG_CATEGORY_TRACE);
}

Board::~Board()
{
  NOM_LOG_TRACE(TTCARDS_LOG_CATEGORY_TRACE);
}

bool
Board::initialize(tt::RegionRuleSet* ruleset, CardResourceLoader* res)
{
  NOM_LOG_TRACE(TTCARDS_LOG_CATEGORY_TRACE);

  this->rules_ = ruleset;
  this->card_res_ = res;

  if( this->rules_ == nullptr || this->card_res_ == nullptr ) {
    return false;
  }

  // Calculate the global screen coordinates for local positions on the board;
  // these are the rectangle bounds starting from the top-left
  this->board_map_[0] =
    nom::IntRect( 0, 0,
                  BOARD_ORIGIN_X + ( CARD_WIDTH * 0),
                  BOARD_ORIGIN_Y + ( CARD_HEIGHT * 0) );
  this->board_map_[1] =
    nom::IntRect( 1, 0,
                  BOARD_ORIGIN_X + ( CARD_WIDTH * 1),
                  BOARD_ORIGIN_Y + ( CARD_HEIGHT * 0) );
  this->board_map_[2] =
    nom::IntRect( 2, 0,
                  BOARD_ORIGIN_X + ( CARD_WIDTH * 2),
                  BOARD_ORIGIN_Y + ( CARD_HEIGHT * 0) );
  this->board_map_[3] =
    nom::IntRect( 0, 1,
                  BOARD_ORIGIN_X + ( CARD_WIDTH * 0),
                  BOARD_ORIGIN_Y + ( CARD_HEIGHT * 1) );
  this->board_map_[4] =
    nom::IntRect( 1, 1,
                  BOARD_ORIGIN_X + ( CARD_WIDTH * 1),
                  BOARD_ORIGIN_Y + ( CARD_HEIGHT * 1) );
  this->board_map_[5] =
    nom::IntRect( 2, 1,
                  BOARD_ORIGIN_X + ( CARD_WIDTH * 2),
                  BOARD_ORIGIN_Y + ( CARD_HEIGHT * 1) );
  this->board_map_[6] =
    nom::IntRect( 0, 2,
                  BOARD_ORIGIN_X + ( CARD_WIDTH * 0),
                  BOARD_ORIGIN_Y + ( CARD_HEIGHT * 2) );
  this->board_map_[7] =
    nom::IntRect( 1, 2,
                  BOARD_ORIGIN_X + ( CARD_WIDTH * 1),
                  BOARD_ORIGIN_Y + ( CARD_HEIGHT * 2) );
  this->board_map_[8] =
    nom::IntRect( 2, 2,
                  BOARD_ORIGIN_X + ( CARD_WIDTH * 2),
                  BOARD_ORIGIN_Y + ( CARD_HEIGHT * 2) );

  // Calculate the global screen coordinates for local positions on the board
  // for comparing against mouse coordinates; for ease of collision bounds
  // checking, these coordinates are the complete bounds of each position
  this->board_mouse_map_[0] =
    nom::IntRect( 0, 0,
                  BOARD_ORIGIN_X + ( CARD_WIDTH * 1),
                  BOARD_ORIGIN_Y + ( CARD_HEIGHT * 1) );
  this->board_mouse_map_[1] =
    nom::IntRect( 1, 0,
                  BOARD_ORIGIN_X + ( CARD_WIDTH * 2),
                  BOARD_ORIGIN_Y + ( CARD_HEIGHT * 1) );
  this->board_mouse_map_[2] =
    nom::IntRect( 2, 0,
                  BOARD_ORIGIN_X + ( CARD_WIDTH * 3),
                  BOARD_ORIGIN_Y + ( CARD_HEIGHT * 1) );
  this->board_mouse_map_[3] =
    nom::IntRect( 0, 1,
                  BOARD_ORIGIN_X + ( CARD_WIDTH * 1),
                  BOARD_ORIGIN_Y + ( CARD_HEIGHT * 2) );
  this->board_mouse_map_[4] =
    nom::IntRect( 1, 1,
                  BOARD_ORIGIN_X + ( CARD_WIDTH * 2),
                  BOARD_ORIGIN_Y + ( CARD_HEIGHT * 2) );
  this->board_mouse_map_[5] =
    nom::IntRect( 2, 1,
                  BOARD_ORIGIN_X + ( CARD_WIDTH * 3),
                  BOARD_ORIGIN_Y + ( CARD_HEIGHT * 2) );
  this->board_mouse_map_[6] =
    nom::IntRect( 0, 2,
                  BOARD_ORIGIN_X + ( CARD_WIDTH * 1),
                  BOARD_ORIGIN_Y + ( CARD_HEIGHT * 3) );
  this->board_mouse_map_[7] =
    nom::IntRect( 1, 2,
                  BOARD_ORIGIN_X + ( CARD_WIDTH * 2),
                  BOARD_ORIGIN_Y + ( CARD_HEIGHT * 3) );
  this->board_mouse_map_[8] =
    nom::IntRect( 2, 2,
                 BOARD_ORIGIN_X + ( CARD_WIDTH * 3),
                 BOARD_ORIGIN_Y + ( CARD_HEIGHT * 3) );

  this->clear();

  bool elemental_ruleset =
    tt::is_card_rule_set(this->rules_, CardRuleset::ELEMENTAL_RULESET);
  if( elemental_ruleset == true ) {
    this->initialize_board_elements();
  }

  return true;
}

void Board::clear()
{
  this->grid.clear();

  // Bizarre things happen here if we do not do this -- it appears as though we
  // overwrite existing memory ... I'd really like to better understand why this
  // is!
  //
  // Call stack:
  //
  // Exception Type:        EXC_BAD_ACCESS (SIGSEGV)
  // Exception Codes:       KERN_INVALID_ADDRESS at 0x0000000000000030
  // Thread 0 Crashed:: Dispatch queue: com.apple.main-thread
  // 0   libc++.1.dylib                  0x00007fff849f3892 std::__1::__shared_weak_count::__release_shared() + 24
  // 1   org.i8degrees.ttcards           0x0000000103039d5f std::__1::shared_ptr<CardRenderer>::~shared_ptr() + 47 (memory:4449)
  // 2   org.i8degrees.ttcards           0x0000000103037185 std::__1::shared_ptr<CardRenderer>::~shared_ptr() + 21 (memory:4449)
  // 3   org.i8degrees.ttcards           0x0000000103040151 Card::operator=(Card const&) + 529 (Card.hpp:78)
  // 4   org.i8degrees.ttcards           0x000000010303ea8d BoardTile::set_tile(Card const&) + 29 (BoardTile.cpp:97)
  // 5   org.i8degrees.ttcards           0x000000010303263c Board::update(nom::Point2<int> const&, Card&) + 2908 (Board.cpp:590)
  // 6   org.i8degrees.ttcards           0x00000001030345f4 Board::load(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&) + 3396 (Board.cpp:776)
  this->grid.resize(BOARD_GRID_HEIGHT);

  for( auto x = 0; x < BOARD_GRID_WIDTH; ++x ) {
    this->grid[x].resize(BOARD_GRID_WIDTH);
  }

  // Initial board values
  auto idx = 0;
  for( auto y = 0; y < BOARD_GRID_HEIGHT; y++ ) {
    for( auto x = 0; x < BOARD_GRID_WIDTH; x++ ) {
      this->grid[x][y].set_tile(Card::null);
      this->grid[x][y].set_bounds(this->board_map_[idx]);
      this->grid[x][y].set_element(0);
      ++idx;
    } // end for x loop
  } // end for y loop
}

const nom::IntRect Board::getGlobalBounds ( nom::int32 x, nom::int32 y ) const
{
  for ( nom::int32 idx = 0; idx < ( BOARD_GRID_WIDTH * BOARD_GRID_HEIGHT ); idx++ )
  {
    if ( x <= this->board_mouse_map_[idx].w && x >= BOARD_ORIGIN_X && y <= this->board_mouse_map_[idx].h && y >= BOARD_ORIGIN_Y )
      return this->board_mouse_map_[idx];
  }

  return nom::IntRect::null;
}

board_tiles Board::find_adjacent(nom::int32 x, nom::int32 y) const
{
  board_tiles adjacent_cards;

  nom::int32 cols = y;
  nom::int32 rows = x;

  // Compare card's WEST rank with opponent's EAST rank
  if ( rows != 0 )
  {
    if ( this->getStatus ( rows - 1, cols ) != BAD_CARD_ID )
    {
      adjacent_cards.push_back ( this->tile ( rows - 1, cols ) );
    }
  }

  // Compare card's EAST rank with opponent's WEST rank
  if ( rows != BOARD_GRID_WIDTH - 1 )
  {
    if ( this->getStatus ( rows + 1, cols ) != BAD_CARD_ID )
    {
      adjacent_cards.push_back ( this->tile ( rows + 1, cols ) );
    }
  }

  // Compare card's NORTH rank with opponent's SOUTH rank
  if ( cols != 0 )
  {
    if ( this->getStatus ( rows, cols - 1 ) != BAD_CARD_ID )
    {
      adjacent_cards.push_back ( this->tile ( rows, cols - 1 ) );
    }
  }

  // Compare card's SOUTH rank with opponent's NORTH rank
  if ( cols != BOARD_GRID_HEIGHT - 1 )
  {
    if ( this->getStatus ( rows, cols + 1 ) != BAD_CARD_ID )
    {
      adjacent_cards.push_back ( this->tile ( rows, cols + 1 ) );
    }
  }

  return std::move(adjacent_cards);
}

board_tiles_result Board::check_board(const nom::Point2i& rel_board_pos)
{
  int x = rel_board_pos.x;
  int y = rel_board_pos.y;
  nom::int32 cols, rows = 0; // iterator
  nom::int32 same_count = 0;
  BoardResult result;
  board_tiles_result coords;

  bool same_rule_applied =
    tt::is_card_rule_set(this->rules_, CardRuleset::SAME_RULESET);

  coords.clear(); // initialize a fresh new coords list

  for( cols = y; y < BOARD_GRID_HEIGHT; y++ ) {
    for( rows = x; x < BOARD_GRID_WIDTH; x++ ) {

      // Compare card's WEST rank with opponent's EAST rank
      if( rows != 0 ) {

        if( this->getPlayerID(rows, cols) != this->getPlayerID(rows - 1, cols) &&
            this->getStatus(rows - 1, cols) != BAD_CARD_ID )
        {
          if( (same_rule_applied == true) &&
              this->grid[rows][cols].tile_card.ranks[RANK_WEST] ==
              this->grid[rows - 1][cols].tile_card.ranks[RANK_EAST] )
          {
            same_count += 1;
            if( same_count < 2 && coords.size() < 2 ) {
              result.tile = this->grid[rows - 1][cols];
              result.applied_ruleset = CardRuleset::SAME_RULESET;
              coords.push_back(result);
            }
          }

          if( this->grid[rows][cols].tile_card.ranks[RANK_WEST] >
              this->grid[rows - 1][cols].tile_card.ranks[RANK_EAST] == true )
          {
            result.tile = this->grid[rows - 1][cols];
            result.applied_ruleset = CardRuleset::NO_RULESET;
            coords.push_back(result);
          }
        }
      }

      // Compare card's SOUTH rank with opponent's NORTH rank
      if( cols != BOARD_GRID_HEIGHT - 1 ) {

        if( this->getPlayerID(rows, cols) != this->getPlayerID(rows, cols + 1) &&
            this->getStatus(rows, cols + 1) != BAD_CARD_ID )
        {
          if( (same_rule_applied == true) &&
              this->grid[rows][cols].tile_card.ranks[RANK_SOUTH] ==
              this->grid[rows][cols + 1].tile_card.ranks[RANK_NORTH] )
          {
            same_count += 1;
            if( same_count < 2 && coords.size() < 2 ) {
              result.tile = this->grid[rows][cols + 1];
              result.applied_ruleset = CardRuleset::SAME_RULESET;
              coords.push_back(result);
            }
          }

          if( this->grid[rows][cols].tile_card.ranks[RANK_SOUTH] >
              this->grid[rows][cols + 1].tile_card.ranks[RANK_NORTH] == true )
          {
            result.tile = this->grid[rows][cols + 1];
            result.applied_ruleset = CardRuleset::NO_RULESET;
            coords.push_back(result);
          }
        }
      }

      // Compare card's EAST rank with opponent's WEST rank
      if( rows != BOARD_GRID_WIDTH - 1 ) {

        if( this->getPlayerID(rows, cols) != this->getPlayerID(rows + 1, cols) &&
            this->getStatus(rows + 1, cols) != BAD_CARD_ID )
        {
          if( (same_rule_applied == true) &&
              this->grid[rows][cols].tile_card.ranks[RANK_EAST] ==
              this->grid[rows + 1][cols].tile_card.ranks[RANK_WEST] )
          {
            same_count += 1;
            if( same_count < 2 && coords.size() < 2 ) {
              result.tile = this->grid[rows + 1][cols];
              result.applied_ruleset = CardRuleset::SAME_RULESET;
              coords.push_back(result);
            }
          }

          if( this->grid[rows][cols].tile_card.ranks[RANK_EAST] >
              this->grid[rows + 1][cols].tile_card.ranks[RANK_WEST] == true )
          {
            result.tile = this->grid[rows + 1][cols];
            result.applied_ruleset = CardRuleset::NO_RULESET;
            coords.push_back(result);
          }
        }
      }

      // Compare card's NORTH rank with opponent's SOUTH rank
      if( cols != 0 ) {

        if( this->getPlayerID(rows, cols) != this->getPlayerID(rows, cols - 1) &&
            this->getStatus(rows, cols - 1) != BAD_CARD_ID )
        {
          if( (same_rule_applied == true) &&
              this->grid[rows][cols].tile_card.ranks[RANK_NORTH] ==
              this->grid[rows][cols - 1].tile_card.ranks[RANK_SOUTH] )
          {
            same_count += 1;
            if( same_count < 2 && coords.size() < 2 ) {
              result.tile = this->grid[rows][cols - 1];
              result.applied_ruleset = CardRuleset::SAME_RULESET;
              coords.push_back(result);
            }
          }

          if( this->grid[rows][cols].tile_card.ranks[RANK_NORTH] >
              this->grid[rows][cols - 1].tile_card.ranks[RANK_SOUTH] == true )
          {
            result.tile = this->grid[rows][cols - 1];
            result.applied_ruleset = CardRuleset::NO_RULESET;
            coords.push_back(result);
          }
        }
      }

    } // rows for loop
  } // cols for loop

  return std::move(coords);
}

nom::uint32 Board::getCount ( void )
{
  nom::uint32 total_count = 0;

  for ( nom::int32 y = 0; y < BOARD_GRID_HEIGHT; y++ )
  {
    for ( nom::int32 x = 0; x < BOARD_GRID_WIDTH; x++ )
    {
      if ( this->getStatus ( x, y ) != BAD_CARD_ID )
      {
        total_count += 1;
      }
    }
  }
  return total_count;
}

nom::uint32 Board::getPlayerCount(PlayerID player_id)
{
  nom::uint32 pid_count = 0;

  for ( nom::int32 y = 0; y < BOARD_GRID_HEIGHT; y++ )
  {
    for ( nom::int32 x = 0; x < BOARD_GRID_WIDTH; x++ )
    {
      if ( this->getPlayerID ( x, y ) != 0 && this->getPlayerID ( x, y ) == player_id )
      {
        pid_count += 1;
      }
    }
  }
  return pid_count;
}

CardID Board::getStatus ( nom::int32 x, nom::int32 y ) const
{
  if ( ( x >= 0 && x <= BOARD_GRID_WIDTH ) && ( y >= 0 && y <= BOARD_GRID_HEIGHT ) )
  {
    return this->grid[x][y].tile_card.id;
  }

  return BAD_CARD_ID;
}

CardID Board::status(const nom::Point2i& rel_board_pos) const
{
  return this->getStatus(rel_board_pos.x, rel_board_pos.y);
}

void Board::update(const nom::Point2i& grid_pos, Card& pcard)
{
  Point2i board_pos(Point2i::zero);

  if( this->grid[grid_pos.x][grid_pos.y].element() != 0 ) {

    // ...The region elemental rule-set is in effect...

    if( pcard.element == this->grid[grid_pos.x][grid_pos.y].element() ) {
      tt::increase_card_rank(RANK_NORTH, pcard);
      tt::increase_card_rank(RANK_EAST, pcard);
      tt::increase_card_rank(RANK_SOUTH, pcard);
      tt::increase_card_rank(RANK_WEST, pcard);
    } else {
      tt::decrease_card_rank(RANK_NORTH, pcard);
      tt::decrease_card_rank(RANK_EAST, pcard);
      tt::decrease_card_rank(RANK_SOUTH, pcard);
      tt::decrease_card_rank(RANK_WEST, pcard);
    }
  }

  // Move the rendering of the player's card to the board
  auto old_renderer =
    pcard.card_renderer;
  if( old_renderer == nullptr || old_renderer->valid() == false ) {

    // Always render player cards on the board face up!
    pcard.face_down = false;
    auto renderer = tt::create_card_renderer(this->card_res_, pcard);
    pcard.card_renderer.reset(renderer);
  }

  auto new_renderer =
    pcard.card_renderer;
  if( new_renderer != nullptr ) {

    // Move the player's card position to the applicable board position
    IntRect screen_bounds =
      this->grid[grid_pos.x][grid_pos.y].bounds();
    board_pos.x = screen_bounds.w;
    board_pos.y = screen_bounds.h;
    new_renderer->set_position(board_pos);
  } else {
    pcard.card_renderer.reset( tt::create_placeholder_card_renderer() );
    NOM_ASSERT(pcard.card_renderer == nullptr);
    NOM_ASSERT(pcard.card_renderer->valid() != false);
  }

  this->grid[grid_pos.x][grid_pos.y].set_tile(pcard);
}

bool Board::update(const Cards& cards)
{
  nom::size_type num_cards = cards.size();
  Card card;

  if( num_cards < 1 ) {
    // No cards to use to update the game board
    return false;
  }

  nom::size_type card_index = 0;
  for( auto y = 0; y < BOARD_GRID_HEIGHT; ++y ) {
    for( auto x = 0; x < BOARD_GRID_HEIGHT; ++x ) {

      if( card_index < num_cards ) {
        card = cards[card_index];
        this->update( Point2i(x, y), card );
      }

      ++card_index;
    }
  }

  // Success!
  return true;
}

PlayerID Board::getPlayerID(nom::int32 x, nom::int32 y) const
{
  return this->grid[x][y].tile_card.player_id;
}

void Board::flip_card(const nom::Point2i& rel_board_pos, PlayerID player_id)
{
  int x = rel_board_pos.x;
  int y = rel_board_pos.y;
  Point2i board_pos(Point2i::zero);

  Card& pcard = this->grid[x][y].tile_card;

  pcard.player_id = player_id;

  // Render a new card background based on the new owner
  auto card_renderer =
    tt::create_card_renderer(this->card_res_, pcard);
  pcard.card_renderer.reset(card_renderer);
  NOM_ASSERT(pcard.card_renderer != nullptr);
  NOM_ASSERT(pcard.card_renderer->valid() == true);

  // Move the player's card position to the applicable board position
  IntRect screen_bounds =
    this->grid[x][y].bounds();
  board_pos.x = screen_bounds.w;
  board_pos.y = screen_bounds.h;
  pcard.card_renderer->set_position(board_pos);
}

const Card& Board::get(nom::int32 x, nom::int32 y) const
{
  return this->grid[x][y].tile();
}

const BoardTile& Board::tile(nom::int32 x, nom::int32 y) const
{
  return this->grid[x][y];
}

void Board::draw ( nom::IDrawable::RenderTarget& target )
{
  for( auto y = 0; y != BOARD_GRID_HEIGHT; ++y ) {
    for( auto x = 0; x != BOARD_GRID_WIDTH; ++x ) {

      Card card = this->get(x, y);

      if( card.id != BAD_CARD_ID ) {
        auto card_renderer =
          card.card_renderer;
        if( card_renderer != nullptr && card_renderer->valid() == true ) {
          card_renderer->render(target);
        }
      }

      BoardTile tile = this->tile(x, y);

      // Only render the elemental when the space is unoccupied
      if( tile.element() != 0 && card.id == BAD_CARD_ID ) {

        nom::Point2i element_pos, board_pos;

        // local board position
        board_pos.x = tile.bounds().w;
        board_pos.y = tile.bounds().h;

        element_pos.x =
          board_pos.x + (CARD_WIDTH - ELEMENT_WIDTH) / 2;

        element_pos.y =
          board_pos.y + (CARD_HEIGHT - ELEMENT_WIDTH) / 2;

        // TODO: Update the element position only when we need to -- this will
        // help ease further integration of animations
        tt::render_card_element(  tile.element(), element_pos,
                                  this->card_res_->card_elements_.get(),
                                  target );
      }

    } // end for loop rows
  } // end for loop cols
}

board_tiles Board::free_tiles() const
{
  board_tiles out;
  int tile_status;

  for( int y = 0; y != BOARD_GRID_HEIGHT; y++ ) {
    for( int x = 0; x != BOARD_GRID_WIDTH; x++ ) {
      tile_status = this->getStatus(x, y);

      // Available tile
      if( tile_status == BAD_CARD_ID ) {
        IntRect tile_bounds( IntRect(x, y, CARD_WIDTH, CARD_HEIGHT) );

        out.push_back( BoardTile(Card::null, tile_bounds, -1) );
      }
    }
  }

  return std::move(out);
}

void Board::dump_values()
{
  nom::uint32 line_number = 1;

  for ( nom::int32 y = 0; y < BOARD_GRID_HEIGHT; y++ )
  {
    for ( nom::int32 x = 0; x < BOARD_GRID_WIDTH; x++ )
    {
      Card tile = this->grid[x][y].tile();
      nom::IntRect pos = this->grid[x][y].bounds();
      nom::uint32 element = this->grid[x][y].element();

      std::cout << line_number
                << ". "
                << tile.name
                << " is at pos "
                << pos.x
                << ", "
                << pos.y
                << " ("
                << pos.w
                << "x"
                << pos.h
                << ") "
                << " with an element ID of "
                << element
                << "."
                << "\n";

    line_number++;
    }
  }
  std::cout << "\n";
}

// Private scope

void Board::initialize_board_elements()
{
  const uint32 MAXIMUM_ELEMENT_TYPE_COUNT = 2;
  const uint32 MAXIMUM_BOARD_ELEMENTS = 4;
  const uint32 MAXIMUM_RAND_NUMBER = 25;
  uint32 num_elements = 0;
  uint32 max_element_type_count[TOTAL_CARD_ELEMENTS] = {0};

  while( num_elements < MAXIMUM_BOARD_ELEMENTS ) {

    for( auto y = 0; y < BOARD_GRID_HEIGHT; y++ ) {
      for( auto x = 0; x < BOARD_GRID_WIDTH; x++ ) {

        uint32 random_element =
          nom::uniform_int_rand<uint32>(0, MAXIMUM_RAND_NUMBER);

        if( random_element == CARD_ELEMENT_EARTH &&
            max_element_type_count[CARD_ELEMENT_EARTH] < MAXIMUM_ELEMENT_TYPE_COUNT &&
            this->grid[x][y].element() != CARD_ELEMENT_EARTH )
        {
          ++max_element_type_count[CARD_ELEMENT_EARTH];
          ++num_elements;
        }

        if( random_element == CARD_ELEMENT_FIRE &&
            max_element_type_count[CARD_ELEMENT_FIRE] < MAXIMUM_ELEMENT_TYPE_COUNT &&
            this->grid[x][y].element() != CARD_ELEMENT_FIRE )
        {
          ++max_element_type_count[CARD_ELEMENT_FIRE];
          ++num_elements;
        }

        if( random_element == CARD_ELEMENT_HOLY &&
            max_element_type_count[CARD_ELEMENT_HOLY] < MAXIMUM_ELEMENT_TYPE_COUNT &&
            this->grid[x][y].element() != CARD_ELEMENT_HOLY )
        {
          ++max_element_type_count[CARD_ELEMENT_HOLY];
          ++num_elements;
        }

        if( random_element == CARD_ELEMENT_ICE &&
            max_element_type_count[CARD_ELEMENT_ICE] < MAXIMUM_ELEMENT_TYPE_COUNT &&
            this->grid[x][y].element() != CARD_ELEMENT_ICE )
        {
          ++max_element_type_count[CARD_ELEMENT_ICE];
          ++num_elements;
        }

        if( random_element == CARD_ELEMENT_POISON &&
            max_element_type_count[CARD_ELEMENT_POISON] < MAXIMUM_ELEMENT_TYPE_COUNT &&
            this->grid[x][y].element() != CARD_ELEMENT_POISON )
        {
          ++max_element_type_count[CARD_ELEMENT_POISON];
          ++num_elements;
        }

        if( random_element == CARD_ELEMENT_THUNDER &&
            max_element_type_count[CARD_ELEMENT_THUNDER] < MAXIMUM_ELEMENT_TYPE_COUNT &&
            this->grid[x][y].element() != CARD_ELEMENT_THUNDER )
        {
          ++max_element_type_count[CARD_ELEMENT_THUNDER];
          ++num_elements;
        }

        if( random_element == CARD_ELEMENT_WATER &&
            max_element_type_count[CARD_ELEMENT_WATER] < MAXIMUM_ELEMENT_TYPE_COUNT &&
            this->grid[x][y].element() != CARD_ELEMENT_WATER )
        {
          ++max_element_type_count[CARD_ELEMENT_WATER];
          ++num_elements;
        }

        if( random_element == CARD_ELEMENT_WIND &&
            max_element_type_count[CARD_ELEMENT_WIND] < MAXIMUM_ELEMENT_TYPE_COUNT &&
            this->grid[x][y].element() != CARD_ELEMENT_WIND )
        {
          ++max_element_type_count[CARD_ELEMENT_WIND];
          ++num_elements;
        }

        if( random_element > (TOTAL_CARD_ELEMENTS - 1) ) {
          random_element = CARD_ELEMENT_NONE;
        }

        this->grid[x][y].set_element(random_element);
      } // end for x loop
    } // end for y loop
  } // end while num_elements < MAXIMUM_BOARD_ELEMENTS
}

nom::Value
serialize_board(const Board* board)
{
  nom::Value objects(nom::Value::ValueType::Null);
  nom::Value card;

  NOM_ASSERT(board != nullptr);
  if( board == nullptr ) {
    // No game board to serialize
    return objects;
  }

  for( nom::int32 y = 0; y != BOARD_GRID_HEIGHT; ++y ) {
    for( nom::int32 x = 0; x != BOARD_GRID_WIDTH; ++x ) {

      // Serialize each card as an object
      card = tt::serialize_card( board->get(x, y) );

      // Additional attributes
      card["player_id"] = board->get(x, y).player_id;
      card["owner"] = board->get(x, y).player_owner;

      objects.push_back(card);
    }
  }

  // Success!
  return objects;
}

tt::Cards
deserialize_board(const nom::Value& objects)
{
  Card card;
  Cards cards;

  // Reconstruct board data
  for( auto itr = objects.begin(); itr != objects.end(); ++itr ) {

    nom::Value attr = itr->ref();
    card = tt::deserialize_card(*itr);

    // Additional card attributes
    card.player_id = NOM_SCAST( PlayerID, attr["player_id"].get_int() );
    card.player_owner = NOM_SCAST( PlayerID, attr["owner"].get_int() );

    cards.push_back(card);
  }

  return cards;
}

} // namespace tt
