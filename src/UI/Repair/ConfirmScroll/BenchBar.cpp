#include "UI/Repair/ConfirmScroll/BenchBar.h"

#include "Core/TraceLog.h"
#include "UI/Flash.h"

#include <array>
#include <optional>
#include <string_view>

namespace ConfirmScroll
{
	namespace
	{
		using Value = Scaleform::GFx::Value;

		// Says in the trace why the box found no bench button bar to stay above.
		std::nullopt_t NoBar(std::string_view a_why)
		{
			TraceLog::Line("menu", "Confirmation box found no bench button bar to stay above, {:s}", a_why);
			return std::nullopt;
		}

		// How far down its movie's screen a clip starts, 0 to 1. A hidden clip is
		// measured too, so the caller decides whether it counts as shown.
		[[nodiscard]] std::optional<double> ScreenTop(Scaleform::GFx::Movie& a_movie, Value& a_clip)
		{
			Value      stage;
			Value      bounds;
			const auto shown = a_movie.GetVisibleFrameRect();
			if (!a_clip.IsDisplayObject() || shown.y2 <= shown.y1 ||
				!a_clip.GetMember("stage"sv, &stage) || !a_clip.Invoke("getBounds", &bounds, std::array{ stage })) {
				return NoBar("its place on the screen was not found"sv);
			}
			return (Flash::Number(bounds, "y"sv) - shown.y1) / (shown.y2 - shown.y1);
		}
	}

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
			return Flash::Bool(*own, "visible"sv) ? ScreenTop(*bench->uiMovie, *own) : NoBar("the workbench's own bar is hidden"sv);
		}

		// The game sets this while the bar at the bottom shows the bench's
		// buttons and not another menu's.
		if (!own->isTopButtonBar) {
			return NoBar("the bar at the bottom shows another menu's buttons"sv);
		}
		const auto drawer = ui->GetMenu("ButtonBarMenu"sv);
		if (!drawer || !drawer->uiMovie) {
			return NoBar("the button bar menu is not open"sv);
		}
		auto holder = Flash::Child(drawer->menuObj, "ButtonBarHolder_mc");
		auto bar = Flash::Child(holder, "ButtonHintBar_mc");
		if (!holder.IsDisplayObject() || !bar.IsDisplayObject()) {
			return NoBar("the button bar menu's bar was not found"sv);
		}
		if (!drawer->uiMovie->GetVisible() || !Flash::Bool(holder, "visible"sv)) {
			return NoBar("the button bar menu is hidden"sv);
		}

		// Each time a menu opens or closes, the repair picker among them, the
		// game hands the bar the bench's buttons again and the bar hides until
		// its next frame, which comes after the box opens. A bar waiting to
		// draw still sits where it last drew, so it counts as shown.
		if (!Flash::Bool(bar, "visible"sv) && !Flash::Bool(bar, "bIsDirty"sv)) {
			return NoBar("the bar has no buttons to show"sv);
		}
		return ScreenTop(*drawer->uiMovie, bar);
	}
}
