#include "UI/Hud/QuickContainer/Meters.h"

#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Hud/QuickContainer/Draw.h"
#include "UI/Hud/QuickContainer/Rows.h"
#include "UI/Roles/Hud.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

namespace QuickContainer
{
	namespace
	{
		using Scaleform::GFx::Value;

		// -------------------------------------------------------------------
		// Finding the rows
		// -------------------------------------------------------------------

		// Where the widget sits in HUDMenu.swf, and its 5 rows from the top,
		// each a QuickContainerItem.
		constexpr const char* WIDGET_PATH = "_root.CenterGroup_mc.QuickContainerWidget_mc";

		constexpr std::array<const char*, MAX_ROWS> ROW_NAMES{ "ItemText0", "ItemText1", "ItemText2", "ItemText3", "ItemText4" };

		// The name each row's meter carries, so the listener can find it again.
		constexpr const char* METER_NAME = "NEC_Condition_mc";

		// The widget's list of rows, or an undefined value where a HUD replacer
		// moved it.
		Value RowList(Scaleform::GFx::Movie& a_movie)
		{
			Value widget;
			Value list;
			if (!a_movie.GetVariable(&widget, WIDGET_PATH) || !widget.IsDisplayObject() ||
				!widget.GetMember("ListItems_mc"sv, &list) || !list.IsDisplayObject()) {
				return {};
			}
			return list;
		}

		// -------------------------------------------------------------------
		// Keeping the meters in sync
		// -------------------------------------------------------------------

		// Something no row holds, so a meter that was never drawn gets drawn.
		constexpr std::int32_t NOT_DRAWN = -2;

		// How near 2 widths of a name must be to count as the same.
		constexpr double SAME_WIDTH = 0.5;

		// Keeps the meters in sync with the rows, from the HUD's thread.
		class Meters
		{
		public:
			// Forgets every meter drawn into an earlier HUD movie.
			void Reset()
			{
				drawn.fill({});
			}

			// a_afterRedraw says the rows have just redrawn, which is when the
			// render listener calls. The frame listener leaves a row still
			// waiting to redraw alone.
			void Update(Scaleform::GFx::Movie& a_movie, bool a_afterRedraw)
			{
				Value widget;
				Value list;
				if (!a_movie.GetVariable(&widget, WIDGET_PATH) || !widget.IsDisplayObject() || !Flash::Bool(widget, "visible"sv) ||
					!widget.GetMember("ListItems_mc"sv, &list) || !list.IsDisplayObject()) {
					return;
				}

				// Switched off, a row NEC drew into goes back to how the game
				// draws it, once, and every other row is left as it is.
				if (!Settings::bQuickContainer.GetValue()) {
					for (std::size_t i = 0; i < MAX_ROWS; i++) {
						auto row = Flash::Child(list, ROW_NAMES[i]);
						if (a_afterRedraw || !Flash::Bool(row, "bIsDirty"sv)) {
							Withdraw(a_movie, row, i);
						}
					}
					return;
				}

				// The rows fill from the top, and a row without data hides
				// itself and its meter.
				std::array<Value, MAX_ROWS> rows;
				std::array<Shown, MAX_ROWS> shown;
				std::size_t                 size = 0;
				for (std::size_t i = 0; i < MAX_ROWS; i++) {
					rows[i] = Flash::Child(list, ROW_NAMES[i]);
					Value data;
					if (size == i && rows[i].IsDisplayObject() && rows[i].GetMember("data"sv, &data) && data.IsObject()) {
						shown[i].text = Flash::String(data, "text"sv);
						shown[i].count = static_cast<std::uint32_t>(Flash::Number(data, "count"sv));
						size++;
					}
				}

				const auto percents = FindConditions(shown, size);
				if (!percents && size > 0) {
					Roles::Noted(a_movie, "kept build matching the quick container rows"sv, "the rows get no CND meter"sv);
				}
				for (std::size_t i = 0; i < size; i++) {
					// A row waiting to redraw would undo anything changed now.
					// The render listener comes back to it.
					if (!a_afterRedraw && Flash::Bool(rows[i], "bIsDirty"sv)) {
						continue;
					}
					Apply(a_movie, rows[i], i, shown[i], percents ? std::optional{ (*percents)[i] } : std::nullopt, a_afterRedraw);
				}
			}

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

			// -----------------------------------------------------------------
			// Showing and hiding a meter
			// -----------------------------------------------------------------

