#include "UI/Inventory/ItemCard/Cards.h"

#include "Core/TraceLog.h"
#include "UI/Flash.h"
#include "UI/MenuMovies.h"

#include <array>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <string>

namespace ItemCard
{
	namespace
	{
		using Scaleform::GFx::Value;

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

		// Moves a freshly drawn CND row up past Damage. The card redraws its
		// rows in a fixed order, plain rows from last entry to first, then the
		// combined damage and resistance rows, then the description, so the
		// entries say which child the CND row is. Every row above CND moves
		// down by the room CND took, and CND takes the highest one's place.
		void RaiseConditionRow(Value& a_card)
		{
			Value entries;
			if (!a_card.GetMember("InfoObj"sv, &entries) || !entries.IsArray()) {
				return;
			}

			std::optional<std::uint32_t> row;
			double                       percent = 0.0;
			std::uint32_t                plainRows = 0;
			for (auto i = entries.GetArraySize(); i-- > 0;) {
				Value entry;
				if (!entries.GetElement(i, &entry) || !entry.IsObject() || !IsPlainRow(entry)) {
					continue;
				}
				if (!row && HasText(entry, CND_TEXT)) {
					row = plainRows;
					percent = Flash::Number(entry, "value"sv);
				}
				plainRows++;
			}
			if (!row) {
				return;
			}

			const auto children = static_cast<std::uint32_t>(Flash::Number(a_card, "numChildren"sv));
			if (*row + 1 >= children) {
				return;
			}

			// The entries can change a moment before the card redraws. Checking
			// that the child prints the CND number keeps a stale layout from
			// being rearranged, with or without the percent sign. A replaced
			// card may draw its rows without a Value_tf.
			auto  rowClip = Flash::ChildAt(a_card, *row);
			Value valueField;
			Value valueText;
			if (!rowClip.IsDisplayObject() || !rowClip.GetMember("Value_tf"sv, &valueField) || !valueField.IsObject() ||
				!valueField.GetMember("text"sv, &valueText) || !valueText.IsString()) {
				return;
			}
			const auto number = std::format("{:.0f}", percent);
			const auto printed = std::string_view{ valueText.GetString() };
			if (printed != number && printed != number + "%") {
				return;
			}

			// The rows above CND are the rest of the plain rows and then any
			// combined row, the only ones holding an EntryHolder_mc. Every row
			// moved below has been checked to be a clip.
			auto last = *row;
			for (auto i = *row + 1; i < children; i++) {
				if (const auto clip = Flash::ChildAt(a_card, i); !clip.IsDisplayObject() || (i >= plainRows && !clip.HasMember("EntryHolder_mc"sv))) {
					break;
				}
				last = i;
			}
			if (last == *row) {
				return;
			}

			// y grows downwards, so a CND row already above the highest row
			// means an earlier render event did this.
			const auto rowY = Flash::Number(rowClip, "y"sv);
			const auto lastY = Flash::Number(Flash::ChildAt(a_card, last), "y"sv);
			if (rowY <= lastY) {
				return;
			}

			// Neighbouring rows overlap a little. Reading it from the rows
			// keeps this in sync with the card's spacing.
			const auto next = Flash::ChildAt(a_card, *row + 1);
			const auto overlap = Flash::Number(next, "y"sv) + Flash::Number(next, "height"sv) - rowY;
			const auto step = Flash::Number(rowClip, "height"sv) - overlap;

			for (auto i = *row + 1; i <= last; i++) {
				auto clip = Flash::ChildAt(a_card, i);
				clip.SetMember("y"sv, Value(Flash::Number(clip, "y"sv) + step));
			}
			rowClip.SetMember("y"sv, Value(lastY));

			TraceLog::Line("card", "{:.0f}% row raised above {:d} rows", percent, last - *row);
		}

		// Where each menu's movie keeps its card, read off the SWFs.
		struct Card
		{
			std::string_view movie;
			const char*      path;
		};

		// A number as short as it will go, 48 or 7.2.
		std::string Short(double a_number)
		{
			return std::format("{:g}", a_number);
		}

		// One line for the card on the screen: each entry's text and value, the
		// difference to the equipped item, and a damage entry's type. Written
		// through First, since the listener runs every frame.
		void TraceCard(const Card& a_card, const Value& a_clip)
		{
			Value entries;
			if (!a_clip.GetMember("InfoObj"sv, &entries) || !entries.IsArray()) {
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
				TraceLog::First("menu", "{:s} card shows {:s}", a_card.movie, text);
			}
		}

		constexpr Card CARDS[] = {
			{ "PipboyMenu.swf"sv, "_root.Menu_mc.CurrentPage.ItemCard_mc" },
			{ "ContainerMenu.swf"sv, "_root.FilterHolder_mc.Menu_mc.ItemCard_mc" },
			{ "BarterMenu.swf"sv, "_root.FilterHolder_mc.Menu_mc.ItemCard_mc" },
			{ "ExamineMenu.swf"sv, "_root.BaseInstance.ItemCardList_mc" },
		};

		const Card* FindCard(std::string_view a_file)
		{
			for (const auto& card : CARDS) {
				if (MenuMovies::IsMovie(a_file, card.movie)) {
					return &card;
				}
			}
			return nullptr;
		}

		// The native function the render event calls. The user data is the
		// menu's entry in CARDS. The card is looked up on every call, since the
		// Pip-Boy loads its inventory page some time after the menu opens and a
		// menu's clips must not be held past the menu.
		class RenderListener final : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				const auto* card = static_cast<const Card*>(a_params.userData);
				Value       clip;
				if (card && a_params.movie && a_params.movie->GetVariable(&clip, card->path) && clip.IsDisplayObject()) {
					RaiseConditionRow(clip);
					if (TraceLog::IsOpen()) {
						TraceCard(*card, clip);
					}
				}
			}
		};

		// One listener serves every menu. Scaleform counts references, and this
		// one starts at one that is never given back, so it lives as long as
		// the plugin.
		RenderListener g_renderListener;
	}

	void WatchCard(Scaleform::GFx::Movie& a_movie, std::string_view a_file)
	{
		const auto* card = FindCard(a_file);
		if (!card) {
			return;
		}

		Value stage;
		if (!a_movie.GetVariable(&stage, "_root.stage") || !stage.IsObject()) {
			REX::WARN("{:s} has no stage to listen on, so its CND row stays under Damage.", card->movie);
			return;
		}

		Value listener;
		a_movie.CreateFunction(&listener, &g_renderListener, const_cast<void*>(static_cast<const void*>(card)));

		// The card redraws from its own render listener at default priority.
		// The lowest priority runs after it.
		const std::array<Value, 4> args{
			Value("render"),
			listener,
			Value(false),
			Value(std::numeric_limits<std::int32_t>::min()),
		};
		if (!stage.Invoke("addEventListener", args)) {
			REX::WARN("{:s} refused the render listener, so its CND row stays under Damage.", card->movie);
			return;
		}

		TraceLog::Line("menu", "{:s} loaded, its CND row is kept above Damage", card->movie);
	}
}
