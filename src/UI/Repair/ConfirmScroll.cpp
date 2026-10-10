#include "UI/Repair/ConfirmScroll.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Roles/Boxes.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>

namespace ConfirmScroll
{
	namespace
	{
		using RE::DIRECTION_VAL;
		using Params = Scaleform::GFx::FunctionHandler::Params;
		using Value = Scaleform::GFx::Value;

		// -------------------------------------------------------------------
		// Buttons and the pace they scroll at
		// -------------------------------------------------------------------

		// What the game does before this. The box has a function table as a
		// menu, for Call and ProcessMessage, and one as an input user.
		REL::Relocation<void (*)(RE::ExamineConfirmMenu*, const Params&)>                    _Call;
		REL::Relocation<RE::UI_MESSAGE_RESULTS (*)(RE::ExamineConfirmMenu*, RE::UIMessage&)> _ProcessMessage;
		REL::Relocation<bool (*)(RE::BSInputEventUser*, const RE::InputEvent*)>              _ShouldHandleEvent;
		REL::Relocation<void (*)(RE::BSInputEventUser*, const RE::ButtonEvent*)>             _OnButtonEvent;

		// Whether each place that scrolls runs, see CallPatch::PatchSlot.
		CallPatch::LinkBase g_messagesLink;
		CallPatch::LinkBase g_handlesLink;
		CallPatch::LinkBase g_keysLink;

		// The numbers the movie calls the box's own 2 buttons by, from
		// ExamineConfirmMenu::MapCodeObjectFunctions.
		constexpr std::uintptr_t ACCEPT_PRESS = 0;
		constexpr std::uintptr_t CANCEL_PRESS = 1;

		// What menu mode calls the arrow keys and the D-pad. W and S have no
		// name in menu mode and keep the ones they have in play.
		constexpr auto UP = "Up"sv;
		constexpr auto DOWN = "Down"sv;
		constexpr auto FORWARD = "Forward"sv;
		constexpr auto BACK = "Back"sv;

		// The pace a held key repeats at in the game's own lists, the game's
		// fKeyboardRepeatDelay and fKeyboardRepeatRate, read each time a key is
		// held, or 0.3 and 0.05 seconds where a setting is missing.
		[[nodiscard]] float Pace(std::string_view a_name, float a_missing)
		{
			auto* const       settings = RE::INISettingCollection::GetSingleton();
			const auto* const setting = settings ? settings->GetSetting(a_name) : nullptr;
			return setting ? setting->GetFloat() : a_missing;
		}

		// The panel's own spacing, from ConfirmPanel.as: 5 under every entry
		// and 20 between the list and the buttons.
		constexpr double ENTRY_GAP = 5.0;
		constexpr double BUTTON_GAP = 20.0;

		// How far a grown box stays from the screen's top and bottom, and above
		// the bench's button bar, in the movie's units, 720 down a 16:9 screen.
		constexpr double SCREEN_MARGIN = 50.0;
		constexpr double BAR_GAP = 10.0;

		// How long the key was held at its last repeat, 0 before its first.
		float g_lastRepeat = 0.0F;

		// Which way a button scrolls the list, or kNone for any other button.
		[[nodiscard]] DIRECTION_VAL Way(const RE::ButtonEvent& a_event)
		{
			// The wheel has no name in menu mode, so it is known by its code.
			if (*a_event.device == RE::INPUT_DEVICE::kMouse) {
				const auto code = static_cast<RE::BS_BUTTON_CODE>(a_event.idCode);
				if (code == RE::BS_BUTTON_CODE::kWheelUp) {
					return DIRECTION_VAL::kUp;
				}
				if (code == RE::BS_BUTTON_CODE::kWheelDown) {
					return DIRECTION_VAL::kDown;
				}
				return DIRECTION_VAL::kNone;
			}

			const auto& name = a_event.QUserEvent();
			if (name == UP) {
				return DIRECTION_VAL::kUp;
			}
			if (name == DOWN) {
				return DIRECTION_VAL::kDown;
			}

			// The game's own rule for W and S: arrows while an open menu asks
			// for it, as every workbench does, and not while typing. The name
			// is read raw, as the game reads it.
			const auto* const ui = RE::UI::GetSingleton();
			const auto* const controls = RE::ControlMap::GetSingleton();
			if (*a_event.device == RE::INPUT_DEVICE::kKeyboard && ui && ui->movementToDirectionalCount > 0 &&
				controls && controls->byTextEntryCount == 0) {
				const auto& raw = a_event.QRawUserEvent();
				if (raw == FORWARD) {
					return DIRECTION_VAL::kUp;
				}
				if (raw == BACK) {
					return DIRECTION_VAL::kDown;
				}
			}
			return DIRECTION_VAL::kNone;
		}

