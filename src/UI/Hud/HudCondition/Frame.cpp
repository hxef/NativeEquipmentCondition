#include "UI/Hud/HudCondition/Frame.h"

#include "Condition/Condition.h"
#include "Core/Settings/Settings.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Hud/HudCondition/Listener.h"
#include "UI/Hud/HudCondition/Place.h"
#include "UI/Hud/HudCondition/Spot.h"
#include "UI/Hud/HudParts/HudParts.h"
#include "UI/Roles/Hud.h"

#include <algorithm>
#include <chrono>
#include <optional>
#include <string_view>

namespace HudCondition
{
	using Scaleform::GFx::Value;

	namespace
	{
		// -------------------------------------------------------------------
		// The readout
		// -------------------------------------------------------------------

		// How long the readout fades on its own, about as long as the counter's
		// own fade.
		constexpr double FADE_SECONDS = 0.25;

		// The bar takes the colour the divider is drawn in, checked every
		// reading, since the HUD colour or a HUD mod can change it. In vanilla
		// the counter is a HUD part as the bar is, so both are expected to
		// carry the HUD colour and nothing is written. Alpha stays the bar's
		// own. A colour written again and again is traced once, by a_told.
		void Recolor(Value& a_readout, Value& a_counter, std::optional<Flash::Tint>& a_told)
		{
			const auto wanted = Roles::Hud::DividerTint(a_counter);
			const auto now = Flash::TintOf(a_readout);
			if (!wanted || !now || now->Near(*wanted) || !Flash::SetTint(a_readout, *wanted) || (a_told && a_told->Near(*wanted))) {
				return;
			}
			a_told = wanted;
			TraceLog::Line("menu", "HUD CND bar takes the colour the ammo divider is drawn in, times {:.3f} {:.3f} {:.3f} plus {:.0f} {:.0f} {:.0f}",
				wanted->red, wanted->green, wanted->blue, wanted->redAdd, wanted->greenAdd, wanted->blueAdd);
		}
	}

	// -------------------------------------------------------------------
	// The frame listener
	// -------------------------------------------------------------------

