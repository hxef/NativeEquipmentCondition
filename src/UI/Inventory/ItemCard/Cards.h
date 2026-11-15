#pragma once

#include "Core/Plugin.h"

#include <cstdint>
#include <optional>
#include <string_view>

// What the files of this feature share. Private to this folder.
namespace ItemCard
{
	// The vanilla translation key for condition, CND in English, so the row
	// reads right in every language.
	inline constexpr const char* CND_TEXT = "$ItemInfo_CND";

	// The equipped items a card is compared with, each with its stack.
	using CompareItems = RE::BSScrapArray<RE::BSTTuple<const RE::BGSInventoryItem*, std::uint32_t>>;

	// The card the game is building on this thread, set while the vanilla
	// builder runs: the condition of the item on it, and the equipped
	// items it is compared with, none for the Pip-Boy.
	struct Building
	{
		float               health;
		const CompareItems* compare;
	};

	inline thread_local const Building* t_building = nullptr;

	// The food on the card the game is building on this thread, set while
	// its builder runs, so the rows of its effects show what eating it gives,
	// see Effects.cpp. Set on every card, with food nullptr for anything but
	// spoiled food, so a card never reads what the player ate.
	struct FoodCard
	{
		const RE::MagicItem* food = nullptr;
		float                share = 1.0F;       // of the strength of each effect that is not harmful
		float                rads = 0.0F;        // more than the food's own
		bool                 radsShown = false;  // a Rads row of the food's own shows them
	};

	inline thread_local FoodCard* t_foodCard = nullptr;

	// The FoodCard of a_stack of a_item, in Effects.cpp.
	[[nodiscard]] FoodCard FoodCardOf(const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack);

	// The Rads row a_card still needs once the game built it, when no Rads
	// row of the food's own showed its rads, in Effects.cpp.
	[[nodiscard]] std::optional<float> RadsRow(const FoodCard& a_card);

	// The text of a Rads row, the vanilla short name for Rads.
	[[nodiscard]] const char* RadsText();

	// The game ends a card with its weight and value rows. A Rads row NEC
	// adds goes just before them, under the food's other effects.
	inline constexpr std::uint32_t CLOSING_ROWS = 2;

	// Patches the calls that fill item cards. Returns whether either CND call
	// was patched.
	bool PatchCards();

	// The CND row of food that spoils, in Food.cpp: how much of its time
	// a_stack has left as a whole percent, 100 before it is stamped and 0
	// once spoiled. Nothing for anything else, and for all food while
	// bFoodSpoils is off.
	[[nodiscard]] std::optional<double> FoodPercent(const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack);

	// Whether the CND row of a_stack's food gets the fridge icon, in
	// Food.cpp: it sits in the fridge the container screen shows, see
	// FoodSpoil::InOpenFridge, and FoodSpoil::FridgeIconOn says so.
	[[nodiscard]] bool InFridge(const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack);

	// What a CND entry carries in the card's data when its row gets the
	// fridge icon. The card itself never reads it.
	inline constexpr std::string_view FRIDGE_MARK = "necFridge";

	// Shows the fridge icon left of the number on a_row, the CND row, while
	// a_fridge says so, and hides one shown before, in FridgeIcon.cpp. The
	// icon is the movie's own cryo damage icon, made by its exported name,
	// so only a movie that names one gets it. a_movieFile is the movie's.
	void MarkFridge(Scaleform::GFx::Movie& a_movie, std::string_view a_movieFile, Scaleform::GFx::Value& a_row, bool a_fridge);

	// What a CND entry carries in the card's data beside its number: the
	// item's wear rate, which the card itself never reads. The Pip-Boy's
	// card keeps no member of NEC's own, so Pipboy keeps its rate instead.
	inline constexpr std::string_view WEAR_RATE_MARK = "necWearRate";

	// What a workbench's CND entry carries beside WEAR_RATE_MARK while a mod
	// is tried on: the wear rate of the item itself, without the mod.
	inline constexpr std::string_view WEAR_RATE_COMPARED_MARK = "necWearRateCompared";

