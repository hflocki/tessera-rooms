#include "rooms.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

namespace esphome::rooms {

using tessera::Font;
using tessera::Tone;
namespace ui = tessera::ui;

// ---- What the tile switches ---------------------------------------------------------------------------------------

static constexpr int MAX_ROWS = 12;   // rows a tile shows at most
static constexpr int MANUAL = 8;      // entity options e1 to e8
static constexpr int SKIPS = 8;       // words of the option "skip"
static const char *const MANUAL_KEYS[MANUAL] = {"e1", "e2", "e3", "e4", "e5", "e6", "e7", "e8"};

// The domains a row can switch with homeassistant.toggle, with their icons for on and off.
struct Domain {
  const char *prefix;
  uint32_t icon_on, icon_off;
  bool lamp;  // a light: its icon on in amber
};
static const Domain DOMAINS[] = {
    {"light.", 0xF0335, 0xF0E4F, true},           // lightbulb / lightbulb-off
    {"switch.", 0xF06A5, 0xF06A6, false},         // power-plug / power-plug-off
    {"fan.", 0xF0210, 0xF081D, false},            // fan / fan-off
    {"input_boolean.", 0xF0521, 0xF1A26, false},  // toggle-switch / toggle-switch-variant-off
};

static const Domain *domain_of(const char *entity) {
  for (const Domain &d : DOMAINS)
    if (strncmp(entity, d.prefix, strlen(d.prefix)) == 0) return &d;
  return nullptr;
}

// The room's icons by the value of the option "icon".
struct RoomIcon {
  const char *id;
  uint32_t codepoint;
};
static const RoomIcon ROOM_ICONS[] = {
    {"home", 0xF02DC},          {"sofa", 0xF04B9},         {"bed", 0xF02E3},
    {"chef-hat", 0xF0B7C},      {"silverware-fork-knife", 0xF0A70},
    {"bathtub", 0xF1818},       {"shower", 0xF09A0},       {"toilet", 0xF09AB},
    {"desk", 0xF1239},          {"garage", 0xF06D9},       {"tree", 0xF0531},
    {"flower", 0xF024A},        {"account-child", 0xF0A89}, {"washing-machine", 0xF072A},
    {"television", 0xF0502},    {"fireplace", 0xF0E2E},    {"lightbulb-group", 0xF1253},
    {"power-plug", 0xF06A5},    {"grill", 0xF0E45},        {"pool", 0xF0606},
    {"car", 0xF010B},           {"dog", 0xF0A43},
};

static uint32_t icon_by_id(const char *id) {
  for (const RoomIcon &i : ROOM_ICONS)
    if (strcmp(i.id, id) == 0) return i.codepoint;
  return 0;
}

static std::string lower(const char *s) {
  std::string out(s ? s : "");
  for (char &c : out) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return out;
}

// "auto": the icon from words in the room's title or area, in English and German.
static uint32_t guess_icon(const std::string &words) {
  static const struct {
    const char *word, *icon;
  } BY_WORD[] = {
      {"wohn", "sofa"},        {"living", "sofa"},       {"lounge", "sofa"},     {"schlaf", "bed"},
      {"bed", "bed"},          {"gast", "bed"},          {"guest", "bed"},       {"küche", "chef-hat"},
      {"kueche", "chef-hat"},  {"kitchen", "chef-hat"},  {"ess", "silverware-fork-knife"},
      {"dining", "silverware-fork-knife"},               {"dusch", "shower"},    {"shower", "shower"},
      {"wc", "toilet"},        {"toilet", "toilet"},     {"bad", "bathtub"},     {"bath", "bathtub"},
      {"büro", "desk"},        {"buero", "desk"},        {"office", "desk"},     {"arbeit", "desk"},
      {"study", "desk"},       {"garage", "garage"},     {"carport", "car"},     {"garten", "tree"},
      {"garden", "tree"},      {"terrasse", "tree"},     {"balkon", "flower"},   {"balcony", "flower"},
      {"patio", "tree"},       {"kind", "account-child"}, {"kid", "account-child"},
      {"child", "account-child"}, {"nursery", "account-child"},                  {"wasch", "washing-machine"},
      {"laundry", "washing-machine"}, {"keller", "washing-machine"},             {"basement", "washing-machine"},
      {"tv", "television"},    {"fernseh", "television"}, {"kino", "television"}, {"media", "television"},
      {"pool", "pool"},
  };
  for (const auto &m : BY_WORD)
    if (words.find(m.word) != std::string::npos) return icon_by_id(m.icon);
  return icon_by_id("home");
}

// An entity's object id as words, for an entity whose name is not known yet: "light.living_floor_lamp" in the area
// "living" is "Floor lamp".
static std::string words_of(const char *entity, const char *area) {
  const char *dot = strchr(entity, '.');
  std::string s = dot ? dot + 1 : entity;
  const size_t al = area ? strlen(area) : 0;
  if (al && s.size() > al + 1 && s.compare(0, al, area) == 0 && s[al] == '_') s = s.substr(al + 1);
  for (char &c : s)
    if (c == '_') c = ' ';
  if (!s.empty()) s[0] = static_cast<char>(toupper(static_cast<unsigned char>(s[0])));
  return s;
}

static bool is_on(const char *state) { return strcmp(state, "on") == 0; }
static bool has_value(const char *state) {
  return state[0] && strcmp(state, "unavailable") != 0 && strcmp(state, "unknown") != 0;
}

// ---- Drawing helpers ----------------------------------------------------------------------------------------------

static void place(lv_obj_t *obj, int x, int y, int w, int h) {
  lv_obj_set_size(obj, std::max(1, w), std::max(1, h));
  lv_obj_set_pos(obj, x, y);
}

static void set_shown(lv_obj_t *obj, bool shown) {
  if (!obj) return;
  if (shown == lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
    if (shown) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
}

static void set_bg_opa(lv_obj_t *obj, lv_opa_t opa) {
  if (lv_obj_get_style_bg_opa(obj, LV_PART_MAIN) != opa) lv_obj_set_style_bg_opa(obj, opa, 0);
}

static void set_x(lv_obj_t *obj, int x, int &kept) {
  if (kept != x) {
    lv_obj_set_x(obj, x);
    kept = x;
  }
}

// A colour's luminance and whether it reads on the card (WCAG 3:1): amber on the light look's white card does not.
static float luminance(lv_color_t c) {
  auto channel = [](uint8_t v) {
    const float s = v / 255.0f;
    return s <= 0.03928f ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f);
  };
  return 0.2126f * channel(c.red) + 0.7152f * channel(c.green) + 0.0722f * channel(c.blue);
}
static bool reads_on_card(lv_color_t c) {
  const float a = luminance(c), b = luminance(ui::color(theme::CARD));
  return (std::max(a, b) + 0.05f) / (std::min(a, b) + 0.05f) >= 3.0f;
}

// ---- The tile -----------------------------------------------------------------------------------------------------

// One entity as the tile shows it: from an option of the tile, or found in its area.
struct Row {
  char id[64];
  char name[48];
  char state[16];
  bool from_area;
};

// A switch a finger turned, shown at once until Home Assistant reports the new state or a few seconds passed.
struct Pending {
  char id[64];
  char before[16];
  bool target;
  uint32_t until;
};

// The parts of one row: a soft block, the entity's icon, its name, a switch at the right or a word where it has none.
struct RowParts {
  lv_obj_t *block, *icon, *name, *track, *knob, *note;
  int knob_off, knob_on, knob_x;
};

// A room: its title with an icon or a picture at the top, and its switches under it, in one to three columns.
class RoomTile : public tessera::Tile {
 public:
  explicit RoomTile(RoomsPlugin *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    snprintf(entity_, sizeof entity_, "%s", c.entity ? c.entity : "");
    snprintf(area_, sizeof area_, "%s", c.options["area"] | "");
    // An area as Home Assistant keys it: lowercase, words joined with "_".
    for (char *p = area_; *p; ++p) *p = *p == ' ' ? '_' : static_cast<char>(tolower(static_cast<unsigned char>(*p)));

    // The title: the option, else the tile's name, else the area in words, else "Room".
    const char *title = c.options["title"] | "";
    if (!*title) title = c.name ? c.name : "";
    if (*title) snprintf(title_, sizeof title_, "%s", title);
    else if (area_[0]) snprintf(title_, sizeof title_, "%s", words_of(area_, "").c_str());
    else snprintf(title_, sizeof title_, "%s", plugin_->text("room"));

    const char *icon = c.options["icon"] | "auto";
    icon_ = icon_by_id(icon);
    if (!icon_) icon_ = guess_icon(lower(title_) + " " + lower(area_));

    for (int k = 0; k < MANUAL; ++k) {
      snprintf(manual_[k].id, sizeof manual_[k].id, "%s", c.options[MANUAL_KEYS[k]] | "");
      manual_[k].name[0] = manual_[k].state[0] = 0;
      manual_[k].from_area = false;
    }
    skips_ = 0;
    for (JsonVariantConst w : c.options["skip"].as<JsonArrayConst>()) {
      const char *word = w | "";
      if (*word && skips_ < SKIPS) snprintf(skip_[skips_++], sizeof skip_[0], "%s", lower(word).c_str());
    }

    lay_out_head(c);
    for (int i = 0; i < MAX_ROWS; ++i) make_row(c.parent, parts_[i]);
    note_ = ui::text(c.parent, Font::BODY, theme::MUTED);
    place(note_, 0, rows_y_, width_, std::max(1, height_ - rows_y_));
    rebuild();
    paint();
  }