			void Show(Value& a_meter, Drawn& a_drawn, double a_x)
			{
				if (a_drawn.x != a_x || !a_drawn.visible) {
					a_meter.SetMember("x"sv, Value(a_x));
					a_meter.SetMember("y"sv, Value(METER_CENTER_Y));
					a_meter.SetMember("visible"sv, Value(true));
					a_drawn.x = a_x;
					a_drawn.visible = true;
				}
			}

			void Hide(Value& a_meter, Drawn& a_drawn)
			{
				if (a_drawn.visible) {
					a_meter.SetMember("visible"sv, Value(false));
					a_drawn.visible = false;
				}
			}

			// -----------------------------------------------------------------
			// A row's name width and layout
			// -----------------------------------------------------------------

			// Whether a_field's width is still NEC's last write in a_drawn.
			[[nodiscard]] static bool HoldsWrite(const Value& a_field, const Drawn& a_drawn)
			{
				return a_drawn.written && std::abs(Flash::Number(a_field, "width"sv) - *a_drawn.written) < SAME_WIDTH;
			}

			// Writes the name's width. A centred name's text moves when its
			// field narrows, so the parts the movie placed against the text's
			// start move as far. The row then lays its icons out again after
			// the name's new end.
			void WriteWidth(Scaleform::GFx::Movie& a_movie, Value& a_row, Value& a_field, const Value& a_meter, double a_width)
			{
				auto before = Roles::Hud::TextStart(a_movie, a_row, a_field, a_meter);
				a_field.SetMember("width"sv, Value(a_width));
				const auto after = before && !before->against.empty() ? Roles::Hud::TextStart(a_movie, a_row, a_field, a_meter) : std::nullopt;
				if (after) {
					for (auto& part : before->against) {
						Flash::Set(part, "x"sv, Value(Flash::Number(part, "x"sv) + after->x - before->x));
					}
				}
				a_row.Invoke("AddIconsToEntry");
			}

			// Hides the meter of a row NEC drew into, and gives the name back
			// the width the meter took, but only while the name still holds
			// NEC's write, so a width the movie set since stays. A row with no
			// data is hidden and sets its width at its next redraw, and
			// AddIconsToEntry fails on it, so it gets no give back.
			void Withdraw(Scaleform::GFx::Movie& a_movie, Value& a_row, std::size_t a_index)
			{
				auto& last = drawn[a_index];
				if (last.percent == NOT_DRAWN && !last.visible) {
					return;
				}

				Value field;
				auto  meter = Flash::Child(a_row, METER_NAME);
				if (!a_row.IsDisplayObject() || !a_row.GetMember("ItemName_tf"sv, &field) || !field.IsDisplayObject() || !meter.IsDisplayObject()) {
					return;
				}
				Hide(meter, last);

				Value data;
				if (a_row.GetMember("data"sv, &data) && data.IsObject() && HoldsWrite(field, last)) {
					WriteWidth(a_movie, a_row, field, meter, last.base);
				}
				last = Drawn{};
			}

			// Lays out one row. a_percent is NO_CONDITION for an item that does
			// not wear, or nothing when no kept build matches.
			void Apply(Scaleform::GFx::Movie& a_movie, Value& a_row, std::size_t a_index, const Shown& a_shown, std::optional<std::int32_t> a_percent, bool a_afterRedraw)
			{
				Value field;
				auto  meter = Flash::Child(a_row, METER_NAME);
				if (!a_row.GetMember("ItemName_tf"sv, &field) || !field.IsDisplayObject() || !meter.IsDisplayObject()) {
					return;
				}
				auto& last = drawn[a_index];

				// The row makes room for its icons by narrowing the name by
				// what CalcIconWidth says, and a movie may narrow it more for
				// icons of its own. Narrowing it by the meter too keeps name,
				// icons and meter centred and shrinks a long name just enough.
				Value iconsWidth;
				a_row.Invoke("CalcIconWidth", &iconsWidth);
				const auto icons = Flash::AsNumber(iconsWidth);

				// A slot showing another item takes its name's width afresh,
				// since the movie's new width can equal NEC's last write. Any
				// other width than that write is the movie's own. A change the
				// frame listener sees first came without a redraw, so a width
				// still at NEC's write is NEC's and goes back first.
				if (last.item.text != a_shown.text || last.item.count != a_shown.count || last.icons != icons) {
					if (!a_afterRedraw && HoldsWrite(field, last)) {
						WriteWidth(a_movie, a_row, field, meter, last.base);
					}
					last.item = a_shown;
					last.icons = icons;
					last.written.reset();
				}
				const auto width = Flash::Number(field, "width"sv);
				if (!last.written || std::abs(width - *last.written) >= SAME_WIDTH) {
					last.base = width;
					last.written.reset();
				}

				// With no build to go on the row is left alone, unless the game
				// just redrew it and a meter left showing would sit on its
				// name.
				if (!a_percent) {
					if (!last.written) {
						Hide(meter, last);
					}
					return;
				}

				const auto wanted = *a_percent >= 0 ? last.base - METER_GAP - METER_WIDTH : last.base;
				if (std::abs(width - wanted) >= SAME_WIDTH) {
					WriteWidth(a_movie, a_row, field, meter, wanted);
				}
				if (*a_percent < 0) {
					last.written.reset();
					Hide(meter, last);
					return;
				}
				last.written = wanted;

				const auto selected = Flash::Bool(a_row, "selected"sv);
				if (last.percent != *a_percent || last.selected != selected) {
					Draw(meter, *a_percent, selected);
					last.percent = *a_percent;
					last.selected = selected;
				}

				// The first icon sits 4 past the name's end, counted in
				// CalcIconWidth, so the icons end at the name's end plus that
				// width. A row with no shown text to measure keeps the meter
				// hidden.
				const auto nameEnd = Roles::Hud::TextEnd(a_movie, a_row);
				if (!nameEnd) {
					Roles::Noted(a_movie, "quick container name's end"sv, "that row shows no CND meter"sv);
					Hide(meter, last);
					return;
				}

				// Whole pixels keep the one pixel frame sharp.
				Show(meter, last, std::round(*nameEnd + icons + METER_GAP));
			}

