#include "UI/Hud/HudCondition/HudCondition.h"

#include "UI/Hud/HudCondition/Frame.h"
#include "UI/Hud/HudCondition/Place.h"
#include "UI/Hud/HudParts/HudParts.h"
#include "UI/MenuMovies.h"
#include "UI/Roles/Hud.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"

#include <array>
#include <string_view>

namespace HudCondition
{
	namespace
	{
		using Scaleform::GFx::Value;

		// The name the readout carries, so the frame listener can find it.
		constexpr const char* READOUT_NAME = "NEC_Condition_mc";

		// -------------------------------------------------------------------
		// Building the bar
		// -------------------------------------------------------------------

		// Adds the readout and its frame listener to the HUD movie.
		void Build(Scaleform::GFx::Movie& a_movie)
		{
			// 1 path lookup per HUD load. The frame listener reads members
			// from the readout instead, see Roles::Hud::AmmoCounter.
			Value root;
			a_movie.GetVariable(&root, "_root");
			auto  counter = Roles::Hud::AmmoCounter(a_movie, root);
			Value meters;
			if (!counter || !counter->GetMember("parent"sv, &meters) || !meters.IsDisplayObject()) {
				Roles::Missing(a_movie, "ammo counter"sv, "the HUD shows no CND bar outside power armor"sv);
				return;
			}

			// 1 readout beside the counter, in the same parent, so it stays
			// when the counter hides. The first frames measure the divider, lay
			// the parts out and colour them. Nothing is added to the HUD until
			// the whole readout is built, so a movie that refuses any part
			// keeps its own divider.
			auto readout = HudParts::Readout::Create(a_movie, READOUT_NAME);
			if (!readout.IsDisplayObject()) {
				REX::WARN("The HUD movie would not take a CND bar, so it shows no CND.");
				return;
			}
			meters.Invoke("addChild", std::array{ readout });

			// A new HUD movie starts with nothing drawn.
			ResetFrames();

			Value listener;
			a_movie.CreateFunction(&listener, &Frames());
			if (!readout.Invoke("addEventListener", std::array{ Value("enterFrame"), listener })) {
				REX::WARN("The HUD refused the frame listener, so its CND bar never updates.");
				return;
			}

			HudParts::Weapon::Queue();
			TraceLog::Line("menu", "HUDMenu.swf loaded, CND goes by the ammo counter");
		}

		HudParts::Waiter g_waiter{ Settings::bHudCondition, &Build };
	}

	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file)
	{
		if (MenuMovies::IsMovie(a_file, "HUDMenu.swf"sv)) {
			Forget();
			g_waiter.Watch(a_movie, "the CND bar");
		}
	}
}
