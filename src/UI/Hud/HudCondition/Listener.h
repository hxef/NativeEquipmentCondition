#pragma once

#include "Core/Plugin.h"

#include "Core/Settings/Settings.h"
#include "UI/Flash.h"
#include "UI/Hud/HudCondition/Place.h"
#include "UI/Hud/HudCondition/Spot.h"
#include "UI/Hud/HudCondition/Table.h"
#include "UI/Hud/HudParts/HudParts.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// The frame listener's class. Frame.cpp follows the counter, draws the level
// and fades the bar every frame, Layout.cpp lays the bar out at its spot and
// size, places it on screen and tells a percent's 1 line from 2. Private to
// this folder.
namespace HudCondition
{
	// Where the bar goes with no divider measured: 44 wide, with its
	// origin on its right end, as HUDMenu.swf ships the divider, and as
	// deep as on that divider, which is 2 thick.
	inline constexpr Spot SHIPPED{ { 0.0, 0.0, 44.0 }, DEEP_PER_THICK * 2.0, true };

	// A value Percent never returns, so the first frame always draws.
	inline constexpr std::int32_t NOT_DRAWN = -2;

	// Called by the HUD every frame through an enterFrame listener on the
	// readout. The event's currentTarget is the readout.
	class FrameListener final : public Scaleform::GFx::FunctionHandler
	{
	public:
		using Clock = std::chrono::steady_clock;

		void Call(const Params& a_params) override;
		void Reset();

	private:
		void PlaceOnScreen(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_meters, Scaleform::GFx::Value& a_counter, Scaleform::GFx::Value& a_readout, const Basis& a_basis, const Spot& a_drawn,
			const HudParts::Readout::Size& a_size);
		void TellAlone(const std::optional<Placed>& a_placed, const Basis& a_basis);
		void Follow(const std::optional<Choice>& a_choice);
		void LayOut(Scaleform::GFx::Value& a_readout, const Spot& a_drawn, const HudParts::Readout::Size& a_size);
		void SwitchForm(Scaleform::GFx::Value& a_readout, bool a_number);
		void FollowLines(Scaleform::GFx::Value& a_readout);
		void DrawNumber(Scaleform::GFx::Value& a_readout);

		// The box the word, or at Percent the 1 line or the 2 lines, fill
		// from the bar's right end, which the spot is held by and, but for
		// the 1 line away from the ammo line, chosen for.
		[[nodiscard]] const HudParts::Readout::LabelPlace& Words() const;

		// The 1 line as drawn now, at the ammo line or away from it, nothing
		// at Bar or in 2 lines.
		[[nodiscard]] const OneLine* One() const;

		std::optional<Clock::time_point> lastFrame;
		std::optional<Clock::time_point> lastReading;
		std::int32_t                     shownPercent = NOT_DRAWN;
		double                           shownOpacity = -1.0;
		double                           fade = 0.0;
		Spot                             spot = SHIPPED;
		Teller                           teller;
		bool                             followed = false;
		bool                             chosen = false;  // a reading chose a spot since the HUD loaded
		bool                             labelled = false;
		bool                             colored = false;
		bool                             dividerHidden = false;

		// The mark where penalties start as last drawn, forgotten with
		// shownPercent, and as the trace last named it, forgotten only in
		// Reset, so a relayout redraws it without a second line. saidFlipped
		// is the same for the trace's flipped line.
		HudParts::Readout::Mark shownMark;
		HudParts::Readout::Mark saidMark;
		bool                    saidFlipped = false;

		// Where the word and its letters sit at its place past the bar,
		// and whether it was last shown, see FollowWordSwitch.
		HudParts::Readout::LabelPlace label;
		std::optional<bool>           wordShown;

		// The spot as last laid out, at the size it was laid out at.
		std::optional<Spot>     laidOut;
		HudParts::Readout::Size laidOutSize;

		// What the bar was last placed from and where it went. placeable
		// is false while it has no place, its counter off screen with no
		// standing alone or the movie refusing a point, onSpot while it
		// sits on its spot, held while the screen's edge holds it short of
		// the shift, and standsAlone while it stands alone, at aloneAt as
		// last traced.
		std::optional<Basis>               placedFrom;
		std::optional<Transform>           placedAt;
		bool                               placeable = false;
		bool                               onSpot = false;
		bool                               held = false;
		bool                               standsAlone = false;
		std::optional<std::array<long, 2>> aloneAt;

		// When a reading may first copy the counter's colour, and the
		// colour last traced.
		Clock::time_point          recolorFrom;
		std::optional<Flash::Tint> toldTint;

		// The divider's own visible from before NEC's bar covered it, given
		// back when the bar moves off.
		std::optional<bool> ownVisible;

		// Found along with the HUD menu, see HudParts::AmmoCounterPart.
		const RE::HUDAmmoCounter* counterPart = nullptr;

		// The form last seen. tabled is true while the readout shows the
		// lines of Percent, see Table.h: the rows read last, the table laid
		// out for them, the word's rise it was put at, the number last
		// written, and whether the line box was last shown and how faded.
		std::optional<Settings::Cnd> saidForm;
		bool                         tabled = false;
		Rows                         rows;
		std::optional<Table>         table;
		double                       laidRise = 0.0;
		std::string                  shownNumber;
		std::optional<bool>          lined;
		double                       lineAlpha = -1.0;

		// At Percent, whether the percent reads in 1 line, CND 87.4%, and
		// whether it stands at the divider, see Lines in Table.h, from the
		// divider's facts the last measure at Percent gave, and the screen's
		// edge the 1 line there is sized by. saidLines is the why the trace
		// last gave.
		bool                  oneLine = false;
		bool                  atLine = false;
		bool                  between = false;
		bool                  fits = false;
		std::optional<double> lineRight;
		std::string_view      saidLines;
	};
}
