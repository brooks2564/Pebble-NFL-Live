#include <pebble.h>

// ── Message keys — must match src/pkjs/index.js exactly ────────────────────
#define KEY_AWAY_ABBR    1
#define KEY_HOME_ABBR    2
#define KEY_AWAY_SCORE   3
#define KEY_HOME_SCORE   4
#define KEY_QUARTER      5
#define KEY_CLOCK        6
#define KEY_DOWN         7
#define KEY_DISTANCE     8
#define KEY_FIELD_POS    9
#define KEY_REDZONE      10
#define KEY_POSSESSION   11
#define KEY_STATUS       12
#define KEY_TEAM_IDX     13
#define KEY_START_TIME   14
#define KEY_AWAY_RECORD  15
#define KEY_HOME_RECORD  16
#define KEY_VIBRATE      17
#define KEY_LAST_PLAY    18
#define KEY_NEXT_GAME    19
#define KEY_BATTERY_BAR  20
#define KEY_TICKER       21
#define KEY_DOWN_TEXT    22
#define KEY_SCORE_EVENT  23
#define KEY_NETWORK      24
#define KEY_FEATURED_TAG 25
#define KEY_TEAM_LOGOS   26
#define KEY_TICKER_SPEED 27

#define PERSIST_TEAM         1
#define PERSIST_VIB          2
#define PERSIST_BAT          3
#define PERSIST_TEAM_LOGOS   4
#define PERSIST_TICKER_SPEED 5

#define MAX_GAMES 6
#define GAME_LEN  26

static Window *s_window;
static Layer  *s_canvas;
static AppTimer *s_ticker_timer;
static int s_ticker_speed = 5000; // ms between ticker advances (default 5s)

// Ticker state — parsed from the pipe-delimited TICKER string
static char s_ticker_raw[200];
static char s_games[MAX_GAMES][GAME_LEN];
static int  s_game_count;
static int  s_game_idx;

// Game state
static char s_time_buf[6]   = "00:00";
static char s_date_buf[14]  = "";
static int  s_team_idx      = 15; // KC
static char s_away_abbr[5]  = "---";
static char s_home_abbr[5]  = "---";
static int  s_away_score    = 0;
static int  s_home_score    = 0;
static int  s_quarter       = 0;
static char s_clock[8]      = "";
static int  s_down          = 0;
static int  s_distance      = 0;
static char s_down_text[24] = "";
static int  s_field_pos     = 50;   // 0 = away goal line, 100 = home goal line
static bool s_redzone       = false;
static int  s_possession    = 2;    // 0=away, 1=home, 2=none
static char s_status[8]     = "off";
static char s_start_time[10]= "";
static char s_away_record[10] = "";
static char s_home_record[10] = "";
static bool s_vibrate       = true;
static char s_last_play[44] = "";
static char s_next_game[32] = "";
static bool s_battery_bar   = true;
static int  s_battery_pct   = 100;
static char s_network[24]   = "";
static char s_featured_tag[5] = ""; // "TNF"/"SNF"/"MNF" when auto-featuring a primetime game
static bool s_team_logos    = true;

// Cached logo bitmaps — reloaded only when the away/home abbreviation changes
#ifdef PBL_COLOR
static GBitmap *s_away_logo_lg = NULL, *s_away_logo_sm = NULL;
static GBitmap *s_home_logo_lg = NULL, *s_home_logo_sm = NULL;
static char s_away_logo_abbr[5] = "";
static char s_home_logo_abbr[5] = "";
#else
static GBitmap *s_away_logo_lg = NULL, *s_away_logo_sm = NULL;
static GBitmap *s_home_logo_lg = NULL, *s_home_logo_sm = NULL;
#endif

// Single shared football icon used as the field-bar ball marker (loaded once,
// not per-team)
static GBitmap *s_football_bmp = NULL;
#ifdef PBL_PLATFORM_EMERY
  #define FOOTBALL_RESOURCE RESOURCE_ID_FOOTBALL_EM
#else
  #define FOOTBALL_RESOURCE RESOURCE_ID_FOOTBALL_BA
#endif

static void request_game_data(void);

// ── Ticker parsing (pipe-delimited "AWAY 10 - HOME 7 Q2 8:42" entries) ──────
static void ticker_parse(void) {
  s_game_count = 0;
  s_game_idx   = 0;
  int len = strlen(s_ticker_raw);
  int start = 0;
  for (int i = 0; i <= len && s_game_count < MAX_GAMES; i++) {
    if (s_ticker_raw[i] == '|' || s_ticker_raw[i] == '\0') {
      int seg_len = i - start;
      if (seg_len > 0) {
        if (seg_len >= GAME_LEN) seg_len = GAME_LEN - 1;
        memcpy(s_games[s_game_count], s_ticker_raw + start, seg_len);
        s_games[s_game_count][seg_len] = '\0';
        s_game_count++;
      }
      start = i + 1;
    }
  }
}