  void on_state(JsonObjectConst data) override {
    // The picture of the tile's own entity (an image, a camera or a person), when it has one.
    if (picture_) {
      std::string url;
      const char *pic = data["attributes"]["entity_picture"] | "";
      if (entity_[0]) url = (strncmp(entity_, "camera.", 7) == 0 ? "camera:" : "entity:") + std::string(entity_);
      else if (strncmp(pic, "/api/", 5) == 0 || strncmp(pic, "/local/", 7) == 0) url = pic;
      ui::set_picture(picture_, url, tessera::Fit::COVER);
    }
    JsonObjectConst entities = data["entities"];
    for (int k = 0; k < MANUAL; ++k) {
      if (!manual_[k].id[0]) continue;
      JsonObjectConst e = entities[MANUAL_KEYS[k]];
      snprintf(manual_[k].state, sizeof manual_[k].state, "%s", e["state"] | "");
      const char *name = e["name"] | "";
      if (*name) snprintf(manual_[k].name, sizeof manual_[k].name, "%s", name);
    }
    rebuild();
    paint();
  }

  void on_tick(uint32_t epoch) override {
    epoch_ = epoch;
    if (area_[0]) {
      area_info_ = plugin_->follow(area_, epoch);
      const uint32_t v = area_info_ ? area_info_->version + 1 : 0;
      if (v != area_version_) {
        area_version_ = v;
        rebuild();
      }
    }
    paint();
  }

