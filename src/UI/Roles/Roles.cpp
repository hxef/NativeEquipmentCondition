#include "UI/Roles/Roles.h"

#include "Core/TraceLog.h"
#include "UI/Flash.h"
#include "UI/MenuMovies.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>

namespace Roles
{
	namespace
	{
		// -------------------------------------------------------------------
		// What has been said, per movie file
		// -------------------------------------------------------------------

		using Parts = std::set<std::string, std::less<>>;

		// The parts a movie load has written a found or noted line for. Cleared
		// when a movie with the same file name loads again.
		struct Said
		{
			Parts found;
			Parts noted;
		};

		// Both maps sit behind 1 lock, since nothing promises that a movie
		// loads on the thread its menu reads parts on. Keyed by the file name
		// in lower case, since the game spells its own file names either way.
		std::mutex                             g_lock;
		std::unordered_map<std::string, Said>  g_said;
		std::unordered_map<std::string, Parts> g_warned;

		[[nodiscard]] std::string KeyOf(std::string_view a_file)
		{
			std::string key{ a_file };
			std::ranges::transform(key, key.begin(), [](unsigned char a_char) {
				return static_cast<char>(std::tolower(a_char));
			});
			return key;
		}

		// The movie's file name for a line and a key, HUDMenu.swf, or a stand in
		// when the game gives none. Every function here goes through it, so a
		// movie load clears the same key its lines sit under.
		[[nodiscard]] std::string_view NameOf(std::string_view a_file)
		{
			return a_file.empty() ? "a menu movie"sv : a_file;
		}

		// Adds a_part to a_set, true when it was not there yet.
		[[nodiscard]] bool First(Parts& a_set, std::string_view a_part)
		{
			return a_set.emplace(a_part).second;
		}
	}

	// -------------------------------------------------------------------
	// A movie loads
	// -------------------------------------------------------------------

	void OnMovieLoaded(Scaleform::GFx::Movie&, std::string_view a_file)
	{
		const std::scoped_lock lock{ g_lock };
		g_said.erase(KeyOf(NameOf(a_file)));
	}

	// -------------------------------------------------------------------
	// Whether a part is on screen
	// -------------------------------------------------------------------

	bool OnScreen(Scaleform::GFx::Movie& a_movie, Value& a_clip)
	{
		// A NaN alpha fails too, since it is never above 0.
		return a_clip.IsDisplayObject() && Flash::Opacity(a_clip) > 0.0 && InFrame(a_movie, a_clip);
	}

	bool InFrame(Scaleform::GFx::Movie& a_movie, Value& a_clip)
	{
		// A clip outside the display list is not shown.
		const auto bounds = Flash::StageBounds(a_clip);
		if (!bounds) {
			return false;
		}
		const auto left = Flash::Number(*bounds, "x"sv);
		const auto top = Flash::Number(*bounds, "y"sv);
		const auto right = left + Flash::Number(*bounds, "width"sv);
		const auto bottom = top + Flash::Number(*bounds, "height"sv);
		if (!std::isfinite(right) || !std::isfinite(bottom)) {
			return false;
		}

		// The part of the stage the screen shows, however the movie is scaled.
		// A clip only touching its edge counts as off screen.
		const auto shown = a_movie.GetVisibleFrameRect();
		return left < shown.x2 && right > shown.x1 && top < shown.y2 && bottom > shown.y1;
	}

	// -------------------------------------------------------------------
	// The found, noted and missing lines
	// -------------------------------------------------------------------

	// Reads as "BarterMenu.swf: button bar found at vanilla's place".
	void Found(Scaleform::GFx::Movie& a_movie, std::string_view a_part, std::string_view a_how)
	{
		if (!TraceLog::IsOpen()) {
			return;
		}
		const auto file = NameOf(MenuMovies::FileOf(a_movie));
		{
			const std::scoped_lock lock{ g_lock };
			if (!First(g_said[KeyOf(file)].found, a_part)) {
				return;
			}
		}
		TraceLog::Line("menu", "{:s}: {:s} found {:s}", file, a_part, a_how);
	}

	// Reads as "HUDMenu.swf: no ammo divider found, so the CND bar keeps its
	// last place".
	void Noted(Scaleform::GFx::Movie& a_movie, std::string_view a_part, std::string_view a_effect)
	{
		if (!TraceLog::IsOpen()) {
			return;
		}
		const auto file = NameOf(MenuMovies::FileOf(a_movie));
		{
			const std::scoped_lock lock{ g_lock };
			if (!First(g_said[KeyOf(file)].noted, a_part)) {
				return;
			}
		}
		TraceLog::Line("menu", "{:s}: no {:s} found, so {:s}", file, a_part, a_effect);
	}

	// Reads as "HUDMenu.swf: no ammo counter found, so the HUD shows no CND
	// bar outside power armor." in NEC.log, once per game start.
	void Missing(Scaleform::GFx::Movie& a_movie, std::string_view a_part, std::string_view a_effect)
	{
		const auto file = NameOf(MenuMovies::FileOf(a_movie));
		bool       first = false;
		{
			const std::scoped_lock lock{ g_lock };
			first = First(g_warned[KeyOf(file)], a_part);
		}
		if (first) {
			REX::WARN("{:s}: no {:s} found, so {:s}.", file, a_part, a_effect);
		} else {
			Noted(a_movie, a_part, a_effect);
		}
	}
}
