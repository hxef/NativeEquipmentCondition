#include "UI/Inventory/ItemCard/Cards.h"

#include "Condition/Condition.h"
#include "Core/Settings/Settings.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Inventory/Pipboy.h"
#include "UI/Roles/Card.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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

		// The movie whose card keeps no member of NEC's own, see Pipboy.h.
		constexpr std::string_view PIPBOY_MOVIE = "PipboyMenu.swf";

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
			std::uint32_t                  index;             // the child vanilla's card draws CND as
			std::uint32_t                  tenths;            // the number the row prints, see Condition::CardTenths
			bool                           fridge;            // the row gets the fridge icon
			std::optional<float>           wearRate;          // the item's wear rate, see WearRate.cpp
			std::optional<float>           wearRateCompared;  // the rate of the item compared, see WEAR_RATE_COMPARED_MARK
			std::optional<Condition::Kind> kind;              // whose penalty point the row's bar marks
		};

		// A rate a CND entry carries under a_mark, which only a card outside
		// the Pip-Boy keeps.
		std::optional<float> WearRateIn(const Value& a_entry, std::string_view a_mark)
		{
			const auto rate = Flash::Member(a_entry, a_mark);
			return Flash::IsAnyNumber(rate) ? std::optional{ static_cast<float>(Flash::AsNumber(rate)) } : std::nullopt;
		}

		// The kind a CND entry carries under PENALTY_KIND_MARK, which only a
		// card outside the Pip-Boy keeps.
		std::optional<Condition::Kind> KindIn(const Value& a_entry)
		{
			const auto kind = Flash::Member(a_entry, PENALTY_KIND_MARK);
			if (!Flash::IsAnyNumber(kind)) {
				return std::nullopt;
			}
			const auto number = Flash::AsNumber(kind);
			if (number == static_cast<double>(Condition::Kind::kWeapon)) {
				return Condition::Kind::kWeapon;
			}
			return number == static_cast<double>(Condition::Kind::kArmor) ? std::optional{ Condition::Kind::kArmor } : std::nullopt;
		}

		// The tenths a CND entry's value stands for. NEC fed it as
		// Condition::CardValue, a hair above its tenth at most, so the nearest
		// tenth is the one. Kept between 0 and 1000.
		std::uint32_t TenthsIn(const Value& a_entry)
		{
			const auto value = Flash::Number(a_entry, "value"sv);
			const auto full = static_cast<double>(Condition::FULL_TENTHS);
			const auto tenths = value * static_cast<double>(Condition::TENTHS_PER_PERCENT);
			return std::isfinite(value) ? static_cast<std::uint32_t>(std::lround(std::clamp(tenths, 0.0, full))) : 0;
		}

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
					return Entry{ plainRows, TenthsIn(entry), Flash::Bool(entry, FRIDGE_MARK), WearRateIn(entry, WEAR_RATE_MARK),
						WearRateIn(entry, WEAR_RATE_COMPARED_MARK), KindIn(entry) };
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
		// place. False when it stays, as an earlier render left it or where
		// the card draws it.
		bool Raise(Scaleform::GFx::Movie& a_movie, Roles::Card::Row& a_row)
		{
			if (a_row.above.empty()) {
				return false;
			}

			// y grows downwards, so a CND row already above the highest row
			// means an earlier render event did this.
			const auto rowY = Flash::Number(a_row.clip, "y"sv);
			const auto lastY = Flash::Number(a_row.above.back(), "y"sv);
			if (!std::isfinite(rowY) || !std::isfinite(lastY)) {
				Roles::Noted(a_movie, "CND row's place"sv, "CND stays where the card draws it"sv);
				return false;
			}
			if (rowY <= lastY) {
				return false;
			}

			// Neighbouring rows overlap a little. Reading it from the rows
			// keeps this in sync with the card's spacing.
			const auto& next = a_row.above.front();
			const auto  overlap = Flash::Number(next, "y"sv) + Flash::Number(next, "height"sv) - rowY;
			const auto  step = Flash::Number(a_row.clip, "height"sv) - overlap;
			if (!std::isfinite(step)) {
				Roles::Noted(a_movie, "CND row's place"sv, "CND stays where the card draws it"sv);
				return false;
			}

			for (auto& clip : a_row.above) {
				Flash::Set(clip, "y"sv, Value(Flash::Number(clip, "y"sv) + step));
			}
			Flash::Set(a_row.clip, "y"sv, Value(lastY));
			return true;
		}

		// Raises the CND row of a_card, the card at a_place, and writes the
		// rate, the bar and the fridge icon there. a_cardBeside says a card
		// is shown beside it: the movie then compares the 2 cards itself and
		// the cell beside the number holds its own figure, which no change
		// NEC fed may sign or write over. The label it looked for, empty
		// when it has none.
		std::string RaiseConditionRow(Scaleform::GFx::Movie& a_movie, const Place& a_place, Value& a_card, bool a_cardBeside)
		{
			const auto look = LookAt(a_movie, a_place, a_card);
			// Before the CND row is looked for, so a card with no CND row,
			// such as a holotape's, loses a bar its clips kept too.
			Roles::Card::HideOnOtherRows(a_card, look.label, CONDITION_BAR_NAME);
			const auto entry = ConditionEntry(a_card);
			if (!entry) {
				return look.label;
			}
			if (look.label.empty()) {
				Roles::Noted(a_movie, "translated CND label"sv, "no CND row is raised"sv);
				return look.label;
			}

			auto row = Roles::Card::ConditionRow(a_movie, a_card, entry->index, look.label, entry->tenths, look.redrawn, CONDITION_BAR_NAME);
			if (!row) {
				return look.label;
			}

			// The Pip-Boy's card keeps no member of NEC's own, so Pipboy keeps
			// its rates and its kind. Read at every render, so the switch needs
			// no new card for the rate.
			struct Rates
			{
				std::optional<float>           rate;
				std::optional<float>           compared;
				std::optional<Condition::Kind> kind;
			};
			Rates rates{ entry->wearRate, entry->wearRateCompared, entry->kind };
			if (a_place.movie == PIPBOY_MOVIE) {
				const auto selected = Pipboy::SelectedEntry(a_movie);
				rates = { selected.rate, selected.compared, selected.kind };
			}
			const auto showsRate = Settings::bCardWearRate.GetValue();
			const auto rate = showsRate ? rates.rate : std::nullopt;
			const auto compared = showsRate && !a_cardBeside ? rates.compared : std::nullopt;
			if (row->drawn) {
				// A row showing the bar keeps it until the card draws it anew.
				// Only a change the movie prints beside it is signed again, as
				// WriteWearRate does at every render.
				if (showsRate) {
					TurnWearRateChange(a_place.movie, row->clip, rate, compared);
				}
			} else {
				// The rate goes in before the bar and the fridge icon, which sit
				// left of it.
				if (showsRate) {
					WriteWearRate(a_movie, a_place.movie, row->clip, rate, compared, a_cardBeside);
				}
				const auto bar = Settings::iItemCardCnd.Form() == Settings::Cnd::kBar;
				if (!bar || !DrawConditionBar(a_movie, a_place.movie, row->clip, BarReading{ entry->tenths, rates.kind, entry->fridge })) {
					MarkFridge(a_movie, a_place.movie, row->clip, entry->fridge);
				}
			}
			if (Raise(a_movie, *row)) {
				TraceLog::Line("card", "{:s}% row raised above {:d} rows", Condition::CardText(entry->tenths), row->above.size());
			}
			return look.label;
		}

		// Raises the CND row of a card shown beside the card at a_place, such
		// as the copy of an earlier card a replacer pins there to compare
		// with, so CND stands at the same place on both. a_label is CND as
		// the movie prints it. Nothing else of that card changes.
		void RaiseBeside(Scaleform::GFx::Movie& a_movie, const Place& a_place, Value& a_card, const std::string& a_label)
		{
			const auto entry = a_label.empty() ? std::nullopt : ConditionEntry(a_card);
			auto       row = entry ? Roles::Card::ConditionRow(a_movie, a_card, entry->index, a_label, entry->tenths, false, CONDITION_BAR_NAME) : std::nullopt;
			if (row && Raise(a_movie, *row)) {
				TraceLog::Line("menu", "{:s} compared card's {:s}% row raised above {:d} rows", MovieName(a_place.movie),
					Condition::CardText(entry->tenths), row->above.size());
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
					auto       besides = Roles::Card::Beside(*a_params.movie, *card);
					const auto label = RaiseConditionRow(*a_params.movie, *place, *card, !besides.empty());
					for (auto& beside : besides) {
						RaiseBeside(*a_params.movie, *place, beside, label);
					}
					if (TraceLog::IsOpen()) {
						TraceCard(place->movie, *card);
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
		ForgetWearRate();

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
