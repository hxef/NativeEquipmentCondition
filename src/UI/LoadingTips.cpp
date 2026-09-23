#include "UI/LoadingTips.h"

#include "Core/CallPatch.h"
#include "Core/Settings.h"
#include "Core/Text.h"
#include "Core/TraceLog.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <string>

namespace LoadingTips
{
	namespace
	{
		// One tip: its words, the vanilla loading screen in Fallout4.esm whose
		// picture it shows, and the switch of the feature it tells of, or
		// nothing for one that always holds. A tip whose switch is off is
		// never made.
		struct Tip
		{
			std::string (*words)();
			RE::TESFormID picture;
			const REX::TIniSetting<bool>* on = nullptr;

			[[nodiscard]] bool IsOn() const { return !on || on->GetValue(); }
		};

		// Every tip, each on a picture of what it tells of. Another one is a
		// sentence in Text.cpp and a row here.
		constexpr Tip TIPS[]{
			// WeaponMod02, the weapons workbench.
			{ .words = &Text::WearTip, .picture = 0x001603FD },
		};

		// The numbers the tips take. FF numbers are the game's dynamic ones,
		// and it never gives out one below FIRST_DYNAMIC_FORM_ID, so FF000001
		// up to that are numbers the game never gives anything and a plugin
		// file cannot give at all.
		constexpr RE::TESFormID UNUSED_FIRST = 0xFF000001;
		constexpr RE::TESFormID UNUSED_END = RE::TESDataHandler::FIRST_DYNAMIC_FORM_ID;

		// The one call to LoadingMenu::CollectLoadScreens inside
		// LoadingMenu::PopulateLoadScreens, which gathers every loading screen
		// that may show, straight before one is picked.
		constexpr CallPatch::CallSite COLLECT_SITE{ RE::ID::LoadingMenu::PopulateLoadScreens.id(), 0xB6, "loading screens" };

		// A loading screen of the plugin's own for every row of TIPS, made once
		// and kept until the game shuts down. One without a borrowed picture is
		// left out every time the screens are collected.
		std::array<RE::TESLoadScreen*, std::size(TIPS)> g_tips{};

		// The number the loading screen prints after "VDSG Catalogue No.", the
		// last 4 digits of the form ID, which tell a tip apart in game.
		std::uint32_t CatalogueNumber(const RE::TESLoadScreen& a_screen)
		{
			return a_screen.formID % 10000;
		}

		// The first number from a_from on that no form holds, or 0 once the
		// unused ones run out. Only another DLL could have taken one.
		RE::TESFormID FreeNumber(RE::TESFormID a_from)
		{
			for (auto number = a_from; number < UNUSED_END; ++number) {
				if (!RE::TESForm::GetFormByID(number)) {
					return number;
				}
			}
			return 0;
		}

		// A tip as a loading screen, still without a picture. The game's own
		// factory makes it, which enters it in the form table with a dynamic
		// number. Setting its number to none takes it back out and hands the
		// number back. Then a_number is written straight onto it, since setting
		// it through the game would enter it again. Load lends it a picture.
		RE::TESLoadScreen* Make(const Tip& a_tip, RE::TESFormID a_number)
		{
			auto* factory = RE::ConcreteFormFactory<RE::TESLoadScreen>::GetFormFactory();
			auto* screen = factory ? factory->Create() : nullptr;
			if (!screen) {
				return nullptr;
			}
			screen->SetFormID(0, true);
			screen->formID = a_number;
			screen->loadingText = a_tip.words();
			return screen;
		}

		// Everything the load order holds, then the tips. The game draws the
		// words from validScreens and the picture from artCandidates. The tips
		// have no conditions and a picture, so they go on both lists. Nothing
		// is offered during a full reset, while the pictures are being handed
		// back and borrowed again.
		void CollectHk(RE::TESDataHandler* a_handler, RE::ENUM_FORM_ID a_type, RE::LoadingMenu::LoadScreenCandidates* a_candidates)
		{
			RE::LoadingMenu::CollectLoadScreens(a_handler, a_type, a_candidates);

			const auto* main = RE::Main::GetSingleton();
			if (main && main->resetGame) {
				return;
			}

			std::size_t added = 0;
			for (auto* screen : g_tips) {
				if (screen && screen->loadNIFData) {
					a_candidates->artCandidates->push_back(screen);
					a_candidates->validScreens->push_back(screen);
					added++;
				}
			}
			if (added == 0) {
				return;
			}

			TraceLog::Line("menu", "loading screen picks from {:d} pictures, {:d} of them tips and {:d} with conditions",
				a_candidates->artCandidates->size(), added, a_candidates->numNonDefaultArtCandidates);
		}

