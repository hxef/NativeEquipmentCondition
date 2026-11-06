#include "UI/Mcm/Bridge.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/Parts.h"
#include "Core/Pieces.h"
#include "Core/Settings.h"
#include "Core/Text/Text.h"
#include "UI/Flash.h"

#include <algorithm>
#include <bitset>
#include <cstddef>
#include <format>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The page's grey rows: the note of each block, and the list at the top in
// TopList.cpp.
// config.json gives every block lead a hidden switch: a feature switch 2,
// <key>:Free for its row and <key>:Note for its note, a number 1, <key>:Note,
// and the list 1, parts:Note. These numbers run on across NEC's pages, with
// 1 for the HUD bar sliders, see HudBar.cpp, and 1 for the Jams below row,
// fJamChance:Jams, which hides that row while the jam chance is 0, as a
// switch that is off hides the rows under it. MCM counts 30 for each page on
// its own, so a feature switch whose rows hide while it is off takes a
// number its page leaves free, which other pages use too. Each note is worked
// out from SettingLinks and CallPatch's losses, so a setting needs no code of
// its own here. A piece left to a mod is named once: in the note of a block
// that works through it, else in the list at the top. MCM asks once a pause
// menu visit, after NEC's recheck as the menu opens, and the jam chance's
// slider has config.json's refreshMenu, so MCM reads every value again after
// it moves and the Jams below row hides and comes back at once. NEC.log keeps
// every line the page leaves out.
namespace Mcm
{
	namespace
	{
		constexpr std::string_view FREE = ":Free"sv;
		constexpr std::string_view SHOWN = ":Note"sv;
		constexpr std::string_view NOTE = ".note"sv;
		constexpr std::string_view PARTS = "parts"sv;
		constexpr std::string_view PARTS_HELP = "parts.help"sv;
		constexpr std::string_view JAMS = "fJamChance:Jams"sv;

		// MCM draws a noTint text entry white, so this colour shows as is.
		// The tag quotes it with " as MCM's own html does.
		constexpr std::string_view GREY = "#808080"sv;

		// -------------------------------------------------------------------
		// The block notes
		// -------------------------------------------------------------------

		// One of the feature switches by its key, bSpawnCondition.
		Settings::Live<bool>* SwitchFor(std::string_view a_key)
		{
			for (auto* on : Settings::Switches()) {
				if (on->key == a_key) {
					return on;
				}
			}
			return nullptr;
		}

		// The setting a block is named after, by its key: a switch, or a
		// number with nothing above it.
		const SettingLink* LeadFor(std::string_view a_key)
		{
			for (const auto& link : SettingLinks()) {
				if (!link.under && link.setting->key == a_key) {
					return &link;
				}
			}
			return nullptr;
		}

		// The settings of a block, its lead first, in NEC.ini's order.
		std::vector<const SettingLink*> Members(const SettingLink& a_lead)
		{
			std::vector<const SettingLink*> members;
			for (const auto& link : SettingLinks()) {
				if (&link == &a_lead || link.under == a_lead.setting) {
					members.push_back(&link);
				}
			}
			return members;
		}

		// Whether the Jams below row shows: only while guns can jam by the
		// chance.
		bool JamStartShows()
		{
			return Settings::fJamChance.GetValue() > 0.0F;
		}

		// A row the page hides, whose setting its block's note leaves out.
		bool Hidden(const Settings::Named& a_setting)
		{
			return HudBarHidden(a_setting) || (&a_setting == &Settings::fJamStart && !JamStartShows());
		}

		// What the line of a_cause names, as Text::PartLines words it: its
		// pieces with each mod among the owners, or with this game version
		// where no mod is, and with the mods the pieces of the part it waits
		// on, as "since Gun wear from firing is left to" names them.
		std::vector<Left> NamesOf(const CallPatch::Loss& a_loss, const CallPatch::Cause& a_cause)
		{
			const bool mods = std::ranges::any_of(a_cause.owners, &CallPatch::Owner::IsMod);
			auto       pieces = a_cause.pieces;
			if (mods && a_cause.part != a_loss.part) {
				std::ranges::copy(a_cause.taken, std::back_inserter(pieces));
			}
			std::vector<Left> names;
			for (const auto& owner : a_cause.owners) {
				if (mods && !owner.IsMod()) {
					continue;
				}
				for (const auto piece : pieces) {
					names.push_back(Left{ piece, owner });
				}
			}
			return names;
		}
	}

