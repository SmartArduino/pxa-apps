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
#include "helpers.hpp"

// Private headers
#include "version.hpp"
#include "CardRenderer.hpp"

// Forward declarations
#include "Card.hpp"
#include "Board.hpp"
#include "CardHand.hpp"
#include "CardResourceLoader.hpp"
#include "nomlib/graphics/Texture.hpp"
#include "nomlib/graphics/Gradient.hpp"
#include "nomlib/graphics/RenderWindow.hpp"
#include "nomlib/graphics/Text.hpp"
#include "nomlib/graphics/sprite/SpriteBatch.hpp"

using namespace nom;

namespace tt {

PlayerID player_id(PlayerIndex player_index)
{
  auto result = PlayerID::PLAYER_ID_INVALID;

  if( player_index == PlayerIndex::PLAYER_1 ) {
    result = PlayerID::PLAYER_ID_1;
  } else if( player_index == PlayerIndex::PLAYER_2 ) {
    result = PlayerID::PLAYER_ID_2;
  } else if( player_index == PlayerIndex::PLAYER_INVALID ) {
    result = PlayerID::PLAYER_ID_INVALID;
  } else {
    // NOTE: Invalid player!
    NOM_ASSERT_INVALID_PATH();
  }

  return result;
}

PlayerID flip_player_id(PlayerIndex player_index)
{
  auto result = PlayerID::PLAYER_ID_INVALID;

  result = NOM_SCAST(PlayerID, player_index + 1);

  if( result == PlayerID::TOTAL_PLAYER_IDS ) {
    // NOTE: Invalid player!
    NOM_ASSERT_INVALID_PATH();
  }

  return result;
}

bool
render_card_background( PlayerID player_id, const nom::Point2i& pos,
                        nom::Gradient* card_background,
                        nom::RenderTarget& target,
                        const nom::Texture* render_target )
{

  if( card_background == nullptr ) {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not render card background: NULL pointer." );
    return false;
  }

  if( card_background->valid() == false ) {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Could not render card background: invalid object." );
    return false;
  }

  // IMPORTANT: This must be done **after** the internals of nom::Gradient has
  // been updated with the final texture!
  if( render_target != nullptr ) {
    if( target.set_render_target(render_target) == false ) {
      NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                    "Could not render card: render targets not supported." );
      return false;
    }
  }

  card_background->draw(target);

  if( render_target != nullptr ) {
    if( target.reset_render_target() == false ) {
      NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                    "Could not render card:",
                    "failed to reset the rendering target." );
      return false;
    }
  }

  return true;
}

bool
render_card_face( CardID face_id, const nom::Point2i& pos,
                  nom::SpriteBatch* card_face,
                  nom::RenderTarget& target,
                  const nom::Texture* render_target )
{
  if( card_face == nullptr ) {
    return false;
  }

  if( card_face->valid() == false ) {
    return false;
  }

  if( face_id < card_face->frames() ) {
    card_face->set_frame(face_id);
  } else {
    return false;
  }

  card_face->set_position(pos);

  if( render_target != nullptr ) {
    if( target.set_render_target(render_target) == false ) {
      NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                    "Could not render card: render targets not supported." );
      return false;
    }
  }

  card_face->draw(target);

  if( render_target != nullptr ) {
    if( target.reset_render_target() == false ) {
      NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                    "Could not render card:",
                    "failed to reset the rendering target." );
      return false;
    }
  }

  return true;
}

bool
render_custom_card_face(  nom::Sprite* card_face, const nom::Point2i& pos,
                          nom::RenderTarget& target,
                          const nom::Texture* render_target )
{
  if( card_face == nullptr ) {
    return false;
  }

  if( card_face->valid() == false ) {
    return false;
  }

  card_face->set_position(pos);

  if( render_target != nullptr ) {
    if( target.set_render_target(render_target) == false ) {
      NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                    "Could not render card: render targets not supported." );
      return false;
    }
  }

  card_face->draw(target);

  if( render_target != nullptr ) {
    if( target.reset_render_target() == false ) {
      NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                    "Could not render card:",
                    "failed to reset the rendering target." );
      return false;
    }
  }

  return true;
}


