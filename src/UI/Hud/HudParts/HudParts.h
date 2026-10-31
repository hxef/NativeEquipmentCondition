#pragma once

#include "Core/Plugin.h"

#include "Core/Settings.h"
#include "UI/Flash.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>

// The pieces the plugin's CND readouts are built from. CND is drawn on the HUD
// twice, in the ammo counter out of power armor and on the dash in it.
// HudCondition.cpp and PowerArmorCondition.cpp each place it by their own
// rules, and both share the readout, the colour and the condition of the weapon
// in hand kept here. Readout.cpp draws the readout, HudParts.cpp the rest.
namespace HudParts
{
	using Value = Scaleform::GFx::Value;

	// Deletes the colour targets AddColorTarget made as their HUD menu goes.
	// The game builds a new HUD menu, and a new HUD movie, at every save load.
	void Install();

	// Lets go of the colour targets of every HUD menu but the one UI lists
	// now. Such a menu went without NEC's delete, which a DLL over it may skip.
	// Each target stops listening for HUD colour changes and is never
	// deleted, since its destructor would touch a movie that may be gone.
	// MenuMovies.cpp runs it as the HUD movie and the main menu movie load,
	// and Load's listener a frame after a HUD menu closes. The HUD movie
	// loads while the new HUD menu is built, so that menu may already sit at
	// the old one's address, but UI does not list it yet and every old
	// target goes.
	void ForgetOldTargets();

	// Listens for the HUD menu closing, once UI exists.
	void Load();

	// The same translation key the item cards use, so the HUD says CND, ZST or
	// СОСТ like the rest of the game.
	inline constexpr const char* CND_TEXT = "$ItemInfo_CND";

	// The font the HUD's own numbers use. Menus name fonts by $ alias, and
	// FontConfig.txt maps each to a font in the engine's library.
	inline constexpr const char* FONT_NAME = "$MAIN_Font_Bold";

	// The condition of the weapon in the player's hands, as the HUD knows it.
	// It is checked through F4SE's task queue and read from an atomic a frame
	// later, and both readouts share it. F4SE runs the task on a worker thread
	// during play, so the check holds the inventory lock while it reads. While
	// both bars are switched off nothing is checked, and while one is, Percent
	// says NONE to it, so it fades as for a weapon with no condition.
	namespace Weapon
	{
		// What Percent returns when nothing in hand wears out, which hides a
		// readout.
		inline constexpr std::int32_t NONE = -1;

		// Asks for a fresh check, which arrives a frame or so later. Asking
		// again while one is on its way does nothing. Also lets go of colour
		// targets once the delete hook's place is lost.
		void Queue();

		// How often a readout asks for a fresh check, several times a second,
		// which keeps up with a weapon wearing hit by hit. It goes by the clock,
		// since the HUD movie's frames come faster at a high frame rate.
		inline constexpr std::chrono::milliseconds READING_EVERY{ 300 };

		// The condition of the weapon in hand as a whole percent, or NONE
		// while a_on, the asking bar's switch, is off.
		[[nodiscard]] std::int32_t Percent(const Settings::Live<bool>& a_on);

		// Whether the player has a weapon drawn. A melee weapon hides the ammo
		// counter, so a readout beside it has to know.
		[[nodiscard]] bool Drawn();

		// Forgets the last reading when another game starts, so the readouts
		// show nothing of the last session's weapon until the next check. See
		// ForgetPlay in main.cpp.
		void Forget();
	}

	// The readout itself: a bar over a dimmed track, and the word CND past the
	// right end of the bar, drawn the way the AP meter is. The track shows the
	// worn part at half opacity like the HP meter's spare bar, and the word
	// starts a little past the bar. Still below says which end of the bar
	// stays put. A readout's origin is the right end of its bar, halfway down,
	// so placing a readout means saying where its bar ends.
	//
	// The Pip-Boy's doll sizes everything from the font size, in the AP
	// meter's proportions below: at font size 20 the bar is 6 deep, like the
	// AP bar. The ammo counter sizes its word the same way and its bar from
	// the counter's divider, see HudCondition.h. The power armor readout sizes
	// its bar and its word separately to fit the dash, with the word level
	// with the bar and anti-aliased for animation since it moves. Each HUD
	// bar's length and thickness then take the player's Size.
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

		// The size a bar's 2 settings ask for now, each held from LEAST_SIZE
		// to MOST_SIZE, and 1 for one that is no number.
		[[nodiscard]] Size SizeFrom(const Settings::Live<float>& a_length, const Settings::Live<float>& a_thickness);