		REL::Relocation<RE::UI_MESSAGE_RESULTS (*)(RE::LoadingMenu*, RE::UIMessage&)> _ProcessMessage;

		// Every message to the loading menu. The one that gathers and picks is
		// the update carrying the menu's data, not the show before it.
		// loadScreenShown turns on once per loading screen, on that message,
		// and the pick is still on the menu as it returns. Put in only while
		// there is a trace to write.
		RE::UI_MESSAGE_RESULTS ProcessMessageHk(RE::LoadingMenu* a_menu, RE::UIMessage& a_message)
		{
			const bool shown = a_menu->loadScreenShown;
			const auto result = _ProcessMessage(a_menu, a_message);
			if (shown || !a_menu->loadScreenShown) {
				return result;
			}

			const auto* screen = a_menu->artScreen;
			if (!screen) {
				TraceLog::Line("menu", "loading screen shows no picture");
				return result;
			}
			const bool tip = std::find(g_tips.begin(), g_tips.end(), screen) != g_tips.end();
			TraceLog::Line("menu", "loading screen shows {:s} {:08X}, catalogue number {:d}",
				tip ? "tip" : "screen", screen->formID, CatalogueNumber(*screen));
			return result;
		}
	}

	void Install()
	{
		if (CallPatch::PatchCall(COLLECT_SITE, RE::ID::LoadingMenu::CollectLoadScreens.address(), reinterpret_cast<std::uintptr_t>(&CollectHk))) {
			REX::INFO("Loading screen tips join the screens the game picks from.");
		} else {
			REX::ERROR("Loading screen tips will not show.");
		}

		if (TraceLog::IsOpen()) {
			REL::Relocation<std::uintptr_t> menu{ RE::LoadingMenu::VTABLE[0] };
			_ProcessMessage = menu.write_vfunc(0x03, ProcessMessageHk);
		}
	}

	void Load()
	{
		// Numbers go out in the order of TIPS. A tip made on a later load
		// starts after every number already taken, since the tips are in no
		// table FreeNumber could find them in.
		auto        next = UNUSED_FIRST;
		std::size_t ready = 0;
		for (std::size_t i = 0; i < std::size(TIPS); i++) {
			const auto& tip = TIPS[i];
			if (!tip.IsOn()) {
				continue;
			}

			auto*& screen = g_tips[i];
			if (!screen) {
				const auto number = FreeNumber(next);
				if (!number) {
					REX::ERROR("The dynamic numbers the game never hands out are all taken, so the tips from here on are left out.");
					break;
				}
				screen = Make(tip, number);
				if (!screen) {
					REX::ERROR("The game made no loading screen for a tip, so the tips from here on are left out.");
					break;
				}
			}
			next = screen->formID + 1;

			// The picture is the vanilla screen's own, shared, and one the
			// loading menu can draw. A loading screen frees its picture when
			// destroyed, and a tip never is, but the vanilla screen is destroyed
			// on a full reset, which is why Unload hands the picture back first.
			const auto* picture = RE::TESForm::GetFormByID<RE::TESLoadScreen>(tip.picture);
			if (!picture || !picture->GetLoadNIFModel()) {
				REX::ERROR("Loading screen {:08X} has no picture to lend, its tip is left out.", tip.picture);
				continue;
			}
			screen->loadNIFData = picture->loadNIFData;
			ready++;

			REX::INFO("Loading screen tip {:08X} ready, shown as catalogue number {:d}.",
				screen->formID, CatalogueNumber(*screen));
		}

		// Counted apart from the loop, which stops early once numbers or
		// screens run out.
		const auto offered = std::ranges::count_if(TIPS, &Tip::IsOn);
		REX::INFO("{:d} of {:d} loading screen tips ready, each as likely to show as any one ordinary loading screen.",
			ready, offered);
	}

	void Unload()
	{
		for (auto* screen : g_tips) {
			if (screen) {
				screen->loadNIFData = nullptr;
			}
		}
	}
}
