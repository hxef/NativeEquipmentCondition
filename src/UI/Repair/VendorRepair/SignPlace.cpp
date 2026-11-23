#include "UI/Repair/VendorRepair/SignPlace.h"

#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Repair/VendorRepair/SignBack.h"
#include "UI/Repair/VendorRepair/SignBlock.h"
#include "UI/Repair/VendorRepair/Stock.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace VendorRepair::Sign
{
	namespace
	{
		using Scaleform::GFx::Value;
		using Roles::TraderSide::Room;

		// Where the panel, its back clip, the line and the line's frame are
		// kept while they are out of the display list, as the barter screen's
		// class is dynamic, see HINT_MEMBER in Button.cpp.
		constexpr std::string_view SIGN_MEMBER = "NECTraderSign"sv;
		constexpr std::string_view BACK_MEMBER = "NECTraderSignBack"sv;
		constexpr std::string_view LINE_MEMBER = "NECTraderSignLine"sv;
		constexpr std::string_view FRAME_MEMBER = "NECTraderSignFrame"sv;
		constexpr std::array MEMBERS{ SIGN_MEMBER, BACK_MEMBER, LINE_MEMBER, FRAME_MEMBER };

		// Where the sign was last put on this screen, as the trace says it,
		// so it says each change once.
		constexpr auto SHOWS_UNDER = "shows under the trader's list"sv;
		constexpr auto LEFT_OUT = "is left out, as it has no room under or above the trader's list or left of its frame"sv;
		// A screen with no line clip tries only the place under the list.
		constexpr auto LEFT_OUT_UNDER = "is left out, as it has no room under the trader's list"sv;
		// A screen with no panel tries only the line's places.
		constexpr auto LEFT_OUT_LINE = "is left out, as it has no room above the trader's list or left of its frame"sv;

		std::optional<std::string> g_placed;

		// The line's forms for this screen, from Draw, the form its field
		// holds now and where, and the form whose lines the frame holds,
		// LineOf's first.
		Forms                      g_line;
		std::optional<Pick>        g_held;
		std::optional<std::size_t> g_framed;

		// Puts a_clip in a_into, the side or the moved sign's clip, at a_spot.
		// Only the side and that clip ever hold it, and their names differ.
		void Put(Value& a_into, Value& a_clip, const Spot& a_spot)
		{
			Flash::Set(a_clip, "x"sv, Value(a_spot.x));
			Flash::Set(a_clip, "y"sv, Value(a_spot.y));
			const auto parent = Flash::Member(a_clip, "parent"sv);
			if (!parent.IsDisplayObject() || Flash::String(parent, "name"sv) != Flash::String(a_into, "name"sv)) {
				Flash::Call(a_into, "addChild", std::array{ a_clip });
			}
		}

		// Whether the screen can show the panel: it was built, the side shows
		// the engine's background and the list's frame lines, and NEC holds
		// the delete hook.
		[[nodiscard]] bool PanelCan(const Room& a_room, const Clips& a_clips)
		{
			return a_clips.sign.IsDisplayObject() && a_room.backed && a_room.lines && SignBack::Held();
		}

		// Whether the screen can show the framed line: where the movie hides
		// the engine's background and draws the list's frame lines.
		[[nodiscard]] bool Framable(const Room& a_room, const Clips& a_clips)
		{
			return a_clips.frame.IsDisplayObject() && !a_room.backed && a_room.lines;
		}
	}

	// -------------------------------------------------------------------
	// The clips and the trace
	// -------------------------------------------------------------------

	Clips ClipsOf(RE::BarterMenu& a_menu)
	{
		return { Flash::Member(a_menu.menuObj, SIGN_MEMBER), Flash::Member(a_menu.menuObj, BACK_MEMBER), Flash::Member(a_menu.menuObj, LINE_MEMBER),
			Flash::Member(a_menu.menuObj, FRAME_MEMBER) };
	}

	void Keep(RE::BarterMenu& a_menu, const Clips& a_clips, Forms a_line)
	{
		const std::array clips{ std::pair{ SIGN_MEMBER, &a_clips.sign }, std::pair{ BACK_MEMBER, &a_clips.back }, std::pair{ LINE_MEMBER, &a_clips.line },
			std::pair{ FRAME_MEMBER, &a_clips.frame } };
		for (const auto& [member, clip] : clips) {
			if (clip->IsDisplayObject()) {
				a_menu.menuObj.SetMember(member, *clip);
			}
		}
		g_line = std::move(a_line);
		g_framed = a_clips.frame.IsDisplayObject() ? std::optional<std::size_t>{ 0 } : std::nullopt;
	}

	void Clear(RE::BarterMenu& a_menu)
	{
		g_line = {};
		g_held.reset();
		g_framed.reset();
		if (!a_menu.menuObj.IsObject()) {
			return;
		}
		for (const auto member : MEMBERS) {
			if (auto clip = Flash::Member(a_menu.menuObj, member); clip.IsDisplayObject()) {
				Off(clip);
				a_menu.menuObj.SetMember(member, Value());
			}
		}
	}

	// Never only hidden, so the side's colour never stretches over another
	// part.
	void Off(Value& a_clip)
	{
		if (auto parent = Flash::Member(a_clip, "parent"sv); parent.IsDisplayObject()) {
			Flash::Call(parent, "removeChild", std::array{ a_clip });
		}
	}

	void PanelOff(RE::BarterMenu& a_menu, Clips& a_clips, std::string_view a_how)
	{
		Off(a_clips.sign);
		SignBack::Drop(a_menu, a_how, SignBack::Target::kBack);
	}

	void Say(RE::BarterMenu* a_menu, std::string_view a_where)
	{
		if (g_placed != a_where) {
			g_placed = a_where;
			TraceLog::Line("menu", "{:s}'s sign {:s}", Trader(a_menu), a_where);
		}
	}

	void ForgetSaid()
	{
		g_placed.reset();
	}

	// -------------------------------------------------------------------
	// The form and its own place
	// -------------------------------------------------------------------

	std::optional<Choice> Choose(Roles::TraderSide::Side& a_side, Room& a_room, Clips& a_clips, bool& a_around)
	{
		const auto panel = PanelCan(a_room, a_clips) ? UnderOf(a_clips.sign, a_room) : std::nullopt;
		if (panel && a_room.Fits(panel->box, GAP, GAP) && a_room.OverBar(panel->box, GAP)) {
			return Choice{ std::nullopt, *panel };
		}
		if (!a_clips.line.IsDisplayObject() || g_line.forms.empty()) {
			return std::nullopt;
		}
		const auto pick = PickOf(g_line, g_held, Framable(a_room, a_clips), a_side, a_room, a_around);
		return pick ? std::optional{ Choice{ pick, SpotFor(g_line, *pick, a_room) } } : std::nullopt;
	}

	std::optional<Choice> FirstOf(Room& a_room, Clips& a_clips)
	{
		if (PanelCan(a_room, a_clips)) {
			if (const auto panel = UnderOf(a_clips.sign, a_room)) {
				return Choice{ std::nullopt, *panel };
			}
		}
		if (!a_clips.line.IsDisplayObject() || g_line.forms.empty()) {
			return std::nullopt;
		}
		const Pick pick{ 0, false, Framable(a_room, a_clips) };
		return Choice{ pick, SpotFor(g_line, pick, a_room) };
	}

	std::optional<std::string_view> Show(RE::BarterMenu& a_menu, Roles::TraderSide::Side& a_side, Clips& a_clips, const Choice& a_choice, Value& a_into)
	{
		if (!a_choice.pick) {
			Off(a_clips.line);
			Off(a_clips.frame);
			ColorLine(a_clips.sign, a_side);
			Put(a_into, a_clips.sign, a_choice.spot);
			SignBack::Make(a_menu, a_clips.back, Trader(&a_menu), SignBack::Target::kBack);
			return SHOWS_UNDER;
		}
		const auto& pick = *a_choice.pick;
		const auto& form = g_line.forms[pick.form];
		if (!(g_held && g_held->form == pick.form) && !Flash::Set(a_clips.line, "htmlText"sv, Value(form.html.c_str()))) {
			return std::nullopt;
		}
		// The field holds the new form's text from here, so a refused frame
		// leaves no form held. The frame is then dropped for this screen, so
		// the next Place tries only the plain forms.
		if (pick.framed && g_framed != pick.form) {
			g_framed.reset();
			if (!Reframe(a_clips.frame, form)) {
				a_menu.menuObj.SetMember(FRAME_MEMBER, Value());
				g_held.reset();
				return std::nullopt;
			}
			g_framed = pick.form;
		}
		PanelOff(a_menu, a_clips, COMES_OUT);
		g_held = pick;
		ColorLine(a_clips.line, a_side);
		Put(a_into, a_clips.line, a_choice.spot);
		if (pick.framed) {
			ColorLine(a_clips.frame, a_side);
			Put(a_into, a_clips.frame, a_choice.spot);
		} else {
			Off(a_clips.frame);
		}
		return pick.framed ? form.saidFramed : (pick.beside ? form.saidLeft : form.said);
	}

	bool LeftOut(RE::BarterMenu& a_menu, const Room& a_room, Clips& a_clips)
	{
		PanelOff(a_menu, a_clips, COMES_OUT);
		Off(a_clips.line);
		Off(a_clips.frame);
		if (!a_clips.line.IsDisplayObject()) {
			Say(&a_menu, LEFT_OUT_UNDER);
			return false;
		}
		Say(&a_menu, PanelCan(a_room, a_clips) ? LEFT_OUT : LEFT_OUT_LINE);
		return true;
	}
}
