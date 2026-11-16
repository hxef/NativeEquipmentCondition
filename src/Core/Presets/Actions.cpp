#include "Core/Presets/Shared.h"

#include "Core/Utf8.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

// What NEC's main MCM page does with presets: a pick, Reset preset, Delete
// preset and Save as new preset. Each caller writes the changes the page
// still owes first, so a change made just before lands in its own file.
namespace Presets
{
	namespace
	{
		// -------------------------------------------------------------------
		// A typed name
		// -------------------------------------------------------------------

		// The longest name a save keeps, in letters, any _2 included. MCM's
		// list draws about 15 letters at full size and shrinks a longer name.
		constexpr std::size_t MOST_LETTERS = 20;

		// The highest number Free adds to a taken name, as in _999.
		constexpr std::size_t MOST_NUMBER = 999;

		// What a name loses: the characters Windows forbids, dots, & and $
		// (Scaleform translates a text that starts with $), brackets, since
		// Hard (Custom) would pass for a changed preset, and ; = #, since
		// sPreset keeps the name in an ini.
		constexpr std::string_view DROPPED = "<>:\"/\\|?*.&$()[]{};=#";

		// The spaces besides ' ' a typed name may hold, each read as ' ',
		// such as the no-break space and the full width space of a Japanese
		// or Chinese input.
		constexpr char32_t SPACES[]{
			0x00A0,
			0x1680,
			0x2000,
			0x2001,
			0x2002,
			0x2003,
			0x2004,
			0x2005,
			0x2006,
			0x2007,
			0x2008,
			0x2009,
			0x200A,
			0x2028,
			0x2029,
			0x202F,
			0x205F,
			0x3000,
		};

		// Marks that take no room, which a name loses.
		constexpr char32_t UNSEEN[]{ 0x200B, 0x200C, 0x200D, 0x2060, 0xFEFF };

		// Names Windows keeps for devices, and Default's own.
		constexpr std::wstring_view TAKEN[]{
			L"Default",
			L"CON",
			L"PRN",
			L"AUX",
			L"NUL",
			L"COM1",
			L"COM2",
			L"COM3",
			L"COM4",
			L"COM5",
			L"COM6",
			L"COM7",
			L"COM8",
			L"COM9",
			L"LPT1",
			L"LPT2",
			L"LPT3",
			L"LPT4",
			L"LPT5",
			L"LPT6",
			L"LPT7",
			L"LPT8",
			L"LPT9",
		};

		// a_text cut to a_most letters, without an _ left at the end.
		std::string Cut(std::string_view a_text, std::size_t a_most)
		{
			std::size_t end = 0;
			for (std::size_t letters = 0; end < a_text.size() && letters < a_most; ++letters) {
				end += std::max<std::size_t>(Utf8::LetterSize(a_text.substr(end)), 1);
			}
			std::string out{ a_text.substr(0, std::min(end, a_text.size())) };
			while (out.ends_with('_')) {
				out.pop_back();
			}
			return out;
		}

		// A typed name as a file name: spaces at the ends go, each other
		// space becomes _, and DROPPED, UNSEEN and control characters go. Any
		// of SPACES counts as a space. "My Run." gives My_Run. A name with
		// broken UTF-8 gives nothing, so Save refuses it as a name with no
		// letter.
		std::string Cleaned(std::string_view a_typed)
		{
			if (Utf8::BadAt(a_typed) != std::string_view::npos) {
				return {};
			}
			std::string out;
			std::size_t spaces = 0;
			for (std::size_t at = 0; at < a_typed.size();) {
				// Every letter is whole, as BadAt checked above.
				const auto size = Utf8::LetterSize(a_typed.substr(at));
				const auto letter = a_typed.substr(at, size);
				at += size;
				const auto point = Utf8::CodePoint(letter);
				if (Utf8::IsControl(letter) || std::ranges::contains(UNSEEN, point) || (size == 1 && DROPPED.contains(letter[0]))) {
					continue;
				}
				if (point == U' ' || std::ranges::contains(SPACES, point)) {
					// Written only before a letter that follows, so the ends
					// lose theirs.
					spaces += out.empty() ? 0 : 1;
					continue;
				}
				out.append(spaces, '_');
				spaces = 0;
				out += letter;
			}
			return Cut(out, MOST_LETTERS);
		}

