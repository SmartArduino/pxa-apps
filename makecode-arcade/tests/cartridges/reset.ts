// MIT. Game-over restart keeps settings and drops the previous VM state.
if (settings.exists("reset-probe")) {
    scene.setBackgroundColor(settings.readNumber("reset-probe") == 42 ? 7 : 2)
} else {
    settings.writeNumber("reset-probe", 42)
    control.reset()
}
