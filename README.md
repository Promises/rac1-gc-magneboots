# Going Commando Magneboots

A native mod for [OpenRAC](https://github.com/OpenRAC/OpenRAC)'s port of Ratchet & Clank: on
magnetic surfaces Ratchet walks and runs at his normal speed, and turns without stopping, like
the Gravity Boots in Going Commando.

In Ratchet & Clank the Magneboots have a slow walk of their own: about 3.5 units a second against
the run's 5.7, and Ratchet stops dead when the stick points more than 70 degrees away from where he
faces, until he has turned. With the mod the magnetic surface is plain ground for his speed: walk
or run by how far the stick is pushed, in any direction.

**0.1.0 is a first step.** Still the original game's on magnetic surfaces, because the OpenRAC mod
interface has no hook point that reaches them yet:

- **Jumping.** The game puts Ratchet back into the magnet walk every tick and turns X into a hop.
- **The run animation.** The magnet walk keeps its own, played faster as he goes faster.
- **Weapons.** The game locks them on magnetic surfaces in the same function that uses them.

For the PAL and NTSC-U discs, OpenRAC 1.1.0 or later (mod interface 1.20).

## How it works

The magnet walk (hero state 0x3F) works out its target speed, then eases Ratchet's ground speed
toward it through `speed_step`, the SDK's `rac1.player.approach_speed` (the same function in every
level). The mod replaces the target just before that, in that state only, with the plain-ground
one: the hero speed table's walk or run speed, read from the game, by how far the stick is pushed.
The easing is the same as on plain ground.

## Build

```sh
python3 tools/fetch_sdk.py          # the SDK sdk.lock pins, into .sdk/
cmake -S . -B build -G Ninja
cmake --build build && ctest --test-dir build
python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py build/package/gc_magneboots
```

## License

MIT.
