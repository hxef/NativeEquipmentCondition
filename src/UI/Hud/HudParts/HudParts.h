#pragma once

#include "Core/Plugin.h"

#include "Core/Settings/Settings.h"
#include "UI/Flash.h"
#include "UI/Hud/HudParts/Readout.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// The pieces the plugin's CND readouts are built from. CND is drawn on the HUD
// 3 times: in the ammo counter out of power armor, on the dash in it, and in
// the pickup prompt. The HudCondition, PowerArmorCondition and PickupBar
// folders each place it by their own rules, and all 3 share the readout, the
// HUD menu lookup and the colour kept here. The 2 weapon bars also share the
// condition of the weapon in hand, and the pickup bar follows the prompt's
// warning colour. Readout.h declares the readout, Readout.cpp and
// Percent.cpp draw it, Draw.cpp the boxes and text fields it is built from,
// HudParts.cpp the rest.
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
	// both readouts are at Off, or bNecOn is off, nothing is checked. While 1
	// is, Percent says NONE to it, so it fades as for a weapon with no
	// condition.
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
		// while a_on, the asking place's stepper, is at Off or bNecOn is off.
		[[nodiscard]] std::int32_t Percent(Settings::Gate a_on);

		// The same condition in tenths, as the weapon's card writes it, see
		// Condition::CardTenths, or nothing whenever Percent says NONE.
		[[nodiscard]] std::optional<std::uint32_t> Tenths(Settings::Gate a_on);

		// Whether the player has a weapon drawn. A melee weapon hides the ammo
		// counter, so a readout beside it has to know.
		[[nodiscard]] bool Drawn();

		// Forgets the last reading when another game starts, so the readouts
		// show nothing of the last session's weapon until the next check. See
		// ForgetPlay in main.cpp.
		void Forget();
	}

	// Puts text into a field as HTML with a font tag, as the HUD's own fields
	// are written. A TextFormat made from native code never reached the fields.
	// The tag takes a whole size only. Scaleform ignores a size with a fraction
	// and the field keeps its default size of 12.
	void SetStyledText(Value& a_field, std::string_view a_text, std::int32_t a_size);

	// The translation of a_key, read back from a_field, which is left holding
	// it as plain text. Plain text starting with $ is translated as it goes
	// in and HTML is not.
	[[nodiscard]] std::string Translated(Value& a_field, const char* a_key);

	// Puts a field's translated text in: the key goes in as plain text
	// first and the translation goes back in as HTML.
	void SetTranslatedText(Value& a_field, const char* a_key, std::int32_t a_size);

	// Moves and sizes a box made by AddBox. The box is drawn 1 by 1, so its
	// scale is its size in pixels.
	void PutBox(Value& a_box, double a_x, double a_y, double a_width, double a_height);

	// Adds a white 1 by 1 box under a_parent, drawn as vanilla menus
	// draw their backing boxes. The HUD tints the white with its colour.
	bool AddBox(Scaleform::GFx::Movie& a_movie, Value& a_parent, const char* a_name);

	// Adds a 1 by 1 box under a_parent holding 2 black triangles, 1 with its
	// base on the top edge and 1 on the bottom edge, each reaching a_deep of
	// the box's depth in. PutBox sizes it like any box.
	bool AddNotches(Scaleform::GFx::Movie& a_movie, Value& a_parent, const char* a_name, double a_deep);

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
	// colour, into a movie that no longer exists. Returns the target to name
	// to SetWarningColor, or null when none was made.
	const RE::BSGFxShaderFXTarget* AddColorTarget(RE::HUDMenu& a_menu, const Value& a_clip);

	// Paints a target AddColorTarget made in the warning colour, or back in
	// the gameplay HUD colour, as the game paints its own parts. The target is
	// looked up first, since all of them are deleted at once if another DLL
	// takes the HUD menu's delete hook from NEC. Returns false once it is gone.
	bool SetWarningColor(const RE::BSGFxShaderFXTarget* a_target, bool a_warning);

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

	// The vanilla pickup prompt part in this HUD menu, or null. Its
	// displayWarningColor says whether taking the item it shows is stealing.
	[[nodiscard]] const RE::HUDComponentBase* RolloverPart(const RE::HUDMenu& a_menu);

	// Builds a HUD part of NEC's the first frame its switch or stepper reads
	// on, so while it is off the HUD movie gets nothing of NEC's but this
	// listener on its stage. One per part, living as long as the plugin. A
	// built part's listener stays and does nothing, since it could only take
	// itself off through a reference to the movie held past the movie.
	class Waiter final : public Scaleform::GFx::FunctionHandler
	{
	public:
		Waiter(Settings::Gate a_on, void (*a_build)(Scaleform::GFx::Movie&)) noexcept :
			on(a_on),
			build(a_build)
		{}

		// Builds at once while the switch or stepper is on, or listens for
		// the first frame it is. a_what names the part in the log.
		void Watch(Scaleform::GFx::Movie& a_movie, const char* a_what);

		void Call(const Params& a_params) override;

	private:
		Settings::Gate on;
		void (*build)(Scaleform::GFx::Movie&);

		// The HUD movie still waiting for its part, or null. Only the HUD's
		// thread reads or writes it.
		const Scaleform::GFx::Movie* waiting = nullptr;
	};
}