static void ticker_advance(void *ctx) {
  if (s_game_count > 0) {
    s_game_idx = (s_game_idx + 1) % s_game_count;
    if (s_canvas) layer_mark_dirty(s_canvas);
  }
  s_ticker_timer = app_timer_register((uint32_t)s_ticker_speed, ticker_advance, NULL);
}

// ── Team colors (emery only — see MLB blueprint precedent) ─────────────────
#ifdef PBL_PLATFORM_EMERY
static GColor team_color(const char *abbr) {
  if (!abbr) return GColorWhite;
  if (strcmp(abbr,"ARI")==0) return GColorFromHEX(0xa40227);
  if (strcmp(abbr,"ATL")==0) return GColorFromHEX(0xa71930);
  if (strcmp(abbr,"BAL")==0) return GColorFromHEX(0x29126f);
  if (strcmp(abbr,"BUF")==0) return GColorFromHEX(0x00338d);
  if (strcmp(abbr,"CAR")==0) return GColorFromHEX(0x0085ca);
  if (strcmp(abbr,"CHI")==0) return GColorFromHEX(0x0b1c3a);
  if (strcmp(abbr,"CIN")==0) return GColorFromHEX(0xfb4f14);
  if (strcmp(abbr,"CLE")==0) return GColorFromHEX(0x8a3324);
  if (strcmp(abbr,"DAL")==0) return GColorFromHEX(0x002a5c);
  if (strcmp(abbr,"DEN")==0) return GColorFromHEX(0xfc4c02);
  if (strcmp(abbr,"DET")==0) return GColorFromHEX(0x0076b6);
  if (strcmp(abbr,"GB") ==0) return GColorFromHEX(0x204e32);
  if (strcmp(abbr,"HOU")==0) return GColorFromHEX(0xeb0028);
  if (strcmp(abbr,"IND")==0) return GColorFromHEX(0x003b75);
  if (strcmp(abbr,"JAX")==0) return GColorFromHEX(0x007487);
  if (strcmp(abbr,"KC") ==0) return GColorFromHEX(0xe31837);
  if (strcmp(abbr,"LV") ==0) return GColorFromHEX(0xa5acaf);
  if (strcmp(abbr,"LAC")==0) return GColorFromHEX(0x0080c6);
  if (strcmp(abbr,"LAR")==0) return GColorFromHEX(0x003594);
  if (strcmp(abbr,"MIA")==0) return GColorFromHEX(0x008e97);
  if (strcmp(abbr,"MIN")==0) return GColorFromHEX(0x4f2683);
  if (strcmp(abbr,"NE") ==0) return GColorFromHEX(0xc60c30);
  if (strcmp(abbr,"NO") ==0) return GColorFromHEX(0xd3bc8d);
  if (strcmp(abbr,"NYG")==0) return GColorFromHEX(0x003c7f);
  if (strcmp(abbr,"NYJ")==0) return GColorFromHEX(0x115740);
  if (strcmp(abbr,"PHI")==0) return GColorFromHEX(0x06424d);
  if (strcmp(abbr,"PIT")==0) return GColorFromHEX(0xffb612);
  if (strcmp(abbr,"SF") ==0) return GColorFromHEX(0xaa0000);
  if (strcmp(abbr,"SEA")==0) return GColorFromHEX(0x69be28);
  if (strcmp(abbr,"TB") ==0) return GColorFromHEX(0xbd1c36);
  if (strcmp(abbr,"TEN")==0) return GColorFromHEX(0x4495d2);
  if (strcmp(abbr,"WSH")==0) return GColorFromHEX(0x5a1414);
  return GColorWhite;
}

static void draw_team_text(GContext *ctx, const char *text, GFont font, GRect rect,
                            GTextOverflowMode overflow, GTextAlignment align, GColor color) {
  graphics_context_set_text_color(ctx, GColorBlack);
  GRect r = rect;
  r.origin.x -= 1; graphics_draw_text(ctx, text, font, r, overflow, align, NULL);
  r.origin.x += 2; graphics_draw_text(ctx, text, font, r, overflow, align, NULL);
  r.origin.x -= 1; r.origin.y -= 1; graphics_draw_text(ctx, text, font, r, overflow, align, NULL);
  r.origin.y += 2; graphics_draw_text(ctx, text, font, r, overflow, align, NULL);
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, font, rect, overflow, align, NULL);
}
#endif