	void FrameListener::Call(const Params& a_params)
	{
		Value readout;
		if (a_params.argCount < 1 || !a_params.movie ||
			!a_params.args[0].GetMember("currentTarget"sv, &readout) || !readout.IsDisplayObject()) {
			return;
		}

		// The bar copies the counter's place, so it stays as last drawn
		// while the counter is gone or not in the bar's parent.
		auto  found = Roles::Hud::AmmoCounter(*a_params.movie, readout);
		Value meters;
		Value beside;
		if (!found || !readout.GetMember("parent"sv, &meters) || !meters.IsDisplayObject() ||
			!meters.Invoke("contains", &beside, &*found, 1) || !beside.IsBoolean() || !beside.GetBoolean()) {
			Roles::Noted(*a_params.movie, "ammo counter"sv, "the CND bar stays as last drawn"sv);
			return;
		}
		auto& counter = *found;

		const auto now = Clock::now();

		// The HUD menu is listed once it has finished being built, a
		// frame after its movie loads. The colour is copied from a
		// reading after the colour target is made, so the HUD colour
		// lands first.
		if (!colored) {
			if (auto* menu = HudParts::MenuFor(*a_params.movie)) {
				HudParts::AddColorTarget(*menu, readout);
				recolorFrom = now + HudParts::Weapon::READING_EVERY;
				counterPart = HudParts::AmmoCounterPart(*menu);
				colored = true;
				TraceLog::Line("menu", "HUD CND is a HUD part of its own, in the gameplay HUD colour, {:s}",
					counterPart ? "and follows the ammo counter's HUD modes" : "but found no ammo counter to follow HUD modes by");
			}
		}

		const auto elapsed = lastFrame ? std::chrono::duration<double>(now - *lastFrame).count() : 0.0;
		lastFrame = now;

		// A change between Bar and Percent lays everything out again, at
		// once, see Table.h.
		const auto form = Settings::iWeaponCnd.Form();
		HudParts::Readout::TellForm(form, saidForm, "Weapon CND"sv);
		if ((form == Settings::Cnd::kPercent) != tabled) {
			SwitchForm(readout, form == Settings::Cnd::kPercent);
		}

		const auto reading = !lastReading || now - *lastReading >= HudParts::Weapon::READING_EVERY;
		if (reading) {
			lastReading = now;
			HudParts::Weapon::Queue();
			// The letters are measured again each reading until a line
			// reads, so a field not laid out yet is not kept for good.
			if (!label.measured) {
				label = Label(readout, !labelled, laidOut ? Flash::Point{ laidOut->wordX, laidOut->wordY + laidRise } : Flash::Point{});
				labelled = true;
			}
			// At Percent the 2 lines are laid out again when the counter's
			// rows, the word or the widest number change.
			if (tabled) {
				rows = RowsNow(*a_params.movie, counter, ownVisible, rows);
				auto laid = LayTable(*a_params.movie, readout, label, rows, table);
				if (!laid || !table || laid->rows != table->rows || laid->word != table->word || laid->widest != table->widest) {
					laidOut.reset();
					shownNumber.clear();
					lined.reset();
				}
				table = std::move(laid);
			}
			// Until a reading measures the divider, the bar keeps the shipped
			// place, flipped as the switch says, so a bar shown before then
			// is never the wrong way round.
			const auto  flipped = FlippedNow();
			const auto& words = Words();
			const auto  choice = Choose(*a_params.movie, counter, readout, ownVisible, words, flipped);
			if (choice) {
				chosen = true;
				teller.Tell(*choice, now);
			}
			auto shipped = SHIPPED;
			shipped.flipped = flipped;
			shipped.wordX = Home(flipped, shipped.line.width, words.letters);
			// A reading that measures nothing keeps the last spot, turned
			// at once by a flip to the spot its measure gave the other way
			// round, see Turned.
			if (choice) {
				Follow(choice);
			} else if (!chosen) {
				Follow(Choice{ shipped, {}, {} });
			} else {
				Follow(Choice{ Turned(spot, flipped, words.letters), "bar turns where it is, since the divider cannot be measured", {} });
			}
			if (colored && now >= recolorFrom) {
				Recolor(readout, counter, toldTint);
			}
		}

		// The bar is drawn at the size the player set, laid out again
		// when the spot or that size changes. clear alone moves nothing.
		const auto size = SizeNow();
		const auto drawn = Sized(spot, size, Words());
		if (!laidOut || !drawn.SitsAs(*laidOut)) {
			LayOut(readout, drawn, size);
		}

		if (!tabled) {
			HudParts::Readout::FollowWordSwitch(readout, Settings::bHudBarShowsWord, wordShown, "HUD"sv);
		}

		// The mark is read every frame, as the size is, and is as wide as
		// half the bar's depth before its thickness slider. The fill's end
		// comes from the spot, as the word's place does, so the 2 never
		// disagree.
		const auto percent = HudParts::Weapon::Percent(&Settings::iWeaponCnd);
		const auto mark = HudParts::Readout::PenaltyMark(Condition::Kind::kWeapon, spot.deep, spot.deep / 2.0);
		if (tabled) {
			DrawNumber(readout);
		} else if (percent != shownPercent || mark != shownMark) {
			const auto still = HudParts::Readout::StillEnd(HudParts::Readout::Still::kRightEnd, drawn.flipped);
			HudParts::Readout::SetPercent(readout, drawn.line.width, drawn.deep, percent, still, mark);
			HudParts::Readout::TellFlipped(drawn.flipped, saidFlipped, "HUD CND bar"sv);
			if (percent != shownPercent) {
				if (percent >= 0) {
					TraceLog::Line("menu", "HUD CND bar drawn at {:d}%", percent);
				} else {
					TraceLog::Line("menu", "HUD CND bar has no reading to draw");
				}
			}
			shownPercent = percent;
			shownMark = mark;
			if (mark != saidMark) {
				saidMark = mark;
				if (mark.at != 0) {
					TraceLog::Line("menu", "HUD CND bar marks weapon penalties from {:d}% {:s} {:.2f} wide", mark.at, mark.line ? "with a line" : "with notches", mark.wide);
				} else {
					TraceLog::Line("menu", "HUD CND bar marks no penalty start");
				}
			}
		}

		// A HUD mod or a power armor HUD can move the counter or a clip
		// it sits in, and the bar goes with it. The MCM page can move
		// the bar, and a new screen size can move the screen's edges.
		const auto shown = a_params.movie->GetVisibleFrameRect();
		const Basis basis{ TransformOf(counter), ParentsOf(meters), ShiftNow(), Flash::Box{ shown.x1, shown.y1, shown.x2, shown.y2 } };
		if (reading || basis != placedFrom) {
			placedFrom = basis;
			PlaceOnScreen(*a_params.movie, meters, counter, readout, basis, drawn, size);
		}

		// While the counter shows, the bar fades with it. A melee
		// weapon hides the counter, so the bar fades on its own, shown
		// while the weapon is out. When the HUD mode hides the counter,
		// in the Pip-Boy for example, the bar fades too, even with a
		// weapon out. A hidden counter with rounds to count keeps the
		// bar hidden while bHudBarHidesWithCounter is on. While it is
		// off, a bar that stays never dips as the counter fades out. A
		// bar that stands alone has no counter to fade with, so it
		// shows while the weapon is out, as a melee weapon's does.
		const auto hudAllows = !counterPart || counterPart->hudModes.canBeVisible;
		const auto alone = hudAllows && HudParts::Weapon::Drawn();
		const auto hidesWithCounter = !standsAlone && counterPart && Settings::bHudBarHidesWithCounter.GetValue() && HudParts::HasRounds(*counterPart);
		const auto counterOpacity = Flash::Opacity(counter);
		const auto counterShows = !standsAlone && counterOpacity > 0.0;
		const auto stays = alone && !hidesWithCounter;

		auto opacity = 0.0;
		if (percent < 0 || (tabled && !table)) {
			fade = 0.0;
		} else if (counterShows) {
			opacity = stays ? std::max(counterOpacity, fade) : counterOpacity;
			fade = opacity;
		} else {
			const auto target = stays ? 1.0 : 0.0;
			const auto step = elapsed / FADE_SECONDS;
			fade = target > fade ? std::min(target, fade + step) : std::max(target, fade - step);
			opacity = fade;
		}
		if (!placeable) {
			opacity = 0.0;
			fade = 0.0;
		}

		// The divider hides while the bar is drawn over it, checked
		// every frame, since a movie can show it again. With no
		// condition to show, or the bar off it, the counter keeps its
		// divider.
		const auto covered = !tabled && counterShows && percent >= 0 && onSpot && spot.onDivider;
		Roles::Hud::Cover(counter, covered, ownVisible);
		if (covered != dividerHidden) {
			dividerHidden = covered;
			TraceLog::Line("menu", "HUD ammo divider {:s}, the bar moved by {:.0f},{:.0f}", covered ? "hides under the CND bar" : "shows", basis.shift.x, basis.shift.y);
		}

		if (opacity != shownOpacity) {
			if ((opacity > 0.0) != (shownOpacity > 0.0)) {
				const auto* onDivider = tabled ? "over and under the ammo divider" : "in place of the ammo divider";
				TraceLog::Line("menu", "HUD CND {:s} {:s}, percent {}", opacity > 0.0 ? "shows" : "hides",
					!counterShows ? (alone && hidesWithCounter ? "with the ammo counter" : "on its own") : (spot.onDivider ? onDivider : "beside the ammo counter"), percent);
			}
			readout.SetMember("alpha"sv, Value(opacity));
			readout.SetMember("visible"sv, Value(opacity > 0.0));
			shownOpacity = opacity;
		}

		// At Percent the line box runs vanilla's line on while the counter
		// shows its line where the 2 lines stand, and fades with it.
		if (tabled && table) {
			const auto line = counterShows && percent >= 0 && placeable && onSpot && spot.onDivider && drawn.wordX == 0.0 && drawn.wordY == 0.0;
			if (line != lined) {
				lined = line;
				DrawLine(*a_params.movie, readout, *table, line);
			}
			const auto alpha = line && opacity > 0.0 ? std::min(1.0, counterOpacity / opacity) : 0.0;
			if (alpha != lineAlpha) {
				lineAlpha = alpha;
				FadeLine(readout, alpha);
			}
		}
	}

