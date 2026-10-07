#include "UI/Inventory/Pipboy.h"

#include "UI/Inventory/ItemCard/ItemCard.h"
#include "UI/MenuMovies.h"
#include "Core/TraceLog.h"

#include <array>
#include <cstdint>
#include <limits>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>

namespace Pipboy
{
	namespace
	{
		using Scaleform::GFx::Value;

		// Where the Pip-Boy keeps its inventory list. On the map or the stats
		// page this finds nothing.
		constexpr const char* LIST_PATH = "_root.Menu_mc.CurrentPage.List_mc";

		// What a worn out name is written in, and what an ordinary one is. The
		// movie is drawn in white and black and tinted with the player's
		// colour, so half white comes out as half strength Pip-Boy green. The
		// list itself only uses alpha 0 and 1.
		constexpr std::uint32_t BROKEN_GREY = 0x7F7F7F;
		constexpr std::uint32_t PLAIN_WHITE = 0xFFFFFF;

		// A stop for the walk along the rows. No Pip-Boy shows anything like
		// this many.
		constexpr std::uint32_t MAX_ROWS = 64;

		// No row at all, where the list puts the clips it is not using.
		constexpr std::uint32_t NO_ROW = std::numeric_limits<std::uint32_t>::max();

		// How many worn out items are worth remembering. The Pip-Boy numbers
		// every value it makes and never reuses a number, so each rebuild
		// leaves the last one's numbers behind. Past this the whole list is
		// cleared and the next rebuild fills it again.
		constexpr std::size_t MAX_REMEMBERED = 4096;

		// The numbers of the items that have worn out. Written on the Pip-Boy's
		// rebuild thread and read on the menu's, so both go through the lock.
		std::mutex                        g_wornLock;
		std::unordered_set<std::uint32_t> g_worn;

		[[nodiscard]] bool IsWorn(std::uint32_t a_node)
		{
			const std::scoped_lock lock{ g_wornLock };
			return g_worn.contains(a_node);
		}

		// A member as a whole number, whichever number type it is kept in, or
		// a_absent.
		[[nodiscard]] std::uint32_t Number(const Value& a_object, std::string_view a_name, std::uint32_t a_absent)
		{
			Value member;
			if (!a_object.GetMember(a_name, &member)) {
				return a_absent;
			}
			if (member.IsUInt()) {
				return member.GetUInt();
			}
			if (member.IsInt()) {
				return static_cast<std::uint32_t>(member.GetInt());
			}
			if (member.IsNumber()) {
				return static_cast<std::uint32_t>(member.GetNumber());
			}
			return a_absent;
		}

		// Which row a clip is showing. The list reuses its clips for other rows
		// as it scrolls, so this is asked every time.
		[[nodiscard]] std::uint32_t RowOf(const Value& a_clip)
		{
			return Number(a_clip, "itemIndex"sv, NO_ROW);
		}

		// Whether the item on a row has worn out. nodeID is the number the game
		// gave the entry, the only thing that reaches the menu without changing
		// the game to send more.
		[[nodiscard]] bool RowIsBroken(const Value& a_entries, std::uint32_t a_row)
		{
			Value entry;
			if (a_row >= a_entries.GetArraySize() || !a_entries.GetElement(a_row, &entry) || !entry.IsObject()) {
				return false;
			}

			const auto node = Number(entry, "nodeID"sv, 0);
			return node != 0 && IsWorn(node);
		}

		// Whether the list draws this row as selected. A selected row is black
		// on a lit bar and stands out on its own, so it is left alone.
		[[nodiscard]] bool RowIsSelected(const Value& a_row)
		{
			Value selected;
			return a_row.GetMember("selected"sv, &selected) &&
			       selected.IsBoolean() && selected.GetBoolean();
		}

		// Whatever text a field is showing, for the trace log to name it by.
		[[nodiscard]] std::string TextOf(const Value& a_field)
		{
			Value text;
			return a_field.GetMember("text"sv, &text) && text.IsString() ? text.GetString() : "(none)";
		}

		// Called by the Pip-Boy every frame. The list is walked, not watched,
		// since it is a plain clip that sends no event when it changes, and it
		// changes for scrolling, sorting, tabs, dropping an item and one
		// breaking with the Pip-Boy open. A few reads a frame.
		class FrameListener final : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				frames++;

				Value list;
				if (!a_params.movie || !a_params.movie->GetVariable(&list, LIST_PATH) ||
					!list.IsDisplayObject()) {
					return;
				}

