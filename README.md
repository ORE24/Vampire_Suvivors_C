# Terminal Survivors: C MVP

A Vampire Survivors-style MVP for a terminal-only environment. It uses ANSI
escape sequences, POSIX terminal input, and the terminal bell for sound events.
There is no raylib, SDL, ncurses, or graphics window dependency.

## Build

```sh
make
```

## Run

```sh
make run
```

## Controls

- Move: `WASD` or arrow keys
- Start: `S` or `Enter`
- Ranking screen: `R` on the title screen
- Level-up choice: `1`, `2`, `3`, arrow keys plus `Enter`, or `Space`
- Restart after run: `R`
- Back to title after run/ranking: `B` or `Esc`
- Toggle sound: `M`
- Quit: `Q`

## MVP Rules

- Clear win condition: survive for 10 minutes.
- Clear lose condition: HP reaches 0.
- One map: a compact crypt arena with walls and tombstones.
- Three enemy types:
  - `b`: 1 HP
  - `G`: 3 HP
  - `V`: 40 HP
- Two automatic weapons:
  - Magic Bolt: targets the nearest enemy and fires `*` projectiles.
  - Holy Aura: periodically damages enemies around the player.
- Enemies drop `+` experience. Pickups are pulled toward the player in range.
- Level-ups pause the game and offer three upgrade choices.
- Rankings are saved to `scores.txt` and shown from the title screen.

## Terminal Sound

Attack, XP pickup, level-up, player hit, game-over, and victory events emit the
terminal bell (`\a`). Some terminals mute bell sounds by default; press `M` to
toggle sounds while running.
