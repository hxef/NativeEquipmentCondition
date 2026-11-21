#include "UI/Mcm/Mcm.h"

#include "Core/Presets/Presets.h"
#include "Core/Text/Menu/MenuPresets.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Mcm/Bridge.h"
#include "UI/MessageBox.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The Reset preset and Delete preset buttons on NEC's main page. A button's
// CallExternalFunction runs f4se.plugins["Native Equipment
// Condition"].ResetPreset() in MCM's movie, the object F4SE gives NEC as the
// movie loads, so NEC puts its 2 functions there. Each asks first in NEC's
// box, whose last button is CANCEL, so Tab, Escape and the Cancel key cancel
// too, see MessageBox::Install. Only the yes button drops anything. The box
// that says a delete or a save failed is here too, with OK only.
namespace Mcm
{
	namespace
	{
		using Scaleform::GFx::Value;

		// Vanilla's own button words, which the game translates.
		constexpr const char* RESET_WORD = "$RESET";
		constexpr const char* DELETE_WORD = "$DELETE";
		constexpr const char* CANCEL_WORD = "$CANCEL";
		constexpr const char* OK_WORD = "$OK";

		// Which button called, as the function's userData.
		enum class Button : std::uintptr_t
		{
			kReset,
			kDelete,
		};

		// What NEC's box calls back into. Button 0 says yes and runs a_yes on
		// the task queue, outside the box's own answer, so a box it opens
		// queues after this one. Any other button cancels.
		class Answered final : public MessageBox::Callback
		{
		public:
			explicit Answered(std::function<void()> a_yes) :
				yes(std::move(a_yes))
			{}

			void Answer(std::uint8_t a_button) override
			{
				if (a_button != 0 || !yes) {
					return;
				}
				if (const auto* tasks = F4SE::GetTaskInterface()) {
					tasks->AddTask(yes);
				} else {
					yes();
				}
			}

			F4_HEAP_REDEFINE_NEW(Answered);

			std::function<void()> yes;
		};

		// The picked preset's name without (Custom), Default in the player's
		// language.
		std::string PickedShown()
		{
			return Presets::PickedStem().empty() ? Text::MenuPresetDefault() : Presets::PickedName();
		}

		// After RESET, while the preset the box asked about is still picked
		// and still has changes.
		void Reset(const std::string& a_stem)
		{
			WriteOwed("reset");
			if (Presets::PickedStem() != a_stem || !Presets::PickedHasChanges()) {
				return;
			}
			for (const auto* setting : Presets::Reset()) {
				Followed(*setting);
			}
			RefreshSoon();
		}

		// After DELETE, while the own preset the box asked about is still
		// picked. A removal that failed changes nothing and says so in a box.
		void Delete(const std::string& a_stem)
		{
			WriteOwed("delete");
			if (Presets::PickedStem() != a_stem || !Presets::PickedIsOwn()) {
				return;
			}
			const auto name = Presets::PickedName();
			const auto changed = Presets::Delete();
			if (!changed) {
				SayFailed(Text::MenuPresetDeleteFailed(name));
				return;
			}
			for (const auto* setting : *changed) {
				Followed(*setting);
			}
			RefreshSoon();
		}

		// A press while NEC's box is up or waiting does nothing, so a second
		// press before the box draws opens no second box.
		class Pressed final : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				const auto button = static_cast<Button>(reinterpret_cast<std::uintptr_t>(a_params.userData));
				const auto words = button == Button::kReset ? "Reset preset"sv : "Delete preset"sv;
				if (MessageBox::AnyAsked()) {
					TraceLog::Line("menu", "MCM's {:s} button finds NEC's box open, so nothing more opens", words);
					return;
				}
				const auto stem = Presets::PickedStem();
				const auto name = PickedShown();
				TraceLog::Line("menu", "MCM's {:s} button asks about {:s}", words, name);
				if (button == Button::kReset) {
					const auto body = Text::MenuPresetReset(name);
					MessageBox::Ask("", body.c_str(), { RESET_WORD, CANCEL_WORD }, new Answered([stem] { Reset(stem); }));
				} else {
					const auto body = Text::MenuPresetDelete(name);
					MessageBox::Ask("", body.c_str(), { DELETE_WORD, CANCEL_WORD }, new Answered([stem] { Delete(stem); }));
				}
			}
		};

		Pressed g_pressed;

		// Said once a session, since the movie loads at every pause.
		std::atomic<bool> g_warned{ false };
	}

	void SayFailed(const std::string& a_body)
	{
		MessageBox::Ask("", a_body.c_str(), { OK_WORD }, new Answered({}));
	}

	void AddPresetButtons(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_plugin)
	{
		if (!a_plugin.IsObject()) {
			return;
		}
		for (const auto& [name, button] : { std::pair{ "ResetPreset", Button::kReset }, std::pair{ "DeletePreset", Button::kDelete } }) {
			Value function;
			a_movie.CreateFunction(&function, &g_pressed, reinterpret_cast<void*>(std::to_underlying(button)));
			if (!a_plugin.SetMember(name, function) && !g_warned.exchange(true)) {
				REX::WARN("MCM's {:s} button could not be given its function, so it does nothing.", name);
			}
		}
	}
}