	// How fast a_stack of a_item wears or spoils against an ordinary item of
	// its kind in this load order, in WearRate.cpp: the wear code's own
	// share, 1 for an ordinary one, 0 for one that never wears. Nothing for
	// an item with no CND row.
	[[nodiscard]] std::optional<float> WearRateOf(const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack);

	// The change a workbench feeds the CND entry, in WearRate.cpp: a_own, the
	// item's printed rate, less a_rate, its copy's, or 0 under 0.1 apart.
	[[nodiscard]] float WearRateChange(float a_own, float a_rate);

	// Writes a_rate beside the number on a_row, the CND row, in WearRate.cpp:
	// in the number's own field, grown left over the label's free room, else
	// in the empty field beside it, else nowhere, so it never draws shrunk.
	// Nothing for a row whose item has no rate. a_movieFile is the movie's.
	// a_compared is the rate of the item a workbench compares its copy with.
	// While a mod changes the printed rate, it signs the rate in the cell
	// beside the number, and a change the movie prints beside the number is
	// kept with its sign turned to the way the rate moves. The rate in the
	// number's field never carries a sign.
	void WriteWearRate(Scaleform::GFx::Movie& a_movie, std::string_view a_movieFile, Scaleform::GFx::Value& a_row, std::optional<float> a_rate,
		std::optional<float> a_compared);

	// Drops what WriteWearRate remembers of the last row it measured, for
	// WatchCard when a card's movie loads.
	void ForgetWearRate();

	// The entries of a card the engine's builder filled, into a_out: a_target
	// itself when a menu refreshes its card, else the ItemCardInfoList member
	// of the list entry a menu builds. False when it holds neither.
	bool CardEntries(Scaleform::GFx::Value& a_target, Scaleform::GFx::Value& a_out);

	// Moves the entry at a_from in a_entries to a_to, an earlier place, and
	// the entries from a_to on 1 place back. Nothing for any other pair.
	void MoveEntry(Scaleform::GFx::Value& a_entries, std::uint32_t a_from, std::uint32_t a_to);

	// While its switch is on, turns the Health row vanilla gives a power
	// armor piece, such as 25/50, into CND and the piece's condition in
	// percent, moved to the top where CND sits on other armor. a_site names
	// the call for the trace. In PowerArmor.cpp, this one for every card but
	// the Pip-Boy's.
	void ShowPowerArmorCnd(Scaleform::GFx::Value& a_target, const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack,
		std::string_view a_site);

	// The same for a_data, the Pip-Boy's card, in PowerArmor.cpp.
	void ShowPowerArmorCnd(RE::PipboyObject& a_data, const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack,
		std::string_view a_site);

	// While FireRate slows worn weapons, patches the fire rate calls of the
	// cards and of the 2 lists that weigh fire rates. Patches the sort's note
	// of each stack's condition while fire rate or accuracy needs it, see
	// TakeSortNote. Runs before PatchAims.
	void PatchRates();

	// While worn guns lose accuracy, patches the accuracy calls of the cards
	// and of the sort, and while they lose range, the range calls of the
	// cards, in Aim.cpp.
	void PatchAims();

	// The condition the sort a container or a trader uses noted for the
	// stack it weighs next, in Rate.cpp. A sort hook takes it once, so a
	// call asked with no note never takes the note of the stack before.
	// Nothing when there is no note.
	[[nodiscard]] std::optional<float> TakeSortNote();

	// Whether NEC writes the sort's note, so the sort hooks get one. In
	// Rate.cpp, known once PatchRates ran.
	[[nodiscard]] bool SortNoted();

	// The condition of the item on the card being built, in Rate.cpp.
	// INVALID_HEALTH when no card is being built.
	[[nodiscard]] float ItemHealth();

	// The condition of the equipped copy of a_weapon that the card being
	// built compares its item with, in Rate.cpp. Only one copy of a weapon
	// can be equipped. INVALID_HEALTH when there is none.
	[[nodiscard]] float ComparedHealth(const RE::TESForm* a_weapon);

	// Adds the render listener that keeps a card's CND row above Damage to
	// a menu movie that draws item cards, and passes over any other movie.
	void WatchCard(Scaleform::GFx::Movie& a_movie, std::string_view a_file);
}
