#include "store.h"

#include <algorithm>
#include <cmath>

namespace sb {

namespace {

// How much you get back for a part you already own. Half is harsh enough that
// switching builds costs something, generous enough that a bad purchase is not
// a run-ending mistake.
constexpr float kSellFraction = 0.5f;

std::string* slotField(Loadout& l, Slot s) {
    switch (s) {
        case Slot::Chassis: return &l.chassis;
        case Slot::Legs:    return &l.legs;
        case Slot::Engine:  return &l.engine;
        case Slot::Armor:   return &l.armor;
        case Slot::Sensor:  return &l.sensor;
        default:            return nullptr;
    }
}

const std::string* slotField(const Loadout& l, Slot s) {
    return slotField(const_cast<Loadout&>(l), s);
}

} // namespace

const std::vector<Slot>& storeSlots() {
    static const std::vector<Slot> s = {
        Slot::Weapon, Slot::Chassis, Slot::Legs,
        Slot::Engine, Slot::Armor, Slot::Sensor
    };
    return s;
}

void Store::open(PlayerProfile& profile) {
    profile_ = &profile;
    open_ = true;
    pane_ = StorePane::Slots;
    slot_ = Slot::Weapon;
    weaponMount_ = 0;
    cursor_ = 0;
    showCandidate_ = true;
    spin_ = 0.0f;
    partSpin_ = 0.0f;
    message_.clear();
    messageAge_ = 99.0f;

    current_ = profile.loadout;
    current_.ensureWeaponSlots();
    currentStats_ = deriveStats(current_);
    rebuildListing();
    rebuildPreview();
}

void Store::close() {
    if (profile_) profile_->loadout = current_;
    open_ = false;
    profile_ = nullptr;
}

void Store::say(const std::string& s) {
    message_ = s;
    messageAge_ = 0.0f;
}

void Store::update(float dt) {
    spin_ += dt * 0.42f;
    partSpin_ += dt * 0.95f;
    messageAge_ += dt;
}

void Store::rebuildListing() {
    listing_.clear();
    const PartCatalog& cat = PartCatalog::instance();

    // Everything in the slot, including weapons this mount cannot take. Those
    // are marked rather than hidden - a pilot in a light frame who only ever
    // sees three autocannons has no way to learn that siege guns exist, and no
    // reason to save for a chassis that can carry one.
    listing_ = cat.bySlot(slot_);

    if (cursor_ >= static_cast<int>(listing_.size()))
        cursor_ = std::max(0, static_cast<int>(listing_.size()) - 1);
}

const PartDef* Store::selected() const {
    if (cursor_ < 0 || cursor_ >= static_cast<int>(listing_.size())) return nullptr;
    return listing_[static_cast<size_t>(cursor_)];
}

const PartDef* Store::fitted() const {
    const PartCatalog& cat = PartCatalog::instance();
    if (slot_ == Slot::Weapon) {
        if (weaponMount_ < 0 ||
            weaponMount_ >= static_cast<int>(current_.weapons.size())) return nullptr;
        return cat.find(current_.weapons[static_cast<size_t>(weaponMount_)]);
    }
    const std::string* f = slotField(current_, slot_);
    return f ? cat.find(*f) : nullptr;
}

std::string Store::blockedReason(const PartDef* p) const {
    if (!p) return "";
    if (p->slot != Slot::Weapon) return "";
    const PartCatalog& cat = PartCatalog::instance();
    const PartDef* ch = cat.find(current_.chassis);
    SizeClass mountSize = SizeClass::Light;
    if (ch && weaponMount_ >= 0 &&
        weaponMount_ < static_cast<int>(ch->hardpoints.size()))
        mountSize = ch->hardpoints[static_cast<size_t>(weaponMount_)].size;
    if (static_cast<int>(p->size) > static_cast<int>(mountSize))
        return std::string("NEEDS A ") + sizeClassName(p->size) + " MOUNT";
    return "";
}

bool Store::alreadyFitted() const {
    const PartDef* sel = selected();
    const PartDef* fit = fitted();
    return sel && fit && sel->id == fit->id;
}

int Store::priceOfSelected() const {
    const PartDef* p = selected();
    if (!p) return 0;
    if (alreadyFitted()) return 0;
    // Salvaged off a bounty elite: it is yours.
    if (profile_ && profile_->hasUnlocked(p->id)) return 0;
    // Trade-in: the fitted part's resale comes off the sticker price, so
    // upgrading within a slot costs the difference rather than the full amount.
    const PartDef* fit = fitted();
    const int tradeIn = fit ? static_cast<int>(fit->price * kSellFraction) : 0;
    return std::max(0, p->price - tradeIn);
}

bool Store::canAfford() const {
    return profile_ && profile_->cash >= priceOfSelected();
}

void Store::rebuildPreview() {
    preview_ = current_;
    const PartDef* sel = selected();
    if (sel && !fittable(sel)) sel = nullptr;   // never preview an impossible fit
    if (sel && pane_ != StorePane::Slots) {
        if (slot_ == Slot::Weapon) {
            preview_.ensureWeaponSlots();
            if (weaponMount_ >= 0 &&
                weaponMount_ < static_cast<int>(preview_.weapons.size()))
                preview_.weapons[static_cast<size_t>(weaponMount_)] = sel->id;
        } else {
            std::string* f = slotField(preview_, slot_);
            if (f) *f = sel->id;
            // Changing the chassis changes the hardpoint count, which can leave
            // weapons hanging off mounts that no longer exist.
            preview_.ensureWeaponSlots();
        }
    }
    previewStats_ = deriveStats(preview_);
    currentStats_ = deriveStats(current_);

    mech_.poseStatic(showCandidate_ ? preview_ : current_, 0.0f);
    rebuildStatLines();
}

void Store::rebuildStatLines() {
    lines_.clear();
    const MechStats& a = currentStats_;
    const MechStats& b = previewStats_;

    auto line = [&](const char* label, float cur, float cand, int digits,
                    bool higherBetter, const char* unit) {
        StatLine l;
        l.label = label;
        l.current = cur;
        l.candidate = cand;
        l.digits = digits;
        l.higherIsBetter = higherBetter;
        l.unit = unit;
        lines_.push_back(l);
    };

    line("MASS",        a.mass, b.mass, 1, false, "t");
    line("POWER DRAW",  a.powerDraw, b.powerDraw, 2, false, "MW");
    line("POWER OUT",   a.powerOutput, b.powerOutput, 2, true, "MW");
    line("STRUCTURE",   a.maxHealth, b.maxHealth, 0, true, "HP");
    line("ARMOUR",      a.armor, b.armor, 1, true, "");
    line("TOP SPEED",   a.maxSpeed, b.maxSpeed, 1, true, "m/s");
    // How much of that speed the REACTOR is buying. Below 1.00 the machine
    // is running on the margin and the legs cannot be driven hard; above it,
    // spare power is going into pace. This is what makes a bigger reactor a
    // mobility purchase and not just a bigger power budget.
    line("DRIVE",       a.driveFactor, b.driveFactor, 2, true, "x");
    line("AGILITY",     a.turnRate, b.turnRate, 2, true, "rad/s");
    line("JUMP",        a.jumpImpulse, b.jumpImpulse, 1, true, "");
    line("CLIMB ANGLE", a.climbAngle * 180.0f / PI, b.climbAngle * 180.0f / PI,
         0, true, "deg");
    // The number above is the honest one, but "can these legs hold a sheer
    // wall" is the question players actually have, and 90 degrees is the line.
    line("WALL CAPABLE", a.canClimbWalls ? 1.0f : 0.0f,
         b.canClimbWalls ? 1.0f : 0.0f, 0, true, "");
    line("SENSORS",     a.sensorRange, b.sensorRange, 0, true, "m");
    line("HEAT CAP",    a.heatCapacity, b.heatCapacity, 1, true, "");

    // For a weapon, the numbers that actually decide a fight are damage per
    // second and how much you have to lead - so both go on the table.
    if (slot_ == Slot::Weapon) {
        const PartDef* sel = selected();
        const PartDef* fit = fitted();
        auto dps = [](const PartDef* p) -> float {
            if (!p) return 0.0f;
            const WeaponDef& w = p->weapon;
            const float interval = (w.ammo == AmmoKind::Cooldown) ? w.cooldownTime
                                                                  : w.fireInterval;
            return (w.damage * std::max(1, w.pellets) + w.blastDamage * 0.5f) /
                   std::max(0.05f, interval);
        };
        auto speed = [](const PartDef* p) { return p ? p->weapon.projectileSpeed : 0.0f; };
        auto blast = [](const PartDef* p) { return p ? p->weapon.blastRadius : 0.0f; };
        line("DPS",        dps(fit), dps(sel), 0, true, "");
        line("MUZZLE VEL", speed(fit), speed(sel), 0, true, "m/s");
        line("BLAST",      blast(fit), blast(sel), 1, true, "m");
    }
}

void Store::moveCursor(int delta) {
    if (pane_ == StorePane::Slots) {
        const std::vector<Slot>& ss = storeSlots();
        int idx = 0;
        for (size_t i = 0; i < ss.size(); ++i) if (ss[i] == slot_) idx = static_cast<int>(i);
        idx = (idx + delta + static_cast<int>(ss.size())) % static_cast<int>(ss.size());
        slot_ = ss[static_cast<size_t>(idx)];
        cursor_ = 0;
        rebuildListing();
        rebuildPreview();
        return;
    }
    if (listing_.empty()) return;
    const int n = static_cast<int>(listing_.size());
    cursor_ = (cursor_ + delta % n + n) % n;
    rebuildPreview();
}

void Store::nextSlot(int delta) {
    // In the weapon pane this cycles which hardpoint you are outfitting.
    if (slot_ == Slot::Weapon) {
        const int n = std::max(1, static_cast<int>(current_.weapons.size()));
        weaponMount_ = (weaponMount_ + delta % n + n) % n;
        cursor_ = 0;
        rebuildListing();
        rebuildPreview();
        return;
    }
    moveCursor(delta);
}

void Store::confirm() {
    if (pane_ == StorePane::Slots) {
        pane_ = StorePane::Parts;
        cursor_ = 0;
        rebuildListing();
        rebuildPreview();
        return;
    }
    if (pane_ == StorePane::Ammo) {
        // Buy one magazine for the highlighted weapon.
        const PartDef* p = selected();
        if (!p || p->weapon.ammo != AmmoKind::Limited) { say("NO MAGAZINE FOR THIS WEAPON"); return; }
        if (!profile_ || profile_->cash < p->weapon.ammoPrice) { say("INSUFFICIENT FUNDS"); return; }
        profile_->cash -= p->weapon.ammoPrice;
        profile_->addAmmo(p->id, p->weapon.magazine);
        say("MAGAZINE PURCHASED");
        return;
    }

    const PartDef* p = selected();
    if (!p || !profile_) return;
    const std::string blocked = blockedReason(p);
    if (!blocked.empty()) { say(blocked); return; }
    if (alreadyFitted()) { say("ALREADY FITTED"); return; }
    const int price = priceOfSelected();
    if (profile_->cash < price) { say("INSUFFICIENT FUNDS"); return; }

    profile_->cash -= price;
    if (slot_ == Slot::Weapon) {
        current_.ensureWeaponSlots();
        if (weaponMount_ >= 0 && weaponMount_ < static_cast<int>(current_.weapons.size()))
            current_.weapons[static_cast<size_t>(weaponMount_)] = p->id;
    } else {
        std::string* f = slotField(current_, slot_);
        if (f) *f = p->id;
        current_.ensureWeaponSlots();
        // A smaller chassis can leave an oversized gun on a mount that no
        // longer takes it; drop those rather than silently carrying dead weight.
        const PartCatalog& cat = PartCatalog::instance();
        const PartDef* ch = cat.find(current_.chassis);
        if (ch) {
            for (size_t i = 0; i < current_.weapons.size() && i < ch->hardpoints.size(); ++i) {
                const PartDef* w = cat.find(current_.weapons[i]);
                if (w && static_cast<int>(w->size) > static_cast<int>(ch->hardpoints[i].size))
                    current_.weapons[i].clear();
            }
        }
    }
    profile_->loadout = current_;
    say("FITTED: " + p->name);
    rebuildPreview();
}

void Store::back() {
    if (pane_ == StorePane::Slots) return;
    pane_ = StorePane::Slots;
    rebuildPreview();
}

void Store::toggleCompare() {
    showCandidate_ = !showCandidate_;
    rebuildPreview();
}

void Store::flipGroup() {
    if (slot_ != Slot::Weapon) return;
    current_.ensureWeaponSlots();
    // -1, not 0: a 0 here PINS the mount to the left trigger and defeats the
    // automatic split in ensureWeaponSlots - which is why opening the
    // workshop once made the right trigger permanently dead.
    if (current_.weaponGroups.size() < current_.weapons.size())
        current_.weaponGroups.resize(current_.weapons.size(), -1);
    const size_t m = static_cast<size_t>(weaponMount_);
    if (m >= current_.weaponGroups.size()) return;
    current_.weaponGroups[m] = current_.weaponGroups[m] ? 0 : 1;
    if (profile_) profile_->loadout = current_;
    message_ = current_.weaponGroups[m] ? "MOUNT ON RIGHT TRIGGER"
                                        : "MOUNT ON LEFT TRIGGER";
    messageAge_ = 0.0f;
}

void Store::sellSelected() {
    if (!profile_) return;
    const PartDef* fit = fitted();
    if (!fit || fit->price <= 0) { say("NOTHING TO SELL"); return; }

    const PartCatalog& cat = PartCatalog::instance();
    if (slot_ == Slot::Weapon) {
        if (weaponMount_ < 0 || weaponMount_ >= static_cast<int>(current_.weapons.size())) return;
        current_.weapons[static_cast<size_t>(weaponMount_)].clear();
    } else {
        const PartDef* fallback = cat.starter(slot_);
        std::string* f = slotField(current_, slot_);
        if (!f || !fallback) return;
        *f = fallback->id;
    }
    // Salvage was never bought, so it is never sold: it goes back on the
    // shelf for nothing and stays there for free.
    const int refund = profile_->hasUnlocked(fit->id) ? 0
                     : static_cast<int>(fit->price * kSellFraction);
    profile_->cash += refund;
    profile_->loadout = current_;
    say(refund > 0 ? "SOLD FOR " + std::to_string(refund) + "cr" : "RETURNED TO STORES");
    rebuildPreview();
}

// ------------------------------------------------------------------ preview

void Store::submitMechPreview(Rasterizer& raster, const Vec3& viewPos) const {
    // The turntable. Re-posing is cheap - the 190-piece assembly was built when
    // the loadout changed, not here - so the machine can spin every frame.
    const_cast<Mech&>(mech_).poseAt(kMechPreviewCentre, spin_);
    mech_.submit(raster, viewPos);
}

void Store::submitPartPreview(Rasterizer& raster, const Vec3& centre,
                              float scale) const {
    // The inspector shows the candidate part on its own, spinning. It draws the
    // same geometry the mech will carry, isolated by slot, so what you inspect
    // is exactly what gets bolted on rather than a stand-in model.
    const PartDef* p = selected();
    if (!p) return;

    Mech& probe = const_cast<Mech&>(partProbe_);
    if (probeId_ != p->id) {
        Loadout l = preview_;
        l.ensureWeaponSlots();
        probe.poseStatic(l, 0.0f);
        const_cast<std::string&>(probeId_) = p->id;
    }
    // Part geometry is authored around the hull origin, so posing the probe
    // with its hull at the panel centre puts the inspected part in frame.
    probe.poseAt(centre, partSpin_);
    (void)scale;
    probe.submitSlotOnly(raster, slot_, slot_ == Slot::Weapon ? weaponMount_ : -1);
}

} // namespace sb
