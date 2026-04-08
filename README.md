# 🎣 Fisherman

A fully playable fishing game that runs in the terminal, built in C.

## About
You control a boat moving left and right across the screen. Press space to cast your hook and catch fish swimming below the surface. Different fish are worth different points — small fish are worth 1 point, medium 3, and big ones 7.

## Features
- Animated fish of three different sizes and speeds
- Controllable boat and hook mechanic
- Three difficulty levels — Easy, Medium, Hard
- Level progression system
- High score saved to disk between sessions
- Colored terminal graphics using ncurses

## How to run
```bash
gcc fisherman.c -o fisherman -lncurses
./fisherman
```

## Controls
| Key | Action |
|-----|--------|
| ← → | Move boat |
| Space | Cast hook |
| Q | Quit |

## Built with
- C
- ncurses
- Linux terminal
