#include "Gameplay/Jam.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <format>
#include <random>
#include <string>

namespace Jam
{
	namespace
	{
		// "Jammed.", a line of dialogue in Fallout4.esm about a door, the only
		// text in the game about something jamming, translated into every
		// language. The fallback covers a strings file without it.
		constexpr std::uint32_t JAMMED_TEXT_ID = 0x19A89;
		constexpr const char*   JAMMED_TEXT_FALLBACK = "Jammed.";

		// fWeaponConditionReloadJam1 to 10.
		constexpr std::size_t NUM_TENTHS = 10;

		std::array<RE::Setting*, NUM_TENTHS> g_chancePerMagazine{};
		std::string                          g_message{ JAMMED_TEXT_FALLBACK };

		// The gun whose last reload jammed, so its next reload loads for sure.
		// Atomic, since animation events arrive on more than one thread.
		std::atomic<const RE::TESObjectWEAP*> g_retryLoads{ nullptr };

		float ChancePerMagazine(float a_health)
		{
			// Worst first, so a gun at 45% reads the 5th.
			const auto tenth = std::clamp(static_cast<int>(a_health * 10.0F), 0, static_cast<int>(NUM_TENTHS) - 1);
			const auto setting = g_chancePerMagazine[static_cast<std::size_t>(tenth)];
			return setting ? std::clamp(setting->GetFloat(), 0.0F, 1.0F) : 0.0F;
		}

		// The chance that the next shot jams, for a magazine of a_capacity that
		// jams with a_perMagazine somewhere in it, with a_loaded left. Each
		// round holds an equal share, and a round that fired cannot have held
		// the jam, so the rounds left split what remains:
		//
		//   perMagazine / (capacity - fired * perMagazine)
		//
		// At a_perMagazine of 1 this is 1 / loaded, the odds of drawing the
		// marked card from what is left of a deck.
		double ChancePerShot(double a_perMagazine, std::uint32_t a_capacity, std::uint32_t a_loaded)
		{
			// A count above the capacity counts as a full magazine.
			const auto loaded = std::min(a_loaded, a_capacity);
			const auto fired = static_cast<double>(a_capacity - loaded);
			return a_perMagazine / (static_cast<double>(a_capacity) - fired * a_perMagazine);
		}

		// The copy's own stats where it has any, since mods change the magazine
		// size.
		const RE::TESObjectWEAP::InstanceData& StatsOf(const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon, const RE::TESObjectWEAP& a_base)
		{
			if (a_weapon.instanceData) {
				return *static_cast<const RE::TESObjectWEAP::InstanceData*>(a_weapon.instanceData.get());
			}
			return a_base.weaponData;
		}

		// A gun fires once per reload when its magazine holds 1 round, or when
		// one shot spends all of it, the Charging Reload flag: ReloadWeapon
		// adds one charge per reload and UseAmmo takes every charge with the
		// next shot.
		bool FiresOncePerReload(const RE::TESObjectWEAP::InstanceData& a_stats)
		{
			return a_stats.ammoCapacity == 1 || a_stats.flags.any(RE::WEAPON_FLAGS::kChargingReload);
		}

		// A generator per rolling thread, as in SpawnCondition/Band.cpp.
		bool RollChance(double a_chance)
		{
			static thread_local std::mt19937 engine{ std::random_device{}() };
			return std::bernoulli_distribution{ a_chance }(engine);
		}

		// Empties a magazine the way firing its last round does, the 3 steps
		// Actor::UseAmmo takes at 0. The last lets go of the trigger, which
		// stops an automatic weapon.
		void EmptyMagazine(RE::Actor& a_actor, RE::BGSEquipIndex a_index)
		{
			a_actor.SetCurrentAmmoCount(a_index, 0);

			// The body seen from outside and the arms in first person each hold
			// a model of the gun with its own animation graph, and
			// iWeaponCharge is how many rounds it shows loaded. By reference:
			// copying the smart pointer would compile the code that frees a
			// biped, which reaches types CommonLibF4 only declares.
			const auto& thirdPerson = a_actor.GetBiped(false);
			const auto& firstPerson = a_actor.GetBiped(true);

			const RE::BSFixedString weaponCharge{ "iWeaponCharge" };
			if (thirdPerson) {
				thirdPerson->SetObjectGraphVariableInt(RE::BIPED_OBJECT::kWeaponGun, weaponCharge, 0);
			}
			if (firstPerson && firstPerson != thirdPerson) {
				firstPerson->SetObjectGraphVariableInt(RE::BIPED_OBJECT::kWeaponGun, weaponCharge, 0);
			}

			a_actor.OnMagazineEmpty(a_index);
		}

