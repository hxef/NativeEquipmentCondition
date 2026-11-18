#pragma once

#include "Core/Plugin.h"

#include "Condition/Condition.h"
#include "Core/Settings/Settings.h"
#include "UI/Flash.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// The CND readout every bar but the quick container's meters is drawn as: the
// Weapon CND bar, the Power armor CND bar, the Pickup CND bar and the bars of
// the Pip-Boy apparel figure. A place set to Percent hides the bar and writes
// CND and the percent in text instead, see the last section. Readout.cpp
// draws it, and Percent.cpp writes the texts of a place at Percent.
namespace HudParts
{
	using Value = Scaleform::GFx::Value;

	// The readout itself: a bar over a dimmed track, and the word CND past the
	// right end of the bar, drawn the way the AP meter is. The track shows the
	// worn part at half opacity like the HP meter's spare bar, and the word
	// starts a little past the bar. Still below says which end of the bar
	// stays put. A readout's origin is the right end of its bar, halfway down,
	// so placing a readout means saying where its bar ends.
	//
	// The Pip-Boy's doll sizes everything from the font size, in the AP
	// meter's proportions below: at font size 20 the bar is 6 deep, like the
	// AP bar, and the doll then makes its bars half again as deep, see
	// PaperDoll.h. The ammo counter sizes its word the same way and its bar from
	// the counter's divider, see HudCondition.h. The power armor readout sizes
	// its bar and its word separately to fit the dash, with the word level
	// with the bar and anti-aliased for animation since it moves. Each HUD
	// bar's length and thickness then take the player's Size.
	//
	// Where penalties start, see Mark, the readout draws 2 black notches over
	// the bar, or a plain line that splits the track and the fill in 2. So it
	// holds 4 more boxes, each hidden while it is not used: the track past the
	// line, the fill past the line, the notches and the line.
	namespace Readout
	{
		inline constexpr double BAR_DEEP = 6.0 / 20.0;
		inline constexpr double LABEL_GAP = 6.0 / 25.0;
		inline constexpr double LABEL_DROP = 4.0 / 25.0;

		// How the word is written, in the movie's units.
		struct Label
		{
			double size = 0.0;  // its font size
			double gap = 0.0;   // how far past the right end of the bar it starts
			double drop = 0.0;  // how far its middle hangs below the bar's, 0 for level
			double wide = 1.0;  // how wide its letters are, as a share of the font's own
		};

		// Builds a hidden readout with nothing drawn yet, for the caller to add
		// where it belongs. Returns a value that is not a display object when
		// the movie refuses any part.
		[[nodiscard]] Value Create(Scaleform::GFx::Movie& a_movie, const char* a_name);

		// Lets the word move as smoothly as the bar, for a readout moved every
		// frame. A text field made from code starts anti-aliased for
		// readability, which snaps text to whole pixels, so a word moved a
		// fraction of a pixel per frame would jump while the bar moves
		// smoothly.
		void AntiAliasForAnimation(Value& a_readout);

		// Writes the word as a_label says and returns how far past the end of
		// the bar it reaches. It measures text, so call it when the size
		// changes, not every frame.
		double SetLabel(Value& a_readout, const Label& a_label);

		// Where the word sits from the readout's origin: its field, and the
		// box its letters fill, from the top of the line to the baseline, and
		// how far the line reaches below that. The letters are the field
		// inside its margins when they cannot be measured, and measured says
		// which. All 0 when the readout has no word.
		struct LabelPlace
		{
			Flash::Box field;
			Flash::Box letters;
			bool       measured = false;
			double     descent = 0.0;
		};
		[[nodiscard]] LabelPlace LabelExtent(Value& a_readout);

		// Puts the word's field with its top left corner at a_x, a_y from the
		// readout's origin.
		void MoveLabel(Value& a_readout, double a_x, double a_y);

