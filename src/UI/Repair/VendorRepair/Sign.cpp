#include "UI/Repair/VendorRepair/Sign.h"

#include "Core/Settings/Settings.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Repair/VendorRepair/SignBlock.h"
#include "UI/Repair/VendorRepair/SignLine.h"
#include "UI/Repair/VendorRepair/Stock.h"
#include "UI/Roles/Roles.h"
#include "UI/Roles/TraderSide.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace VendorRepair::Sign
{
	namespace
	{
		using Scaleform::GFx::Value;
		using Flash::Box;

		// Where the block and the line are kept while they are out of the
		// display list, as the barter screen's class is dynamic, see
		// HINT_MEMBER in Button.cpp.
		constexpr std::string_view SIGN_MEMBER = "NECTraderSign"sv;
		constexpr std::string_view LINE_MEMBER = "NECTraderSignLine"sv;

		// What the player sees when the movie lacks a part the block, the
		// line or both need.
		constexpr auto NO_SIGN = "the barter screen shows no trader repair sign"sv;
		constexpr auto ONLY_ABOVE = "the trader repair sign shows only above the trader's list"sv;
		constexpr auto ONLY_UNDER = "the trader repair sign shows only under the trader's list"sv;

		// -------------------------------------------------------------------
		// The place
		// -------------------------------------------------------------------

		using Roles::TraderSide::Room;

		// The block's spot: its right edge on the bracket's, its top a gap
		// under the bracket. Nothing when its box does not read as numbers.
		[[nodiscard]] std::optional<Spot> Under(Value& a_sign, const Room& a_room)
		{
			const auto own = Roles::BoundsIn(a_sign, a_sign);
			const auto scale = Flash::Number(a_sign, "scaleX"sv);
			if (!own || !std::isfinite(scale) || !(scale > 0.0)) {
				return std::nullopt;
			}
			const auto top = a_room.bracket.bottom + GAP * a_room.unit;
			Spot       spot;
			spot.x = a_room.bracket.right - own->right * scale;
			spot.y = top - own->top * scale;
			spot.box = Box{ spot.x + own->left * scale, top, a_room.bracket.right, top + (own->bottom - own->top) * scale };
			return spot;
		}

		// Puts a_clip on a_side at a_spot.
		void Put(Value& a_side, Value& a_clip, const Spot& a_spot)
		{
			Flash::Set(a_clip, "x"sv, Value(a_spot.x));
			Flash::Set(a_clip, "y"sv, Value(a_spot.y));
			if (!Flash::Member(a_clip, "parent"sv).IsDisplayObject()) {
				Flash::Call(a_side, "addChild", std::array{ a_clip });
			}
		}

		// Takes a_clip out of the display list, if it is in, never only
		// hidden, so the side's colour never stretches over another part.
		void Off(Value& a_clip)
		{
			if (auto parent = Flash::Member(a_clip, "parent"sv); parent.IsDisplayObject()) {
				Flash::Call(parent, "removeChild", std::array{ a_clip });
			}
		}

		// A form of the line and whether it goes left of the frame.
		struct Pick
		{
			std::size_t form = 0;
			bool        beside = false;
		};

		// The line's forms for this screen, from Draw, the one its field holds
		// now and where, and the title row as Place last saw it while the
		// line was shown or left out for want of room, for TitleRowChanged.
		std::vector<Form>       g_forms;
		std::optional<Pick>     g_held;
		std::optional<TitleRow> g_row;

		// Takes the screen's block and line out and forgets them.
		void TakeOut(RE::BarterMenu& a_menu)
		{
			g_forms.clear();
			g_held.reset();
			g_row.reset();
			if (!a_menu.menuObj.IsObject()) {
				return;
			}
			for (const auto member : { SIGN_MEMBER, LINE_MEMBER }) {
				if (auto clip = Flash::Member(a_menu.menuObj, member); clip.IsDisplayObject()) {
					Off(clip);
					a_menu.menuObj.SetMember(member, Value());
				}
			}
		}

		// Where the sign was last put on this screen, as the trace says it,
		// so it says each change once.
		constexpr auto SHOWS_UNDER = "shows under the trader's list"sv;
		constexpr auto LEFT_OUT = "is left out, as it has no room under or above the trader's list or left of its frame"sv;
		// A screen with no line clip tries only the place under the list.
		constexpr auto LEFT_OUT_UNDER = "is left out, as it has no room under the trader's list"sv;
		// A screen with no block tries only the line's places.
		constexpr auto LEFT_OUT_LINE = "is left out, as it has no room above the trader's list or left of its frame"sv;

		std::optional<std::string_view> g_placed;

		void Say(RE::BarterMenu* a_menu, std::string_view a_where)
		{
			if (g_placed != a_where) {
				g_placed = a_where;
				TraceLog::Line("menu", "{:s}'s sign {:s}", Trader(a_menu), a_where);
			}
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
		TakeOut(*a_menu);
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

		// The block needs the card's scale and rows, the line only the
		// trader's title, so a movie that refuses the one still shows the
		// other. When both fail, the trace names only the block's part, so 1
		// line says no sign shows.
		const auto* card = a_menu->itemCard_mc.get();
		const auto  scale = card && card->IsDisplayObject() ? Flash::Number(*card, "scaleX"sv) : 0.0;
		auto        missing = "item card"sv;
		auto        sign = std::isfinite(scale) && scale > 0.0 ? Build(movie, *ceilings, scale, missing) : Value();
		auto        line = LineOf(movie, *side, *ceilings);
		if (sign.IsDisplayObject()) {
			a_menu->menuObj.SetMember(SIGN_MEMBER, sign);
		} else {
			Roles::Noted(movie, missing, line ? ONLY_ABOVE : NO_SIGN);
		}
		if (line) {
			a_menu->menuObj.SetMember(LINE_MEMBER, line->field);
			g_forms = std::move(line->forms);
		} else if (sign.IsDisplayObject()) {
			Roles::Noted(movie, "trader's title"sv, ONLY_UNDER);
		}
		if (sign.IsDisplayObject() || line) {
			TraceLog::Line("menu", "{:s}'s sign reads {:s}", Trader(a_menu), Reads(*ceilings));
		}
	}

	void Place(RE::BarterMenu* a_menu, const std::optional<Roles::Bars::Hints>& a_bar)
	{
		if (!a_menu || !a_menu->uiMovie || !a_menu->menuObj.IsObject()) {
			return;
		}
		auto sign = Flash::Member(a_menu->menuObj, SIGN_MEMBER);
		auto line = Flash::Member(a_menu->menuObj, LINE_MEMBER);
		g_row.reset();
		if (!sign.IsDisplayObject() && !line.IsDisplayObject()) {
			return;
		}
		// A script can turn the sign off while the screen is open.
		if (!Settings::bTraderSign.GetValue()) {
			TakeOut(*a_menu);
			return;
		}
		auto side = Roles::TraderSide::Find(*a_menu);
		auto room = side && a_bar ? Roles::TraderSide::RoomOf(*a_menu->uiMovie, *side, a_bar->bar) : std::nullopt;
		if (!room) {
			Off(sign);
			Off(line);
			return;
		}

		// The block under the list first, where the movie could build it,
		// then the line above it.
		if (const auto under = sign.IsDisplayObject() ? Under(sign, *room) : std::nullopt; under && room->Fits(under->box, GAP, GAP)) {
			Put(side->side, sign, *under);
			Off(line);
			Say(a_menu, SHOWS_UNDER);
			return;
		}
		Off(sign);
		if (!line.IsDisplayObject()) {
			Say(a_menu, LEFT_OUT_UNDER);
			return;
		}

		// The form shown last while it still fits where it is, so a title row
		// that grows and shrinks back leaves the sign as it is, else the first
		// that fits above the list, then the first left of the frame, where
		// the rest of the screen is read only when it is needed.
		auto       around = false;
		const auto fits = [&](const Pick& a_pick) {
			if (a_pick.beside && !around) {
				Roles::TraderSide::AroundOf(*side, *room);
				around = true;
			}
			return room->Fits(SpotOf(g_forms[a_pick.form], *room, a_pick.beside).box, GAP, a_pick.beside ? GAP_UNDER_LEFT : GAP, a_pick.beside);
		};
		auto pick = g_held && fits(*g_held) ? g_held : std::nullopt;
		for (const auto beside : { false, true }) {
			for (std::size_t i = 0; !pick && i < g_forms.size(); i++) {
				if (fits(Pick{ i, beside })) {
					pick = Pick{ i, beside };
				}
			}
		}
		if (pick && ((g_held && g_held->form == pick->form) || Flash::Set(line, "htmlText"sv, Value(g_forms[pick->form].html.c_str())))) {
			g_held = pick;
			ColorLine(line, *side);
			Put(side->side, line, SpotOf(g_forms[pick->form], *room, pick->beside));
			Say(a_menu, pick->beside ? g_forms[pick->form].saidLeft : g_forms[pick->form].said);
		} else {
			Off(line);
			Say(a_menu, sign.IsDisplayObject() ? LEFT_OUT : LEFT_OUT_LINE);
		}
		// Read after the line went in or out, as the side counts it, with each
		// part's look while the place left of the frame is in play.
		g_row = RowOf(*side, around);
	}

	void Forget(RE::BarterMenu* a_menu)
	{
		if (a_menu) {
			TakeOut(*a_menu);
		}
		g_placed.reset();
	}

	bool Shown(RE::BarterMenu* a_menu)
	{
		if (!a_menu) {
			return false;
		}
		return std::ranges::any_of(std::array{ SIGN_MEMBER, LINE_MEMBER }, [&](std::string_view a_member) {
			const auto clip = Flash::Member(a_menu->menuObj, a_member);
			return clip.IsDisplayObject() && Flash::Member(clip, "parent"sv).IsDisplayObject();
		});
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