	void FrameListener::Reset()
	{
		lastFrame.reset();
		lastReading.reset();
		shownPercent = NOT_DRAWN;
		shownMark = HudParts::Readout::Mark{};
		saidMark = HudParts::Readout::Mark{};
		saidFlipped = false;
		shownOpacity = -1.0;
		fade = 0.0;
		spot = SHIPPED;
		label = HudParts::Readout::LabelPlace{};
		teller.Reset();
		placedFrom.reset();
		placedAt.reset();
		placeable = false;
		onSpot = false;
		held = false;
		standsAlone = false;
		aloneAt.reset();
		followed = false;
		chosen = false;
		laidOut.reset();
		laidOutSize = HudParts::Readout::Size{};
		labelled = false;
		wordShown.reset();
		colored = false;
		recolorFrom = {};
		toldTint.reset();
		dividerHidden = false;
		ownVisible.reset();
		counterPart = nullptr;
		saidForm.reset();
		tabled = false;
		rows = Rows{};
		table.reset();
		laidRise = 0.0;
		shownNumber.clear();
		lined.reset();
		lineAlpha = -1.0;
	}

	const HudParts::Readout::LabelPlace& FrameListener::Words() const
	{
		return tabled && table ? table->place : label;
	}

	// A change of form shows the bar's boxes, or hides them and shows the
	// word whatever its switch says, and forgets everything laid out, so
	// this frame reads and lays the new form out.
	void FrameListener::SwitchForm(Value& a_readout, bool a_number)
	{
		tabled = a_number;
		HudParts::Readout::ShowBar(a_readout, !a_number);
		if (a_number) {
			HudParts::Readout::ShowLabel(a_readout, true);
		} else {
			HideTable(a_readout);
		}
		wordShown.reset();
		laidOut.reset();
		shownPercent = NOT_DRAWN;
		shownMark = HudParts::Readout::Mark{};
		table.reset();
		shownNumber.clear();
		lined.reset();
		lineAlpha = -1.0;
		placedFrom.reset();
		lastReading.reset();
	}

	// Writes the percent under the word when it changes, as the weapon's
	// card writes it.
	void FrameListener::DrawNumber(Value& a_readout)
	{
		const auto tenths = HudParts::Weapon::Tenths(&Settings::iWeaponCnd);
		const auto number = tenths && table ? HudParts::Readout::PercentText(*tenths) : std::string{};
		if (number == shownNumber) {
			return;
		}
		shownNumber = number;
		lined.reset();
		if (!number.empty()) {
			WriteNumber(a_readout, number);
			TraceLog::Line("menu", "HUD CND drawn as {:s} {:s}", Flash::String(HudParts::Readout::LabelOf(a_readout), "text"sv), number);
		}
	}

	namespace
	{
		// Lives as long as the plugin, see Flash.h.
		FrameListener g_frameListener;
	}

	Scaleform::GFx::FunctionHandler& Frames()
	{
		return g_frameListener;
	}

	void ResetFrames()
	{
		g_frameListener.Reset();
	}
}
