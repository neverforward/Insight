#pragma once

#include <optional>
#include <string>

#include "mc/deps/core/math/Vec3.h"
#include "mc/math/vector/Vecs.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/phys/AABB.h"

namespace insight {

struct EntityHit {
    ::Actor* actor    = nullptr;
    double   distance = 0.0;
};

// Nearest actor whose AABB the look ray enters (ignoring `except`), or a
// default EntityHit when nothing is intersected within `maxDist`.
[[nodiscard]] EntityHit
findLookEntity(BlockSource& region, Actor const* except, Vec3 const& from, Vec3 const& dir, double maxDist);

// Display name of an entity: player real name / name tag / localized type.
[[nodiscard]] std::string entityDisplayName(Actor const* actor, std::string const& langCode);

// The vanilla localization key the display name above is resolved from, e.g.
// "entity.zombie" - what the entity panel shows in brackets.
[[nodiscard]] std::string entityDisplayKey(Actor const* actor);

// Whether this entity has hit points at all (false for items, projectiles,
// paintings, ...). Asked directly at the engine's health attribute, so no list
// of entity types is involved; when it returns false the HP placeholders stay
// hidden for that target.
[[nodiscard]] bool entityHasHealth(Actor const& actor);

// Hit points as the engine stores them. The health attribute keeps the fraction
// that the integer getHealth() rounds away, which is what entity.healthDecimals
// shows ("19.5"); entities without the attribute report their integer value.
[[nodiscard]] float entityHealthExact(Actor const& actor);

// Armour points of a mob - the number the vanilla armour bar draws - or nullopt
// for an entity that is not a mob (and therefore cannot wear armour). Read from
// Mob::getArmorValue() rather than an attribute: Bedrock has no armour attribute
// (SharedAttributes has no ARMOR entry), the virtual is where the engine keeps it.
[[nodiscard]] std::optional<int> entityArmorValue(Actor const& actor);

} // namespace insight
