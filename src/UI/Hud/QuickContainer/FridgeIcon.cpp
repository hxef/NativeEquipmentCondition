#include "UI/Hud/QuickContainer/FridgeIcon.h"

#include "Core/TraceLog.h"
#include "Gameplay/FoodSpoil/FoodSpoil.h"
#include "UI/Flash.h"
#include "UI/FridgeIcon.h"
#include "UI/Hud/QuickContainer/Rows.h"
#include "UI/Roles/ContainerName.h"

#include <array>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace QuickContainer
{
	namespace
	{
		using Scaleform::GFx::Value;

		// -------------------------------------------------------------------
		// Where the icon comes from and where it goes
		// -------------------------------------------------------------------

		// A file that holds the damage icons, and the icon clip's exported
		// name in it.
		struct Source
		{
			const char* file;
			const char* iconClass;
		};

		// HUDMenu.swf holds no damage icons. The Pip-Boy's pages do, and the
		// Pip-Boy loads them into its own movie's domain, so NEC loads one into
		// the HUD's the same way, never shown, and makes the icon by its
		// exported name. A mod that changes the file changes the icon. The
		// inventory page comes first. A mod's copy of it may take some of its
		// classes from that mod's own Pip-Boy file, and then the HUD cannot
		// make the icon from it. The stats page holds the same icon, so it
		// comes next.
		constexpr std::array SOURCES{
			Source{ "Pipboy_InvPage.swf", "Pipboy_InvPage_fla.DamageIcons_24" },
			Source{ "Pipboy_StatsPage.swf", "Pipboy_StatsPage_fla.DamageIcons_91" },
		};

		// The space between the name's last letter and the icon, as between a
		// row's name and its first icon.
		constexpr double ICON_GAP = 6.0;

		// -------------------------------------------------------------------
		// What the HUD's thread keeps
		// -------------------------------------------------------------------

		// The icon's own bounds, read as it is made.
		std::optional<Flash::Box> g_box;

		// Whether the icon showed at the last frame, so the trace writes only
		// changes.
		bool g_shown = false;

		// The colour NEC gave the icon, so it is written only on a change.
		Flash::Tint g_tint;

		// The movie loading a file, and its loader, held until the file
		// arrives, since nothing in the movie holds a loader that is not on
		// screen. When a new HUD movie loads first, the old loader is let go
		// of and never released, since its movie may be gone.
		const Scaleform::GFx::Movie* g_loading = nullptr;
		Value*                       g_loader = nullptr;

		// The file of SOURCES loading or last loaded.
		std::size_t g_source = 0;

		// -------------------------------------------------------------------
		// Every frame
		// -------------------------------------------------------------------

		// Gives the icon the colour the name's letters show. A HUD may colour
		// the name's field itself, which never reaches the icon beside it.
		// The vanilla HUD colours a clip above the name's, which reaches both,
		// so the field has no colour of its own and nothing is written.
		void Recolor(Value& a_icon, const Value& a_field)
		{
			const auto wanted = Flash::LetterTint(a_field);
			if (!wanted.Near(g_tint) && Flash::SetTint(a_icon, wanted)) {
				g_tint = wanted;
			}
		}

		// Shows the icon right of the name while the container is a fridge,
		// and hides it otherwise.
		void Update(Value& a_name)
		{
			auto icon = FridgeIcon::Of(a_name);
			if (!g_box || !icon.IsDisplayObject() || (!g_shown && !FoodSpoil::FridgeIconOn())) {
				return;
			}

			// As high as the name's capitals, centred on its line as its
			// letters are. Finding where the line is drawn takes LineOf about
			// 10 calls into the movie, made every frame while a fridge's name
			// shows, which is too few to be worth keeping between frames.
			auto       field = Roles::ContainerName::Field(a_name);
			const auto line = FoodSpoil::FridgeIconOn() && ShowsFridge() ? FridgeIcon::LineOf(field) : std::nullopt;
			const auto shown = line && FridgeIcon::Put(icon, *g_box, FridgeIcon::Side::kRight, line->right + ICON_GAP, line->capitals, line->middle);
			if (shown) {
				Recolor(icon, field);
			}
			if (shown == g_shown) {
				return;
			}
			g_shown = shown;
			Flash::Set(icon, "visible"sv, Value(shown));
			if (shown) {
				const auto box = Roles::BoundsIn(icon, a_name);
				const auto tint = Flash::TintOf(icon);
				TraceLog::Line("menu", "Quick container fridge icon shown beside \"{:s}\" at x {:.1f}, {:.1f} high, its tint {:s}, its middle at y {:.1f} on the line's {:.1f} as drawn, {:.1f} by its metrics",
					Flash::String(field, "text"sv), Flash::Number(icon, "x"sv), Flash::Number(icon, "height"sv),
					tint ? std::format("{:06X}", tint->OnWhite()) : "none"s, box ? (box->top + box->bottom) / 2.0 : std::numeric_limits<double>::quiet_NaN(),
					line->middle, line->metricsMiddle);
			} else {
				TraceLog::Line("menu", "Quick container fridge icon hidden");
			}
		}

		// Runs Update at every frame, on the name's clip.
		class FrameListener final : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				auto name = a_params.argCount > 0 ? Flash::Member(a_params.args[0], "currentTarget"sv) : Value{};
				if (name.IsDisplayObject()) {
					Update(name);
				}
			}
		};

		FrameListener g_frameListener;

		// -------------------------------------------------------------------
		// Making the icon once a file arrives
		// -------------------------------------------------------------------

		// Puts the icon beside the name with its every frame listener.
		void Attach(Scaleform::GFx::Movie& a_movie, Value& a_name, Value& a_icon)
		{
			const auto& source = SOURCES[g_source];
			const auto  own = Roles::BoundsIn(a_icon, a_icon);
			if (!own) {
				REX::WARN("The cryo frame of {:s} in {:s} is empty, so fridges show no icon on the HUD.", source.iconClass, source.file);
				return;
			}

			Value frame;
			a_movie.CreateFunction(&frame, &g_frameListener);
			if (!a_name.Invoke("addEventListener", std::array{ Value("enterFrame"), frame })) {
				REX::WARN("The HUD refused the fridge icon, so fridges show no icon on the HUD.");
				return;
			}
			g_box = own;
			TraceLog::Line("menu", "HUDMenu.swf loaded {:s}, the quick container's name carries the fridge icon", source.file);
		}

		bool Load(Scaleform::GFx::Movie& a_movie);

		// Loads the files of SOURCES from a_first on until one starts loading.
		// Logs once when none is left.
		void LoadFrom(Scaleform::GFx::Movie& a_movie, std::size_t a_first)
		{
			for (g_source = a_first; g_source < SOURCES.size(); ++g_source) {
				if (Load(a_movie)) {
					return;
				}
			}
			REX::WARN("The HUD could not make the fridge icon from {:s} or {:s}, so fridges show no icon on the HUD. A mod may have changed them.",
				SOURCES[0].file, SOURCES[1].file);
		}

		// Makes the icon from the file that arrived, or loads the next file
		// when the movie cannot make the icon from it.
		void Arrival(Scaleform::GFx::Movie& a_movie, bool a_loaded)
		{
			auto name = Roles::ContainerName::Find(a_movie);
			if (!name) {
				return;
			}

			const auto& source = SOURCES[g_source];
			if (a_loaded) {
				auto icon = FridgeIcon::Make(a_movie, *name, source.iconClass);
				if (icon.IsDisplayObject()) {
					Attach(a_movie, *name, icon);
					return;
				}
			}
			TraceLog::Line("menu", "HUDMenu.swf could not {:s} {:s}", a_loaded ? "make the fridge icon from" : "load", source.file);
			LoadFrom(a_movie, g_source + 1);
		}

		// The loader's listener: true once the file arrived, false when it
		// could not be read.
		class Arrived final : public Scaleform::GFx::FunctionHandler
		{
		public:
			explicit Arrived(bool a_loaded) noexcept :
				loaded(a_loaded)
			{}

			void Call(const Params& a_params) override
			{
				if (!a_params.movie || a_params.movie != g_loading) {
					return;
				}
				// The movie the loader belongs to is the one calling, so it is
				// there to release it.
				g_loading = nullptr;
				delete std::exchange(g_loader, nullptr);
				Arrival(*a_params.movie, loaded);
			}

		private:
			bool loaded;
		};

		Arrived g_loaded{ true };
		Arrived g_failed{ false };

		// Starts loading the file of SOURCES at g_source into the HUD movie's
		// own domain, so the icon's class lands there too and the movie can
		// make it by name. The movie makes a class by name only from its own
		// domain and the ones above it, never from a domain below. False when
		// the load could not start.
		bool Load(Scaleform::GFx::Movie& a_movie)
		{
			const auto domain = Flash::Domain(a_movie);

			Value       loader;
			Value       request;
			Value       context;
			const Value url{ SOURCES[g_source].file };
			const std::array<Value, 2> how{ Value(false), domain };
			a_movie.CreateObject(&loader, "flash.display.Loader");
			a_movie.CreateObject(&request, "flash.net.URLRequest", &url, 1);
			a_movie.CreateObject(&context, "flash.system.LoaderContext", how.data(), static_cast<std::uint32_t>(how.size()));
			auto info = Flash::Member(loader, "contentLoaderInfo"sv);

			Value loaded;
			Value failed;
			a_movie.CreateFunction(&loaded, &g_loaded);
			a_movie.CreateFunction(&failed, &g_failed);
			if (!domain.IsObject() || !request.IsObject() || !context.IsObject() || !info.IsObject() ||
				!info.Invoke("addEventListener", std::array{ Value("complete"), loaded }) ||
				!info.Invoke("addEventListener", std::array{ Value("ioError"), failed }) || !loader.Invoke("load", std::array{ request, context })) {
				return false;
			}
			g_loading = &a_movie;
			g_loader = new Value(loader);
			return true;
		}
	}

	void ForgetFridgeIcon()
	{
		g_box.reset();
		g_shown = false;
		g_tint = {};
		g_loading = nullptr;
		g_loader = nullptr;
		g_source = 0;
	}

	void AddFridgeIcon(Scaleform::GFx::Movie& a_movie)
	{
		if (!Roles::ContainerName::Find(a_movie)) {
			REX::WARN("The HUD has no quick container name, so fridges show no icon there. A HUD replacer may have moved it.");
			return;
		}
		LoadFrom(a_movie, 0);
	}
}
