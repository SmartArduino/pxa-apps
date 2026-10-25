# smallchesslib

*A tiny public domain suckless chess library and engine for the benefit of all living beings.*

Smallchesslib is a small/suckless/KISS single header chess library that comes
with functions for playing the game, manipulating standard chess formats, and
even comes with a basic AI. The repository also includes a simple chess engine
built with the library.

The philosophy here is not to be the best, the fastest, to offer the strongest
AI or completely comply with all the standards, but rather to offer something
simple and very portable to people who want to create simple chess software.
There are many strong engines and complex libraries, but few that try to keep it
simple and portable in pursuit of understandability, maximum freedom and
friendliness to smaller projects. The library is completely in the public domain
under CC0 so as to serve everyone equally. This isn't made for any profit but
purely for the benefit of everyone.

```
     A B C D E F G H
  8 ▒♜░♞▒▒[][♛░░▒♚░♝
  7 ░♟▒♝░♟▒♟░░▒♟░░▒♟
  6 ▒▒░♟▒▒░░▒▒░░▒♟░░
  5 ░░▒▒░░▒♙░░▒♜░░▒▒
  4 ▒♙░░▒♗░░▒▒░♙▒▒░░
  3 ░░▒♙░♙▒▒░♗▒▒░♙▒♙
  2 ▒▒░░▒▒░░▒♘░░▒♕░░
  1 ░♖▒▒░░▒▒░♔▒▒░░▒♖

black played d8e8
white to move

ply number: 34
FEN: rn2q1kb/pbpp1p1p/1p4p1/3P1r2/P1B2P2/1PP1B1PP/4N1Q1/R3K2R w kq - 1 18
board static evaluation: -0.125000 (-32)
board hash: 461096787
phase: midgame
en passant: 0
50 move rule count: 1
PGN: 1. h3 b6 2. e4 e5 3. Nc3 Nf6 4. b3 Bb4 5. Nd5 Nxd5 6. exd5 O-O 7. c3 Bd6 8. d4 Re8 9. Be3 Bb7 10. dxe5 Rxe5 11. Bc4 Qh4 12. Qf3 g6 13. g3 Qd8 14. Ne2 Rf5 15. Qg2 Be5 16. f4 Bh8 17. a4 Qe8*
```

## Features

- simple, **suckless**, **KISS**
- **single header** library
- pure **C99**, **well commented** source code
- completely **public domain free software** under **CC0**, owned by no one, zero conditions on use
- **no dependencies** (not even standard library, except for stdint)
- **no build system**, just compile
- basic functions: computing moves, estimating game phases, ...
- uses **no floating point**, mostly just 8 bit integer math
- uses **no dynamic allocation** on heap
- included simple CLI **engine**, *smolchess*, with (partial) **xboard** support (tested with xboard, pychess and lichess bot)
- very **efficient**, runs even on tiny **embedded** and **bare metal** systems such as Arduboy
  (2.5 KB RAM, 15 MHz CPU Arduino) and Pokitto (32 KB RAM, 48 MHz).
- printing chessboard in several formats (ASCII, UTF8, ...)
- simple internal board representation with ASCII characters, easy to manipulate
- **FEN**
- **PGN**
- **game recording**
- move **undos**
- simple **graphical rendering of the board** (48 x 48 1bit image)
- recursive position evaluation/analysis
- simple **psudorandom number generators**
- **chess 960** variant (Fischer random): starting position numbering/generation, compile-time option for 960 castling
- **board hashing**
- simple **AI** (traditional state tree search, no machine learning):
  - **rating about 1500+**: played on lichess under the nickname [smolchessbot](https://lichess.org/@/smolchessbot) (on Intel Core 2 Duo @2.66GHz, with base search depth 3)
  - evaluation function considering many aspects (material, pawn structure, king
    position, mobility, ...)
  - **alpha-beta pruning**
  - two kinds of extensions (extra search depth): exchange and check
- simple support for some variants (that only differ from chess by starting position)

## Limitations

- focused on traditional chess and chess960, very little support for variants with wildly different rules
- no UCI (only xboard)
- xboard support isn't full, it can't be used for analysis, just playing
- AI doesn't consider draw by 50 move rule and is only able to consider repetition draw in a limited way
- AI doesn't use any opening book or endgame tablebase
- AI always promotes to queen
- even though the effect is minized, AI still ocassionaly suffers from the horizon effect
- no transposition tables
- no move ordering during search
- no iterative deepening, time management is pretty primitive
- for memory reasons, draw by repetition only considers consecutive moves by default
- some advanced features like pondering are not implemented
- the library doesn't perform extensive checks and isn't super memory safe, don't give it garbage

## How To Use

Simply include `smallchesslib.h`, all code is in this one file, no need for linking. All functions and
other resources are also documented in this file, so take a look at it to see the library's API. For a
typical use you want to focus on the `SCL_Game` struct and its associated functions starting with
`SCL_game*`.

## Usage Rights

**tl;dr: everything in this repository is CC0 + a waiver of all rights, completely public domain as much as humanly possible, do absolutely anything you want**

I, Miloslav Číž (drummyfish), have created everything in this repository.

This work's goal is to never be encumbered by any exclusive intellectual property rights, it is intended to always stay completely and forever in the public domain, available for any use whatsoever.

I therefore release everything in this repository under CC0 1.0 (public domain, https://creativecommons.org/publicdomain/zero/1.0/) + a waiver of all other IP rights (including patents), which is as follows:

*Each contributor to this work agrees that they waive any exclusive rights, including but not limited to copyright, patents, trademark, trade dress, industrial design, plant varieties and trade secrets, to any and all ideas, concepts, processes, discoveries, improvements and inventions conceived, discovered, made, designed, researched or developed by the contributor either solely or jointly with others, which relate to this work or result from this work. Should any waiver of such right be judged legally invalid or ineffective under applicable law, the contributor hereby grants to each affected person a royalty-free, non transferable, non sublicensable, non exclusive, irrevocable and unconditional license to this right.*

I would like to ask you, without it being any requirement at all, to please support free software and free culture by sharing at least some of your own work in a similar way I do with this project.

If you'd like to support me or just read something about me and my projects, visit my site: [www.tastyfish.cz](http://www.tastyfish.cz/).