		// What every jam does: the magazine empties, the gun clicks as with no
		// ammo, and the HUD says it jammed.
		void JamGun(RE::Actor& a_actor, RE::BGSEquipIndex a_index, const RE::TESObjectWEAP::InstanceData& a_stats)
		{
			EmptyMagazine(a_actor, a_index);

			// The click of a trigger pulled with no ammo, as the player's
			// controls play it.
			if (a_stats.attackFailSound) {
				RE::BGSAudio::PlaySoundDescriptor(a_stats.attackFailSound, 0, nullptr, nullptr);
			}

			// Throttled, so the message does not stack up in the corner.
			RE::SendHUDMessage::ShowHUDMessage(g_message.c_str(), nullptr, true, false);

			// What the next reload starts from.
			if (TraceLog::IsOpen()) {
				TraceLog::Line("jam", "the magazine holds {:d} after the jam", a_actor.GetCurrentAmmoCount(a_index));
			}
		}

		// How many rounds the weapon in one of an actor's equip slots holds.
		std::uint32_t LoadedRounds(const RE::Actor& a_actor, std::uint32_t a_equipIndex)
		{
			return a_actor.GetCurrentAmmoCount(RE::BGSEquipIndex{ a_equipIndex });
		}

		// Rolls for a jam as a reload of the player's gun finishes. Only guns
		// that fire once per reload roll here. a_loadedBefore is how many
		// rounds the magazine held before.
		void RollReload(RE::PlayerCharacter& a_player, const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon, std::uint32_t a_equipIndex,
			std::uint32_t a_loadedBefore)
		{
			if (!Settings::bJam.GetValue()) {
				return;
			}

			auto* object = a_weapon.object;
			if (!object || !object->IsWeapon()) {
				return;
			}
			auto&       weapon = static_cast<RE::TESObjectWEAP&>(*object);
			const auto& stats = StatsOf(a_weapon, weapon);
			if (!FiresOncePerReload(stats)) {
				return;
			}

			// Only the reload that puts the first round in rolls. A Laser Musket
			// finishes a reload per charge cranked in.
			const auto loaded = LoadedRounds(a_player, a_equipIndex);
			TraceLog::Begin("RELOAD", "{:s} [{:08X}] from {:d} to {:d} loaded", RE::TESFullName::GetFullName(weapon), weapon.formID,
				a_loadedBefore, loaded);
			if (a_loadedBefore != 0 || loaded == 0) {
				TraceLog::Line("jam", "no roll, only a reload that loads an empty gun rolls");
				return;
			}

			// The reload after a jam always loads. Under 10% every reload jams, and
			// this keeps such guns firing at all.
			const RE::TESObjectWEAP* retry = &weapon;
			if (g_retryLoads.compare_exchange_strong(retry, nullptr)) {
				TraceLog::Line("jam", "clear, the reload after a jam always loads");
				return;
			}

			// A gun that does not wear has no condition to jam from.
			const auto health = Equipped::WeaponHealth(&a_player, &weapon);
			if (health < 0.0F) {
				TraceLog::Line("jam", "clear, it does not wear");
				return;
			}

			// The magazine is 1 round, so the magazine's chance is this reload's.
			const auto perMagazine = ChancePerMagazine(health);
			if (perMagazine <= 0.0F) {
				TraceLog::Line("jam", "clear, no chance at condition {:.3f}", health);
				return;
			}
			const bool jammed = RollChance(perMagazine);

			TraceLog::Line("jam", "{:s} at {:.2f}% on the reload, condition {:.3f}",
				jammed ? "JAMMED" : "clear", perMagazine * 100.0, health);
			if (jammed) {
				g_retryLoads = &weapon;
				JamGun(a_player, RE::BGSEquipIndex{ a_equipIndex }, stats);
			}
		}

		// The one call to Actor::ReloadWeapon inside
		// ReloadCompleteHandler::Handle, the function the reloadComplete
		// animation event runs, through the actor's vtable slot 0xEF.
		constexpr CallPatch::CallSite RELOAD_SITE{ 2235362, 0xAE, "reload" };
		constexpr std::size_t         RELOAD_WEAPON_SLOT = 0xEF;

		// Set when NEC runs on top of a mod's hook at this vtable call, so the
		// reload goes on to that mod. Empty for the game's own, which the hook
		// reaches through the actor's table itself.
		CallPatch::Link<bool(RE::Actor*, const RE::BGSObjectInstanceT<RE::TESObjectWEAP>&, std::uint32_t)> g_reloadLink;

