#pragma once
#include <pxa/ui.hpp>
#include <pxa/ui_controller.hpp>
namespace arcade {
template<class F>struct GameInput {
    static constexpr bool overlay=true;
    static constexpr pxa::ui::Capacity capacity{1,0,1};
    F pointer;
    template<class Page>bool render(Page& page,uint32_t parent){
        using namespace pxa::ui;
        auto node=page.create(parent,protocol::canvas);if(!node)return false;
        auto& tx=page.transaction();
        return tx.fill(node,protocol::width)&&tx.fill(node,protocol::height)&&
            tx.rgba(node,protocol::background,0)&&
            tx.u64(node,protocol::event_mask,(1ull<<6)|(1ull<<9))&&page.on_pointer(node,pointer);
    }
};
}