				Value entries;
				if (!list.GetMember("entryList"sv, &entries) || !entries.IsArray()) {
					Survey("the inventory list holds no items at all");
					return;
				}

				const auto    cards = ItemCard::PipboyCards();
				std::uint32_t clips = 0;
				for (; clips < MAX_ROWS; clips++) {
					Value      row;
					const auto index = Value(static_cast<std::int32_t>(clips));
					if (!list.Invoke("GetClipByIndex", &row, &index, 1) || !row.IsDisplayObject()) {
						break;
					}

					Value name;
					if (!row.GetMember("textField"sv, &name) || !name.IsDisplayObject()) {
						continue;
					}

					const auto faded = cards && RowIsBroken(entries, RowOf(row)) && !RowIsSelected(row);
					const auto written = Number(name, "textColor"sv, PLAIN_WHITE);

					// Only a name that should be faded and is not, or the
					// reverse, is written, since writing the same value back
					// marks the row for redrawing.
					if (faded != (written == BROKEN_GREY)) {
						name.SetMember("textColor"sv, Value(faded ? BROKEN_GREY : PLAIN_WHITE));

						// Said once, and only when something was written, since
						// it shows that every step works.
						if (faded && !saidFaded) {
							saidFaded = true;
							TraceLog::Line("menu", "Pip-Boy faded the name of {:s}, {:#08x} became {:#08x}",
								TextOf(name), written, Number(name, "textColor"sv, PLAIN_WHITE));
						}
					}
				}

				SurveyList(entries, clips);
			}

			void Reset()
			{
				frames = 0;
				saidFaded = false;
				surveyed = false;
			}

		private:
			// Said once per Pip-Boy, the first time its list is found. Every
			// step depends on the game's own movie, so when names do not fade
			// this shows what was found.
			void SurveyList(const Value& a_entries, std::uint32_t a_clips)
			{
				if (surveyed) {
					return;
				}
				surveyed = true;

				const auto items = a_entries.GetArraySize();

				std::size_t remembered = 0;
				{
					const std::scoped_lock lock{ g_wornLock };
					remembered = g_worn.size();
				}

				std::uint32_t matched = 0;
				for (std::uint32_t item = 0; item < items; item++) {
					Value      entry;
					const auto node = a_entries.GetElement(item, &entry) && entry.IsObject() ?
					                      Number(entry, "nodeID"sv, 0) :
					                      0;
					if (node != 0 && IsWorn(node)) {
						matched++;
					}
				}

				TraceLog::Line("menu", "Pip-Boy list found after {:d} frames, {:d} rows on screen, {:d} items, {:d} worn out items remembered, {:d} of them on show",
					frames, a_clips, items, remembered, matched);
			}

			void Survey(std::string_view a_what)
			{
				if (!surveyed) {
					surveyed = true;
					TraceLog::Line("menu", "Pip-Boy list found after {:d} frames, {:s}", frames, a_what);
				}
			}

			std::uint32_t frames = 0;
			bool          saidFaded = false;
			bool          surveyed = false;
		};

		// Lives as long as the plugin, the same way as the HUD's listeners.
		FrameListener g_frameListener;
	}

	void MarkBroken(const RE::PipboyObject& a_entry, bool a_broken)
	{
		const std::scoped_lock lock{ g_wornLock };

		if (!a_broken) {
			g_worn.erase(a_entry.m_id);
			return;
		}

		// Numbers from earlier rebuilds are never asked about again, so past a
		// generous limit the whole list is cleared and the next rebuild fills
		// it in.
		if (g_worn.size() >= MAX_REMEMBERED) {
			g_worn.clear();
		}
		g_worn.insert(a_entry.m_id);
	}

	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file)
	{
		if (!MenuMovies::IsMovie(a_file, "PipboyMenu.swf"sv)) {
			return;
		}

		Value stage;
		if (!a_movie.GetVariable(&stage, "_root.stage") || !stage.IsObject()) {
			REX::WARN("The Pip-Boy has no stage to listen on, so broken items read like any other.");
			return;
		}

		// A new Pip-Boy has logged nothing yet.
		g_frameListener.Reset();

		Value listener;
		a_movie.CreateFunction(&listener, &g_frameListener);
		if (!stage.Invoke("addEventListener", std::array{ Value("enterFrame"), listener })) {
			REX::WARN("The Pip-Boy refused the frame listener, so broken items read like any other.");
			return;
		}

		TraceLog::Line("menu", "PipboyMenu.swf loaded, a worn out item's name is faded in the list");
	}
}