	// A block's note: the line of a switch left to another mod, which
	// stands in for the hidden switch, the lines of each part with pieces
	// off that a setting of the block works through, then what that
	// leaves of each setting still on the page. A part another block's
	// lead works through, and this one's does not, is left to that block's
	// note, as the weapon start works through every weapon penalty. So is
	// a part another lead has among its core pieces and this one does
	// not, as Price drop when worn names Worn item prices and Trader
	// repairs does not. A setting's line leaves out the pieces either note
	// names as off, and goes when none is left, so no piece is named twice
	// on the page.
	Block BlockOf(const SettingLink& a_lead)
	{
		std::vector<std::string>       lines;
		std::vector<std::vector<Left>> says;   // what the part lines name, first in lines
		std::vector<Piece>             named;  // pieces a part line names when off

		const auto add = [&](const CallPatch::Loss& a_loss) {
			auto partLines = Text::PartLines(a_loss);
			// PartLines gives 1 line a cause. Should that change, a line
			// names nothing, so the top list keeps every piece.
			const bool perCause = partLines.size() == a_loss.causes.size();
			for (std::size_t i = 0; i < partLines.size(); i++) {
				says.push_back(perCause ? NamesOf(a_loss, a_loss.causes[i]) : std::vector<Left>{});
			}
			std::ranges::move(partLines, std::back_inserter(lines));
			for (const auto& cause : a_loss.causes) {
				std::ranges::copy(cause.pieces, std::back_inserter(named));
			}
		};
		const auto* on = SwitchFor(a_lead.setting->key);
		const auto  yield = on ? CallPatch::YieldOf(*on) : std::nullopt;
		if (yield) {
			add(*yield);
		}

		const auto              members = Members(a_lead);
		auto                    linked = PartsOf(a_lead);
		std::bitset<PART_COUNT> elsewhere;
		const auto              core = CoreParts(a_lead);
		for (const auto& link : SettingLinks()) {
			if (!link.under && &link != &a_lead) {
				elsewhere |= (PartsOf(link) & ~linked) | (CoreParts(link) & linked & ~core);
			}
		}
		linked &= ~elsewhere;
		for (const auto* member : members) {
			linked |= PartsOf(*member) & ~elsewhere;
			for (const auto list : { member->core, member->side }) {
				std::ranges::copy_if(list, std::back_inserter(named),
					[&](Piece a_piece) { return elsewhere.test(static_cast<std::size_t>(RowOfPiece(a_piece).part)); });
			}
		}
		if (yield) {
			linked.reset(static_cast<std::size_t>(yield->part));
		}
		for (std::size_t i = 0; i < PART_COUNT; i++) {
			if (!linked.test(i)) {
				continue;
			}
			if (const auto loss = CallPatch::LossOf(static_cast<Part>(i))) {
				add(*loss);
			}
		}

		// 2 settings of a block can say the same sentence, which is said
		// once.
		std::vector<std::string> none;
		for (const auto* member : members) {
			if ((yield && member == &a_lead) || Hidden(*member->setting)) {
				continue;
			}
			const auto effect = CallPatch::EffectOf(*member->setting);
			if (effect.idle) {
				continue;
			}
			if (effect.None()) {
				none.push_back(Text::MenuLine(member->setting->key));
				continue;
			}
			for (auto& line : Text::SettingLines(*member, effect, Text::Out::kPage, named)) {
				if (std::ranges::find(lines, line) == lines.end()) {
					lines.push_back(std::move(line));
				}
			}
		}
		if (!none.empty()) {
			lines.push_back(Text::MenuNoEffect(none));
		}
		auto  kept = Capped(std::move(lines));
		Block block{ std::move(kept.lines), {} };
		for (std::size_t i = 0; i < std::min(kept.whole, says.size()); i++) {
			std::ranges::move(says[i], std::back_inserter(block.left));
		}
		return block;
	}

	// -------------------------------------------------------------------
	// What MCM asks
	// -------------------------------------------------------------------

	namespace
	{
		// The lines as 1 grey html text, or nothing for no lines.
		std::string Grey(const std::vector<std::string>& a_lines)
		{
			if (a_lines.empty()) {
				return {};
			}
			std::string text;
			for (const auto& line : a_lines) {
				text += text.empty() ? Flash::HtmlEscaped(line) : "<br>" + Flash::HtmlEscaped(line);
			}
			return std::format("<font color=\"{:s}\">{:s}</font>", GREY, text);
		}
	}

	// A row shows exactly when its words are not empty, since both ask the
	// same functions.
	std::optional<bool> Shows(std::string_view a_id)
	{
		if (a_id == JAMS) {
			return JamStartShows();
		}
		if (a_id.ends_with(FREE)) {
			const auto* on = SwitchFor(a_id.substr(0, a_id.size() - FREE.size()));
			return on ? std::optional{ !CallPatch::IsYielded(*on) } : std::nullopt;
		}
		if (!a_id.ends_with(SHOWN)) {
			return std::nullopt;
		}
		const auto key = a_id.substr(0, a_id.size() - SHOWN.size());
		if (key == PARTS) {
			return !TopLines().empty();
		}
		const auto* lead = LeadFor(key);
		return lead ? std::optional{ !BlockOf(*lead).lines.empty() } : std::nullopt;
	}

	// MCM asks for every line as it builds the page and shows a note only
	// when its hidden switch says so, so a note is empty while nothing is
	// left to another mod.
	std::optional<std::string> Note(std::string_view a_id)
	{
		if (a_id == PARTS_HELP) {
			return Flash::HtmlEscaped(OnlyGameVersion() ? Text::MenuVersionHelp() : Text::MenuNoteHelp());
		}
		if (!a_id.ends_with(NOTE)) {
			return std::nullopt;
		}
		const auto key = a_id.substr(0, a_id.size() - NOTE.size());
		if (key == PARTS) {
			return Grey(TopLines());
		}
		const auto* lead = LeadFor(key);
		return lead ? std::optional{ Grey(BlockOf(*lead).lines) } : std::nullopt;
	}
}
