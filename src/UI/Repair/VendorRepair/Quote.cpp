#include "UI/Repair/VendorRepair/Quote.h"

#include "Condition/Repair.h"
#include "Core/Settings.h"
#include "Core/Text/Text.h"
#include "Core/TraceLog.h"
#include "UI/Repair/Columns/Columns.h"
#include "UI/Repair/Gains.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace VendorRepair
{
	namespace
	{
		// Caps, where Fallout 4's list of default objects fails to name them. A
		// load order that replaces the currency says so in that list.
		constexpr RE::TESFormID CAPS_FORM = 0x0000000F;

		// The most fTraderPriceMult counts for, as the MCM slider allows. A
		// multiple in the millions overflows the price rounding and would sell
		// steps for 1 cap.
		constexpr float MAX_PRICE_MULT = 10.0F;
	}

	RE::TESBoundObject* Caps()
	{
		const auto* defaults = RE::BGSDefaultObjectManager::GetSingleton();
		auto*       named = defaults ? defaults->GetDefaultObject<RE::TESBoundObject>(
		                                   RE::DEFAULT_OBJECT::kGold) :
		                               nullptr;
		if (named) {
			return named;
		}
		auto* form = RE::TESForm::GetFormByID(CAPS_FORM);
		return form ? form->As<RE::TESBoundObject>() : nullptr;
	}

	float Scaled(float a_multiple)
	{
		return a_multiple * std::min(Settings::fTraderPriceMult.GetValue(), MAX_PRICE_MULT);
	}

	std::vector<Quote> Quotes(const Selection& a_selection, std::uint32_t a_ceiling)
	{
		if (!a_selection.Worn() || a_selection.percent >= a_ceiling) {
			return {};
		}

		const auto worth = static_cast<float>(a_selection.worth);
		const auto multiple = Scaled(WRECK_MULTIPLE);
		const auto owed = Repair::Whole(worth * Repair::Debt(a_selection.percent, multiple));

		std::vector<Quote> steps;
		for (const auto level : Repair::Above(a_selection.percent)) {
			if (level > a_ceiling) {
				break;
			}
			const auto left = Repair::Whole(worth * Repair::Debt(level, multiple));
			const auto caps = owed > left ? owed - left : 0U;
			steps.push_back({ level, caps > 0 ? caps : 1U });
		}
		return Repair::Distinct(std::move(steps));
	}

	std::vector<Quote> Afforded(const std::vector<Quote>& a_quotes)
	{
		auto*              player = RE::PlayerCharacter::GetSingleton();
		const auto         pocket = player ? player->GetGoldAmount() : 0;
		std::vector<Quote> out;
		for (const auto& quote : a_quotes) {
			if (static_cast<std::int64_t>(quote.price) <= pocket) {
				out.push_back(quote);
			}
		}
		return out;
	}

	std::vector<std::string> Rows(RE::BarterMenu* a_menu, const Selection& a_selection, const std::vector<Quote>& a_quotes)
	{
		std::vector<std::uint32_t> levels;
		for (const auto& quote : a_quotes) {
			levels.push_back(quote.level);
		}
		const auto     gains = Gains::Read(a_selection, levels);
		Columns::Table table;
		for (std::size_t i = 0; i < a_quotes.size(); i++) {
			const auto& quote = a_quotes[i];
			Gains::AddRow(table, { Repair::NameOf(quote.level), Text::RepairCaps(quote.price) }, gains, i);
			TraceLog::Line("menu", "{:s} offers {:d}% of {:s}, its card {:s}", Trader(a_menu), quote.level, a_selection.Name(),
				Gains::Spelled(gains, i));
		}
		return Columns::Lay(a_menu ? a_menu->uiMovie.get() : nullptr, table, Trader(a_menu) + "'s repair box");
	}
}
