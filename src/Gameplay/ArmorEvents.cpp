#include "Gameplay/ArmorEvents.h"

#include "Condition/ArmorWear/ArmorWear.h"
#include "Condition/Equipped.h"
#include "Core/TraceLog.h"
#include "Gameplay/ArmorRating.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>

namespace ArmorEvents
{
	namespace
	{
		using LIMB = RE::BGSBodyPartDefs::LIMB_ENUM;

		// The condition actor value of the limb a blow struck, or nothing for
		// a limb the target has no value for. The limb is an index into the
		// target's body part list, and the part names the actor value the game
		// cripples, which is also what a race maps a slot to, see LimbSlots.
		const RE::ActorValueInfo* LimbCondition(RE::Actor& a_target, LIMB a_limb)
		{
			const auto* parts = a_target.GetBodyPartData();
			const auto  index = static_cast<std::size_t>(a_limb);
			if (!parts || index >= std::size(parts->partArray)) {
				return nullptr;
			}
			const auto* part = parts->partArray[index];
			return part ? part->data.actorValue : nullptr;
		}

		// The biped slots the wearer's race maps to this limb, or to any limb
		// when a_condition is null. Bit 0 is slot 30, as in ArmorPiece::slots.
		// A person's race maps the helmet, eyes, torso, arm and leg slots, a
		// dog's its helmet and its back, a super mutant's no slot at all. The
		// game's own TESObjectARMO::Protects reads the piece's own race
		// instead, and the Welding Goggles name Dogmeat's race, whose map puts
		// the eyes on no limb.
		std::uint32_t LimbSlots(const RE::TESRace* a_race, const RE::ActorValueInfo* a_condition)
		{
			std::uint32_t slots = 0;
			if (!a_race) {
				return slots;
			}
			for (std::size_t slot = 0; slot < std::size(a_race->bipedObjectConditions); slot++) {
				const auto* limb = a_race->bipedObjectConditions[slot];
				if (limb && (!a_condition || limb == a_condition)) {
					slots |= 1U << slot;
				}
			}
			return slots;
		}

		// How hard a blow was before armor: what got through plus what the
		// armor stopped, for both kinds of damage. A blow is resisted by the
		// actor's armor as a whole, so every piece it reaches takes its share
		// of the whole.
		float Brought(const RE::HitData& a_hit)
		{
			return a_hit.totalDamage + a_hit.resistedTypedDamage;
		}

		// Wears the pieces every blow reaches.
		class HitSink : public RE::BSTEventSink<RE::TESHitEvent>
		{
		public:
			F4_HEAP_REDEFINE_NEW(HitSink);

		private:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent& a_event, RE::BSTEventSource<RE::TESHitEvent>*) override;
		};

		RE::BSEventNotifyControl HitSink::ProcessEvent(const RE::TESHitEvent& a_event, RE::BSTEventSource<RE::TESHitEvent>*)
		{
			// Only a blow with damage numbers says how hard it was. The game
			// sends the rest with none, a blast reaching a door for example.
			if (!a_event.usesHitData) {
				return RE::BSEventNotifyControl::kContinue;
			}

			const auto& hit = a_event.hitData;
			const auto  target = hit.target.get();
			if (!target) {
				return RE::BSEventNotifyControl::kContinue;
			}

			// A blow marked as a prediction is a question the AI asks, not a
			// blow that landed.
			if (hit.flags.any(RE::HitData::Flag::kPredictDamage, RE::HitData::Flag::kPredictBaseDamage)) {
				return RE::BSEventNotifyControl::kContinue;
			}

			const auto damage = Brought(hit);
			if (damage <= 0.0F) {
				return RE::BSEventNotifyControl::kContinue;
			}

			// An essential NPC's own outfit from the editor never wears, since
			// some of it can never be handed to the player to repair, Strong's
			// leg guards for example. What the player hands them wears as it
			// would on anyone. The game marks an NPC essential by its own
			// record, a script or a role in a quest such as a companion's. It
			// marks the player too during a fist fight, and the player's armor
			// wears all the same.
			const bool theirs = target.get() != RE::PlayerCharacter::GetSingleton();
			const bool keeps = theirs && target->boolFlags.all(RE::Actor::BOOL_FLAGS::kEssential);

			// Where it was struck. A blow that names no limb, a blast or some
			// bullets on a super mutant, reaches every piece on a limb. A blow
			// that struck the held weapon reached no armor, and neither did one
			// on a limb the target has no condition value for.
			const auto limb = static_cast<LIMB>(hit.damageLimb.underlying());
			if (limb == LIMB::kWeapon) {
				return RE::BSEventNotifyControl::kContinue;
			}
			const auto* condition = limb == LIMB::kNone ? nullptr : LimbCondition(*target, limb);
			if (limb != LIMB::kNone && !condition) {
				return RE::BSEventNotifyControl::kContinue;
			}

			// Whose armor: every piece on, or none for an actor in power armor.
			const auto pieces = Equipped::ArmorPieces(target.get());
			if (pieces.empty()) {
				return RE::BSEventNotifyControl::kContinue;
			}

			// One block per blow, in the log of whoever was struck.
			const auto where = condition ? RE::TESFullName::GetFullName(*condition) : "the whole body"sv;
			TraceLog::For(theirs).Begin("ARMOR", "{:s} struck on {:s} for {:.2f}", TraceLog::Who{ target.get() }, where, damage);

			// The pieces on the limb struck, every piece on a limb for a blow
			// that names none, and the pieces on no limb, which cover the whole
			// body. A blow lands on 1 spot, so the pieces on no limb split it:
			// a super mutant's 3 each take a third, and a lone visor takes it all.
			// A piece an essential NPC keeps still takes its share, and keeps
			// its condition.
			const auto onALimb = LimbSlots(target->race, nullptr);
			const auto struck = condition ? LimbSlots(target->race, condition) : onALimb;
			const auto sharing = std::ranges::count_if(pieces, [onALimb](const auto& a_piece) { return (a_piece.slots & onALimb) == 0; });
			bool       moved = false;
			for (const auto& piece : pieces) {
				const bool onNoLimb = (piece.slots & onALimb) == 0;
				if (!onNoLimb && (piece.slots & struck) == 0) {
					continue;
				}
				if (keeps && piece.issued) {
					TraceLog::For(theirs).Line("wear", "{:s} [{:08X}] is their own and they are essential, so no wear",
						RE::TESFullName::GetFullName(*piece.armor), piece.armor->formID);
					continue;
				}
				const auto split = onNoLimb ? sharing : 1;
				moved = ArmorWear::Wear(*target, *piece.armor, damage / static_cast<float>(split), split > 1 ? "shared hit" : "hit", theirs) || moved;
			}
			if (!moved) {
				return RE::BSEventNotifyControl::kContinue;
			}

			// The worn pieces protect less from the next blow on. The engine
			// has released the inventory lock by here. The player's Pip-Boy
			// lists each piece that wore again by itself, see ItemCards.h.
			ArmorRating::Refresh(*target);
			return RE::BSEventNotifyControl::kContinue;
		}
	}

	void Load()
	{
		static bool registered = false;
		if (registered) {
			return;
		}

		auto* source = RE::TESHitEvent::GetEventSource();
		if (!source) {
			REX::ERROR("No hit event source, so armor will not wear down.");
			return;
		}
		source->RegisterSink(new HitSink());
		registered = true;
		REX::INFO("Armor wears from every blow that lands, on anybody, by the limb the blow struck.");
	}
}
