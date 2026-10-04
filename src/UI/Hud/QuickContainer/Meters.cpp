#include "UI/Hud/QuickContainer/Meters.h"

#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Hud/QuickContainer/Rows.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace QuickContainer
{
	namespace
	{
		using Scaleform::GFx::Value;

		// -------------------------------------------------------------------
		// The meters on the HUD's rows
		// -------------------------------------------------------------------

		// Something no row holds, so a meter that was never drawn gets drawn.
		constexpr std::int32_t NOT_DRAWN = -2;

		// Where the widget sits in HUDMenu.swf, and its 5 rows from the top,
		// each a QuickContainerItem.
		constexpr const char* WIDGET_PATH = "_root.CenterGroup_mc.QuickContainerWidget_mc";

		constexpr std::array<const char*, MAX_ROWS> ROW_NAMES{ "ItemText0", "ItemText1", "ItemText2", "ItemText3", "ItemText4" };

		// The name each row's meter carries, so the listener can find it again.
		constexpr const char* METER_NAME = "NEC_Condition_mc";

		// The meter's size in the row's coordinates, where the name is 20 high:
		// the HP meter's drawing in small, a bar inside a 1 pixel frame with a
		// pixel of space between.
		constexpr double METER_WIDTH = 26.0;
		constexpr double METER_HEIGHT = 8.0;

		// The space before the meter, a little more than the 4 the row leaves
		// before its first icon.
		constexpr double METER_GAP = 6.0;

		// The height the row's icons are centred on, read off the same symbol.
		constexpr double METER_CENTER_Y = 14.0;

		// The row's own text colours: white, and black on the highlighted row,
		// whose bright bar would hide white.
		constexpr std::uint32_t WHITE = 0xFFFFFF;
		constexpr std::uint32_t BLACK = 0x000000;

		void Rect(Value& a_graphics, double a_x, double a_y, double a_width, double a_height)
		{
			a_graphics.Invoke("drawRect", std::array{ Value(a_x), Value(a_y), Value(a_width), Value(a_height) });
		}

		// Draws a meter with its left edge at x 0 and its middle at y 0.
		void Draw(Value& a_meter, std::int32_t a_percent, bool a_selected)
		{
			Value graphics;
			if (!a_meter.GetMember("graphics"sv, &graphics) || !graphics.IsObject()) {
				return;
			}

			const auto top = -METER_HEIGHT / 2.0;
			const auto bar = (METER_WIDTH - 4.0) * std::clamp(a_percent, 0, 100) / 100.0;

			graphics.Invoke("clear");
			graphics.Invoke("beginFill", std::array{ Value(a_selected ? BLACK : WHITE), Value(1.0) });

			// 4 strips that never overlap, since overlapping shapes in one fill
			// cut holes in each other.
			Rect(graphics, 0.0, top, METER_WIDTH, 1.0);
			Rect(graphics, 0.0, top + METER_HEIGHT - 1.0, METER_WIDTH, 1.0);
			Rect(graphics, 0.0, top + 1.0, 1.0, METER_HEIGHT - 2.0);
			Rect(graphics, METER_WIDTH - 1.0, top + 1.0, 1.0, METER_HEIGHT - 2.0);

			// The bar, a pixel clear of the frame all round.
			if (bar > 0.0) {
				Rect(graphics, 2.0, top + 2.0, bar, METER_HEIGHT - 4.0);
			}

			graphics.Invoke("endFill");
		}

		// -------------------------------------------------------------------
		// Keeping the meters in sync
		// -------------------------------------------------------------------

		// Keeps the meters in sync with the rows, from the HUD's thread.
		class Meters
		{
		public:
			// Forgets every meter drawn into an earlier HUD movie. a_nameWidth
			// is how wide a row's name starts out.
			void Reset(double a_nameWidth)
			{
				nameWidth = a_nameWidth;
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
							Withdraw(row, i);
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
				for (std::size_t i = 0; i < size; i++) {
					// A row waiting to redraw would undo anything changed now.
					// The render listener comes back to it.
					if (!a_afterRedraw && Flash::Bool(rows[i], "bIsDirty"sv)) {
						continue;
					}
					Apply(rows[i], i, percents ? std::optional{ (*percents)[i] } : std::nullopt);
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
			};

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

			// Hides the meter of a row NEC drew into, and gives the name back
			// the width the meter took, but only while the name still has the
			// width it had with a meter.
			void Withdraw(Value& a_row, std::size_t a_index)
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

				Value iconsWidth;
				a_row.Invoke("CalcIconWidth", &iconsWidth);
				const auto withoutMeter = nameWidth - Flash::AsNumber(iconsWidth);
				const auto withMeter = withoutMeter - METER_GAP - METER_WIDTH;
				if (std::abs(Flash::Number(field, "width"sv) - withMeter) < 0.5) {
					field.SetMember("width"sv, Value(withoutMeter));
					a_row.Invoke("AddIconsToEntry");
				}
				last = Drawn{};
			}

			// Lays out one row. a_percent is NO_CONDITION for an item that does
			// not wear, or nothing when no kept build matches.
			void Apply(Value& a_row, std::size_t a_index, std::optional<std::int32_t> a_percent)
			{
				Value field;
				auto  meter = Flash::Child(a_row, METER_NAME);
				if (!a_row.GetMember("ItemName_tf"sv, &field) || !field.IsDisplayObject() || !meter.IsDisplayObject()) {
					return;
				}
				auto& last = drawn[a_index];

				// The row makes room for its icons by narrowing the name by
				// what CalcIconWidth says. Narrowing it by the meter too keeps
				// name, icons and meter centred and shrinks a long name just
				// enough.
				Value iconsWidth;
				a_row.Invoke("CalcIconWidth", &iconsWidth);
				const auto icons = Flash::AsNumber(iconsWidth);
				const auto withoutMeter = nameWidth - icons;
				const auto withMeter = withoutMeter - METER_GAP - METER_WIDTH;
				const auto width = Flash::Number(field, "width"sv);

				// With no build to go on the row is left alone, unless the game
				// just redrew it and a meter left showing would sit on its
				// name.
				if (!a_percent) {
					if (std::abs(width - withMeter) >= 0.5) {
						Hide(meter, last);
					}
					return;
				}

				const auto wanted = *a_percent >= 0 ? withMeter : withoutMeter;
				if (std::abs(width - wanted) >= 0.5) {
					field.SetMember("width"sv, Value(wanted));
					// The row lays its icons out again after the name's new
					// end.
					a_row.Invoke("AddIconsToEntry");
				}

				if (*a_percent < 0) {
					Hide(meter, last);
					return;
				}

				const auto selected = Flash::Bool(a_row, "selected"sv);
				if (last.percent != *a_percent || last.selected != selected) {
					Draw(meter, *a_percent, selected);
					last.percent = *a_percent;
					last.selected = selected;
				}

				// The first icon sits 4 past the name's end, counted in
				// CalcIconWidth, so the icons end at the name's end plus that
				// width. A replaced row whose name is no text field has no
				// line to measure, and the meter stays hidden.
				Value       metrics;
				const Value line{ 0 };
				if (!field.Invoke("getLineMetrics", &metrics, &line, 1) || !metrics.IsObject()) {
					Hide(meter, last);
					return;
				}
				const auto nameEnd = Flash::Number(field, "x"sv) + Flash::Number(metrics, "x"sv) + Flash::Number(metrics, "width"sv);

				// Whole pixels keep the one pixel frame sharp.
				Show(meter, last, std::round(nameEnd + icons + METER_GAP));
			}

			double                      nameWidth{ 0.0 };
			std::array<Drawn, MAX_ROWS> drawn{};
		};

		Meters g_meters;

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

		// Live as long as the plugin, the same way as the item card listener.
		Listener g_renderListener{ true };
		Listener g_frameListener{ false };
	}

	bool AddMeters(Scaleform::GFx::Movie& a_movie)
	{
		Value widget;
		Value list;
		if (!a_movie.GetVariable(&widget, WIDGET_PATH) || !widget.IsDisplayObject() ||
			!widget.GetMember("ListItems_mc"sv, &list) || !list.IsDisplayObject()) {
			REX::WARN("The HUD has no quick container at {:s}, so its rows show no CND. A HUD replacer may have moved it.", WIDGET_PATH);
			return false;
		}

		// The row keeps its starting name width to itself, so it is read from
		// the name before any row is drawn. Every row is the same symbol, so
		// reading the first is enough for all 5.
		std::array<Value, MAX_ROWS> rows;
		double                      nameWidth = 0.0;
		for (std::size_t i = 0; i < MAX_ROWS; i++) {
			rows[i] = Flash::Child(list, ROW_NAMES[i]);
			Value field;
			if (!rows[i].IsDisplayObject() || !rows[i].GetMember("ItemName_tf"sv, &field) || !field.IsDisplayObject()) {
				REX::WARN("The quick container has no row {:s} with a name in it, so its rows show no CND.", ROW_NAMES[i]);
				return false;
			}
			if (i == 0) {
				nameWidth = std::floor(Flash::Number(field, "width"sv));
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

		// A new HUD movie starts with nothing drawn.
		g_meters.Reset(nameWidth);

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
