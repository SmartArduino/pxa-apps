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
#include "CardRules.hpp"

// Private headers
#include "debug.hpp"

using namespace nom;

namespace tt {

bool
is_card_rule_set(const RegionRuleSet* region, nom::uint32 rule)
{
  bool result = false;

  if( region != nullptr && rule != 0 ) {
    result = ( (region->rule_set & rule) == rule);
  }

  return result;
}

void
append_card_ruleset(RegionRuleSet* region, nom::uint32 rule)
{
  if( region != nullptr ) {
    region->rule_set |= rule;
  }
}

void
remove_card_ruleset(RegionRuleSet* region, nom::uint32 rule)
{
  if( region != nullptr ) {
    region->rule_set &= ~rule;
  }
}

void
clear_card_rulesets(RegionRuleSet* region)
{
  if( region != nullptr ) {
    region->rule_set = CardRuleset::NO_RULESET;
  }
}

//

bool
is_trade_rule_set(const RegionRuleSet* region, nom::uint32 rule)
{
  bool result = false;

  if( region != nullptr ) {
    result = region->trade_rule;
  }

  return result;
}

void
set_trade_rule(RegionRuleSet* region, nom::uint32 rule)
{
  if( region != nullptr ) {
    region->trade_rule = rule;
  }
}

void
remove_trade_rules(RegionRuleSet* region)
{
  if( region != nullptr ) {
    region->trade_rule = CardTradeRule::NO_TRADE_RULE;
  }
}

} // namespace tt
