#include "UI/Roles/Boxes.h"

#include "Core/TraceLog.h"
#include "UI/Flash.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Roles::Boxes
{
	namespace
	{
		// -------------------------------------------------------------------
		// Vanilla's places
		// -------------------------------------------------------------------

		// The panel's parts a growth moves, and the field it asks in.
		constexpr const char* BACKGROUND_NAME = "BGRect_mc";
		constexpr const char* BUTTONS_NAME = "ButtonHintBar_mc";
		constexpr const char* ARROW_NAME = "ScrollDown_mc";
		constexpr const char* QUESTION_NAME = "ConfirmQuestion_tf";

		// The menu that draws a bench's buttons for it, and its bar there.
		constexpr auto        DRAWER_MENU = "ButtonBarMenu"sv;
		constexpr const char* HOLDER_NAME = "ButtonBarHolder_mc";
		constexpr const char* BAR_NAME = "ButtonHintBar_mc";

		// What the player sees when the box cannot be grown.
		constexpr auto KEPT = "the box keeps the size its movie gave it"sv;

		// -------------------------------------------------------------------
		// Reading a part
		// -------------------------------------------------------------------

		// A member of a clip as a number, or nothing when it is no number or
		// not finite, as a row's originalY is before a late layout writes it.
		[[nodiscard]] std::optional<double> NumberOf(Value& a_clip, std::string_view a_name)
		{
			const auto member = Flash::Member(a_clip, a_name);
			if (!Flash::IsAnyNumber(member)) {
				return std::nullopt;
			}
			const auto number = Flash::AsNumber(member);
			return std::isfinite(number) ? std::optional{ number } : std::nullopt;
		}

		// Says in the trace why the box found no bench button bar to stay above.
		std::nullopt_t NoBar(std::string_view a_why)
		{
			TraceLog::Line("menu", "Confirmation box found no bench button bar to stay above, {:s}", a_why);
			return std::nullopt;
		}

		// How far down its movie's screen a shown clip starts, 0 to 1.
		[[nodiscard]] std::optional<double> TopOf(Scaleform::GFx::Movie& a_movie, Value& a_clip)
		{
			const auto shown = a_movie.GetVisibleFrameRect();
			const auto bounds = Flash::StageBounds(a_clip);
			if (shown.y2 <= shown.y1 || !bounds) {
				return NoBar("its place on the screen was not found"sv);
			}
			const auto top = (Flash::Number(*bounds, "y"sv) - shown.y1) / (shown.y2 - shown.y1);
			return std::isfinite(top) ? std::optional{ top } : NoBar("its place on the screen was not found"sv);
		}
	}

	// -------------------------------------------------------------------
	// The box
	// -------------------------------------------------------------------

	std::optional<Parts> ConfirmPanel(RE::ExamineConfirmMenu& a_menu)
	{
		if (!a_menu.uiMovie) {
			return std::nullopt;
		}
		auto& movie = *a_menu.uiMovie;
		auto& panel = a_menu.confirmObj;
		Parts parts{ Flash::Member(panel, BACKGROUND_NAME), Flash::Member(panel, BUTTONS_NAME), Flash::Member(panel, ARROW_NAME) };
		if (!panel.IsDisplayObject() || !parts.background.IsDisplayObject() || !parts.buttons.IsDisplayObject() ||
			!parts.arrow.IsDisplayObject()) {
			Noted(movie, "confirmation box background, buttons or down arrow"sv, KEPT);
			return std::nullopt;
		}

		// The down arrow shows only where Build cut rows off. A box with no
		// rows, such as Exit Station?, has no last row to read.
		if (!Flash::Bool(parts.arrow, "visible"sv)) {
			Found(movie, "confirmation box's parts"sv, "at vanilla's place"sv);
			return std::nullopt;
		}

		// A clip still counts hidden rows in its height. The numbers a growth
		// moves are read here too, so none of them is written back as NaN.
		const auto children = NumberOf(panel, "numChildren"sv);
		auto       last = children && *children >= 1.0 ? Flash::ChildAt(panel, static_cast<std::uint32_t>(*children - 1.0)) : Value{};
		const auto start = last.IsDisplayObject() ? NumberOf(last, "originalY"sv) : std::nullopt;
		const auto height = NumberOf(last, "height"sv);
		if (!start || !height || !NumberOf(parts.background, "height"sv) || !NumberOf(parts.buttons, "y"sv) ||
			!NumberOf(parts.arrow, "y"sv) || !NumberOf(panel, "y"sv)) {
			Noted(movie, "laid out confirmation box"sv, KEPT);
			return std::nullopt;
		}
		if (!OnScreen(movie, parts.background)) {
			Noted(movie, "shown confirmation box background"sv, KEPT);
			return std::nullopt;
		}
		parts.listEnd = *start + *height;
		Found(movie, "confirmation box's parts"sv, "at vanilla's place"sv);
		return parts;
	}

	// The box writes its question into its panel as it opens.
	std::string Question(RE::ExamineConfirmMenu& a_menu)
	{
		auto text = Flash::String(Flash::Child(a_menu.confirmObj, QUESTION_NAME), "text"sv);
		std::ranges::replace(text, '\r', ' ');
		std::ranges::replace(text, '\n', ' ');
		return text;
	}

	// -------------------------------------------------------------------
	// The bench's button bar
	// -------------------------------------------------------------------

	// The bench hands its buttons to ButtonBarMenu to draw, unless its bar
	// draws them itself. Every menu's movie is drawn the full height of the
	// screen, so the bar's share of its movie's visible height is the same
	// share of the box's.
	std::optional<double> BenchBarTop()
	{
		auto* const ui = RE::UI::GetSingleton();
		const auto  name = ui && ui->GetMenuOpen<RE::PowerArmorModMenu>() ? RE::PowerArmorModMenu::MENU_NAME : RE::ExamineMenu::MENU_NAME;
		const auto  open = ui ? ui->GetMenu(name) : Scaleform::Ptr<RE::IMenu>{};
		auto* const bench = open && open->OnStack() ? static_cast<RE::GameMenuBase*>(open.get()) : nullptr;
		if (!bench) {
			return NoBar("no workbench is open"sv);
		}
		auto* const own = bench->buttonHintBar.get();
		if (!own || !bench->uiMovie) {
			return NoBar("the workbench has no button bar"sv);
		}
		if (!own->redirectToButtonBarMenu) {
			if (!OnScreen(*bench->uiMovie, *own)) {
				return NoBar("the workbench's own bar is hidden or off screen"sv);
			}
			Found(*bench->uiMovie, "bench button bar"sv, "through the engine"sv);
			return TopOf(*bench->uiMovie, *own);
		}

		// The game sets this while the bar at the bottom shows the bench's
		// buttons and not another menu's.
		if (!own->isTopButtonBar) {
			return NoBar("the bar at the bottom shows another menu's buttons"sv);
		}
		const auto drawer = ui->GetMenu(DRAWER_MENU);
		if (!drawer || !drawer->uiMovie) {
			return NoBar("the button bar menu is not open"sv);
		}
		auto& movie = *drawer->uiMovie;
		auto  holder = Flash::Child(drawer->menuObj, HOLDER_NAME);
		auto  bar = Flash::Child(holder, BAR_NAME);
		if (!holder.IsDisplayObject() || !bar.IsDisplayObject()) {
			return NoBar("the button bar menu's bar was not found"sv);
		}

		// OnScreen reads only the bar's own flags, so the movie's and the
		// holder's are asked here.
		if (!movie.GetVisible() || !Flash::Bool(holder, "visible"sv)) {
			return NoBar("the button bar menu is hidden"sv);
		}

		// Each time a menu opens or closes, the repair picker among them, the
		// game hands the bar the bench's buttons again and the bar hides until
		// its next frame, which comes after the box opens. A bar waiting to
		// draw still sits where it last drew, so it counts as shown.
		const auto shown = Flash::Bool(bar, "visible"sv);
		if (!shown && !Flash::Bool(bar, "bIsDirty"sv)) {
			return NoBar("the bar has no buttons to show"sv);
		}
		if (shown ? !OnScreen(movie, bar) : !(Flash::Number(bar, "alpha"sv) > 0.0 && InFrame(movie, bar))) {
			return NoBar("the bar is faded out or off screen"sv);
		}
		Found(movie, "bench button bar"sv, "at vanilla's place"sv);
		return TopOf(movie, bar);
	}

	// -------------------------------------------------------------------
	// The message box
	// -------------------------------------------------------------------

	Value MessageBackground(RE::MessageBoxMenu& a_menu)
	{
		return Flash::Member(a_menu.menuObj, BACKGROUND_NAME);
	}
}
