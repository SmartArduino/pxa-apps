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
#ifndef TTCARDS_DEBUG_HPP
#define TTCARDS_DEBUG_HPP

/// \brief The predefined logging categories.
///
/// \see nom::SDL2Logger
///
/// \todo Consider breaking the logging category cfg (file I/O) into input &
/// output?
enum
{
  /// \brief Default logging categories
  ///
  /// \deprecated This identifier exists only to preserve legacy code, and
  /// isn't likely to disappear anytime soon. New code should use one of the
  /// logging categories that best describes their purpose (or add a new one).
  TTCARDS = NOM_LOG_CATEGORY_CUSTOM,

  TTCARDS_LOG_CATEGORY_APPLICATION,
  TTCARDS_LOG_CATEGORY_ERROR,
  TTCARDS_LOG_CATEGORY_ASSERT,
  TTCARDS_LOG_CATEGORY_SYSTEM,
  TTCARDS_LOG_CATEGORY_AUDIO,
  TTCARDS_LOG_CATEGORY_VIDEO,
  TTCARDS_LOG_CATEGORY_RENDER,
  TTCARDS_LOG_CATEGORY_INPUT,
  TTCARDS_LOG_CATEGORY_EVENTS,
  TTCARDS_LOG_CATEGORY_TEST,
  TTCARDS_LOG_CATEGORY_GUI,
  /// \todo Rename to TTCARDS_LOG_CATEGORY_FILE?
  TTCARDS_LOG_CATEGORY_CFG,

  TTCARDS_LOG_CATEGORY_TRACE,
  TTCARDS_LOG_CATEGORY_TRACE_AUDIO,
  TTCARDS_LOG_CATEGORY_TRACE_VIDEO,
  TTCARDS_LOG_CATEGORY_TRACE_RENDER,
  TTCARDS_LOG_CATEGORY_TRACE_GUI,
  TTCARDS_LOG_CATEGORY_TRACE_EVENTS,
  TTCARDS_LOG_CATEGORY_TRACE_STATES,

  TTCARDS_LOG_CATEGORY_INIT_GAME_STATE,
  TTCARDS_LOG_CATEGORY_CARDS_MENU_STATE,
  TTCARDS_LOG_CATEGORY_PLAY_STATE,
  TTCARDS_LOG_CATEGORY_GAME_OVER_STATE,
  TTCARDS_LOG_CATEGORY_PAUSE_STATE,
  TTCARDS_LOG_CATEGORY_CONFIRMATION_DIALOG_STATE,

  TTCARDS_LOG_CATEGORY_CARD_HAND,
  TTCARDS_LOG_CATEGORY_CPU_PLAYER
};

#endif // include guard defined
