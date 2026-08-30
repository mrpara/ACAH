# ACAH

A real-time 3D engine that renders to coloured ASCII characters, wrapped around
a game about piloting a ten-metre walking tank through a corporate war. You
build the machine out of parts and fight down fifteen linear contracts - mostly
against the small stuff: power-suit infantry, wheeled armour, gun tanks,
turrets and drones, with enemy spidertanks as the rare, serious exceptions -
wrecking everything billable on the way. Money follows destruction; the
workshop turns money into a better machine; a boss closes every act.

Everything is drawn by a multithreaded software rasterizer running on the CPU.
The GPU's only job is blitting the finished character grid — one instanced draw
call per frame.

```
  W A S D      move (relative to the camera)
  Space        hold to charge a jump - the charge is the height
  Mouse        aim / orbit          LMB + RMB   the two fire groups
  Z            gunner scope         X           first-person drive
  Q E R F      abilities            1-4         toggle mounts, 5-8 regroup them
  Walk into a wall to start climbing it. The sea is not climbable.
```

---

## Building on Windows (command line, no IDE)

### One-time setup

You need a C++17 compiler and CMake. If you have neither, these two commands
install everything — the Build Tools package is the compiler alone, with no
Visual Studio IDE:

```
winget install Kitware.CMake
winget install Microsoft.VisualStudio.2022.BuildTools --override "--quiet --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

Then **open a new terminal** so the updated `PATH` takes effect.

You do not need a Developer Command Prompt. `build.bat` locates the MSVC
toolchain itself and loads its environment, so a plain `cmd` window works.

### Build and run

```
build.bat            build Release
build.bat run        build, then launch
build.bat debug      build a Debug configuration
build.bat headless   build and run the windowless render test
build.bat clean      delete the build directory
```

The first build downloads and compiles SDL3 (pinned to `release-3.4.14`) via
CMake's `FetchContent`. Expect a few minutes once; after that it is cached in
`build/` and rebuilds take seconds. SDL is linked statically, so the result is a
single self-contained `.exe` with no loose DLLs.

If you would rather drive CMake directly:

```
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
build\bin\spiderbot.exe
```

On a machine with no network access, point the build at a local SDL checkout:

```
cmake -B build -S . -DSPIDERBOT_SDL_REPOSITORY=C:\path\to\SDL
```

The project also builds unchanged on Linux with GCC or Clang.

---

## Playing it

A run is a chain of fifteen contracts in three acts - the city, the open
country, the enemy heartland - then an endless ladder. Each mission lays its
objectives down a lane across the sector: push forward, break the defences,
do the job. The objective kinds cover demolition, marked targets, reach and
hold, escorts, convoy interceptions, radar blackouts with an alarm level,
timed destruction windows, breakthroughs under a walking barrage, and named
elite kills. A boss or set-piece closes each act.

Completing an objective is a checkpoint: dying costs a fifth of the payout and
returns you there patched to fighting shape, never the whole mission. The
debrief is an after-action ledger - kills by type, structures wrecked,
objective fees, time bonus - and everything on it converts to credits.
Destruction literally pays: fuel stores, crate dumps, masts and sheds are all
billable, many drop ammunition, and the explosive ones chain. Cleared
contracts stay open for replay (arrow keys on the briefing screen), and the
run saves itself to acah_save.txt after every mission.

The machine cannot swim. Leg-deep water wades slow with no jumping; hull-deep
water floods and kills, yours and theirs alike - island maps make the
causeway chains the whole battlefield.

### Controls

| Key | Action |
| --- | --- |
| `W` `A` `S` `D` | Move, relative to where the camera is looking |
| `Shift` | Sprint — overdrives the legs past walking speed |
| `Space` | Hold to charge a jump — release height depends on the charge |
| Mouse | Orbit the camera; the turret tracks the crosshair |
| Left click | Fire group 1 (light guns by default) |
| Right click | Fire group 2 (medium and heavy guns by default) |
| `1`-`4` | Toggle that weapon mount on / off |
| `5`-`8` | Move that mount to the other trigger |
| `Q` `E` `R` `F` | Abilities: legs / engine / armour / sensors |
| `Z` | Gunner scope: off → x2.5 → x7 → off |
| `X` | First-person drive view (in the workshop: sell the part) |
| Wheel | Camera distance |
| `Enter` | Deploy, accept a result, open a slot, fit the highlighted part |
| `Space` | Workshop: leave and start the next mission |
| `<` `>` | Briefing: pick any unlocked contract to (re)play |
| Arrow keys | Navigate the workshop |
| `Backspace` | Back out of a workshop pane; on a briefing: open the workshop |
| `Tab` | Workshop: preview your machine with or without the part |
| `H` | Toggle HUD |
| `V` | Toggle the ASCII filter off entirely — raw 3D view |
| `M` | Mute |
| `,` `.` | Volume down / up |
| `F2` | Cycle palette (phosphor green / amber / ice / mono / full RGB) |
| `F3` | Cycle character ramp, `F4` cell background, `F5` character density |
| `F6` | Toggle edge enhancement, `F7` dither, `F8` CRT glow |
| `F9` | Cycle supersampling 1x / 2x / 3x |
| `-` `=` | Glyph scale, for high-DPI displays |
| `F11` | Fullscreen (the game starts fullscreen; `--windowed` to opt out) |
| `Esc` | Quit |

Command-line options: `--width N --height N --ss N --seed N --threads N
--fullscreen --no-vsync --raw --background N --font N --ramp N --dither`.

The defaults are the densest font (4x8), the finest ramp and solid cell
backgrounds, which together read much more like an image with character texture
over it than like text on black. All three are still switchable at runtime with
`F`, `R` and `B` if you prefer the sparser look.

The HUD is drawn in its own pass at its own size, so making the scene denser
never shrinks the text.

### Climbing

Walk into a wall and the machine climbs it. There is no button: the legs look for
something to hold, and if the surface is within what they can grip, they take it.
The body then reorients to whatever its feet are holding — which is why the
camera rolls with you and the horizon tips, rather than the machine tipping out
of frame. On a building wall the ground ends up on one side of the screen and the
sky on the other.

What you can hold depends on the legs and on how heavy the whole machine is. The
grip rating maps to a maximum surface angle; mass works against it, because the
same claws have more to hold up. Most of the lighter limbs will get you up a
wall — but only the Harrier set is *good* at it. Anything less will scrabble the
whole way, slipping and catching itself, and the two heavy sets cannot climb at
all.

Working sideways around a corner is the hardest thing a climbing machine does:
for a moment its limbs span two planes and the load is on half the feet. A
specialist steps round it. A heavy machine, or one on the wrong legs, comes off
the wall — so the fall damage on the way down is part of the decision.

Enemies climb too, and anything that can hold a wall will come after you up one
rather than stand at the bottom. Their turrets elevate to 82°, so height is
cover from the ground but not from a machine that follows you up.

### The hull families

Ten chassis in four families, and the family is a rule set, not a skin:

- **Turreted line tanks** — hull plus a real rotating turret. The baseline,
  from the two-mount Wasp to the five-mount Bastion.
- **Casemates** (Ferrum, Judicator) — no turret at all. The guns sit in an
  armoured bow with a sliver of traverse, and past that the whole machine
  turns on its legs to lay them. The best frontal plate money buys, and a back
  you must never, ever show.
- **Low-profile** (Viper) — flattened and widened, rides low, and gives
  gunners a third less machine to hit out of plating that stops a third less.
- **Artillery platforms** (Longbow, and the Bastion) — an open cradle around
  an enormous rack. Anything above a light gun holds fire until the legs are
  still: devastating planted, self-defense only on the move.

Armour is directional for everyone: a strong frontal arc, ordinary flanks, a
thin rear. Facing is a decision, flanking is a tactic, and circling a casemate
is how you kill one.

### Weapons

Twenty-four of them across three size classes, and they are meant to feel
different rather than just scale. Combat is deliberately slow: rounds travel,
ranges are long, and a fight is decided over tens of seconds rather than in
one burst. Every mount is assigned to the left or right mouse trigger (number
keys re-toggle and re-group them mid-fight), so a brawler can alpha-strike on
one button and keep the railgun off the trigger on the other.

Many of the serious weapons run on finite ammunition - and the missions feed
them: wrecked vehicles, crate dumps and destroyed installations drop magazines
generously, so an all-ammo build is viable as long as you keep destroying
things. A few specials mount on ANY frame regardless of size - the Javelin
anti-armour missile and the Hive rocket pod turn even a scout's spare light
mount into a threat. Mortars arc; the Shrike flechette canister opens into a
fan of darts; the Longspur and Pike exist for the gunner scope; everything
has its own tracer so you can read a fight by its light.

- **Light** — autocannons and needle guns. Unlimited ammunition, limited only by
  heat. These are what you fight with most of the time.
- **Medium** — lances, plasma throwers, flak. Also unlimited, but gated by a
  per-shot recharge, so each shot is a decision.
- **Heavy** — missile swarms, siege mortars. Finite rounds. Buy magazines in the
  workshop, or pick them up off machines you destroy.

Every weapon has its own projectile speed and nothing hitscans. A lance round
crosses 420 metres a second and is close to point-and-click; a plasma bolt at 64
only connects if you read where the target is going. The tracer length scales
with speed, so you can see which is which. Some weapons arc under gravity.

Heat is the other constraint. Sustained fire from anything heavier than a pair of
light guns will overheat you, and an overheated machine cannot shoot at all until
it cools — the engine you fit decides how much of that you can absorb.

### Abilities

Every part except a weapon carries an ability, so the slot you are shopping in
is never only a stat block. Each slot's active lives on its own key - legs on
`Q`, engine on `E`, armour on `R`, sensors on `F` - and every fitted active
works at once, on its own cooldown. Your loadout IS your ability bar.

| Ability | From | What it does |
| --- | --- | --- |
| Dash | legs | A hard burst along the current heading |
| Brace | legs | Plant and lock: no movement at all, the steadiest guns in the game |
| Overdrive | engine | Speed and cooling surge, paid for in heat afterwards |
| Vent heat | engine | Dump the sink instantly, once per cooldown |
| Bulwark | armour | A few seconds of heavy damage reduction |
| Target lock | sensors | Hard lock: tighter convergence and a lead indicator |
| Reactive plate | armour | *Passive.* The first big hit of a wave is mostly absorbed |
| Regenerator | armour | *Passive.* Slow structure repair while nothing is shooting you |
| Stabiliser | legs | *Passive.* Much less disturbed by impacts and rough ground |
| Ghost | sensors | *Passive.* Enemies take longer to acquire you |
| Rangefinder | sensors | *Passive.* Weapons converge better at long range |

The HUD shows the active one with its cooldown, and the workshop lists it on
every part, so you can shop for a playstyle rather than for numbers.

### Sensors and automation

The top-end sensors are specialists: each does one thing near-perfectly and
nothing else. The Seeker uplink gives true homing to capable missiles; the
Aegis array shoots down the single incoming round that mattered; the Reflex
autodrive sidesteps heavy shells before you have seen them; the tracker and
lidar lines sharpen your own gunnery instead. You pick your automation - no
build gets all of it, and the enemy's machines shop from the same shelf.

### Sound

Nothing is sampled. Every sound is built at run time out of oscillators, noise
and envelopes — square waves for the light guns, a filtered descending boom for
the heavy ones, short noise bursts for footfalls, a scraping buzz for limbs
biting into a wall. That is partly to keep the game a single self-contained
executable with no asset folder, and partly because a bank of detuned saws is
the right instrument for something drawn in characters.

The soundtrack is synthesised live in four flavours, assigned per mission:
dark synthwave (a relentless detuned-saw pulse under a closed filter that
opens with the fighting), breakbeat (fast broken drums and an urgent bass
riff), brooding ambient techno (fog and a heartbeat that only condenses into
drums when the fight is real), and hybrid orchestral (drum-corps percussion
and brass-like stacked fifths, the finale sound). Every flavour reads the same
live intensity the game tracks, so it swells into a fight and settles out of
one - and boss missions push their flavour faster and a step darker.

`M` mutes, and `,` / `.` move the volume.

### The workshop

Six slots: chassis, legs, engine, armour, sensors, and however many weapon
hardpoints your chassis provides. The chassis decides both the number and the
size of the mounts, so a light frame physically cannot carry siege artillery.

Each part shows a full stat block compared side by side against what you have
fitted, with improvements and regressions coloured. The machine turns on a
turntable beside it, and `Tab` swaps that preview between your current build and
the build with the highlighted part on it — so you can see what it looks like as
well as what it does. Parts genuinely change the silhouette: armour bolts visible
plates on, a big engine grows the exhaust stacks, each weapon has its own model.

Every weapon in the catalogue is listed whether or not your current mounts can
take it. The ones that cannot are dimmed and marked with the mount they need, so
a light frame's pilot can see what a heavier chassis would let them carry rather
than never learning the siege guns exist.

Two things to watch. Mass slows you down and hurts your grip — the stat block
has a WALL CAPABLE row, because "climb angle 73 degrees" is not something you
can turn into "no" at a glance, and a sheer wall needs more than 90. And the
reactor has a fixed output: fit more than it can feed and everything derates at
once, which the store warns about in red because it is the one way to make the
machine strictly worse by spending money.

Upgrading within a slot costs the difference — the fitted part is traded in at
half price automatically.

---

## How it fits together

```
src/core/        the engine and the game - pure C++17, no dependencies at all
  math3d.h         vectors, column-major matrices, the segment/bone helpers
  mesh.*           indexed meshes and the primitive builders
  threadpool.h     persistent worker pool
  raster.*         multithreaded software rasterizer -> linear RGB + depth
  ascii.*          RGB buffer -> coloured character grid, plus HUD text
  noise.h          value noise and fBm
  terrain.*        analytic height field + the tiled mesh built from it
  obstacle.*       oriented boxes: the one solid primitive everything shares
  buildings.*      procedural ruins, containers, barriers, pipe racks, masts
  arena.*          the twelve battlefield types
  world.*          scattering, collision, line of sight, foothold queries
  parts.*          the part catalog and the derived machine statistics
  mech.*           the machine: orientation, locomotion, gait, IK, jumping
  mech_visual.*    assembling ~190 pieces into a spidertank
  combat.*         projectiles, damage, splash, salvage
  ai.*             hostile behaviour and the enemy archetypes
  campaign.*       levels, waves, money, the run
  store.*          the workshop
  game.*           camera, input, screens, HUD
  audio.h          the sound request queue - no synthesis, no SDL
  font_data.h      three baked bitmap fonts (generated, checked in)