  void on_theme() override { paint(); }

  void on_tap() override {
    const tessera::Point p = tessera::tap_point();
    for (int i = 0; i < visible_; ++i) {
      const Rect &r = rects_[i];
      if (p.x < r.x || p.x >= r.x + r.w || p.y < r.y || p.y >= r.y + r.h) continue;
      const Row &row = rows_[i];
      if (!has_value(row.state)) return;
      if (!tessera::action("homeassistant.toggle", row.id)) return;
      remember(row, !shown_on(row));
      if (row.from_area) plugin_->soon(area_);
      paint();
      return;
    }
  }

 private:
  struct Rect {
    int x, y, w, h;
  };

  // The head: a picture over the whole width on a large tile, else an icon or a small picture beside the title, and
  // how many are on at the right.
  void lay_out_head(const tessera::TileContext &c) {
    const bool big = height_ >= ui::mm(55) && width_ >= ui::mm(45);
    const Font title_font = big ? Font::HEADLINE : Font::TITLE;
    const Font icon_font = big ? Font::ICON : Font::ICON_SMALL;
    const bool with_picture = entity_[0] && ui::pictures();
    int head_h = std::max(ui::line_height(title_font), ui::line_height(icon_font));
    // A small picture beside the title is a little larger than the title's line, to be recognised.
    if (with_picture && !big) head_h = std::max(head_h, ui::mm(8));
    int y = 0, x = 0;
    if (with_picture && big) {
      const int banner = std::min(height_ / 3, ui::mm(24));
      picture_ = ui::picture(c.parent);
      place(picture_, 0, 0, width_, banner);
      y = banner + ui::px(6);
    } else if (with_picture) {
      picture_ = ui::picture(c.parent);
      place(picture_, 0, 0, head_h, head_h);
      x = head_h + ui::px(8);
    }
    if (!picture_ || big) {
      icon_label_ = ui::label(c.parent, icon_font, theme::INK_SOFT);
      const int iw = ui::line_height(icon_font);
      place(icon_label_, x, y + (head_h - ui::line_height(icon_font)) / 2, iw, ui::line_height(icon_font));
      ui::set_text(icon_label_, ui::icon(icon_));
      x += iw + ui::px(8);
    }
    // How many are on, at the right of a tile wide enough for it: room for "12 on +12".
    count_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    lv_obj_set_style_text_align(count_, LV_TEXT_ALIGN_RIGHT, 0);
    const std::string widest = tessera::format(plugin_->text("on_count"), 12) + " " +
                               tessera::format(plugin_->text("more"), 12);
    const int cw = ui::text_width(widest, Font::BODY) + ui::px(4);
    const bool counted = width_ - x - cw >= ui::mm(14);
    title_label_ = ui::label(c.parent, title_font, theme::INK);
    const int tw = width_ - x - (counted ? cw + ui::px(6) : 0);
    place(title_label_, x, y + (head_h - ui::line_height(title_font)) / 2, tw, ui::line_height(title_font));
    if (counted) place(count_, width_ - cw, y + (head_h - ui::line_height(Font::BODY)) / 2, cw,
                       ui::line_height(Font::BODY));
    else set_shown(count_, false);
    // A long title steps down a size rather than being cut.
    Font f = title_font;
    if (ui::text_width(title_, f) > tw && f == Font::HEADLINE) f = Font::TITLE;
    if (ui::text_width(title_, f) > tw) f = Font::BODY_LARGE;
    ui::set_font(title_label_, f);
    ui::set_text(title_label_, title_);
    rows_y_ = y + head_h + ui::px(8);
  }

