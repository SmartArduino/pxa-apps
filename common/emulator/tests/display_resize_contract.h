/* GPL-2.0-or-later. Run against the actual GB/NES/shared frontend state. */
static void test_display_resize_contract(void) {
    surface=0;display_surface_pending=0;width=240;height=320;
    pxa_ui_environment_t env={.width=240,.height=320,.density_q16=65536};
    display_environment=env;menu_environment(&env);frontend_reset_video();
    controls.controller_hidden=1;
    assert(!request_display_surface() && display_surface_pending);
    // Rotation and another size change arrive before the first create reply.
    env.width=320;env.height=240;
    assert(display_environment_changed(&env)==1 && width==240 && height==320);
    env.width=854;env.height=480;
    assert(display_environment_changed(&env)==1 && width==240 && height==320);
    assert(display_surface_created(123)==1 && width==854 && height==480 && display_surface_pending);
    assert(controls.controller_hidden && video_renderer.width==854 && video_renderer.height==480);
    assert(display_surface_created(124)==0 && !display_surface_pending);
    assert(display_environment_changed(&env)==0);
    env.width=env.height=480;
    assert(display_environment_changed(&env)==1 && width==480 && height==480);
    assert(display_surface_created(125)==0);
    env.width=240;env.height=320;
    assert(display_environment_changed(&env)==1 && width==240 && height==320);
    assert(display_surface_created(126)==0);
    assert(controls.controller_hidden && video_renderer.width==240 && video_renderer.height==320);
    assert(!pxa_close_handle(surface));surface=0;
    puts("PASS: portrait/landscape/square resize, delayed creates, repeated metrics and preserved control visibility");
}
