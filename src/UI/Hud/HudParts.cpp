#include "UI/Hud/HudParts.h"

#include "Condition/Equipped.h"
#include "Core/TraceLog.h"
#include "UI/Flash.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <format>
#include <memory>
#include <mutex>
#include <vector>

namespace HudParts
{
	namespace Weapon
	{
		namespace
		{
			// The latest condition, and whether a weapon is drawn, written by
			// the task and read by the HUD's frames.
			std::atomic<std::int32_t> g_percent{ NONE };
			std::atomic<bool>         g_drawn{ false };

			// Set while a check waits in the task queue.
			std::atomic<bool> g_queued{ false };
		}

		void Queue()
		{
			if (g_queued.exchange(true)) {
				return;
			}

			const auto* tasks = F4SE::GetTaskInterface();
			if (!tasks) {
				g_queued = false;
				return;
			}

			// F4SE runs the task a frame later, on a worker thread during play,
			// so the inventory can change under it. Equipped::TryWeaponHealth
			// holds the inventory lock while it reads and returns nothing while
			// the inventory is busy, which keeps the last condition for a
			// frame.
			tasks->AddTask([] {
				auto*      player = RE::PlayerCharacter::GetSingleton();
				const auto health = Equipped::TryWeaponHealth(player);
				if (health) {
					g_percent = *health < 0.0F ? NONE : static_cast<std::int32_t>(std::lround(std::clamp(*health, 0.0F, 1.0F) * 100.0F));
				} else {
					TraceLog::Once("menu", "HUD CND kept its last reading, another thread had the player's inventory");
				}
				g_drawn = player && player->GetWeaponMagicDrawn();
				g_queued = false;
			});
		}

		std::int32_t Percent()
		{
			return g_percent.load();
		}

		bool Drawn()
		{
			return g_drawn.load();
		}
	}