		// Stands in for the actor's ReloadWeapon when the reloadComplete
		// animation event finishes a reload. A gun that fires once per reload
		// can jam here.
		bool ReloadHk(RE::Actor* a_actor, const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon, std::uint32_t a_equipIndex)
		{
			// For every actor, so only the player's guns that wear are watched.
			auto*      object = a_weapon.object;
			auto*      player = RE::PlayerCharacter::GetSingleton();
			const bool watched = player && a_actor == player && object && object->IsWeapon() && g_reloadLink.Live();
			const auto before = watched ? LoadedRounds(*a_actor, a_equipIndex) : 0;

			// On to the mod NEC runs on top of, or through the actor's vtable,
			// as the call this replaced, so the player's own ReloadWeapon runs
			// for the player.
			const bool loaded = g_reloadLink ? g_reloadLink(a_actor, a_weapon, a_equipIndex) :
			                                   a_actor->ReloadWeapon(a_weapon, RE::BGSEquipIndex{ a_equipIndex });

			if (watched) {
				auto& weapon = static_cast<RE::TESObjectWEAP&>(*object);
				if (!Condition::WhyNoCondition(weapon) && weapon.weaponData.type.get() == RE::WEAPON_TYPE::kGun) {
					RollReload(*player, a_weapon, a_equipIndex, before);
				}
			}
			return loaded;
		}
	}

	void Install()
	{
		if (CallPatch::PatchVirtualCall(RELOAD_SITE, RELOAD_WEAPON_SLOT, reinterpret_cast<std::uintptr_t>(&ReloadHk), g_reloadLink, Part::kReloadJam)) {
			REX::INFO("Guns that fire once per reload can jam as the reload finishes.");
		} else {
			REX::ERROR("Guns that fire once per reload will not jam.");
		}
	}

	void Unload()
	{
		// The settings are built into the game, not read from a file, so they
		// are not deleted with the forms.
		g_retryLoads = nullptr;
	}

	void Load()
	{
		auto* settings = RE::GameSettingCollection::GetSingleton();
		for (std::size_t i = 0; i < NUM_TENTHS; i++) {
			const auto name = std::format("fWeaponConditionReloadJam{:d}", i + 1);
			g_chancePerMagazine[i] = settings ? settings->GetSetting(name) : nullptr;
			if (!g_chancePerMagazine[i]) {
				REX::ERROR("{:s} is missing, guns in that tenth of condition will not jam.", name);
			}
		}

		// Read as game data loads, so a shot never waits on the strings file.
		const auto* master = g_dataHandler ? g_dataHandler->LookupModByName("Fallout4.esm") : nullptr;
		if (master) {
			RE::BSFixedStringCS text;
			RE::BGSLocalizedStringIL::LookupByID(text, master, JAMMED_TEXT_ID);
			if (!text.empty()) {
				g_message = text.c_str();
			}
		}

		REX::INFO("Worn guns can jam. The message reads \"{:s}\".", g_message);
	}

	bool Roll(RE::PlayerCharacter& a_player, const RE::BGSObjectInstanceT<RE::TESObjectWEAP>& a_weapon, std::uint32_t a_equipIndex)
	{
		// Switched off, nothing rolls. The fire hook belongs to WeaponEvents
		// and calls in here whatever the switch says.
		if (!Settings::bJam.GetValue()) {
			return false;
		}

		auto* object = a_weapon.object;
		if (!object || !object->IsWeapon()) {
			return false;
		}
		auto&       weapon = static_cast<RE::TESObjectWEAP&>(*object);
		const auto& stats = StatsOf(a_weapon, weapon);

		// With only one shot to give, such a gun jams on the reload instead.
		if (FiresOncePerReload(stats)) {
			return false;
		}

		const RE::BGSEquipIndex index{ a_equipIndex };

		// A gun with no magazine has nothing to jam, and neither has one whose
		// magazine is already empty, which the engine is about to reload.
		if (stats.ammoCapacity == 0 || !a_player.GetCurrentAmmo(index)) {
			return false;
		}
		const auto loaded = LoadedRounds(a_player, a_equipIndex);
		if (loaded == 0) {
			return false;
		}

		const auto health = Equipped::WeaponHealth(&a_player, &weapon);
		if (health < 0.0F) {
			TraceLog::Line("jam", "clear, it does not wear");
			return false;
		}

		// A roll that cannot jam is still logged, or a gun in good condition
		// would look like one the roll never reached.
		const auto perMagazine = ChancePerMagazine(health);
		if (perMagazine <= 0.0F) {
			TraceLog::Line("jam", "clear, no chance at condition {:.3f}", health);
			return false;
		}
		const auto capacity = static_cast<std::uint32_t>(stats.ammoCapacity);
		const auto perShot = ChancePerShot(perMagazine, capacity, loaded);
		const bool jammed = RollChance(perShot);

		// Every roll goes to the trace log, so the chance can be watched
		// climbing as a magazine empties.
		TraceLog::Line("jam", "{:s} at {:.2f}% with {:d} of {:d} loaded, {:.2f} a magazine at condition {:.3f}",
			jammed ? "JAMMED" : "clear", perShot * 100.0, loaded, capacity, perMagazine, health);
		if (jammed) {
			JamGun(a_player, index, stats);
		}
		return jammed;
	}

}