		// Whether a name has a letter or a digit. A byte from 0x80 is part
		// of a letter that is not English.
		bool HasLetter(std::string_view a_name)
		{
			return std::ranges::any_of(a_name, [](char a_char) {
				const auto byte = static_cast<unsigned char>(a_char);
				return byte >= 0x80 || (byte >= '0' && byte <= '9') || ((byte | 0x20) >= 'a' && (byte | 0x20) <= 'z');
			});
		}

		// a_name, or with _2, _3 and so on when that name is taken, so a
		// save never writes over a file. Taken is any .ini name in either
		// folder, Default in English and in a_default, a device name, and a
		// file the file system says is there.
		std::optional<std::string> Free(const std::string& a_name, const Scanned& a_scan, std::string_view a_default)
		{
			std::wstring local;
			std::string underscored{ a_default };
			std::ranges::replace(underscored, ' ', '_');
			REX::UTF8_TO_UTF16(underscored, local);
			const auto taken = [&](const std::string& a_try) {
				std::wstring wide;
				if (!REX::UTF8_TO_UTF16(a_try, wide)) {
					return true;
				}
				// A name that lists as a taken one is taken too, so New Vegas
				// is while NEC_New_Vegas.ini is there.
				const auto same = [&](std::wstring_view a_other) {
					return SameFile(wide, a_other) || SameFile(Unprefixed(wide), Unprefixed(a_other));
				};
				std::error_code ec;
				return std::ranges::any_of(TAKEN, same) || (!local.empty() && same(local)) || std::ranges::any_of(a_scan.names, same) ||
				       std::filesystem::exists(PathOf(CUSTOM, a_try + ".ini"), ec);
			};
			if (!taken(a_name)) {
				return a_name;
			}
			for (std::size_t number = 2; number <= MOST_NUMBER; ++number) {
				const auto end = std::format("_{:d}", number);
				const auto next = Cut(a_name, MOST_LETTERS - end.size()) + end;
				if (!taken(next)) {
					return next;
				}
			}
			return std::nullopt;
		}
	}

	// -------------------------------------------------------------------
	// A pick
	// -------------------------------------------------------------------

	Changed Pick(std::size_t a_index)
	{
		Changed changed;
		{
			auto& state = Now();
			const std::scoped_lock l{ state.lock };
			if (a_index >= state.shown.size()) {
				return {};
			}
			auto next = state.shown[a_index];
			if (SameEntry(next, state.picked)) {
				// The entry in play changes nothing, but Default picked again
				// lets go of a preset the game start found gone.
				const auto& [section, key] = Settings::PRESET_KEY;
				if (next.kind == Kind::kDefault && !state.dangling.empty() && Settings::Drop(section, key)) {
					REX::INFO("NEC_custom.ini no longer picks the preset {:s}, so Default stays.", state.dangling);
					state.dangling.clear();
				}
				return {};
			}

			const auto old = ShownOf(state.picked);
			const auto applied = Apply(next);
			next.changed = applied.changes;
			Settings::LogChange("Preset", old, ShownOf(next), "from the MCM page");
			Take(state, next, applied);
			if (next.kind != Kind::kDefault) {
				Say(applied.lines);
			}
			changed = applied.changed;
		}
		Settings::ReportLine(false);
		return changed;
	}

	// -------------------------------------------------------------------
	// Reset preset and Delete preset
	// -------------------------------------------------------------------

