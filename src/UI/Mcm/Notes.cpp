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
#include <functional>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The page's grey rows: the list at the top and the note of each block.
// config.json gives every block lead a hidden switch: a feature switch 2,
// <key>:Free for its row and <key>:Note for its note, a number 1, <key>:Note,
// and the list 1, parts:Note. MCM has 30 a page, 26 are used. Each note is
// worked out from SettingLinks and CallPatch's losses, so a setting needs no
// code of its own here. MCM asks once a pause menu visit, after NEC's recheck
// as the menu opens. NEC.log keeps every line the page leaves out.
namespace Mcm
{
	namespace
	{
		constexpr std::string_view FREE = ":Free"sv;
		constexpr std::string_view SHOWN = ":Note"sv;
		constexpr std::string_view NOTE = ".note"sv;
		constexpr std::string_view PARTS = "parts"sv;
		constexpr std::string_view PARTS_HELP = "parts.help"sv;

		// MCM draws a noTint text entry white, so this colour shows as is.
		// The tag quotes it with " as MCM's own html does.
		constexpr std::string_view GREY = "#808080"sv;

		// The most lines the top list gives the mods, the last one saying how
		// many more there are when they do not fit.
		constexpr std::size_t MOST_OWNER_LINES = 8;

		// ---------------------------------------------------------------------
		// The cap on a grey row
		// ---------------------------------------------------------------------

		// MCM's list is 450 px tall, and with NEC's page in None mode, see
		// Layout.cpp, a row taller than that never shows: at most 18
		// lines of 24.8 px, or 21 lines of Japanese or Chinese at 21 px. The
		// cap keeps 1 line under each, so 17 and 20. A line is counted as so
		// many characters a line, by language. Checked on about 10000 of NEC's
		// rows against the widths of the game's fonts, that count was never
		// below the lines a row wraps to.
		struct Fit
		{
			std::string_view code;
			std::size_t      perLine;
			std::size_t      budget;
		};

		constexpr Fit FITS[]{
			{ "en", 62, 17 },
			{ "fr", 64, 17 },
			{ "de", 58, 17 },
			{ "it", 66, 17 },
			{ "es", 62, 17 },
			{ "esmx", 62, 17 },
			{ "ptbr", 62, 17 },
			{ "pl", 60, 17 },
			{ "ru", 54, 17 },
			{ "ja", 27, 20 },
			{ "zhhant", 26, 20 },
			{ "zhhans", 26, 20 },
		};

		// The player's language's row, or English's.
		const Fit& FitNow()
		{
			const auto language = Text::Language();
			const auto it = std::ranges::find(FITS, language, &Fit::code);
			return it != std::end(FITS) ? *it : FITS[0];
		}

		// The lines a_line takes, from its characters before html escaping.
		// A character of 2 to 4 bytes counts once, so Cyrillic and Chinese
		// count right.
		std::size_t LinesOf(std::string_view a_line, const Fit& a_fit)
		{
			const auto characters = static_cast<std::size_t>(
				std::ranges::count_if(a_line, [](char a_byte) { return (static_cast<unsigned char>(a_byte) & 0xC0) != 0x80; }));
			return std::max<std::size_t>(1, (characters + a_fit.perLine - 1) / a_fit.perLine);
		}

		// Builds line a_line again in a_room lines or fewer, or nothing where
		// it has no such form.
		using Shorter = std::function<std::optional<std::string>(std::size_t a_line, std::size_t a_room)>;

		// a_lines whole while they fit. Else the lines from the first while
		// they fit in all but 1 line, the first that does not fit in its
		// a_shorter form where it has one that fits, always the first line,
		// then the line saying NEC.log names the rest.
		std::vector<std::string> Capped(std::vector<std::string> a_lines, const Shorter& a_shorter = {})
		{
			const auto& fit = FitNow();
			std::size_t used = 0;
			for (const auto& line : a_lines) {
				used += LinesOf(line, fit);
			}
			if (used <= fit.budget) {
				return a_lines;
			}
			std::vector<std::string> kept;
			used = 0;
			for (std::size_t i = 0; i < a_lines.size(); i++) {
				const auto lines = LinesOf(a_lines[i], fit);
				if (used + lines < fit.budget) {
					used += lines;
					kept.push_back(std::move(a_lines[i]));
					continue;
				}
				if (auto shorter = a_shorter ? a_shorter(i, fit.budget - 1 - used) : std::nullopt) {
					kept.push_back(std::move(*shorter));
				} else if (kept.empty()) {
					kept.push_back(std::move(a_lines[i]));
				}
				break;
			}
			kept.push_back(Text::MenuNoteRest());
			return kept;
		}

		// ---------------------------------------------------------------------
		// The rows
		// ---------------------------------------------------------------------

		// One of the 10 feature switches by its key, bJam.
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

