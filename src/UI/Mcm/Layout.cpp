#include "UI/Mcm/Bridge.h"

#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// MCM draws its settings list in Multi-Line mode. There a row is as tall as
// its name wrapped 530 px wide, while a grey note's words are drawn 670 px
// wide, so a note leaves empty lines under it. And the last row that starts
// inside the list is drawn whole, however far it runs past the bottom, over
// the help line and the frame. In None mode a row is as tall as its words,
// and a row that does not fit is left out until a scroll makes it fit. So
// while one of NEC's pages shows, NEC puts the list in None mode, and back in
// Multi-Line once another page shows. Only a name would wrap differently, and
// no name on NEC's pages is wider than 1 line. On NEC's pages it also writes
// the HUD bar sliders' live ends, see HudBar.cpp, and the line under Preset,
// see PresetRows.cpp. And while MCM asks for a hide of HIDES in Hides.h,
// such as whether the Jams start at row shows, NEC keeps MCM's left list on
// the open page, see KeepOpenPage. While bNecOn is off, NEC's pages leave the
// left list, see HidePages. After a preset's box is answered, NEC has MCM
// draw the page again, see RefreshMcm.
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

		// The left list of page names in a_mcm, MCM's menu, a value that is no
		// object while MCM is not open.
		Value LeftList(const Value& a_mcm)
		{
			return Flash::Member(Flash::Member(a_mcm, "HelpPanel_mc"sv), "HelpList_mc"sv);
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

		// The owner NEC's pages take while bNecOn is off. A mod's name is its
		// folder's, which never holds a |, so no open mod matches it. MOD and
		// this are literals, so each data() ends in a 0 for a Value.
		constexpr std::string_view OFF_OWNER = "NEC|off"sv;

		// The left list as NEC last wrote it, -1 when it has not yet, and the
		// place of the first of NEC's pages in it, -1 for none. Set once a
		// member is missing, which is said once, and then the pages stay.
		std::atomic<std::int64_t> g_pagesLength{ -1 };
		std::atomic<std::int64_t> g_firstPage{ -1 };
		std::atomic<bool>         g_pagesOn{ true };
		std::atomic<bool>         g_pagesMissing{ false };

		// Hides NEC's pages in MCM's left list while bNecOn is off, and shows
		// them again once it is on. The list shows a page only while its
		// ownerModName is the open mod's modName (ListFiltererEx.as,
		// EntryMatchesFilter), and NEC's own entry has no ownerModName, so it
		// stays. MCM filters the list again on InvalidateData, as it does
		// itself after it changes the open mod (MCM_Menu.as, onListItemPress),
		// and reads ownerModName elsewhere only while it loads the configs
		// (decodeJSON, onAllModsLoad). It walks the list only when bNecOn or
		// the list's length changed, or when the entry where NEC last found
		// its first page lacks the owner NEC wants, as in a list MCM built
		// again at the same length. So a list MCM builds while NEC is off
		// hides the pages before a player can open NEC. numPages is left
		// alone, since MCM's page index counts on it.
		void HidePages(Value& a_menus)
		{
			auto left = LeftList(McmMenu(a_menus));
			auto pages = Flash::Member(left, "entryList"sv);
			if (g_pagesMissing.load() || !pages.IsArray()) {
				g_pagesLength.store(-1);
				return;
			}
			const auto on = Settings::NecOn();
			const auto owner = on ? MOD : OFF_OWNER;
			const auto length = static_cast<std::int64_t>(pages.GetArraySize());
			const auto first = PageAt(pages, static_cast<double>(g_firstPage.load()));
			const auto kept = g_firstPage.load() < 0 || (first.IsObject() && Flash::String(first, "ownerModName"sv) == owner);
			if (g_pagesLength.load() == length && g_pagesOn.load() == on && kept) {
				return;
			}
			g_pagesLength.store(length);
			g_pagesOn.store(on);
			g_firstPage.store(-1);

			// NEC's entries, and those of them that carry an owner, the 6 pages.
			auto               ours = 0;
			std::vector<Value> owned;
			for (std::uint32_t i = 0; i < pages.GetArraySize(); i++) {
				Value page;
				if (pages.GetElement(i, &page) && page.IsObject() && Flash::String(page, "modName"sv) == MOD) {
					ours++;
					if (page.HasMember("ownerModName"sv)) {
						if (owned.empty()) {
							g_firstPage.store(i);
						}
						owned.push_back(page);
					}
				}
			}
			if (ours == 0) {
				return;
			}
			// MCM puts NEC's entry and all its pages in the list at once, so an
			// entry with no owned page means MCM no longer works this way.
			const char* missing = nullptr;
			if (owned.empty()) {
				missing = "ownerModName";
			} else if (Flash::Member(left, "InvalidateData"sv).IsUndefined()) {
				missing = "InvalidateData";
			}
			if (missing) {
				if (!g_pagesMissing.exchange(true)) {
					REX::WARN("MCM's page list has no {:s}, so NEC's pages stay listed while NEC is off.", missing);
				}
				return;
			}

			auto moved = 0;
			for (auto& page : owned) {
				if (Flash::String(page, "ownerModName"sv) != owner && Flash::Set(page, "ownerModName"sv, Value(owner.data()))) {
					moved++;
				}
			}
			if (moved > 0) {
				Flash::Call(left, "InvalidateData");
				TraceLog::Line("menu", "MCM's left list {:s} {:d} of NEC's pages, as bNecOn is {:s}", on ? "shows" : "hides", moved, on ? "on" : "off");
			}
		}
	}

	void Layout(Scaleform::GFx::Movie& a_movie)
	{
		Value menus;
		const auto found = a_movie.GetVariable(&menus, "_root.Menu_mc") && menus.IsObject();
		HidePages(menus);
		if (g_missing.load() || !found) {
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
			if (AboutRow(entries)) {
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

	// MCM refreshes the page after a row with refreshMenu changed
	// (RefreshMCM_internal) and takes the page's row filter from the left
	// list's highlight, not from the open page. The left list moves its
	// highlight on every mouse over, so a hand that crossed Armor on its way to
	// the jam chance slider gave Weapons Armor's filter, and Jams start at hid.
	// In that refresh MCM reads every value of every page, each hide of HIDES
	// among them, before it reads the filter, so here the highlight goes back
	// on the open page, as MCM itself does when the right shoulder button moves
	// into the settings list. The first hide asked puts it back and the others
	// find it there. Only on NEC's pages, and not while MCM builds its pages,
	// when the highlight is below 0.
	//
	// A press on a page name opens the page the highlight is on when the click
	// comes, and the hover that moved it there can come a frame before the
	// refresh and the click a frame after. So the highlight goes back for the
	// refresh, and at the next frame the page name still under the pointer
	// gets it again, so a press there opens that page.
	namespace
	{
		// The left list's entry the highlight was put back from while the
		// pointer was on it, -1 for none. Only the main or pause menu's
		// movie thread reads and writes it.
		std::atomic<double> g_hovered{ -1.0 };

		// Whether the pointer is on the clip that draws the left list's entry
		// a_page, tested as the list itself tests it in SetFocusUnderMouse.
		// False for an entry not on screen.
		bool PointerOn(Value& a_left, const Value& a_page)
		{
			const auto clipIndex = Flash::Number(a_page, "clipIndex"sv, -1.0);
			const auto shown = Flash::Number(a_left, "itemsShown"sv, 0.0);
			if (!(clipIndex >= 0.0 && clipIndex < shown)) {
				return false;
			}
			Value      clip;
			const auto index = Value(static_cast<std::uint32_t>(clipIndex));
			const auto stage = Flash::Member(a_left, "stage"sv);
			if (!a_left.Invoke("GetClipByIndex", &clip, &index, 1) || !clip.IsDisplayObject() || !stage.IsObject()) {
				return false;
			}
			const std::array at{ Value(Flash::Number(stage, "mouseX"sv)), Value(Flash::Number(stage, "mouseY"sv)), Value(false) };
			Value            hit;
			return clip.Invoke("hitTestPoint", &hit, at.data(), at.size()) && hit.IsBoolean() && hit.GetBoolean();
		}

		// Where the left list's highlight was, where the open page is, and
		// whether the highlight went back there.
		struct Highlight
		{
			Value  pages;
			double lit = -1.0;
			double page = -1.0;
			bool   back = false;
		};

		// Puts the highlight back on the open page when that is one of NEC's,
		// see KeepOpenPage, and notes the lit page name for GiveHoverBack
		// while the pointer is on it. Nothing while MCM is not open.
		std::optional<Highlight> PutBack(const Value& a_mcm)
		{
			auto       left = LeftList(a_mcm);
			const auto pages = Flash::Member(left, "entryList"sv);
			if (!pages.IsArray()) {
				return std::nullopt;
			}
			const auto page = Flash::Number(a_mcm, "selectedPage"sv, -1.0);
			const auto lit = Flash::Number(left, "selectedIndex"sv, -1.0);
			const auto open = PageAt(pages, page);
			const auto ours = open.IsObject() && Flash::String(open, "modName"sv) == MOD;
			const auto hovered = lit >= 0.0 && lit != page && ours && PointerOn(left, PageAt(pages, lit));
			const auto back = lit >= 0.0 && lit != page && ours && Flash::Set(left, "selectedIndex"sv, Value(page));
			if (back && hovered) {
				g_hovered.store(lit);
			}
			return Highlight{ pages, lit, page, back };
		}
	}

	void KeepOpenPage(Scaleform::GFx::Movie* a_movie, std::string_view a_id)
	{
		Value menus;
		if (!a_movie || !a_movie->GetVariable(&menus, "_root.Menu_mc") || !menus.IsObject()) {
			return;
		}
		if (const auto put = PutBack(McmMenu(menus))) {
			TraceLog::Line("menu", "MCM asks for {:s} with the left list's highlight on {:g} ({:s}) and the open page {:g} ({:s}), put back {:s}",
				a_id, put->lit, PageName(PageAt(put->pages, put->lit)), put->page, PageName(PageAt(put->pages, put->page)), put->back ? "yes" : "no");
		}
	}

	// MCM's own refresh, RefreshMCM, takes the page's row filter from the
	// highlight too, so it goes back first.
	void RefreshMcm(Scaleform::GFx::Movie& a_movie)
	{
		Value menus;
		if (!a_movie.GetVariable(&menus, "_root.Menu_mc") || !menus.IsObject()) {
			return;
		}
		auto mcm = McmMenu(menus);
		if (!mcm.IsObject()) {
			return;
		}
		const auto put = PutBack(mcm);
		const auto done = mcm.Invoke("RefreshMCM");
		TraceLog::Line("menu", "NEC has MCM refresh the page {:g} ({:s}), highlight put back {:s}, {:s}", put ? put->page : -1.0,
			put ? PageName(PageAt(put->pages, put->page)) : std::string{ "(none)" }, put && put->back ? "yes" : "no", done ? "done" : "refused");
	}

	void GiveHoverBack(Scaleform::GFx::Movie& a_movie)
	{
		const auto hovered = g_hovered.exchange(-1.0);
		Value      menus;
		if (hovered < 0.0 || !a_movie.GetVariable(&menus, "_root.Menu_mc") || !menus.IsObject()) {
			return;
		}
		const auto mcm = McmMenu(menus);
		auto       left = LeftList(mcm);
		const auto pages = Flash::Member(left, "entryList"sv);
		const auto page = Flash::Number(mcm, "selectedPage"sv, -1.0);
		// Only while the highlight is still where the put back left it.
		if (!pages.IsArray() || Flash::Number(left, "selectedIndex"sv, -1.0) != page || !PointerOn(left, PageAt(pages, hovered))) {
			return;
		}
		if (Flash::Set(left, "selectedIndex"sv, Value(hovered))) {
			TraceLog::Line("menu", "MCM's left list highlight given back to {:g} ({:s}) after the refresh, the pointer still on it", hovered, PageName(PageAt(pages, hovered)));
		}
	}
}
