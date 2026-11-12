#include "UI/Repair/Gains.h"

#include "Condition/Condition.h"
#include "Core/Settings.h"
#include "Core/Text/Text.h"
#include "Gameplay/ArmorRating.h"
#include "Gameplay/HealthDamage/HealthDamage.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <utility>

namespace Gains
{
	namespace
	{
		using Numbers = std::vector<std::pair<std::uint32_t, float>>;

		// The card's numbers for the copy on a_stack at a_health.
		[[nodiscard]] Numbers At(const SelectedItem::Item& a_item, const RE::BGSInventoryItem::Stack* a_stack, float a_health)
		{
			return a_item.Armor() ? ArmorRating::CardResists(*a_item.item, a_stack, a_health) :
			                        HealthDamage::CardDamage(*a_item.item, a_stack, a_health);
		}

		// A number as the card prints it. The card writes the number out and
		// cuts it at the point, so 24.9 prints 24.
		[[nodiscard]] std::int64_t Printed(float a_number)
		{
			return static_cast<std::int64_t>(std::trunc(static_cast<double>(a_number)));
		}

		// a_column's number at the level of a_row.
		[[nodiscard]] float To(const Column& a_column, std::size_t a_row)
		{
			return a_row < a_column.at.size() ? a_column.at[a_row] : a_column.now;
		}

		// What the card's printed number gains at the level of a_row.
		[[nodiscard]] std::int64_t Gain(const Column& a_column, std::size_t a_row)
		{
			return Printed(To(a_column, a_row)) - Printed(a_column.now);
		}

		// The number a_numbers holds for a_type, 0 for none.
		[[nodiscard]] float Of(const Numbers& a_numbers, std::uint32_t a_type)
		{
			const auto found = std::ranges::find(a_numbers, a_type, &Numbers::value_type::first);
			return found != a_numbers.end() ? found->second : 0.0F;
		}
	}

	Found Read(const SelectedItem::Item& a_item, const std::vector<std::uint32_t>& a_levels)
	{
		Found found;
		if (!Settings::bRepairBoxGains.GetValue()) {
			found.off = true;
			return found;
		}
		const auto* stack = a_item.item ? a_item.item->GetStackByID(a_item.stack) : nullptr;
		const auto  card = a_item.object && (a_item.object->Is(RE::ENUM_FORM_ID::kWEAP) || a_item.object->Is(RE::ENUM_FORM_ID::kARMO));
		if (!stack || !card) {
			return found;
		}

		const auto now = At(a_item, stack, Condition::HealthOf(stack));
		std::vector<Numbers> after;
		after.reserve(a_levels.size());
		for (const auto level : a_levels) {
			after.push_back(At(a_item, stack, Condition::FromPercent(level)));
		}

		// The card shows a type while its number is above 0, so a type gets a
		// column when any level would show it.
		for (const auto& [type, number] : now) {
			const auto shown = std::ranges::any_of(after, [type](const Numbers& a_at) { return Of(a_at, type) > 0.0F; });
			if (!shown || Text::GainByType(0, type, a_item.Armor(), false).empty()) {
				continue;
			}
			auto& column = found.columns.emplace_back(Column{ type, number, {}, a_item.Armor() });
			for (const auto& at : after) {
				column.at.push_back(Of(at, type));
			}
		}
		return found;
	}

	void AddRow(Columns::Table& a_table, std::vector<std::string> a_kept, const Found& a_found, std::size_t a_row)
	{
		a_table.kept = a_kept.size();
		auto& full = a_table.rows.emplace_back(std::move(a_kept));
		auto& shortWords = a_table.shortGains.emplace_back();
		for (const auto& column : a_found.columns) {
			const auto gain = Gain(column, a_row);
			full.push_back(Text::GainByType(gain, column.type, column.armor, false));
			shortWords.push_back(Text::GainByType(gain, column.type, column.armor, true));
		}
	}

	std::string Spelled(const Found& a_found, std::size_t a_row)
	{
		std::string out;
		for (const auto& column : a_found.columns) {
			out += std::format("{:s}type {:d} {:.2f} to {:.2f} +{:d}", out.empty() ? "" : ", ", column.type, column.now,
				To(column, a_row), Gain(column, a_row));
		}
		if (out.empty()) {
			return a_found.off ? "gains off" : "no numbers";
		}
		return out;
	}
}
