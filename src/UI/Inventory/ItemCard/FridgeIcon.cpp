#include "UI/Inventory/ItemCard/Cards.h"

#include "Core/TraceLog.h"
#include "UI/Flash.h"
#include "UI/FridgeIcon.h"
#include "UI/Roles/Card.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>

namespace ItemCard
{
	namespace
	{
		using Scaleform::GFx::Value;

		// The movie whose card can show food in a fridge, and the exported
		// name of its damage icons, the clip its damage rows show.
		constexpr std::string_view ICONS_MOVIE = "ContainerMenu.swf";
		constexpr const char*      ICONS_CLASS = "ContainerMenu_fla.DamageIcons_47";

		// A timed row, such as a food's HP, shows the vanilla clock icon left of its
		// number. The fridge icon takes the clock's size and place.
		constexpr const char* TIMED_CLASS = "Components.ItemCard_TimedEntry";

		// The game puts the clock 10 left of the x the number's line reports,
		// which is 2 short of its first letter.
		constexpr double ICON_GAP = 12.0;

		// Where a timed row draws its clock, from its number field's x and y.
		struct Clock
		{
			double right = 0.0;
			double height = 0.0;
			double middle = 0.0;
		};

		// Read from the first card that needs it, since every card of the
		// movie draws its rows from the same file.
		std::optional<Clock> g_clock;

		// Makes a timed row, hidden, measures its clock and takes it off
		// again. It goes into a_row only for that, since a clip outside the
		// display list has no bounds.
		std::optional<Clock> MeasureClock(Scaleform::GFx::Movie& a_movie, Value& a_row)
		{
			Value timed;
			a_movie.CreateObject(&timed, TIMED_CLASS);
			if (!timed.IsDisplayObject()) {
				return std::nullopt;
			}
			Flash::Set(timed, "visible"sv, Value(false));
			if (!Flash::Call(a_row, "addChild", std::array{ timed })) {
				return std::nullopt;
			}
			auto       clock = Flash::Member(timed, "TimerIcon_mc"sv);
			const auto field = Flash::Member(timed, "Value_tf"sv);
			const auto box = Roles::BoundsIn(clock, timed);
			Flash::Call(a_row, "removeChild", std::array{ timed });
			if (!box || !field.IsDisplayObject()) {
				return std::nullopt;
			}
			const auto x = Flash::Number(field, "x"sv);
			const auto y = Flash::Number(field, "y"sv);
			return Clock{ box->right - x, box->bottom - box->top, (box->top + box->bottom) / 2.0 - y };
		}

		// Where the icon goes in a_row, in its units.
		struct Spot
		{
			double edge = 0.0;
			double height = 0.0;
			double middle = 0.0;
			double lineMiddle = 0.0;
		};

		// Puts the icon where a timed row puts its clock: as high, as far
		// below the number field's top and as far left of the number, though
		// never right of the clock's own column. The spot it took, or nothing
		// when the row or the clock cannot be measured.
		std::optional<Spot> Place(Scaleform::GFx::Movie& a_movie, Value& a_icon, Value& a_row)
		{
			if (!g_clock) {
				g_clock = MeasureClock(a_movie, a_row);
				if (!g_clock) {
					Roles::Missing(a_movie, TIMED_CLASS, "a food's card in a fridge shows no icon"sv);
					return std::nullopt;
				}
			}

			auto       field = Roles::Card::NumberField(a_row);
			const auto line = FridgeIcon::LineOf(field);
			const auto box = Roles::BoundsIn(a_icon, a_icon);
			if (!line || !box) {
				return std::nullopt;
			}
			const auto x = Flash::Number(field, "x"sv);
			const auto y = Flash::Number(field, "y"sv);
			const Spot spot{ std::min(line->left - ICON_GAP, x + g_clock->right), g_clock->height, y + g_clock->middle, line->middle };
			if (!FridgeIcon::Put(a_icon, *box, FridgeIcon::Side::kLeft, spot.edge, spot.height, spot.middle)) {
				return std::nullopt;
			}
			return spot;
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

		const auto spot = a_fridge ? Place(a_movie, icon, a_row) : std::nullopt;
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
		const auto nan = std::numeric_limits<double>::quiet_NaN();
		TraceLog::Line("menu", "Fridge icon on the CND row of {:s}, {:.1f} high with its middle at y {:.1f}, a timed row's clock {:.1f} high at {:.1f}, the number's line at {:.1f}",
			ICONS_MOVIE, box ? box->bottom - box->top : nan, box ? (box->top + box->bottom) / 2.0 : nan, spot->height, spot->middle, spot->lineMiddle);
	}
}
