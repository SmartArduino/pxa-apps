# arduboy-games

Tiny games for the [Arduboy](https://www.arduboy.com)

## Minesweeper

Minimalistic implementation, flashes red LED on lose, green LED on win

Controls:

- `UP`, `DOWN`, `RIGHT`, `LEFT` buttons to navigate the field
- `A` button to: reveal a tile if not revealed, reveal closest non-flagged neighbors (helps speed up opening the non-mined tiles)
- `B` button to toggle flag on a tile (mark tile as mined)

.hex file is available in [/minesweeper/binaries/](/minesweeper/binaries/), you can try it [on the emulator](https://felipemanga.github.io/ProjectABE/?url=https://raw.githubusercontent.com/SuperSolik/arduboy-games/master/minesweeper/binaries/minesweeper.hex)

<img width="300" alt="logo" src="https://user-images.githubusercontent.com/31539612/213283596-319814df-716a-457f-b5c2-df04d42eaf46.png">
<img width="300" alt="gameplay" src="https://user-images.githubusercontent.com/31539612/213283593-5d612723-06ec-4a56-8c8c-cc5f7f644a24.png">

## Dot Arduboy Edition

Arduboy adaptation of the core gameplay of the IOS game I recently found ([App Store link](https://apps.apple.com/us/app/dot-the-game/id1634392900)), originally created by the https://twitter.com/OuzzGame, all credits for the game idea/concept goes to them, I've adapted the gameplay for the Arduboy merely for fun

Similar to Minesweeper, but with different "cell" layout, and instead of bombs the player should reveal the dot pattern

title screen:
- `A` or `B` buttons to difficulty screen
- `UP` and `DOWN` buttons simultaneously to reset the score

difficulty screen:
- `UP` and `DOWN` buttons to choose between difficulties
- `A` button to select difficulty

gameplay:
- `UP`, `DOWN`, `RIGHT`, `LEFT` buttons to navigate
- `A` button to toggle dot

When all of the dots will be opened in such a way that it satisfies the dot counts (represented as numbers), the game will show the overall score (persistently saved to EEPROM)

.hex file is available in the [/guess_dots/binaries](/guess_dots/binaries/), also playable [on emulator](https://felipemanga.github.io/ProjectABE/?url=https://raw.githubusercontent.com/SuperSolik/arduboy-games/master/guess_dots/binaries/guess_dots.hex)

<img width="300" alt="guess dots gameplay easy & medium" src="https://user-images.githubusercontent.com/31539612/215164272-23bcb568-a8ea-41fd-b0de-c066c448a99a.gif" >
<img width="300" alt="guess dots gameplay hard" src="https://user-images.githubusercontent.com/31539612/215164860-926f271f-4136-46b5-a2bb-f0b211cfd23d.gif" >

## Dash Arduboy Edition

Sidescroller, inspired by the famous "Geometry Dash" game. The obstacle batches are continuously generated, so the gameplay is endless.

In the top left corner the first number represents the overall max score, second number represents the current score. The max score is persistent in the EEPROM.

title screen:
- `A` buttons to start the game
- `UP` and `DOWN` buttons simultaneously to reset the max score

gameplay:
- `UP` or `A` or `B` button to jump
- `DOWN` to pause

.hex file is available in the [/ardu_dash/binaries](/ardu_dash/binaries/), also playable [on emulator](https://felipemanga.github.io/ProjectABE/?url=https://raw.githubusercontent.com/SuperSolik/arduboy-games/master/ardu_dash/binaries/ardu_dash.hex)

![ardy_dash_clip_1](https://user-images.githubusercontent.com/31539612/222920794-01c06409-8002-4c3c-9d0e-980b92dcfc10.gif)

![ardu_dash_clip_2](https://user-images.githubusercontent.com/31539612/222920796-e084e97f-9958-4f99-9910-684c2358b4c8.gif)

## Rotating 3D cube demo

Classic rotating 3D cube demo, rotation is controlled by the `UP`, `DOWN`, `RIGHT`, `LEFT` buttons

<img width="300" alt="rotating cube" src="https://user-images.githubusercontent.com/31539612/213282958-d6de1cda-13d0-43b0-b172-a82ecb3a8aec.gif" >
