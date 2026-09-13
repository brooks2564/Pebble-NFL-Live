// ── NFL Live Watchface  ·  PebbleKit JS ────────────────────────────────────
// Data source: ESPN's public scoreboard/summary API (free, no key required)
var Clay = require('pebble-clay');
var clayConfig = require('./config.json');
var clay = new Clay(clayConfig);   // autoHandleEvents: true — Clay persists & sends AppMessage

// Keys must match #define KEY_* in main.c exactly
var KEY_AWAY_ABBR    = 1;
var KEY_HOME_ABBR    = 2;
var KEY_AWAY_SCORE   = 3;
var KEY_HOME_SCORE   = 4;
var KEY_QUARTER      = 5;
var KEY_CLOCK        = 6;
var KEY_DOWN         = 7;
var KEY_DISTANCE     = 8;
var KEY_FIELD_POS    = 9;
var KEY_REDZONE      = 10;
var KEY_POSSESSION   = 11;
var KEY_STATUS       = 12;
var KEY_TEAM_IDX     = 13;
var KEY_START_TIME   = 14;
var KEY_AWAY_RECORD  = 15;
var KEY_HOME_RECORD  = 16;
var KEY_VIBRATE      = 17;
var KEY_LAST_PLAY    = 18;
var KEY_NEXT_GAME    = 19;
var KEY_BATTERY_BAR  = 20;
var KEY_TICKER       = 21;
var KEY_DOWN_TEXT    = 22;
var KEY_SCORE_EVENT  = 23;
var KEY_NETWORK      = 24;

var SCOREBOARD_URL = "https://site.api.espn.com/apis/site/v2/sports/football/nfl/scoreboard";
var SUMMARY_URL    = "https://site.api.espn.com/apis/site/v2/sports/football/nfl/summary";

var TEAMS = [
  { abbr: "ARI", name: "Cardinals"  },
  { abbr: "ATL", name: "Falcons"    },
  { abbr: "BAL", name: "Ravens"     },
  { abbr: "BUF", name: "Bills"      },
  { abbr: "CAR", name: "Panthers"   },
  { abbr: "CHI", name: "Bears"      },
  { abbr: "CIN", name: "Bengals"    },
  { abbr: "CLE", name: "Browns"     },
  { abbr: "DAL", name: "Cowboys"    },
  { abbr: "DEN", name: "Broncos"    },
  { abbr: "DET", name: "Lions"      },
  { abbr: "GB",  name: "Packers"    },
  { abbr: "HOU", name: "Texans"     },
  { abbr: "IND", name: "Colts"      },
  { abbr: "JAX", name: "Jaguars"    },
  { abbr: "KC",  name: "Chiefs"     },
  { abbr: "LV",  name: "Raiders"    },
  { abbr: "LAC", name: "Chargers"   },
  { abbr: "LAR", name: "Rams"       },
  { abbr: "MIA", name: "Dolphins"   },
  { abbr: "MIN", name: "Vikings"    },
  { abbr: "NE",  name: "Patriots"   },
  { abbr: "NO",  name: "Saints"     },
  { abbr: "NYG", name: "Giants"     },
  { abbr: "NYJ", name: "Jets"       },
  { abbr: "PHI", name: "Eagles"     },
  { abbr: "PIT", name: "Steelers"   },
  { abbr: "SF",  name: "49ers"      },
  { abbr: "SEA", name: "Seahawks"   },
  { abbr: "TB",  name: "Buccaneers" },
  { abbr: "TEN", name: "Titans"     },
  { abbr: "WSH", name: "Commanders" }
];

// ── Saved state ────────────────────────────────────────────────────────────
var gTeamIdx    = 15;   // KC
var gVibrate    = true;
var gBatteryBar = true;

function loadFromClay() {
  var cs = {};
  try { cs = JSON.parse(localStorage.getItem("clay-settings")) || {}; } catch(e) {}
  var pIdx = parseInt(cs.TEAM_IDX, 10);
  if (!isNaN(pIdx) && pIdx >= 0 && pIdx < TEAMS.length) gTeamIdx = pIdx;
  if (cs.VIBRATE     !== undefined) gVibrate    = !!cs.VIBRATE;
  if (cs.BATTERY_BAR !== undefined) gBatteryBar = !!cs.BATTERY_BAR;
}
loadFromClay();