bool render_card_element( nom::uint32 element_id, const nom::Point2i& pos,
                          nom::SpriteBatch* card_element,
                          nom::RenderTarget& target,
                          const nom::Texture* render_target )
{
  if( card_element == nullptr ) {
    return false;
  }

  if( card_element->valid() == false ) {
    return false;
  }

  switch(element_id)
  {
    case CARD_ELEMENT_NONE:
    default:
    {
      card_element->set_frame(CARD_ELEMENT_NONE);
    } break;

    case CARD_ELEMENT_EARTH:
    {
      card_element->set_frame(CARD_ELEMENT_EARTH);
    } break;

    case CARD_ELEMENT_FIRE:
    {
      card_element->set_frame(CARD_ELEMENT_FIRE);
    } break;

    case CARD_ELEMENT_HOLY:
    {
      card_element->set_frame(CARD_ELEMENT_HOLY);
    } break;

    case CARD_ELEMENT_ICE:
    {
      card_element->set_frame(CARD_ELEMENT_ICE);
    } break;

    case CARD_ELEMENT_POISON:
    {
      card_element->set_frame(CARD_ELEMENT_POISON);
    } break;

    case CARD_ELEMENT_THUNDER:
    {
      card_element->set_frame(CARD_ELEMENT_THUNDER);
    } break;

    case CARD_ELEMENT_WATER:
    {
      card_element->set_frame(CARD_ELEMENT_WATER);
    } break;

    case CARD_ELEMENT_WIND:
    {
      card_element->set_frame(CARD_ELEMENT_WIND);
    } break;
  }

  card_element->set_position(pos);

  if( render_target != nullptr ) {
    if( target.set_render_target(render_target) == false ) {
      NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                    "Could not render card: render targets not supported." );
      return false;
    }
  }

  card_element->draw(target);

  if( render_target != nullptr ) {
    if( target.reset_render_target() == false ) {
      NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                    "Could not render card:",
                    "failed to reset the rendering target." );
      return false;
    }
  }

  return true;
}

bool
render_card_text( nom::uint32 rank, const nom::Point2i& pos,
                  nom::Text* card_text, nom::RenderTarget& target,
                  const nom::Texture* render_target )
{
  if( card_text == nullptr ) {
    return false;
  }

  if( card_text->valid() == false ) {
    return false;
  }

  if( rank == 10 ) {
    card_text->set_text("A");
  } else {
    card_text->set_text( std::to_string(rank) );
  }

  // IMPORTANT: This must be done **after** the internals of nom::Text has been
  // updated with the final texture!
  if( render_target != nullptr ) {
    if( target.set_render_target(render_target) == false ) {
      NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                    "Could not update cache: render targets not supported." );
      return false;
    }
  }

  card_text->set_position(pos);
  card_text->draw(target);

  if( render_target != nullptr ) {
    if( target.reset_render_target() == false ) {
      NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                    "Could not render card:",
                    "failed to reset the rendering target." );
      return false;
    }
  }

  return true;
}

std::shared_ptr<nom::Sprite>
generate_text_sprite(nom::Text& font, const std::string& text)
{
  std::shared_ptr<nom::Sprite> sprite;
  font.set_text(text);

  auto tex = font.clone_texture();
  if( tex != nullptr ) {
    sprite.reset( new Sprite() );
    sprite->set_texture(tex);
  }

  return sprite;
}

void
modify_card_rank( CardResourceLoader* card_res, CardHand* player_hand, bool modifier,
                  nom::uint32 direction )
{
  NOM_ASSERT(card_res != nullptr);
  NOM_ASSERT(player_hand != nullptr);

  Card pcard = player_hand->getSelectedCard();
  CardsIterator pos = player_hand->begin();

  if(modifier) {
    tt::increase_card_rank( NOM_SCAST(CardRank, direction), pcard);
  } else {
    tt::decrease_card_rank( NOM_SCAST(CardRank, direction), pcard);
  }

  // Get the position of the selected card before we erase it so we can
  // reposition the new card at the same index.
  pos = pos + player_hand->at(pcard);

  player_hand->erase(pcard);

  // Render a new card based on the new card ranks text
  auto renderer = tt::create_card_renderer(card_res, pcard);
  pcard.card_renderer.reset(renderer);
  NOM_ASSERT(pcard.card_renderer != nullptr);
  NOM_ASSERT(pcard.card_renderer->valid() == true);

  // Update the player hand with our modified card attributes
  player_hand->cards.insert(pos, pcard);

  // Update the player's selected card
  player_hand->set_position( player_hand->at(pcard) );
  // player_hand->selectCard(pcard);
}

