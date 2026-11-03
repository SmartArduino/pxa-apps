#pragma once
#include <tuple>
#include <type_traits>
#include <utility>
// Only the typed field descriptors are needed by kys-cpp's SQL serializer.
// Keep them independent of Glaze's JSON, network and filesystem backends.
namespace kys_meta {
template<class T> struct meta;
template<class... T> struct fields { std::tuple<T...> value; };
template<class... T> constexpr auto object(T... v) { return fields<T...>{{v...}}; }
template<class T> constexpr auto tuple_size_v = std::tuple_size_v<T>;
using std::get;
template<class T, class M> decltype(auto) get_member(T& t, M m) {
    if constexpr (std::is_member_pointer_v<M>) return t.*m;
    else return m(t);
}
template<size_t N, class F> constexpr void for_each(F&& f) {
    [&]<size_t... I>(std::index_sequence<I...>) {
        (f.template operator()<I>(), ...);
    }(std::make_index_sequence<N>{});
}
}
