#include "Core/CallPatch/Ledger.h"

#include "Core/Feature.h"
#include "Core/Pieces.h"
#include "Core/Text/Text.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <iterator>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

// The summary lines NEC.log writes after the Settings line: what is left to
// other mods, what NEC shares with them, and what that leaves of each
// setting. Written again whenever a recheck changes what they say. The last
// summary written is kept, so a change from the MCM page writes it again only
// when it reads differently.
namespace CallPatch
{
	namespace
	{
		std::mutex               g_writtenLock;
		std::vector<std::string> g_written;

		// The lines of the settings with pieces off, 1 a setting: "Weapon wear
		// speed: no effect on gun wear from firing for now. Still changes
		// melee wear.", then "No effect for now: Workbench repair cost." A
		// switch left to another mod says so on the Settings line, so only
		// the settings under it are named here.
		std::vector<std::string> EffectLines()
		{
			std::vector<std::string> lines;
			std::vector<std::string> none;
			for (const auto& link : SettingLinks()) {
				const auto switches = Settings::Switches();
				const auto on = std::ranges::find(switches, link.setting, [](const auto* a_on) { return static_cast<const Settings::Named*>(a_on); });
				if (on != switches.end() && IsYielded(**on)) {
					continue;
				}
				const auto effect = EffectOf(*link.setting);
				if (effect.idle) {
					continue;
				}
				if (effect.None()) {
					none.push_back(Text::SettingLogName(*link.setting));
					continue;
				}
				// 2 settings can say the same sentence, which is said once.
				for (auto& line : Text::SettingLines(link, effect, Text::Out::kLog)) {
					if (std::ranges::find(lines, line) == lines.end()) {
						lines.push_back(std::move(line));
					}
				}
			}
			if (!none.empty()) {
				lines.push_back(Text::MenuNoEffect(none, Text::Out::kLog));
			}
			return lines;
		}
	}

	std::string LeftLine()
	{
		// A part only this game version keeps off is no other mod's, so it
		// gets a sentence of its own. The pieces come before the owners. A
		// piece off only because a piece of another part is taken is named in
		// its part's line, not here.
		std::string mods;
		std::string version;
		for (const auto& loss : Losses()) {
			const auto name = Text::PartLogName(loss.part);
			for (const auto& cause : loss.causes) {
				const auto whole = Whole(loss.part, cause.pieces);
				if (cause.part != loss.part) {
					continue;
				}
				if (OnlyVersion(cause.owners)) {
					std::format_to(std::back_inserter(version), "{:s}{:s}", version.empty() ? "" : ", ",
						Text::PartPieces(loss.part, cause.pieces, Text::Out::kLog));
				} else if (whole) {
					std::format_to(std::back_inserter(mods), "{:s}{:s} ({:s})", mods.empty() ? "" : ", ", name, Words(cause.owners));
				} else {
					std::format_to(std::back_inserter(mods), "{:s}{:s} ({:s}, {:s})", mods.empty() ? "" : ", ", name,
						Text::PieceNames(cause.pieces, Text::Out::kLog), Words(cause.owners));
				}
			}
		}
		std::string line;
		if (!mods.empty()) {
			line = std::format("Left to other mods: {:s}.", mods);
		}
		if (!version.empty()) {
			std::format_to(std::back_inserter(line), "{:s}Off on game version {:s}, which is not supported: {:s}.", line.empty() ? "" : " ",
				GameVersion(), version);
		}
		return line;
	}

	std::string SharedLine()
	{
		const std::scoped_lock l{ Lock() };

		// The DLLs NEC shares a part with: the ones it runs on top of, and the
		// ones that hooked over it and still hand the call on. Each once, in
		// the order of Before.
		std::string shared;
		for (auto i = static_cast<std::size_t>(Part::kPerks); i < static_cast<std::size_t>(Part::kTrace); i++) {
			const auto         part = static_cast<Part>(i);
			std::vector<Owner> owners;
			for (const auto& place : Places()) {
				// A place still working with another DLL: on top of one, or
				// shared with one while its set runs. A place whose set a cut
				// stopped or that waits, whose row's switch is left to another
				// mod, or whose pieces are all off anyway, see PiecesOffTail,
				// does nothing there, so it is not counted.
				if (place.part != part || IsOff(place) || RunOf(place.set) != Run::kLive ||
					(place.row->on && YieldedLocked(place.row->on)) || (place.under.empty() && place.line.empty()) ||
					!PiecesOffTail(place).empty()) {
					continue;
				}
				// The DLLs NEC runs on top of and the ones over it, each once.
				for (const auto* from : { &place.under, &place.line }) {
					for (const auto& owner : *from) {
						if (std::ranges::find(owners, owner) == owners.end()) {
							owners.push_back(owner);
						}
					}
				}
			}
			if (owners.empty()) {
				continue;
			}
			std::ranges::sort(owners, Before);
			std::format_to(std::back_inserter(shared), "{:s}{:s} ({:s})", shared.empty() ? "" : ", ", Text::PartLogName(part),
				Words(owners));
		}
		return shared.empty() ? std::string{} : std::format("Shared with other mods: {:s}.", shared);
	}

	std::vector<std::string> Summary()
	{
		std::vector<std::string> lines;
		for (auto line : { LeftLine(), SharedLine() }) {
			if (!line.empty()) {
				lines.push_back(std::move(line));
			}
		}
		std::ranges::move(EffectLines(), std::back_inserter(lines));
		return lines;
	}

	bool KeepSummary(const std::vector<std::string>& a_summary)
	{
		const std::scoped_lock l{ g_writtenLock };
		if (a_summary == g_written) {
			return false;
		}
		g_written = a_summary;
		return true;
	}
}
