#include "UI/Inventory/ItemCard/Cards.h"

#include "Condition/Condition.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/ItemCards.h"
#include "Core/Settings/Settings.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Inventory/ItemCard/ItemCard.h"
#include "UI/Inventory/Pipboy.h"

#include <atomic>
#include <cstdint>
#include <optional>
#include <string_view>

// The marks right of the CND row that compare an item's wear rate with the
// equipped item's, fed through vanilla's own compare members while the card
// shows the rate. Cards outside the Pip-Boy get them in Hooks.cpp from the
// pick below. The Pip-Boy compares its rows in a pass of its own after every
// build and on every equip, which never touches CND, so 3 of its calls are
// patched here.
namespace ItemCard
{
	namespace
	{
		using CallPatch::AE;
		using CallPatch::CallSite;
		using CallPatch::OG;

		// -------------------------------------------------------------------
		// The Pip-Boy's CND entry
		// -------------------------------------------------------------------

		using Text = RE::PipboyPrimitiveValue<RE::BSFixedStringCS>;
		using Float = RE::PipboyPrimitiveValue<float>;
		using Int = RE::PipboyPrimitiveValue<std::int32_t>;

		// Whether a_value is a T, told by its vtable as PowerArmor.cpp tells
		// a text. The address is looked up once, since the pass asks for
		// every entry of every row under the Pip-Boy's lock.
		template <class T>
		bool Is(const RE::PipboyValue* a_value)
		{
			static const auto vtable = T::VTABLE[0].address();
			return a_value && *reinterpret_cast<const std::uintptr_t*>(a_value) == vtable;
		}

		// a_object's member a_name when it is the kind vanilla's builder
		// makes, else nullptr, so a value of another mod is left alone.
		template <class T>
		T* MemberOf(RE::PipboyObject& a_object, const RE::BSFixedString& a_name)
		{
			auto* member = a_object.GetMember<RE::PipboyValue*>(a_name);
			return Is<T>(member) ? static_cast<T*>(member) : nullptr;
		}

		// The member names, made once as the engine keeps its own keys.
		struct Keys
		{
			RE::BSFixedString entries{ "itemCardInfoList" };
			RE::BSFixedString text{ "text" };
			RE::BSFixedString difference{ "difference" };
			RE::BSFixedString rating{ "diffRating" };
			RE::BSFixedString precision{ "precision" };
		};

		const Keys& Names()
		{
			static const Keys keys;
			return keys;
		}

		// The CND entry of a_row's card, found by its text as vanilla finds
		// $wt. Nothing on a card with no CND row.
		RE::PipboyObject* CndEntry(RE::PipboyObject& a_row)
		{
			auto* entries = MemberOf<RE::PipboyArray>(a_row, Names().entries);
			if (!entries) {
				return nullptr;
			}
			for (auto* value : entries->elements) {
				if (!Is<RE::PipboyObject>(value)) {
					continue;
				}
				auto*       entry = static_cast<RE::PipboyObject*>(value);
				const auto* text = MemberOf<Text>(*entry, Names().text);
				if (text && std::string_view{ text->m_value } == CND_TEXT) {
					return entry;
				}
			}
			return nullptr;
		}

		// Writes the change and its marks into a_entry in place, as vanilla
		// writes its own rows in this pass, and marks a value for sending
		// only when it changed. A weapon row is written once a pass, so an
		// equip resends none that stayed the same. An armor row is cleared
		// and written again in every pass, as vanilla does its resistance
		// rows, so it is resent each time. A change gets precision 1, as at
		// the workbench, since vanilla draws a change only from 1 step of the
		// precision up.
		void WriteChange(RE::PipboyObject& a_entry, float a_change, std::int32_t a_rating)
		{
			auto* difference = MemberOf<Float>(a_entry, Names().difference);
			auto* rating = MemberOf<Int>(a_entry, Names().rating);
			if (!difference || !rating) {
				return;
			}
			if (difference->m_value != a_change) {
				difference->m_value = a_change;
				difference->SetDirtyUpChain();
			}
			if (rating->m_value != a_rating) {
				rating->m_value = a_rating;
				rating->SetDirtyUpChain();
			}
			const auto& precision = Names().precision;
			if (a_change != 0.0F && !a_entry.GetMember<RE::PipboyValue*>(precision)) {
				a_entry.AddMember(&precision, new RE::PipboyPrimitiveValue<std::uint32_t>(1, &a_entry));
			}
		}

