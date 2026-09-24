#ifndef PXA_UI_DEMO_PAGE_H
#define PXA_UI_DEMO_PAGE_H

#include "pxa_ui.h"

/* Opinionated page template shared by the PXA lab applications. */
#define PXA_UI_DEMO_PAGE_HAS_BUTTON UINT8_C(1)
#define PXA_UI_DEMO_PAGE_HAS_PROGRESS UINT8_C(2)
#define PXA_UI_DEMO_PAGE_HAS_SWITCH UINT8_C(4)
#define PXA_UI_DEMO_PAGE_HAS_SLIDER UINT8_C(8)
#define PXA_UI_DEMO_PAGE_HAS_IMAGE UINT8_C(16)

#define PXA_UI_DEMO_PAGE_NODE_SWITCH UINT32_C(9)
#define PXA_UI_DEMO_PAGE_NODE_SLIDER UINT32_C(10)
#define PXA_UI_DEMO_PAGE_NODE_BUTTON UINT32_C(12)

typedef struct {
    const char* title;
    const char* body;
    const char* status;
    const char* action;
    const char* image_path;
    uint8_t features;
    uint8_t icon;
    uint8_t progress;
    uint8_t switch_value;
    uint8_t slider_value;
    uint8_t enabled;
} pxa_ui_demo_page_t;

static inline size_t pxa_ui_demo_string_length(const char* value) {
    size_t size = 0;
    if (value == NULL) return 0;
    while (value[size] != '\0') ++size;
    return size;
}

