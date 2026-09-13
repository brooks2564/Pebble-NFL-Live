# NFL Live Scores - Claude Code Instructions

## Project Overview
**NFL Live Scores** is a Pebble smartwatch **watchface** that displays live NFL scores, quarter/clock, down & distance, a field-position graphic, team passing/rushing/turnover stats, and a scrolling ticker of all games this week.
UUID: `b13118dd-472d-4ce5-a65b-5c8515fcb02d`
GitHub: `brooks2564/Pebble-NFL-Live` (private)
Data: ESPN's public scoreboard/summary API (free, no key - site.api.espn.com)

## Build & Install
Always rebuild, update the committed PBW, and push together:
```bash
pebble build
cp build/Pebble-NFL-Live.pbw Pebble-NFL-Live.pbw
pebble install --phone 192.168.0.120
git add Pebble-NFL-Live.pbw
git commit -m "Update PBW"
git push
```

## Project Structure
```
Pebble-NFL-Live/
├── package.json              <- Pebble manifest, 5 platforms, 31 message keys, 128 logo resources
├── wscript                   <- Build script
├── CLAUDE.md                 <- This file
├── README.md                 <- App store / GitHub description
├── Pebble-NFL-Live.pbw       <- Latest compiled binary (for store upload)
├── screenshots/
│   ├── banner.png            <- 720x320 app store banner (football through goal post, green field)
│   └── icon_25.png           <- 25x25 Pebble menu icon (unmodified pebble-dev football icon)
├── resources/images/
│   ├── menu_icon.png         <- Watch menu icon (same football icon, embedded resource)
│   ├── FOOTBALL_EM.png       <- Field-bar ball marker, emery (20x20, brown/black/white, flat colors)
│   ├── FOOTBALL_BA.png       <- Field-bar ball marker, aplite/basalt/diorite/flint (14x14)
│   └── logos/                <- 32 teams x {EM,BA} x {LG,SM} = 128 real team logo PNGs,
│                                 pre-flattened onto opaque black squares
├── src/
│   ├── c/main.c              <- Watchface C code
│   └── pkjs/
│       ├── index.js          <- PebbleKit JS (ESPN fetch, play-text parsing, ticker/stats build)
│       └── config.json       <- Clay settings schema
```

## Target Platforms (5 - no round support)
- aplite  - Pebble / Pebble Steel (B&W, 144x168)
- basalt  - Pebble Time / Time Steel (color, 144x168)
- diorite - Pebble 2 SE/HR (B&W, 144x168)
- emery   - Pebble Time 2 (color, 200x228) - **primary target, gets extra polish**
- flint   - Pebble 2 Duo (B&W, 144x168)

Chalk/gabbro (round) were never added. Several visual features are **emery-only** (see below) because the smaller screens don't have room, not because of a platform bug.

## Message Keys (must match #define KEY_* in main.c)
| Key              | ID | Direction   |
|------------------|----|-------------|
| AWAY_ABBR        | 1  | JS -> Watch |
| HOME_ABBR        | 2  | JS -> Watch |
| AWAY_SCORE       | 3  | JS -> Watch |
| HOME_SCORE       | 4  | JS -> Watch |
| QUARTER          | 5  | JS -> Watch |
| CLOCK            | 6  | JS -> Watch |
| DOWN             | 7  | JS -> Watch |
| DISTANCE         | 8  | JS -> Watch |
| FIELD_POS        | 9  | JS -> Watch |
| REDZONE          | 10 | JS -> Watch |
| POSSESSION       | 11 | JS -> Watch |
| STATUS           | 12 | JS -> Watch |
| TEAM_IDX         | 13 | Both        |
| START_TIME       | 14 | JS -> Watch |
| AWAY_RECORD      | 15 | JS -> Watch |
| HOME_RECORD      | 16 | JS -> Watch |
| VIBRATE          | 17 | JS -> Watch |
| LAST_PLAY        | 18 | JS -> Watch |
| NEXT_GAME        | 19 | JS -> Watch |
| BATTERY_BAR      | 20 | JS -> Watch |
| TICKER           | 21 | JS -> Watch |
| DOWN_TEXT        | 22 | JS -> Watch |
| SCORE_EVENT      | 23 | JS -> Watch |
| NETWORK          | 24 | JS -> Watch |
| FEATURED_TAG     | 25 | JS -> Watch |
| TEAM_LOGOS       | 26 | JS -> Watch (settings only, Clay-sent) |
| TICKER_SPEED     | 27 | JS -> Watch (settings only, Clay auto-sends, not in index.js) |
| AWAY_STATS       | 28 | JS -> Watch |
| HOME_STATS       | 29 | JS -> Watch |
| AWAY_TURNOVERS   | 30 | JS -> Watch |
| HOME_TURNOVERS   | 31 | JS -> Watch |