  void make_row(lv_obj_t *parent, RowParts &p) {
    p.block = ui::block(parent, theme::TRACK);
    lv_obj_set_style_pad_all(p.block, 0, 0);
    lv_obj_remove_flag(p.block, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(p.block, LV_OBJ_FLAG_CLICKABLE);
    p.icon = ui::label(p.block, Font::ICON_SMALL, theme::MUTED);
    p.name = ui::label(p.block, Font::BODY, theme::INK);
    p.track = ui::block(p.block, theme::PANEL_TOGGLE_OFF);
    lv_obj_set_style_radius(p.track, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(p.track, 0, 0);
    lv_obj_remove_flag(p.track, LV_OBJ_FLAG_SCROLLABLE);
    p.knob = ui::block(p.track, theme::KNOB);
    lv_obj_set_style_radius(p.knob, LV_RADIUS_CIRCLE, 0);
    p.note = ui::label(p.block, Font::BODY, theme::MUTED);
    lv_obj_set_style_text_align(p.note, LV_TEXT_ALIGN_RIGHT, 0);
    p.knob_off = p.knob_on = 0;
    p.knob_x = -1;
    lv_obj_add_flag(p.block, LV_OBJ_FLAG_HIDDEN);
  }

  // Where the rows go for `n` of them: as many as fit under each other, in up to three columns on a wide tile, each
  // at least a finger high. Only the positions change: the parts were all made in create().
  void lay_out_rows(int n) {
    laid_ = n;
    const int gap = ui::px(6);
    const int room = height_ - rows_y_;
    const int min_h = ui::mm(8), max_h = ui::mm(12);
    const int fit = room >= min_h ? (room + gap) / (min_h + gap) : 0;
    const int most_cols = std::max(1, std::min(3, width_ / ui::mm(30)));
    int cols = 1;
    if (fit > 0 && n > fit) cols = std::min(most_cols, (n + fit - 1) / fit);
    visible_ = std::min(n, cols * fit);
    const int per_col = visible_ ? (visible_ + cols - 1) / cols : 1;
    cols = visible_ ? (visible_ + per_col - 1) / per_col : 1;
    const int rh = std::min(max_h, (room - gap * (per_col - 1)) / std::max(1, per_col));
    const int cw = (width_ - gap * (cols - 1)) / cols;
    const bool roomy = rh >= ui::mm(10);
    for (int i = 0; i < MAX_ROWS; ++i) {
      RowParts &p = parts_[i];
      if (i >= visible_) {
        set_shown(p.block, false);
        continue;
      }
      Rect &r = rects_[i];
      r = {(i / per_col) * (cw + gap), rows_y_ + (i % per_col) * (rh + gap), cw, rh};
      place(p.block, r.x, r.y, r.w, r.h);
      const int pad = ui::px(8);
      const int ih = ui::line_height(Font::ICON_SMALL);
      place(p.icon, pad, (rh - ih) / 2, ih, ih);
      const int nx = pad + ih + ui::px(8);
      // At the right the switch, or the word of an unavailable entity in the same room. A row too narrow for both
      // the switch and a name keeps the name: its icon and its colour still say on or off.
      const int th = std::max(ui::px(20), std::min(ui::px(34), rh / 2));
      const int tw = th * 9 / 5;
      const int right = std::min(cw / 2, std::max(tw, ui::text_width(plugin_->text("unavailable"), Font::BODY) + ui::px(4)));
      toggles_ = cw - pad - right - ui::px(6) - nx >= ui::mm(10);
      const int rw = toggles_ ? right : 0;
      const int tx = cw - pad - tw;
      place(p.track, tx, (rh - th) / 2, tw, th);
      const int kp = ui::px(3), ks = th - 2 * kp;
      p.knob_off = kp;
      p.knob_on = tw - kp - ks;
      place(p.knob, p.knob_off, kp, ks, ks);
      p.knob_x = p.knob_off;
      const Font nf = roomy ? Font::BODY_LARGE : Font::BODY;
      ui::set_font(p.name, nf);
      const int name_w = std::max(1, cw - pad - (rw ? rw + ui::px(6) : 0) - nx);
      place(p.name, nx, (rh - ui::line_height(nf)) / 2, name_w, ui::line_height(nf));
      place(p.note, cw - pad - right, (rh - ui::line_height(Font::BODY)) / 2, right, ui::line_height(Font::BODY));
    }
  }

  // The rows: the tile's own entities first, then those of the area it does not have yet, without the skipped ones.
  void rebuild() {
    int n = 0;
    for (int k = 0; k < MANUAL && n < MAX_ROWS; ++k) {
      const Row &m = manual_[k];
      if (!m.id[0] || !domain_of(m.id) || listed(m.id, n)) continue;
      rows_[n] = m;
      if (!m.name[0]) snprintf(rows_[n].name, sizeof rows_[n].name, "%s", words_of(m.id, area_).c_str());
      ++n;
    }
    if (area_info_) {
      for (int i = 0; i < area_info_->count && n < MAX_ROWS; ++i) {
        const AreaEntity &e = area_info_->entity[i];
        if (listed(e.id, n) || skipped(e)) continue;
        Row &r = rows_[n++];
        snprintf(r.id, sizeof r.id, "%s", e.id);
        if (e.name[0]) snprintf(r.name, sizeof r.name, "%s", e.name);
        else snprintf(r.name, sizeof r.name, "%s", words_of(e.id, area_).c_str());
        snprintf(r.state, sizeof r.state, "%s", e.state);
        r.from_area = true;
      }
    }
    count_rows_ = n;
  }

  bool listed(const char *id, int n) const {
    for (int i = 0; i < n; ++i)
      if (strcmp(rows_[i].id, id) == 0) return true;
    return false;
  }

  bool skipped(const AreaEntity &e) const {
    if (!skips_) return false;
    const std::string id = lower(e.id), name = lower(e.name);
    for (int i = 0; i < skips_; ++i)
      if (id.find(skip_[i]) != std::string::npos || name.find(skip_[i]) != std::string::npos) return true;
    return false;
  }

  void remember(const Row &row, bool target) {
    Pending *slot = nullptr;
    for (Pending &p : pending_)
      if (strcmp(p.id, row.id) == 0) slot = &p;
    if (!slot)
      for (Pending &p : pending_)
        if (!p.id[0] || p.until < epoch_) {
          slot = &p;
          break;
        }
    if (!slot) slot = &pending_[0];
    snprintf(slot->id, sizeof slot->id, "%s", row.id);
    snprintf(slot->before, sizeof slot->before, "%s", row.state);
    slot->target = target;
    slot->until = epoch_ + 5;
  }

  // On or off as the row shows it: what a finger just chose, until Home Assistant reports a change or 5 s passed.
  bool shown_on(const Row &row) {
    for (Pending &p : pending_) {
      if (!p.id[0] || strcmp(p.id, row.id) != 0) continue;
      if (p.until < epoch_ || strcmp(p.before, row.state) != 0) {
        p.id[0] = 0;
        break;
      }
      return p.target;
    }
    return is_on(row.state);
  }

  void paint() {
    if (count_rows_ != laid_) lay_out_rows(count_rows_);
    int on = 0;
    for (int i = 0; i < count_rows_; ++i)
      if (has_value(rows_[i].state) && shown_on(rows_[i])) ++on;
    for (int i = 0; i < visible_; ++i) paint_row(parts_[i], rows_[i]);

    std::string count = on ? tessera::format(plugin_->text("on_count"), on) : std::string(plugin_->text("all_off"));
    if (count_rows_ > visible_ && visible_ > 0)
      count += " " + tessera::format(plugin_->text("more"), count_rows_ - visible_);
    ui::set_text(count_, count_rows_ ? count : std::string());
    ui::set_color(count_, on ? theme::INK_SOFT : theme::MUTED);
    ui::set_color(title_label_, theme::INK);
    if (icon_label_) ui::set_color(icon_label_, on ? theme::INK : theme::INK_SOFT);

    // Nothing to show yet: say why.
    const char *why = "";
    if (!count_rows_) {
      if (!area_[0]) why = plugin_->text("choose");
      else if (!area_info_ || (!area_info_->found && !area_info_->failed)) why = plugin_->text("searching");
      else if (!area_info_->found) why = plugin_->text("area_failed");
      else why = plugin_->text("empty_area");
    }
    set_shown(note_, *why && height_ - rows_y_ >= ui::line_height(Font::BODY));
    ui::set_text(note_, why);
  }

  void paint_row(RowParts &p, const Row &row) {
    set_shown(p.block, true);
    const Domain *d = domain_of(row.id);
    const bool value = has_value(row.state);
    const bool on = value && shown_on(row);
    ui::set_text(p.name, row.name);
    ui::set_color(p.name, value ? theme::INK : theme::MUTED);
    ui::set_text(p.icon, ui::icon(on ? d->icon_on : d->icon_off));
    if (on && d->lamp && reads_on_card(ui::color(theme::MARK_AMBER))) ui::set_color(p.icon, theme::MARK_AMBER);
    else if (on) ui::set_tone(p.icon, Tone::ACCENT, theme::ACCENT);
    else ui::set_color(p.icon, value ? theme::MUTED : theme::SUBTLE);
    // The row lightly in the accent while on.
    if (on) {
      ui::set_fill(p.block, Tone::ACCENT, theme::ACCENT);
      set_bg_opa(p.block, LV_OPA_20);
    } else {
      ui::set_fill(p.block, Tone::NORMAL, theme::TRACK);
      set_bg_opa(p.block, LV_OPA_50);
    }
    // The switch while the entity has a state; a word where it is unavailable; nothing while its state is on its way.
    set_shown(p.track, value && toggles_);
    set_shown(p.note, !value && row.state[0] && toggles_);
    if (!value) {
      ui::set_text(p.note, row.state[0] ? plugin_->text("unavailable") : "");
      return;
    }
    if (on) ui::set_fill(p.track, Tone::ACCENT, theme::ACCENT);
    else ui::set_fill(p.track, Tone::NORMAL, theme::PANEL_TOGGLE_OFF);
    ui::set_fill(p.knob, Tone::NORMAL, theme::KNOB);
    set_x(p.knob, on ? p.knob_on : p.knob_off, p.knob_x);
  }

  RoomsPlugin *plugin_;
  const Area *area_info_ = nullptr;
  uint32_t area_version_ = 0, epoch_ = 0;
  int width_ = 0, height_ = 0, rows_y_ = 0;
  int count_rows_ = 0, visible_ = 0, laid_ = -1, skips_ = 0;
  bool toggles_ = true;
  uint32_t icon_ = 0;
  char entity_[96]{}, area_[48]{}, title_[64]{};
  char skip_[SKIPS][24]{};
  Row manual_[MANUAL]{};
  Row rows_[MAX_ROWS]{};
  Rect rects_[MAX_ROWS]{};
  Pending pending_[4]{};
  RowParts parts_[MAX_ROWS]{};
  lv_obj_t *picture_{}, *icon_label_{}, *title_label_{}, *count_{}, *note_{};
};

// ---- The plugin: what it asks Home Assistant about an area ---------------------------------------------------------

// Each area goes through these: which entities it has, their states, then their names one by one, then its states
// again every POLL seconds while a tile shows it.
enum Phase : uint8_t { RELATED, STATES, NAMES, IDLE };
static constexpr uint32_t POLL = 15;        // seconds between two questions for the states of an area
static constexpr uint32_t PATIENCE = 20;    // seconds an answer may take
static constexpr uint32_t RETRY = 60;       // seconds before an area Home Assistant did not answer is asked again
static constexpr uint32_t UNSEEN = 30;      // seconds without a tile before an area's place goes to another
static constexpr size_t REQUEST = 480;      // bytes of a question at most (the app takes 512)

void RoomsPlugin::setup() {
  areas_.resize(AREAS);
  for (Area &a : areas_) memset(&a, 0, sizeof a);
  add_tile("room", [this]() { return new RoomTile(this); });
}

Area *RoomsPlugin::find(const char *area) {
  for (Area &a : areas_)
    if (a.id[0] && strcmp(a.id, area) == 0) return &a;
  return nullptr;
}

const Area *RoomsPlugin::follow(const char *area, uint32_t now) {
  if (!area || !*area || areas_.empty()) return nullptr;
  Area *a = find(area);
  if (!a) {
    // A free place, or the one no tile has shown for the longest while.
    for (Area &b : areas_)
      if (!b.id[0] || (now && b.seen + UNSEEN < now && (!a || b.seen < a->seen))) a = &b;
    if (!a) return nullptr;
    const uint32_t version = a->version + 1;
    memset(a, 0, sizeof *a);
    snprintf(a->id, sizeof a->id, "%s", area);
    a->version = version;
  }
  a->seen = now;
  pump(*a, now);
  return a;
}

void RoomsPlugin::soon(const char *area) {
  Area *a = find(area);
  if (a && a->phase == IDLE) a->due = std::min(a->due, tessera::epoch() + 2);
}

void RoomsPlugin::pump(Area &a, uint32_t now) {
  if (!now) return;
  if (a.asked) {
    if (now - a.asked_at < PATIENCE) return;
    a.asked = 0;   // no answer: ask again below
  }
  switch (a.phase) {
    case RELATED:
      if (now >= a.due) ask_related(a);
      break;
    case STATES:
      ask_states(a, now);
      break;
    case NAMES:
      ask_name(a, now);
      break;
    default:
      if (now >= a.due) {
        a.phase = STATES;
        a.next = 0;
        ask_states(a, now);
      }
      break;
  }
}

bool RoomsPlugin::ask_related(Area &a) {
  JsonDocument request;
  request["ask"] = "search/related";
  request["data"]["item_type"] = "area";
  request["data"]["item_id"] = a.id;
  a.asked = tessera::send(this, request.as<JsonObjectConst>());
  a.asked_at = tessera::epoch();
  return a.asked != 0;
}

// The states of the next entities that fit in one question, without their attributes: a small answer.
bool RoomsPlugin::ask_states(Area &a, uint32_t now) {
  if (a.next >= a.count) {
    a.phase = a.named ? IDLE : NAMES;
    a.next = 0;
    a.due = now + POLL;
    return a.phase == NAMES ? ask_name(a, now) : false;
  }
  JsonDocument request;
  request["ask"] = "history/history_during_period";
  JsonObject data = request["data"].to<JsonObject>();
  // From a minute ago: the state then, and any change since (the last one counts).
  const time_t from = static_cast<time_t>(now) - 60;
  struct tm utc;
  gmtime_r(&from, &utc);
  char start[24];
  strftime(start, sizeof start, "%Y-%m-%dT%H:%M:%SZ", &utc);
  data["start_time"] = start;
  data["minimal_response"] = true;
  data["no_attributes"] = true;
  data["significant_changes_only"] = false;
  JsonArray ids = data["entity_ids"].to<JsonArray>();
  a.chunk = 0;
  for (int i = a.next; i < a.count; ++i) {
    ids.add(a.entity[i].id);
    if (measureJson(request) > REQUEST && a.chunk) {
      ids.remove(ids.size() - 1);
      break;
    }
    ++a.chunk;
  }
  a.asked = tessera::send(this, request.as<JsonObjectConst>());
  a.asked_at = now;
  return a.asked != 0;
}

// The friendly name of one entity: its first state of the last minute comes with its attributes.
bool RoomsPlugin::ask_name(Area &a, uint32_t now) {
  if (a.next >= a.count) {
    a.phase = IDLE;
    a.named = true;
    a.due = now + POLL;
    return false;
  }
  JsonDocument request;
  request["ask"] = "history/history_during_period";
  JsonObject data = request["data"].to<JsonObject>();
  const time_t from = static_cast<time_t>(now) - 60;
  struct tm utc;
  gmtime_r(&from, &utc);
  char start[24];
  strftime(start, sizeof start, "%Y-%m-%dT%H:%M:%SZ", &utc);
  data["start_time"] = start;
  data["minimal_response"] = true;
  data["significant_changes_only"] = false;
  data["entity_ids"].to<JsonArray>().add(a.entity[a.next].id);
  a.chunk = 1;
  a.asked = tessera::send(this, request.as<JsonObjectConst>());
  a.asked_at = now;
  return a.asked != 0;
}

void RoomsPlugin::on_message(JsonObjectConst message) {
  const uint32_t re = message["re"] | 0u;
  if (!re) return;
  Area *a = nullptr;
  for (Area &b : areas_)
    if (b.id[0] && b.asked == re) a = &b;
  if (!a) return;
  const uint32_t now = tessera::epoch();
  a->asked = 0;
  const bool ok = message["ok"] | false;
  JsonObjectConst result = message["result"];
  switch (a->phase) {
    case RELATED:
      if (ok) {
        heard_related(*a, result);
      } else {
        a->failed = true;
        a->due = now + RETRY;
      }
      break;
    case STATES:
      if (ok) heard_history(*a, result);
      a->failed = !ok;
      a->next += std::max<uint8_t>(1, a->chunk);
      break;
    case NAMES:
      if (ok) heard_history(*a, result);
      a->next += 1;
      break;
    default:
      break;
  }
  ++a->version;
  pump(*a, now);
}

// search/related answers with every item that belongs to the area, by kind: {"entity": [...], "device": [...]}. The
// switchable entities are kept, in Home Assistant's order.
void RoomsPlugin::heard_related(Area &a, JsonObjectConst result) {
  AreaEntity before[AREA_MOST];
  const int had = a.count;
  memcpy(before, a.entity, sizeof before);
  a.count = 0;
  for (JsonVariantConst v : result["entity"].as<JsonArrayConst>()) {
    const char *id = v | "";
    // An id of 48 bytes or more came cut by the app: it would switch nothing.
    if (!domain_of(id) || strlen(id) >= 48 || a.count >= AREA_MOST) continue;
    bool twice = false;
    for (int i = 0; i < a.count; ++i) twice |= strcmp(a.entity[i].id, id) == 0;
    if (twice) continue;
    AreaEntity &e = a.entity[a.count++];
    memset(&e, 0, sizeof e);
    snprintf(e.id, sizeof e.id, "%s", id);
    for (int i = 0; i < had; ++i)
      if (strcmp(before[i].id, id) == 0) e = before[i];
  }
  a.found = true;
  a.failed = false;
  a.phase = STATES;
  a.next = 0;
}

// history/history_during_period answers {"<entity>": [{"s": "on", "a": {...}, "lu": ...}, ...]}, oldest first. The app
// cuts a key to 32 characters, so an entity is found by the start of its id.
void RoomsPlugin::heard_history(Area &a, JsonObjectConst result) {
  for (JsonPairConst kv : result) {
    const char *key = kv.key().c_str();
    const size_t kl = strlen(key);
    JsonArrayConst list = kv.value().as<JsonArrayConst>();
    if (!list.size()) continue;
    for (int i = 0; i < a.count; ++i) {
      AreaEntity &e = a.entity[i];
      const size_t il = strlen(e.id);
      if (kl != std::min<size_t>(il, 32) || strncmp(e.id, key, kl) != 0) continue;
      const char *state = list[list.size() - 1]["s"] | "";
      if (*state) snprintf(e.state, sizeof e.state, "%s", state);
      const char *name = list[0]["a"]["friendly_name"] | "";
      if (*name) snprintf(e.name, sizeof e.name, "%s", name);
    }
  }
}

}  // namespace esphome::rooms