		// Shows or hides the word of a HUD bar as a_shows, its own switch,
		// says. The bar keeps its place either way. a_shown is what was last
		// written, so a frame writes only on a change, and a_bar names the bar
		// in the trace.
		void FollowWordSwitch(Value& a_readout, const Settings::Live<bool>& a_shows, std::optional<bool>& a_shown, std::string_view a_bar);

		// How many times its own length and thickness a HUD bar is drawn. The
		// bar grows and shrinks about the readout's origin, so the right end
		// it is placed by and the word past it stay where they are, and the
		// word keeps its size so it stays readable.
		struct Size
		{
			double length = 1.0;
			double thickness = 1.0;

			bool operator==(const Size&) const = default;
		};

		// The least and the most a size setting counts for, as the MCM page's
		// sliders allow.
		inline constexpr double LEAST_SIZE = 0.5;
		inline constexpr double MOST_SIZE = 3.0;

		// a_value held from a_least to a_most, and 1 for a value that is no
		// number.
		[[nodiscard]] double Held(const Settings::Live<float>& a_value, double a_least, double a_most);

		// The size a bar's 2 settings ask for now, each held from LEAST_SIZE
		// to MOST_SIZE, and 1 for one that is no number.
		[[nodiscard]] Size SizeFrom(const Settings::Live<float>& a_length, const Settings::Live<float>& a_thickness);

		// The least and the most fPenaltyMarkWidth counts for, as the MCM
		// page's slider allows.
		inline constexpr double MARK_LEAST = 0.5;
		inline constexpr double MARK_MOST = 2.0;

		// The mark where penalties start, as a bar draws it. A bar compares
		// the mark it wants with the one it drew, so it redraws on a change.
		struct Mark
		{
			std::int32_t at = 0;       // the point in whole percents, 0 for no mark
			double       wide = 0.0;   // the mark's width in the movie's units
			double       step = 0.0;   // how far the fill's end keeps out of the mark near the point
			bool         line = false; // a plain line instead of the notches

			bool operator==(const Mark&) const = default;
		};

		// The mark for an item of a_kind, or no mark while its point is 0 or
		// 1, so moving the width or the line switch then redraws nothing.
		// a_deep is the bar's depth at its normal thickness and a_wide the
		// mark's width at a Penalty mark width of 1. The fill's end keeps a
		// sixth of a_deep out of the mark.
		[[nodiscard]] Mark PenaltyMark(Condition::Kind a_kind, double a_deep, double a_wide);

		// Lays the track out a_width across and a_deep deep.
		void SetTrack(Value& a_readout, double a_width, double a_deep);

		// The end of the bar that stays still as it empties. The HUD keeps the
		// right end, as the AP meter does. The Pip-Boy keeps the left end, as
		// its own meters do. Each place's own flip switch, such as
		// bHudBarFlipped, turns its bar to its other end.
		enum class Still
		{
			kRightEnd,
			kLeftEnd,
		};

		// The still end of a bar that ships with a_shipped, the other end
		// while a_flipped, its place's flip switch as the bar was laid out.
		[[nodiscard]] Still StillEnd(Still a_shipped, bool a_flipped);

		// Sets how much of a bar laid out as above is left, and draws a_mark
		// over it.
		void SetPercent(Value& a_readout, double a_width, double a_deep, std::int32_t a_percent, Still a_still = Still::kRightEnd, const Mark& a_mark = {});

		// Moves the word to its mirror about the middle of a bar a_width
		// long, so a word that starts 6 past the right end ends 6 before the
		// left end. The field keeps the same margin on each side, so its
		// letters mirror with it. Call it after SetLabel, which puts the word
		// back past the end.
		void MirrorLabel(Value& a_readout, double a_width);

		// Says in the trace that a_bars are drawn flipped, or as shipped,
		// when a_flipped differs from a_said, what was said last. a_said
		// starts false and is reset with the bar, so a bar never flipped
		// says nothing.
		void TellFlipped(bool a_flipped, bool& a_said, std::string_view a_bars);

