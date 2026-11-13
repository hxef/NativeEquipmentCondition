#include "UI/Repair/VendorRepair/SignBlock.h"

#include "Core/Text/Text.h"
#include "UI/Flash.h"
#include "UI/Roles/Roles.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace VendorRepair::Sign
{
	namespace
	{
		using Scaleform::GFx::Value;

		// -------------------------------------------------------------------
		// The block
		// -------------------------------------------------------------------

		// The block, and the 2 card rows it is made of, which both games'
		// barter movies export by these names. Each row's part is what the
		// trace names when the movie has no such class.
		constexpr const char* SIGN_CLASS = "flash.display.Sprite";

		struct RowClass
		{
			const char*      name;
			std::string_view part;
		};

		constexpr RowClass TITLE_CLASS{ "Components.ItemCard_DescriptionEntry", "item card title row class"sv };
		constexpr RowClass ROW_CLASS{ "ItemCard_StandardEntry", "item card row class"sv };

		// No name holds "ItemCard", so nothing that finds the card by name
		// finds the sign.
		constexpr const char* SIGN_NAME = "NEC_TraderSign_mc";
		constexpr const char* TITLE_NAME = "NEC_TraderSignTitle_mc";

		struct Kind
		{
			Trade       trade;
			const char* name;
		};

		constexpr Kind KINDS[]{
			{ Trade::kWeapons, "NEC_TraderSignWeapons_mc" },
			{ Trade::kArmor, "NEC_TraderSignArmor_mc" },
			{ Trade::kClothing, "NEC_TraderSignClothing_mc" },
		};

		// How far each row reaches into the one above it, as the card stacks
		// its rows.
		constexpr double OVERLAP = 3.5;

		// A row of a_class named a_name, filled through the row's own
		// PopulateEntry as the card fills it: a_level prints at the right as
		// "80%". Not a display object when the movie has no such class or the
		// row refuses the call, with a_missing saying which. A new row's
		// compare arrow stays on its first frame, which is empty.
		[[nodiscard]] Value Row(Scaleform::GFx::Movie& a_movie, const RowClass& a_class, const char* a_name,
			const std::string& a_text, std::optional<std::uint32_t> a_level, std::string_view& a_missing)
		{
			Value row;
			a_movie.CreateObject(&row, a_class.name);
			Value entry;
			a_movie.CreateObject(&entry);
			if (!row.IsDisplayObject() || !entry.IsObject()) {
				a_missing = a_class.part;
				return {};
			}
			Flash::Set(entry, "text"sv, Value(a_text.c_str()));
			if (a_level) {
				Flash::Set(entry, "value"sv, Value(static_cast<double>(*a_level)));
				Flash::Set(entry, "showAsPercent"sv, Value(true));
			}
			Flash::Set(row, "name"sv, Value(a_name));
			// A movie whose rows ask for more than the entry refuses the call.
			if (!Flash::Call(row, "PopulateEntry", std::array{ entry })) {
				a_missing = "item card row that fills from 1 entry"sv;
				return {};
			}
			return row;
		}
	}

	Value Build(Scaleform::GFx::Movie& a_movie, const Ceilings& a_ceilings, double a_scale, std::string_view& a_missing)
	{
		Value sign;
		a_movie.CreateObject(&sign, SIGN_CLASS);
		if (!sign.IsDisplayObject()) {
			a_missing = "Sprite class"sv;
			return {};
		}
		Flash::Set(sign, "name"sv, Value(SIGN_NAME));
		Flash::Set(sign, "mouseEnabled"sv, Value(false));
		Flash::Set(sign, "mouseChildren"sv, Value(false));

		const auto any = std::ranges::any_of(KINDS, [&](const Kind& a_kind) { return a_ceilings.Of(a_kind.trade) > 0; });
		// Each row goes in as it is made, so the trace names the first
		// step the movie refuses.
		double     top = 0.0;
		const auto stack = [&](Value a_row) {
			if (!a_row.IsDisplayObject()) {
				return false;
			}
			const auto box = Flash::Call(sign, "addChild", std::array{ a_row }) ? Roles::BoundsIn(a_row, sign) : std::nullopt;
			if (!box) {
				a_missing = "item card row the sign can stack"sv;
				return false;
			}
			Flash::Set(a_row, "y"sv, Value(Flash::Number(a_row, "y"sv) + top - box->top));
			top += box->bottom - box->top - OVERLAP;
			return true;
		};
		if (!stack(Row(a_movie, TITLE_CLASS, TITLE_NAME, any ? Text::SignTitle() : Text::SignNone(), std::nullopt, a_missing))) {
			return {};
		}
		for (const auto& kind : KINDS) {
			if (const auto level = a_ceilings.Of(kind.trade); level > 0 && !stack(Row(a_movie, ROW_CLASS, kind.name, Text::SignKind(kind.trade), level, a_missing))) {
				return {};
			}
		}
		Flash::Set(sign, "scaleX"sv, Value(a_scale));
		Flash::Set(sign, "scaleY"sv, Value(a_scale));
		return sign;
	}
}