// ── Scoring-play tracker (per gamePk) — mirrors the MLB HR tracker pattern.
// Only the FIRST call for a given gameId establishes the baseline (no buzz for
// plays that happened before the watch started watching); every scoring play
// found after that baseline is examined for TD / FG / Safety.
var gLastGameId     = null;
var gLastScoreCount = -1;

// Returns 0 (no buzz), 1 (single short buzz — FG or Safety) or 2 (triple buzz — TD)
// for the highest-priority NEW scoring play by `myAbbr` since the last check.
// PATs and 2-point conversions are folded into the touchdown's own scoring-play
// entry by ESPN's API (they never appear as their own entry), so they never
// produce a separate event — exactly the behavior we want.
function checkScoreEvent(summaryData, gameId, myAbbr) {
  var scoringPlays = (summaryData && summaryData.scoringPlays) || [];

  if (gameId !== gLastGameId) {
    gLastGameId     = gameId;
    gLastScoreCount = scoringPlays.length;
    return 0;
  }

  var event = 0;
  for (var i = gLastScoreCount; i < scoringPlays.length; i++) {
    var sp   = scoringPlays[i];
    var team = ((sp.team) || {}).abbreviation || "";
    if (team !== myAbbr) continue;

    var abbr = ((sp.type) || {}).abbreviation || "";
    var text = ((sp.type) || {}).text || "";
    if (abbr === "TD" || text.indexOf("Touchdown") !== -1) {
      event = 2; // triple buzz — highest priority, stop looking for a bigger one
    } else if (event < 1 && (abbr === "FG" || abbr === "SF" ||
               text.indexOf("Field Goal") !== -1 || text.indexOf("Safety") !== -1)) {
      event = 1; // single buzz
    }
  }
  gLastScoreCount = scoringPlays.length;
  return event;
}

// ── Utility ───────────────────────────────────────────────────────────────
function formatStartTime(isoStr) {
  if (!isoStr) return "";
  try {
    var d = new Date(isoStr);
    if (isNaN(d.getTime())) return "";
    var h = d.getHours(), m = d.getMinutes();
    var ampm = h >= 12 ? "PM" : "AM";
    h = h % 12; if (h === 0) h = 12;
    return h + ":" + (m < 10 ? "0" + m : m) + " " + ampm;
  } catch(e) { return ""; }
}

function formatDayOfWeek(isoStr) {
  if (!isoStr) return "";
  try {
    var d = new Date(isoStr);
    if (isNaN(d.getTime())) return "";
    return ["Sun","Mon","Tue","Wed","Thu","Fri","Sat"][d.getDay()];
  } catch(e) { return ""; }
}

function ordinal(n) {
  if (n === 1) return "1st";
  if (n === 2) return "2nd";
  if (n === 3) return "3rd";
  if (n === 4) return "4th";
  return n + "th";
}

function competitorFor(comp, side) {
  for (var i = 0; i < comp.competitors.length; i++) {
    if (comp.competitors[i].homeAway === side) return comp.competitors[i];
  }
  return {};
}

function recordFor(competitor) {
  var recs = competitor.records || [];
  for (var i = 0; i < recs.length; i++) {
    if (recs[i].type === "total") return recs[i].summary || "";
  }
  return recs[0] ? (recs[0].summary || "") : "";
}

function findTeamEvent(events, abbr) {
  for (var i = 0; i < events.length; i++) {
    var comp = events[i].competitions[0];
    var away = competitorFor(comp, "away").team || {};
    var home = competitorFor(comp, "home").team || {};
    if ((away.abbreviation || "") === abbr || (home.abbreviation || "") === abbr) {
      return events[i];
    }
  }
  return null;
}

// Pick best TV network: national broadcast first, else whatever is listed
function getNetwork(broadcasts) {
  if (!broadcasts || !broadcasts.length) return "";
  for (var i = 0; i < broadcasts.length; i++) {
    if (broadcasts[i].market === "national" && broadcasts[i].names && broadcasts[i].names[0]) {
      return broadcasts[i].names[0];
    }
  }
  return (broadcasts[0].names && broadcasts[0].names[0]) || "";
}