		// -------------------------------------------------------------------
		// A percent in place of the bar
		// -------------------------------------------------------------------

		// The parts only a place at Percent uses, each made the first time it
		// is asked for, so a place that never shows a percent holds what it
		// held before: a 2nd copy of the word for a place that moves it, a
		// field for the number and a line box. All 3 start hidden.
		enum class Extra
		{
			kWord,
			kNumber,
			kLine,
		};

		// a_readout's part a_extra, made on its first use. A value that is
		// not a display object when the movie refuses it. A new field draws
		// as smoothly as the word does, see AntiAliasForAnimation.
		[[nodiscard]] Value ExtraOf(Scaleform::GFx::Movie& a_movie, Value& a_readout, Extra a_extra);

		// a_readout's part a_extra once made, else a value that is not a
		// display object. It makes nothing, so a place that never showed a
		// percent stays as it was.
		[[nodiscard]] Value ExtraIn(Value& a_readout, Extra a_extra);

		// The word's own field.
		[[nodiscard]] Value LabelOf(Value& a_readout);

		// Shows or hides the bar's 6 boxes: the track, the fill and the 4 of
		// the mark. A place at Percent hides them. Once they show again the
		// caller draws the bar again with SetPercent, which shows the mark.
		void ShowBar(Value& a_readout, bool a_shown);

		// Shows or hides the word's field, for a place at Percent, where the
		// word shows whatever the bar's word switch says.
		void ShowLabel(Value& a_readout, bool a_shown);

		// The number a place at Percent writes for a_tenths, as an item's
		// card writes it, see Condition::CardText: 87.4%, 98% or 100%.
		[[nodiscard]] std::string PercentText(std::uint32_t a_tenths);

		// The CND word as a_field's movie translates it, a space and
		// PercentText, "CND 87.4%".
		[[nodiscard]] std::string WordAndPercent(Value& a_field, std::uint32_t a_tenths);

		// The 2 numbers that write widest: a worn item at its most, 99.9% or
		// 99% as CND decimals says, and a full one, 100%. Digits share 1
		// width in the HUD's fonts, so the wider of the 2 is as wide as any
		// item's number gets.
		inline constexpr std::array<std::uint32_t, 2> WIDEST{ Condition::MOST_WORN_TENTHS, Condition::FULL_TENTHS };

		// Where a text's letters stand from the point a place puts it by.
		enum class Align
		{
			kStart,
			kMiddle,
			kEnd,
		};

		// Hides the bar's 6 boxes, shows the word's field and writes a_text
		// into it at a_label's size and width, never mirrored. Its letters
		// start at, are centred on or end at a_label.gap across from the
		// origin, as a_align says, and its field's middle hangs a_label.drop
		// below it, as SetLabel hangs the word. Returns where the field and
		// its letters sit.
		LabelPlace ShowNumber(Value& a_readout, std::string_view a_text, const Label& a_label, Align a_align);

		// Writes a_text into a_field in the word's font, a_size big and its
		// letters a_wide of the font's width, as SetLabel writes the word.
		// The field keeps its place. Returns where it and its letters sit,
		// as ExtentOf does.
		LabelPlace WriteText(Value& a_field, std::string_view a_text, double a_size, double a_wide);

		// Where a field of the readout and its letters sit from the
		// readout's origin, as LabelExtent measures the word's.
		[[nodiscard]] LabelPlace ExtentOf(Value& a_field);

		// Moves a field of the readout so its top left corner is at a_x,
		// a_y from the readout's origin.
		void MoveField(Value& a_field, double a_x, double a_y);

		// Says in the trace that a_place now shows a_form, when it differs
		// from a_said, the form last seen: "Weapon CND now shows a percent".
		// The first form seen, with a_said empty, says nothing.
		void TellForm(Settings::Cnd a_form, std::optional<Settings::Cnd>& a_said, std::string_view a_place);
	}
}
