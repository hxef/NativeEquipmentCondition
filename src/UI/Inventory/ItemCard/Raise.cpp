#include "UI/Inventory/ItemCard/Cards.h"

#include "Core/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Roles/Card.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace ItemCard
{
	namespace
	{
		using Roles::Card::Place;
		using Scaleform::GFx::Value;

		// -------------------------------------------------------------------
		// The CND entry in the card's data
		// -------------------------------------------------------------------

		bool HasText(const Value& a_entry, std::string_view a_text)
		{
			Value text;
			return a_entry.GetMember("text"sv, &text) && text.IsString() && a_text == text.GetString();
		}

		// Whether the card gives this entry a row of its own, the 3 tests
		// ItemCard.redrawUIComponent makes: damage and resistance join the
		// combined rows, and a description goes above everything.
		bool IsPlainRow(const Value& a_entry)
		{
			if (HasText(a_entry, "$dmg"sv) || HasText(a_entry, "$dr"sv)) {
				return false;
			}
			Value description;
			return !(a_entry.GetMember("showAsDescription"sv, &description) && description.IsBoolean() && description.GetBoolean());
		}

		struct Entry
		{
			std::uint32_t index;    // the child vanilla's card draws CND as
			double        percent;  // the number the row prints
		};

		// The card draws its plain rows from the last entry to the first, then
		// the combined damage and resistance rows, then the description, so
		// the entries say which child the CND row is in vanilla's card.
		std::optional<Entry> ConditionEntry(const Value& a_card)
		{
			Value entries;
			if (!a_card.GetMember("InfoObj"sv, &entries) || !entries.IsArray()) {
				return std::nullopt;
			}

			std::uint32_t plainRows = 0;
			for (auto i = entries.GetArraySize(); i-- > 0;) {
				Value entry;
				if (!entries.GetElement(i, &entry) || !entry.IsObject() || !IsPlainRow(entry)) {
					continue;
				}
				if (HasText(entry, CND_TEXT)) {
					return Entry{ plainRows, Flash::Number(entry, "value"sv) };
				}
				plainRows++;
			}
			return std::nullopt;
		}

		// -------------------------------------------------------------------
		// What NEC saw of each card
		// -------------------------------------------------------------------

		// What NEC saw of one card since its movie loaded. Strings only, since
		// the menu can close before the next render.
		struct Seen
		{
			std::optional<std::string> label;     // CND as the card prints it, read once
			std::optional<std::string> firstRow;  // the first row's name at the last render
		};

		// Behind a lock, since a movie may load on another thread than the one
		// its menu draws on.
		std::mutex                             g_lock;
		std::unordered_map<const Place*, Seen> g_seen;

		// CND in the movie's language. The game translates a key as it is
		// written into a text field, so the key goes into a field of NEC's own
		// that is never shown, and is read back. Empty when that fails.
		std::string PrintedLabel(Scaleform::GFx::Movie& a_movie)
		{
			Value field;
			Value text;
			a_movie.CreateObject(&field, "flash.text.TextField");
			if (!Flash::Set(field, "text"sv, Value(CND_TEXT)) || !field.GetMember("text"sv, &text) || !text.IsString()) {
				return {};
			}
			return text.GetString();
		}

		struct Look
		{
			std::string label;
			bool        redrawn;  // the card drew its rows anew since the last render
		};

		// Reads the label at the card's first render after its movie loads,
		// and notes the first row's name at every render. The first render
		// has nothing to compare with, so it never counts as a redraw.
		Look LookAt(Scaleform::GFx::Movie& a_movie, const Place& a_place, Value& a_card)
		{
			auto                   name = Roles::Card::FirstRowName(a_card);
			const std::scoped_lock lock{ g_lock };
			auto&                  seen = g_seen[&a_place];
			if (!seen.label) {
				seen.label = PrintedLabel(a_movie);
				TraceLog::Line("menu", "{:s} prints CND as \"{:s}\"", a_place.movie, *seen.label);
			}
			const auto redrawn = seen.firstRow && *seen.firstRow != name;
			if (redrawn) {
				TraceLog::Line("menu", "{:s} card drawn again, first row {:s}", a_place.movie, name);
			}
			seen.firstRow = std::move(name);
			return { *seen.label, redrawn };
		}

		// -------------------------------------------------------------------
		// Raising the row
		// -------------------------------------------------------------------

		// Moves a freshly drawn CND row up past Damage. Every row above CND
		// moves down by the room CND took, and CND takes the highest one's
		// place.
		void RaiseConditionRow(Scaleform::GFx::Movie& a_movie, const Place& a_place, Value& a_card)
		{
			const auto look = LookAt(a_movie, a_place, a_card);
			const auto entry = ConditionEntry(a_card);
			if (!entry) {
				return;
			}
			if (look.label.empty()) {
				Roles::Noted(a_movie, "translated CND label"sv, "no CND row is raised"sv);
				return;
			}

			auto row = Roles::Card::ConditionRow(a_movie, a_card, entry->index, look.label, entry->percent, look.redrawn);
			if (!row || row->above.empty()) {
				return;
			}

			// y grows downwards, so a CND row already above the highest row
			// means an earlier render event did this.
			const auto rowY = Flash::Number(row->clip, "y"sv);
			const auto lastY = Flash::Number(row->above.back(), "y"sv);
			if (!std::isfinite(rowY) || !std::isfinite(lastY)) {
				Roles::Noted(a_movie, "CND row's place"sv, "CND stays where the card draws it"sv);
				return;
			}
			if (rowY <= lastY) {
				return;
			}

			// Neighbouring rows overlap a little. Reading it from the rows
			// keeps this in sync with the card's spacing.
			const auto& next = row->above.front();
			const auto  overlap = Flash::Number(next, "y"sv) + Flash::Number(next, "height"sv) - rowY;
			const auto  step = Flash::Number(row->clip, "height"sv) - overlap;
			if (!std::isfinite(step)) {
				Roles::Noted(a_movie, "CND row's place"sv, "CND stays where the card draws it"sv);
				return;
			}

			for (auto& clip : row->above) {
				Flash::Set(clip, "y"sv, Value(Flash::Number(clip, "y"sv) + step));
			}
			Flash::Set(row->clip, "y"sv, Value(lastY));

			TraceLog::Line("card", "{:.0f}% row raised above {:d} rows", entry->percent, row->above.size());
		}

		// -------------------------------------------------------------------
		// The trace
		// -------------------------------------------------------------------

		// A number as short as it will go, 48 or 7.2.
		std::string Short(double a_number)
		{
			return std::format("{:g}", a_number);
		}

		// One line for the card on the screen: each entry's text and value, the
		// difference to the equipped item, and a damage entry's type. Written
		// through First, since the listener runs at every redraw in the menu,
		// often many times a second while the list scrolls.
		void TraceCard(const Place& a_place, const Value& a_card)
		{
			Value entries;
			if (!a_card.GetMember("InfoObj"sv, &entries) || !entries.IsArray()) {
				return;
			}

			std::string text;
			const auto  size = entries.GetArraySize();
			for (std::uint32_t i = 0; i < size; i++) {
				Value entry;
				Value name;
				Value value;
				if (!entries.GetElement(i, &entry) || !entry.IsObject() ||
					!entry.GetMember("text"sv, &name) || !name.IsString()) {
					continue;
				}

				text += text.empty() ? "" : ", ";
				text += name.GetString();
				if (entry.GetMember("value"sv, &value)) {
					text += " ";
					text += value.IsString() ? std::string{ value.GetString() } : Short(Flash::AsNumber(value));
				}

				Value flag;
				if (entry.GetMember("showAsPercent"sv, &flag) && flag.IsBoolean() && flag.GetBoolean()) {
					text += "%";
				}
				if (const auto difference = Flash::Number(entry, "difference"sv); difference != 0.0) {
					text += std::format(" ({:s}{:s})", difference > 0.0 ? "+" : "", Short(difference));
				}
				if (Value type; (HasText(entry, "$dmg"sv) || HasText(entry, "$dr"sv)) && entry.GetMember("damageType"sv, &type)) {
					text += std::format(" type {:s}", Short(Flash::AsNumber(type)));
				}
			}
			if (!text.empty()) {
				TraceLog::First("menu", "{:s} card shows {:s}", a_place.movie, text);
			}
		}

		// -------------------------------------------------------------------
		// The render listener
		// -------------------------------------------------------------------

		// The native function the render event calls. The user data is the
		// menu's card place. The card is looked up on every call, since the
		// Pip-Boy loads its inventory page some time after the menu opens and a
		// menu's clips must not be held past the menu.
		class RenderListener final : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				const auto* place = static_cast<const Place*>(a_params.userData);
				if (!place || !a_params.movie) {
					return;
				}
				if (auto card = Roles::Card::Find(*a_params.movie, *place)) {
					RaiseConditionRow(*a_params.movie, *place, *card);
					if (TraceLog::IsOpen()) {
						TraceCard(*place, *card);
					}
				}
			}
		};

		// One listener serves every menu, see Flash.h.
		RenderListener g_renderListener;
	}

	void WatchCard(Scaleform::GFx::Movie& a_movie, std::string_view a_file)
	{
		const auto* place = Roles::Card::For(a_file);
		if (!place) {
			return;
		}
		{
			const std::scoped_lock lock{ g_lock };
			g_seen.erase(place);
		}

		Value stage;
		if (!a_movie.GetVariable(&stage, "_root.stage") || !stage.IsObject()) {
			REX::WARN("{:s} has no stage to listen on, so its CND row stays under Damage.", place->movie);
			return;
		}

		Value listener;
		a_movie.CreateFunction(&listener, &g_renderListener, const_cast<void*>(static_cast<const void*>(place)));

		// The card redraws from its own render listener at default priority.
		// The lowest priority runs after it.
		const std::array<Value, 4> args{
			Value("render"),
			listener,
			Value(false),
			Value(std::numeric_limits<std::int32_t>::min()),
		};
		if (!stage.Invoke("addEventListener", args)) {
			REX::WARN("{:s} refused the render listener, so its CND row stays under Damage.", place->movie);
			return;
		}

		TraceLog::Line("menu", "{:s} loaded, its CND row is kept above Damage", place->movie);
	}
}
