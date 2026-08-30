// store.h - the workshop between missions.
//
// Three things have to be true for a store to be worth using: you can see what
// a part does, you can see what it looks like, and you can see what it does to
// the machine you already own. So the screen carries a stat block with a
// side-by-side comparison against the fitted part, a slowly rotating preview of
// the part itself, and a toggle that swaps the full mech preview between your
// current build and the build with this part fitted.
#pragma once

#include <string>
#include <vector>
#include "campaign.h"
#include "math3d.h"
#include "mech.h"
#include "parts.h"
#include "raster.h"

namespace sb {

// Where the store's 3D previews live in world space. The store renders into the
// same rasterizer as the game, so the two turntables just sit somewhere the
// battlefield is not.
const Vec3 kMechPreviewCentre(0.0f, 4000.0f, 0.0f);
const Vec3 kPartPreviewCentre(0.0f, 4000.0f, 40.0f);

enum class StorePane : int {
    Slots = 0,      // choosing which slot to shop for
    Parts,          // browsing parts in that slot
    Ammo            // buying magazines for limited-ammo weapons
};

// One line of the comparison table.
struct StatLine {
    std::string label;
    float current = 0.0f;
    float candidate = 0.0f;
    int digits = 1;
    bool higherIsBetter = true;
    std::string unit;
};

class Store {
public:
    void open(PlayerProfile& profile);
    void close();
    bool isOpen() const { return open_; }

    void update(float dt);

    // Input, all edge-triggered by the caller.
    void moveCursor(int delta);
    void nextSlot(int delta);
    void confirm();          // enter a pane, or buy/fit the selection
    void back();             // leave a pane
    void toggleCompare();    // preview with or without the candidate part
    void sellSelected();     // refund the fitted part at half price
    // Flip the highlighted weapon mount between the left and right trigger
    // groups. The field keys 5-8 do the same mid-mission; this is where you
    // set the plan before deploying.
    void flipGroup();

    // ------------------------------------------------------------ queries --
    StorePane pane() const { return pane_; }
    Slot slot() const { return slot_; }
    int weaponMount() const { return weaponMount_; }
    int cursor() const { return cursor_; }
    bool comparing() const { return showCandidate_; }
    const std::string& message() const { return message_; }
    float messageAge() const { return messageAge_; }

    // The parts currently listed, already filtered by slot and mount size.
    const std::vector<const PartDef*>& listing() const { return listing_; }
    const PartDef* selected() const;
    const PartDef* fitted() const;

    // Why a listed part cannot be fitted, or empty if it can. Weapons too big
    // for the mount are still listed: hiding them means a light pilot has no
    // idea the heavy catalogue exists, and no reason to want a bigger chassis.
    std::string blockedReason(const PartDef* p) const;
    bool fittable(const PartDef* p) const { return blockedReason(p).empty(); }

    // What is fitted right now, and what it would be with the selection.
    const Loadout& currentLoadout() const { return current_; }
    const Loadout& previewLoadout() const { return preview_; }
    const MechStats& currentStats() const { return currentStats_; }
    const MechStats& previewStats() const { return previewStats_; }

    // The comparison table for the highlighted part.
    const std::vector<StatLine>& statLines() const { return lines_; }

    int cash() const { return profile_ ? profile_->cash : 0; }
    int priceOfSelected() const;
    bool canAfford() const;
    bool alreadyFitted() const;

    // ------------------------------------------------------------ preview --
    // The mech shown in the big preview: either the current build or the build
    // with the candidate fitted, depending on the compare toggle.
    const Mech& previewMech() const { return mech_; }
    float previewSpin() const { return spin_; }
    float partSpin() const { return partSpin_; }

    // Draws the rotating part on its own, for the small inspector panel.
    void submitPartPreview(Rasterizer& raster, const Vec3& centre,
                           float scale) const;
    void submitMechPreview(Rasterizer& raster, const Vec3& viewPos) const;

private:
    void rebuildListing();
    void rebuildPreview();
    void rebuildStatLines();
    void say(const std::string& s);

    PlayerProfile* profile_ = nullptr;
    bool open_ = false;

    StorePane pane_ = StorePane::Slots;
    Slot slot_ = Slot::Chassis;
    int weaponMount_ = 0;         // which hardpoint we are shopping for
    int cursor_ = 0;
    bool showCandidate_ = true;

    std::vector<const PartDef*> listing_;
    Loadout current_, preview_;
    MechStats currentStats_, previewStats_;
    std::vector<StatLine> lines_;

    // A real Mech instance used purely as a preview. It never gets a world, so
    // its legs are posed from the rest positions rather than solved against
    // terrain - which is exactly what you want on a showroom turntable.
    Mech mech_;
    float spin_ = 0.0f;
    float partSpin_ = 0.0f;

    // A second machine, rebuilt only when the highlighted part changes, used to
    // draw that one part in isolation for the inspector panel.
    Mech partProbe_;
    std::string probeId_;

    std::string message_;
    float messageAge_ = 99.0f;
};

// Slots the store lets you shop for, in display order.
const std::vector<Slot>& storeSlots();

} // namespace sb
