#include "UI/Repair/VendorRepair/SignMove.h"

#include "Core/Settings/Settings.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Repair/VendorRepair/SignBack.h"
#include "UI/Repair/VendorRepair/Stock.h"

#include <cmath>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace VendorRepair::Sign
{
	namespace
	{
		using Scaleform::GFx::Value;
		using Roles::TraderSide::Room;

		// Where the moved sign's clip is kept, as the panel is, see
		// SignPlace.cpp, and its name.
		constexpr std::string_view MOVED_MEMBER = "NECTraderSignMoved"sv;
		constexpr const char*      MOVED_NAME = "NEC_TraderSignMoved_mc";

		// -------------------------------------------------------------------
		// How far it moves
		// -------------------------------------------------------------------

		// A value that is no number counts as 0, as the Pickup CND's does.
		[[nodiscard]] double Offset(const Settings::Live<float>& a_setting)
		{
			const auto value = a_setting.GetValue();
			return std::isfinite(value) ? value : 0.0;
		}

		// a_spot moved a_by, then pulled back until the screen holds all of
		// its box, how far it went in the end and whether it was pulled back.
		// Where the box is larger than the screen, its left or top edge
		// stays on.
		struct Moved
		{
			Spot spot;
			Move by;
			bool pulled = false;
		};

		[[nodiscard]] Moved MovedOf(const Spot& a_spot, const Move& a_by, const Room& a_room)
		{
			const auto& frame = *a_room.frame;
			const auto  pulled = [](double a_shift, double a_low, double a_high, double a_least, double a_most) {
				if (a_high + a_shift > a_most) {
					a_shift = a_most - a_high;
				}
				return a_low + a_shift < a_least ? a_least - a_low : a_shift;
			};
			// The movie's y runs down, so up is taken off.
			const auto across = a_by.x * a_room.unit;
			const auto down = -a_by.y * a_room.unit;
			const auto x = pulled(across, a_spot.box.left, a_spot.box.right, frame.left, frame.right);
			const auto y = pulled(down, a_spot.box.top, a_spot.box.bottom, frame.top, frame.bottom);
			const auto held = x != across || y != down;
			return { Spot{ a_spot.x + x, a_spot.y + y, a_spot.box.Moved(x, y) }, held ? Move{ x / a_room.unit, -y / a_room.unit } : a_by, held };
		}

		// "20 right and 30 up", to a tenth.
		[[nodiscard]] std::string Said(const Move& a_move)
		{
			const auto tenth = [](double a_value) { return std::abs(std::round(a_value * 10.0) / 10.0); };
			return std::format("{:g} {:s} and {:g} {:s}", tenth(a_move.x), a_move.x < 0.0 ? "left" : "right", tenth(a_move.y), a_move.y < 0.0 ? "down" : "up");
		}

		// -------------------------------------------------------------------
		// The clip that holds it
		// -------------------------------------------------------------------

		// The clip a moved sign goes in, made once a Draw: right above the
		// side in its parent, so the sign keeps the depth it has in the side,
		// with the side's place, scale and look, so the sign's units stay the
		// side's. Not a display object when the movie refuses a step.
		[[nodiscard]] Value HolderOf(RE::BarterMenu& a_menu, Roles::TraderSide::Side& a_side)
		{
			auto& side = a_side.side;
			if (!Flash::Member(side, "parent"sv).IsDisplayObject()) {
				return {};
			}
			auto holder = Flash::Member(a_menu.menuObj, MOVED_MEMBER);
			if (!holder.IsDisplayObject()) {
				a_menu.uiMovie->CreateObject(&holder, "flash.display.Sprite");
				const auto made = holder.IsDisplayObject() && Flash::Set(holder, "name"sv, Value(MOVED_NAME)) &&
				                  Flash::Set(holder, "mouseEnabled"sv, Value(false)) && Flash::Set(holder, "mouseChildren"sv, Value(false));
				if (!made) {
					return {};
				}
				a_menu.menuObj.SetMember(MOVED_MEMBER, holder);
			}
			for (const auto& [key, absent] : { std::pair{ "x"sv, 0.0 }, std::pair{ "y"sv, 0.0 }, std::pair{ "scaleX"sv, 1.0 }, std::pair{ "scaleY"sv, 1.0 },
					 std::pair{ "alpha"sv, 1.0 } }) {
				Flash::Set(holder, key, Value(Flash::Number(side, key, absent)));
			}
			Flash::Set(holder, "visible"sv, Value(Flash::Bool(side, "visible"sv)));
			if (!Flash::Member(holder, "parent"sv).IsDisplayObject() && !Roles::TraderSide::AddAbove(a_side, holder)) {
				return {};
			}
			return holder;
		}

		// Gives the holder its own colour target, so the engine draws it, and
		// then the side's own colour. The target writes the HUD colour into
		// the clip, as the engine's does into the side, but a movie can give
		// the side another or none and colour its texts itself, and the sign
		// keeps the colour it shows at its own place.
		void ColorHolder(RE::BarterMenu& a_menu, Roles::TraderSide::Side& a_side, Value& a_holder)
		{
			SignBack::Make(a_menu, a_holder, Trader(&a_menu), SignBack::Target::kColor);
			const auto side = Flash::TintOf(a_side.side);
			if (const auto now = Flash::TintOf(a_holder); side && (!now || !now->Near(*side))) {
				Flash::SetTint(a_holder, *side);
			}
		}
	}

	// -------------------------------------------------------------------
	// The place the player moves it to
	// -------------------------------------------------------------------

	std::optional<Move> MoveOf(const Room& a_room)
	{
		const Move move{ Offset(Settings::fTraderSignX), Offset(Settings::fTraderSignY) };
		if (move == Move{}) {
			return std::nullopt;
		}
		// The holder's colour target needs the hook's sure end, as the
		// panel's background does.
		if (!SignBack::Held()) {
			TraceLog::Once("menu", "NEC does not hold the barter screen delete, so the trader repair sign stays at its own place, whatever fTraderSignX and fTraderSignY say");
			return std::nullopt;
		}
		return a_room.frame ? std::optional{ move } : std::nullopt;
	}

	std::optional<bool> PlaceMoved(RE::BarterMenu& a_menu, Roles::TraderSide::Side& a_side, Room& a_room, Clips& a_clips, const std::optional<Choice>& a_own,
		const Move& a_by)
	{
		const auto choice = a_own ? a_own : FirstOf(a_room, a_clips);
		if (!choice) {
			MovedOff(a_menu, COMES_OUT);
			return LeftOut(a_menu, a_room, a_clips);
		}
		auto holder = HolderOf(a_menu, a_side);
		if (!holder.IsDisplayObject()) {
			return std::nullopt;
		}
		const auto moved = MovedOf(choice->spot, a_by, a_room);
		const auto where = Show(a_menu, a_side, a_clips, Choice{ choice->pick, moved.spot }, holder);
		if (!where) {
			MovedOff(a_menu, COMES_OUT);
			LeftOut(a_menu, a_room, a_clips);
			return true;
		}
		ColorHolder(a_menu, a_side, holder);
		auto said = std::format("{:s}, moved {:s}", *where, Said(moved.by));
		if (moved.pulled) {
			said += std::format(", the most the screen holds of {:s}", Said(a_by));
		}
		if (!a_own) {
			said += ", though its own place has no room";
		}
		Say(&a_menu, said);
		return choice->pick.has_value();
	}

	void MovedOff(RE::BarterMenu& a_menu, std::string_view a_how)
	{
		auto holder = Flash::Member(a_menu.menuObj, MOVED_MEMBER);
		Off(holder);
		SignBack::Drop(a_menu, a_how, SignBack::Target::kColor);
	}

	void ForgetMoved(RE::BarterMenu& a_menu)
	{
		if (auto holder = Flash::Member(a_menu.menuObj, MOVED_MEMBER); holder.IsDisplayObject()) {
			Off(holder);
			a_menu.menuObj.SetMember(MOVED_MEMBER, Value());
		}
	}
}
