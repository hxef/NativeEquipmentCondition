#include "UI/Repair/VendorRepair/Sign.h"

#include "Core/Settings/Settings.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Repair/VendorRepair/SignBack.h"
#include "UI/Repair/VendorRepair/SignBlock.h"
#include "UI/Repair/VendorRepair/SignLine.h"
#include "UI/Repair/VendorRepair/SignMove.h"
#include "UI/Repair/VendorRepair/SignPlace.h"
#include "UI/Repair/VendorRepair/Stock.h"
#include "UI/Roles/Roles.h"
#include "UI/Roles/TraderSide.h"

#include <algorithm>
#include <array>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace VendorRepair::Sign
{
	namespace
	{
		// What the player sees when the movie lacks a part the panel, the
		// line or both need.
		constexpr auto NO_SIGN = "the barter screen shows no trader repair sign"sv;
		constexpr auto ONLY_ABOVE = "the trader repair sign shows only above the trader's list"sv;
		constexpr auto ONLY_UNDER = "the trader repair sign shows only under the trader's list"sv;

		// The title row as Place last saw it while the line was shown or left
		// out for want of room, for TitleRowChanged.
		std::optional<TitleRow> g_row;

		// Takes the screen's panel, line and moved sign out and forgets them,
		// with every target, a_how saying why.
		void TakeOut(RE::BarterMenu& a_menu, std::string_view a_how)
		{
			g_row.reset();
			SignBack::Drop(a_menu, a_how);
			Clear(a_menu);
			ForgetMoved(a_menu);
		}

		// "weapons 100%, armor 80%", or "no repairs".
		[[nodiscard]] std::string Reads(const Ceilings& a_ceilings)
		{
			std::string out;
			for (const auto trade : TRADES) {
				if (const auto level = a_ceilings.Of(trade); level > 0) {
					out += std::format("{:s}{:s} {:d}%", out.empty() ? "" : ", ", Named(trade), level);
				}
			}
			return out.empty() ? "no repairs" : out;
		}
	}

	// -------------------------------------------------------------------
	// Drawing, placing and forgetting the sign
	// -------------------------------------------------------------------

	void Draw(RE::BarterMenu* a_menu)
	{
		if (!a_menu || !a_menu->uiMovie || !a_menu->menuObj.IsObject()) {
			return;
		}
		TakeOut(*a_menu, COMES_OUT);
		if (!Settings::NecOn()) {
			TraceLog::Line("menu", "All of NEC is off, so {:s}'s screen shows no sign", Trader(a_menu));
			return;
		}
		if (!Settings::bVendorRepair.GetValue()) {
			TraceLog::Line("menu", "Trader repairs are off, so {:s}'s screen shows no sign", Trader(a_menu));
			return;
		}
		if (!Settings::bTraderSign.GetValue()) {
			TraceLog::Line("menu", "The trader sign is off, so {:s}'s screen shows none", Trader(a_menu));
			return;
		}

		auto& movie = *a_menu->uiMovie;
		auto  side = Roles::TraderSide::Find(*a_menu);
		if (!side) {
			Roles::Noted(movie, "trader's side"sv, NO_SIGN);
			return;
		}
		const auto ceilings = Now(a_menu);
		if (!ceilings) {
			return;
		}

		// The panel needs NEC's delete hook and the title's size, the line
		// only the title, so a movie that refuses the one still shows the
		// other. When both fail, the trace names only the panel's part, so 1
		// line says no sign shows.
		auto                 line = LineOf(movie, *side, *ceilings);
		std::optional<Panel> panel;
		if (SignBack::Held()) {
			auto missing = "Sprite class"sv;
			panel = Build(movie, *side, *ceilings, missing);
			if (!panel) {
				Roles::Noted(movie, missing, line ? ONLY_ABOVE : NO_SIGN);
			}
		} else {
			TraceLog::Once("menu", "NEC does not hold the barter screen delete, so {:s}", ONLY_ABOVE);
		}
		Clips kept;
		Forms forms;
		if (panel) {
			kept.sign = panel->clip;
			kept.back = panel->back;
		}
		if (line) {
			kept.line = line->field;
			if (line->frame) {
				kept.frame = line->frame->shape;
				forms.frame = line->frame->box;
			}
			forms.forms = std::move(line->forms);
		} else if (panel) {
			Roles::Noted(movie, "trader's title"sv, ONLY_UNDER);
		} else if (!SignBack::Held()) {
			Roles::Noted(movie, "trader's title"sv, NO_SIGN);
		}
		Keep(*a_menu, kept, std::move(forms));
		if (panel || line) {
			TraceLog::Line("menu", "{:s}'s sign reads {:s}", Trader(a_menu), Reads(*ceilings));
		}
	}

	void Place(RE::BarterMenu* a_menu, const std::optional<Roles::Bars::Hints>& a_bar)
	{
		if (!a_menu || !a_menu->uiMovie || !a_menu->menuObj.IsObject()) {
			return;
		}
		auto clips = ClipsOf(*a_menu);
		g_row.reset();
		if (!clips.sign.IsDisplayObject() && !clips.line.IsDisplayObject()) {
			return;
		}
		// A script can turn the sign off while the screen is open.
		if (!Settings::bTraderSign.GetValue()) {
			TakeOut(*a_menu, COMES_OUT);
			return;
		}
		// The engine's bar is drawn by another menu, a bar of the movie's own
		// where it sits.
		auto       side = Roles::TraderSide::Find(*a_menu);
		const auto row = a_bar && !a_bar->convention ? Roles::Bars::DrawnTop(*a_menu) : std::nullopt;
		auto       room = side && a_bar ? Roles::TraderSide::RoomOf(*a_menu->uiMovie, *side, a_bar->bar, row) : std::nullopt;
		if (!room) {
			PanelOff(*a_menu, clips, COMES_OUT);
			Off(clips.line);
			Off(clips.frame);
			MovedOff(*a_menu, COMES_OUT);
			return;
		}

		// The form's own place is the spot Choose gives. Show puts it there,
		// or PlaceMoved where the player moved it.
		auto       around = false;
		const auto choice = Choose(*side, *room, clips, around);
		if (const auto by = MoveOf(*room)) {
			if (const auto row = PlaceMoved(*a_menu, *side, *room, clips, choice, *by)) {
				if (*row) {
					g_row = RowOf(*side, around);
				}
				return;
			}
		}
		MovedOff(*a_menu, COMES_OUT);
		const auto where = choice ? Show(*a_menu, *side, clips, *choice, side->side) : std::nullopt;
		if (where) {
			Say(a_menu, *where);
			if (!choice->pick) {
				return;
			}
		} else if (!LeftOut(*a_menu, *room, clips)) {
			return;
		}
		// Read after the line went in or out, as the side counts it, with each
		// part's look while the place left of the frame is in play.
		g_row = RowOf(*side, around);
	}

	void Forget(RE::BarterMenu* a_menu)
	{
		if (a_menu) {
			TakeOut(*a_menu, CLOSES);
		}
		ForgetSaid();
	}

	bool Shown(RE::BarterMenu* a_menu)
	{
		if (!a_menu) {
			return false;
		}
		const auto clips = ClipsOf(*a_menu);
		return std::ranges::any_of(std::array{ &clips.sign, &clips.line },
			[](const Scaleform::GFx::Value* a_clip) { return a_clip->IsDisplayObject() && Flash::Member(*a_clip, "parent"sv).IsDisplayObject(); });
	}

	bool TitleRowChanged(const Scaleform::GFx::Movie* a_movie)
	{
		// Asked each frame, so Find stays quiet in the trace.
		auto* const menu = g_row ? OpenBarter() : nullptr;
		auto        side = menu && menu->uiMovie.get() == a_movie ? Roles::TraderSide::Find(*menu, true) : std::nullopt;
		if (!side) {
			return false;
		}
		// Kept at once, so a refresh that never reaches Place is asked for
		// once per change, not each frame.
		auto now = RowOf(*side, g_row->each.has_value());
		if (now == *g_row) {
			return false;
		}
		g_row = std::move(now);
		return true;
	}
}