// ── Team logos (color platforms only) ───────────────────────────────────────
// Real ESPN team logos, pre-flattened onto an opaque black square per team,
// at two exact pixel sizes per platform (LG for the score row, SM for the
// records-shown slot) — graphics_draw_bitmap_in_rect() clips rather than
// scales, so each size must match its resource's native dimensions exactly.
// Emery and basalt each get their own resource set; flint has no logo
// resources at all, so it always falls back to text below.
#ifdef PBL_COLOR
typedef struct { const char *abbr; uint32_t lg_id; uint32_t sm_id; } LogoEntry;
#if defined(PBL_PLATFORM_EMERY)
  #define HAVE_LOGOS 1
  #define LOGO_LG(a) RESOURCE_ID_LOGO_##a##_EM_LG
  #define LOGO_SM(a) RESOURCE_ID_LOGO_##a##_EM_SM
#elif defined(PBL_PLATFORM_BASALT)
  #define HAVE_LOGOS 1
  #define LOGO_LG(a) RESOURCE_ID_LOGO_##a##_BA_LG
  #define LOGO_SM(a) RESOURCE_ID_LOGO_##a##_BA_SM
#endif
#endif

#ifdef HAVE_LOGOS
static const LogoEntry LOGO_TABLE[] = {
  {"ARI", LOGO_LG(ARI), LOGO_SM(ARI)}, {"ATL", LOGO_LG(ATL), LOGO_SM(ATL)},
  {"BAL", LOGO_LG(BAL), LOGO_SM(BAL)}, {"BUF", LOGO_LG(BUF), LOGO_SM(BUF)},
  {"CAR", LOGO_LG(CAR), LOGO_SM(CAR)}, {"CHI", LOGO_LG(CHI), LOGO_SM(CHI)},
  {"CIN", LOGO_LG(CIN), LOGO_SM(CIN)}, {"CLE", LOGO_LG(CLE), LOGO_SM(CLE)},
  {"DAL", LOGO_LG(DAL), LOGO_SM(DAL)}, {"DEN", LOGO_LG(DEN), LOGO_SM(DEN)},
  {"DET", LOGO_LG(DET), LOGO_SM(DET)}, {"GB",  LOGO_LG(GB),  LOGO_SM(GB) },
  {"HOU", LOGO_LG(HOU), LOGO_SM(HOU)}, {"IND", LOGO_LG(IND), LOGO_SM(IND)},
  {"JAX", LOGO_LG(JAX), LOGO_SM(JAX)}, {"KC",  LOGO_LG(KC),  LOGO_SM(KC) },
  {"LV",  LOGO_LG(LV),  LOGO_SM(LV) }, {"LAC", LOGO_LG(LAC), LOGO_SM(LAC)},
  {"LAR", LOGO_LG(LAR), LOGO_SM(LAR)}, {"MIA", LOGO_LG(MIA), LOGO_SM(MIA)},
  {"MIN", LOGO_LG(MIN), LOGO_SM(MIN)}, {"NE",  LOGO_LG(NE),  LOGO_SM(NE) },
  {"NO",  LOGO_LG(NO),  LOGO_SM(NO) }, {"NYG", LOGO_LG(NYG), LOGO_SM(NYG)},
  {"NYJ", LOGO_LG(NYJ), LOGO_SM(NYJ)}, {"PHI", LOGO_LG(PHI), LOGO_SM(PHI)},
  {"PIT", LOGO_LG(PIT), LOGO_SM(PIT)}, {"SF",  LOGO_LG(SF),  LOGO_SM(SF) },
  {"SEA", LOGO_LG(SEA), LOGO_SM(SEA)}, {"TB",  LOGO_LG(TB),  LOGO_SM(TB) },
  {"TEN", LOGO_LG(TEN), LOGO_SM(TEN)}, {"WSH", LOGO_LG(WSH), LOGO_SM(WSH)},
};
#define LOGO_TABLE_LEN (int)(sizeof(LOGO_TABLE)/sizeof(LOGO_TABLE[0]))

static void find_logo_resources(const char *abbr, uint32_t *lg, uint32_t *sm) {
  *lg = 0; *sm = 0;
  for (int i = 0; i < LOGO_TABLE_LEN; i++)
    if (strcmp(LOGO_TABLE[i].abbr, abbr) == 0) { *lg = LOGO_TABLE[i].lg_id; *sm = LOGO_TABLE[i].sm_id; return; }
}

