// MIT. Runnable fibers progress together, while bytecode loops are preempted.
let updates = 0
game.onUpdate(function () { updates++ })
control.runInParallel(function () {
    let total = 0
    for (let i = 0; i < 1000000; i++) total += i & 1
    scene.setBackgroundColor(total == 500000 && updates > 2 ? 7 : 2)
})
