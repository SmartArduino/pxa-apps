/* GPL-2.0-or-later. Shared by the GB, NES and multi-console frontends.
 * Include after frontend_reset_video() and frontend_display_canvas() exist. */
static pxa_ui_environment_t display_environment;
static unsigned display_surface_pending;
static int request_display_surface(void) {
    uint8_t bytes[128];uint32_t n;
    pxa_surface_desc_t desc={.width=width,.height=height,.format=PXA_SURFACE_RGB565,.buffer_count=2,
        .flags=PXA_SURFACE_GUEST_MAPPED|PXA_SURFACE_PREFER_DIRECT_SCANOUT};
    if(!pxa_surface_build_create(bytes,sizeof(bytes),T_SURFACE,desc,&n))return PXA_STATUS_INTERNAL;
    int status=pxa_control_packet(bytes,n);
    display_surface_pending=status==0;
    return status;
}
static int resize_display(const pxa_ui_environment_t *env) {
    unsigned w=env->width,h=env->height;
    if(!frontend_display_canvas(&w,&h) || (w==width && h==height))return 0;
    if(surface) {int status=pxa_close_handle(surface);if(status)return status;surface=0;}
    width=w;height=h;
    input_density_q16=env->density_q16?env->density_q16:65536;
    menu_environment(env);release_input();frontend_reset_video();update_locale();redraw=1;
    int status=request_display_surface();
    return status?status:1;
}
static int display_environment_changed(const pxa_ui_environment_t *env) {
    display_environment=*env;
    // Finish a creation before reusing its token, dimensions or mapped buffers.
    if(display_surface_pending)return 1;
    return resize_display(env);
}
static int display_surface_created(uint64_t handle) {
    surface=handle;display_surface_pending=0;
    int resized=resize_display(&display_environment);
    if(!resized) {
        input_density_q16=display_environment.density_q16?display_environment.density_q16:65536;
        menu_environment(&display_environment);release_input();frontend_reset_video();update_locale();
    }
    return resized;
}