## Key Architecture Details
- **Watchface** (not watchapp) - no button handling, no menu
- **MINUTE_UNIT tick** triggers `request_game_data()` every minute
- **AppMessage inbox** is 512 bytes; outbox is 64 bytes; main message + TICKER split across two sends
- **STATUS values**: `"live"`, `"pre"`, `"final"`, `"off"` (off = bye week or fetch error)
- **Team-tracking priority**: live > pre-game starting within 2hrs > today's final > next scheduled
- **Primetime auto-featuring**: on Thu/Sun/Mon, if your own team isn't live, automatically shows
  that night's TNF/SNF/MNF game instead (matched by broadcast network, falls back to latest
  kickoff that day). Reverts on its own once your team goes live or the primetime game ends.
  A yellow "SNF"/"MNF"/"TNF" tag folds into the status line when this is active. Toggle:
  Settings > Primetime > Auto-Show Primetime Games.
- **Team logos** - real ESPN logos (`a.espncdn.com/i/teamlogos/nfl/500/{abbr}.png`), cropped to
  their actual artwork bounding box before scaling (ESPN's source canvases have wildly
  inconsistent padding - skipping the crop makes some teams' logos tiny and others oversized).
  Emery gets 36px/22px (LG/SM) logos, basalt gets 26px/16px; aplite/diorite/flint have no logo
  resources and always fall back to colored abbreviation text. Toggle: Settings > Team Logos.
- **Field position bar** (`draw_field_bar`, emery gets the full version):
  - 0-100 scale, 0 = away team's own goal line, 100 = home team's own goal line
  - Endzones filled in each team's real color (emery only; generic black elsewhere)
  - Endzone abbreviation text is emery-only (smaller screens can't fit 3 letters without
    truncating - deliberately dropped there rather than showing "D...")
  - **Layer stacking order (bottom to top)**: green field -> white yard/goal lines
    (full sideline-to-sideline, never extending past the bar) -> red or white outline
    (red = REDZONE true) -> endzone text / separator content -> yard-number labels
    ("20"/"50"/"20") -> football icon + direction arrow, which is **always drawn last**
    so it's on top of everything, unconditionally
  - Ball marker is the pebble-dev American football icon (brown/black/white, flattened to
    exactly 3 flat colors - anti-aliased edges from the original SVG resize degenerate to
    gray on Pebble's quantized palette, so the source PNGs are pre-quantized, not just resized)
  - Direction arrow (">"/"<") shows which endzone the possessing team is driving toward
- **Passing/rushing/turnover stats** - pulled from the same ESPN summary/boxscore fetch already
  used for scoring plays (no extra request). Shown directly under the score row (the space
  that's otherwise blank during a live game, since records only show pre/final): "P:X R:X" line,
  then "INT:X FUMB:X" on the line below it (emery only - no vertical room on smaller screens).
- **Last play formatting** - ESPN's raw play-by-play text is parsed down to just the relevant
  player + yardage (`formatLastPlay`/`extractPlayerYards` in index.js): drops the QB's name on
  completions (shows receiver only) but keeps whoever's name is in the rusher slot as-is, so a
  scrambling QB still shows correctly. Falls back to the raw (truncated) text when the wording
  doesn't match the expected pattern closely enough to trust (trick plays, laterals, etc.) -
  never shows garbled output. This last-play line is **always visible** during live games (no
  toggling/cycling).
- **Vibration** - via `SCORE_EVENT`: 1 = single short buzz (FG or safety), 2 = triple buzz (TD),
  0 = no buzz. Computed in JS from new entries in `scoringPlays` since the last check (mirrors
  the MLB app's HR-tracker pattern). PATs/2-point tries never get their own scoring-play entry
  in ESPN's data, so they never produce a buzz - no special-casing needed. Only fires for your
  own tracked team, never for a featured primetime game that isn't yours.
- **Ticker** - sits directly under the time/date row, above the divider (same position as the
  MLB watchface), left-aligned, cycling through every other game this week. Speed is a Clay
  select (5000/10000/30000/60000 ms, same values as MLB) that Clay auto-sends and the watch
  applies immediately via `PERSIST_TICKER_SPEED` + timer restart - no manual JS wiring needed
  for that key.

## Settings (Clay, self-contained - no hosted HTML page)
`src/pkjs/config.json` defines the schema; `pebble-clay` (vendored in `node_modules/` and
`vendor/`, same fork as the MLB project) handles the settings webview and AppMessage send
automatically. Settings: Team (32 teams), Vibrate on Score, Battery Bar, Team Logos,
Ticker Speed, Auto-Show Primetime Games.

## CloudPebble (repebble)
Import this repo directly into CloudPebble:
```
https://cloudpebble.repebble.com/ide/import/github/brooks2564/Pebble-NFL-Live
```
cloudpebble.net is dead (redirects to Fitbit). Use cloudpebble.repebble.com instead.

## Git
```bash
git add -p
git commit -m "message"
git push   # token already embedded in remote URL
```
