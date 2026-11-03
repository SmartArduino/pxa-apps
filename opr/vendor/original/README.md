# **Open Procedural Roguelite**

A tiny, self‑contained roguelike engine written in C.  
This library implements the full game logic for a classic Roguelite:  
floor generation, fog‑of‑war, movement, combat, loot, progression, and victory/game‑over states.

It is designed to be:

- **Portable** — ANSI C, no external dependencies  
- **Deterministic** — predictable behavior across platforms  
- **Embeddable** — works in native apps, games, and WebAssembly  
- **Modular** — each subsystem lives in its own file  
- **Minimal** — the entire engine fits in a few hundred lines

You can embed it in:

- C / C++ games  
- SDL2 / Raylib / OpenGL engines  
- WebAssembly (Emscripten)  
- Game engines like Phaser, Godot (via WASM), Unity (via native plugin)

---

## **Features**

- Procedural dungeon generation  
- Fog‑of‑war and visited tiles  
- Player classes (Warrior, Rogue, Mage)  
- Enemies, bosses, chests, potions, shrines, stairs  
- Turn‑based combat  
- Leveling system  
- Floor progression  
- Victory and game‑over states  
- Clean API for embedding in any frontend

---

## **Project Structure**

```
include/
    opr.h              # Public API

src/
    opr_init.c         # Game initialization
    opr_floor.c        # Floor generation
    opr_move.c         # Movement & interactions
    opr_combat.c       # Combat system
    opr_interact.c     # Chest & shrine logic
    opr_progress.c     # Leveling, descent, victory/gameover

tests/
    ...                # Unity-based unit tests
```

---

# **Public API**

All public functions are declared in `include/opr.h`.

---

## **Initialization**

### **`void opr_init(OPR_GameState *g, int classId);`**

Initializes the entire game state:

- Resets player stats based on class  
- Clears map and fog  
- Clears entities  
- Generates the first floor  
- Resets combat and flags

---

## **Floor Generation**

### **`void opr_generate_floor(OPR_GameState *g);`**

Creates a new dungeon floor:

- Random rooms  
- Corridors  
- Player spawn  
- Stairs or boss  
- Enemies  
- Chests  
- Potions  
- Shrines  
- Fog reveal around player

Called automatically by `opr_init` and `opr_descend`.

---

## **Movement & Interaction**

### **`void opr_move(OPR_GameState *g, int dx, int dy);`**

Attempts to move the player by `(dx, dy)`:

- Blocks movement into walls  
- Starts combat when stepping onto an enemy  
- Opens chests  
- Picks up potions  
- Uses shrines  
- Descends stairs  
- Reveals fog around the player

---

## **Combat System**

### **`void opr_combat_attack(OPR_GameState *g);`**

Player attacks the enemy:

- Deals damage  
- Enemy counterattacks if alive  
- Awards XP on kill  
- Removes enemy from entity list  
- Checks level‑up  
- Triggers victory if boss dies  
- Triggers game‑over if player dies

---

### **`void opr_combat_potion(OPR_GameState *g);`**

Player uses a potion:

- Heals based on level  
- Enemy counterattacks  
- Can trigger game‑over

---

### **`void opr_combat_flee(OPR_GameState *g);`**

Attempts to flee combat:

- Success chance based on player speed  
- Failure causes enemy attack  
- Can trigger game‑over

---

## **Progression**

### **`void opr_descend(OPR_GameState *g);`**

Moves player to the next floor:

- Heals player  
- Generates new floor  
- Resets fog  
- Updates floor counter

---

### **`void opr_check_levelup(OPR_GameState *g);`**

Checks if player XP exceeds threshold:

- Increases level  
- Raises stats  
- Fully heals  
- Increases next XP requirement

---

### **`void opr_trigger_victory(OPR_GameState *g);`**

Sets victory flag and ends combat.

---

### **`void opr_trigger_gameover(OPR_GameState *g);`**

Sets game‑over flag and ends combat.

---

# **Data Structures**

### **`OPR_Player`**

Contains player stats:

- Position  
- HP / Max HP  
- ATK / DEF / SPD  
- Level / XP / XPNext  
- Weapon / Armor bonuses  
- Potion count  
- Current floor  
- Class ID

---

### **`OPR_Entity`**

Represents enemies, chests, potions, shrines, stairs, bosses.

Fields:

- Position  
- Type  
- HP / Max HP  
- ATK / DEF  
- XP reward  
- Tier

---

### **`OPR_Map`**

Contains:

- `tiles[ROWS][COLS]` — floor/wall  
- `fog[ROWS][COLS]` — hidden tiles  
- `visited[ROWS][COLS]` — explored tiles

---

### **`OPR_GameState`**

The full game state:

- Player  
- Map  
- Entities  
- Entity count  
- Combat flags  
- Victory / Game‑over flags

---

# **Embedding Example (Native C)**

```c
#include "opr.h"

int main() {
    OPR_GameState g;
    opr_init(&g, 0); // Warrior

    opr_move(&g, 1, 0);
    if (g.inCombat) {
        opr_combat_attack(&g);
    }

    return 0;
}
```

---

# **Embedding Example (WebAssembly)**

Using Emscripten:

```c
void opr_web_init(int classId) { opr_init(&G, classId); }
OPR_GameState* opr_web_get_state() { return &G; }
void opr_web_move(int dx, int dy) { opr_move(&G, dx, dy); }
void opr_web_attack() { opr_combat_attack(&G); }
void opr_web_potion() { opr_combat_potion(&G); }
void opr_web_flee() { opr_combat_flee(&G); }
```

Then in JavaScript:

```js
Module().then(mod => {
  const init = mod.cwrap("opr_web_init", null, ["number"]);
  const move = mod.cwrap("opr_web_move", null, ["number","number"]);
});
```

---

# **License**

MIT License (recommended for game engines).  
You can choose another license if you prefer.

---

# **Contributing**

Pull requests are welcome!

Areas where contributions are especially helpful:

- Additional player classes  
- More enemy types  
- Expanded loot tables  
- Alternative dungeon generators  
- WASM bindings  
- SDL2 / Raylib demo frontends  
- Documentation improvements  
