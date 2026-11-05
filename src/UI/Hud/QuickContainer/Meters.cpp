#include "UI/Hud/QuickContainer/Meters.h"

#include "Core/Settings.h"
#include "Core/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Hud/QuickContainer/Draw.h"
#include "UI/Hud/QuickContainer/RowLayout.h"
#include "UI/Hud/QuickContainer/Rows.h"
#include "UI/Roles/Hud.h"

#include <array>
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
	}

	// -------------------------------------------------------------------
	// Keeping the meters in sync
	// -------------------------------------------------------------------

	// Forgets every meter drawn into an earlier HUD movie.
	void Meters::Reset()
	{
		drawn.fill({});
		tints.fill({});
	}

	// a_afterRedraw says the rows have just redrawn, which is when the
	// render listener calls. The frame listener leaves a row still
	// waiting to redraw alone.
	void Meters::Update(Scaleform::GFx::Movie& a_movie, const Value& a_widget, bool a_afterRedraw)
	{
		auto list = Flash::Member(a_widget, "ListItems_mc"sv);
		if (!Flash::Bool(a_widget, "visible"sv) || !list.IsDisplayObject()) {
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

	// -------------------------------------------------------------------
	// Showing and hiding a meter
	// -------------------------------------------------------------------

	// True when the meter moved or came into view.
	bool Meters::Show(Value& a_meter, Drawn& a_drawn, double a_x)
	{
		if (a_drawn.x == a_x && a_drawn.visible) {
			return false;
		}
		a_meter.SetMember("x"sv, Value(a_x));
		a_meter.SetMember("y"sv, Value(METER_CENTER_Y));
		a_meter.SetMember("visible"sv, Value(true));
		a_drawn.x = a_x;
		a_drawn.visible = true;
		return true;
	}

	void Meters::Hide(Value& a_meter, Drawn& a_drawn)
	{
		if (a_drawn.visible) {
			a_meter.SetMember("visible"sv, Value(false));
			a_drawn.visible = false;
		}
	}

	// -------------------------------------------------------------------
	// A meter's colour
	// -------------------------------------------------------------------

	// The meter takes the colour its row's name has of its own, so both
	// show alike under whatever colours the list above them. The vanilla
	// HUD colours the whole list, the warning colour too, so the name has
	// none of its own and nothing is written. A HUD mod that clears the
	// list's colour and colours each part of a row itself would leave
	// the meter white. A HUD colours a row as it redraws, so the caller
	// asks then.
	void Meters::Recolor(Value& a_meter, const Value& a_name, std::size_t a_index)
	{
		const auto wanted = Flash::TintOf(a_name);
		auto&      carried = tints[a_index];
		if (!wanted || wanted->Near(carried) || !Flash::SetTint(a_meter, *wanted)) {
			return;
		}
		carried = *wanted;
		TraceLog::Line("menu", "Quick container row {:d} CND meter takes its name's colour, times {:.3f} {:.3f} {:.3f} plus {:.0f} {:.0f} {:.0f}",
			a_index, carried.red, carried.green, carried.blue, carried.redAdd, carried.greenAdd, carried.blueAdd);
	}

	namespace
	{
		Meters g_meters;

		// -------------------------------------------------------------------
		// Listening to the HUD
		// -------------------------------------------------------------------

		// The rows redraw from Flash's render event before a frame is drawn,
		// whenever the widget gets new rows. The render listener lays them out
		// as drawn, the frame listener catches changes without a redraw, and
		// both read the widget as members, see Roles::Hud::AmmoCounter.
		class Listener final : public Scaleform::GFx::FunctionHandler
		{
		public:
			explicit Listener(bool a_afterRedraw) noexcept :
				afterRedraw(a_afterRedraw)
			{}

			void Call(const Params& a_params) override
			{
				auto       on = a_params.argCount > 0 ? Flash::Member(a_params.args[0], "currentTarget"sv) : Value{};
				const auto widget = afterRedraw ? Flash::Member(Flash::Member(Flash::ChildAt(on, 0), "CenterGroup_mc"sv), "QuickContainerWidget_mc"sv) : Flash::Member(on, "parent"sv);
				if (a_params.movie) {
					g_meters.Update(*a_params.movie, widget, afterRedraw);
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
