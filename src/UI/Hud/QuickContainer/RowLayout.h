#pragma once

#include "Core/Plugin.h"
#include "UI/Flash.h"
#include "UI/Hud/QuickContainer/Rows.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

// The class that keeps the meters on the HUD's rows in sync. Meters.cpp adds
// them, shows, hides and colours them, RowLayout.cpp fits each row's name,
// icons and meter. Private to this folder.
namespace QuickContainer
{
	// The name each row's meter carries, so the listener can find it again.
	inline constexpr const char* METER_NAME = "NEC_Condition_mc";

	// Something no row holds, so a meter that was never drawn gets drawn.
	inline constexpr std::int32_t NOT_DRAWN = -2;

	// How near 2 widths of a name must be to count as the same.
	inline constexpr double SAME_WIDTH = 0.5;

	// Keeps the meters in sync with the rows, from the HUD's thread.
	class Meters
	{
	public:
		void Reset();
		void Update(Scaleform::GFx::Movie& a_movie, const Scaleform::GFx::Value& a_widget, bool a_afterRedraw);

	private:
		// What was last drawn into a row's meter, and where it was put.
		struct Drawn
		{
			std::int32_t percent{ NOT_DRAWN };
			bool         selected{ false };
			double       x{ 0.0 };
			bool         visible{ false };

			// The item the slot shows, told by its text, count and icons'
			// width, and its name's width: the movie's own and NEC's last
			// write. A width within SAME_WIDTH of it is still NEC's.
			Shown                 item;
			double                icons{ 0.0 };
			double                base{ 0.0 };
			std::optional<double> written;
		};

		bool Show(Scaleform::GFx::Value& a_meter, Drawn& a_drawn, double a_x);
		void Hide(Scaleform::GFx::Value& a_meter, Drawn& a_drawn);
		void Recolor(Scaleform::GFx::Value& a_meter, const Scaleform::GFx::Value& a_name, std::size_t a_index);

		[[nodiscard]] static bool HoldsWrite(const Scaleform::GFx::Value& a_field, const Drawn& a_drawn);
		void WriteWidth(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_row, std::size_t a_index, Scaleform::GFx::Value& a_field, const Scaleform::GFx::Value& a_meter, double a_width);
		void Withdraw(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_row, std::size_t a_index);
		void Apply(Scaleform::GFx::Movie& a_movie, Scaleform::GFx::Value& a_row, std::size_t a_index, const Shown& a_shown, std::optional<std::int32_t> a_percent, bool a_afterRedraw);

		std::array<Drawn, MAX_ROWS> drawn{};

		// The colour each meter carries, as Recolor last wrote it. A meter
		// keeps it while hidden, so only a new HUD movie, with new meters
		// in no colour of their own, forgets it.
		std::array<Flash::Tint, MAX_ROWS> tints{};
	};
}