static inline size_t pxa_ui_demo_format_u32(char* output, uint32_t value) {
    char reverse[10];
    size_t count = 0;
    size_t index;
    do {
        reverse[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0);
    for (index = 0; index < count; ++index)
        output[index] = reverse[count - index - 1u];
    output[count] = '\0';
    return count;
}

static inline int pxa_ui_demo_page_render_with_root_event_mask(
    uint32_t* generation, uint8_t* scratch, size_t scratch_capacity,
    const pxa_ui_demo_page_t* page, uint64_t root_event_mask) {
    pxa_ui_transaction_t transaction = {0};
    uint32_t next;
    int ok;
    if (generation == NULL || page == NULL || page->title == NULL ||
        page->body == NULL || page->status == NULL) return 0;
    next = *generation + 1u;
    if (next == 0 ||
        !pxa_ui_transaction_begin(&transaction, next,
                                  PXA_UI_TRANSACTION_REPLACE_SURFACE, scratch,
                                  scratch_capacity)) return 0;
    ok = pxa_ui_create(&transaction, 1, 0, 0, PXA_UI_NODE_ROOT) &&
         pxa_ui_set_event_mask(&transaction, 1, root_event_mask) &&
         pxa_ui_set_u8(&transaction, 1, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_theme_color(&transaction, 1, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_BACKGROUND) &&
         pxa_ui_create(&transaction, 2, 1, 0, PXA_UI_NODE_BOX) &&
         pxa_ui_set_length(&transaction, 2, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_length(&transaction, 2, PXA_UI_PROPERTY_HEIGHT,
                           PXA_UI_LENGTH_PX, 42) &&
         pxa_ui_set_u8(&transaction, 2, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_ROW) &&
         pxa_ui_set_u8(&transaction, 2, PXA_UI_PROPERTY_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_padding(&transaction, 2, 12, 6, 12, 6) &&
         pxa_ui_set_dp(&transaction, 2, PXA_UI_PROPERTY_GAP, 8) &&
         pxa_ui_set_theme_color(&transaction, 2, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_PRIMARY) &&
         pxa_ui_create(&transaction, 3, 2, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_icon(&transaction, 3, page->icon) &&
         pxa_ui_set_theme_color(&transaction, 3, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_ON_PRIMARY) &&
         pxa_ui_create(&transaction, 4, 2, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 4, page->title,
                         pxa_ui_demo_string_length(page->title)) &&
         pxa_ui_set_font_role(&transaction, 4, PXA_UI_FONT_ROLE_TITLE) &&
         pxa_ui_set_theme_color(&transaction, 4, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_ON_PRIMARY) &&
         pxa_ui_create(&transaction, 5, 1, 0, PXA_UI_NODE_SCROLL) &&
         pxa_ui_set_length(&transaction, 5, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_length(&transaction, 5, PXA_UI_PROPERTY_HEIGHT,
                           PXA_UI_LENGTH_PX, 0) &&
         pxa_ui_set_u16(&transaction, 5, PXA_UI_PROPERTY_GROW, 1) &&
         pxa_ui_set_u8(&transaction, 5, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_u8(&transaction, 5, PXA_UI_PROPERTY_SCROLL_AXIS, 2) &&
         pxa_ui_set_u8(&transaction, 5, PXA_UI_PROPERTY_SCROLLBAR, 1) &&
         pxa_ui_set_padding(&transaction, 5, 14, 12, 14, 14) &&
         pxa_ui_set_dp(&transaction, 5, PXA_UI_PROPERTY_GAP, 10) &&
         pxa_ui_set_theme_color(&transaction, 5, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_BACKGROUND) &&
         pxa_ui_create(&transaction, 6, 5, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_length(&transaction, 6, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_text(&transaction, 6, page->body,
                         pxa_ui_demo_string_length(page->body)) &&
         pxa_ui_set_theme_color(&transaction, 6, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_TEXT) &&
         pxa_ui_create(&transaction, 7, 5, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_length(&transaction, 7, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_text(&transaction, 7, page->status,
                         pxa_ui_demo_string_length(page->status)) &&
         pxa_ui_set_theme_color(&transaction, 7, PXA_UI_PROPERTY_FOREGROUND,
             page->enabled ? PXA_UI_THEME_MUTED : PXA_UI_THEME_DANGER);
    if (ok && (page->features & PXA_UI_DEMO_PAGE_HAS_PROGRESS) != 0) {
        ok = pxa_ui_create(&transaction, 8, 5, 0, PXA_UI_NODE_PROGRESS) &&
             pxa_ui_set_length(&transaction, 8, PXA_UI_PROPERTY_WIDTH,
                               PXA_UI_LENGTH_FILL, 0) &&
             pxa_ui_set_length(&transaction, 8, PXA_UI_PROPERTY_HEIGHT,
                               PXA_UI_LENGTH_PX, 12) &&
             pxa_ui_set_i32(&transaction, 8, PXA_UI_PROPERTY_VALUE,
                            page->progress);
    }
    if (ok && (page->features & PXA_UI_DEMO_PAGE_HAS_SWITCH) != 0) {
        ok = pxa_ui_create_typed(&transaction,
                                 PXA_UI_DEMO_PAGE_NODE_SWITCH, 5, 0,
                                 PXA_UI_NODE_CONTROL,
                                 PXA_UI_CONTROL_TOGGLE) &&
             pxa_ui_set_i32(&transaction, PXA_UI_DEMO_PAGE_NODE_SWITCH,
                            PXA_UI_PROPERTY_VALUE,
                            page->switch_value ? 1 : 0) &&
             pxa_ui_set_event_mask(&transaction,
                                   PXA_UI_DEMO_PAGE_NODE_SWITCH,
                                   PXA_UI_EVENT_MASK_VALUE_CHANGED);
    }
    if (ok && (page->features & PXA_UI_DEMO_PAGE_HAS_SLIDER) != 0) {
        ok = pxa_ui_create_typed(&transaction,
                                 PXA_UI_DEMO_PAGE_NODE_SLIDER, 5, 0,
                                 PXA_UI_NODE_CONTROL,
                                 PXA_UI_CONTROL_SLIDER) &&
             pxa_ui_set_length(&transaction, PXA_UI_DEMO_PAGE_NODE_SLIDER,
                               PXA_UI_PROPERTY_WIDTH,
                               PXA_UI_LENGTH_FILL, 0) &&
             pxa_ui_set_i32(&transaction, PXA_UI_DEMO_PAGE_NODE_SLIDER,
                            PXA_UI_PROPERTY_VALUE, page->slider_value) &&
             pxa_ui_set_event_mask(&transaction,
                                   PXA_UI_DEMO_PAGE_NODE_SLIDER,
                                   PXA_UI_EVENT_MASK_VALUE_CHANGED);
    }
    if (ok && (page->features & PXA_UI_DEMO_PAGE_HAS_IMAGE) != 0 &&
        page->image_path != NULL) {
        ok = pxa_ui_create(&transaction, 11, 5, 0, PXA_UI_NODE_IMAGE) &&
             pxa_ui_set_length(&transaction, 11, PXA_UI_PROPERTY_WIDTH,
                               PXA_UI_LENGTH_PX, 64) &&
             pxa_ui_set_length(&transaction, 11, PXA_UI_PROPERTY_HEIGHT,
                               PXA_UI_LENGTH_PX, 64) &&
             pxa_ui_set_property(&transaction, 11, PXA_UI_PROPERTY_ASSET,
                                 page->image_path,
                                 pxa_ui_demo_string_length(page->image_path));
    }
    if (ok && (page->features & PXA_UI_DEMO_PAGE_HAS_BUTTON) != 0 &&
        page->action != NULL) {
        ok = pxa_ui_create_typed(&transaction,
                                 PXA_UI_DEMO_PAGE_NODE_BUTTON, 5, 0,
                                 PXA_UI_NODE_CONTROL,
                                 PXA_UI_CONTROL_BUTTON) &&
             pxa_ui_set_length(&transaction, PXA_UI_DEMO_PAGE_NODE_BUTTON,
                               PXA_UI_PROPERTY_WIDTH,
                               PXA_UI_LENGTH_FILL, 0) &&
             pxa_ui_set_length(&transaction, PXA_UI_DEMO_PAGE_NODE_BUTTON,
                               PXA_UI_PROPERTY_HEIGHT,
                               PXA_UI_LENGTH_PX, 40) &&
             pxa_ui_set_u8(&transaction, PXA_UI_DEMO_PAGE_NODE_BUTTON,
                           PXA_UI_PROPERTY_LAYOUT, PXA_UI_LAYOUT_COLUMN) &&
             pxa_ui_set_u8(&transaction, PXA_UI_DEMO_PAGE_NODE_BUTTON,
                           PXA_UI_PROPERTY_JUSTIFY, PXA_UI_ALIGN_CENTER) &&
             pxa_ui_set_u8(&transaction, PXA_UI_DEMO_PAGE_NODE_BUTTON,
                           PXA_UI_PROPERTY_ALIGN, PXA_UI_ALIGN_CENTER) &&
             pxa_ui_set_dp(&transaction, PXA_UI_DEMO_PAGE_NODE_BUTTON,
                           PXA_UI_PROPERTY_RADIUS, 6) &&
             pxa_ui_set_u8(&transaction, PXA_UI_DEMO_PAGE_NODE_BUTTON,
                           PXA_UI_PROPERTY_ENABLED, page->enabled) &&
             pxa_ui_set_event_mask(&transaction,
                                   PXA_UI_DEMO_PAGE_NODE_BUTTON,
                                   PXA_UI_EVENT_MASK_CLICK) &&
             pxa_ui_set_theme_color(&transaction, PXA_UI_DEMO_PAGE_NODE_BUTTON,
                                    PXA_UI_PROPERTY_BACKGROUND,
                                    PXA_UI_THEME_PRIMARY) &&
             pxa_ui_create(&transaction, 13, PXA_UI_DEMO_PAGE_NODE_BUTTON, 0,
                           PXA_UI_NODE_TEXT) &&
             pxa_ui_set_text(&transaction, 13, page->action,
                             pxa_ui_demo_string_length(page->action)) &&
             pxa_ui_set_theme_color(&transaction, 13,
                                    PXA_UI_PROPERTY_FOREGROUND,
                                    PXA_UI_THEME_ON_PRIMARY);
    }
    if (!ok || !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active)
            (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    *generation = next;
    return 1;
}

static inline int pxa_ui_demo_page_render(uint32_t* generation,
                                          uint8_t* scratch,
                                          size_t scratch_capacity,
                                          const pxa_ui_demo_page_t* page) {
    return pxa_ui_demo_page_render_with_root_event_mask(
        generation, scratch, scratch_capacity, page, 0);
}

#endif
