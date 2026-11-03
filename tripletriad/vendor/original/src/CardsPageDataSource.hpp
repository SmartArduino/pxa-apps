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
#ifndef TTCARDS_CARDS_PAGE_DATA_SOURCE_HPP
#define TTCARDS_CARDS_PAGE_DATA_SOURCE_HPP

#include <unordered_map>

#include <Rocket/Core.h>
#include <Rocket/Controls/DataFormatter.h>
#include <Rocket/Controls/DataSource.h>

#include "nomlib/config.hpp"

#include "types.hpp"
#include "Card.hpp"
#include "CardCollection.hpp"

namespace tt {

/// \brief Paged cards model view for holding the game's card collection in a
/// tabular format.
///
/// \see http://librocket.com/wiki/documentation/C%2B%2BManual/Controls/DataGrid
/// \see http://librocket.com/wiki/documentation/tutorials/Datagrid
///
/// \note This interface only supports using one table per data source.
class CardsPageDataSource: public Rocket::Controls::DataSource
{
  public:
    CardsPageDataSource(  const std::string& source,
                          const std::string& table_name = "cards" );

    /// \brief Destructor.
    ///
    /// \remarks libRocket handles the deletion of the DataSource object
    /// instance for us.
    virtual ~CardsPageDataSource();

    /// \note Implements Rocket::Controls::DataSource::GetRow.
    virtual void
    GetRow( Rocket::Core::StringList& row,
            const Rocket::Core::String& table,
            int row_index,
            const Rocket::Core::StringList& columns );

    /// \brief Get the total number of rows for a page.
    ///
    /// \returns An integer value of the number of rows per the current page.
    ///
    /// \note The number of rows returned influence the minimal size (height)
    /// of the the datagrid, even when RCSS properties such as 'max-height',
    /// 'height', etc. are being applied.
    ///
    /// \note Implements Rocket::Controls::DataSource::GetNumRows.
    ///
    /// \see ::num_rows, ::cards_per_page.
    virtual int GetNumRows( const Rocket::Core::String& table );

    const std::string& table_name() const;

    /// \brief Set the table name to use on the data source.
    void set_table_name( const std::string& name );

    /// \brief Get the total number of cards (rows) in storage.
    nom::size_type num_rows() const;

    /// \brief Fetches the contents of a row of a table within the data source.
    ///
    /// \param row        The container object to use use in row results output.
    /// \param table      The name of the table to grab results for.
    /// \param row_index  The element's index position.
    /// \param columns    The columns within the row to grab results for.
    ///
    /// \remarks Convenience alias for ::GetRow.
    void row( tt::string_list& row, int row_index,
              const tt::string_list& columns );

    /// \param pos The internal array element index to insert at; this will
    /// overwrite an existing element.
    ///
    /// \param card The card to be inserted.
    int insert_card(int pos, const Card& card);

    /// \param pos The internal array element index to begin inserting at;
    /// this will overwrite existing element(s).
    ///
    /// \param card The cards to be inserted.
    int insert_cards(int pos, const std::vector<Card>& cards);

    int append_card(const Card& card);

    int append_cards(const std::vector<Card>& cards);

    /// \brief Test whether the cards storage is empty.
    ///
    /// \returns Boolean TRUE if the cards storage is empty; i.e.:
    /// ```::num_rows() == 0```.
    ///   Boolean FALSE if the cards storage is not empty; i.e.:
    /// ```::num_rows() != 0```.
    bool empty() const;

    /// \brief Erase a card from the stored data source instance.
    ///
    /// \param pos The internal array element index to destroy.
    ///
    /// \returns The resulting size of the cards storage after removal on
    /// success, or nom::npos on failure, i.e.: invalid index.
    int erase_card(int pos);

    /// \brief Destroy all the cards in storage.
    void erase_cards();

    /// \brief Rudimentary debugging aid.
    std::string dump();

    /// \brief Reference a card by name.
    ///
    /// \param name The card name to look up.
    const Card& find_by_name(const std::string& name) const;

    /// \brief Reference a card by an internal array element position.
    ///
    /// \param id The card's array element index.
    const Card& find_by_pos(int pos) const;

    int cards_per_page() const;
    int total_pages() const;
    int page() const;

    /// \see ::set_page
    void set_cards_per_page(int page);

    /// \brief Set the current page in the table.
    ///
    /// \remarks This method call is potentially expensive, as it calls for a
    /// refresh on all row indexes within the table.
    void set_page(int page);

