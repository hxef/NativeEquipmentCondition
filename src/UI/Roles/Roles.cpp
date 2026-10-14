#include "UI/Roles/Roles.h"

#include "Core/TraceLog.h"
#include "UI/Flash.h"
#include "UI/MenuMovies.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
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

		// The movie's file name for a line and a key, HUDMenu.swf, or a stand
		// in when the game gives none. Every function here goes through it, so
		// a movie load clears the same key its lines sit under.
		[[nodiscard]] std::string_view NameOf(std::string_view a_file)
		{
			return a_file.empty() ? "a menu movie"sv : a_file;
		}

		// Adds a_part to a_set, true when it was not there yet.
		[[nodiscard]] bool First(Parts& a_set, std::string_view a_part)
		{
			return a_set.emplace(a_part).second;
		}

		// a_point through the clip's localToGlobal or globalToLocal, a_how.
		[[nodiscard]] std::optional<Point> Mapped(Scaleform::GFx::Movie& a_movie, Value& a_clip, const char* a_how, const Point& a_point)
		{
			const std::array args{ Value(a_point.x), Value(a_point.y) };
			Value             given;
			Value             mapped;
			a_movie.CreateObject(&given, "flash.geom.Point", args.data(), static_cast<std::uint32_t>(args.size()));
			if (!given.IsObject() || !a_clip.IsDisplayObject() || !a_clip.Invoke(a_how, &mapped, &given, 1) || !mapped.IsObject()) {
				return std::nullopt;
			}
			const Point point{ Flash::Number(mapped, "x"sv), Flash::Number(mapped, "y"sv) };
			return std::isfinite(point.x) && std::isfinite(point.y) ? std::optional{ point } : std::nullopt;
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
	// A part's box
	// -------------------------------------------------------------------

	std::optional<Box> BoundsIn(Value& a_clip, Value& a_space)
	{
		Value bounds;
		if (!a_clip.IsDisplayObject() || !a_space.IsDisplayObject() ||
			!a_clip.Invoke("getBounds", &bounds, &a_space, 1) || !bounds.IsObject()) {
			return std::nullopt;
		}
		const auto left = Flash::Number(bounds, "x"sv);
		const auto top = Flash::Number(bounds, "y"sv);
		const auto width = Flash::Number(bounds, "width"sv);
		const auto height = Flash::Number(bounds, "height"sv);
		if (!std::isfinite(left) || !std::isfinite(top) || !(width > 0.0) || !(height > 0.0) ||
			!std::isfinite(width) || !std::isfinite(height)) {
			return std::nullopt;
		}
		return Box{ left, top, left + width, top + height };
	}

	std::optional<Box> FrameIn(Scaleform::GFx::Movie& a_movie, Value& a_clip)
	{
		// 3 corners of the frame go from the stage into the clip's units, the
		// 3rd to tell a turned clip.
		const auto shown = a_movie.GetVisibleFrameRect();
		const auto first = FromStage(a_movie, a_clip, Point{ shown.x1, shown.y1 });
		const auto last = FromStage(a_movie, a_clip, Point{ shown.x2, shown.y2 });
		const auto side = FromStage(a_movie, a_clip, Point{ shown.x1, shown.y2 });
		if (!first || !last || !side) {
			return std::nullopt;
		}

		// Unturned, the frame's lower left corner keeps the upper left's x
		// and the lower right's y.
		constexpr double LEEWAY = 0.01;
		if (std::abs(side->x - first->x) > LEEWAY || std::abs(side->y - last->y) > LEEWAY) {
			return std::nullopt;
		}
		const Box frame{ std::min(first->x, last->x), std::min(first->y, last->y), std::max(first->x, last->x), std::max(first->y, last->y) };
		return frame.right > frame.left && frame.bottom > frame.top ? std::optional{ frame } : std::nullopt;
	}

	// -------------------------------------------------------------------
	// A point to and from the stage
	// -------------------------------------------------------------------

	std::optional<Point> ToStage(Scaleform::GFx::Movie& a_movie, Value& a_clip, const Point& a_point)
	{
		return Mapped(a_movie, a_clip, "localToGlobal", a_point);
	}

	std::optional<Point> FromStage(Scaleform::GFx::Movie& a_movie, Value& a_clip, const Point& a_point)
	{
		return Mapped(a_movie, a_clip, "globalToLocal", a_point);
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
