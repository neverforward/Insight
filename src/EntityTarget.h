#pragma once

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

// Whether this entity has hit points at all (false for items, projectiles,
// paintings, ...). Asked directly at the engine's health attribute, so no list
// of entity types is involved; when it returns false the HP placeholders stay
// hidden for that target.
[[nodiscard]] bool entityHasHealth(Actor const& actor);

} // namespace insight
