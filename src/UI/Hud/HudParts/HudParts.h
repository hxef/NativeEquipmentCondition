#pragma once

#include "Core/Plugin.h"

#include "Core/Settings.h"

#include <cstdint>
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
	// FontConfig.txt maps each to a font in the game's library.
	inline constexpr const char* FONT_NAME = "$MAIN_Font_Bold";

	// A text field keeps a 2 wide margin inside its box.
	inline constexpr double FIELD_MARGIN = 2.0;

	// The condition of the weapon in the player's hands, as the HUD knows it.
	// It is checked through F4SE's task queue and read from an atomic a frame
	// later, and both readouts share it. F4SE runs the task on a worker thread
	// during play, so the check holds the inventory lock while it reads. While
	// bHudCondition is off nothing is checked and Percent says NONE, so both
	// readouts fade as for a weapon with no condition.
	namespace Weapon
	{
		// What Percent returns when nothing in hand wears out, which hides a
		// readout.
		inline constexpr std::int32_t NONE = -1;

		// Asks for a fresh check, which arrives a frame or so later. Asking
		// again while one is on its way does nothing.
		void Queue();

		// The condition of the weapon in hand as a whole percent, or NONE.
		[[nodiscard]] std::int32_t Percent();

		// Whether the player has a weapon drawn. A melee weapon hides the ammo
		// counter, so a readout beside it has to know.
		[[nodiscard]] bool Drawn();
	}

	// The readout itself: a bar over a dimmed track, and the word CND past the
	// right end of the bar, drawn the way the AP meter is. The bar keeps its
	// right end still and shrinks from the left, the track shows the worn part
	// at half opacity like the HP meter's spare bar, and the word starts a
	// little past the bar. A readout's origin is the right end of its bar,
	// halfway down, so placing a readout means saying where its bar ends.
	//
	// The ammo counter and the Pip-Boy's doll size everything from the font
	// size, in the AP meter's proportions below: at font size 20 the bar is 6
	// deep, like the AP bar. The power armor readout sizes its bar and its word
	// separately to fit the dash, with the word level with the bar and
	// anti-aliased for animation since it moves.
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

		// Lays the track out a_width across and a_deep deep.
		void SetTrack(Value& a_readout, double a_width, double a_deep);

		// Sets how much of a bar laid out as above is left.
		void SetPercent(Value& a_readout, double a_width, double a_deep, std::int32_t a_percent);
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

	// Adds a white 1 by 1 box under a_parent, drawn as the game's own menus
	// draw their backing boxes. The HUD tints the white with its colour.
	bool AddBox(Scaleform::GFx::Movie& a_movie, Value& a_parent, const char* a_name);

	// Adds an empty text field under a_parent, in the game's own fonts and out
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
	// reads it under. The list does not own its targets, the game's parts own
	// theirs, so the plugin keeps this one and deletes it when the menu is
	// deleted, see Install, or lets go of it once the menu is gone, see
	// ForgetOldTargets. One left listening would still be sent the next HUD's
	// colour, into a movie that no longer exists.
	void AddColorTarget(RE::HUDMenu& a_menu, const Value& a_clip);

	// Where the HUD part carrying this vtable keeps canBeVisible, which says
	// whether the current HUD mode lets it be seen, or null. The game writes
	// the byte and the HUD reads it, so no lock.
	[[nodiscard]] const bool* ComponentCanBeVisible(const RE::HUDMenu& a_menu, std::uintptr_t a_vtable);

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
