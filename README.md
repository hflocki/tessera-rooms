> in testing not ready

# Rooms

A room on a tile: its name at the top with an icon or a picture, and under it the lights, plugs, switches and fans of
the room, each with a switch to tap. Wide or tall, from a single cell to the whole page: the rows go under each other,
in up to three columns on a wide tile, each at least a finger high. The head says how many are on.

The switches come from two places, and you can use both on one tile:

- **Chosen on the tile**: up to eight lights, switches, fans or toggles (`input_boolean`), in the order you put them.
  Their states reach the screen at once when they change.
- **Found in an area**: name a Home Assistant area and the tile adds its lights, switches, fans and toggles under the
  ones above, with their names. An entity you chose yourself is not shown twice.

Everything is set on the tile itself, in the layout's inspector, without a build:

- **Title**: the room's name. Empty takes the tile's name, else the area's.
- **Icon**: Living room, Bedroom, Kitchen, Bathroom, Office, Garden and more. **Auto** picks one from the title or the
  area (English and German words: "Wohnzimmer", "Kitchen", "Bad", ...).
- **Picture**: the tile's own entity is its picture, when you choose one: an image, a camera or a person. On a large
  tile it lies across the top, on a smaller one it takes the icon's place. Screens without pictures (the CYD) show the
  icon.
- **Area**: the id of a Home Assistant area, such as `living_room`.
- **Leave out**: words for entities of the area that do not belong on the tile, such as `child_lock` or `indicator`
  (the switches many plugs bring for their lock and their LED).
- **Switch 1** to **Switch 8**: the entities you choose yourself.

A tap on a row switches its entity (`homeassistant.toggle`). The switch moves at once and follows Home Assistant
after.

## Set up

1. In Tessera, open **Plugins**, **Add with a link**, and paste the link of this repository. Add the plugin to a
   screen and build it once.
2. The screen must be allowed to perform actions: in Home Assistant, Settings, Devices and services, ESPHome, the
   screen's **Configure**, then **Allow the device to perform Home Assistant actions**.
3. In **Layout**, place a **Room** tile from the library's Plugins group, in the size you like (on a 5 x 5 grid, for
   example 2 x 5 for one room in a column, or 5 x 2 for a room across the page).
4. In the inspector, either choose the switches (**Switch 1**, **Switch 2**, ...), or fill in the **Area**, or both.
5. Give it a **Title** and an **Icon**, or choose an image, camera or person as the tile's entity for a picture.

### Finding an area's id

In Home Assistant, open Settings, Areas, labels and zones, and click the area. The end of the address in the browser is
its id: `.../config/areas/area/living_room` is `living_room`. An area you named "Wohnzimmer" usually has the id
`wohnzimmer`.

## Good to know

- **An area's switches are asked, not followed.** Home Assistant sends a screen changes only of entities chosen on a
  tile. The switches of an area are asked again every 15 seconds while the tile is on the glass, and a moment after a
  tap. A light switched elsewhere shows on the tile within those 15 seconds; choose it on the tile for at once.
- **An area's switches need the recorder.** Their states and names come from Home Assistant's history, so an entity
  the recorder leaves out shows without a state.
- An area lists every switch of its devices, the configuration ones too (a plug's child lock, its LED). **Leave out**
  takes them off; it starts with `child_lock` and `indicator`.
- An entity id of 48 characters or more is left out of an area: the Tessera app cuts such an id on its way.
- At most twelve rows per tile. When not all fit, the head says how many more there are (`+3`): make the tile larger,
  or split the room over two tiles.

## Privacy

The plugin talks to Home Assistant only, through the Tessera app: it asks which entities an area has and their
states, and switches the entity of a row you tap. Nothing leaves your home.

## A hobby project

This plugin is a private, non-commercial hobby project. I build and maintain it in my spare time and share it
free of charge as open source under the [MIT License](LICENSE). It is free and will stay free: there is no paid
version, no paid feature and no paid support.

Bug reports and ideas are welcome, but please keep in mind:

- There is no support and no guaranteed response time. I answer when I find the time, which can take days or weeks.
- Not every request gets built. I add what fits the plugin and what I can keep working over time.
- Pull requests are welcome; for a larger change, open an issue first so we can agree on the way.

### No warranty, no liability

The plugin is provided "as is", without any warranty and without guaranteed support, updates or fixes. You use it at
your own risk. To the extent permitted by law, I accept no liability for any damage or data loss resulting from its
use. The full terms are in the [MIT License](LICENSE).

### Buy me a coffee

If the plugin is useful to you and you would like to say thanks, you are welcome to buy me a coffee. A donation is
entirely voluntary: it is a thank-you, not a payment. It does not buy support, features, a response time or any other
service, and the plugin stays exactly the same for everyone, whether they donate or not.

[![Buy me a coffee](https://img.buymeacoffee.com/button-api/?text=Buy%20me%20a%20coffee&emoji=&slug=hflocki&button_colour=FFDD00&font_colour=000000&font_family=Cookie&outline_colour=000000&coffee_colour=ffffff)](https://www.buymeacoffee.com/hflocki)

☕ [buymeacoffee.com/hflocki](https://www.buymeacoffee.com/hflocki)