		// Whether a button is due a step: a press is one, a release is none,
		// and holding repeats at the pace above. A turn of the wheel arrives as
		// a press and a release together, so each turn is one step.
		[[nodiscard]] bool Due(const RE::ButtonEvent& a_event)
		{
			if (!a_event.QPressed()) {
				g_lastRepeat = 0.0F;
				return false;
			}
			if (a_event.QJustPressed()) {
				g_lastRepeat = 0.0F;
				return true;
			}

			const auto next = g_lastRepeat == 0.0F ? Pace("fKeyboardRepeatDelay:Controls"sv, 0.3F) :
			                                         g_lastRepeat + Pace("fKeyboardRepeatRate:Controls"sv, 0.05F);
			if (a_event.QHeldDownSecs() < next) {
				return false;
			}
			g_lastRepeat = a_event.QHeldDownSecs();
			return true;
		}

		// -------------------------------------------------------------------
		// Naming things for the trace
		// -------------------------------------------------------------------

		// What a button was pressed on, for the trace.
		[[nodiscard]] std::string_view DeviceName(const RE::ButtonEvent& a_event)
		{
			switch (*a_event.device) {
			case RE::INPUT_DEVICE::kKeyboard:
				return "keyboard"sv;
			case RE::INPUT_DEVICE::kMouse:
				return "mouse"sv;
			case RE::INPUT_DEVICE::kGamepad:
				return "gamepad"sv;
			case RE::INPUT_DEVICE::kVirtualKeyboard:
				return "virtual keyboard"sv;
			default:
				return "unknown device"sv;
			}
		}

		// Which box a trace line is about, by where it sits in memory, so a
		// line from a box nobody sees shows whether it is the last one opened.
		[[nodiscard]] std::uintptr_t Id(const RE::ExamineConfirmMenu* a_menu)
		{
			return reinterpret_cast<std::uintptr_t>(a_menu);
		}

		// A button as the trace names it: the device, its code there and its
		// raw name, the one W and S still carry in a menu.
		[[nodiscard]] std::string Named(const RE::ButtonEvent& a_event)
		{
			const auto* name = a_event.QRawUserEvent().c_str();
			return std::format("{:s} {:#x} {:s}", DeviceName(a_event), a_event.QIDCode(), *name ? name : "unnamed");
		}

		// -------------------------------------------------------------------
		// Growing the box
		// -------------------------------------------------------------------

		// Moves the panel's list one row, as a push of the stick does. A step
		// with nothing further to show does nothing.
		bool Step(Value& a_panel, DIRECTION_VAL a_way)
		{
			return a_panel.Invoke("onLeftThumbstickInput", std::array{ Value(static_cast<std::uint32_t>(a_way)) });
		}

