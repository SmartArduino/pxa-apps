// MIT. The two controller instances must not be ORed into player one.
scene.setBackgroundColor(1)
controller.player1.A.onEvent(ControllerButtonEvent.Pressed, function () { scene.setBackgroundColor(2) })
controller.player2.A.onEvent(ControllerButtonEvent.Pressed, function () { scene.setBackgroundColor(7) })
