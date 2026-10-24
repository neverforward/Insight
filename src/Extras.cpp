#include "Extras.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/shared_types/legacy/actor/ArmorSlot.h"
#include "mc/entity/components/ArmorStandPoseIndexComponent.h"
#include "mc/entity/components_json_legacy/BreedableComponent.h"
#include "mc/entity/components_json_legacy/ContainerComponent.h"
#include "mc/entity/components_json_legacy/TransformationComponent.h"
#include "mc/world/Container.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/actor/Motif.h"
#include "mc/world/actor/Painting.h"
#include "mc/world/actor/monster/ZombieVillager.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/state/PropertyComponent.h"
#include "mc/world/attribute/AttributeInstance.h"
#include "mc/world/attribute/AttributeInstanceConstRef.h"
#include "mc/world/attribute/SharedAttributes.h"
#include "mc/world/effect/MobEffectInstance.h"
#include "mc/world/effect/MobEffect.h"
#include "mc/world/effect/MobEffectIds.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStackBase.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/level/block/VanillaStates.h"
#include "mc/world/level/block/actor/BannerBlockActor.h"
#include "mc/world/level/block/actor/BeaconBlockActor.h"
#include "mc/world/level/block/actor/BeehiveBlockActor.h"
#include "mc/world/level/block/actor/BlockActor.h"
#include "mc/world/level/block/actor/BlockActorType.h"
#include "mc/world/level/block/actor/BrewingStandBlockActor.h"
#include "mc/world/level/block/actor/CampfireBlockActor.h"
#include "mc/world/level/block/actor/ComparatorBlockActor.h"
#include "mc/world/level/block/actor/CrafterBlockActor.h"
#include "mc/world/level/block/actor/DecoratedPotBlockActor.h"
#include "mc/world/level/block/actor/FlowerPotBlockActor.h"
#include "mc/world/level/block/actor/FurnaceBlockActor.h"
#include "mc/world/level/block/actor/ItemFrameBlockActor.h"
#include "mc/world/level/block/actor/JukeboxBlockActor.h"
#include "mc/world/level/block/actor/LecternBlockActor.h"
#include "mc/world/level/block/actor/PistonBlockActor.h"
#include "mc/world/level/block/actor/PistonState.h"
#include "mc/world/level/block/actor/ShelfBlockActor.h"
#include "mc/world/level/block/actor/SignBlockActor.h"
#include "mc/world/level/block/actor/component/IVanillaMainBlockActorComponent.h"
#include "mc/world/level/block/components/BlockFlammableComponent.h"
#include "mc/world/level/block/components/BlockInstrumentComponent.h"
#include "mc/world/level/block/states/BlockStateInstance.h"

#include "ll/api/io/Logger.h"

#include "Colors.h"
#include "I18n.h"
#include "Insight.h"
#include "Translation.h"
#include "Util.h"

