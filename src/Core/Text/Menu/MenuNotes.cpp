#include "Core/Text/Text.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/Text/Lines.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The grey lines of the MCM page about parts left to other mods, see
// UI/Mcm/Notes.cpp, and the words that join a list. NEC.log reads the English
// of the part lines.
namespace Text
{
	std::vector<std::string> PartLines(const CallPatch::Loss& a_loss, Out a_out)
	{
		std::vector<std::string> lines;
		for (const auto& cause : a_loss.causes) {
			const auto title = PartPieces(a_loss.part, cause.pieces, a_out);
			const auto owners = CallPatch::OwnerNames(cause.owners, PickFor(OWNER_MOD, a_out));
			// No mod took it, every owner is this game version.
			if (owners.empty()) {
				lines.push_back(SayFor(a_out, PART_VERSION, title, CallPatch::GameVersion()));
			} else if (cause.part != a_loss.part) {
				lines.push_back(SayFor(a_out, cause.waits ? PART_SINCE_NOW : PART_SINCE, title, PartPieces(cause.part, cause.taken, a_out),
					List(owners, a_out)));
			} else {
				lines.push_back(SayFor(a_out, cause.waits ? PART_OFF_NOW : PART_OFF, title, List(owners, a_out)));
			}
		}
		if (!lines.empty() && !a_loss.works.empty()) {
			lines.back() = Sentences(lines.back(), StillWorks(a_loss.works, a_out), a_out);
		}
		return lines;
	}

	std::string MenuNoEffect(std::span<const std::string> a_names, Out a_out)
	{
		return SayFor(a_out, NO_EFFECT, Commas(a_names, a_out));
	}

	std::string MenuTopIntro()
	{
		return Pick(TOP_INTRO);
	}

	std::string MenuTopOwner(const CallPatch::Owner& a_owner, std::span<const std::string> a_parts)
	{
		const auto list = Commas(a_parts, Out::kPage);
		if (!a_owner.dll.empty()) {
			return Say(TOP_BY, a_owner.dll, list);
		}
		return a_owner.sure ? Say(TOP_MOD, list) : Say(TOP_VERSION, CallPatch::GameVersion(), list);
	}

	std::string MenuTopMore(std::size_t a_count)
	{
		return Say(TOP_MORE, a_count);
	}

	std::string MenuNoteHelp()
	{
		return Pick(NOTE_HELP);
	}

	std::string MenuVersionHelp()
	{
		return Pick(NOTE_HELP_VERSION);
	}

	std::string MenuNoteRest()
	{
		return Pick(NOTE_REST);
	}

	std::string List(std::span<const std::string> a_items, Out a_out)
	{
		if (a_items.empty()) {
			return {};
		}
		std::string list = a_items.front();
		for (std::size_t i = 1; i < a_items.size(); i++) {
			list = SayFor(a_out, i + 1 == a_items.size() ? LIST_AND : LIST_COMMA, list, a_items[i]);
		}
		return list;
	}

	std::string Commas(std::span<const std::string> a_items, Out a_out)
	{
		if (a_items.empty()) {
			return {};
		}
		std::string list = a_items.front();
		for (std::size_t i = 1; i < a_items.size(); i++) {
			list = SayFor(a_out, LIST_COMMA, list, a_items[i]);
		}
		return list;
	}
}