// "TEAM 35" / "50" → { team: "TEAM", yard: 35 }. ESPN's possessionText gives
// yards from the NAMED team's own goal line (broadcast convention).
function parsePossessionText(text) {
  if (!text) return null;
  var m = /^([A-Z]+)\s+(\d+)$/.exec(text.trim());
  if (m) return { team: m[1], yard: parseInt(m[2], 10) };
  var m2 = /^(\d+)$/.exec(text.trim());
  if (m2) return { team: null, yard: parseInt(m2[1], 10) }; // midfield ("50")
  return null;
}

// Map possession + yard-from-own-goal into a 0-100 field position where
// 0 = away team's own goal line (left edge) and 100 = home team's own goal
// line (right edge) — matches the field bar drawn on the watch.
function computeFieldPos(situation, awayAbbr, homeAbbr) {
  var parsed = parsePossessionText(situation.possessionText || "");
  if (!parsed) return 50;
  if (parsed.team === awayAbbr) return parsed.yard;
  if (parsed.team === homeAbbr) return 100 - parsed.yard;
  return 50; // exact midfield, no team prefix
}

// ── Ticker builder (other games this week) ──────────────────────────────────
function buildTicker(events, myAbbr) {
  var parts = [];
  for (var i = 0; i < events.length; i++) {
    var comp  = events[i].competitions[0];
    var awayC = competitorFor(comp, "away");
    var homeC = competitorFor(comp, "home");
    var away  = (awayC.team || {}).abbreviation || "";
    var home  = (homeC.team || {}).abbreviation || "";
    if (away === myAbbr || home === myAbbr) continue;

    var st = ((comp.status || {}).type || {});
    var entry = "";
    if (st.state === "pre") {
      entry = away + " vs " + home + " " + formatStartTime(comp.date || "");
    } else if (st.state === "post") {
      entry = away + " " + (awayC.score || 0) + " - " + home + " " + (homeC.score || 0) + " F";
    } else if (st.state === "in") {
      var status = comp.status || {};
      var q = status.period ? "Q" + status.period : "";
      entry = away + " " + (awayC.score || 0) + " - " + home + " " + (homeC.score || 0) +
              " " + q + " " + (status.displayClock || "");
    } else {
      continue;
    }
    if (entry.length > 24) entry = entry.substring(0, 24);
    parts.push(entry);
  }
  return parts.join("|");
}

// ── Live detail fetch (scoring plays + situation refinement) ───────────────
function fetchSummary(gameId, callback) {
  var url = SUMMARY_URL + "?event=" + gameId;
  console.log("[NFL] Fetching summary: " + gameId);
  var xhr = new XMLHttpRequest();
  xhr.open("GET", url, true);
  xhr.setRequestHeader("Accept", "application/json");
  xhr.onload = function() {
    if (xhr.status !== 200) { callback(null); return; }
    try { callback(JSON.parse(xhr.responseText)); }
    catch(e) { console.log("[NFL] Summary parse error: " + e); callback(null); }
  };
  xhr.onerror = function() { callback(null); };
  xhr.send();
}

// ── Next-game lookahead (bye weeks / after a final) ─────────────────────────
function fetchWeek(week, callback) {
  var url = SCOREBOARD_URL + "?seasontype=2&week=" + week;
  var xhr = new XMLHttpRequest();
  xhr.open("GET", url, true);
  xhr.setRequestHeader("Accept", "application/json");
  xhr.onload = function() {
    if (xhr.status !== 200) { callback(null); return; }
    try { callback(JSON.parse(xhr.responseText)); } catch(e) { callback(null); }
  };
  xhr.onerror = function() { callback(null); };
  xhr.send();
}

function findNextGame(currentWeek, abbr, attemptsLeft, callback) {
  if (attemptsLeft <= 0) { callback(null); return; }
  var nextWeek = currentWeek + 1;
  fetchWeek(nextWeek, function(data) {
    if (!data || !data.events) { callback(null); return; }
    var ev = findTeamEvent(data.events, abbr);
    if (ev) { callback(ev); return; }
    findNextGame(nextWeek, abbr, attemptsLeft - 1, callback);
  });
}