		// A block's note: the line of a switch left to another mod, which
		// stands in for the hidden switch, the lines of each part with pieces
		// off that a setting of the block works through, then what that
		// leaves of each setting still on the page. A setting's line leaves
		// out the pieces a part line above names as off, and goes when none
		// is left, so no piece is named twice.
		std::vector<std::string> BlockLines(const SettingLink& a_lead)
		{
			std::vector<std::string> lines;
			std::vector<Piece>       named;  // the pieces the part lines name as off

			const auto add = [&](const CallPatch::Loss& a_loss) {
				std::ranges::move(Text::PartLines(a_loss), std::back_inserter(lines));
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
			std::bitset<PART_COUNT> linked;
			for (const auto* member : members) {
				for (const auto list : { member->core, member->side, member->exceptions }) {
					for (const auto piece : list) {
						linked.set(static_cast<std::size_t>(RowOfPiece(piece).part));
					}
				}
			}
			linked.reset(static_cast<std::size_t>(Part::kNone));
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
				if (yield && member == &a_lead) {
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
			return Capped(std::move(lines));
		}

		// The list at the top: its first line while a mod has a part, then
		// each mod with the parts it took, a part with some pieces taken naming
		// them, in the order of CallPatch::Before. With only this game
		// version, its own line says why.
		std::vector<std::string> TopLines()
		{
			const auto losses = CallPatch::Losses();

			struct Taken
			{
				Part               part;
				std::vector<Piece> pieces;
			};

			struct Holder
			{
				CallPatch::Owner   owner;
				std::vector<Taken> parts;
			};

			// A piece off only because a piece of another part is taken is no
			// mod's, and its part's line says what it waits on.
			std::vector<Holder> holders;
			for (const auto& loss : losses) {
				for (const auto& cause : loss.causes) {
					if (cause.part != loss.part) {
						continue;
					}
					for (const auto& owner : cause.owners) {
						auto it = std::ranges::find(holders, owner, &Holder::owner);
						if (it == holders.end()) {
							holders.push_back(Holder{ owner, {} });
							it = holders.end() - 1;
						}
						if (it->parts.empty() || it->parts.back().part != loss.part) {
							it->parts.push_back(Taken{ loss.part, {} });
						}
						std::ranges::copy(cause.pieces, std::back_inserter(it->parts.back().pieces));
					}
				}
			}
			if (holders.empty()) {
				return {};
			}
			std::ranges::stable_sort(holders, CallPatch::Before, &Holder::owner);

			std::vector<std::string> lines;
			if (std::ranges::any_of(holders, [](const Holder& a_holder) { return a_holder.owner.sure; })) {
				lines.push_back(Text::MenuTopIntro());
			}
			const auto first = lines.size();
			const auto shown = holders.size() > MOST_OWNER_LINES ? MOST_OWNER_LINES - 1 : holders.size();

			// Each shown mod's parts, kept for a line cut back to fewer.
			std::vector<std::vector<std::string>> parts(shown);
			for (std::size_t i = 0; i < shown; i++) {
				for (auto& taken : holders[i].parts) {
					std::ranges::sort(taken.pieces);
					parts[i].push_back(Text::PartPieces(taken.part, taken.pieces));
				}
				lines.push_back(Text::MenuTopOwner(holders[i].owner, parts[i]));
			}
			if (shown < holders.size()) {
				lines.push_back(Text::MenuTopMore(holders.size() - shown));
			}

			// Too tall for the page, an owner's line is cut back to as many of
			// its parts as fit, and the more line goes, as the rest line says it.
			return Capped(std::move(lines), [&](std::size_t a_line, std::size_t a_room) -> std::optional<std::string> {
				const auto owner = a_line - first;
				for (auto count = a_line >= first && owner < shown ? parts[owner].size() : 0; count > 1; count--) {
					auto shorter = Text::MenuTopOwner(holders[owner].owner, std::span{ parts[owner] }.first(count - 1));
					if (LinesOf(shorter, FitNow()) <= a_room) {
						return shorter;
					}
				}
				return std::nullopt;
			});
		}

		// Whether parts are off and no mod has any of them, only this game
		// version, so removing a mod gets nothing back.
		bool OnlyGameVersion()
		{
			const auto losses = CallPatch::Losses();
			return !losses.empty() && std::ranges::none_of(losses, [](const CallPatch::Loss& a_loss) {
				return std::ranges::any_of(a_loss.causes, [](const CallPatch::Cause& a_cause) {
					return std::ranges::any_of(a_cause.owners, &CallPatch::Owner::sure);
				});
			});
		}

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
		return lead ? std::optional{ !BlockLines(*lead).empty() } : std::nullopt;
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
		return lead ? std::optional{ Grey(BlockLines(*lead)) } : std::nullopt;
	}
}
