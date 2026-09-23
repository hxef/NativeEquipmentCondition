#pragma once

#include "Core/Plugin.h"

#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>

// 3 more log files, for lines that arrive in floods: one per shot, per hit and
// per item spawned, thousands a minute in a fight. NEC.log keeps what was
// patched and what went wrong. The rest goes to:
//
//   NEC.trace.log      the player's game: shots, wear, damage, spawns
//   NEC.ui.trace.log   the menus: what was drawn, where, from what
//   NEC.npc.trace.log  everybody else's fighting
//
// All 4 sit in Documents\My Games\Fallout4\F4SE. bTraceLogs switches the 3 on
// together, see Settings.h. Anything that can happen more than once in a
// session goes here, not in NEC.log. UI_TAGS decides between the first 2 files.
// The NPC file is asked for by name through Npc below, since a damage line
// reads the same whoever struck the blow.
//
// A file reads as blocks. A header line with a tag in capitals opens one, SHOT
// or MELEE, with the clock time and the thread. The lines below carry a
// lowercase tag and the milliseconds since the header:
//
//     18:53:01.663  SHOT    Combat Rifle [000DF42E]              thread 2348
//             +0ms  wear    Combat Rifle [000DF42E] 0.914 -> 0.734 (-0.180)
//             +2ms  damage  attack2  health 0.734  90.00 x 0.7477 = 67.29
//           +268ms  hit     Naomi with Combat Rifle [000DF42E]  physical 67.29
//
// A line with no block open becomes a header of its own. A line names its
// thread only when it differs from the header's. A few moments open a block in
// all 3 files at once: the plugin starting, a save loading, a new game and a
// full reset.
namespace TraceLog
{
	// Opens the 3 files beside NEC.log. Call it after F4SE::Init, which creates
	// NEC.log.
	void Open();

	// False until Open has succeeded. Every function below asks first, so a
	// trace call costs almost nothing without a file.
	bool IsOpen();

	// Tags switched off. Each floods the log once its code works: 1 shot
	// rebuilds the card and the price of every weapon carried, the combat
	// health line is repeated by the damage line under it, and a save loading
	// hands thousands of stacks to the characters and containers around the
	// player, which the spawn block counts instead. Take a tag out while
	// working on its code. The check is made at compile time.
	inline constexpr std::string_view QUIET[]{ "card", "price", "health", "loot" };

	// Tags whose lines go to the UI file. Split by tag and not by source file,
	// because what a menu prints is often worked out in gameplay code, such as
	// the item card's damage in HealthDamage/Card.cpp.
	inline constexpr std::string_view UI_TAGS[]{ "menu", "cnd", "card", "price" };

	[[nodiscard]] constexpr bool IsUI(std::string_view a_tag)
	{
		for (const auto tag : UI_TAGS) {
			if (tag == a_tag) {
				return true;
			}
		}
		return false;
	}

	[[nodiscard]] constexpr bool Wanted(std::string_view a_tag)
	{
		for (const auto quiet : QUIET) {
			if (quiet == a_tag) {
				return false;
			}
		}
		return true;
	}

	namespace detail
	{
		// Which of the 3 files a line goes to.
		enum class Target
		{
			kGame,
			kUI,
			kNpc,
		};

		// Where a tag's lines go when nobody asked for the NPC file.
		[[nodiscard]] constexpr Target ByTag(std::string_view a_tag)
		{
			return IsUI(a_tag) ? Target::kUI : Target::kGame;
		}

		void Begin(Target a_target, std::string_view a_tag, std::string_view a_text);
		void Mark(Target a_target, std::string_view a_tag, std::string_view a_text);
		void Group(Target a_target, std::string_view a_tag, std::string_view a_text);
		void Line(Target a_target, std::string_view a_tag, std::string_view a_text);
		void Once(Target a_target, std::string_view a_tag, std::string_view a_text);
		void First(Target a_target, std::string_view a_tag, std::string_view a_text);
		void Count(Target a_target, std::string_view a_tag, std::string_view a_what);

		[[nodiscard]] std::uint32_t Blocks(Target a_target);
	}

	// Opens a new block. Header tags are written in capitals, SHOT or MELEE.
	template <class... T>
	void Begin(std::string_view a_tag, std::format_string<T...> a_fmt, T&&... a_args)
	{
		if (IsOpen()) {
			detail::Begin(detail::ByTag(a_tag), a_tag, std::format(a_fmt, std::forward<T>(a_args)...));
		}
	}

	// Opens a block unless one with this tag is still open, so a run of lines
	// that belong together, such as a burst of spawns, shares one header.
	inline void Group(std::string_view a_tag, std::string_view a_text)
	{
		if (IsOpen()) {
			detail::Group(detail::ByTag(a_tag), a_tag, a_text);
		}
	}

