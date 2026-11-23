#pragma once

#include "Core/Settings/Settings.h"
#include "UI/Flash.h"
#include "UI/Hud/QuickContainer/Draw.h"
#include "UI/Hud/QuickContainer/Rows.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

// The class that keeps the meters on the HUD's rows in sync. Meters.cpp adds
// them, shows, hides and colours them, RowLayout.cpp fits each row's name,
// icons and meter, and RowPlace.cpp places its CND and writes its number at
// Percent. Private to this folder.
namespace QuickContainer
{
	// The name each row's meter carries, so the listener can find it again.
	inline constexpr const char* METER_NAME = "NEC_Condition_mc";

	// The name of a row's number field, made the first time the row shows
	// a number at Percent.
	inline constexpr const char* NUMBER_NAME = "NEC_ConditionNumber_tf";

	// Something no row holds, so a meter that was never drawn gets drawn.
	inline constexpr std::int32_t NOT_DRAWN = -2;

	// How near 2 widths of a name must be to count as the same.
	inline constexpr double SAME_WIDTH = 0.5;

	// How many fields beside ItemName_tf a row's name may be drawn in, such
	// as a title and its subtitle, see Meters::NarrowOthers.
	inline constexpr std::size_t MAX_OTHER_FIELDS = 4;

	// Where a row's CND stands, for its trace line: at the row's right edge
	// a_edge, or after the name.
	[[nodiscard]] std::string Standing(std::optional<double> a_edge);

	// Keeps the meters in sync with the rows, from the HUD's thread.
	class Meters
	{
	public:
		void Reset();
		void Update(Scaleform::GFx::Movie& a_movie, const Scaleform::GFx::Value& a_widget, bool a_afterRedraw);

	private:
		// A field beside ItemName_tf that a HUD mod draws the row's name in,
		// told by its name, with its own width and NEC's last write. A mod
		// may never set its width again, so its own is kept here.
		struct Other
		{
			std::string           name;
			double                base{ 0.0 };
			std::optional<double> written;
		};

		// What was last drawn into a row's meter, and where it was put.
		struct Drawn
		{
			std::int32_t percent{ NOT_DRAWN };
			bool         selected{ false };
			bool         fromRight{ false };
			double       x{ 0.0 };
			bool         visible{ false };

			// The item the slot shows, told by its text, count and icons'
			// width, and its name's width: the movie's own and NEC's last
			// write. A width within SAME_WIDTH of it is still NEC's.
			Shown                 item;
			double                icons{ 0.0 };
			double                base{ 0.0 };
			std::optional<double> written;

			// Where the room the movie gives the row's text ends, see Measure,
			// and the other fields the name is drawn in.
			std::optional<double>                   room;
			std::array<Other, MAX_OTHER_FIELDS>     others{};

			// Where Place last asked whether the row's right edge is clear,
			// and whether it is.
			std::optional<double> probed;
			bool                  edgeFree{ false };

			// At Percent: the number last written into the number field,
			// whether on the highlighted row, how wide its letters are,
			// where the field was put and whether it shows, and the CND
			// word as the movie translates it. widest is the widest text
			// the row can write, widestOf those texts and letters the
			// name's font they were measured in, see Widest.
			std::string            number;
			bool                   numberSelected{ false };
			double                 numberWidth{ 0.0 };
			double                 numberX{ 0.0 };
			bool                   numberShown{ false };
			std::string            word;
			std::optional<Letters> letters;
			std::string            widestOf;
			double                 widest{ 0.0 };
		};

		// Where a row's CND starts across, and the row's right edge when
		// it stands there, see Place in RowLayout.cpp.
		struct Spot
		{
			double                x = 0.0;
			std::optional<double> edge;
		};

		[[nodiscard]] static Spot Place(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_row, const Scaleform::GFx::Value& a_name, Drawn& a_drawn, double a_nameEnd,
			double a_icons, double a_width, double a_end, bool a_afterRedraw);
		bool Show(Scaleform::GFx::Value& a_meter, Drawn& a_drawn, double a_x);
		void Hide(Scaleform::GFx::Value& a_meter, Drawn& a_drawn);
		static void HideNumber(Scaleform::GFx::Value& a_row, Drawn& a_drawn);
		[[nodiscard]] static std::optional<double> Widest(Scaleform::GFx::Value& a_number, const Scaleform::GFx::Value& a_name, Drawn& a_drawn);
		void ShowNumber(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_row, std::size_t a_index, Scaleform::GFx::Value& a_number, Scaleform::GFx::Value& a_name, const Reading& a_reading,
			double a_icons, double a_end, bool a_afterRedraw);
		void Recolor(Scaleform::GFx::Value& a_meter, const Scaleform::GFx::Value& a_name, std::size_t a_index);

		[[nodiscard]] static bool HoldsWrite(const Scaleform::GFx::Value& a_field, const Drawn& a_drawn);
		void WriteWidth(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_row, std::size_t a_index, Scaleform::GFx::Value& a_field, const Scaleform::GFx::Value& a_meter, double a_width);
		void NarrowOthers(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_row, std::size_t a_index, std::optional<double> a_limit);
		bool GiveOthersBack(Scaleform::GFx::Value& a_row, Drawn& a_drawn);
		void LayIcons(Scaleform::GFx::Value& a_row, std::size_t a_index);
		void Withdraw(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_row, std::size_t a_index);
		bool Measure(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_row, std::size_t a_index, const Shown& a_shown, bool a_afterRedraw);
		void Apply(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_row, std::size_t a_index, std::optional<Reading> a_reading, bool a_asNumber,
			std::optional<double> a_edge, bool a_afterRedraw);

		std::array<Drawn, MAX_ROWS> drawn{};

		// The colour each meter carries, as Recolor last wrote it. A meter
		// keeps it while hidden, so only a new HUD movie, with new meters
		// in no colour of their own, forgets it.
		std::array<Flash::Tint, MAX_ROWS> tints{};

		// Whether the trace last said the meters are drawn flipped, see
		// HudParts::Readout::TellFlipped.
		bool saidFlipped = false;

		// The form last seen, and whether the trace said the rows show
		// numbers since the form became Percent.
		std::optional<Settings::Cnd> saidForm;
		bool                         saidNumbers = false;

		// Where the trace last said each row's CND stands, at the right
		// edge or after the name, see bQuickContainerCndAtRight.
		std::optional<bool> saidAtRight;
	};
}
