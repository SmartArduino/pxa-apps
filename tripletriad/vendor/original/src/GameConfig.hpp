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
#ifndef TTCARDS_GAME_CONFIG_HPP
#define TTCARDS_GAME_CONFIG_HPP

#include <string>
#include <map>

#include <nomlib/config.hpp>

#include "config.hpp"
#include "types.hpp"

namespace nom {

// Forward declarations
class IValueDeserializer;

} // namespace nom

namespace tt {

/// \brief Simple, convenient file-based settings interface
///
/// \remarks This interface does not allow duplicate keys members.
///
/// \todo Consider supporting the use of multiple objects within a
/// configuration file, so we could consolidate config_assets* into one file?
/// (I'm not sure yet if I even like the idea of consolidating asset files).
class GameConfig
{
  public:
    GameConfig();
    ~GameConfig();

    std::string
    get_string( const std::string& node,
                const std::string& default_value = "" ) const;

    /// \returns The de-serialized integer value on success, or
    /// nom::NOM_INT_MIN on failure, such as when the node does not exist.
    ///
    /// \todo Rename to get_int32
    nom::int32 get_int( const std::string& node,
                        nom::int32 default_value = nom::NOM_INT_MIN ) const;

    bool get_bool(const std::string& node, bool default_value) const;

    /// \brief Get a boolean configuration value.
    ///
    /// \returns The boolean value as an integer on success, or negative one
    /// (-1) on failure, such as when the specified node does not exist.
    int get_bool32(const std::string& node, bool default_value) const;

    /// \returns The de-serialized floating-point value on success, or the set
    /// default value on failure, such as when the node does not exist.
    nom::real32
    get_real32(const std::string& node, nom::real32 default_value) const;

    /// \returns The de-serialized floating-point value on success, or the set
    /// default value on failure, such as when the node does not exist.
    nom::real64
    get_real64(const std::string& node, nom::real64 default_value) const;

    tt::string_list get_array(const std::string& node) const;

    bool find(const std::string& node);

    /// \brief Set a new value in the configuration node.
    ///
    /// \remarks Existing member keys will be overwritten.
    ///
    /// \see ::has_property
    void set_property(const std::string& node, const nom::Value& value);

    /// \brief De-serialize a configuration node from a file source.
    ///
    /// \param fp The high-level data output interface to use.
    ///
    /// \todo Restore the current configuration if we fail to parse config and
    /// have cleared the?
    bool load_file(const std::string& filename, nom::IValueDeserializer* fp);

    /// \brief Diagnostics output.
    std::string dump_tree() const;

  private:
    std::map<std::string, nom::Value> config_;
};

} // namespace tt

#endif // include guard defined
