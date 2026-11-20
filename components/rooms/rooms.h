#pragma once
#include "esphome/core/component.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::rooms {

// How many switchable entities the plugin keeps of one area, and how many areas it follows at once.
static constexpr int AREA_MOST = 16;
static constexpr int AREAS = 4;

// One switchable entity found in an area: its id, its friendly name once asked, and its last state.
struct AreaEntity {
  char id[64];
  char name[48];
  char state[16];
};

// What the plugin knows of one Home Assistant area. Every tile that names the same area shares it.
struct Area {
  char id[48];
  AreaEntity entity[AREA_MOST];
  uint8_t count;
  uint8_t phase;        // Phase in rooms.cpp
  uint8_t next;         // the next entity to ask for (states in chunks, names one by one)
  uint8_t chunk;        // how many entities the states question on its way asks for
  bool found;           // Home Assistant answered which entities the area has
  bool named;           // the names of every entity were asked once
  bool failed;          // Home Assistant did not answer the last question about the area
  uint32_t asked;       // the number of the question on its way, 0 for none
  uint32_t asked_at;    // when it was sent (screen clock)
  uint32_t due;         // when the states are asked again
  uint32_t seen;        // when a tile last showed the area
  uint32_t version;     // raised whenever something a tile shows changed
};

// The plugin: one tile type, "room", any number of times on a screen. A tile shows the switches it was given in its
// options, and those Home Assistant has in the area it names; the area's entities and states are asked here, once for
// every tile of that area.
class RoomsPlugin : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void on_message(JsonObjectConst message) override;

  // The area a tile shows, asked and kept up to date while some tile calls this (once a second, from its on_tick).
  const Area *follow(const char *area, uint32_t now);
  // A tile switched an entity of the area: ask its states again in a moment.
  void soon(const char *area);

 private:
  Area *find(const char *area);
  void pump(Area &a, uint32_t now);
  bool ask_related(Area &a);
  bool ask_states(Area &a, uint32_t now);
  bool ask_name(Area &a, uint32_t now);
  void heard_related(Area &a, JsonObjectConst result);
  void heard_history(Area &a, JsonObjectConst result);

  tessera::List<Area> areas_;
};

}  // namespace esphome::rooms
