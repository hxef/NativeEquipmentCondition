#include "UI/Mcm/Bridge.h"

#include "Core/TraceLog.h"
#include "UI/Flash.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>

// MCM draws its settings list in Multi-Line mode. There a row is as tall as
// its name wrapped 530 px wide, while a grey note's words are drawn 670 px
// wide, so a note leaves empty lines under it. And the last row that starts
// inside the list is drawn whole, however far it runs past the bottom, over
// the help line and the frame. In None mode a row is as tall as its words,
// and a row that does not fit is left out until a scroll makes it fit. So
// while one of NEC's pages shows, NEC puts the list in None mode, and back in
// Multi-Line once another page shows. Only a name would wrap differently, and
// no name on NEC's pages is wider than 1 line. On NEC's pages it also writes
// the HUD bar sliders' live ends, see HudBar.cpp. And while MCM asks whether
// the Jams below row shows, NEC keeps MCM's left list on the open page, see
// KeepOpenPage.
//
// MCM's menu is a child of _root.Menu_mc, its list is configPanel_mc's
// configList_mc, and the list holds the shown page's entries in entryList.
// The left list of page names is HelpPanel_mc's HelpList_mc.
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

		// MCM's menu, from the last child of a_menus, _root.Menu_mc, back,
		// since MCM adds its menu last. A value that is no object while MCM is
		// not open.
		Value McmMenu(Value& a_menus)
		{
			const auto children = static_cast<std::int32_t>(Flash::Number(a_menus, "numChildren"sv));
			for (auto i = children - 1; i >= 0; i--) {
				auto child = Flash::ChildAt(a_menus, static_cast<std::uint32_t>(i));
				if (child.IsObject() && child.HasMember("configPanel_mc"sv)) {
					return child;
				}
			}
			return {};
		}

		// MCM's settings list, a value that is no object while MCM is not open.
		Value SettingsList(Value& a_menus)
		{
			auto list = Flash::Member(Flash::Member(McmMenu(a_menus), "configPanel_mc"sv), "configList_mc"sv);
			return list.IsObject() ? list : Value{};
		}

		// The left list's entry at a_at, a value that is no object past its
		// ends.
		Value PageAt(const Value& a_pages, double a_at)
		{
			Value at;
			if (a_at >= 0.0 && a_at < static_cast<double>(a_pages.GetArraySize())) {
				a_pages.GetElement(static_cast<std::uint32_t>(a_at), &at);
			}
			return at;
		}

		// A left list entry for the trace, as Weapons of NEC.
		std::string PageName(const Value& a_page)
		{
			return a_page.IsObject() ? Flash::String(a_page, "text"sv) + " of " + Flash::String(a_page, "modName"sv) : "(none)";
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

		// Whether a_entries are one of NEC's pages. MCM puts config.json's
		// modName on every entry of each page.
		bool IsNecPage(const Value& a_entries)
		{
			Value first;
			Value mod;
			return a_entries.GetArraySize() > 0 && a_entries.GetElement(0, &first) && first.IsObject() &&
			       first.GetMember("modName"sv, &mod) && mod.IsString() && mod.GetString() && std::string_view{ mod.GetString() } == MOD;
		}
	}

	void Layout(Scaleform::GFx::Movie& a_movie)
	{
		Value menus;
		if (g_missing.load() || !a_movie.GetVariable(&menus, "_root.Menu_mc") || !menus.IsObject()) {
			return;
		}
		auto list = SettingsList(menus);
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

		// MCM lays its rows out again once for both changes.
		const std::string_view now{ mode.GetString() };
		auto                   redraw = false;
		if (IsNecPage(entries)) {
			if (now == MULTI_LINE) {
				list.SetMember("textOption"sv, Value(NONE));
				g_changed.store(true);
				redraw = true;
			}
			if (HudBarRanges(entries)) {
				redraw = true;
			}
		} else if (g_changed.load() && now == NONE) {
			list.SetMember("textOption"sv, Value(MULTI_LINE));
			g_changed.store(false);
			redraw = true;
		}
		if (redraw) {
			list.Invoke("InvalidateData");
		}
	}

	// MCM refreshes the page after a slider with refreshMenu moved
	// (RefreshMCM_internal) and takes the page's row filter from the left
	// list's highlight, not from the open page. The left list moves its
	// highlight on every mouse over, so a hand that crossed Armor on its way to
	// the jam chance slider gave Weapons Armor's filter, and Jams below hid.
	// MCM asks for fJamChance:Jams in that refresh before it reads the filter,
	// so here the highlight goes back on the open page, as MCM itself does when
	// the right shoulder button moves into the settings list. Only on NEC's
	// pages, and not while MCM builds its pages, when the highlight is below 0.
	void KeepOpenPage(Scaleform::GFx::Movie* a_movie)
	{
		Value menus;
		if (!a_movie || !a_movie->GetVariable(&menus, "_root.Menu_mc") || !menus.IsObject()) {
			return;
		}
		const auto mcm = McmMenu(menus);
		auto       left = Flash::Member(Flash::Member(mcm, "HelpPanel_mc"sv), "HelpList_mc"sv);
		const auto pages = Flash::Member(left, "entryList"sv);
		if (!pages.IsArray()) {
			return;
		}
		const auto page = Flash::Number(mcm, "selectedPage"sv, -1.0);
		const auto lit = Flash::Number(left, "selectedIndex"sv, -1.0);
		const auto open = PageAt(pages, page);
		const auto ours = open.IsObject() && Flash::String(open, "modName"sv) == MOD;
		const auto back = lit >= 0.0 && lit != page && ours && Flash::Set(left, "selectedIndex"sv, Value(page));
		TraceLog::Line("menu", "MCM asks for {:s} with the left list's highlight on {:g} ({:s}) and the open page {:g} ({:s}), put back {:s}",
			JAMS, lit, PageName(PageAt(pages, lit)), page, PageName(open), back ? "yes" : "no");
	}
}
