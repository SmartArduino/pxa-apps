/******************************************************************************

  nomlib - C++11 cross-platform game engine

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
#include "CardsPageDataSource.hpp"

using namespace Rocket::Core;

namespace tt {

CardsPageDataSource::CardsPageDataSource( const std::string& source,
                                          const std::string& table_name ) :
  Rocket::Controls::DataSource( source.c_str() )
{
  NOM_LOG_TRACE_PRIO( NOM_LOG_CATEGORY_TRACE, nom::NOM_LOG_PRIORITY_VERBOSE );

  // Defaults
  this->set_cards_per_page(11);
  this->set_total_pages(0);
  this->set_page(0);
  this->set_table_name(table_name);
}

CardsPageDataSource::~CardsPageDataSource()
{
  NOM_LOG_TRACE_PRIO( NOM_LOG_CATEGORY_TRACE, nom::NOM_LOG_PRIORITY_VERBOSE );
}

void
CardsPageDataSource::GetRow(  Rocket::Core::StringList& row,
                              const Rocket::Core::String& table,
                              int row_index,
                              const Rocket::Core::StringList& columns )
{
  int cards_per_page = this->cards_per_page();
  int page = this->page();

  // The starting ID position to begin reading from the storage container
  int row_offset = row_index + (cards_per_page * page);

  // The last ID position; should match return value of ::GetNumRows
  int row_end = cards_per_page;

  // Out of bounds
  if( row_offset >= this->num_rows() ) {
    return;
  }

  if( table == this->table_name().c_str() ) {

    for( auto i = 0; i != columns.size(); ++i ) {

      if( columns[i] == "status" && row_index < row_end ) {

        // First column; card status

        if( this->db_[row_offset] == Card::null ) {
          row.push_back("-1");  // Do not render this column
        } else if( row_index < row_end && this->db_[row_offset].num < 1 ) {
          row.push_back("0");   // No cards left sprite frame
        } else {
          row.push_back("1");   // Remaining cards left sprite frame
        }
      } else if( row_index < row_end && columns[i] == "available" ) {

        // Second column (A); class selector of the card tag to control the
        // rendering of the text color of this column
        String card_class =
          "unavailable-card";

        if( this->db_[row_offset] == Card::null ) {
          // Do not render this column
          card_class = "erased";
        } else if( this->db_[row_offset].num > 0 ) {
          card_class = "available-card";
        }

        row.push_back(card_class);
      } else if( row_index < row_end && columns[i] == "id" ) {

        // Second column (B); card name (id selector of the card tag)
        String card_id =
          this->db_[row_offset].name.c_str();

        row.push_back(card_id);
      } else if( row_index < row_end && columns[i] == "name" ) {

        // Second column (C); card name (inner RML text of the tag)
        String card_name =
          this->db_[row_offset].name.c_str();

        row.push_back(card_name);
      } else if( row_index < row_end && columns[i] == "num_class" ) {

        // Third column (A); class selector of the card tag to control the
        // rendering of the text for this column
        String card_class = "available-card";

        if( this->db_[row_offset] == Card::null ) {
          // Do not render this column
          card_class = "erased";
        }

        row.push_back(card_class);
      } else if( row_index < row_end && columns[i] == "num" ) {

        // Third column (B); total number of collected cards -- inner RML text
        // of the tag
        String card_num =
          std::to_string( this->db_[row_offset].num ).c_str();

        row.push_back(card_num);
      }
    } // end for cols loop
  }
} // end GetRow func

int CardsPageDataSource::GetNumRows(const Rocket::Core::String& table)
{
  int cards_per_page = this->cards_per_page();

  if( table == this->table_name().c_str() ) {
    // NOTE: Influences the maximum number of row items returned from
    // ::GetRow (libRocket interface)
    return cards_per_page;
  }

  NOM_ASSERT( table == this->table_name().c_str() );

  // Err; ..invalid table?
  return 0;
}

const std::string& CardsPageDataSource::table_name() const
{
  return this->table_name_;
}

void CardsPageDataSource::set_table_name( const std::string& name )
{
  this->table_name_ = name;
}

nom::size_type CardsPageDataSource::num_rows() const
{
  return this->db_.size();
}

void
CardsPageDataSource::row( tt::string_list& row, int row_index,
                          const tt::string_list& columns )
{
  Rocket::Core::StringList ret_rows;
  Rocket::Core::StringList ret_columns;

  for( auto itr = columns.begin(); itr != columns.end(); ++itr )
  {
    ret_columns.push_back( (*itr).c_str() );
  }

  // this->GetRow( ret_rows, table.c_str(), row_index, ret_columns );
  this->GetRow( ret_rows, this->table_name().c_str(), row_index, ret_columns );

  for( auto itr = ret_rows.begin(); itr != ret_rows.end(); ++itr )
  {
    row.push_back( (*itr).CString() );
  }
}

int CardsPageDataSource::insert_card(int pos, const Card& card)
{
  int num_rows = this->num_rows();
  int page = this->page();
  int page_pos = -1;
  Card c = card;

  if( pos <= num_rows ) {

    this->db_[pos] = card;

    // Translate the internal array element position to a paged element
    // position
    page_pos = this->map_page_row(pos, page);
    if( page_pos != -1 ) {
      NotifyRowChange(  this->table_name().c_str(),
                        // First row index changed (zero-based index)
                        page_pos,
                        // The number of rows changed, including the first row
                        1 );

      // Update page count
      this->update();

      // Resulting (modified) size
      return this->num_rows();
    }
  }

  // Err
  return 0;
}

int CardsPageDataSource::insert_cards(int pos, const std::vector<Card>& cards)
{
  int num_rows = this->num_rows();
  int input_pos = 0;
  int card_pos = 0;

  if( pos <= num_rows ) {

    for( auto itr = cards.begin(); itr != cards.end(); ++itr ) {

      // card insertion row
      int card_index = pos + input_pos;

      this->db_[card_index] = *itr;
      ++input_pos;
    };

    num_rows = this->num_rows();
    card_pos = pos + ( cards.size() - 1 );
    if( card_pos >= 0 && card_pos <= num_rows ) {

      NotifyRowChange( this->table_name().c_str() );

      // Update page count
      this->update();

      // Resulting (modified) size
      return num_rows;
    }
  }

  // Err
  return 0;
}

int CardsPageDataSource::append_card(const Card& card)
{
  int num_rows = this->num_rows();
  int page = this->page();
  int page_pos = -1;

  this->db_[num_rows] = card;

  page_pos = this->map_page_row(num_rows, page);
  if( page_pos != -1 ) {

    NotifyRowAdd( this->table_name().c_str(),
                  // The index of the first row added (zero-based index)
                  page_pos,
                  // The number of rows added, including first row
                  1 );
  } else {
    // NOTE: The current page is filled, so we need to add the card to the
    // beginning of a new page and render the results immediately.
    NotifyRowChange( this->table_name().c_str() );
  }

  // Update page count
  this->update();

  // Resulting (modified) size
  return this->num_rows();
}

int CardsPageDataSource::append_cards(const std::vector<Card>& cards)
{
  int num_rows = this->num_rows();
  int page = this->page();
  int page_pos = -1;
  int card_pos = 0;

  for( auto itr = cards.begin(); itr != cards.end(); ++itr ) {

    // card insertion row
    int card_index = num_rows + card_pos;

    this->db_[card_index] = *itr;
    ++card_pos;
  }

  page_pos = this->map_page_row(num_rows, page);
  if( page_pos != -1 ) {
    NotifyRowAdd( this->table_name().c_str(),
                  // The index of the first row added (zero-based index)
                  page_pos,
                  // The number of rows added, including the first row
                  cards.size() );
  } else {
    // NOTE: The current page is filled, so we need to add the card to the
    // beginning of a new page and render the results immediately.
    NotifyRowChange( this->table_name().c_str() );
  }

  // Update page count
  this->update();

  // Resulting (modified) size
  return this->num_rows();
}

bool CardsPageDataSource::empty() const
{
  return this->db_.empty();
}

int CardsPageDataSource::erase_card(int pos)
{
  int num_rows = this->num_rows();
  int page = this->page();
  int page_pos = -1;

  if( pos <= num_rows ) {

    this->db_.erase(pos);

    page_pos = this->map_page_row(pos, page);
    if( page_pos != -1 ) {
      NotifyRowChange(  this->table_name().c_str() );
    }

    // Update page count
    this->update();

    // Resulting (modified) size
    return this->num_rows();
  }

  // Err
  return 0;
}

void CardsPageDataSource::erase_cards()
{
  this->db_.clear();

  NotifyRowChange( this->table_name().c_str() );

  // Update page count
  this->update();
}

std::string CardsPageDataSource::dump()
{
  std::stringstream os;

  os
  << "table: " << this->table_name()
  << "[" << this->num_rows() << "]" << std::endl;

  os << std::endl;
  for( auto itr = this->db_.begin(); itr != this->db_.end(); ++itr )
  {
    os
    << (itr)->second.id
    << " : "
    << (itr)->second.name

    << " ["
    << (itr)->second.num
    << "]"
    << std::endl;
  }

  return os.str();
}

const Card& CardsPageDataSource::find_by_name(const std::string& name) const
{
  for( auto itr = this->db_.begin(); itr != this->db_.end(); ++itr )
  {
    if( (itr)->second.name == name ) {
      // Successful match
      return itr->second;
    }
  }

  // No match
  return Card::null;
}

const Card& CardsPageDataSource::find_by_pos(int pos) const
{
  auto res = this->db_.find(pos);
  if( res != this->db_.end() ) {
    // Successful match
    return res->second;
  } else {
    // No match
    return Card::null;
  }
}

int CardsPageDataSource::cards_per_page() const
{
  return this->cards_per_page_;
}

int CardsPageDataSource::total_pages() const
{
  return this->total_pages_;
}

int CardsPageDataSource::page() const
{
  return this->page_;
}

void CardsPageDataSource::set_cards_per_page(int pg)
{
  this->cards_per_page_ = pg;

  this->update();
}

void CardsPageDataSource::set_page(int page)
{
  this->page_ = page;

  // This is necessary for ::GetRow to immediately update its row indexes to
  // match the requested page
  NotifyRowChange(  this->table_name().c_str(),
                    // First row index changed (zero-based index)
                    0,
                    // The number of rows changed, including the first row
                    11 );

  // Update total page count
  this->update();
}

int CardsPageDataSource::map_card_pos(int page_pos) const
{
  int cards_per_page = this->cards_per_page();
  int page = this->page();
  int selection = -1;
  selection = page_pos + ( cards_per_page * page );

  NOM_ASSERT( selection >= 0 );

  return selection;
}

// private
int CardsPageDataSource::map_page_row(int pos, int pg) const
{
  int cards_per_page = this->cards_per_page();
  int selection = -1;
  NOM_ASSERT( pos <= this->num_rows() );

  selection = pos - ( cards_per_page * pg );

  if( selection >= cards_per_page ) {
    // Err; out of bounds
    return -1;
  }

  NOM_ASSERT( selection >= 0 );

  return selection;
}

// private
void CardsPageDataSource::set_total_pages(int num_pages)
{
  this->total_pages_ = num_pages;
}

// private
void CardsPageDataSource::update()
{
  double pages = this->num_rows() / this->cards_per_page();

  // Calculate total number of pages we need for the stored cards
  if( ( this->num_rows() % this->cards_per_page() ) != 0 ) {
    // Partial page
    pages = floor(pages + 1.0f);
  }

  this->set_total_pages( NOM_SCAST(int, pages) );
}

// CardStatusFormatter

CardStatusFormatter::CardStatusFormatter() :
  Rocket::Controls::DataFormatter("card_status")
{
  NOM_LOG_TRACE_PRIO( NOM_LOG_CATEGORY_TRACE, nom::NOM_LOG_PRIORITY_VERBOSE );
}

CardStatusFormatter::~CardStatusFormatter()
{
  NOM_LOG_TRACE_PRIO( NOM_LOG_CATEGORY_TRACE, nom::NOM_LOG_PRIORITY_VERBOSE );
}

CardStatusFormatter::CardStatusFormatter(const std::string& formatter) :
  Rocket::Controls::DataFormatter( formatter.c_str() )
{
  NOM_LOG_TRACE_PRIO( NOM_LOG_CATEGORY_TRACE, nom::NOM_LOG_PRIORITY_VERBOSE );
}

void CardStatusFormatter::FormatData( Rocket::Core::String& formatted_data,
                                      const Rocket::Core::StringList& raw_data )
{
  if( raw_data[0] == "-1" ) {
    // Do not render
    formatted_data = "<status class='erased' />";
  } else if( raw_data[0] == "0" ) {
    formatted_data = "<status class='unavailable' />";
  } else if( raw_data[0] == "1" ) {
    formatted_data = "<status class='available' />";
  }
}

// CardNameFormatter

CardNameFormatter::CardNameFormatter() :
  Rocket::Controls::DataFormatter("card_name")
{
  NOM_LOG_TRACE_PRIO( NOM_LOG_CATEGORY_TRACE, nom::NOM_LOG_PRIORITY_VERBOSE );
}

CardNameFormatter::~CardNameFormatter()
{
  NOM_LOG_TRACE_PRIO( NOM_LOG_CATEGORY_TRACE, nom::NOM_LOG_PRIORITY_VERBOSE );
}

CardNameFormatter::CardNameFormatter(const std::string& formatter) :
  Rocket::Controls::DataFormatter( formatter.c_str() )
{
  NOM_LOG_TRACE_PRIO( NOM_LOG_CATEGORY_TRACE, nom::NOM_LOG_PRIORITY_VERBOSE );
}

void CardNameFormatter::FormatData( Rocket::Core::String& formatted_data,
                                    const Rocket::Core::StringList& raw_data )
{
  if( raw_data.size() == 3 ) {
    formatted_data = "<card class='" + raw_data[0] + "'" +
    "id='" + raw_data[1] + "'>" + raw_data[2] + "</card>";
  }
}

// CardsAvailableFormatter

CardsAvailableFormatter::CardsAvailableFormatter() :
  Rocket::Controls::DataFormatter("card_num")
{
  NOM_LOG_TRACE_PRIO( NOM_LOG_CATEGORY_TRACE, nom::NOM_LOG_PRIORITY_VERBOSE );
}

CardsAvailableFormatter::~CardsAvailableFormatter()
{
  NOM_LOG_TRACE_PRIO( NOM_LOG_CATEGORY_TRACE, nom::NOM_LOG_PRIORITY_VERBOSE );
}

CardsAvailableFormatter::CardsAvailableFormatter(const std::string& formatter) :
  Rocket::Controls::DataFormatter( formatter.c_str() )
{
  NOM_LOG_TRACE_PRIO( NOM_LOG_CATEGORY_TRACE, nom::NOM_LOG_PRIORITY_VERBOSE );
}

void
CardsAvailableFormatter::FormatData(  Rocket::Core::String& formatted_data,
                                      const Rocket::Core::StringList& raw_data )
{
  if( raw_data.size() == 2 ) {
    formatted_data = "<card class='" + raw_data[0] + "'>" + raw_data[1] +
    "</card>";
  }
}

} // namespace tt