		// Grows the box to show as much of its list as the screen has room for,
		// carrying on from where Build cut the list off at 400 and moving the
		// box up by half of the growth, as Build does to keep it centred.
		void Grow(RE::ExamineConfirmMenu& a_menu)
		{
			auto parts = Roles::Boxes::ConfirmPanel(a_menu);
			if (!parts) {
				return;
			}
			auto&      panel = a_menu.confirmObj;
			auto&      background = parts->background;
			auto&      buttons = parts->buttons;
			auto&      arrow = parts->arrow;
			const auto end = parts->listEnd + ENTRY_GAP;
			const auto cut = Flash::Number(buttons, "y"sv) - BUTTON_GAP;

			// Where the box sits on the stage, and the part of the stage the
			// screen shows.
			const auto bounds = Flash::StageBounds(background);
			if (!bounds) {
				TraceLog::Line("menu", "Confirmation box left at its size, its place on the screen was not found");
				return;
			}
			const auto top = Flash::Number(*bounds, "y"sv);
			const auto height = Flash::Number(*bounds, "height"sv);
			const auto shown = a_menu.uiMovie->GetVisibleFrameRect();

			// The bottom stops above the bench's button bar, or short of the
			// screen's bottom edge where that is higher or no bar is shown.
			const auto bar = Roles::Boxes::BenchBarTop();
			const auto edge = shown.y2 - SCREEN_MARGIN;
			const auto bottom = bar ? std::min(edge, shown.y1 + (*bar * (shown.y2 - shown.y1)) - BAR_GAP) : edge;
			const auto limit = bottom < edge ? std::format("its bottom kept above the bench's button bar at {:.1f}", bottom) :
			                                   std::format("its bottom kept {:g} from the bottom of the screen, {:s}", SCREEN_MARGIN,
												   bar ? "the bench's button bar starts lower"sv : "no bench button bar is shown"sv);

			const auto room = 2.0 * std::min(top - (shown.y1 + SCREEN_MARGIN), bottom - (top + height));
			const auto grow = std::min(end - cut, room);
			if (grow <= 0.0) {
				TraceLog::Line("menu", "Confirmation box left at its size, no room for more of its list with {:s}", limit);
				return;
			}

			background.SetMember("height"sv, Value(Flash::Number(background, "height"sv) + grow));
			buttons.SetMember("y"sv, Value(Flash::Number(buttons, "y"sv) + grow));
			arrow.SetMember("y"sv, Value(Flash::Number(arrow, "y"sv) + grow));
			panel.SetMember("y"sv, Value(Flash::Number(panel, "y"sv) - (grow / 2.0)));

			// Build hid the rows past the old bottom, and only a scroll clips
			// them again. A step down and back up shows the rows that now fit.
			Step(panel, DIRECTION_VAL::kDown);
			Step(panel, DIRECTION_VAL::kUp);

			TraceLog::Line("menu", "Confirmation box grown by {:.1f} to {:.1f} high, {:s}, {:s}", grow, height + grow,
				grow < end - cut ? "as far as there is room" : "enough for its whole list", limit);
		}

		// -------------------------------------------------------------------
		// The hooks
		// -------------------------------------------------------------------

		// The box's 2 buttons clicked, so the trace tells a click from a key.
		void CallHk(RE::ExamineConfirmMenu* a_menu, const Params& a_params)
		{
			const auto pressed = reinterpret_cast<std::uintptr_t>(a_params.userData);
			if (TraceLog::IsOpen() && (pressed == ACCEPT_PRESS || pressed == CANCEL_PRESS)) {
				TraceLog::Line("menu", "Confirmation box {:X} had its own {:s} button clicked", Id(a_menu),
					pressed == ACCEPT_PRESS ? "accept" : "cancel");
			}
			_Call(a_menu, a_params);
		}

