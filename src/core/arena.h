// arena.h - the twelve battlefield types.
//
// An arena is a terrain profile, a set of scatter rules, and a lighting mood.
// Everything about how a level looks and plays underfoot comes from here, so
// adding a thirteenth is a matter of appending one entry.
#pragma once

#include <string>
#include <vector>
#include "math3d.h"
#include "raster.h"
#include "terrain.h"

namespace sb {

// How structures are laid out across the map.
enum class DistrictLayout : int {
    Scattered = 0,   // ruins dotted about wherever the ground allows
    Grid,            // city blocks on a street grid
    Ring,            // structures around the rim, open middle
    Cluster,         // one dense knot of buildings
    Line,            // a strip, like a road or a canyon floor
    None
};

struct ArenaDef {
    std::string id;
    std::string name;
    std::string subtitle;

    TerrainProfile terrain;
    RenderSettings render;

    DistrictLayout layout = DistrictLayout::Scattered;
    int ruinCount = 12;
    float districtRadius = 110.0f;
    int floorsMin = 2, floorsMax = 6;
    float ruinScale = 1.0f;
    float ruinDamage = 0.5f;
    Vec3 concreteTint{0.34f, 0.34f, 0.33f};

    // Scatter densities, roughly "items per 10,000 square metres".
    float treeDensity = 0.0f;
    float rockDensity = 0.6f;
    int containerCount = 0;
    int barrierCount = 0;
    int pipeRackCount = 0;
    int bunkerCount = 0;
    int mastCount = 0;
    int rubbleCount = 0;

    float extent = 210.0f;        // half-size of the playable square
    float spawnClear = 26.0f;     // radius kept free of props at the origin

    // Water. Anything below this plane is sea; a spidertank wades at leg
    // depth and floods at hull depth. Left at the sentinel = a dry map.
    float waterLevel = -10000.0f;
    // Raised causeway road segments (island maps), and covered galleries
    // (tunnel fights). Counts of structure pieces to place.
    int causewayCount = 0;
    int galleryCount = 0;

    // How vertical the fight is meant to be, purely for the briefing text.
    const char* verticality = "LOW";
};

// The full catalog, in campaign order.
const std::vector<ArenaDef>& arenaCatalog();
const ArenaDef& arenaByIndex(int index);
const ArenaDef* arenaById(const std::string& id);

} // namespace sb
