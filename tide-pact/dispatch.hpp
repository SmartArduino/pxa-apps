#pragma once

namespace tide {

enum DispatchKind { DispatchMail, DispatchFreight, DispatchPassengers };
struct DispatchTerms {
    Text name;
    int space, bond, fee, slack, renown;
};
inline constexpr DispatchTerms DispatchRates[]{
    {"急信",1,18,52,1,2}, {"货运",6,45,80,4,2}, {"载客",4,35,100,3,3},
};

struct DispatchOrder {
    int from, to, stop;
    DispatchKind kind;
    Text title;
};
// Entries are numbered in the voyage save. Keep the existing order.
inline constexpr DispatchOrder DispatchOrders[]{
    {lisbon,cadiz,-1,DispatchMail,"海关回函"},
    {lisbon,madeira,-1,DispatchFreight,"修船的木箱"},
    {lisbon,canaries,madeira,DispatchPassengers,"南下的织工"},
    {madeira,lisbon,-1,DispatchMail,"糖价清单"},
    {madeira,canaries,-1,DispatchFreight,"送往糖坊的模具"},
    {madeira,cadiz,lisbon,DispatchPassengers,"回家探亲"},
    {canaries,madeira,-1,DispatchMail,"风季前的回信"},
    {canaries,ribeira,-1,DispatchFreight,"给河口的麻绳"},
    {canaries,lisbon,madeira,DispatchPassengers,"返乡的木匠"},
    {cadiz,tangier,-1,DispatchMail,"渡船的回执"},
    {cadiz,lisbon,-1,DispatchFreight,"封好的盐桶"},
    {cadiz,madeira,lisbon,DispatchPassengers,"去糖坊的学徒"},
    {ribeira,canaries,-1,DispatchFreight,"回航的空木桶"},
    {tangier,cadiz,-1,DispatchMail,"鱼市来信"},
    {bordeaux,lisbon,-1,DispatchFreight,"酒窖的空桶"},
    {bordeaux,bristol,-1,DispatchPassengers,"北上的织匠"},
    {bristol,bordeaux,-1,DispatchFreight,"羊毛样包"},
    {bristol,bruges,-1,DispatchMail,"织机订条"},
    {bruges,bristol,-1,DispatchMail,"布市的回函"},
    {lisbon,madeira,-1,DispatchFreight,"驿道木料"},
};
inline constexpr int DispatchCount=std::size(DispatchOrders), RoadFreight=19;
static_assert(DispatchCount<255);

enum DispatchOutcome { NoDispatchReceipt, DispatchOnTime, DispatchLate, DispatchCancelled, DispatchLost };
struct DispatchRecord {
    int due=0;
    std::uint8_t number=0, stopped=0, last=0, outcome=NoDispatchReceipt;
};
struct DispatchQuote {
    int days=0, fee=0;
};

} // namespace tide