	Changed Reset()
	{
		Changed changed;
		{
			auto& state = Now();
			const std::scoped_lock l{ state.lock };
			const auto picked = state.picked;
			if (picked.kind == Kind::kOwn || picked.changed == 0) {
				return {};
			}
			const auto name = LogName(picked.changes);
			Lines lines;
			if (picked.kind == Kind::kDefault) {
				// Every other line stays, sPreset and the UI page's keys too.
				const auto dropped = Settings::DropAll(picked.changes, HeldNames());
				if (!dropped) {
					return {};
				}
				lines.push_back({ false, std::format("Preset Default reset from the MCM page, so NEC_custom.ini no longer sets {:s}.", Count(*dropped, "setting")) });
			} else if (std::string reason; Remove(picked.changes, reason)) {
				lines.push_back({ false, std::format("Preset {:s} reset from the MCM page. {:s}, which set {:s}, is removed.", picked.name, name, Count(picked.changed, "setting")) });
			} else if (const auto dropped = Settings::DropAll(picked.changes, HeldNames())) {
				// Taken out instead, so the reset holds and the file keeps its
				// comments and every key a preset does not hold.
				lines.push_back({ true, std::format("{:s} could not be removed ({:s}), so NEC took its {:s} out of it instead.", name, reason, Count(*dropped, "setting")) });
			} else {
				REX::WARN("{:s} could not be removed ({:s}), so {:s} keeps your changes.", name, reason, picked.name);
				return {};
			}

			const auto applied = Apply(picked);
			state.picked.changed = applied.changes;
			state.original = applied.original;
			state.given = applied.given;
			state.about = applied.about;
			if (picked.kind == Kind::kOriginal) {
				lines.insert(lines.end(), applied.lines.begin(), applied.lines.end());
			}
			Say(lines);
			changed = applied.changed;
		}
		Settings::ReportLine(false);
		return changed;
	}

	std::optional<Changed> Delete()
	{
		Changed changed;
		{
			auto& state = Now();
			const std::scoped_lock l{ state.lock };
			const auto picked = state.picked;
			if (picked.kind != Kind::kOwn) {
				return Changed{};
			}
			// A failed removal changes nothing: the preset, its file and the
			// pick stay.
			const auto name = LogName(picked.file);
			std::string reason;
			if (!Remove(picked.file, reason)) {
				REX::WARN("{:s} could not be removed ({:s}), so {:s} stays. Remove it by hand, or in Mod Organizer 2 from the mod that holds it.", name, reason, picked.name);
				return std::nullopt;
			}
			// An own file records no preset it came from, so Default is the
			// one fixed place to land.
			const auto fallback = DefaultEntry();
			const auto applied = Apply(fallback);
			REX::INFO("Preset {:s} deleted from the MCM page. {:s} is removed, and Default is picked.", picked.name, name);
			Take(state, fallback, applied);
			changed = applied.changed;
		}
		Settings::ReportLine(false);
		return changed;
	}

	// -------------------------------------------------------------------
	// Save as new preset
	// -------------------------------------------------------------------

	Saved Save(std::string_view a_typed, std::string_view a_default)
	{
		const auto name = Cleaned(a_typed);
		if (!HasLetter(name)) {
			REX::WARN("The name typed for a new preset has no letter or digit a file name can take, so nothing is saved.");
			return { .end = SaveEnd::kNoName, .name = {} };
		}

		auto& state = Now();
		const std::scoped_lock l{ state.lock };
		const auto free = Free(name, Scan(), a_default);
		const auto file = PathOf(CUSTOM, free.value_or(name) + ".ini");
		const auto shown = Shown(free.value_or(name));
		const auto from = ShownOf(state.picked);
		std::size_t count = 0;
		// The reason while no free name is left, which a failed write replaces.
		auto reason = std::format("every name up to _{:d} is taken", MOST_NUMBER);
		if (!free || !WriteNew(file, OwnTextHeld(state, shown, from, {}, count), reason)) {
			REX::WARN("{:s} could not be written ({:s}), so no preset is saved.", LogName(file), reason);
			return { .end = SaveEnd::kNotWritten, .name = shown };
		}

		// Its file holds the settings in play, so no value changes.
		Applied kept;
		kept.original.resize(HeldSettings().size());
		kept.given = state.given;
		Take(state, { .name = shown, .stem = *free, .kind = Kind::kOwn, .file = file, .changes = {} }, kept);
		REX::INFO("Preset {:s} saved in {:s} from {:s}, with {:s} that {:s} from NEC.ini.", shown, LogName(file), from,
			Count(count, "setting"), count == 1 ? "differs" : "differ");
		return { .end = SaveEnd::kSaved, .name = shown };
	}
}
