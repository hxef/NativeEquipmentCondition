#include "UI/Mcm/Bridge.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/Parts.h"
#include "Core/Pieces.h"
#include "Core/Text/Text.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <vector>

// The grey list at the top of NEC's main page: each mod with the parts it
// took that no block's note names.
namespace Mcm
{
	namespace
	{
		// The most lines the top list gives the mods, the last one saying how
		// many more there are when they do not fit.
		constexpr std::size_t MOST_OWNER_LINES = 8;

		// -------------------------------------------------------------------
		// The list at the top
		// -------------------------------------------------------------------

		// Every piece a block's note names as left to an owner. config.json
		// gives every block lead a note.
		std::vector<Left> InBlocks()
		{
			std::vector<Left> left;
			for (const auto& link : SettingLinks()) {
				if (!link.under) {
					std::ranges::move(BlockOf(link).left, std::back_inserter(left));
				}
			}
			return left;
		}
	}

	// The list at the top: its first line while a mod has a part, then
	// each mod with the parts it took that no block's note names, a part
	// with some pieces taken naming them, in the order of
	// CallPatch::Before. With only this game version, its own line says
	// why. Empty when the notes name every piece.
	std::vector<std::string> TopLines()
	{
		const auto losses = CallPatch::Losses();
		if (losses.empty()) {
			return {};
		}
		const auto inBlocks = InBlocks();

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
		// mod's, and its part's line says what it waits on. A piece a
		// block's note names with this mod is left to that note.
		std::vector<Holder> holders;
		for (const auto& loss : losses) {
			for (const auto& cause : loss.causes) {
				if (cause.part != loss.part) {
					continue;
				}
				for (const auto& owner : cause.owners) {
					std::vector<Piece> pieces;
					std::ranges::copy_if(cause.pieces, std::back_inserter(pieces), [&](Piece a_piece) {
						return std::ranges::find(inBlocks, Left{ a_piece, owner }) == inBlocks.end();
					});
					if (pieces.empty()) {
						continue;
					}
					auto it = std::ranges::find(holders, owner, &Holder::owner);
					if (it == holders.end()) {
						holders.push_back(Holder{ owner, {} });
						it = holders.end() - 1;
					}
					if (it->parts.empty() || it->parts.back().part != loss.part) {
						it->parts.push_back(Taken{ loss.part, {} });
					}
					std::ranges::move(pieces, std::back_inserter(it->parts.back().pieces));
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
		// its parts as fit, and the more line goes, as the rest line says
		// it.
		auto kept = Capped(std::move(lines), [&](std::size_t a_line, std::size_t a_room) -> std::optional<std::string> {
			const auto owner = a_line - first;
			for (auto count = a_line >= first && owner < shown ? parts[owner].size() : 0; count > 1; count--) {
				auto shorter = Text::MenuTopOwner(holders[owner].owner, std::span{ parts[owner] }.first(count - 1));
				if (LinesOf(shorter) <= a_room) {
					return shorter;
				}
			}
			return std::nullopt;
		});
		return std::move(kept.lines);
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
}