    /// \brief Get the internal array element position of a card.
    ///
    /// \param page_pos The row element index of the page.
    ///
    /// \returns An array element index between zero (0) and the maximum
    /// number of cards stored in the data source instance.
    ///
    /// \note The maximum range for a row element index is determined by the
    /// set number of cards per page, i.e.: ~0..10
    ///
    /// \see ::set_cards_per_page
    /// \see ::insert_card, ::insert_cards
    int map_card_pos(int page_pos) const;

  private:
    /// \brief Recalculate the total number of pages.
    ///
    /// \remarks The total number of pages is calculated by dividing the total
    /// number of rows (internally stored) by the number of cards set for a
    /// single page.
    ///
    /// \note This method should ordinarily not need to be called.
    ///
    /// \see ::num_rows, ::set_cards_per_page
    void update();

    void set_total_pages(int num_pages);

    /// \brief Translate the internal array element position to its paged row
    /// element position.
    ///
    /// \param pos  The internal array element index.
    /// \param pg   The row index for the current page of cards.
    ///
    /// \returns A row index between ~0..10 on success, and negative one (-1)
    /// on failure, such as an invalid page number and/or array element
    /// index.
    ///
    /// \note The internal array element range is from ~0..110; the maximum
    /// element range is determined by the number of cards stored by the data
    /// source instance. The maximum range for a row index is determined by the
    /// set number of cards per page.
    ///
    /// \see ::set_cards_per_page
    /// \see ::insert_card, ::insert_cards
    int map_page_row(int pos, int pg) const;

    /// \brief The table name of the data source.
    ///
    /// \note libRocket interfacing
    std::string table_name_;

    /// \brief The data source container.
    std::unordered_map<int, Card> db_;

    /// \brief The maximum number of cards rendered per page.
    int cards_per_page_;

    /// \brief The total number of pages.
    int total_pages_;

    /// \brief The active (updated and rendered) page.
    int page_;
};

/// \brief The status column.
///
/// \note The first column creates a status tag element with a class selector,
/// responsible for rendering the appropriate sprite frame. This sprite serves
/// as a visual indicator of availability of a given card.
///
/// \code
/// <status class='available'></status>
/// <status class='unavailable'></status>
/// \endcode
///
///     See also:
/// Class selectors: status.unavailable, status.available, status.erased
///
/// \see dataview.rml
class CardStatusFormatter: public Rocket::Controls::DataFormatter
{
  public:
    /// \brief Default constructor.
    ///
    /// \remarks The default column formatter used is 'card_status'.
    CardStatusFormatter();

    virtual ~CardStatusFormatter();

    /// \brief Construct an object using a custom column formatter name.
    ///
    /// \param formatter The datagrid column formatter attribute name.
    CardStatusFormatter(const std::string& formatter);

    void FormatData(  Rocket::Core::String& formatted_data,
                      const Rocket::Core::StringList& raw_data );
};

/// \brief The card name column.
///
/// \note The second column is composed of three fields to create a card tag
/// element, used for rendering the text of the card's name.
///
/// \code
/// <card class='unavailable-card' id='Geezard'>Geezard</card>
/// \endcode
///
///     See also:
/// Class selectors: card.unavailable-card, card.available-card, card.erased
///
/// \see dataview.rml
class CardNameFormatter: public Rocket::Controls::DataFormatter
{
  public:
    /// \brief Default constructor.
    ///
    /// \remarks The default column formatter used is 'card_name'.
    CardNameFormatter();

    virtual ~CardNameFormatter();

    /// \brief Construct an object using a custom column formatter name.
    ///
    /// \param formatter The datagrid column formatter attribute name.
    CardNameFormatter(const std::string& formatter);

    void FormatData(  Rocket::Core::String& formatted_data,
                      const Rocket::Core::StringList& raw_data );
};

/// \brief The column showing card availability count.
///
/// \note The third column is composed of a single field to create a card tag
/// element. This tag uses a class selector to render the number of available
/// cards left for the player to choose from.
///
/// \code
/// <card class='available-card' id='Geezard'>Geezard</card>
/// \endcode
///
///     See also:
/// Class selectors: card.available-card, card.erased
///
/// \see dataview.rml
class CardsAvailableFormatter: public Rocket::Controls::DataFormatter
{
  public:
    /// \brief Default constructor.
    ///
    /// \remarks The default column formatter used is 'card_num'.
    CardsAvailableFormatter();

    virtual ~CardsAvailableFormatter();

    /// \brief Construct an object using a custom column formatter name.
    ///
    /// \param formatter The datagrid column formatter attribute name.
    CardsAvailableFormatter(const std::string& formatter);

    void FormatData(  Rocket::Core::String& formatted_data,
                      const Rocket::Core::StringList& raw_data );
};

} // namespace tt

#endif // include guard defined
