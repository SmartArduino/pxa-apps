// MIT. Includes nested throws and a throw across the native toString bridge.
let result = 0
try {
    try { throw 7 } catch (e) { result += 1; throw 9 }
} catch (e) { result += 2 }
class Bomb {
    toString(): string { throw "conversion" }
}
try {
    let text = "value=" + new Bomb()
    result = 100
} catch (e) { result += 4 }
scene.setBackgroundColor(result == 7 ? 7 : 2)