void set_face_down(CardHand* player_hand, bool face_down)
{
  NOM_ASSERT(player_hand != nullptr);
  if( player_hand == nullptr ) {
    return;
  }

  for( auto itr = player_hand->begin(); itr != player_hand->end(); ++itr ) {
    itr->face_down = face_down;
  }

  // Update the card renderings
  player_hand->update();
}

void set_card_id(Card& card, PlayerID pid)
{
  if( card != Card::null ) {
    card.player_id = pid;
    card.player_owner = pid;
  }
}

nom::uint32
update_hand_rendering(CardHand* phand, const nom::Point2i& origin)
{
  uint32 num_updates = 0;
  uint32 hand_index = 0;
  Point2i card_pos(Point2i::zero);

  for( auto itr = phand->begin(); itr != phand->end(); ++itr ) {

    card_pos.x = origin.x;
    card_pos.y = origin.y + (CARD_HEIGHT / 2) * hand_index;

    auto card_sp = (itr)->card_renderer;
    if( card_sp != nullptr ) {
      card_sp->set_position(card_pos);
      ++num_updates;
    } else {
      NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION, "Card ID,", (itr)->id,
                    "was not valid; card renderer invalid." );
    }

    ++hand_index;
  }

  return num_updates;
}

void render_player_hand(const nom::RenderTarget& target, const CardHand* phand)
{
  for( auto itr = phand->begin(); itr != phand->end(); ++itr ) {

    auto card_renderer = (itr)->card_renderer;
    if( card_renderer != nullptr && card_renderer->valid() == true ) {
      card_renderer->render(target);
    }
  }
}

void increase_card_rank(CardRank rank, Card& card)
{
  uint32 dir = (card.ranks[rank] + 1);
  card.ranks[rank] = nom::minimum(dir, MAX_RANK);
}

void decrease_card_rank(CardRank rank, Card& card)
{
  uint32 dir = (card.ranks[rank] - 1);
  card.ranks[rank] = nom::maximum(dir, MIN_RANK);
}

// NOTE: This implementation derives from [Handmade Hero](https://www.handmadehero.org/)'s
// "Enforcing a Video Frame Rate" (Week 4). It is currently assumed that
// "granular sleep" AKA high-resolution timing is properly supported by the
// platform -- this might come back to bite us in the ass someday!
void set_frame_interval(nom::uint32 interval)
{
  real32 target_seconds_per_frame =
    1.0f / (real32)interval;
  uint64 last_delta = 0;
  HighResolutionTimer anim_timer;

  // Abort our frame capping logic when explicitly requested
  if( interval == 0 ) {
    return;
  }

  anim_timer.start();
  last_delta = anim_timer.ticks();

  real32 elapsed_delta =
    HighResolutionTimer::elapsed_ticks(last_delta, anim_timer.ticks() );

  if(elapsed_delta < target_seconds_per_frame) {

    uint32 sleep_ms =
      (uint32)(1000.0f * (target_seconds_per_frame - elapsed_delta) );
    if(sleep_ms > 0) {
      nom::sleep(sleep_ms);
    }
  }
}

std::string version_string()
{
  std::stringstream version_str;

  version_str << APP_NAME << " " << "v"
              << TTCARDS_VERSION_MAJOR << "."
              << TTCARDS_VERSION_MINOR << "."
              << TTCARDS_VERSION_PATCH;

#if ! defined(NDEBUG)
  version_str << "-Debug";
#endif

  return version_str.str();
}

} // namespace tt
