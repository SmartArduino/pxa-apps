#pragma once
#include <pxa/ui.hpp>

namespace pxa::ui {

// Scroll axis 2 is vertical in the current public UI protocol. Keep this
// explicit for strategy screens whose actions extend below small displays.
template<class... Children>
class VerticalScrollView : public ViewModifiers {
public:
    static constexpr Capacity capacity = container_capacity<Children...>();
    explicit VerticalScrollView(Children... children)
        : children_(std::move(children)...) {}
    void reset_mount() noexcept requires (detail::ResettableView<Children> || ...) {
        std::apply([](auto&... child) { (detail::reset_view(child), ...); }, children_);
    }
    template<class Page> bool render(Page& page, std::uint32_t parent) {
        auto node = page.create(parent, 3);
        if (!node || !page.transaction().fill(node, protocol::width) ||
            !page.transaction().fill(node, protocol::height) ||
            !page.transaction().u8(node, protocol::scroll_axis, 2))
            return false;
        return std::apply([&](auto&... child) {
            return (child.render(page, node) && ...);
        }, children_);
    }
private:
    std::tuple<Children...> children_;
};

template<class... Children>
auto VerticalScroll(Children&&... children) {
    return VerticalScrollView<std::decay_t<Children>...>(std::forward<Children>(children)...);
}

} // namespace pxa::ui
