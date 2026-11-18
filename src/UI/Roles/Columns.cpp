#include "UI/Roles/Columns.h"

#include "Core/IniText.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"
#include "UI/MenuMovies.h"
#include "UI/Roles/Conventions.h"

#include <cstddef>
#include <mutex>
#include <optional>
#include <span>
#include <string>

namespace Roles
{
	namespace
	{
		// -------------------------------------------------------------------
		// Finding the keys
		// -------------------------------------------------------------------

		// What a movie answered when its settings class was looked for.
		enum class Shape
		{
			kNone,   // no such class, as in vanilla's movies
			kOther,  // the class is there, a member on the way is not what it should be
			kFound,
		};

		struct Keys
		{
			Shape                       shape = Shape::kNone;
			Value                       settings;  // the class, which also runs the function once loaded
			Value                       holder;    // the object the comma list is a member of
			const Conventions::Columns* convention = nullptr;
		};

		// The class named a_name in the movie's own domain, or nothing. The
		// domain is asked whether it has the class first, since getDefinition
		// throws for a name it lacks.
		[[nodiscard]] std::optional<Value> ClassOf(Scaleform::GFx::Movie& a_movie, const char* a_name)
		{
			auto        domain = Flash::Domain(a_movie);
			const Value name{ a_name };
			Value       has;
			if (!domain.IsObject() || !domain.Invoke("hasDefinition", &has, &name, 1) || !has.IsBoolean() || !has.GetBoolean()) {
				return std::nullopt;
			}
			Value found;
			if (!domain.Invoke("getDefinition", &found, &name, 1) || !found.IsObject()) {
				return std::nullopt;
			}
			return found;
		}

		// The first row of Conventions.h whose class the movie has, walked
		// down to the comma list. The members are read anew at each call,
		// since the movie may swap an object for another when it loads.
		[[nodiscard]] Keys Find(Scaleform::GFx::Movie& a_movie)
		{
			for (const auto& convention : Conventions::COLUMNS) {
				auto settings = ClassOf(a_movie, convention.settings);
				if (!settings) {
					continue;
				}
				const auto config = Flash::Member(*settings, convention.list[0]);
				const auto holder = Flash::Member(config, convention.list[1]);
				if (!holder.IsObject() || !Flash::Member(holder, convention.list[2]).IsString()) {
					return Keys{ .shape = Shape::kOther, .settings = {}, .holder = {}, .convention = nullptr };
				}
				return Keys{ .shape = Shape::kFound, .settings = *settings, .holder = holder, .convention = &convention };
			}
			return Keys{};
		}

		// -------------------------------------------------------------------
		// Adding the row
		// -------------------------------------------------------------------

		// Whether a_list names a_row, split on commas with the spaces around
		// each name trimmed and the case ignored, as the movie reads it.
		[[nodiscard]] bool Names(std::string_view a_list, std::string_view a_row)
		{
			while (true) {
				const auto comma = a_list.find(',');
				if (IniText::SameName(IniText::Trimmed(a_list.substr(0, comma)), a_row)) {
					return true;
				}
				if (comma == std::string_view::npos) {
					return false;
				}
				a_list.remove_prefix(comma + 1);
			}
		}

		// Called by the movie with no arguments once its settings have
		// loaded, at once when they already have. Adds the row to the comma
		// list unless the list names it already, the player's own keys kept
		// as they are.
		class Loaded final : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Hold(std::string_view a_row)
			{
				const std::scoped_lock lock{ lock_ };
				row_ = a_row;
			}

			void Call(const Params& a_params) override
			{
				if (!a_params.movie) {
					return;
				}
				auto keys = Find(*a_params.movie);
				if (keys.shape != Shape::kFound) {
					return;
				}
				std::string row;
				{
					const std::scoped_lock lock{ lock_ };
					row = row_;
				}
				const auto list = Flash::String(keys.holder, keys.convention->list[2]);
				const auto file = MenuMovies::FileOf(*a_params.movie);
				const auto name = file.empty() ? "a menu movie"sv : file;
				if (Names(list, row)) {
					TraceLog::Line("menu", "{:s}: list columns already take {:s}", name, row);
					return;
				}
				const auto joined = IniText::Trimmed(list).empty() ? row : list + "," + row;
				if (Flash::Set(keys.holder, keys.convention->list[2], Value(joined.c_str()))) {
					TraceLog::Line("menu", "{:s}: list columns take {:s}", name, row);
				}
			}

		private:
			std::mutex  lock_;
			std::string row_;  // the card row to add, set before the movie gets the function
		};

		// Lives as long as the plugin, see Flash.h.
		Loaded g_loaded;
	}

	// Reads as "PipboyMenu.swf: list columns found by the movie's own column
	// keys", then "PipboyMenu.swf: list columns take $ItemInfo_CND" once the
	// movie's settings have loaded.
	bool GiveColumn(Scaleform::GFx::Movie& a_movie, std::string_view a_row, bool a_give)
	{
		auto keys = Find(a_movie);
		if (keys.shape == Shape::kNone) {
			return false;
		}
		if (keys.shape == Shape::kOther) {
			Noted(a_movie, "list column keys"sv, "its lists get no CND column"sv);
			return false;
		}
		Found(a_movie, "list columns"sv, "by the movie's own column keys"sv);
		if (!a_give) {
			return true;
		}

		g_loaded.Hold(a_row);
		Value fn;
		a_movie.CreateFunction(&fn, &g_loaded);
		if (!Flash::Call(keys.settings, keys.convention->loaded, std::span{ &fn, std::size_t{ 1 } })) {
			Noted(a_movie, "way to add a list column"sv, "its lists get no CND column"sv);
			return false;
		}
		return true;
	}
}