// ── Main fetch ───────────────────────────────────────────────────────────
function sendOffMessage(nextGameText) {
  var msg = {};
  msg[KEY_STATUS]    = "off";
  msg[KEY_NEXT_GAME] = nextGameText || "";
  sendMessage(msg);
}

function fetchGameData(teamIdx) {
  var abbr = TEAMS[teamIdx].abbr;
  console.log("[NFL] Fetching for " + abbr);
  var xhr = new XMLHttpRequest();
  xhr.open("GET", SCOREBOARD_URL, true);
  xhr.setRequestHeader("Accept", "application/json");
  xhr.onload = function() {
    if (xhr.status !== 200) { console.log("[NFL] API error: " + xhr.status); sendOffMessage(); return; }
    try {
      var data   = JSON.parse(xhr.responseText);
      var events = data.events || [];
      var week   = (data.week && data.week.number) || 1;
      processEvents(data, events, week, abbr);
    } catch(e) {
      console.log("[NFL] Parse error: " + e);
      sendOffMessage();
    }
  };
  xhr.onerror = function() { sendOffMessage(); };
  xhr.send();
}

function buildNextGameText(ev, abbr) {
  var comp  = ev.competitions[0];
  var away  = (competitorFor(comp, "away").team || {}).abbreviation || "";
  var home  = (competitorFor(comp, "home").team || {}).abbreviation || "";
  var opp   = (away === abbr) ? home : away;
  var t     = formatStartTime(comp.date || "");
  var day   = formatDayOfWeek(comp.date || "");
  var text  = "Bye - Next: " + opp + (day ? " " + day : "") + (t ? " " + t : "");
  return text.length > 23 ? text.substring(0, 23) : text;
}

function processEvents(data, events, week, abbr) {
  var ev = findTeamEvent(events, abbr);

  if (!ev) {
    // Bye week — look ahead up to 3 weeks for the next scheduled game
    findNextGame(week, abbr, 3, function(nextEv) {
      sendOffMessage(nextEv ? buildNextGameText(nextEv, abbr) : "Bye Week");
    });
    return;
  }

  var comp    = ev.competitions[0];
  var awayC   = competitorFor(comp, "away");
  var homeC   = competitorFor(comp, "home");
  var awayAbbr = (awayC.team || {}).abbreviation || "---";
  var homeAbbr = (homeC.team || {}).abbreviation || "---";
  var isUserAway = (awayAbbr === abbr);

  var statusType = (comp.status || {}).type || {};
  var status = statusType.state === "in"   ? "live"
             : statusType.state === "post" ? "final"
             : statusType.state === "pre"  ? "pre" : "off";

  var msg = {};
  msg[KEY_AWAY_ABBR]   = awayAbbr;
  msg[KEY_HOME_ABBR]   = homeAbbr;
  msg[KEY_AWAY_SCORE]  = parseInt(awayC.score, 10) || 0;
  msg[KEY_HOME_SCORE]  = parseInt(homeC.score, 10) || 0;
  msg[KEY_STATUS]      = status;
  msg[KEY_START_TIME]  = formatStartTime(comp.date || "");
  msg[KEY_AWAY_RECORD] = recordFor(awayC);
  msg[KEY_HOME_RECORD] = recordFor(homeC);
  msg[KEY_VIBRATE]     = gVibrate ? 1 : 0;
  msg[KEY_BATTERY_BAR] = gBatteryBar ? 1 : 0;
  msg[KEY_NETWORK]     = getNetwork(comp.broadcasts);
  msg[KEY_TICKER]      = buildTicker(events, abbr);
  msg[KEY_NEXT_GAME]   = "";
  msg[KEY_QUARTER]     = comp.status && comp.status.period ? comp.status.period : 0;
  msg[KEY_CLOCK]       = (comp.status && comp.status.displayClock) || "";
  msg[KEY_DOWN]        = 0;
  msg[KEY_DISTANCE]    = 0;
  msg[KEY_DOWN_TEXT]   = "";
  msg[KEY_FIELD_POS]   = 50;
  msg[KEY_REDZONE]     = 0;
  msg[KEY_POSSESSION]  = 2; // none
  msg[KEY_LAST_PLAY]   = "";
  msg[KEY_SCORE_EVENT] = 0;

  var situation = comp.situation;
  if (situation) {
    msg[KEY_DOWN]      = situation.down || 0;
    msg[KEY_DISTANCE]  = situation.distance || 0;
    msg[KEY_DOWN_TEXT] = situation.shortDownDistanceText || situation.downDistanceText || "";
    var fieldPos = computeFieldPos(situation, awayAbbr, homeAbbr);
    msg[KEY_FIELD_POS] = fieldPos;

    var possAbbr = (parsePossessionText(situation.possessionText || "") || {}).team;
    msg[KEY_POSSESSION] = (possAbbr === awayAbbr) ? 0 : (possAbbr === homeAbbr) ? 1 : 2;

    if (typeof situation.isRedZone === "boolean") {
      msg[KEY_REDZONE] = situation.isRedZone ? 1 : 0;
    } else {
      var rz = (msg[KEY_POSSESSION] === 0 && fieldPos >= 80) ||
               (msg[KEY_POSSESSION] === 1 && fieldPos <= 20);
      msg[KEY_REDZONE] = rz ? 1 : 0;
    }

    if (situation.lastPlay && situation.lastPlay.text) {
      msg[KEY_LAST_PLAY] = situation.lastPlay.text.substring(0, 40);
    }
  }

  // Final: figure out next game for next week
  if (status === "final") {
    findNextGame(week, abbr, 2, function(nextEv) {
      if (nextEv) msg[KEY_NEXT_GAME] = buildNextGameText(nextEv, abbr);
      finishSend(status, ev, msg, isUserAway ? awayAbbr : homeAbbr);
    });
    return;
  }

  finishSend(status, ev, msg, isUserAway ? awayAbbr : homeAbbr);
}