	void SetStyledText(Value& a_field, std::string_view a_text, std::int32_t a_size)
	{
		std::string escaped;
		for (const auto c : a_text) {
			switch (c) {
			case '&':
				escaped += "&amp;";
				break;
			case '<':
				escaped += "&lt;";
				break;
			case '>':
				escaped += "&gt;";
				break;
			default:
				escaped += c;
				break;
			}
		}
		const auto html = std::format(R"(<font face="{:s}" size="{}" color="#FFFFFF">{:s}</font>)", FONT_NAME, a_size, escaped);
		a_field.SetMember("htmlText"sv, Value(std::string_view{ html }));
	}

	void SetTranslatedText(Value& a_field, const char* a_key, std::int32_t a_size)
	{
		a_field.SetMember("text"sv, Value(a_key));
		SetStyledText(a_field, Flash::String(a_field, "text"sv), a_size);
	}

	void PutBox(Value& a_box, double a_x, double a_y, double a_width, double a_height)
	{
		a_box.SetMember("x"sv, Value(a_x));
		a_box.SetMember("y"sv, Value(a_y));
		a_box.SetMember("scaleX"sv, Value(std::max(a_width, 0.0)));
		a_box.SetMember("scaleY"sv, Value(std::max(a_height, 0.0)));
	}

	bool AddBox(Scaleform::GFx::Movie& a_movie, Value& a_parent, const char* a_name)
	{
		Value box;
		a_movie.CreateObject(&box, "flash.display.Sprite");
		if (!box.IsDisplayObject()) {
			return false;
		}

		Value graphics;
		if (!box.GetMember("graphics"sv, &graphics) ||
			!graphics.Invoke("beginFill", std::array{ Value(0xFFFFFF), Value(1.0) }) ||
			!graphics.Invoke("drawRect", std::array{ Value(0.0), Value(0.0), Value(1.0), Value(1.0) }) ||
			!graphics.Invoke("endFill")) {
			return false;
		}

		box.SetMember("name"sv, Value(a_name));
		return a_parent.Invoke("addChild", std::array{ box });
	}

	bool AddField(Scaleform::GFx::Movie& a_movie, Value& a_parent, const char* a_name)
	{
		Value field;
		a_movie.CreateObject(&field, "flash.text.TextField");
		if (!field.IsDisplayObject()) {
			return false;
		}

		field.SetMember("name"sv, Value(a_name));

		// The HUD font lives in the game's font library, which is what
		// embedFonts asks for.
		field.SetMember("embedFonts"sv, Value(true));
		field.SetMember("selectable"sv, Value(false));
		field.SetMember("mouseEnabled"sv, Value(false));
		field.SetMember("autoSize"sv, Value("left"));
		return a_parent.Invoke("addChild", std::array{ field });
	}

	RE::HUDMenu* MenuFor(const Scaleform::GFx::Movie& a_movie)
	{
		const auto* ui = RE::UI::GetSingleton();
		const auto  menu = ui ? ui->GetMenu<RE::HUDMenu>() : nullptr;
		return menu && menu->uiMovie.get() == &a_movie ? menu.get() : nullptr;
	}

	namespace
	{
		// The colour targets AddColorTarget made, with the HUD menu each was
		// made for.
		struct ColorTarget
		{
			const RE::HUDMenu*                       menu;
			std::unique_ptr<RE::BSGFxShaderFXTarget> target;
		};

		std::mutex               g_colorTargetsLock;
		std::vector<ColorTarget> g_colorTargets;

		REL::Relocation<void* (*)(RE::HUDMenu*, std::uint32_t)> _DeleteMenu;

		// Runs before the menu's own destructor, so the movie still exists when
		// a target releases its clip. The last reference to a menu can be
		// released on any thread, hence the lock.
		void* DeleteMenuHk(RE::HUDMenu* a_menu, std::uint32_t a_flags)
		{
			std::vector<std::unique_ptr<RE::BSGFxShaderFXTarget>> gone;
			{
				const std::scoped_lock l(g_colorTargetsLock);
				for (auto it = g_colorTargets.begin(); it != g_colorTargets.end();) {
					if (it->menu == a_menu) {
						gone.push_back(std::move(it->target));
						it = g_colorTargets.erase(it);
					} else {
						++it;
					}
				}
			}

			if (!gone.empty()) {
				{
					const RE::BSAutoWriteLock l(a_menu->cachedQuadsLock);
					auto& list = a_menu->shaderFXObjects;
					for (const auto& target : gone) {
						if (const auto it = std::find(list.begin(), list.end(), target.get()); it != list.end()) {
							list.erase(it);
						}
					}
				}
				TraceLog::Line("menu", "HUD menu goes, and the {:d} colour targets of its CND parts with it", gone.size());
				gone.clear();
			}
			return _DeleteMenu(a_menu, a_flags);
		}
	}

	void Install()
	{
		REL::Relocation<std::uintptr_t> menu{ RE::HUDMenu::VTABLE[0] };
		_DeleteMenu = menu.write_vfunc(0x00, DeleteMenuHk);
	}

	void AddColorTarget(RE::HUDMenu& a_menu, const Value& a_clip)
	{
		auto target = std::make_unique<RE::BSGFxShaderFXTarget>(a_clip);
		target->SetToHUDColor(false);
		{
			const RE::BSAutoWriteLock l(a_menu.cachedQuadsLock);
			a_menu.shaderFXObjects.push_back(target.get());
		}

		const std::scoped_lock l(g_colorTargetsLock);
		g_colorTargets.push_back({ &a_menu, std::move(target) });
	}

	const bool* ComponentCanBeVisible(const RE::HUDMenu& a_menu, std::uintptr_t a_vtable)
	{
		// The first thing in any HUD part is its vtable, which says what kind
		// of part it is.
		for (const auto& component : a_menu.hudObjects) {
			if (component && *reinterpret_cast<const std::uintptr_t*>(component.get()) == a_vtable) {
				return &component->hudModes.canBeVisible;
			}
		}
		return nullptr;
	}

	namespace Readout
	{
		namespace
		{
			// The parts of every readout. Each readout is a sprite of its own,
			// so both use the same 3 names.
			constexpr const char* TRACK_NAME = "NEC_ConditionTrack_mc";
			constexpr const char* FILL_NAME = "NEC_ConditionFill_mc";
			constexpr const char* LABEL_NAME = "NEC_ConditionLabel_tf";

			// The HP meter draws its spare bar at half opacity, so the track
			// does too.
			constexpr double TRACK_ALPHA = 0.5;

			// The whole size the word is written at, see SetStyledText. Its
			// field is then scaled to the size asked for.
			constexpr std::int32_t WRITTEN_SIZE = 20;
		}

		Value Create(Scaleform::GFx::Movie& a_movie, const char* a_name)
		{
			Value readout;
			a_movie.CreateObject(&readout, "flash.display.Sprite");
			if (!readout.IsDisplayObject()) {
				return Value{};
			}
			readout.SetMember("name"sv, Value(a_name));
			readout.SetMember("mouseEnabled"sv, Value(false));
			readout.SetMember("mouseChildren"sv, Value(false));
			readout.SetMember("visible"sv, Value(false));

			// The track goes down first and the bar over it.
			if (!AddBox(a_movie, readout, TRACK_NAME) ||
				!AddBox(a_movie, readout, FILL_NAME) ||
				!AddField(a_movie, readout, LABEL_NAME)) {
				return Value{};
			}
			return readout;
		}

		void AntiAliasForAnimation(Value& a_readout)
		{
			// ActionScript's "normal" is anti-alias for animation, "advanced"
			// for readability.
			auto label = Flash::Child(a_readout, LABEL_NAME);
			if (label.IsDisplayObject()) {
				label.SetMember("antiAliasType"sv, Value("normal"));
			}
		}

		double SetLabel(Value& a_readout, const Label& a_label)
		{
			auto label = Flash::Child(a_readout, LABEL_NAME);
			if (!label.IsDisplayObject()) {
				return 0.0;
			}

			// The field sizes itself to its text, so its size is known once
			// the text is in. It keeps a margin on each side, scaled with its
			// letters, and its middle is the middle of the capitals, so a word
			// set level with the bar looks level.
			const auto scale = a_label.size / WRITTEN_SIZE;
			SetTranslatedText(label, CND_TEXT, WRITTEN_SIZE);
			label.SetMember("scaleX"sv, Value(a_label.wide * scale));
			label.SetMember("scaleY"sv, Value(scale));
			const auto margin = FIELD_MARGIN * a_label.wide * scale;
			label.SetMember("x"sv, Value(a_label.gap - margin));
			label.SetMember("y"sv, Value(a_label.drop - (Flash::Number(label, "height"sv) / 2.0)));
			return a_label.gap + Flash::Number(label, "width"sv) - (margin * 2.0);
		}

		void SetTrack(Value& a_readout, double a_width, double a_deep)
		{
			auto track = Flash::Child(a_readout, TRACK_NAME);
			if (!track.IsDisplayObject()) {
				return;
			}

			PutBox(track, -a_width, -a_deep / 2.0, a_width, a_deep);
			track.SetMember("alpha"sv, Value(TRACK_ALPHA));
		}

		void SetPercent(Value& a_readout, double a_width, double a_deep, std::int32_t a_percent)
		{
			auto fill = Flash::Child(a_readout, FILL_NAME);
			if (!fill.IsDisplayObject()) {
				return;
			}

			// The right end stays next to the word and the left end moves in,
			// as the AP meter does.
			const auto left = a_width * std::clamp(a_percent, 0, 100) / 100.0;
			PutBox(fill, -left, -a_deep / 2.0, left, a_deep);
		}
	}
}