		// Every message to the box. The one that opens it has the panel filled
		// and laid out by Build before it returns. Opening and closing are
		// traced, since a short box leaves no other line.
		RE::UI_MESSAGE_RESULTS ProcessMessageHk(RE::ExamineConfirmMenu* a_menu, RE::UIMessage& a_message)
		{
			const auto result = _ProcessMessage(a_menu, a_message);
			if (!a_menu) {
				return result;
			}
			switch (*a_message.type) {
			case RE::UI_MESSAGE_TYPE::kShow:
				if (TraceLog::IsOpen()) {
					TraceLog::Line("menu", "Confirmation box {:X} opens, asking \"{:s}\"", Id(a_menu), Roles::Boxes::Question(*a_menu));
				}
				if (Settings::bConfirmScroll.GetValue() && g_messagesLink.Live()) {
					Grow(*a_menu);
				}
				break;
			case RE::UI_MESSAGE_TYPE::kHide:
			case RE::UI_MESSAGE_TYPE::kForceHide:
				TraceLog::Line("menu", "Confirmation box {:X} closes", Id(a_menu));
				break;
			default:
				break;
			}
			return result;
		}

		// Asked about every input while the box is open. The game takes
		// Accept, Cancel, Activate and the left stick and nothing else.
		bool ShouldHandleEventHk(RE::BSInputEventUser* a_this, const RE::InputEvent* a_event)
		{
			const auto* const button = a_event->As<RE::ButtonEvent>();
			return (Settings::bConfirmScroll.GetValue() && g_handlesLink.Live() && button && Way(*button) != DIRECTION_VAL::kNone) ||
			       _ShouldHandleEvent(a_this, a_event);
		}

		// A button the box took. The game answers Accept and Cancel as they are
		// released, so a scrolling button never reaches it.
		void OnButtonEventHk(RE::BSInputEventUser* a_this, const RE::ButtonEvent* a_event)
		{
			auto*      menu = static_cast<RE::ExamineConfirmMenu*>(a_this);
			const auto way = Settings::bConfirmScroll.GetValue() && g_keysLink.Live() ? Way(*a_event) : DIRECTION_VAL::kNone;
			if (way == DIRECTION_VAL::kNone) {
				// Said on a release, before the game's answer closes the box.
				if (a_event->QReleased() && TraceLog::IsOpen()) {
					TraceLog::Line("menu", "Confirmation box {:X} heard {:s} let go on the {:s}",
						Id(menu), a_event->QUserEvent().c_str(), Named(*a_event));
				}
				_OnButtonEvent(a_this, a_event);
				return;
			}
			if (!Due(*a_event)) {
				return;
			}

			// The panel is the clip the game hands the stick to, kept in
			// confirmObj from the moment the box opens.
			auto&      panel = menu->confirmObj;
			const auto sent = panel.IsObject() && Step(panel, way);
			if (TraceLog::IsOpen()) {
				TraceLog::Line("menu", "Confirmation box {:X} told to scroll {:s} by the {:s}{:s}",
					Id(menu), way == DIRECTION_VAL::kUp ? "up" : "down", Named(*a_event),
					sent ? "" : ", but its panel has no onLeftThumbstickInput");
			}
		}
	}

	void Install()
	{
		REL::Relocation<std::uintptr_t> menu{ RE::ExamineConfirmMenu::VTABLE[0] };
		REL::Relocation<std::uintptr_t> input{ RE::ExamineConfirmMenu::VTABLE[1] };

		const auto messages = CallPatch::PatchSlot(menu, 0x03, ProcessMessageHk, "confirm box messages", Part::kNone, true, &g_messagesLink);
		const auto handles = CallPatch::PatchSlot(input, 0x01, ShouldHandleEventHk, "confirm box input", Part::kNone, true, &g_handlesLink);
		const auto keys = CallPatch::PatchSlot(input, 0x08, OnButtonEventHk, "confirm box keys", Part::kNone, true, &g_keysLink);
		_Call = CallPatch::PatchSlot(menu, 0x01, CallHk, "confirm box clicks", Part::kTrace).value_or(0);
		if (!messages || !handles || !keys) {
			return;
		}
		_ProcessMessage = *messages;
		_ShouldHandleEvent = *handles;
		_OnButtonEvent = *keys;

		REX::INFO("A workbench's confirmation box grows to show as much of a long list as the screen allows, "
				  "and scrolls the rest with the mouse wheel, the arrow keys, the D-pad and W and S, as well as the left stick.");
	}
}