		// Whether a row carries a change NEC fed since the cards were last
		// built anew, see SettleCompare.
		std::atomic<bool> g_fed{ false };

		// Feeds a_row's CND entry the change against a_compared, the row of
		// the equipped item the pass compares it with, nullptr for none. A
		// row with no rate of NEC's own is left as built: a power armor
		// piece, or a row built while another mod held the card calls.
		// Written on every pass, so a change goes when its compared item does,
		// and while NEC does not feed the pass a hook gives nullptr, which
		// clears what an earlier pass fed.
		void FeedCnd(RE::PipboyObject& a_row, const RE::PipboyObject* a_compared)
		{
			const auto rate = Pipboy::GearWearRate(a_row.m_id);
			if (!rate) {
				return;
			}
			const auto own = a_compared ? Pipboy::GearWearRate(a_compared->m_id) : std::nullopt;
			Pipboy::SetComparedWearRate(a_row.m_id, own);
			auto* entry = CndEntry(a_row);
			if (!entry) {
				return;
			}

			// The rate is drawn only while its switch is on, see Raise.cpp,
			// so with it off the entry is fed nothing. The highlighted
			// equipped item compares with itself and gets nothing too.
			const auto change = own && Settings::bCardWearRate.GetValue() ? WearRateChange(*own, *rate) : 0.0F;
			std::int32_t rating = 0;
			if (change != 0.0F) {
				rating = MARKS_SAY_BETTER ? RE::PipboyInventoryUtils::GetDiffRating(*own, *rate) : RE::PipboyInventoryUtils::GetDiffRating(*rate, *own);
			}
			WriteChange(*entry, change, rating);
			if (change != 0.0F) {
				g_fed.store(true);
				TraceLog::First("menu", "Pip-Boy entry {:d} CND wears at {:.3f} against {:.3f}, difference {:g}, rating {:d}", a_row.m_id, *rate, *own,
					change, rating);
			}
		}

		// -------------------------------------------------------------------
		// The compare pass
		// -------------------------------------------------------------------

		// The calls in PipboyInventoryData::UpdateCompareDiffs, which the game
		// runs on each row of a category after building it, on every row of it
		// again after an equip and on each row it builds anew. Each callee has
		// this 1 caller. For a weapon the pass passes the equipped weapon it
		// picked to the damage compare, for armor it clears the resistances
		// and then takes off each worn piece that shares a slot.
		constexpr CallSite COMPARE_SITES[] = {
			{ AE{ RE::ID::PipboyInventoryData::UpdateCompareDiffs.ae(), 0x278 }, OG{ RE::ID::PipboyInventoryData::UpdateCompareDiffs.og(), 0x270 },
				"pipboy compare weapon" },
			{ AE{ RE::ID::PipboyInventoryData::UpdateCompareDiffs.ae(), 0x285 }, OG{ RE::ID::PipboyInventoryData::UpdateCompareDiffs.og(), 0x27D },
				"pipboy compare armor" },
			{ AE{ RE::ID::PipboyInventoryData::UpdateCompareDiffs.ae(), 0x320 }, OG{ RE::ID::PipboyInventoryData::UpdateCompareDiffs.og(), 0x317 },
				"pipboy compare worn piece" },
		};

		using Damage_t = decltype(&RE::PipboyInventoryData::UpdateDamageDiffs);
		using Reset_t = decltype(&RE::PipboyInventoryData::ResetResistDiffs);
		using Subtract_t = decltype(&RE::PipboyInventoryData::SubtractResistDiffs);

		CallPatch::Link<Damage_t>   g_damageLink;
		CallPatch::Link<Reset_t>    g_resetLink;
		CallPatch::Link<Subtract_t> g_subtractLink;

		// The 3 calls, which go in together.
		CallPatch::Held g_compare;

		// The armor row the pass compares on this thread, and how many worn
		// pieces share a slot with it so far.
		thread_local const RE::PipboyObject* t_armorRow = nullptr;
		thread_local std::uint32_t           t_wornPieces = 0;

		// Whether NEC feeds the pass now. The rates come from the Pip-Boy
		// card calls, so they must hold too.
		bool Feeds(const CallPatch::LinkBase& a_link)
		{
			return g_compare.Runs(a_link) && PipboyCards();
		}

		void DamageDiffsHk(RE::PipboyObject* a_item, RE::PipboyObject* a_compared)
		{
			const auto feeds = Feeds(g_damageLink);
			g_damageLink(a_item, a_compared);
			if (a_item) {
				FeedCnd(*a_item, feeds ? a_compared : nullptr);
			}
		}