static void set_team_logo(GBitmap **bmp_lg, GBitmap **bmp_sm, char *cached_abbr, const char *abbr) {
  if (strcmp(cached_abbr, abbr) == 0) return; // already loaded
  if (*bmp_lg) { gbitmap_destroy(*bmp_lg); *bmp_lg = NULL; }
  if (*bmp_sm) { gbitmap_destroy(*bmp_sm); *bmp_sm = NULL; }
  strncpy(cached_abbr, abbr, 4); cached_abbr[4] = 0;
  uint32_t lg, sm;
  find_logo_resources(abbr, &lg, &sm);
  if (lg) *bmp_lg = gbitmap_create_with_resource(lg);
  if (sm) *bmp_sm = gbitmap_create_with_resource(sm);
}

static void update_team_logos(void) {
  set_team_logo(&s_away_logo_lg, &s_away_logo_sm, s_away_logo_abbr, s_away_abbr);
  set_team_logo(&s_home_logo_lg, &s_home_logo_sm, s_home_logo_abbr, s_home_abbr);
}
#else
static void update_team_logos(void) { }
#endif

// Draws a team's logo (if enabled + available) or falls back to its 3-letter
// abbreviation. Logos are drawn at their native resource size (never
// stretched/clipped), centered in rect and anchored per alignment.
static void draw_team_badge(GContext *ctx, const char *abbr, GFont font, GRect rect,
                             GTextOverflowMode overflow, GTextAlignment align,
                             GBitmap *logo_lg, GBitmap *logo_sm, bool use_large) {
#ifdef HAVE_LOGOS
  GBitmap *logo = use_large ? logo_lg : logo_sm;
  if (s_team_logos && logo) {
    GRect b = gbitmap_get_bounds(logo);
    int x = rect.origin.x;
    if (align == GTextAlignmentRight) x = rect.origin.x + rect.size.w - b.size.w;
    else if (align == GTextAlignmentCenter) x = rect.origin.x + (rect.size.w - b.size.w) / 2;
    int y = rect.origin.y + (rect.size.h - b.size.h) / 2;
    graphics_draw_bitmap_in_rect(ctx, logo, GRect(x, y, b.size.w, b.size.h));
    return;
  }
#endif
#ifdef PBL_PLATFORM_EMERY
  draw_team_text(ctx, abbr, font, rect, overflow, align, team_color(abbr));
#else
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, abbr, font, rect, overflow, align, NULL);
#endif
}