namespace insight {

namespace {

bool contains(std::string const& s, char const* part) { return s.find(part) != std::string::npos; }

// ---------------------------------------------------------------------------
// localization: engine i18n, exact key first then "<key>.name" (vanilla lang
// file convention); on failure show the raw translation key itself.
// ---------------------------------------------------------------------------
std::string localizeKey(std::string const& langCode, std::string const& key) {
    if (key.empty()) {
        return {};
    }
    return resolveDisplayName(key, langCode);
}

// ---------------------------------------------------------------------------
// generic block-state reader: resolves a state property *by name* through the
// block type's state-name table, then reads this block's value.
// ---------------------------------------------------------------------------

bool regionLoaded(IConstBlockSource const& region, BlockPos const& pos);

// Read a state through the engine's own typed handle (VanillaStates::Candles()
// and friends). This works for data-driven blocks whose legacy name -> id table
// is empty, which is why the name-based lookup silently found nothing for them.
template <class T>
std::optional<T>
readVanillaState(IConstBlockSource const& region, BlockPos const& pos, BlockStateVariant<T> const& state) {
    if (!regionLoaded(region, pos)) {
        return std::nullopt;
    }
    try {
        return region.getBlock(pos).getState<T>(state);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<int> readStateInt(IConstBlockSource const& region, BlockPos const& pos, char const* stateName) {
    try {
        auto const& block = region.getBlock(pos);
        auto const& type  = block.getBlockType();
        // linear lookup is fine here (property tables are small)
        uint64 id    = 0;
        bool   found = false;
        for (auto const& [name, stateId] : *type.mStateNameMap) {
            if (name.getString() == stateName) {
                id    = stateId;
                found = true;
                break;
            }
        }
        if (!found) {
            // Data-driven blocks (minecraft:noteblock, ...) register no name in
            // the legacy table: their states live in mStates, where each instance
            // knows its BlockState and therefore its name.
            for (auto const& [stateId, instance] : *type.mStates) {
                if (instance.mState->mName->getString() == stateName) {
                    return block.getState<int>(stateId);
                }
            }
            for (auto const& collection : *type.mAlteredStateCollections) {
                if (collection && collection->mBlockState->get().mName->getString() == stateName) {
                    return block.getState<int>(collection->mBlockState->get().mID);
                }
            }
            return std::nullopt;
        }
        return block.getState<int>(id);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<int>
readStateIntAny(IConstBlockSource const& region, BlockPos const& pos, std::initializer_list<char const*> names) {
    for (auto const* n : names) {
        if (auto v = readStateInt(region, pos, n)) {
            return v;
        }
    }
    return std::nullopt;
}

std::optional<bool> readStateBool(IConstBlockSource const& region, BlockPos const& pos, char const* stateName) {
    if (auto v = readStateInt(region, pos, stateName)) {
        return *v != 0;
    }
    return std::nullopt;
}

// Every "label value" line in this file is coloured from the same two slots the
// built-in lines use (config `colors.label` / `colors.value`), so one colour
// scheme covers the whole panel - the extras included.
std::string labelText(std::string const& langCode, char const* labelKey) {
    return colored(Insight::cfg().colors.label, tr(langCode, labelKey));
}

std::string valueText(std::string const& text) { return colored(Insight::cfg().colors.value, text); }

std::string valueLine(std::string const& langCode, char const* labelKey, int value) {
    return labelText(langCode, labelKey) + " " + valueText(std::to_string(value));
}

std::string textLine(std::string const& langCode, char const* labelKey, std::string const& value) {
    return labelText(langCode, labelKey) + " " + valueText(value);
}

// ---------------------------------------------------------------------------
// entity adapters (EntityExtrasConfig): the small readers behind the owner,
// effect, timer, pet armour, mount and armour stand lines.
// ---------------------------------------------------------------------------

// A countdown in ticks as "m:ss" - the timers an entity shows are minutes at most.
std::string clockText(int ticks) {
    int const seconds = std::max(0, ticks) / 20;
    char      buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%d:%02d", seconds / 60, seconds % 60);
    return buffer;
}

// A roman numeral for an effect level, the way the vanilla effect list writes it.
#ifndef INSIGHT_TARGET_CLIENT
// The effect line is server-side only: a client is never sent more than the *latest*
// effect of another creature (its list holds that one entry plus empty slots, where a
// server holds the complete list - 19 slots with the real ones in their id places), so
// on a client the line could only ever show one effect and then vanish with it. These
// helpers are therefore compiled for the server alone.

// A roman numeral for an effect level, the way the vanilla effect list writes it.
std::string romanLevel(int level) {
    static constexpr char const* kRoman[] = {"I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"};
    if (level >= 1 && level <= static_cast<int>(std::size(kRoman))) {
        return kRoman[level - 1];
    }
    return std::to_string(level);
}

// Whether an engine effect slot carries a real effect: the list keeps a "no effect"
// placeholder in every slot that is not in use, and that one has id 0.
bool validEffectId(uint id) { return id != 0 && id < static_cast<uint>(MobEffectIds::Count); }

// One status effect the way the vanilla effect list reads it: the engine's own
// translation key for the effect ("potion.moveSpeed") resolved through the game's
// language table, the level as a roman numeral when it is above the first, and the time
// left. That key is what the game builds its own effect list from, so the wording is the
// game's own; the engine's display name is only the fallback, because it carries the
// level while this table does not. An entry the engine has no name for (its "no effect"
// slot, or an effect the table does not know) returns nothing, and the caller counts
// what came back rather than what it iterated.
std::string effectValue(std::string const& langCode, MobEffectInstance const& effect) {
    std::string name;
    bool        named = false;
    if (validEffectId(effect.mId)) {
        auto& table = MobEffect::mMobEffects();
        if (auto const& definition = table[static_cast<MobEffectIds>(effect.mId)]; definition != nullptr) {
            std::string const key = *definition->mDescriptionId;
            if (!key.empty()) {
                name  = localizeKey(langCode, key);
                named = true;
            }
        }
    }
    if (name.empty()) {
        name = effect.getDisplayName();
    }
    if (name.empty()) {
        return {};
    }
    std::string const level = named && effect.mAmplifier > 0 ? " " + romanLevel(effect.mAmplifier + 1) : std::string{};
    return name + level + " " + clockText(effect.mDuration->mValue);
}
#endif

// An attribute's raw value, or nothing when the entity has no such attribute at all.
std::optional<float> attributeValue(Actor const& actor, Attribute const& attribute) {
    if (auto const& instance = actor.getAttribute(attribute); instance.mPtr != nullptr) {
        return instance.mPtr->mCurrentValue;
    }
    return std::nullopt;
}

// The height a horse's jump strength reaches, in blocks: the cubic the game's own
// horse jump uses (the same curve the attribute is documented with).
double jumpHeight(double strength) {
    return -0.1817584952 * strength * strength * strength + 3.689713992 * strength * strength
         + 2.128599134 * strength - 0.343930367;
}

// The pet armour slot: wolf / nautilus armour lives in the body slot, which is not
// one of the four humanoid slots the equipment lines already report, so it gets its
// own line. `mode` is EntityExtrasConfig::petArmor: "all" shows whatever is worn,
// "damaged" only armour that has actually been worn down, "off" nothing. The value
// is "name durability/max" for a damageable piece and just the name for the rest.
std::string petArmorValue(Actor const& actor, std::string const& langCode, std::string const& mode) {
    if (mode == "off") {
        return {};
    }
    auto const& armor = actor.getArmor(SharedTypes::Legacy::ArmorSlot::Body);
    if (armor.isNull()) {
        return {};
    }
    int damage    = 0;
    int maxDamage = 0;
    if (armor.isDamageableItem() && armor.mItem) {
        damage    = armor.getDamageValue();
        maxDamage = armor.mItem->getMaxDamage();
    }
    if (mode == "damaged" && (maxDamage <= 0 || damage <= 0)) {
        return {}; // "damaged only" and this piece is untouched
    }
    std::string value = localizeKey(langCode, armor.getDescriptionId());
    if (maxDamage > 0) {
        value += " " + std::to_string(maxDamage - damage) + "/" + std::to_string(maxDamage);
    }
    return value;
}

// The horse family, by the engine's own type ids: the speed / jump numbers only mean
// something for these (the attribute exists for every mob).
bool isHorse(Actor const& actor) {
    return actor.isType(ActorType::Horse) || actor.isType(ActorType::Donkey) || actor.isType(ActorType::Mule)
        || actor.isType(ActorType::SkeletonHorse) || actor.isType(ActorType::ZombieHorse);
}

// A horse's coat colour and markings, in exactly the order the game's own horse render
// controller lists its textures in (Array.base / Array.markings, indexed by
// query.variant / query.mark_variant) - which is the same order as HorseVariant /
// HorseMarkings. Marking 0 is "none" and is skipped rather than spelled out.
std::string horseCoatName(std::string const& langCode, int variant) {
    static constexpr char const* kCoats[] = {
        "White", "Creamy", "Chestnut", "Brown", "Black", "Gray", "Dark brown",
    };
    if (variant < 0 || variant >= static_cast<int>(std::size(kCoats))) {
        return {};
    }
    return tr(langCode, kCoats[variant]);
}

std::string horseMarkingsName(std::string const& langCode, int markings) {
    static constexpr char const* kMarkings[] = {
        nullptr, "White details", "White fields", "White dots", "Black dots",
    };
    if (markings <= 0 || markings >= static_cast<int>(std::size(kMarkings)) || kMarkings[markings] == nullptr) {
        return {};
    }
    return tr(langCode, kMarkings[markings]);
}

// A synced actor-data integer. This is the channel the client and the server share:
// the entity's *properties* are not all synced, actor data is what the client's own
// render controllers read as well.
std::optional<int> syncedInt(Actor const& actor, ActorDataIDs id) {
    auto const  key  = static_cast<ushort>(id);
    auto const& data = actor.mEntityData; // the storage wraps the wrapper: use ->
    if (!data->hasData(key)) {
        return std::nullopt;
    }
    return data->getInt(key);
}

// The villager's profession, from the synced actor data the client's own villager
// render controller reads (variable.profession_index = query.variant). The names are
// the game's own "entity.villager.*" keys, so they follow the player's language; the
// last entry (the nitwit) has no key in the game's language files and carries its own.
std::string villagerProfession(Actor const& actor, std::string const& langCode) {
    static constexpr char const* kProfessions[] = {
        "unskilled", "farmer", "fisherman", "shepherd", "fletcher", "librarian", "cartographer",
        "cleric",    "armor",  "weapon",    "tool",     "butcher",  "leather",     "mason",
    };
    auto const profession = syncedInt(actor, ActorDataIDs::Variant);
    if (!profession || *profession < 0 || *profession > 14) {
        return {};
    }
    if (*profession == 14) {
        return tr(langCode, "Nitwit");
    }
    std::string const key  = std::string("entity.villager.") + kProfessions[*profession];
    std::string const name = localizeKey(langCode, key);
    return name == key ? std::string(kProfessions[*profession]) : name;
}

// Whether a copper golem is waxed. The engine has no member, component or actor-data
// id for it - the state is an entity *property*, which the game's own JSON declares as
// "minecraft:is_waxed". A client is never sent that property, so this whole reader is
// server-side (see the caller).
#ifndef INSIGHT_TARGET_CLIENT
bool copperGolemWaxed(Actor const& actor) {
    auto const properties = actor.getEntityContext().tryGetComponent<PropertyComponent>();
    if (!properties) {
        return false;
    }
    return properties->tryGetBool(HashedString::computeHash("minecraft:is_waxed")).value_or(false);
}
#endif

// ---------------------------------------------------------------------------
// Debug diagnostics: these run on every sampling round, so they are emitted
// once per change (same tag + position + message) instead of on every round.
// The log level decides whether they show up at all.
// ---------------------------------------------------------------------------
std::map<std::tuple<std::string, int, int, int>, std::string>& loggedOnce() {
    static std::map<std::tuple<std::string, int, int, int>, std::string> map;
    return map;
}

void clearLoggedOnce() { loggedOnce().clear(); }

void logOnce(ll::io::Logger& logger, char const* tag, BlockPos const& pos, std::string const& message) {
    static std::map<std::tuple<std::string, int, int, int>, std::string>& lastLogged = loggedOnce();

    if (!logger.shouldLog(ll::io::LogLevel::Debug)) {
        return;
    }
    // the bookkeeping is only a throttle for the log: drop it when it grows
    if (lastLogged.size() > 512) {
        lastLogged.clear();
    }
    auto key = std::make_tuple(std::string(tag), pos.x, pos.y, pos.z);
    if (auto it = lastLogged.find(key); it != lastLogged.end() && it->second == message) {
        return;
    }
    lastLogged[key] = message;
    logger.debug("[{}] ({},{},{}) {}", tag, pos.x, pos.y, pos.z, message);
}

// ---------------------------------------------------------------------------
// direction helpers (Bedrock state value conventions)
// ---------------------------------------------------------------------------
std::string facingWord(std::string const& langCode, int code) {
    // 0 = down, 1 = up, 2 = north, 3 = south, 4 = west, 5 = east
    switch (code) {
    case 0:
        return tr(langCode, "down");
    case 1:
        return tr(langCode, "up");
    case 2:
        return tr(langCode, "north");
    case 3:
        return tr(langCode, "south");
    case 4:
        return tr(langCode, "west");
    case 5:
        return tr(langCode, "east");
    default:
        return {};
    }
}

std::string compassWord(std::string const& langCode, int code) {
    // 0 = south, 1 = west, 2 = north, 3 = east
    switch (((code % 4) + 4) % 4) {
    case 0:
        return tr(langCode, "south");
    case 1:
        return tr(langCode, "west");
    case 2:
        return tr(langCode, "north");
    default:
        return tr(langCode, "east");
    }
}

// BlockPos offset for a facing_direction value (0 down, 1 up, 2 north, 3 south,
// 4 west, 5 east), used for piston-arm / attachment checks.
BlockPos facingOffset(int code) {
    switch (code) {
    case 0:
        return BlockPos(0, -1, 0);
    case 1:
        return BlockPos(0, 1, 0);
    case 2:
        return BlockPos(0, 0, -1);
    case 3:
        return BlockPos(0, 0, 1);
    case 4:
        return BlockPos(-1, 0, 0);
    case 5:
        return BlockPos(1, 0, 0);
    default:
        return BlockPos(0, 0, 0);
    }
}

// Reading a block from an unloaded chunk is undefined; guard every read of a
// neighbour / scanned position with the same chunk test the raycast uses.
bool regionLoaded(IConstBlockSource const& region, BlockPos const& pos) {
    try {
        return region.hasChunksAt(pos, 0, false);
    } catch (...) {
        return false;
    }
}

std::string blockTypeAt(IConstBlockSource const& region, BlockPos const& pos) {
    if (!regionLoaded(region, pos)) {
        return {};
    }
    try {
        return region.getBlock(pos).getTypeName();
    } catch (...) {
        return {};
    }
}

bool isAirAt(IConstBlockSource const& region, BlockPos const& pos) {
    if (!regionLoaded(region, pos)) {
        return false;
    }
    try {
        return region.getBlock(pos).isAir();
    } catch (...) {
        return false;
    }
}

// ---------------------------------------------------------------------------
// live container snapshot (client): see setLiveContainerSnapshot()
// ---------------------------------------------------------------------------
struct ContainerSnapshot {
    std::vector<ContainerItem> items; // the non-empty slots, in slot order
    int                        size = 0;
};

std::map<std::tuple<int, int, int>, ContainerSnapshot>& liveSnapshots() {
    static std::map<std::tuple<int, int, int>, ContainerSnapshot> map;
    return map;
}

std::map<int64_t, ContainerSnapshot>& liveEntitySnapshots() {
    static std::map<int64_t, ContainerSnapshot> map;
    return map;
}

// ---------------------------------------------------------------------------
// containers: slot scanning (data available server-side and in local worlds)
// ---------------------------------------------------------------------------
// 26.40 moved container access off BlockActor: an actor now hands out its "main
// component", and that component owns the container. Every container actor
// implements it (chest, barrel, hopper, furnace, brewing stand, ...), so this
// stays engine-driven instead of listing block actor types.
Container const* containerOf(BlockActor const* actor) {
    if (!actor) {
        return nullptr;
    }
    auto const* main = actor->_getMainComponent();
    return main ? main->getContainer() : nullptr;
}


// Compact "name ×count" list of the slots that are not empty (shelf, chiseled
// bookshelf). The interesting part of those blocks is what they hold, while
// chestLine() already reports how full they are.
std::string itemListLine(Container const& container, std::string const& langCode) {
    std::string out;
    int         shown = 0;
    for (int i = 0; i < container.getContainerSize(); ++i) {
        auto const& stack = container.getItem(i);
        if (stack.isNull()) {
            continue;
        }
        if (shown == 4) {
            out += " " + colored(Insight::cfg().colors.label, "...");
            break;
        }
        if (!out.empty()) {
            out += colored(Insight::cfg().colors.label, ", ");
        }
        out += valueText(localizeKey(langCode, stack.getDescriptionId()));
        if (stack.mCount > 1) {
            out += valueText("x" + std::to_string(stack.mCount));
        }
        ++shown;
    }
    return out;
}

// The non-empty slots of a container, in slot order, in the form the client panel turns
// into item icons. Null stacks are skipped (an empty slot is not drawn) and the stack's
// aux value is kept, because a name alone does not carry the variant (wool colour, ...).
std::vector<ContainerItem> readContainerItems(Container const& container) {
    std::vector<ContainerItem> items;
    int const                  size = container.getContainerSize();
    if (size > 0) {
        items.reserve(static_cast<size_t>(size));
    }
    for (int i = 0; i < size; ++i) {
        auto const& stack = container.getItem(i);
        if (stack.isNull() || !stack.mItem) {
            continue;
        }
        items.push_back(ContainerItem{stack.mItem->mFullName->getString(), stack.mCount, stack.getAuxValue()});
    }
    return items;
}

// The number of items a list of stacks holds in total.
int totalCountOf(std::vector<ContainerItem> const& items) {
    int total = 0;
    for (auto const& item : items) {
        total += item.count;
    }
    return total;
}

std::string chestLine(
    BlockActor const*   be,
    BlockPos const&     pos,
    std::string const&  langCode,
    ContainerContents*  out
) {
    if (!be) {
        return {};
    }
    auto* c = containerOf(be);
    if (!c) {
        return {};
    }
    int size = c->getContainerSize();
    if (size <= 0) {
        return {};
    }
    std::vector<ContainerItem> items  = readContainerItems(*c);
    int                        filled = static_cast<int>(items.size());
    int                        total  = totalCountOf(items);

    // Prefer the live snapshot taken from an open container UI: the engine's
    // world copy of a container keeps its old contents until the chunk is
    // reloaded, so the player's own edits would otherwise not show up. The snapshot
    // carries the stacks themselves now, so the icons follow an open screen just as
    // the numbers do.
    int   worldFilled  = filled;
    int   worldTotal   = total;
    bool  usedSnapshot = false;
    auto& snapshots    = liveSnapshots();
    auto  key          = std::make_tuple(pos.x, pos.y, pos.z);
    if (auto it = snapshots.find(key); it != snapshots.end()) {
        auto const& snap = it->second;
        if (snap.size == size && snap.items == items) {
            snapshots.erase(it); // the world copy caught up: drop the snapshot
        } else {
            items        = snap.items;
            filled       = static_cast<int>(items.size());
            total        = totalCountOf(items);
            usedSnapshot = true;
        }
    }
    {
        logOnce(
            Insight::getInstance().getSelf().getLogger(),
            "chest",
            pos,
            "world=" + std::to_string(worldFilled) + "/" + std::to_string(size) + " (total "
                + std::to_string(worldTotal) + ") shown=" + std::to_string(filled) + "/" + std::to_string(size)
                + " (total " + std::to_string(total) + ") used_snapshot=" + (usedSnapshot ? "yes" : "no")
        );
    }

    if (out != nullptr) {
        out->items = std::move(items);
    }
    std::string s = labelText(langCode, "Items") + " " + valueText(std::to_string(filled) + "/" + std::to_string(size));
    if (total > 0) {
        s += " " + colored(Insight::cfg().colors.label, "·") + " " + valueText(std::to_string(total));
    }
    return s;
}

std::string machineLine(BlockActor const* be, BlockPos const& pos, std::string const& langCode) {
    if (!be) {
        return {};
    }
    auto* c = containerOf(be);
    if (!c) {
        return {};
    }
    int size = c->getContainerSize();
    if (size <= 0) {
        return {};
    }
    std::vector<ContainerItem> const items  = readContainerItems(*c);
    int                              filled = static_cast<int>(items.size());
    // Furnaces / brewing stands are block containers too: their screen models hold the live
    // stacks while the block entity's own copy of them is partial or stale, so the snapshot
    // taken from the open screen is the better source - the same rule as chestLine().
    auto& snapshots = liveSnapshots();
    auto  key       = std::make_tuple(pos.x, pos.y, pos.z);
    if (auto it = snapshots.find(key); it != snapshots.end()) {
        auto const& snap = it->second;
        if (snap.size == size && snap.items == items) {
            snapshots.erase(it);
        } else {
            filled = static_cast<int>(snap.items.size());
        }
    }
    return labelText(langCode, "Slots") + " " + valueText(std::to_string(filled) + "/" + std::to_string(size));
}

// First non-empty slot of a container as a localized item name ("" if empty).
std::string firstItemName(BlockActor const* be, std::string const& langCode) {
    if (!be) {
        return {};
    }
    auto* c = containerOf(be);
    if (!c) {
        return {};
    }
    for (int i = 0; i < c->getContainerSize(); ++i) {
        auto const& st = c->getItem(i);
        if (!st.isNull()) {
            return localizeKey(langCode, st.getDescriptionId());
        }
    }
    return {};
}

// Container-ness is engine data (the block type knows whether it is a
// container), so no list of container block ids is needed here.
bool isContainerBlock(IConstBlockSource const& region, BlockPos const& pos) {
    if (!regionLoaded(region, pos)) {
        return false;
    }
    try {
        return region.getBlock(pos).getBlockType().isContainerBlock();
    } catch (...) {
        return false;
    }
}

// Modern versions encode the plant in the block itself, and the engine exposes
// no flag for "potted plant" - but the block does name itself accordingly
// ("tile.potted_<plant>"), which covers every current and future potted plant
// without a list of block ids.
bool isPottedBlock(IConstBlockSource const& region, BlockPos const& pos) {
    if (!regionLoaded(region, pos)) {
        return false;
    }
    try {
        return region.getBlock(pos).getDescriptionId().rfind("tile.potted_", 0) == 0;
    } catch (...) {
        return false;
    }
}

// ---------------------------------------------------------------------------
// comparator: the block state carries the value the engine just wrote, so it is
// read first; the block actor is only a fallback. Reading the actor first made
// the line report 0 forever: its signal is written on the server tick, while the
// state is already correct when the client looks at it.
// ---------------------------------------------------------------------------
std::string comparatorLine(
    IConstBlockSource const& region,
    BlockPos const&          pos,
    BlockActor const*        be,
    std::string const&       langCode
) {
    if (auto v = readStateIntAny(region, pos, {"output_signal", "powered"})) {
        return valueLine(langCode, "Signal", *v);
    }
    if (be && be->getType() == BlockActorType::Comparator) {
        int signal = const_cast<ComparatorBlockActor*>(static_cast<ComparatorBlockActor const*>(be))->getOutputSignal();
        return valueLine(langCode, "Signal", signal);
    }
    return {};
}

// ---------------------------------------------------------------------------
// misc states: machines and the block states that describe them.
//
// The engine's own state names drive everything here: a block that reports
// `open_bit` is a door/trapdoor/fence gate/lever, one that reports `crafting`
// is a crafter, one that reports `composter_fill_level` is a composter, and so
// on. That is why this function needs no list of block ids - it simply reports
// the states the block actually has. Only two cases cannot be expressed that
// way and are marked below (the piston arm block id, and the enchanting table
// whose power comes from its surroundings).
// ---------------------------------------------------------------------------

void miscStateLines(
    IConstBlockSource const&  region,
    BlockPos const&           pos,
    std::string const&        type,
    std::string const&        langCode,
    std::vector<std::string>& lines,
    BlockExtrasConfig const&  opt
) {
    BlockActor const* be = region.getBlockEntity(pos);

    // ---- binary states ---------------------------------------------------
    if (auto v = readStateBool(region, pos, "open_bit")) { // doors, trapdoors, gates, levers
        lines.push_back(textLine(langCode, "State", tr(langCode, *v ? "open" : "closed")));
    }
    if (auto v = readStateBool(region, pos, "powered")) { // redstone power (doors, ...)
        if (*v) {
            lines.push_back(textLine(langCode, "Powered", tr(langCode, "yes")));
        }
    }
    if (auto v = readStateBool(region, pos, "button_pressed_bit")) {
        lines.push_back(textLine(langCode, "State", tr(langCode, *v ? "pressed" : "released")));
    }
    if (auto v = readStateBool(region, pos, "connected_bit")) { // tripwire hooks
        lines.push_back(textLine(langCode, "Connected", tr(langCode, *v ? "yes" : "no")));
    }
    if (auto v = readStateBool(region, pos, "occupied_bit")) { // bed
        lines.push_back(textLine(langCode, "Occupied", tr(langCode, *v ? "yes" : "no")));
    }
    if (auto v = readStateBool(region, pos, "powered_bit")) { // observers, tripwire hooks, ...
        lines.push_back(textLine(langCode, "Powered", tr(langCode, *v ? "yes" : "no")));
    }
    // A crafter has exactly two states of its own: `crafting` (mouth open, top
    // glowing) and `triggered_bit` (it was activated). Bedrock spells booleans
    // with the `_bit` suffix - reading the Java spelling "triggered" matched no
    // state at all, which is why only one of the two ever showed up. Its disabled
    // slots are not a state either: they live in the block entity.
    auto const crafting = readStateBool(region, pos, "crafting");
    if (crafting) {
        lines.push_back(textLine(langCode, "State", tr(langCode, *crafting ? "crafting" : "idle")));
        if (auto v = readStateBool(region, pos, "triggered_bit")) {
            lines.push_back(textLine(langCode, "Triggered", tr(langCode, *v ? "yes" : "no")));
        }
        if (be && be->getType() == BlockActorType::Crafter) {
            auto const* crafter  = static_cast<CrafterBlockActor const*>(be);
            int const   disabled = static_cast<int>(crafter->mDisabledSlots->count());
            if (disabled > 0) {
                lines.push_back(valueLine(langCode, "Disabled slots", disabled));
            }
        }
    }

    // ---- numeric states --------------------------------------------------
    if (auto v = readStateInt(region, pos, "composter_fill_level")) {
        lines.push_back(textLine(langCode, "Compost", std::to_string(*v) + "/8"));
    }
    if (auto v = readStateInt(region, pos, "bite_counter")) { // cake
        lines.push_back(textLine(langCode, "Slices", std::to_string(7 - *v) + "/7"));
    }
    // TODO(26.40 + LeviLamina): the note block gives us nothing to read. Verified
    // in-game for minecraft:noteblock: BlockType::mStateNameMap is empty, mStates is
    // empty, mAlteredStateCollections has no named state and Block::getData() stays
    // 0, so neither the pitch nor the instrument can be obtained (VanillaStates has
    // no Note() handle either, and the instrument component lookup reads unrelated
    // memory, like BlockFlammableComponent did). The note pitch / instrument lines
    // and the extras.noteBlock switch were removed together; re-add them once the
    // data is reachable.
    if (auto v = readStateInt(region, pos, "cluster_count")) { // sea pickle
        lines.push_back(valueLine(langCode, "Count", *v + 1));
    }

    // ---- a few more states ----------------------------------
    if (auto v = readVanillaState(region, pos, VanillaStates::BeehiveHoneyLevel())) { // beehive / bee nest
        lines.push_back(textLine(langCode, "Honey level", std::to_string(*v) + "/5"));
    }
    if (auto v = readVanillaState(region, pos, VanillaStates::Extinguished())) { // campfire, soul campfire
        lines.push_back(textLine(langCode, "State", tr(langCode, *v ? "extinguished" : "lit")));
    }

    // ---- redstone components (one switch each) ---------------------------
    if (opt.repeater) {
        auto delay = readVanillaState(region, pos, VanillaStates::RepeaterDelay());
        if (!delay) {
            delay = readStateInt(region, pos, "repeater_delay");
        }
        if (delay) { // 0..3 -> 1..4
            lines.push_back(textLine(langCode, "Delay", std::to_string(*delay + 1) + "/4"));
        }
        // a repeater stores no signal level: it either passes full power or none
        auto powered = readVanillaState(region, pos, VanillaStates::PoweredBit());
        if (!powered) {
            powered = readStateBool(region, pos, "powered");
        }
        if (powered) {
            lines.push_back(textLine(langCode, "Signal", *powered ? "15" : "0"));
        }
    }
    if (opt.dispenser && !crafting) {
        // dispensers and droppers keep the state of their last activation; a
        // crafter has the same state name but reports it as its own (above)
        if (auto v = readStateBool(region, pos, "triggered_bit")) {
            lines.push_back(textLine(langCode, "State", tr(langCode, *v ? "triggered" : "idle")));
        }
    }
    if (opt.candle) {
        auto candles = readVanillaState(region, pos, VanillaStates::Candles());
        if (!candles) {
            candles = readStateInt(region, pos, "candles");
        }
        if (candles) {
            auto const v = candles;
            // the engine counts candles from 0: a single candle reports 0
            lines.push_back(textLine(langCode, "Candles", std::to_string(*v + 1) + "/4"));
        }
        auto lit = readVanillaState(region, pos, VanillaStates::Lit());
        if (!lit) {
            lit = readStateBool(region, pos, "lit");
        }
        if (lit) {
            lines.push_back(textLine(langCode, "State", tr(langCode, *lit ? "Lit" : "Unlit")));
        }
    }
    if (opt.respawnAnchor) {
        if (auto v = readStateInt(region, pos, "respawn_anchor_charge")) {
            lines.push_back(textLine(langCode, "Charge", std::to_string(*v) + "/4"));
        }
    }

    // ---- pistons ---------------------------------------------------------
    // A piston body has no state of its own, but its block actor tracks the
    // real animation state.
    if (opt.piston) {
        std::optional<PistonState> pistonState;
        if (be && be->getType() == BlockActorType::PistonArm) {
            // mState is a small trivially copyable member, so TypedStorage resolves
            // to PistonState itself (no dereference needed).
            pistonState = static_cast<PistonBlockActor const*>(be)->mState;
        }
        if (pistonState) {
            std::string stateText;
            switch (*pistonState) {
            case PistonState::Expanded:
                stateText = tr(langCode, "extended");
                break;
            case PistonState::Expanding:
                stateText = tr(langCode, "extending");
                break;
            case PistonState::Retracting:
                stateText = tr(langCode, "retracting");
                break;
            default:
                stateText = tr(langCode, "retracted");
                break;
            }
            {
                std::string neighbors;
                for (auto const& offset : {
                         BlockPos{0,  -1, 0 },
                         BlockPos{0,  1,  0 },
                         BlockPos{0,  0,  -1},
                         BlockPos{0,  0,  1 },
                         BlockPos{-1, 0,  0 },
                         BlockPos{1,  0,  0 }
                }) {
                    if (!neighbors.empty()) {
                        neighbors += " ";
                    }
                    neighbors += blockTypeAt(region, pos + offset);
                }
                logOnce(
                    Insight::getInstance().getSelf().getLogger(),
                    "piston",
                    pos,
                    "type=" + type
                        + " actor=" + (be ? std::to_string(static_cast<int>(be->getType())) : std::string("<none>"))
                        + " state=" + std::to_string(static_cast<int>(*pistonState)) + " states=["
                        + describeBlockStateNames(region, pos) + "] neighbors=[" + neighbors + "]"
                );
            }
            lines.push_back(textLine(langCode, "State", stateText));
        } else if (auto facing = readStateInt(region, pos, "facing_direction")) {
            // No piston actor (a client-side copy of a remote world): the extended
            // state is visible as the arm block the piston pushes in front of it.
            // The arm's own block id is the engine data here, so only "extended"
            // can be detected this way - a retracted piston shows no state line.
            auto armType = blockTypeAt(region, pos + facingOffset(*facing));
            if (contains(armType, "piston_arm") || contains(armType, "moving_block")) {
                lines.push_back(textLine(langCode, "State", tr(langCode, "extended")));
            }
        }
    }

    // The arm block itself: Bedrock models it as its own block id
    // (piston_arm_collision / sticky_piston_arm_collision) and exposes neither
    // a state nor a block actor, so the id is the only data available.
    if (contains(type, "piston_arm")) {
        bool sticky = type.find("sticky") != std::string::npos;
        lines.push_back(textLine(langCode, "Piston", sticky ? tr(langCode, "sticky") : tr(langCode, "normal")));
        lines.push_back(textLine(langCode, "State", tr(langCode, "extended")));
    }

    // ---- enchanting table ------------------------------------------------
    // No state involved: the power comes from the bookshelves around it, so the
    // block actor type is what identifies the table.
    if (be && be->getType() == BlockActorType::EnchantingTable) {
        int power = 0;
        for (int dy = 0; dy <= 1; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
                for (int dz = -2; dz <= 2; ++dz) {
                    if (dx >= -1 && dx <= 1 && dz >= -1 && dz <= 1) {
                        continue;
                    }
                    BlockPos shelf(pos.x + dx, pos.y + dy, pos.z + dz);
                    if (blockTypeAt(region, shelf) != "minecraft:bookshelf") {
                        continue;
                    }
                    // The block directly between shelf and table must be air
                    // (vanilla requires one free block along the axis the shelf
                    // sits on: |dx| == 2 -> x axis, otherwise the z axis).
                    BlockPos gap = (dx == 2 || dx == -2) ? BlockPos(pos.x + (dx > 0 ? 1 : -1), pos.y + dy, pos.z)
                                                         : BlockPos(pos.x, pos.y + dy, pos.z + (dz > 0 ? 1 : -1));
                    if (isAirAt(region, gap)) {
                        ++power;
                    }
                }
            }
        }
        lines.push_back(valueLine(langCode, "Enchant power", std::min(power, 15)));
    }
}

// ---------------------------------------------------------------------------
// Local cooking estimate.
//
// The engine only details a block entity to a remote client while its container
// screen is open, so everything we can read client side (slots, cooking
// progress, fuel) stops moving the moment that screen is closed. Keep the last
// value the engine gave us and let the clock carry it forward: the estimate is
// exact as long as the machine goes on burning the items that were inside it,
// and it is re-anchored the moment a real value arrives (screen opened again, or
// any change to the slots / to the item being cooked). Fuel running out, or a
// hopper refilling while nobody looks, is the one thing a client cannot see; the
// estimate is bounded by the items that were really in the machine, so it never
// invents a cooking machine that was empty.
struct CookEstimate {
    std::uint64_t                         tag     = 0;
    int                                   perItem = 0;
    int                                   elapsed = 0;
    int                                   queued  = 1;
    std::chrono::steady_clock::time_point anchored{};
    std::chrono::steady_clock::time_point seen{};
};

std::unordered_map<std::uint64_t, CookEstimate>& cookEstimates() {
    static std::unordered_map<std::uint64_t, CookEstimate> estimates;
    return estimates;
}

std::uint64_t cookEstimateKey(void const* source, BlockPos const& pos, int slot) {
    std::uint64_t key = reinterpret_cast<std::uint64_t>(source);
    key               = key * 1000003u + static_cast<std::uint32_t>(pos.x);
    key               = key * 1000003u + static_cast<std::uint32_t>(pos.y);
    key               = key * 1000003u + static_cast<std::uint32_t>(pos.z);
    key               = key * 1000003u + static_cast<std::uint32_t>(slot);
    return key;
}

// `rawElapsed` is the ticks the machine had already done on the item in hand and
// `queued` how many items it had left (that one included), both as of the last
// value the engine sent. Returns false when there is nothing honest left to say:
// nothing was cooking when we last saw a real value, or every item we knew about
// has finished since.
bool advanceCooking(
    void const*     source,
    BlockPos const& pos,
    int             slot,
    std::uint64_t   tag,
    int             perItem,
    int             rawElapsed,
    int             queued,
    int&            outElapsed
) {
    if (perItem <= 0 || rawElapsed <= 0) {
        return false;
    }
    auto&      estimates = cookEstimates();
    auto const now       = std::chrono::steady_clock::now();
    if (estimates.size() > 64) { // nobody keeps every furnace they ever looked at
        for (auto it = estimates.begin(); it != estimates.end();) {
            if (now - it->second.seen > std::chrono::seconds(30)) {
                it = estimates.erase(it);
            } else {
                ++it;
            }
        }
    }
    auto [it, inserted] = estimates.try_emplace(cookEstimateKey(source, pos, slot));
    auto& estimate      = it->second;
    if (inserted || estimate.tag != tag || estimate.perItem != perItem || estimate.elapsed != rawElapsed
        || estimate.queued != queued) {
        if (inserted) { // once per block: what the engine actually reported, for calibration
            logOnce(
                Insight::getInstance().getSelf().getLogger(),
                "cook",
                pos,
                "anchor raw=" + std::to_string(rawElapsed) + "/" + std::to_string(perItem)
                    + " queued=" + std::to_string(queued)
            );
        }
        estimate = CookEstimate{tag, perItem, rawElapsed, std::max(1, queued), now, now};
    } else {
        estimate.seen = now;
    }
    double elapsed = static_cast<double>(estimate.elapsed)
                   + std::chrono::duration<double>(now - estimate.anchored).count() * 20.0; // the server runs at 20 tps
    int    left    = estimate.queued; // items we know were in the machine, the cooking one included
    while (elapsed >= static_cast<double>(perItem)) {
        if (left <= 1) {
            return false; // all of them are done, and we cannot see what came next
        }
        elapsed -= static_cast<double>(perItem);
        --left;
    }
    outElapsed = std::max(1, static_cast<int>(elapsed));
    return true;
}

// Block-actor based extras that are not plain containers.
// Numbers the engine keeps inside the block entity: brewing-stand progress, honey in a bee
// nest, beacon level, campfire cooking. All of them are engine fields, so no block id list is
// involved; each line obeys the switch of the adapter it belongs to.
void blockEntityNumberLines(
    void const*               source,
    BlockPos const&           pos,
    BlockActor const*         be,
    std::string const&        langCode,
    BlockExtrasConfig const&  opt,
    std::vector<std::string>& lines
) {
    if (!be) {
        return;
    }
    switch (be->getType()) {
    // The furnace family has no line of its own here: a client is only sent a partial, bursty
    // copy of a machine's cooking counter, so its `Time left` / `Cook progress` lines never
    // matched what the machine was really doing and were taken out again. Its slot line and its
    // contents still come from machineLine() and chestLine().
    case BlockActorType::Furnace:
    case BlockActorType::BlastFurnace:
    case BlockActorType::Smoker:
        break;
    case BlockActorType::BrewingStand: {
        if (!opt.brewing) {
            break;
        }
        auto const* stand = static_cast<BrewingStandBlockActor const*>(be);
        int const   time  = static_cast<int>(stand->mBrewTime); // ticks left of 400
        int         done  = 0;
        if (advanceCooking(source, pos, 0, 0, 400, 400 - time, 1, done)) {
            lines.push_back(textLine(langCode, "Brewing", std::to_string(done / 4) + "%"));
        }
        int const fuel = static_cast<int>(stand->mFuelAmount);
        if (fuel > 0) {
            lines.push_back(textLine(langCode, "Fuel", std::to_string(fuel) + "/" + std::to_string(stand->mFuelTotal)));
        }
        break;
    }
    case BlockActorType::Beehive: {
        if (!opt.misc) {
            break;
        }
        auto const* hive = static_cast<BeehiveBlockActor const*>(be);
        lines.push_back(textLine(langCode, "Bees", std::to_string(hive->mOccupants->size()) + "/3"));
        break;
    }
    case BlockActorType::Beacon: {
        if (!opt.misc) {
            break;
        }
        auto const* beacon = static_cast<BeaconBlockActor const*>(be);
        int const   level  = static_cast<int>(beacon->mNumLevels);
        if (level > 0) {
            lines.push_back(textLine(langCode, "Beacon level", std::to_string(level)));
        }
        break;
    }
    case BlockActorType::Campfire: {
        if (!opt.misc) {
            break;
        }
        auto const* campfire = static_cast<CampfireBlockActor const*>(be);
        for (int slot = 0; slot < 4; ++slot) {
            auto const& item = *campfire->mCookingItem[slot];
            if (item.isNull()) {
                continue;
            }
            // mCookingTime counts the ticks left for that slot (600 = 30 s total)
            int const           left = static_cast<int>(campfire->mCookingTime[slot]);
            std::uint64_t const tag  = std::hash<std::string>{}(item.getDescriptionId());
            int                 done = 0;
            if (!advanceCooking(source, pos, slot, tag, 600, 600 - left, 1, done)) {
                continue;
            }
            lines.push_back(textLine(
                langCode,
                "Cooking",
                localizeKey(langCode, item.getDescriptionId()) + " " + std::to_string((600 - done + 19) / 20) + "s/30s"
            ));
        }
        break;
    }
    default:
        break;
    }
}

void blockActorLines(
    IConstBlockSource const&  region,
    BlockPos const&           pos,
    std::string const&        type,
    std::string const&        langCode,
    BlockActor const*         be,
    std::vector<std::string>& lines,
    BlockExtrasConfig const&  opt
) {
    blockEntityNumberLines(&region, pos, be, langCode, opt, lines);
    (void)region;
    (void)pos;
    (void)type;
    if (!be) {
        return;
    }

    if (opt.banner && be->getType() == BlockActorType::Banner) {
        auto const* banner = static_cast<BannerBlockActor const*>(be);
        // 26.40 turned getPatternCount() into a static that expects a tag; the
        // pattern vector is available on both platforms, so it is read directly.
        int count = static_cast<int>((*banner->mPatterns).size());
        if (count > 0) {
            lines.push_back(valueLine(langCode, "Patterns", count));
        } else {
            lines.push_back(textLine(langCode, "Patterns", tr(langCode, "none")));
        }
        return;
    }
    if (opt.pot && be->getType() == BlockActorType::DecoratedPot) {
        auto const* pot = static_cast<DecoratedPotBlockActor const*>(be);
        // The pot itself is the container (DecoratedPotBlockActor derives from
        // Container) and 26.40 exposes the item range through the actor's own
        // container interface (getItem(0) / mContainedItem) instead.
        if (pot->getContainerSize() > 0) {
            auto const& stack = pot->getItem(0);
            if (stack.isNull()) {
                lines.push_back(textLine(langCode, "Item", tr(langCode, "empty")));
            } else {
                lines.push_back(textLine(
                    langCode,
                    "Item",
                    localizeKey(langCode, stack.getDescriptionId()) + " ×" + std::to_string(stack.mCount)
                ));
            }
        }
        auto const& sherds = *pot->mSherdItemNames; // 26.40: was getSherdNames()
        int         custom = 0;
        for (auto const& s : sherds) {
            if (!s.empty() && s != "minecraft:brick") {
                ++custom;
            }
        }
        {
            int         size    = pot->getContainerSize();
            std::string message = "containerSize=" + std::to_string(size);
            if (size > 0) {
                auto const& stack  = pot->getItem(0);
                message           += " item=" + (stack.isNull() ? std::string("<empty>") : stack.getDescriptionId())
                                   + " count=" + std::to_string(stack.mCount);
            }
            message += " sherds=" + std::to_string(custom);
            logOnce(Insight::getInstance().getSelf().getLogger(), "pot", pos, message);
        }
        lines.push_back(valueLine(langCode, "Sherds", custom) + "/4");
        return;
    }
    if (opt.bookshelf && be->getType() == BlockActorType::ChiseledBookshelf) {
        if (auto* container = containerOf(be)) {
            if (auto list = itemListLine(*container, langCode); !list.empty()) {
                lines.push_back(labelText(langCode, "Bookshelf") + " " + list);
            }
        }
        return;
    }
    if (be->getType() == BlockActorType::Shelf) {
        auto const* shelf = static_cast<ShelfBlockActor const*>(be);
        // isSlotOccupied() is client-only; the container interface is shared.
        int used = 0;
        for (int i = 0; i < shelf->getContainerSize(); ++i) {
            if (!shelf->getItem(i).isNull()) {
                ++used;
            }
        }
        lines.push_back(valueLine(langCode, "Items", used) + "/3");
        if (auto list = itemListLine(*containerOf(be), langCode); !list.empty()) {
            lines.push_back(labelText(langCode, "Shelf") + " " + list);
        }
        return;
    }
    if (opt.itemFrame
        && (be->getType() == BlockActorType::ItemFrame || be->getType() == BlockActorType::GlowItemFrame)) {
        auto const* frame = static_cast<ItemFrameBlockActor const*>(be);
        lines.push_back(textLine(langCode, "Rotation", std::to_string(static_cast<int>(frame->mRotation)) + "°"));
        auto const& item = *frame->mItem; // 26.40: was getFramedItem()
        if (item.isNull()) {
            lines.push_back(textLine(langCode, "Displayed item", tr(langCode, "empty")));
        } else {
            lines.push_back(textLine(langCode, "Displayed item", localizeKey(langCode, item.getDescriptionId())));
        }
        return;
    }
    if (opt.sign && (be->getType() == BlockActorType::Sign || be->getType() == BlockActorType::HangingSign)) {
        auto const* sign = static_cast<SignBlockActor const*>(be);
        // the panel keeps one line per entry, so the sign's own line breaks
        // become separators instead of splitting the panel
        for (int side = 0; side < 2; ++side) {
            std::string text;
            for (char const ch : sign->getMessage(side == 0 ? SignTextSide::Front : SignTextSide::Back)) {
                if (ch == '\n' || ch == '\r') {
                    if (!text.empty()) {
                        // separator plus the colour the following line's text goes back
                        // to, since the sign's text is one run per side
                        text += " " + colored(Insight::cfg().colors.label, "|") + " "
                              + colorPrefix(Insight::cfg().colors.value);
                    }
                } else {
                    text += ch;
                }
            }
            if (!text.empty()) {
                lines.push_back(labelText(langCode, side == 0 ? "Text" : "Text (back)") + " " + valueText(text));
            }
        }
        return;
    }
    if (opt.lectern && be->getType() == BlockActorType::Lectern) {
        auto const* lectern = static_cast<LecternBlockActor const*>(be);
        if ((*lectern->mBook).isNull()) { // 26.40: was hasBook()
            lines.push_back(textLine(langCode, "Book", tr(langCode, "none")));
        } else {
            std::string name = firstItemName(be, langCode);
            lines.push_back(textLine(langCode, "Book", name.empty() ? tr(langCode, "has") : name));
            int         page     = static_cast<int>(lectern->mPage);       // 26.40: was getPage()
            int         total    = static_cast<int>(lectern->mTotalPages); // 26.40: was getTotalPages()
            std::string pageText = std::to_string(page + 1);
            if (total > 0) {
                pageText += "/" + std::to_string(total);
            }
            lines.push_back(textLine(langCode, "Page", pageText));
        }
        return;
    }
}

} // namespace

// Exported wrapper around the file-local containerOf(): the client's live-snapshot
// code needs the same lookup to recognise a screen's block container model.
Container const* blockContainerOf(BlockActor const* actor) { return containerOf(actor); }

std::string describeBlockFacing(IConstBlockSource const& region, BlockPos const& pos, std::string const& langCode) {
    if (auto v = readStateInt(region, pos, "facing_direction")) {
        return facingWord(langCode, *v);
    }
    if (auto v = readStateIntAny(region, pos, {"direction", "weirdo_direction"})) {
        return compassWord(langCode, *v);
    }
    // Doors, chests, barrels, ... use the namespaced string-enum state
    // "minecraft:cardinal_direction" (Direction::Type: South=0 West=1
    // North=2 East=3), which shares the compass mapping above.
    if (auto v = readStateIntAny(region, pos, {"minecraft:cardinal_direction", "cardinal_direction"})) {
        return compassWord(langCode, *v);
    }
    if (auto v = readStateIntAny(region, pos, {"ground_sign_direction", "rotation"})) {
        // 16-step rotation: 0 = south, 4 = west, 8 = north, 12 = east
        return compassWord(langCode, (*v / 4) % 4);
    }
    if (auto v = readStateIntAny(region, pos, {"pillar_axis", "axis"})) {
        switch (*v) {
        case 0:
            return tr(langCode, "x");
        case 1:
            return tr(langCode, "y");
        case 2:
            return tr(langCode, "z");
        default:
            return {};
        }
    }
    return {};
}

std::string describeEntityFacing(Actor const& actor, std::string const& langCode) {
    try {
        // Bedrock yaw: 0 = south (+Z), 90 = west (-X), 180 = north (-Z), 270 = east.
        // The same convention compassWord() already uses for block states.
        float const yaw  = actor.getRotation().x;
        int const   step = static_cast<int>(std::floor(yaw / 90.0f + 0.5f));
        return compassWord(langCode, step);
    } catch (...) {
        return {};
    }
}

std::string describeBlockLight(IConstBlockSource const& region, BlockPos const& pos) {
    try {
        float brightness = region.getBrightness(pos); // 0..1
        int   level      = static_cast<int>(brightness * 15.0f + 0.5f);
        if (level < 0) {
            level = 0;
        }
        if (level > 15) {
            level = 15;
        }
        return std::to_string(level);
    } catch (...) {
        return {};
    }
}

std::string describeBlockEmission(IConstBlockSource const& region, BlockPos const& pos) {
    try {
        auto const& block = region.getBlock(pos);
        auto const& type  = block.getBlockType();
        auto        value = type.getLightEmission(block); // virtual, 0..15
        int         level = static_cast<int>(value.mValue);
        if (level < 0) {
            level = 0;
        }
        if (level > 15) {
            level = 15;
        }
        return std::to_string(level);
    } catch (...) {
        return {};
    }
}

void setLiveContainerSnapshot(BlockPos const& pos, std::vector<ContainerItem> const& items, int size) {
    auto& snapshots = liveSnapshots();
    if (snapshots.size() > 512) {
        snapshots.clear(); // defensive: never grow without bound
    }
    snapshots[std::make_tuple(pos.x, pos.y, pos.z)] = ContainerSnapshot{items, size};
}

void setLiveEntityContainerSnapshot(int64_t actorId, std::vector<ContainerItem> const& items, int size) {
    auto& snapshots = liveEntitySnapshots();
    if (snapshots.size() > 512) {
        snapshots.clear();
    }
    snapshots[actorId] = ContainerSnapshot{items, size};
}

void resetRuntimeState() {
    // Everything here is keyed by a block position, an actor id or a pointer into the level's
    // memory - none of which is meaningful in another world. A reload reuses both addresses and
    // coordinates, so a stale entry would be read as the new world's own data: a chest at the
    // same coordinates showing the previous world's contents, cooking estimates walking on
    // items that no longer exist.
    liveSnapshots().clear();
    liveEntitySnapshots().clear();
    cookEstimates().clear();
    clearLoggedOnce();
}

std::string describeBlockStateNames(IConstBlockSource const& region, BlockPos const& pos) {
    try {
        auto const& block = region.getBlock(pos);
        auto const& type  = block.getBlockType();
        std::string out;
        for (auto const& [name, stateId] : *type.mStateNameMap) {
            auto value = block.getState<int>(stateId);
            if (!out.empty()) {
                out += ",";
            }
            out += name.getString();
            if (value) {
                out += "=" + std::to_string(*value);
            }
        }
        return out;
    } catch (...) {
        return {};
    }
}

std::string buildBlockExtras(
    IConstBlockSource const& region,
    BlockPos const&          pos,
    std::string const&       typeName,
    std::string const&       langCode,
    BlockExtrasConfig const& opt,
    ContainerContents*       contents
) {
    if (!opt.enabled) {
        return {};
    }
    std::vector<std::string> lines;

    // ---- data every block carries ----------------------------------------
    // Breaking time and explosion resistance come straight from the block type;
    // flame odds only exist for blocks with a flammable component.
    {
        auto const& block     = region.getBlock(pos);
        auto const& blockType = block.getBlockType();
        if (opt.hardness) {
            lines.push_back(textLine(langCode, "Breaking time", util::trimNumber(blockType.getDestroySpeed(), 2)));
        }
        if (opt.blastResistance) {
            lines.push_back(
                textLine(langCode, "Explosion resistance", util::trimNumber(blockType.getExplosionResistance(), 2))
            );
        }
        // TODO(26.40 + LeviLamina): "chance to catch fire" is not readable yet.
        // BlockType exposes no flame/burn/odds member, BlockProperty has no
        // flammability flag, and BlockFlammableComponent cannot be reached -
        // hasComponent<BlockFlammableComponent>() answers true but the typed
        // pointer lands on unrelated block-type floats (3.0 / 30.0 / 4.0 came back
        // as 16448 / 16880 / 16512), and that header even disagrees with
        // BlockFlammableDescription about the field width (2-byte FlameOdds vs
        // 4-byte int). Re-add the line and the extras.igniteChance switch once
        // something upstream makes the data reachable.
    }

    BlockActor const* be        = nullptr;
    bool const        wantActor = opt.chest || opt.bookshelf || opt.shelf || opt.lectern || opt.pot || opt.furnace
                               || opt.brewing || opt.sign || opt.banner || opt.itemFrame || opt.flowerPot || opt.jukebox
                               || opt.redstone || opt.comparator || opt.dispenser || opt.repeater || opt.candle
                               || opt.respawnAnchor || opt.piston || opt.misc;
    if (wantActor) {
        be = region.getBlockEntity(pos);
    }

    // A container's contents are the last line of the block panel, whatever else the block
    // reports: they are what a player looks at the container for, and on the client they are
    // a block of icons that would push every other line around from the middle. The text is
    // built here (it also fills `contents`), the line itself is appended at the very end.
    std::string chestText;
    if (opt.chest && be && containerOf(be) && isContainerBlock(region, pos)) {
        chestText = chestLine(be, pos, langCode, contents);
    }
    // machine slots: the block actor type says whether this is a furnace-like
    // block or a brewing stand (engine enum, not a list of block ids)
    if (opt.furnace && be && containerOf(be)
        && (be->getType() == BlockActorType::Furnace || be->getType() == BlockActorType::BlastFurnace
            || be->getType() == BlockActorType::Smoker)) {
        auto line = machineLine(be, pos, langCode);
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    if (opt.brewing && be && containerOf(be) && be->getType() == BlockActorType::BrewingStand) {
        auto line = machineLine(be, pos, langCode);
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    if (opt.redstone) {
        // Anything that carries a redstone level (wire, pressure plate, target,
        // repeater, ...) reports it as a block state, so no block id list is
        // needed; a comparator additionally has its own switch below.
        if (auto v = readStateIntAny(region, pos, {"output_signal", "redstone_signal"})) {
            lines.push_back(valueLine(langCode, "Signal", *v));
        }
    }
    if (opt.comparator && be && be->getType() == BlockActorType::Comparator) {
        auto line = comparatorLine(region, pos, be, langCode);
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    // block-state lines (misc is the master switch for them; the individual
    // switches below refine it)
    if (opt.misc) {
        miscStateLines(region, pos, typeName, langCode, lines, opt);
    }
    // block entities: blockActorLines() gates every branch by its own switch, so
    // this call has to happen whenever any of those switches is on. A switch left
    // out here silently takes the lines it owns down with it - turning banners off
    // used to silence the furnace readout and the campfire timers too.
    if (opt.banner || opt.pot || opt.shelf || opt.bookshelf || opt.lectern || opt.itemFrame || opt.sign || opt.furnace
        || opt.brewing || opt.misc) {
        blockActorLines(region, pos, typeName, langCode, be, lines, opt);
    }

    {
        // 26.40 uses BlockActorType::Jukebox for the block, older data says
        // Music; both are containers holding the disc in slot 0.
        if (opt.jukebox && be && (be->getType() == BlockActorType::Music || be->getType() == BlockActorType::Jukebox)) {
            if (auto* c = containerOf(be); c && c->getContainerSize() > 0) {
                auto const& item = c->getItem(0);
                if (item.isNull()) {
                    lines.push_back(
                        labelText(langCode, "Record") + " " + colored(Insight::cfg().colors.label, "-")
                    );
                } else {
                    // The engine carries one localization key per disc
                    // (item.record_<variant>.desc), so the disc's own name comes
                    // from the game's tables - including resource packs. Only the
                    // variant has to be worked out: modern items encode it in the
                    // raw id (minecraft:record_cat), legacy ones in the aux value.
                    std::string name = localizeKey(langCode, item.getDescriptionId());
                    auto const  raw  = item.getRawNameId();
                    // The item itself knows which disc it is (minecraft:record_cat);
                    // the stack-level raw id stays the generic "minecraft:record".
                    std::string serialized;
                    if (item.mItem) {
                        serialized = item.mItem->getSerializedName();
                    }
                    // Modern Bedrock names discs minecraft:music_disc_cat, older
                    // data uses minecraft:record_cat; the localization key is
                    // item.record_<variant>.desc in both cases.
                    static constexpr char const* kDiscPrefixes[] = {"minecraft:music_disc_", "minecraft:record_"};
                    std::string                  variant;
                    for (auto const* prefix : kDiscPrefixes) {
                        auto const length = std::strlen(prefix);
                        if (serialized.rfind(prefix, 0) == 0) {
                            variant = serialized.substr(length);
                            break;
                        }
                        if (raw.rfind(prefix, 0) == 0) {
                            variant = raw.substr(length);
                            break;
                        }
                    }
                    if (variant.empty()) {
                        if (int const aux = item.getAuxValue(); aux >= 0 && aux < 12) {
                            static constexpr char const* kLegacyDiscVariants[] = {
                                "13",
                                "cat",
                                "blocks",
                                "chirp",
                                "far",
                                "mall",
                                "mellohi",
                                "stal",
                                "strad",
                                "ward",
                                "11",
                                "wait"
                            };
                            variant = kLegacyDiscVariants[aux];
                        }
                    }
                    if (!variant.empty()) {
                        std::string const disc = localizeKey(langCode, "item.record_" + variant + ".desc");
                        if (disc.rfind("item.record_", 0) != 0) {
                            name = disc; // resolved by the engine
                        } else {
                            name += " (" + variant + ")"; // no entry for it here
                        }
                    }
                    logOnce(
                        Insight::getInstance().getSelf().getLogger(),
                        "record",
                        pos,
                        "id=" + item.getDescriptionId() + " raw=" + raw + " serialized=" + serialized
                            + " idAux=" + std::to_string(item.getIdAux()) + " aux=" + std::to_string(item.getAuxValue())
                            + " shown=" + name
                    );
                    lines.push_back(textLine(langCode, "Record", name));
                }
            }
        } else if (opt.flowerPot && be && be->getType() == BlockActorType::FlowerPot) {
            auto const* fp = static_cast<FlowerPotBlockActor const*>(be);
            // 26.40: getPlantItem() was replaced by the mPlant member.
            auto const* plant = static_cast<Block const*>(fp->mPlant);
            if (plant) {
                lines.push_back(textLine(langCode, "Pot", localizeKey(langCode, plant->getDescriptionId())));
            }
        } else if (opt.flowerPot && isPottedBlock(region, pos)) {
            // Modern versions encode the plant in the block itself. Bedrock
            // exposes no flag for "this block is a potted plant", so the block's
            // own display name (which is where the plant lives) is used - the
            // engine data, without parsing the id.
            lines.push_back(textLine(langCode, "Pot", localizeKey(langCode, region.getBlock(pos).getDescriptionId())));
        }
    }

    if (!chestText.empty()) {
        if (contents != nullptr) {
            contents->index = static_cast<int>(lines.size()); // last line, inside the extras block
        }
        lines.push_back(std::move(chestText));
    }

    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i) {
            out += '\n';
        }
        out += lines[i];
    }
    return out;
}

std::string buildEntityExtras(
    IConstBlockSource const&  region,
    Actor const&              actor,
    std::string const&        langCode,
    BlockExtrasConfig const&  opt,
    EntityExtrasConfig const& entityOpt,
    ContainerContents*        contents
) {
    // kept for signature symmetry with buildBlockExtras(); entity extras only
    // read actor components, so no block sampling happens here.
    (void)region;
    if (!opt.enabled) {
        return {};
    }
    std::vector<std::string> lines;

    // ---- container entities: whatever carries a container component ------
    // (chest/hopper minecart, boat with chest, ...). The component is the
    // engine data, so no list of entity ids is needed.
    // The container's contents go last here as well (see buildBlockExtras): built now, pushed
    // at the end of the extras.
    std::string                chestText;
    std::vector<ContainerItem> chestItems;
    if (opt.chest) {
        int                        size = 0;
        std::vector<ContainerItem> items;
        if (auto component = actor.getEntityContext().tryGetComponent<ContainerComponent>()) {
            // 26.40: the component no longer forwards the container interface
            // itself, it holds the FillingContainer that does.
            auto const& container = *component->mContainer;
            size                  = container.getContainerSize();
            items                 = readContainerItems(container);
        }
        // Entity containers have the same staleness problem as block ones: the
        // component keeps its old contents, while the container UI shows live
        // ones. Prefer the snapshot captured from the open UI.
        auto& snapshots = liveEntitySnapshots();
        auto  key       = static_cast<int64_t>(actor.getOrCreateUniqueID().rawID);
        if (auto it = snapshots.find(key); it != snapshots.end()) {
            auto const& snap = it->second;
            if (snap.size == size && snap.items == items) {
                snapshots.erase(it);
            } else {
                items = snap.items;
                size  = snap.size;
            }
        }
        if (size > 0) {
            int const filled = static_cast<int>(items.size());
            int const total  = totalCountOf(items);
            chestItems       = items;
            chestText =
                labelText(langCode, "Items") + " " + valueText(std::to_string(filled) + "/" + std::to_string(size));
            if (total > 0) {
                chestText += " " + colored(Insight::cfg().colors.label, "·") + " " + valueText(std::to_string(total));
            }
        }
    }

    // ---- paintings -------------------------------------------------------
    // A painting is an entity whose motive says which artwork it is; the motive
    // name is engine data, so no list of painting ids is involved.
    if (opt.painting && actor.isType(ActorType::Painting)) {
        auto const* painting = static_cast<Painting const*>(&actor);
        if (painting->mMotif) {
            auto const& name = *painting->mMotif->mName;
            if (!name.empty()) {
                lines.push_back(textLine(langCode, "Painting", name));
            }
        }
    }

    // ---- worn equipment --------------------------------------------------
    // The engine exposes the equipment slots themselves, so whatever actually
    // carries an item there reports it (armor stands, players, armored mobs) -
    // no entity id check is involved. Entities without equipment return empty
    // stacks and simply contribute nothing.
    if (opt.misc) {
        struct SlotLabel {
            SharedTypes::Legacy::ArmorSlot slot;
            char const*                    labelKey;
        };
        static constexpr SlotLabel slots[] = {
            {SharedTypes::Legacy::ArmorSlot::Head,  "Helmet"    },
            {SharedTypes::Legacy::ArmorSlot::Torso, "Chestplate"},
            {SharedTypes::Legacy::ArmorSlot::Legs,  "Leggings"  },
            {SharedTypes::Legacy::ArmorSlot::Feet,  "Boots"     },
        };
        try {
            for (auto const& s : slots) {
                auto const& item = actor.getArmor(s.slot);
                if (!item.isNull()) {
                    lines.push_back(textLine(langCode, s.labelKey, localizeKey(langCode, item.getDescriptionId())));
                }
            }
            auto const& mainHand = actor.getCarriedItem();
            if (!mainHand.isNull()) {
                lines.push_back(textLine(langCode, "Main hand", localizeKey(langCode, mainHand.getDescriptionId())));
            }
            auto const& offHand = actor.getOffhandSlot();
            if (!offHand.isNull()) {
                lines.push_back(textLine(langCode, "Off hand", localizeKey(langCode, offHand.getDescriptionId())));
            }
        } catch (...) {
            // actors without an equipment inventory simply report nothing
        }
    }

    // ---- {light}/{direction} are separate placeholders: they are not
    //      repeated here (see describeBlockLight / describeBlockFacing) ----

    // ---- tamed animal: who owns it ---------------------------------------
    // The engine resolves the owner id to the actor itself whenever that actor is
    // around, so no list of tameable types is involved. A player owner is asked for
    // its real name (a player carries no name tag), any other owner for its tag; an
    // owner that is not loaded at all contributes nothing rather than a placeholder.
    if (entityOpt.owner) {
        std::string name;
        if (auto const* owner = actor.getPlayerOwner(); owner != nullptr) {
            name = owner->getRealName();
        } else if (auto const* owner = actor.getOwner(); owner != nullptr) {
            name = owner->getNameTag();
        }
        if (!name.empty()) {
            lines.push_back(textLine(langCode, "Owner", name));
        }
    }

    // ---- active status effects (server side) ------------------------------
    // One line, like the vanilla effect list reads: every effect's own name, its level
    // and its remaining time, separated by a middot. Only a server has that list: the
    // engine's array is indexed by effect id and keeps a "no effect" placeholder (id 0)
    // in the slots that are not in use, which the loop filters out. A client is only
    // ever sent the latest effect of another creature, so this line is deliberately not
    // drawn there - one effect that then vanishes with it would be a lie, and guessing
    // the rest is not something a panel should do.
#ifndef INSIGHT_TARGET_CLIENT
    if (entityOpt.effects) {
        std::vector<std::string> values;
        for (auto const& effect : actor.getAllEffects()) {
            if (validEffectId(effect.mId)) {
                if (auto const value = effectValue(langCode, effect); !value.empty()) {
                    values.push_back(value);
                }
            }
        }
        if (!values.empty()) {
            int const   limit = entityOpt.effectsLimit;
            int const   total = static_cast<int>(values.size());
            int const   shown = limit > 0 && total > limit ? limit : total;
            std::string text;
            for (int i = 0; i < shown; ++i) {
                if (i) {
                    text += " " + colored(Insight::cfg().colors.label, "·") + " ";
                }
                text += valueText(values[i]);
            }
            if (total > shown) {
                text += " " + colored(Insight::cfg().colors.label, "·") + " "
                      + valueText("+" + std::to_string(total - shown));
            }
            lines.push_back(labelText(langCode, "Effects") + " " + text);
        }
    }
#endif

    // ---- breeding timer ---------------------------------------------------
    // Whichever timer is running: the love timer of an animal that was fed, or the
    // cooldown left after it bred. Those counters are server-side state, so a client
    // usually has no component to read at all - the "in love" flag is what it is told,
    // and it is checked on its own so that the line appears there too.
    if (entityOpt.breeding) {
        int ticks = 0;
        if (auto component = actor.getEntityContext().tryGetComponent<BreedableComponent>()) {
            ticks = component->mLoveTimer > 0 ? component->mLoveTimer : component->mBreedCooldown;
        }
        if (ticks > 0) {
            lines.push_back(textLine(langCode, "Breeding", clockText(ticks)));
        } else if (actor.getStatusFlag(ActorFlags::Inlove)) {
            lines.push_back(textLine(langCode, "Breeding", tr(langCode, "In love")));
        }
    }

    // ---- zombie villager: conversion time ---------------------------------
    // The countdown the game itself runs before it transforms the entity is what the
    // line is about, and it is a component of its own - the "converting" flag is not
    // set on every converting villager, and the old per-class field is zero on this
    // build, so the component is read first and the class is only the fallback.
    if (entityOpt.conversion) {
        int ticks = 0;
        if (auto component = actor.getEntityContext().tryGetComponent<TransformationComponent>()) {
            ticks = component->mDelayTicks;
        }
        if (ticks <= 0 && (actor.isType(ActorType::ZombieVillager) || actor.isType(ActorType::ZombieVillagerV2))) {
            ticks = static_cast<ZombieVillager const&>(actor).villagerConversionTime;
        }
        if (ticks > 0) {
            lines.push_back(textLine(langCode, "Converting", clockText(ticks)));
        } else if (actor.getStatusFlag(ActorFlags::Converting)) {
            lines.push_back(labelText(langCode, "Converting"));
        }
    }

    // ---- pet armour -------------------------------------------------------
    if (auto const value = petArmorValue(actor, langCode, entityOpt.petArmor); !value.empty()) {
        lines.push_back(textLine(langCode, "Pet armor", value));
    }

    // ---- mount numbers ----------------------------------------------------
    // A horse gets two lines: what it looks like, and its numbers. The panel's first
    // line already names the animal (Horse, Donkey, ...), so the coat line is about the
    // colour only. The raw attributes mean nothing on their own - they are "engine
    // units per tick" - so they are converted into what a player can judge: movement
    // speed x 42.16 is blocks per second, and the jump strength goes through the same
    // cubic the game's horse jump uses to become a height in blocks.
    if (entityOpt.horse && isHorse(actor)) {
        if (actor.isType(ActorType::Horse)) {
            if (auto const coat = horseCoatName(langCode, actor.getVariant()); !coat.empty()) {
                std::string text = labelText(langCode, "Coat") + " " + valueText(coat);
                if (auto const markings = horseMarkingsName(langCode, actor.getMarkVariant()); !markings.empty()) {
                    text += " " + colored(Insight::cfg().colors.label, "·") + " " + labelText(langCode, "Markings") + " "
                          + valueText(markings);
                }
                lines.push_back(text);
            }
        }
        std::string const speedUnit = tr(langCode, "blocks/s");
        std::string const heightUnit = tr(langCode, "blocks");
        std::string       text;
        if (auto const speed = attributeValue(actor, SharedAttributes::MOVEMENT_SPEED()); speed) {
            text = labelText(langCode, "Speed") + " "
                 + valueText(util::trimNumber(static_cast<double>(*speed) * 42.16, 2) + " " + speedUnit);
        }
        if (auto const jump = attributeValue(actor, SharedAttributes::JUMP_STRENGTH()); jump) {
            if (!text.empty()) {
                text += " " + colored(Insight::cfg().colors.label, "·") + " ";
            }
            text += labelText(langCode, "Jump") + " "
                  + valueText(util::trimNumber(jumpHeight(*jump), 2) + " " + heightUnit);
        }
        if (actor.hasSaddle()) {
            if (!text.empty()) {
                text += " " + colored(Insight::cfg().colors.label, "·") + " ";
            }
            text += valueText(tr(langCode, "Saddled"));
        }
        if (!text.empty()) {
            lines.push_back(text);
        }
    }

    // ---- armour stand pose ------------------------------------------------
    // The engine exposes the pose *index* (0 = the default pose), not the pose names
    // the game uses for them, so the index is what is shown - and only when it is not
    // the default, which is what every freshly placed stand has.
    if (entityOpt.armorStand && actor.isType(ActorType::ArmorStand)) {
        if (auto pose = actor.getEntityContext().tryGetComponent<ArmorStandPoseIndexComponent>();
            pose && pose->mValue != 0) {
            lines.push_back(textLine(langCode, "Pose", std::to_string(pose->mValue)));
        }
    }

    // ---- villager trade table ---------------------------------------------
    // Profession and level, both synced actor data: the client's own villager render
    // controller reads the same two values (query.variant / query.trade_tier). The V2
    // villager is the one whose profession *is* its variant; the old villager type is
    // left alone rather than labelled from the wrong table. The trade tier is counted
    // from zero, so the level a player reads in the trade screen is one more.
    if (entityOpt.villager && actor.isType(ActorType::VillagerV2)) {
        std::string text;
        if (auto const profession = villagerProfession(actor, langCode); !profession.empty()) {
            text = labelText(langCode, "Profession") + " " + valueText(profession);
        }
        if (auto const tier = syncedInt(actor, ActorDataIDs::TradeTier); tier && *tier >= 0) {
            if (!text.empty()) {
                text += " " + colored(Insight::cfg().colors.label, "·") + " ";
            }
            text += labelText(langCode, "Level") + " " + valueText(std::to_string(*tier + 1));
        }
        if (!text.empty()) {
            lines.push_back(text);
        }
    }

    // ---- copper golem: waxed ----------------------------------------------
    // The game declares this property as *not* client-synced, so a client can only ever
    // read its default value - showing "No" for a golem that is in fact waxed would be
    // worse than showing nothing, so this line belongs to the side that owns the state.
    // The value is the plain yes/no a player expects, not the on/off of a switch.
#ifndef INSIGHT_TARGET_CLIENT
    if (entityOpt.waxed && actor.isType(ActorType::CopperGolem)) {
        lines.push_back(textLine(langCode, "Waxed", tr(langCode, copperGolemWaxed(actor) ? "Yes" : "No")));
    }
#endif

    if (!chestText.empty()) {
        if (contents != nullptr) {
            contents->index = static_cast<int>(lines.size()); // last line, inside the extras block
            contents->items = std::move(chestItems);
        }
        lines.push_back(std::move(chestText));
    }

    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i) {
            out += '\n';
        }
        out += lines[i];
    }
    return out;
}

} // namespace insight