			std::array<Drawn, MAX_ROWS> drawn{};
		};

		Meters g_meters;

		// -------------------------------------------------------------------
		// Listening to the HUD
		// -------------------------------------------------------------------

		// The rows redraw from Flash's render event, raised before a frame is
		// drawn whenever the widget gets new rows. The render listener lays
		// them out as they are drawn, and the frame listener catches changes
		// without a redraw.
		class Listener final : public Scaleform::GFx::FunctionHandler
		{
		public:
			explicit Listener(bool a_afterRedraw) noexcept :
				afterRedraw(a_afterRedraw)
			{}

			void Call(const Params& a_params) override
			{
				if (a_params.movie) {
					g_meters.Update(*a_params.movie, afterRedraw);
				}
			}

		private:
			bool afterRedraw;
		};

		Listener g_renderListener{ true };
		Listener g_frameListener{ false };
	}

	void ForgetMeters()
	{
		g_meters.Reset();
	}

	bool AddMeters(Scaleform::GFx::Movie& a_movie)
	{
		auto list = RowList(a_movie);
		if (!list.IsDisplayObject()) {
			REX::WARN("The HUD has no quick container at {:s}, so its rows show no CND. A HUD replacer may have moved it.", WIDGET_PATH);
			return false;
		}

		std::array<Value, MAX_ROWS> rows;
		for (std::size_t i = 0; i < MAX_ROWS; i++) {
			rows[i] = Flash::Child(list, ROW_NAMES[i]);
			Value field;
			if (!rows[i].IsDisplayObject() || !rows[i].GetMember("ItemName_tf"sv, &field) || !field.IsDisplayObject()) {
				REX::WARN("The quick container has no row {:s} with a name in it, so its rows show no CND.", ROW_NAMES[i]);
				return false;
			}
		}

		for (auto& row : rows) {
			Value meter;
			a_movie.CreateObject(&meter, "flash.display.Shape");
			if (!meter.IsDisplayObject()) {
				REX::WARN("The HUD could not make a condition meter, so the quick container rows show no CND.");
				return false;
			}
			meter.SetMember("name"sv, Value(METER_NAME));
			meter.SetMember("visible"sv, Value(false));
			row.Invoke("addChild", std::array{ meter });
		}

		Value stage;
		if (!a_movie.GetVariable(&stage, "_root.stage") || !stage.IsObject()) {
			REX::WARN("The HUD has no stage to listen on, so the quick container rows show no CND.");
			return false;
		}

		// The rows redraw from their own render listeners at default priority.
		// The lowest priority runs after them.
		Value render;
		a_movie.CreateFunction(&render, &g_renderListener);
		const std::array<Value, 4> renderArgs{
			Value("render"),
			render,
			Value(false),
			Value(std::numeric_limits<std::int32_t>::min()),
		};

		Value frame;
		a_movie.CreateFunction(&frame, &g_frameListener);

		if (!stage.Invoke("addEventListener", renderArgs) || !list.Invoke("addEventListener", std::array{ Value("enterFrame"), frame })) {
			REX::WARN("The HUD refused the quick container listeners, so its rows show no CND.");
			return false;
		}

		TraceLog::Line("menu", "HUDMenu.swf loaded, quick container rows carry a CND meter");
		return true;
	}
}
