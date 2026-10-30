// SPDX-License-Identifier: CC0-1.0
// PXA's layer size must match its surface size. Draw in logical coordinates,
// resample into bounded output buffers, and let the Host apply only its exact
// integer 1x/2x/4x scale. All origins below are physical display coordinates.
pocket_ui::Viewport viewport;
#ifdef PXA_POCKET_FILL
pocket_ui::Viewport content_viewport;
#endif
uint32_t frame_bytes;
bool surface_inflight;
int surface_display_w, surface_display_h;
int request_surface() {
#ifdef PXA_POCKET_UTILITY
    if(display_h>display_w){WIDTH=240;HEIGHT=std::clamp(display_h*240/std::max(1,display_w),260,360);}
    else{WIDTH=288;HEIGHT=224;}
#endif
    surface_ready = false; prepared = false; staged = false;
    if (surface) {
        int32_t status = pxa_close_handle(surface);
        if (status) return status;
        surface = 0;
    }
#ifdef PXA_POCKET_FILL
    viewport = pocket_ui::fill_viewport(display_w, display_h);
    content_viewport = viewport;
#ifdef PXA_POCKET_STUDIO
    uint32_t radius = 0; for (uint32_t r : studio_corners) radius = std::max(radius, r);
    int corner = int((uint64_t(radius) * 293 + 999) / 1000);
    int edges[4]; for (int i = 0; i < 4; ++i)
        edges[i] = (std::max(corner, int(studio_safe[i])) + viewport.host_scale - 1) / viewport.host_scale;
    if (edges[1] + edges[3] < viewport.width - 32 && edges[0] + edges[2] < viewport.height - 32) {
        content_viewport.x += edges[3] * viewport.host_scale;
        content_viewport.y += edges[0] * viewport.host_scale;
        content_viewport.width -= edges[1] + edges[3];
        content_viewport.height -= edges[0] + edges[2];
    }
#endif
    // Minimum readable layout, preserving the actual available aspect ratio.
    const int cw = content_viewport.width, ch = content_viewport.height;
    if (cw <= ch) {
#ifdef PXA_POCKET_UTILITY
        WIDTH = 240; HEIGHT = (ch * WIDTH + cw / 2) / cw;
        if (HEIGHT < 284) { HEIGHT = 284; WIDTH = (cw * HEIGHT + ch / 2) / ch; }
#else
        WIDTH = 288; HEIGHT = (ch * WIDTH + cw / 2) / cw;
#endif
    } else {
        HEIGHT = 224; WIDTH = (cw * HEIGHT + ch / 2) / ch;
        if (WIDTH < 288) { WIDTH = 288; HEIGHT = (ch * WIDTH + cw / 2) / cw; }
    }
#else
    viewport = pocket_ui::fit_viewport(display_w, display_h, WIDTH, HEIGHT);
#ifdef PXA_POCKET_STUDIO
    // Keep the complete board and controls inside rounded corners/safe insets.
    // R*(1-1/sqrt(2)) is the diagonal inset of a quarter-circle corner.
    uint32_t radius=0;for(uint32_t r:studio_corners)if(r>radius)radius=r;
    int corner=int((uint64_t(radius)*293+999)/1000);
    int top=std::max(corner,int(studio_safe[0])),right=std::max(corner,int(studio_safe[1]));
    int bottom=std::max(corner,int(studio_safe[2])),left=std::max(corner,int(studio_safe[3]));
    if(left+right<display_w-32&&top+bottom<display_h-32){
        viewport=pocket_ui::fit_viewport(display_w-left-right,display_h-top-bottom,WIDTH,HEIGHT);
        viewport.x+=left;viewport.y+=top;
    }
#endif
#endif
    surface_display_w = display_w; surface_display_h = display_h;
    frame_bytes = viewport.pixels()*2;
    frame_id = 0;
    pxa_surface_desc_t desc{uint16_t(viewport.width), uint16_t(viewport.height), PXA_SURFACE_RGB565, 2,
        uint8_t(PXA_SURFACE_PREFER_DIRECT_SCANOUT | (mapped ? PXA_SURFACE_GUEST_MAPPED : 0))};
    uint32_t n;
    if (!pxa_surface_build_create(packet, sizeof(packet), CREATE, desc, &n)) return PXA_STATUS_INTERNAL;
    int32_t status = pxa_control_packet(packet, n);
    surface_inflight = status == 0;
    return status;
}
int32_t handle_surface(const pxa_event_t &e) {
    if (e.service != PXA_SURFACE_SERVICE) return PXA_EVENT_UNHANDLED;
    if (e.token == CREATE) {
        pxa_surface_create_result_t r;
        if (!pxa_surface_parse_create(&e, CREATE, &r)) return PXA_STATUS_INTERNAL;
        if (r.status == PXA_STATUS_UNSUPPORTED && mapped) { mapped = false; return request_surface(); }
        if (r.status || r.stride_bytes != uint32_t(viewport.width*2) || r.frame_bytes != frame_bytes || r.buffer_count != 2)
            return PXA_STATUS_INTERNAL;
        surface = r.handle;
        if (mapped && pxa_surface_register_buffers(surface, frames, frame_bytes, 2) != int32_t(frame_bytes*2))
            return PXA_STATUS_INTERNAL;
        uint32_t n;
        if (!pxa_surface_build_configure_layer(packet, sizeof(packet), CONFIGURE, surface,
            viewport.x, viewport.y, uint16_t(viewport.width), uint16_t(viewport.height), 0, 1, &n)) return PXA_STATUS_INTERNAL;
        int32_t status = pxa_control_packet(packet, n);
        return status ? status : PXA_EVENT_HANDLED;
    }
    if (e.token == CONFIGURE) {
        int32_t status;
        if (!pxa_surface_parse_status(&e, CONFIGURE, PXA_SURFACE_CONFIGURE_LAYER, &status) || status)
            return PXA_STATUS_INTERNAL;
        surface_inflight = false;
        // Resize notifications may arrive during asynchronous creation. Finish
        // the outstanding exchange before reusing its token and buffer memory.
        if (surface_display_w != display_w || surface_display_h != display_h) {
            status = request_surface(); return status ? status : PXA_EVENT_HANDLED;
        }
        surface_ready = true; pocket::redraw();
        char message[176];
        const auto &layout =
#ifdef PXA_POCKET_FILL
            content_viewport;
#else
            viewport;
#endif
        snprintf(message, sizeof(message), "Pocket: viewport canvas=%dx%d origin=%d,%d size=%dx%d display=%dx%d",
            WIDTH, HEIGHT, layout.x, layout.y, layout.physical_width(), layout.physical_height(), display_w, display_h);
        pxa_log_write(2, message);
#ifdef PXA_POCKET_FILL
        snprintf(message, sizeof(message), "Pocket: fullscreen origin=%d,%d size=%dx%d display=%dx%d",
            viewport.x, viewport.y, viewport.physical_width(), viewport.physical_height(), display_w, display_h);
        pxa_log_write(2, message);
#endif
        return PXA_EVENT_HANDLED;
    }
    return PXA_EVENT_UNHANDLED;
}
