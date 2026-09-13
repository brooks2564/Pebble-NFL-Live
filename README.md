# NFL Live Scores for Pebble

Stuck in a meeting? Can't watch the game? Don't worry - keep the score on your wrist. NFL Live Scores updates every minute straight from ESPN's public scoreboard API, so a glance at your watch is all you need.

## Features

**Never miss the big game either** - when your team isn't playing, the watchface automatically switches to that night's Thursday/Sunday/Monday Night Football game (a small yellow SNF/MNF/TNF tag folds into the status line so it's never mistaken for your own team's game), then switches back on its own once that game ends or your team's does start. Toggle: **Auto-Show Primetime Games**.

**Team logos** - real team logos in place of 3-letter abbreviations, right in the score row. Toggle: **Team Logos** (color watches only - the B&W models fall back to colored text).

**Live game** - score, quarter & clock, down & distance, and the last play - shortened to just the name that matters and the yardage (the receiver on a completion, the ball carrier on a run), so it reads like "Boutte 12 yds" instead of a full play-by-play sentence. A quarterback scrambling for yards still shows up correctly since his name is whoever's in the rusher spot. The last play is always on screen during a live game - it doesn't cycle away.

**Team stats** - each team's passing and rushing yards ("P:220 R:95") sit right under the score, in the space that would otherwise be empty during a live game. Turnovers ("INT:2 FUMB:1") get their own line right below that.

**Field position bar** - a compact field graphic instead of a static diamond:
- End zones shaded in each team's real color
- Full-width yard lines (including the goal lines) run the whole height of the bar, never past it
- A football-shaped marker shows exactly where the ball is, with a small arrow (`>` or `<`) pointing toward whichever end zone the offense is driving
- The outline turns red the instant either team enters the red zone and back to white the instant they leave it - no animation, just an at-a-glance state change, and it's always drawn so the football, yard lines, and end zone labels stay visible on top of it
- Small "20"/"50"/"20" yard-number labels sit just below the field

**TV network** - which channel's carrying the game, shown pre-game and during play.

**Pre-game** - kickoff time, TV network, team records.

**Final** - final score, next scheduled game (including bye-week lookahead).

**Bye week** - "Bye Week" and your next opponent shown on their own lines, with both teams' real logos already up (in the score row and on the field bar) so next week's matchup is right there waiting.

**Ticker** - sits left-aligned right under the time and date, same spot as the MLB watchface, cycling through every other game happening this week. Toggle the pace with **Ticker Speed** (5 / 10 / 30 / 60 seconds).

**Notifications** - feel it before you see it: 1 short buzz for a field goal or safety, 3 short buzzes for a touchdown; no buzz for extra points or two-point tries, and it only ever buzzes for your own tracked team - never for a primetime game you're just watching. Toggle: **Vibrate on Score**.

**Always visible** - time, date, and an optional battery bar so it's still just a watch. Toggle: **Battery Bar**.

## Settings

| Setting | Options |
|---|---|
| Team | All 32 NFL teams |
| Auto-Show Primetime Games | On / Off |
| Team Logos | On / Off |
| Ticker Speed | 5s / 10s / 30s / 60s |
| Vibrate on Score | On / Off |
| Battery Bar | On / Off |

## Data

ESPN's public scoreboard/summary API - free, no API key. Refreshes every minute, so you're always caught up.

Questions: broomaninks@gmail.com
