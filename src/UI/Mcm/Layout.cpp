#include "UI/Mcm/Bridge.h"

#include "UI/Flash.h"

#include <atomic>
#include <cstdint>
#include <string_view>

// MCM draws its settings list in Multi-Line mode. There a row is as tall as
// its name wrapped 530 px wide, while a grey note's words are drawn 670 px
// wide, so a note leaves empty lines under it. And the last row that starts
// inside the list is drawn whole, however far it runs past the bottom, over
// the help line and the frame. In None mode a row is as tall as its words,
// and a row that does not fit is left out until a scroll makes it fit. So
// while NEC's page shows, NEC puts the list in None mode, and back in
// Multi-Line once another page shows. Only a name would wrap differently, and
// no name on NEC's page is wider than 1 line.
//
// MCM's menu is a child of _root.Menu_mc, its list is configPanel_mc's
// configList_mc, and the list holds the shown page's entries in entryList.
// Nothing is kept past a frame, since the movie can be gone by the next one.
namespace Mcm
{
	namespace
	{
		using Scaleform::GFx::Value;

		constexpr const char* MULTI_LINE = "Multi-Line";
		constexpr const char* NONE = "None";

		// Set while NEC has the list in None mode, so another page gets
		// Multi-Line back and a list NEC never changed is left alone.
		std::atomic<bool> g_changed{ false };

		// Set once a member NEC needs is missing, which is said once, and
		// from then on NEC leaves the list alone.
		std::atomic<bool> g_missing{ false };

		// MCM's settings list, from the last child of a_menu back, since MCM
		// adds its menu last. A value that is no object while MCM is not open.
		Value SettingsList(Value& a_menu)
		{
			const auto children = static_cast<std::int32_t>(Flash::Number(a_menu, "numChildren"sv));
			for (auto i = children - 1; i >= 0; i--) {
				auto  child = Flash::ChildAt(a_menu, static_cast<std::uint32_t>(i));
				Value panel;
				Value list;
				if (child.IsObject() && child.HasMember("configPanel_mc"sv) && child.GetMember("configPanel_mc"sv, &panel) &&
					panel.IsObject() && panel.HasMember("configList_mc"sv) && panel.GetMember("configList_mc"sv, &list) &&
					list.IsObject()) {
					return list;
				}
			}
			return {};
		}

		// The first member of the list NEC needs that it lacks, nullptr when
		// it has all 3.
		const char* Missing(const Value& a_list, Value& a_entries, Value& a_mode)
		{
			Value invalidate;
			if (!a_list.GetMember("entryList"sv, &a_entries) || !a_entries.IsArray()) {
				return "entryList";
			}
			if (!a_list.GetMember("textOption"sv, &a_mode) || !a_mode.IsString() || !a_mode.GetString()) {
				return "textOption";
			}
			if (!a_list.GetMember("InvalidateData"sv, &invalidate) || invalidate.IsUndefined()) {
				return "InvalidateData";
			}
			return nullptr;
		}

		// Whether a_entries are NEC's page. MCM puts the page's modName on
		// every entry.
		bool IsNecPage(const Value& a_entries)
		{
			Value first;
			Value mod;
			return a_entries.GetArraySize() > 0 && a_entries.GetElement(0, &first) && first.IsObject() &&
			       first.GetMember("modName"sv, &mod) && mod.IsString() && mod.GetString() && std::string_view{ mod.GetString() } == MOD;
		}

		// Sets the list's mode, and has MCM lay its rows out again with it.
		void SetMode(Value& a_list, const char* a_mode)
		{
			a_list.SetMember("textOption"sv, Value(a_mode));
			a_list.Invoke("InvalidateData");
		}
	}

	void Layout(Scaleform::GFx::Movie& a_movie)
	{
		Value menu;
		if (g_missing.load() || !a_movie.GetVariable(&menu, "_root.Menu_mc") || !menu.IsObject()) {
			return;
		}
		auto list = SettingsList(menu);
		if (!list.IsObject()) {
			return;
		}

		Value entries;
		Value mode;
		if (const auto* missing = Missing(list, entries, mode)) {
			if (!g_missing.exchange(true)) {
				REX::WARN("MCM's settings list has no {:s}, so NEC's page keeps MCM's own layout.", missing);
			}
			return;
		}

		const std::string_view now{ mode.GetString() };
		if (IsNecPage(entries)) {
			if (now == MULTI_LINE) {
				SetMode(list, NONE);
				g_changed.store(true);
			}
		} else if (g_changed.load() && now == NONE) {
			SetMode(list, MULTI_LINE);
			g_changed.store(false);
		}
	}
}