	// One line into the open block. Line tags are lowercase, wear or hit. A tag
	// in QUIET writes nothing.
	template <class... T>
	void Line(std::string_view a_tag, std::format_string<T...> a_fmt, T&&... a_args)
	{
		if (Wanted(a_tag) && IsOpen()) {
			detail::Line(detail::ByTag(a_tag), a_tag, std::format(a_fmt, std::forward<T>(a_args)...));
		}
	}

	// Line for code the game runs over and over on the same thing, a list
	// sorted or a card drawn every frame. Written once, then again after a
	// pause.
	template <class... T>
	void Once(std::string_view a_tag, std::format_string<T...> a_fmt, T&&... a_args)
	{
		if (Wanted(a_tag) && IsOpen()) {
			detail::Once(detail::ByTag(a_tag), a_tag, std::format(a_fmt, std::forward<T>(a_args)...));
		}
	}

	// Line for what the game works out the same way all session, such as a
	// card. Written the first time its words come up after a save loads, so
	// it comes again only once something in it changes.
	template <class... T>
	void First(std::string_view a_tag, std::format_string<T...> a_fmt, T&&... a_args)
	{
		if (Wanted(a_tag) && IsOpen()) {
			detail::First(detail::ByTag(a_tag), a_tag, std::format(a_fmt, std::forward<T>(a_args)...));
		}
	}

	// Counts one of a kind of thing the open block has too many of to list,
	// "already set" for a stack a spawn leaves alone, so it goes after a Group
	// or a Begin. The block ends with one line giving every count, written as
	// the next block opens.
	inline void Count(std::string_view a_tag, std::string_view a_what)
	{
		if (Wanted(a_tag) && IsOpen()) {
			detail::Count(detail::ByTag(a_tag), a_tag, a_what);
		}
	}

	// Opens a block in all 3 files at once. Every line First wrote may come
	// again after it.
	template <class... T>
	void Mark(std::string_view a_tag, std::format_string<T...> a_fmt, T&&... a_args)
	{
		if (IsOpen()) {
			const auto text = std::format(a_fmt, std::forward<T>(a_args)...);
			detail::Mark(detail::Target::kGame, a_tag, text);
			detail::Mark(detail::Target::kUI, a_tag, text);
			detail::Mark(detail::Target::kNpc, a_tag, text);
		}
	}

	// The same for NEC.npc.trace.log. QUIET applies here too.
	namespace Npc
	{
		template <class... T>
		void Begin(std::string_view a_tag, std::format_string<T...> a_fmt, T&&... a_args)
		{
			if (IsOpen()) {
				detail::Begin(detail::Target::kNpc, a_tag, std::format(a_fmt, std::forward<T>(a_args)...));
			}
		}

		template <class... T>
		void Line(std::string_view a_tag, std::format_string<T...> a_fmt, T&&... a_args)
		{
			if (Wanted(a_tag) && IsOpen()) {
				detail::Line(detail::Target::kNpc, a_tag, std::format(a_fmt, std::forward<T>(a_args)...));
			}
		}

		// How many blocks this file has opened. 2 lines with the same count are
		// in the same block.
		[[nodiscard]] inline std::uint32_t Blocks()
		{
			return detail::Blocks(detail::Target::kNpc);
		}
	}

	// The files a line about somebody's item goes to: the NPC file for theirs,
	// the player's files for the player's own. For a reader both share, so it
	// writes each line once: TraceLog::For(a_theirs).Line("wear", ...).
	struct Files
	{
		bool theirs{ false };

		template <class... T>
		void Begin(std::string_view a_tag, std::format_string<T...> a_fmt, T&&... a_args) const
		{
			if (theirs) {
				Npc::Begin(a_tag, a_fmt, std::forward<T>(a_args)...);
			} else {
				TraceLog::Begin(a_tag, a_fmt, std::forward<T>(a_args)...);
			}
		}

		template <class... T>
		void Line(std::string_view a_tag, std::format_string<T...> a_fmt, T&&... a_args) const
		{
			if (theirs) {
				Npc::Line(a_tag, a_fmt, std::forward<T>(a_args)...);
			} else {
				TraceLog::Line(a_tag, a_fmt, std::forward<T>(a_args)...);
			}
		}
	};

	[[nodiscard]] constexpr Files For(bool a_theirs) noexcept
	{
		return Files{ a_theirs };
	}

	// An actor in a line: the name the player sees and the form ID in brackets,
	// since a fight is often several actors with 1 name. The name is looked up
	// as the line is written, so a switched off line never calls into the
	// engine for it.
	struct Who
	{
		const RE::TESForm* form{ nullptr };
	};

	namespace detail
	{
		[[nodiscard]] std::string Name(const RE::TESForm* a_form);
	}
}

// Lets a TraceLog::Who stand in a format string wherever a name would.
template <>
struct std::formatter<TraceLog::Who> : std::formatter<std::string_view>
{
	auto format(const TraceLog::Who& a_who, std::format_context& a_context) const
	{
		return std::formatter<std::string_view>::format(TraceLog::detail::Name(a_who.form), a_context);
	}
};