		// Lays the track out a_width across and a_deep deep.
		void SetTrack(Value& a_readout, double a_width, double a_deep);

		// The end of the bar that stays still as it empties. The HUD keeps the
		// right end, as the AP meter does. The Pip-Boy keeps the left end, as
		// its own meters do.
		enum class Still
		{
			kRightEnd,
			kLeftEnd,
		};

		// Sets how much of a bar laid out as above is left.
		void SetPercent(Value& a_readout, double a_width, double a_deep, std::int32_t a_percent, Still a_still = Still::kRightEnd);
	}

	// Puts text into a field as HTML with a font tag, as the HUD's own fields
	// are written. A TextFormat made from native code never reached the fields.
	// The tag takes a whole size only. Scaleform ignores a size with a fraction
	// and the field keeps its default size of 12.
	void SetStyledText(Value& a_field, std::string_view a_text, std::int32_t a_size);

	// Puts a field's translated text in. Plain text starting with $ is
	// translated as it goes in and HTML is not, so the key goes in as plain
	// text first and the translation goes back in as HTML.
	void SetTranslatedText(Value& a_field, const char* a_key, std::int32_t a_size);

	// Moves and sizes a box made by AddBox. The box is drawn 1 by 1, so its
	// scale is its size in pixels.
	void PutBox(Value& a_box, double a_x, double a_y, double a_width, double a_height);

	// Adds a white 1 by 1 box under a_parent, drawn as vanilla menus
	// draw their backing boxes. The HUD tints the white with its colour.
	bool AddBox(Scaleform::GFx::Movie& a_movie, Value& a_parent, const char* a_name);

	// Adds an empty text field under a_parent, in the vanilla fonts and out
	// of the way of the mouse.
	bool AddField(Scaleform::GFx::Movie& a_movie, Value& a_parent, const char* a_name);

	// The HUD menu this movie belongs to, or null while the menu is still being
	// built, which is what the movie load callback sees. It returns the menu a
	// frame later.
	[[nodiscard]] RE::HUDMenu* MenuFor(const Scaleform::GFx::Movie& a_movie);

	// Makes a clip a HUD part of its own, the way the game makes each of its
	// parts. The HUD movie is drawn in white and the HUD colour is applied
	// through rectangles, one per BSGFxShaderFXTarget in the menu's
	// shaderFXObjects list, and anything no target covers is not drawn at all.
	// The list is changed under the menu's cachedQuadsLock, the lock the game
	// reads it under. The list does not own its targets, the vanilla parts own
	// theirs, so the plugin keeps this one and deletes it when the menu is
	// deleted, see Install, or lets go of it once the menu is gone, see
	// ForgetOldTargets. One left listening would still be sent the next HUD's
	// colour, into a movie that no longer exists.
	void AddColorTarget(RE::HUDMenu& a_menu, const Value& a_clip);

	// The vanilla ammo counter part in this HUD menu, or null. Its
	// canBeVisible says whether the HUD mode now lets the counter be seen.
	[[nodiscard]] const RE::HUDAmmoCounter* AmmoCounterPart(const RE::HUDMenu& a_menu);

	// Whether the counter has rounds to count. NEC reads them on the HUD's
	// thread without the lock the game takes for them, so a read is a frame
	// old at worst.
	[[nodiscard]] inline bool HasRounds(const RE::HUDAmmoCounter& a_counter)
	{
		return a_counter.nextAmmoCount.eventDataStruct.optionalValue.has_value();
	}

	// Builds a HUD part of NEC's the first frame its switch reads on, so while
	// it is off the HUD movie gets nothing of NEC's but this listener on its
	// stage. One per part, living as long as the plugin. A built part's
	// listener stays and does nothing, since it could only take itself off
	// through a reference to the movie held past the movie.
	class Waiter final : public Scaleform::GFx::FunctionHandler
	{
	public:
		Waiter(const Settings::Live<bool>& a_on, void (*a_build)(Scaleform::GFx::Movie&)) noexcept :
			on(&a_on),
			build(a_build)
		{}

		// Builds at once while the switch is on, or listens for the first
		// frame it is. a_what names the part in the log.
		void Watch(Scaleform::GFx::Movie& a_movie, const char* a_what);

		void Call(const Params& a_params) override;

	private:
		const Settings::Live<bool>* on;
		void (*build)(Scaleform::GFx::Movie&);

		// The HUD movie still waiting for its part, or null. Only the HUD's
		// thread reads or writes it.
		const Scaleform::GFx::Movie* waiting = nullptr;
	};
}
