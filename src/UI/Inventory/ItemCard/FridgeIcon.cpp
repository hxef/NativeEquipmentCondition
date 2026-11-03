#include "UI/Inventory/ItemCard/Cards.h"

#include "Core/TraceLog.h"
#include "UI/Flash.h"
#include "UI/FridgeIcon.h"
#include "UI/Roles/Card.h"

#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <string>

namespace ItemCard
{
	namespace
	{
		using Scaleform::GFx::Value;

		// The movie whose card can show food in a fridge, and the exported
		// name of its damage icons, the clip its damage rows show.
		constexpr std::string_view ICONS_MOVIE = "ContainerMenu.swf";
		constexpr const char*      ICONS_CLASS = "ContainerMenu_fla.DamageIcons_47";

		// The game puts a timed row's clock 10 left of the x the number's
		// line reports, which is 2 short of its first letter. The icon keeps
		// that gap.
		constexpr double ICON_GAP = 12.0;

		// On the vanilla card a timed row's clock stands 21 high beside
		// numbers whose capitals stand 26 high. The icon keeps that size to
		// the number on every card, so a card with smaller numbers gets a
		// smaller icon.
		constexpr double ICON_PER_CAPITALS = 21.0 / 26.0;

		// The number the icon went beside, in a_row's units.
		struct Spot
		{
			FridgeIcon::Line line;
			double           fieldX = 0.0;
			std::uint32_t    color = 0;  // its letters' colour, 0xRRGGBB
		};

		// Puts the icon left of the number on its first line, ICON_GAP short
		// of its first letter and centred on the line, and gives it the
		// number's colour. A card may colour each field and not the row, by
		// its textColor or its own colour, so the icon takes the colour from
		// the field. The number it went beside, or nothing when the row
		// cannot be measured.
		std::optional<Spot> Place(Value& a_icon, Value& a_row)
		{
			auto       field = Roles::Card::NumberField(a_row);
			const auto line = FridgeIcon::LineOf(field);
			const auto box = Roles::BoundsIn(a_icon, a_icon);
			if (!line || !box ||
				!FridgeIcon::Put(a_icon, *box, FridgeIcon::Side::kLeft, line->left - ICON_GAP, line->capitals * ICON_PER_CAPITALS, line->middle)) {
				return std::nullopt;
			}
			const auto tint = Flash::LetterTint(field);
			Flash::SetTint(a_icon, tint);
			return Spot{ *line, Flash::Number(field, "x"sv), tint.OnWhite() };
		}
	}

	void MarkFridge(Scaleform::GFx::Movie& a_movie, std::string_view a_movieFile, Value& a_row, bool a_fridge)
	{
		if (a_movieFile != ICONS_MOVIE) {
			return;
		}
		auto icon = FridgeIcon::Of(a_row);
		if (!icon.IsDisplayObject()) {
			if (!a_fridge) {
				return;
			}
			icon = FridgeIcon::Make(a_movie, a_row, ICONS_CLASS);
			if (!icon.IsDisplayObject()) {
				Roles::Missing(a_movie, ICONS_CLASS, "a food's card in a fridge shows no icon"sv);
				return;
			}
		}

		const auto spot = a_fridge ? Place(icon, a_row) : std::nullopt;
		const auto shown = spot.has_value();
		if (shown == Flash::Bool(icon, "visible"sv)) {
			return;
		}
		Flash::Set(icon, "visible"sv, Value(shown));
		if (!shown) {
			TraceLog::Line("menu", "Fridge icon off the CND row of {:s}", ICONS_MOVIE);
			return;
		}
		const auto box = Roles::BoundsIn(icon, a_row);
		const auto tint = Flash::TintOf(icon);
		const auto nan = std::numeric_limits<double>::quiet_NaN();
		TraceLog::Line("menu", "Fridge icon on the CND row of {:s}, {:.1f} high with its middle at y {:.1f}, its right edge at x {:.1f} and its tint {:s}, by the number's line from x {:.1f} with its middle at y {:.1f} as drawn, {:.1f} by its metrics, and its capitals {:.1f} high, in its field at x {:.1f} coloured {:06X}",
			ICONS_MOVIE, box ? box->bottom - box->top : nan, box ? (box->top + box->bottom) / 2.0 : nan, box ? box->right : nan,
			tint ? std::format("{:06X}", tint->OnWhite()) : "none"s, spot->line.left, spot->line.middle, spot->line.metricsMiddle, spot->line.capitals, spot->fieldX,
			spot->color);
	}
}
