#include "UI/Repair/VendorRepair/Quote.h"

#include "Core/Settings.h"

#include <utility>

namespace VendorRepair
{
	float Scaled(float a_multiple)
	{
		return a_multiple * Settings::fTraderPriceMult.GetValue();
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
}
