#include "EntityTarget.h"

#include <algorithm>
#include <cmath>

#include "mc/world/actor/player/Player.h"
#include "mc/world/attribute/AttributeInstanceConstRef.h"
#include "mc/world/attribute/SharedAttributes.h"

#include "Translation.h"

namespace insight {

namespace {

// Ray vs axis-aligned box (slab method). `tOut` = entry distance along dir.
bool rayIntersectsBox(Vec3 const& from, Vec3 const& dir, Vec3 const& min, Vec3 const& max, double& tOut) {
    double tmin = 0.0;
    double tmax = 1e30;
    for (int axis = 0; axis < 3; ++axis) {
        double o  = axis == 0 ? from.x : axis == 1 ? from.y : from.z;
        double d  = axis == 0 ? dir.x : axis == 1 ? dir.y : dir.z;
        double lo = axis == 0 ? min.x : axis == 1 ? min.y : min.z;
        double hi = axis == 0 ? max.x : axis == 1 ? max.y : max.z;
        if (std::abs(d) < 1e-9) {
            if (o < lo || o > hi) {
                return false;
            }
        } else {
            double t1 = (lo - o) / d;
            double t2 = (hi - o) / d;
            if (t1 > t2) {
                std::swap(t1, t2);
            }
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) {
                return false;
            }
        }
    }
    tOut = tmin;
    return true;
}

std::string shortId(std::string const& typeName) {
    auto p = typeName.rfind(':');
    return p == std::string::npos ? typeName : typeName.substr(p + 1);
}

} // namespace

EntityHit findLookEntity(BlockSource& region, Actor const* except, Vec3 const& from, Vec3 const& dir, double maxDist) {
    EntityHit result;

    double ex = 1.2;
    double fx = from.x, fy = from.y, fz = from.z;
    double dx = dir.x, dy = dir.y, dz = dir.z;
    AABB   bb(
        static_cast<float>(std::min(fx, fx + dx * maxDist) - ex),
        static_cast<float>(std::min(fy, fy + dy * maxDist) - ex),
        static_cast<float>(std::min(fz, fz + dz * maxDist) - ex),
        static_cast<float>(std::max(fx, fx + dx * maxDist) + ex),
        static_cast<float>(std::max(fy, fy + dy * maxDist) + ex),
        static_cast<float>(std::max(fz, fz + dz * maxDist) + ex)
    );

    double bestT  = 1e30;
    auto   actors = region.fetchEntities(except, bb, true, false);
    for (auto ent : actors) {
        Actor* a = ent.get();
        if (!a || a == except) {
            continue;
        }
        auto const& box = a->getAABB();
        double      t   = 0;
        if (!rayIntersectsBox(from, dir, box.min, box.max, t) || t < 0.05 || t > maxDist) {
            continue;
        }
        if (t < bestT) {
            bestT        = t;
            result.actor = a;
        }
    }
    if (result.actor) {
        result.distance = bestT;
    }
    return result;
}

std::string entityDisplayName(Actor const* actor, std::string const& langCode) {
    if (!actor) {
        return {};
    }
    if (actor->isPlayer()) {
        auto const* p = static_cast<Player const*>(actor);
        try {
            auto name = p->getRealName();
            if (!name.empty()) {
                return name;
            }
        } catch (...) {}
    }
    auto const& tag = actor->getNameTag();
    if (!tag.empty()) {
        return tag;
    }
    std::string key       = "entity." + shortId(actor->getTypeName());
    std::string localized = resolveDisplayName(key, langCode);
    if (localized != key) {
        return localized;
    }
    return shortId(actor->getTypeName());
}

bool entityHasHealth(Actor const& actor) {
    // Health is an engine attribute, not a per-type flag: an entity whose
    // definition has no minecraft:health component simply has no attribute
    // instance (items, projectiles, paintings, flying blocks, ...), so the
    // engine answers this directly and no list of type names is needed.
    return actor.getAttribute(SharedAttributes::HEALTH()).mPtr != nullptr;
}

} // namespace insight
