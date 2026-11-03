/******************************************************************************

  TTcards - Triple Triad remake

Copyright (c) 2013, 2014, 2015 Jeffrey Carpenter <i8degrees@gmail.com>
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
#ifndef TTCARDS_CARD_RULES_HPP
#define TTCARDS_CARD_RULES_HPP

#include <nomlib/config.hpp>

// See also: Resources/Documentation/rules.md

namespace tt {

enum CardRule: nom::uint32
{
  NO_RULE = 0,
  OPEN_RULE = 1,
  RANDOM_RULE = 2,
  ELEMENTAL_RULE = 4,
  COMBO_RULE = 8,
  SAME_RULE = 16,
  SAME_WALL_RULE = 32,
  PLUS_RULE = 64,
  SUDDEN_DEATH_RULE = 128,
};

enum CardRuleset: nom::uint32
{
  NO_RULESET = NO_RULE,
  OPEN_RULESET = OPEN_RULE,
  RANDOM_RULESET = RANDOM_RULE,
  ELEMENTAL_RULESET = ELEMENTAL_RULE,
  COMBO_RULESET = COMBO_RULE,
  SAME_RULESET = (COMBO_RULE | SAME_RULE),
  SAME_WALL_RULESET = (COMBO_RULE | SAME_WALL_RULE),
  PLUS_RULESET = (COMBO_RULE | PLUS_RULE),
  SUDDEN_DEATH_RULESET = SUDDEN_DEATH_RULE,
};

enum CardTradeRule: nom::uint32
{
  NO_TRADE_RULE = 0,
  ONE_TRADE_RULE,
  DIFF_TRADE_RULE,
  DIRECT_TRADE_RULE,
  ALL_TRADE_RULE,
  LOSER_WINNER_TRADE_RULE,
};

/// \brief The current rule sets in effect for a game.
struct RegionRuleSet
{
  nom::uint32 rule_set = CardRuleset::NO_RULESET;
  nom::uint32 trade_rule = CardTradeRule::NO_TRADE_RULE;
};

bool
is_card_rule_set(const RegionRuleSet* region, nom::uint32 rule);

void
append_card_ruleset(RegionRuleSet* region, nom::uint32 rule);

void
remove_card_ruleset(RegionRuleSet* region, nom::uint32 rule);

void
clear_card_rulesets(RegionRuleSet* region);

//

bool
is_trade_rule_set(const RegionRuleSet* region, nom::uint32 rule);

void
set_trade_rule(RegionRuleSet* region, nom::uint32 rule);

void
clear_trade_rule(RegionRuleSet* region);

} // namespace tt

#endif // defined include guard