src/platform/    the host - the only code that knows SDL or OpenGL exists
  gl.*             ~40 OpenGL 3.3 entry points loaded through SDL
  glyph_renderer.* font atlas + one instanced draw call for the whole grid
  raw_renderer.*   blits the colour buffer as pixels when ASCII is switched off
  audio.*          the synthesiser: 24 voices, no sampled assets at all
  main.cpp         window, input, frame clock

tools/
  headless.cpp     runs the game with no window, writes frames to disk
  sim.cpp          plays the campaign with a scripted pilot and reports results
  ctrl.cpp         drives the game with synthetic input: movement, camera,
                   jumping, the briefing, spawns, climbing and the workshop
  probe_mech.cpp   renders one machine, for tuning proportions
  genfont.py       regenerates font_data.h (only needed if cell metrics change)
```

The split matters: **`src/core` never includes SDL or OpenGL**, which is what
lets the whole game be compiled, run, played and verified with no display at
all. That is how this was developed and checked.

### The rendering pipeline

1. The mission submits draw items — terrain tiles, structures, every machine
   part, tracers and effects — each a mesh plus a transform, bounding-sphere
   culled against the frustum on submission.
2. `Rasterizer::endFrame` splits all submitted geometry into fixed-size triangle
   batches and runs vertex transform, Gouraud shading, near-plane clipping and
   projection across the thread pool.
3. Screen-space triangles are rasterized with incremental edge functions and a
   1/w depth buffer, in horizontal bands, one job per band.
4. `convertToAscii` box-filters the supersampled buffer down to one sample per
   character cell, applies gamma, levels and an S-curve, and picks a glyph.
5. The platform layer uploads the grid and issues a single instanced draw.

### Four things that were not obvious

**The character ramp has to be calibrated by ink coverage, not by index.** The
familiar `" .:-=+*#%@"` ramp looks evenly spaced but is not: `-` inks about 4% of
its cell and `@` nearly 34%, with a large empty gap in between. Selecting a glyph
by its position in the string makes the midtones collapse into what reads as
empty space. `buildRampLut()` in `ascii.cpp` measures every glyph's actual filled
pixel count from the font bitmap and maps luminance to the nearest match, and the
ramps themselves were chosen so their measured coverages rise in even steps.

**A display gamma of 2.2 is wrong here.** With only ten or so ramp steps to
spend, standard gamma compresses the scene's luminance ratios into two or three
glyphs and everything turns into a flat wash. The default is 1.32, with explicit
black and white points bracketing the scene's actual range. `spiderbot_headless
--hist` prints the glyph histogram, which is the fastest way to re-tune these
after changing the lighting.

**The scene needs a deliberate value hierarchy or the subject disappears.** Grey
concrete and a grey steel hull under the same light land on the same glyph, and
then a tank standing in front of a building is invisible — it is all one bright
mass. Structures are therefore held down the ramp on purpose and machines given
by far the strongest rim lighting in the scene, so the thing the player is
looking at is reliably the brightest thing in frame. The white point was raised
from its original terrain-only calibration for the same reason: leaving headroom
at the top is what keeps close-range geometry from clipping into a single
saturated glyph.

**A low sun is on purpose.** At about 28° of elevation, hillsides swing across
the whole ramp. With a high sun every patch of ground returns nearly the same
value and the ramp has nothing to work with. The terrain's mid-scale relief
exists for the same reason — and gives the leg IK something to react to.

### The machine

Nothing about the walker is keyframed. Six legs, each three segments plus a foot,
placed every frame by:

- **A single surface abstraction.** Everything solid in the world — terrain,
  buildings, containers, rubble — answers the same query: *given a point and an
  up vector, what is there to stand on, and which way does it face?* Walking up a
  hill, up a wall and across a ceiling are consequently the same code path. The
  only thing that changes is which surfaces the legs are permitted to hold.
- **Body orientation from the feet.** "Up" is the average of the surface normals
  the planted legs are gripping. Nothing decides that the machine is now
  climbing; as the leading legs find the wall, the average tips, the body rolls,
  and the trailing legs follow onto the same surface on their next step.
- **Gait planning.** Each leg has an ideal planted position in body space and an
  *anchor* half a stride ahead of it, so the foot sweeps symmetrically around
  rest rather than trailing behind. The two tripods `{FL, MR, BL}` and
  `{FR, ML, BR}` alternate, and a tripod may only lift once the other is planted
  — except that a planted foot nearing the limb's reach steps immediately
  regardless, and a foot already in the air keeps chasing its anchor so a hard
  turn cannot leave it landing somewhere stale.
- **Two-bone analytic IK.** The knee is placed by the law of cosines, bending
  away from the surface so the profile stays arachnid. The solved foot position
  is clamped to the limb's actual reach and *that* is what gets drawn: if the
  gait and the geometry ever disagree, the leg bends short rather than
  stretching.
- **Continuous body height.** Ride height comes from sampling the terrain height
  field under the machine, not from averaging the planted feet. The foot average
  is piecewise constant — it only changes when a foot lands — so the body used to
  move in visible steps and twitch on every touchdown. Sampling the field is a
  continuous function of position and heading, so the body rises and tilts
  smoothly and the legs simply follow.

The visual assembly is about 190 instances of a dozen shared unit primitives, so
a fully kitted machine costs 190 transforms rather than 190 meshes. Parts carry
an LOD tier: a distant enemy is about twenty boxes, the one in your face is all
of them.

---

## The tools

Both build alongside the game and neither needs a display.

### `spiderbot_headless` — rendering and gait

Runs the full loop with a scripted pilot and writes frames to disk.

```
build\bin\spiderbot_headless.exe --frames 240 --cols 200 --rows 56 --out out --hist
```

| Flag | Meaning |
| --- | --- |
| `--cols N --rows N --ss N` | Grid size and supersampling |
| `--frames N --seed N --threads N` | Simulation length, world seed, worker count |
| `--out DIR` | Where to write captures |
| `--raw` | Also dump the pre-ASCII colour buffer — the first place to look when geometry or shading looks wrong |
| `--orbit` | Fixed orbit camera on the machine, for inspecting the gait |
| `--hist` | Print the glyph histogram |
| `--gamma --black --white --contrast` | Override the tone curve (thousandths, e.g. `--gamma 1320`) |
| `--wander` | Hard-driving pilot that covers the whole map; the gait regression test |

Every run prints a gait check — worst limb extension as a fraction of reach
(must stay under 1.0), minimum hull clearance (must stay positive), and the
largest single-frame body height change. Those three numbers are the regression
test for the walker.

### `spiderbot_ctrl` — movement, camera and jump

Drives the real game with synthetic input and reports what the machine actually
did. Control bugs are close to impossible to judge from a screenshot and
miserable to test by hand.

```
build\bin\spiderbot_ctrl.exe
```

```
WALK    3s of W: 24.0 m travelled, peak 8.7 m/s, 100% along the view
STRAFE  D goes 100% to the camera's right
CAMERA  4s of W, no mouse: view swung 3.1 deg total, worst 0.04 deg/frame
MOUSE   right -> view turns RIGHT, down -> view tilts DOWN
JUMP    charged 0.50s, left the ground, apex 4.33 m above the start
```

Every one of those lines exists because it was once wrong. `--screens` also
dumps each menu screen as text, which is how the workshop layout gets checked
without a display.

### `spiderbot_sim` — combat, AI and the economy

None of the game layer can be judged from a screenshot, so this plays the
campaign with a scripted pilot of roughly average competence and reports what
actually happened.

```
build\bin\spiderbot_sim.exe --missions 12 --seconds 200
```

```
mission  1  SHAKEDOWN        SECTOR 7         CLEARED     5.7s  hp 205/205  kills 2/2  cash +1120
mission  2  TREELINE         IRONWOOD FLATS   CLEARED    12.6s  hp 207/207  kills 4/4  cash +2765
mission  4  RED CHANNEL      RED CHANNEL      FAILED     32.4s  hp   0/275  kills 2/4  cash +2988
mission  4  RED CHANNEL      RED CHANNEL      CLEARED    50.4s  hp 131/290  kills 6/6  cash +7898
```

That output is the difficulty curve. `--verbose` adds a per-second trace of
health, positions and wave state, which is how every balance problem in the
campaign was found. `--no-store` plays without shopping, which isolates mission
balance from progression.

The whole enemy health curve, the bounty rate and the failure payout were tuned
against this rather than by feel.

---

## Failure modes worth knowing about

These are the bugs that were hardest to find, kept here because they are the ones
most likely to come back.

**A camera built from the machine's heading fights the player.** The machine
turns to face wherever you drive it, and you drive it relative to the camera. So
deriving the camera's orbit from the machine's forward vector closes a loop: you
press forward, the body turns, the camera swings, "forward" now means somewhere
else, the body turns again. The result is a machine that crabs sideways at a
fraction of its rated speed and a view that never settles. The reference
direction is now carried across frames and only re-fitted to the camera's up
vector, so it is rock steady on level ground and rotates exactly as much as the
surface does when you climb.

**`cross(up, forward)` is the *left* vector.** The renderer builds its basis as
`cross(forward, up)`, and anything else computing a right vector has to match.
Getting it backwards had `D` strafing left, and reversed the camera's yaw sign
too — two wrongs that cancelled, so horizontal mouse-look felt fine while the
strafe keys were mirrored.

**A mission that cannot be finished.** Twice during development the two sides
simply lost each other — a machine wedged in a canyon wall, or one walking toward
a "last known position" it had never actually had, which defaulted to the world
origin where it was already standing. The player ends up patrolling an empty map
forever and the game never says why. There are now three layers against it: a
stall guard that orders every remaining hostile to close after fourteen quiet
seconds, an unstick that walks a wedged machine out along its contact normal, and
a relocation failsafe that picks up anything still not moving and puts it back
near the player.

**A run that cannot progress.** A failed mission originally paid nothing and went
straight back into the same fight. A player whose machine is not good enough for
a mission then has no way to earn their way out of it, and the run is over
without the game ever acknowledging it. Failures now pay a share of what you
destroyed plus a flat recovery amount, route through the workshop like any other
result, and re-roll the level seed so the retry is a different fight.

**Aim error measured in the wrong units.** Enemy accuracy was expressed as a
metre offset on the aim point. A machine is about 1.5 m in radius, so a
half-metre error is no error at all, and a nominally poor gunner never missed. It
now scales hard with range and with the gunner's rating, which is what makes
distance a real defence and closing the gap a real decision.

**One key doing two jobs.** Enter raised both "confirm" and "continue", so in
the workshop it opened the parts list and deployed on the same keystroke — no
part was ever on screen long enough to buy, which reads as "the store is broken
and there are no parts". Any screen with both a confirm and a leave action needs
two keys.

**A briefing screen that runs the mission.** The world ticks behind the briefing
text so the transition has no seam, which also let the enemy AI move and open
fire on a player who had no control yet. A mission now starts in a briefing
phase where everything animates but nothing hostile acts.

**Gravity along a surface you are gripping.** Only the component along the
surface *normal* was being cancelled — enough not to fall off a wall, but the
component along the wall stayed at a full 24 m/s², so a machine clinging to a
face slid down it as though the face were greased. It would get two body lengths
up and drop. Gripping means the limbs carry that load too.

**Averaging normals cannot survive a transition.** Halfway onto a wall, three
legs are on the ground and three on the face, so the mean surface normal is a
45° diagonal that neither surface holds. The machine has to *commit* to the face
and let the trailing legs catch up.

**Projecting the pilot's intent onto the surface deletes it.** Movement is
camera-relative and projected onto whatever the machine is standing on, which is
what makes "forward" mean the same thing on flat ground and halfway up a wall.
But at the instant the machine commits to a wall, the pilot is still looking at
the building and the building's normal has just become "up" — so the projection
of the camera's forward onto the surface is nearly zero, and W meant whatever
sideways scrap survived. The machine crabbed along the face instead of going up
it, which is what "climbing doesn't work" actually was. Driving *into* the
surface you are gripping has to mean climbing it; the component that points at
the face is redirected uphill rather than discarded. Anywhere a control scheme
projects a vector, check what happens when the vector is parallel to the axis
being projected out.

**An urgency rule that outranks the gait.** A planted foot near the limb's limit
steps immediately, whatever the tripod would prefer — reasonable on its own, and
correct on flat ground. On a wall the body climbs faster than the swing places a
foot, so *every* limb went urgent at once, all six left the surface together, and
the machine had one foot on a vertical face when it needed two to count as
supported. It then fell off. A stability cap that outranks urgency — never more
than three limbs in the air — fixes it. When you add a safety valve that bypasses
a scheduler, check what happens when every client trips it in the same frame.

**Corner-rounding needs a ray aimed at the corner.** Walking off the edge of a
wall leaves the hull diagonally out from the corner post: the old face is behind
it, the new one is off to the side, and both are a hull radius away. Every probe
direction that seemed natural — straight on, tucking in, wrapping out — misses,
because they are all *tangent* to the geometry. The only ray that finds anything
is aimed back at the corner itself.

**A step function in difficulty is a wall, not a ramp.** Enemy equipment tier
was `power / 3.2`, so on one specific mission every hostile upgraded at once and
the run stopped there. Rolling the fractional part per machine spreads the same
progression over two or three missions, so the player meets one of the new thing
before they meet a wave of it — and a wave stops reading as a block of identical
machines.

**Asking for aggressive enemies and heavier guns at once.** Both were reasonable
requests and together they made the early campaign unplayable: the player faces
four hostiles, and each of them faces one player. The fix is a hostile damage
scale that ramps across the campaign, not a duller AI — the enemies stay as
sharp as they were, they just do not delete you while being it.

**A destructible's collision box ate the shots meant for it.** Props carried a
hit-sphere for damage and a blocking box for movement, and the box - tested as
world geometry - caught almost every round first, so crates soaked fire and
never died. Anything that both blocks and can be destroyed must route world
hits on its volume back to its own health.

**An escort that outruns or outdies its purpose.** The first crawler drove
into the sea, then into the guns, then died in seconds to concentrated AT fire
while every hostile ignored the player shredding them. Escorts need all four:
a route made of real geometry (the causeway decks), a pace tied to their
protection, fire distribution that favours the bigger threat, and a failure
mode gentler than instant mission loss - disabled-and-repaired, paid for on
the ledger.

**A hull family is a rule set or it is nothing.** The casemate shipped as
stats first and was unplayable: with the gun fixed forward and the hull only
turning to drive, nothing could be aimed at. The family only started working
when aiming itself was rerouted - the machine walks its feet around to lay the
bow. If a shape is supposed to play differently, the difference has to live in
the verbs, not the numbers.

**A landing test that only asks "is there a surface within reach".** The foothold
search looks a full limb length down — nearly five metres. On the way up from a
jump it still finds the ground the machine just left, so the mech landed about a
metre into its own leap. Together with an impulse that only lifted it 0.75 m,
jumping looked like it did nothing at all. Touching down now also requires moving
toward the surface and being close enough to stand on it.

---

## Performance

The rasterizer is the whole cost; the GPU side is negligible. At a 200x60 grid
with 2x supersampling and a full fight in view, a frame takes about 6 ms —
roughly 165 fps — and it scales close to linearly with core count. The 4x8 font
at 1600x900 gives a 400x112 grid, which is four times the cells.

Cheapest knobs if it ever needs to be faster: drop supersampling to 1x (`1`), or
use a coarser font (`F`). `viewDistance_` in `game.h` controls how much of the
world is considered at all.

---

## Where this would go next

- **Navigation for the escort AI.** The crawler follows the causeway decks by
  waypoint and recovers when wedged, but a real router over the obstacle grid
  would let convoys and escorts thread dense arenas without the recovery winch.
- **More music per flavour.** The synthesiser plays one progression per style;
  a workshop theme and per-act variations are cheap now the styles exist.
- **Directional armour on the HUD.** The hit already knows its arc; a hull
  outline that flashes the struck facet would teach the system in one fight.
- **Objective variety on the endless ladder.** Endless patrols still use the
  plain clear-everything shape; the objective machinery could roll random
  contracts instead.
- **Foot-plant snapping onto props,** and treads of dust on hard landings.