function finishSend(status, ev, msg, myAbbr) {
  if (status === "live" && ev.id) {
    fetchSummary(ev.id, function(summaryData) {
      msg[KEY_SCORE_EVENT] = checkScoreEvent(summaryData, ev.id, myAbbr);
      sendMessage(msg);
    });
    return;
  }
  sendMessage(msg);
}

// ── AppMessage transport ────────────────────────────────────────────────────
// Split across two messages (main + ticker) to stay well under the 512-byte
// inbox limit — same reasoning as the MLB watchface.
function sendMessage(dict) {
  var ticker = dict[KEY_TICKER];
  delete dict[KEY_TICKER];

  Pebble.sendAppMessage(dict,
    function() {
      console.log("[NFL] Message sent OK");
      if (ticker !== undefined) {
        var tm = {};
        tm[KEY_TICKER] = ticker;
        Pebble.sendAppMessage(tm,
          function()  { console.log("[NFL] Ticker sent OK"); },
          function(e) { console.log("[NFL] Ticker NACK: " + JSON.stringify(e)); }
        );
      }
    },
    function(e) { console.log("[NFL] NACK: " + JSON.stringify(e)); }
  );
}

// ── Pebble events ─────────────────────────────────────────────────────────
Pebble.addEventListener("ready", function() {
  console.log("[NFL] Ready – team: " + TEAMS[gTeamIdx].abbr);
  fetchGameData(gTeamIdx);
});

Pebble.addEventListener("appmessage", function(e) {
  var msg = e.payload;
  var idx = parseInt(msg[KEY_TEAM_IDX]);
  if (!isNaN(idx) && idx >= 0 && idx < TEAMS.length) {
    gTeamIdx = idx;
  }
  fetchGameData(gTeamIdx);
});

Pebble.addEventListener("webviewclosed", function(e) {
  if (!e || !e.response || e.response === "CANCELLED") return;
  try {
    loadFromClay();
    fetchGameData(gTeamIdx);
  } catch(ex) {
    console.log("[NFL] webviewclosed error: " + ex);
  }
});