		// Runs first for each armor row, so it starts the count and clears
		// the change for a row that shares a slot with no worn piece. It
		// asks Runs only so a set that waits sees this hook run.
		void ResetResistsHk(RE::PipboyObject* a_item)
		{
			static_cast<void>(g_compare.Runs(g_resetLink));
			g_resetLink(a_item);
			t_armorRow = a_item;
			t_wornPieces = 0;
			if (a_item) {
				FeedCnd(*a_item, nullptr);
			}
		}

		// The 1st worn piece gives the change. A 2nd takes it away again,
		// since the item would replace both.
		void SubtractResistsHk(RE::PipboyObject* a_item, RE::PipboyObject* a_worn)
		{
			const auto feeds = Feeds(g_subtractLink);
			g_subtractLink(a_item, a_worn);
			if (!a_item || a_item != t_armorRow) {
				return;
			}
			t_wornPieces++;
			FeedCnd(*a_item, feeds && t_wornPieces == 1 ? a_worn : nullptr);
		}
	}

	// -------------------------------------------------------------------
	// The item compared
	// -------------------------------------------------------------------

	// The same picks as InventoryUserUIUtils::PopulateItemCardInfo_Helper
	// makes for Damage and the resistances. A trader's item is compared with
	// the player's too, as vanilla does.
	std::optional<float> ComparedWearRate(const RE::BGSInventoryItem& a_item, const CompareItems& a_compareItems)
	{
		const auto* object = a_item.object;
		if (!object || !Condition::WearsOut(*object)) {
			return std::nullopt;
		}

		const RE::BGSInventoryItem* picked = nullptr;
		std::uint32_t               stackID = 0;
		if (const auto* weapon = object->As<RE::TESObjectWEAP>()) {
			for (const auto& [item, id] : a_compareItems) {
				const auto* other = item && item->object ? item->object->As<RE::TESObjectWEAP>() : nullptr;
				if (other && other->IsThrownWeapon() == weapon->IsThrownWeapon()) {
					picked = item;
					stackID = id;
				}
			}
		} else if (const auto* armor = object->As<RE::TESObjectARMO>()) {
			// The engine's slot test reads the same mask, see
			// PipboyInventoryUtils::DoSlotsOverlap. An item that would
			// replace 2 worn pieces has no 1 rate to compare with.
			const auto  slots = armor->bipedModelData.bipedObjectSlots;
			std::size_t shared = 0;
			for (const auto& [item, id] : a_compareItems) {
				const auto* other = item && item->object ? item->object->As<RE::TESObjectARMO>() : nullptr;
				if (other && (other->bipedModelData.bipedObjectSlots & slots) != 0) {
					picked = item;
					stackID = id;
					shared++;
				}
			}
			if (shared != 1) {
				return std::nullopt;
			}
		}

		if (!picked || !picked->object || !Condition::WearsOut(*picked->object)) {
			return std::nullopt;
		}
		return WearRateOf(*picked, picked->GetStackByID(stackID));
	}

	// The rebuild goes in a task, since a caller may hold the Pip-Boy's
	// lock, and it puts a fresh card on every row, whose CND has no change.
	void SettleCompare()
	{
		if ((g_compare.Intact() && PipboyCards()) || !g_fed.exchange(false)) {
			return;
		}
		if (const auto* tasks = F4SE::GetTaskInterface()) {
			tasks->AddTask([] {
				ItemCards::Refresh(RE::ENUM_FORM_ID::kWEAP);
				ItemCards::Refresh(RE::ENUM_FORM_ID::kARMO);
			});
		}
	}

	void PatchCompare()
	{
		g_compare = CallPatch::PatchTogether({
			{ COMPARE_SITES[0], RE::ID::PipboyInventoryData::UpdateDamageDiffs, reinterpret_cast<std::uintptr_t>(&DamageDiffsHk), &g_damageLink },
			{ COMPARE_SITES[1], RE::ID::PipboyInventoryData::ResetResistDiffs, reinterpret_cast<std::uintptr_t>(&ResetResistsHk), &g_resetLink },
			{ COMPARE_SITES[2], RE::ID::PipboyInventoryData::SubtractResistDiffs, reinterpret_cast<std::uintptr_t>(&SubtractResistsHk), &g_subtractLink },
		});
		if (g_compare) {
			REX::INFO("Pip-Boy item cards compare the wear rate on the CND row with the equipped item's.");
		}
	}
}
