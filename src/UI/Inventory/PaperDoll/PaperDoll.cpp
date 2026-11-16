#include "UI/Inventory/PaperDoll/PaperDoll.h"

#include "Condition/Condition.h"
#include "Core/Settings/Settings.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Hud/HudParts/HudParts.h"
#include "UI/Inventory/PaperDoll/Regions.h"
#include "UI/Inventory/PaperDoll/Room.h"
#include "UI/MenuMovies.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace PaperDoll
{
	namespace
	{
		using Scaleform::GFx::Value;

		// Where the Pip-Boy keeps its paper doll. On the map or the stats page
		// this finds nothing, and off the apparel tab the doll hides itself.
		constexpr const char* DOLL_PATH = "_root.Menu_mc.CurrentPage.PaperDoll_mc";

		// Where the Pip-Boy keeps its page and tab names, which the bars stay
		// under, see Room.h.
		constexpr const char* HEADER_PATH = "_root.Menu_mc.Header_mc";

		// The 2 parts of a resistance entry, the icon and the number.
		constexpr const char* ICON_NAME = "Icon_mc";
		constexpr const char* VALUE_NAME = "Value_tf";

		// The name the bar carries over each entry, so the listener can find
		// it again.
		constexpr const char* READOUT_NAME = "NEC_Condition_mc";

		// The most Pip-Boy CND bar thickness counts for, as the MCM page's
		// slider allows. At 3 the bars turn into square blocks, and at 2 each
		// leg bar stays at least 13.8 clear of the figure's hands, which no
		// named clip covers.
		constexpr double PIPBOY_MOST = 2.0;

		// How wide the penalty mark is at Penalty mark width 1. Wider than on
		// the HUD, since the Pip-Boy's blur eats a thinner one.
		constexpr double DOLL_MARK = 6.0;

		// A check every 10 frames, several a second, keeps up with a piece
		// repaired, dropped or put on with the Pip-Boy open. The same frame
		// checks the room above each bar, see Stale.
		constexpr std::uint32_t FRAMES_PER_READING = 10;

		// A value no region ever holds, so the first frame always draws.
		constexpr std::int32_t NOT_DRAWN = -2;

		// The entry over each region, in Region's order.
		constexpr std::array<const char*, kRegions> ENTRY_NAMES{
			"Head_Resist_mc",
			"Torso_Resist_mc",
			"LArm_Resist_mc",
			"RArm_Resist_mc",
			"LLeg_Resist_mc",
			"RLeg_Resist_mc",
		};

		// How wide the number's digits are. It changes with the resistance and
		// with the damage type the player cycles to.
		[[nodiscard]] double DigitsWidth(Value& a_entry)
		{
			auto value = Flash::Child(a_entry, VALUE_NAME);
			return value.IsDisplayObject() ? Flash::Number(value, "textWidth"sv) : 0.0;
		}

		// One bar's state, kept between frames so only a change is written.
		// The digits and the thickness are what the bar was laid out for, -1
		// before that, the room what it found above it then and held what
		// held it thinner. tight is the room under held when the bar covers
		// it, see Hold, and tightSaid what the trace last named of it.
		// shownMark and shownStill are the mark and the still end last
		// drawn, forgotten with shown.
		struct Bar
		{
			Placement                placed;
			double                   digits{ -1.0 };
			double                   thickness{ -1.0 };
			Room                     room;
			std::optional<Limit>     held;
			std::optional<double>    tight;
			std::optional<double>    tightSaid;
			std::int32_t             shown{ NOT_DRAWN };
			HudParts::Readout::Mark  shownMark;
			HudParts::Readout::Still shownStill{ HudParts::Readout::Still::kLeftEnd };
		};

		// Called by the Pip-Boy every frame. The doll is looked up every time,
		// since the inventory page is a movie of its own that loads and unloads
		// with the page, and a bar is built the first frame its entry is seen
		// without one. The doll jumps to a timeline frame per underwear type on
		// every redraw, which can give it new entries, so an entry can be seen
		// without a bar again.
		class FrameListener final : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				Value doll;
				if (!a_params.movie || !a_params.movie->GetVariable(&doll, DOLL_PATH) || !doll.IsDisplayObject()) {
					return;
				}

				// Off the apparel tab the doll hides itself, and the bars are
				// its descendants, so nothing is drawn and nothing is read.
				if (!Flash::Bool(doll, "visible"sv)) {
					return;
				}

				const auto pass = frames++ % FRAMES_PER_READING == 0;
				if (pass) {
					Queue();
				}

				// All 3 are read every frame, as the HUD bars read theirs. The
				// mark keeps its width at every thickness.
				const auto thickness = HudParts::Readout::Held(Settings::fPipboyBarThickness, HudParts::Readout::LEAST_SIZE, PIPBOY_MOST);
				const auto mark = HudParts::Readout::PenaltyMark(Condition::Kind::kArmor, BAR_DEEP, DOLL_MARK);
				const auto flipped = Settings::bCndBarsFlipped.GetValue();
				const auto still = HudParts::Readout::StillEnd(HudParts::Readout::Still::kLeftEnd, flipped);

				// The header's texts, read on the first bar that needs them.
				std::optional<std::vector<Value>> texts;
				const auto headerTexts = [&]() -> std::vector<Value>& {
					if (!texts) {
						Value header;
						texts = a_params.movie->GetVariable(&header, HEADER_PATH) ? HeaderTexts(header) : std::vector<Value>{};
					}
					return *texts;
				};

				// What changed this frame, one trace line for all 6 regions.
				std::string laid;
				std::string drawn;
				auto        redrawn = false;
				for (std::size_t region = 0; region < kRegions; region++) {
					auto entry = Flash::Child(doll, ENTRY_NAMES[region]);
					if (!entry.IsDisplayObject()) {
						continue;
					}

					auto  readout = Flash::Child(entry, READOUT_NAME);
					auto& bar = bars[region];
					if (!readout.IsDisplayObject()) {
						readout = Build(*a_params.movie, entry);
						if (!readout.IsDisplayObject()) {
							return;
						}
						bar = Bar{};
					}

					// The digits set where the bar ends and the thickness how
					// deep it is, so a change of either lays it out again and
					// fills it again, as a change above it may.
					const auto digits = DigitsWidth(entry);
					if (digits != bar.digits || thickness != bar.thickness || (pass && Stale(*a_params.movie, headerTexts(), entry, bar))) {
						if (Lay(*a_params.movie, readout, doll, headerTexts(), entry, bar, digits, thickness)) {
							const auto& at = bar.placed;
							std::format_to(std::back_inserter(laid), "{:s}{:s} {:.0f},{:.0f} {:.0f} across{:s}", laid.empty() ? "" : ", ",
								ENTRY_NAMES[region], at.x, at.y, at.width, at.measured ? "" : " as shipped");
						}
					}

					const auto percent = LowestPercent(region);
					if (percent == bar.shown && mark == bar.shownMark && still == bar.shownStill) {
						continue;
					}
					HudParts::Readout::SetPercent(readout, bar.placed.width, bar.placed.deep, percent, still, mark);
					readout.SetMember("visible"sv, Value(percent >= 0));
					if (percent != bar.shown) {
						std::format_to(std::back_inserter(drawn), "{:s}{:s} {:s}", drawn.empty() ? "" : ", ", ENTRY_NAMES[region],
							percent >= 0 ? std::format("{:d}%", percent) : std::string{ "hidden" });
					}
					bar.shown = percent;
					bar.shownMark = mark;
					bar.shownStill = still;
					redrawn = true;
				}

				SayLayouts(laid, thickness);
				if (!drawn.empty()) {
					TraceLog::Line("menu", "Pip-Boy doll CND drawn {:s}, hidden where nothing that wears covers it", drawn);
				}
				if (redrawn) {
					HudParts::Readout::TellFlipped(flipped, saidFlipped, "Pip-Boy doll CND bars"sv);
				}
				if (redrawn && mark != saidMark) {
					saidMark = mark;
					if (mark.at != 0) {
						TraceLog::Line("menu", "Pip-Boy doll CND bars mark armor penalties from {:d}% {:s} {:.2f} wide", mark.at, mark.line ? "with a line" : "with notches", mark.wide);
					} else {
						TraceLog::Line("menu", "Pip-Boy doll CND bars mark no penalty start");
					}
				}
			}

			void Reset()
			{
				frames = 0;
				refused = false;
				bars.fill(Bar{});
				saidMark = HudParts::Readout::Mark{};
				saidFlipped = false;
				noClipsSaid = false;
				noBoxSaid = false;
			}

		private:
			// Builds one bar and hangs it on an entry, for Lay to place. A
			// movie that refuses any part of it gets no bar and no second try.
			Value Build(Scaleform::GFx::Movie& a_movie, Value& a_entry)
			{
				if (refused) {
					return Value{};
				}

				auto readout = HudParts::Readout::Create(a_movie, READOUT_NAME);
				if (!readout.IsDisplayObject() || !a_entry.Invoke("addChild", std::array{ readout })) {
					refused = true;
					REX::WARN("The Pip-Boy's paper doll would not take a CND bar, so it shows none.");
					return Value{};
				}
				return readout;
			}

			// Whether the room above a laid out bar may have changed: a named
			// clip gave no box at its layout, or the header's letters over it
			// came, went or moved, as a header built after the figure does.
			// Asked once every FRAMES_PER_READING frames.
			static bool Stale(Scaleform::GFx::Movie& a_movie, std::vector<Value>& a_texts, Value& a_entry, const Bar& a_bar)
			{
				const auto& at = a_bar.placed;
				return !a_bar.room.noBox.empty() || HeaderAbove(a_movie, a_texts, a_entry, at.y + (at.deep / 2.0), at.x - at.width, at.x) != a_bar.room.header;
			}

			// Places a bar over its entry for the digits the number shows now,
			// as deep as a_thickness asks and the room above it lets it be. Its
			// bottom stays put and it grows upward. False when the layout comes
			// out as the last one, which then changes nothing.
			static bool Lay(Scaleform::GFx::Movie& a_movie, Value& a_readout, Value& a_doll, std::vector<Value>& a_texts, Value& a_entry, Bar& a_bar, double a_digits, double a_thickness)
			{
				const auto asked = BAR_DEEP * a_thickness;
				auto       icon = Flash::Child(a_entry, ICON_NAME);
				auto       value = Flash::Child(a_entry, VALUE_NAME);
				auto       placement = PlaceOver(a_entry, icon, value, a_digits, asked);
				const auto bottom = placement.y + (asked / 2.0);
				a_bar.room = RoomAbove(a_movie, a_doll, a_texts, a_entry, bottom, placement.x - placement.width, placement.x);
				a_bar.digits = a_digits;
				a_bar.thickness = a_thickness;

				const auto hold = HoldIn(a_bar.room, bottom, asked);
				a_bar.held = hold.held;
				a_bar.tight = hold.tight;
				placement.deep = hold.deep;
				placement.y = bottom - (hold.deep / 2.0);
				if (placement == a_bar.placed) {
					return false;
				}
				a_readout.SetMember("x"sv, Value(placement.x));
				a_readout.SetMember("y"sv, Value(placement.y));
				HudParts::Readout::SetTrack(a_readout, placement.width, placement.deep);

				a_bar.placed = placement;
				a_bar.shown = NOT_DRAWN;
				a_bar.shownMark = HudParts::Readout::Mark{};
				a_bar.shownStill = HudParts::Readout::Still::kLeftEnd;
				return true;
			}

			// The frame's layout lines. A frame that moved a bar names every
			// held bar, each limit in its entry's units, so a held bar's top is
			// 2 under the number its line ends on. A bar that covers its limit
			// is named once each time its room changes, never at every layout.
			// The lines on unread clips are written once each, the second again
			// only after every bar's last layout has read every box.
			void SayLayouts(const std::string& a_laid, double a_thickness)
			{
				if (!a_laid.empty()) {
					TraceLog::Line("menu", "Pip-Boy doll CND bars laid over {:s}", a_laid);
					TraceLog::Line("menu", "Pip-Boy doll CND bars {:.1f} deep at thickness {:g}", BAR_DEEP * a_thickness, a_thickness);
					for (std::size_t region = 0; region < kRegions; region++) {
						if (const auto& held = bars[region].held) {
							TraceLog::Line("menu", "Pip-Boy doll CND bar over {:s} held to {:.1f} deep under {:s}{:s}, whose {:s} at {:.1f}", ENTRY_NAMES[region], bars[region].placed.deep,
								held->letters ? "the header's " : "", held->under, held->letters ? "letters end" : "box ends", held->at);
						}
					}
				}
				for (std::size_t region = 0; region < kRegions; region++) {
					auto& bar = bars[region];
					if (bar.tight == bar.tightSaid) {
						continue;
					}
					bar.tightSaid = bar.tight;
					if (const auto& held = bar.held; held && bar.tight) {
						TraceLog::Line("menu", "Pip-Boy doll CND bar over {:s} has only {:.1f} of room under {:s}{:s}, so its least depth of {:.1f} overlaps it", ENTRY_NAMES[region], *bar.tight,
							held->letters ? "the header's " : "", held->under, bar.placed.deep);
					}
				}
				if (!noClipsSaid && std::ranges::any_of(bars, [](const Bar& a_bar) { return a_bar.room.noClips; })) {
					noClipsSaid = true;
					TraceLog::Line("menu", "Pip-Boy doll CND bars find no figure clips, so they grow to {:.1f} deep at most", NO_CLIPS_DEEP);
				}
				const auto unread = std::ranges::find_if(bars, [](const Bar& a_bar) { return !a_bar.room.noBox.empty(); });
				if (unread == bars.end()) {
					noBoxSaid = false;
				} else if (!noBoxSaid) {
					noBoxSaid = true;
					TraceLog::Line("menu", "Pip-Boy doll CND bars read no box for {:s}, so they grow to {:.1f} deep at most for now", unread->room.noBox, NO_CLIPS_DEEP);
				}
			}

			std::uint32_t                frames = 0;
			bool                         refused = false;
			std::array<Bar, kRegions>    bars;

			// The mark the trace last named, forgotten only in Reset, so a
			// relayout redraws it without a second line, and the same for the
			// flipped line.
			HudParts::Readout::Mark saidMark;
			bool                    saidFlipped = false;

			// Whether the trace named the clips it could not read, see
			// SayLayouts.
			bool noClipsSaid = false;
			bool noBoxSaid = false;
		};

		FrameListener g_frameListener;
	}

	void OnMovieLoaded(Scaleform::GFx::Movie& a_movie, std::string_view a_file)
	{
		if (!MenuMovies::IsMovie(a_file, "PipboyMenu.swf"sv)) {
			return;
		}

		Value stage;
		if (!a_movie.GetVariable(&stage, "_root.stage") || !stage.IsObject()) {
			REX::WARN("The Pip-Boy has no stage to listen on, so its paper doll shows no CND.");
			return;
		}

		// A new Pip-Boy starts with nothing drawn.
		g_frameListener.Reset();

		Value listener;
		a_movie.CreateFunction(&listener, &g_frameListener);
		if (!stage.Invoke("addEventListener", std::array{ Value("enterFrame"), listener })) {
			REX::WARN("The Pip-Boy refused the frame listener, so its paper doll shows no CND.");
			return;
		}

		Queue();
		TraceLog::Line("menu", "PipboyMenu.swf loaded, the paper doll gets a CND bar per body region");
	}
}
