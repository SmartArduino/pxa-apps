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
#include "GameConfig.hpp"

// Forward declarations
#include <nomlib/serializers.hpp>

using namespace nom;

namespace tt {

GameConfig::GameConfig()
{
  NOM_LOG_TRACE_PRIO(TTCARDS_LOG_CATEGORY_TRACE, NOM_LOG_PRIORITY_VERBOSE);
}

GameConfig::~GameConfig()
{
  NOM_LOG_TRACE_PRIO(TTCARDS_LOG_CATEGORY_TRACE, NOM_LOG_PRIORITY_VERBOSE);
}

std::string GameConfig::
get_string(const std::string& node, const std::string& default_value) const
{
  std::string result = default_value;

  auto res = this->config_.find(node);
  if( res != this->config_.end() ) {
    result = res->second.get_string();
  }

  return result;
}

nom::int32
GameConfig::get_int(  const std::string& node,
                      nom::int32 default_value ) const
{
  auto res = this->config_.find(node);

  if( res == this->config_.end() ) {
    return default_value;
  } else {
    return res->second.get_int();
  }
}

bool GameConfig::get_bool(const std::string& node, bool default_value) const
{
  bool result = false;

  auto res = this->config_.find(node);
  if( res == this->config_.end() ) {
    result = default_value;
  } else {
    result = res->second.get_bool();
  }

  return result;
}

int GameConfig::get_bool32(const std::string& node, bool default_value) const
{
  int result = 0;
  auto res = this->config_.find(node);

  if( res == this->config_.end() ) {
    result = default_value;
  } else {
    result = NOM_SCAST(int, res->second.get_bool() );
  }

  return result;
}

nom::real32
GameConfig::get_real32(const std::string& node, nom::real32 default_value) const
{
  real32 result = default_value;

  auto res = this->config_.find(node);
  if( res != this->config_.end() ) {
    result = res->second.get_float();
  }

  return result;
}

nom::real64
GameConfig::get_real64(const std::string& node, nom::real64 default_value) const
{
  real64 result = default_value;

  auto res = this->config_.find(node);
  if( res != this->config_.end() ) {
    result = res->second.get_double();
  }

  return result;
}

tt::string_list
GameConfig::get_array(const std::string& node) const
{
  tt::string_list out;

  auto res = this->config_.find(node);

  if( res != this->config_.end() ) {

    NOM_ASSERT( res->second.array_type() == true ||
                res->second.object_type() == true );

    nom::Value arr = res->second;

    for( auto it = arr.begin(); it != arr.end(); ++it ) {
      out.push_back( it->get_string() );
    } // end for loop
  } // end if found

  return out;
}

bool GameConfig::find(const std::string& node)
{
  auto res = this->config_.find(node);

  if( res != this->config_.end() ) {
    // Success -- the property exists in the node
    return true;
  } else {
    // Failure -- the property does **not** exist in the node
    return false;
  }
}

void
GameConfig::set_property(const std::string& node, const nom::Value& value)
{
  std::string value_as_string("INVALID VALUE");

  this->config_[node] = value;

  value_as_string = "'" + value.stringify() + "'";
  NOM_LOG_DEBUG(  TTCARDS_LOG_CATEGORY_CFG, node, ":", value_as_string,
                  "has been added to the configuration cache." );
}

bool
GameConfig::load_file(const std::string& filename, nom::IValueDeserializer* fp)
{
  // Storage buffer for our configuration we are loading in; if everything is
  // successful, we will overwrite the existing configuration map with this one.
  GameConfig cfg;
  nom::Value objects;

  if( fp == nullptr ) {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Unable to parse input:",
                  "de-serializer was NULL." );
    return false;
  }

  if( fp->load(filename, objects) == false ) {
    NOM_LOG_ERR(  TTCARDS_LOG_CATEGORY_APPLICATION,
                  "Unable to parse input file:", filename );
    return false;
  }

  NOM_LOG_INFO( TTCARDS_LOG_CATEGORY_CFG, "Parsed game configuration file:",
                filename );

  NOM_LOG_VERBOSE(TTCARDS_LOG_CATEGORY_CFG, objects);

  // For sake of simplicity, we do not support storing more than one object in
  // the configuration file,
  NOM_ASSERT(objects.size() == 1);

  for( auto itr = objects.begin(); itr != objects.end(); ++itr ) {

    if( itr->object_type() ) {
      nom::Object obj = itr->object();

      for( auto it = obj.begin(); it != obj.end(); ++it ) {

        nom::Value::ConstIterator members(it);
        std::string key = members.key();

        if( members->int_type() ) {
          cfg.set_property(key, members->get_int() );
        } else if( members->double_type() ) {
          cfg.set_property(key, members->get_double() );
        } else if( members->string_type() ) {
          cfg.set_property(key, members->get_string() );
        } else if( members->bool_type() ) {
          cfg.set_property(key, members->get_bool() );
        } else if( members->array_type() ) {
          cfg.set_property(key, members->array() );
        } else if( members->object_type() ) {
          cfg.set_property(key, members->object() );
        } else {
          // In practice, this condition should never happen! Something has
          // probably gone terribly wrong with nomlib's de-serializing
          // internals if this ever occurs.
          NOM_LOG_WARN( TTCARDS_LOG_CATEGORY_APPLICATION,
                        "Unrecognized member pair in", filename,
                        "with the key", key );
        }
      } // end for object loop
    } // end if object type
  } // end for objects loop

  // If we have made it this far, go ahead and overwrite our new configuration
  // onto the existing configuration map store!
  this->config_ = cfg.config_;

  return true;
}

std::string GameConfig::dump_tree() const
{
  for( auto itr = this->config_.begin(); itr != this->config_.end(); ++itr ) {
    return itr->second.dump(itr->second);
  }

  return "\0";
}

} // namespace tt