// White-on-black-outline halo text — legible over any background color,
// which is exactly what the endzone abbreviations need since they sit on
// top of each team's own (highly variable) color.
static void draw_halo_text(GContext *ctx, const char *text, GFont font, GRect rect) {
  graphics_context_set_text_color(ctx, GColorBlack);
  GRect r = rect;
  r.origin.x -= 1; graphics_draw_text(ctx, text, font, r, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  r.origin.x += 2; graphics_draw_text(ctx, text, font, r, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  r.origin.x -= 1; r.origin.y -= 1; graphics_draw_text(ctx, text, font, r, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  r.origin.y += 2; graphics_draw_text(ctx, text, font, r, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, text, font, rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// ── Field position bar ──────────────────────────────────────────────────────
// 0 = away team's own goal line (left edge), 100 = home team's own goal line
// (right edge). End zones are shaded in each team's color on emery (generic
// dark shading elsewhere), each labeled with its 3-letter abbreviation. A red
// outline appears for as long as REDZONE is true and disappears the instant
// it clears — no animation, no timer.
static void draw_field_bar(GContext *ctx, int x, int y, int w, int h, GFont f_ez) {
#ifdef PBL_PLATFORM_EMERY
  int ez_w = w * 13 / 100; // ~13% end zone width each side, visually generous
#else
  int ez_w = w * 12 / 100;
#endif
  GRect bar = GRect(x, y, w, h);

  // Field (middle) background
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, bar, 0, GCornerNone);

  // End zones
#ifdef PBL_PLATFORM_EMERY
  graphics_context_set_fill_color(ctx, team_color(s_away_abbr));
  graphics_fill_rect(ctx, GRect(x, y, ez_w, h), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, team_color(s_home_abbr));
  graphics_fill_rect(ctx, GRect(x + w - ez_w, y, ez_w, h), 0, GCornerNone);
#else
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(x, y, ez_w, h), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x + w - ez_w, y, ez_w, h), 0, GCornerNone);
#endif

  // Endzone abbreviations — white-on-black halo reads over any team color
  draw_halo_text(ctx, s_away_abbr, f_ez, GRect(x, y, ez_w, h));
  draw_halo_text(ctx, s_home_abbr, f_ez, GRect(x + w - ez_w, y, ez_w, h));

  // Separator lines between end zone and field
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_line(ctx, GPoint(x + ez_w, y), GPoint(x + ez_w, y + h));
  graphics_draw_line(ctx, GPoint(x + w - ez_w, y), GPoint(x + w - ez_w, y + h));

  // 10-yard tick marks across the field
  int field_w = w - 2 * ez_w;
  for (int i = 1; i < 10; i++) {
    int tx = x + ez_w + (field_w * i) / 10;
    graphics_draw_line(ctx, GPoint(tx, y + h - 4), GPoint(tx, y + h));
  }

  // Ball marker (football icon) + a small arrow showing which way the
  // offense is driving
  if (strcmp(s_status, "live") == 0 && s_football_bmp) {
    int bx = x + ez_w + (field_w * s_field_pos) / 100;
    GRect fb = gbitmap_get_bounds(s_football_bmp);
    GRect dest = GRect(bx - fb.size.w / 2, y + (h - fb.size.h) / 2, fb.size.w, fb.size.h);
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, s_football_bmp, dest);
    graphics_context_set_compositing_mode(ctx, GCompOpAssign);

    if (s_possession == 0 || s_possession == 1) {
      const char *arrow = (s_possession == 0) ? ">" : "<";
      int arrow_x = (s_possession == 0) ? dest.origin.x + fb.size.w + 1 : dest.origin.x - 9;
      graphics_context_set_text_color(ctx, GColorWhite);
      graphics_draw_text(ctx, arrow, f_ez, GRect(arrow_x, y - 1, 10, h),
        GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    }
  }

  // Red zone outline — on while REDZONE is true, off the moment it clears
#ifdef PBL_COLOR
  GColor rz_color = GColorRed;
#else
  GColor rz_color = GColorWhite;
#endif
  GColor outline = s_redzone ? rz_color : GColorLightGray;
  graphics_context_set_stroke_color(ctx, outline);
  GRect outline_rect = bar;
  graphics_draw_rect(ctx, outline_rect);
  if (s_redzone) {
    // second pass, 1px in, so the outline reads as a clear 2px ring
    GRect inner = GRect(x + 1, y + 1, w - 2, h - 2);
    graphics_draw_rect(ctx, inner);
  }
}

// ── Canvas ───────────────────────────────────────────────────────────────
static void canvas_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int w = b.size.w;
  int h = b.size.h;
  int split = h * 3 / 10;
  int by = split + 2;
#ifdef PBL_ROUND
  int hpad = 18;
#else
  int hpad = 2;
#endif

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  if (s_battery_bar) {
    int bw = (w * s_battery_pct) / 100;
    graphics_context_set_fill_color(ctx, GColorDarkGray);
    graphics_fill_rect(ctx, GRect(0, h - 3, w, 3), 0, GCornerNone);
    GColor bc = s_battery_pct > 50 ? GColorGreen :
                s_battery_pct > 20 ? GColorYellow : GColorRed;
    graphics_context_set_fill_color(ctx, bc);
    graphics_fill_rect(ctx, GRect(0, h - 3, bw, 3), 0, GCornerNone);
  }

  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_line(ctx, GPoint(0, split), GPoint(w, split));

  bool live_now  = strcmp(s_status, "live")  == 0;
  bool final_now = strcmp(s_status, "final") == 0;
  bool pre_now   = strcmp(s_status, "pre")   == 0;
  bool off_now   = strcmp(s_status, "off")   == 0;

#ifdef PBL_PLATFORM_EMERY
  GFont f_score = fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);
  GFont f_abbr  = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GFont f_mid   = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont f_small = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  GFont f_tiny  = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  // score_h/abbr logo height match the emery LG logo (36x36) exactly, and
  // score_y sits far enough below the divider that the logo never paints
  // over it (badges are opaque, unlike text, so they need real clearance).
  int score_w = 110, abbr_w = 44, score_h = 36;
  int score_y = by + 2, rec_y = by + 40;
  int status_y = by + 60, detail_y = by + 82, lp_y = by + 102;
  int fb_y = by + 124, fb_h = 24;
  int ticker_top_y = 32, ticker_top_h = 24;
#else
  GFont f_score = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GFont f_abbr  = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont f_mid   = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GFont f_small = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont f_tiny  = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  // score_h matches the basalt LG logo (26x26) exactly, same reasoning as above.
  int score_w = 68, abbr_w = 36, score_h = 26;
  int score_y = by, rec_y = by + 28;
  int status_y = by + 42, detail_y = by + 56, lp_y = by + 70;
  int fb_y = by + 86, fb_h = 14;
  int ticker_top_y = 28, ticker_top_h = 18;
#endif

  // Time + date
  graphics_context_set_text_color(ctx, GColorWhite);
#ifdef PBL_PLATFORM_EMERY
  graphics_draw_text(ctx, s_time_buf, f_mid, GRect(hpad, 2, 72, 30),
    GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, s_date_buf, fonts_get_system_font(FONT_KEY_GOTHIC_24),
    GRect(68, 2, w - 68 - hpad, 26), GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
#else
  graphics_draw_text(ctx, s_time_buf, f_mid, GRect(hpad, 2, 60, 24),
    GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, s_date_buf, fonts_get_system_font(FONT_KEY_GOTHIC_18),
    GRect(56, 2, w - 56 - hpad, 20), GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
#endif

  // Ticker — other games this week, cycling every few seconds. Same slot as
  // the MLB watchface: directly under the time/date row, above the divider.
  graphics_context_set_text_color(ctx, GColorLightGray);
  const char *ticker_text = (s_game_count > 0) ? s_games[s_game_idx] : "";
  graphics_draw_text(ctx, ticker_text, f_tiny, GRect(hpad, ticker_top_y, w - 2 * hpad, ticker_top_h),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Away / Home badges (team logo when enabled + available, else abbreviation) + score
  draw_team_badge(ctx, s_away_abbr, f_abbr, GRect(hpad, score_y, abbr_w, score_h),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft,
    s_away_logo_lg, s_away_logo_sm, true);
  draw_team_badge(ctx, s_home_abbr, f_abbr, GRect(w - abbr_w - hpad, score_y, abbr_w, score_h),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentRight,
    s_home_logo_lg, s_home_logo_sm, true);

  if (!off_now) {
    char sbuf[16];
    graphics_context_set_text_color(ctx, GColorWhite);
    snprintf(sbuf, sizeof(sbuf), "%d - %d", s_away_score, s_home_score);
    graphics_draw_text(ctx, sbuf, f_score, GRect(hpad + abbr_w, score_y, score_w, score_h),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  // Records
  if (pre_now || final_now) {
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, s_away_record, f_tiny, GRect(hpad, rec_y, abbr_w + 20, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, s_home_record, f_tiny, GRect(w - abbr_w - hpad - 20, rec_y, abbr_w + 20, 16),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
  }

  // Status row: quarter + clock (live), kickoff time (pre), FINAL (final), bye (off).
  // A featured-primetime game (see Auto-Show Primetime Games) prefixes an
  // "SNF"/"MNF"/"TNF" tag so it's never mistaken for your own team's game.
  graphics_context_set_text_color(ctx, GColorWhite);
  char status_buf[40] = "";
  char status_core[32] = "";
  if (live_now) {
    if (s_quarter > 0) snprintf(status_core, sizeof(status_core), "Q%d  %s", s_quarter, s_clock);
    else snprintf(status_core, sizeof(status_core), "%s", s_clock);
  } else if (pre_now) {
    snprintf(status_core, sizeof(status_core), "Kickoff %s", s_start_time);
  } else if (final_now) {
    snprintf(status_core, sizeof(status_core), "FINAL");
  } else {
    snprintf(status_core, sizeof(status_core), "%s", s_next_game[0] ? "Bye Week" : "No Game");
  }
  if (s_featured_tag[0]) snprintf(status_buf, sizeof(status_buf), "%s - %s", s_featured_tag, status_core);
  else snprintf(status_buf, sizeof(status_buf), "%s", status_core);
  graphics_draw_text(ctx, status_buf, f_mid, GRect(hpad, status_y, w - 2 * hpad, 26),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Detail row: down & distance (live), network (pre), next game (final)
  graphics_context_set_text_color(ctx, GColorLightGray);
  char detail_buf[32] = "";
  if (live_now && s_down_text[0]) {
    snprintf(detail_buf, sizeof(detail_buf), "%s", s_down_text);
  } else if (pre_now && s_network[0]) {
    snprintf(detail_buf, sizeof(detail_buf), "%s", s_network);
  } else if (final_now && s_next_game[0]) {
    snprintf(detail_buf, sizeof(detail_buf), "%s", s_next_game);
  } else if (off_now && s_next_game[0]) {
    snprintf(detail_buf, sizeof(detail_buf), "%s", s_next_game);
  }
  if (detail_buf[0]) {
    graphics_draw_text(ctx, detail_buf, f_small, GRect(hpad, detail_y, w - 2 * hpad, 20),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  // Last play
  if (live_now && s_last_play[0]) {
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, s_last_play, f_tiny, GRect(hpad, lp_y, w - 2 * hpad, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  // Field position bar
  draw_field_bar(ctx, hpad, fb_y, w - 2 * hpad, fb_h, f_tiny);
}

// ── Clock ────────────────────────────────────────────────────────────────
static void update_clock(struct tm *t) {
  clock_copy_time_string(s_time_buf, sizeof(s_time_buf));
  strftime(s_date_buf, sizeof(s_date_buf), "%a  %b %d", t);
}

static void tick_handler(struct tm *t, TimeUnits units) {
  update_clock(t);
  if (units & MINUTE_UNIT) request_game_data();
  if (s_canvas) layer_mark_dirty(s_canvas);
}

// ── Vibration ────────────────────────────────────────────────────────────
// 1 short buzz for a field goal or safety, 3 short buzzes for a touchdown.
// No buzz at all for PATs / two-point tries (they never produce a distinct
// score_event — see checkScoreEvent() in index.js).
static void fire_score_vibe(int score_event) {
  if (!s_vibrate || score_event <= 0) return;
  if (score_event == 2) {
    static const uint32_t segments[] = {80, 120, 80, 120, 80};
    VibePattern pat = {
      .durations = segments,
      .num_segments = ARRAY_LENGTH(segments),
    };
    vibes_enqueue_custom_pattern(pat);
  } else {
    vibes_short_pulse();
  }
}

// ── Inbox ──────────────────────────────────────────────────────────────────
static void inbox_received(DictionaryIterator *iter, void *ctx) {
  Tuple *t;

  t = dict_find(iter, KEY_AWAY_ABBR);
  if (t) { strncpy(s_away_abbr, t->value->cstring, 4); s_away_abbr[4] = 0; }
  t = dict_find(iter, KEY_HOME_ABBR);
  if (t) { strncpy(s_home_abbr, t->value->cstring, 4); s_home_abbr[4] = 0; }
  update_team_logos();
  t = dict_find(iter, KEY_AWAY_SCORE);  if (t) s_away_score = (int)t->value->int32;
  t = dict_find(iter, KEY_HOME_SCORE);  if (t) s_home_score = (int)t->value->int32;
  t = dict_find(iter, KEY_QUARTER);     if (t) s_quarter    = (int)t->value->int32;
  t = dict_find(iter, KEY_CLOCK);
  if (t) { strncpy(s_clock, t->value->cstring, 7); s_clock[7] = 0; }
  t = dict_find(iter, KEY_DOWN);        if (t) s_down     = (int)t->value->int32;
  t = dict_find(iter, KEY_DISTANCE);    if (t) s_distance = (int)t->value->int32;
  t = dict_find(iter, KEY_DOWN_TEXT);
  if (t) { strncpy(s_down_text, t->value->cstring, 23); s_down_text[23] = 0; }
  t = dict_find(iter, KEY_FIELD_POS);   if (t) s_field_pos  = (int)t->value->int32;
  t = dict_find(iter, KEY_REDZONE);     if (t) s_redzone    = (bool)t->value->int32;
  t = dict_find(iter, KEY_POSSESSION);  if (t) s_possession = (int)t->value->int32;
  t = dict_find(iter, KEY_STATUS);
  if (t) { strncpy(s_status, t->value->cstring, 7); s_status[7] = 0; }
  t = dict_find(iter, KEY_START_TIME);
  if (t) { strncpy(s_start_time, t->value->cstring, 9); s_start_time[9] = 0; }
  t = dict_find(iter, KEY_AWAY_RECORD);
  if (t) { strncpy(s_away_record, t->value->cstring, 9); s_away_record[9] = 0; }
  t = dict_find(iter, KEY_HOME_RECORD);
  if (t) { strncpy(s_home_record, t->value->cstring, 9); s_home_record[9] = 0; }
  t = dict_find(iter, KEY_VIBRATE);
  if (t) { s_vibrate = (bool)t->value->int32; persist_write_bool(PERSIST_VIB, s_vibrate); }
  t = dict_find(iter, KEY_LAST_PLAY);
  if (t) { strncpy(s_last_play, t->value->cstring, 43); s_last_play[43] = 0; }
  t = dict_find(iter, KEY_NEXT_GAME);
  if (t) { strncpy(s_next_game, t->value->cstring, 31); s_next_game[31] = 0; }
  t = dict_find(iter, KEY_BATTERY_BAR);
  if (t) { s_battery_bar = (bool)t->value->int32; persist_write_bool(PERSIST_BAT, s_battery_bar); }
  t = dict_find(iter, KEY_TEAM_LOGOS);
  if (t) { s_team_logos = (bool)t->value->int32; persist_write_bool(PERSIST_TEAM_LOGOS, s_team_logos); }
  t = dict_find(iter, KEY_TICKER_SPEED);
  if (t) {
    int spd = (int)t->value->int32;
    // Accept only valid values: 5000, 10000, 30000, 60000
    if (spd == 5000 || spd == 10000 || spd == 30000 || spd == 60000) {
      s_ticker_speed = spd;
      persist_write_int(PERSIST_TICKER_SPEED, s_ticker_speed);
      if (s_ticker_timer) {
        app_timer_cancel(s_ticker_timer);
        s_ticker_timer = app_timer_register((uint32_t)s_ticker_speed, ticker_advance, NULL);
      }
    }
  }
  t = dict_find(iter, KEY_NETWORK);
  if (t) { strncpy(s_network, t->value->cstring, 23); s_network[23] = 0; }
  t = dict_find(iter, KEY_FEATURED_TAG);
  if (t) { strncpy(s_featured_tag, t->value->cstring, 4); s_featured_tag[4] = 0; }
  t = dict_find(iter, KEY_TICKER);
  if (t) {
    strncpy(s_ticker_raw, t->value->cstring, sizeof(s_ticker_raw) - 1);
    s_ticker_raw[sizeof(s_ticker_raw) - 1] = 0;
    ticker_parse();
  }
  t = dict_find(iter, KEY_TEAM_IDX);
  if (t) { s_team_idx = (int)t->value->int32; persist_write_int(PERSIST_TEAM, s_team_idx); }

  int score_event = 0;
  t = dict_find(iter, KEY_SCORE_EVENT);
  if (t) score_event = (int)t->value->int32;
  fire_score_vibe(score_event);

  if (s_canvas) layer_mark_dirty(s_canvas);
}

static void inbox_dropped(AppMessageResult reason, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Inbox dropped: %d", (int)reason);
}

static void request_game_data(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) return;
  dict_write_int(iter, KEY_TEAM_IDX, &s_team_idx, sizeof(int), true);
  app_message_outbox_send();
}

static void battery_handler(BatteryChargeState state) {
  s_battery_pct = state.charge_percent;
  if (s_canvas) layer_mark_dirty(s_canvas);
}

// ── Window ───────────────────────────────────────────────────────────────
static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);

  s_football_bmp = gbitmap_create_with_resource(FOOTBALL_RESOURCE);
}

static void window_unload(Window *window) {
  if (s_ticker_timer) { app_timer_cancel(s_ticker_timer); s_ticker_timer = NULL; }
  if (s_canvas) { layer_destroy(s_canvas); s_canvas = NULL; }
  if (s_football_bmp) { gbitmap_destroy(s_football_bmp); s_football_bmp = NULL; }
  if (s_away_logo_lg) { gbitmap_destroy(s_away_logo_lg); s_away_logo_lg = NULL; }
  if (s_away_logo_sm) { gbitmap_destroy(s_away_logo_sm); s_away_logo_sm = NULL; }
  if (s_home_logo_lg) { gbitmap_destroy(s_home_logo_lg); s_home_logo_lg = NULL; }
  if (s_home_logo_sm) { gbitmap_destroy(s_home_logo_sm); s_home_logo_sm = NULL; }
}

static void init(void) {
  memset(s_ticker_raw, 0, sizeof(s_ticker_raw));

  if (persist_exists(PERSIST_TEAM))        s_team_idx    = persist_read_int(PERSIST_TEAM);
  if (persist_exists(PERSIST_VIB))         s_vibrate     = persist_read_bool(PERSIST_VIB);
  if (persist_exists(PERSIST_BAT))         s_battery_bar = persist_read_bool(PERSIST_BAT);
  if (persist_exists(PERSIST_TEAM_LOGOS))  s_team_logos  = persist_read_bool(PERSIST_TEAM_LOGOS);
  if (persist_exists(PERSIST_TICKER_SPEED)) s_ticker_speed = persist_read_int(PERSIST_TICKER_SPEED);

  time_t now = time(NULL);
  update_clock(localtime(&now));

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load, .unload = window_unload });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);
  s_battery_pct = battery_state_service_peek().charge_percent;

  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_open(512, 64);

  s_ticker_timer = app_timer_register((uint32_t)s_ticker_speed, ticker_advance, NULL);
  // Delay initial fetch so Clay's ready-event config send isn't competing with game data
  app_timer_register(2000, (AppTimerCallback)request_game_data, NULL);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) { init(); app_event_loop(); deinit(); return 0; }
