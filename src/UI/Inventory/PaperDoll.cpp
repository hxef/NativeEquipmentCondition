#include "UI/Inventory/PaperDoll.h"

#include "Condition/Equipped.h"
#include "Core/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Hud/HudParts.h"
#include "UI/MenuMovies.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <string>

namespace PaperDoll
{
	namespace
	{
		using Scaleform::GFx::Value;

		// Where the Pip-Boy keeps its paper doll. On the map or the stats page
		// this finds nothing, and off the apparel tab the doll hides itself.
		constexpr const char* DOLL_PATH = "_root.Menu_mc.CurrentPage.PaperDoll_mc";

		// The 2 parts of a resistance entry, the icon and the number.
		constexpr const char* ICON_NAME = "Icon_mc";
		constexpr const char* VALUE_NAME = "Value_tf";

		// The name the bar carries over each entry, so the listener can find
		// it again.
		constexpr const char* READOUT_NAME = "NEC_Condition_mc";

		// How deep the bar is: sized for the number next to the icon, 18 high,
		// as every CND bar is sized for its text, see HudParts::Readout. Its
		// word is never written, so the bar shows alone.
		constexpr double BAR_DEEP = HudParts::Readout::BAR_DEEP * 18.0;

		// How far above the icon's top the bar's middle sits: a gap of 2, plus
		// half the bar's depth.
		constexpr double BAR_RISE = 2.0 + (BAR_DEEP / 2.0);

		// The narrowest bar worth drawing.
		constexpr double BAR_LEAST = 20.0;

		// How wide an entry is when its parts cannot be read, the icon and the
		// number as Pipboy_InvPage.swf ships them.
		constexpr double ENTRY_WIDTH = 62.0;

		// A check every 10 frames, several a second, keeps up with a piece
		// repaired, dropped or put on with the Pip-Boy open.
		constexpr std::uint32_t FRAMES_PER_READING = 10;

		// What a region shows when nothing that wears covers it, which hides
		// its bar.
		constexpr std::int32_t NONE = -1;

		// A value no region ever holds, so the first frame always draws.
		constexpr std::int32_t NOT_DRAWN = -2;

		// The 6 regions, in the order the doll's own code names them.
		enum Region : std::size_t
		{
			kHead,
			kTorso,
			kLeftArm,
			kRightArm,
			kLeftLeg,
			kRightLeg,

			kRegions
		};

		constexpr std::array<const char*, kRegions> ENTRY_NAMES{
			"Head_Resist_mc",
			"Torso_Resist_mc",
			"LArm_Resist_mc",
			"RArm_Resist_mc",
			"LLeg_Resist_mc",
			"RLeg_Resist_mc",
		};

		// A biped slot as a bit of ArmorPiece::slots, bit 0 for slot 30.
		[[nodiscard]] constexpr std::uint32_t Slot(std::uint32_t a_slot)
		{
			return 1U << (a_slot - 30);
		}

		// The slots the game's own doll counts a piece for, read from
		// PipboyInventoryData::UpdateSlotResists. The head is the helmet on 30
		// or 46, goggles on 47 and a gas mask on 48 and 49 together. The torso
		// and the limbs are 41 to 45. Underwear is anything on the underarmor
		// slots 36 to 40 and counts for the torso and all 4 limbs. The game
		// also treats a piece carrying one keyword as underwear, a keyword it
		// keeps in a global nothing visibly writes, so that one is left out.
		constexpr std::uint32_t HELMET = Slot(30) | Slot(46);
		constexpr std::uint32_t GOGGLES = Slot(47);
		constexpr std::uint32_t GAS_MASK = Slot(48) | Slot(49);
		constexpr std::uint32_t UNDERARMOR = Slot(36) | Slot(37) | Slot(38) | Slot(39) | Slot(40);
		constexpr std::array<std::uint32_t, kRegions> OWN_SLOT{ 0, Slot(41), Slot(42), Slot(43), Slot(44), Slot(45) };

		[[nodiscard]] bool IsUnderwear(const Equipped::ArmorPiece& a_piece)
		{
			return (a_piece.slots & UNDERARMOR) != 0;
		}

		[[nodiscard]] bool Covers(const Equipped::ArmorPiece& a_piece, Region a_region, bool a_underwear)
		{
			if (a_region == kHead) {
				return (a_piece.slots & (HELMET | GOGGLES)) != 0 || (a_piece.slots & GAS_MASK) == GAS_MASK;
			}
			return a_underwear || (a_piece.slots & OWN_SLOT[a_region]) != 0;
		}

		// A piece's health as a whole percent. A piece with no health counts as
		// new.
		[[nodiscard]] std::int32_t PercentOf(float a_health)
		{
			return a_health < 0.0F ? 100 : static_cast<std::int32_t>(std::lround(std::clamp(a_health, 0.0F, 1.0F) * 100.0F));
		}

		// The latest condition per region, written by the task and read by the
		// Pip-Boy's frames.
		std::array<std::atomic<std::int32_t>, kRegions> g_percent{ NONE, NONE, NONE, NONE, NONE, NONE };

		// Set while a check waits in the task queue.
		std::atomic<bool> g_queued{ false };

		// Asks for a fresh check, which arrives a frame or so later. Asking
		// again while one is on its way does nothing. The task takes the
		// inventory lock as a writer and never waits, so a busy inventory keeps
		// the last condition for a frame.
		void Queue()
		{
			if (g_queued.exchange(true)) {
				return;
			}

			const auto* tasks = F4SE::GetTaskInterface();
			if (!tasks) {
				g_queued = false;
				return;
			}

			tasks->AddTask([] {
				auto*      player = RE::PlayerCharacter::GetSingleton();
				const auto pieces = Equipped::TryArmorPieces(player);
				if (!pieces) {
					TraceLog::Once("menu", "Pip-Boy doll CND kept its last reading, another thread had the player's inventory");
				} else {
					// The lowest condition over each region, since the piece
					// nearest to breaking is the one the player wants to see.
					std::array<std::int32_t, kRegions> lowest;
					lowest.fill(NONE);
					for (const auto& piece : *pieces) {
						const auto percent = PercentOf(piece.health);
						const auto underwear = IsUnderwear(piece);
						for (std::size_t region = 0; region < kRegions; region++) {
							if (Covers(piece, static_cast<Region>(region), underwear)) {
								lowest[region] = lowest[region] == NONE ? percent : std::min(lowest[region], percent);
							}
						}
					}
					for (std::size_t region = 0; region < kRegions; region++) {
						g_percent[region] = lowest[region];
					}
				}
				g_queued = false;
			});
		}

		// A clip's edges in another clip's coordinates, through getBounds, so
		// where the icon's own drawing puts its origin does not matter. A clip
		// with nothing drawn has no edges.
		struct Edges
		{
			double left{ 0.0 };
			double top{ 0.0 };
			double right{ 0.0 };
			bool   found{ false };
		};

		[[nodiscard]] Edges EdgesIn(Value& a_clip, Value& a_space)
		{
			Value rect;
			if (!a_clip.IsDisplayObject() || !a_clip.Invoke("getBounds", &rect, &a_space, 1) || !rect.IsObject()) {
				return {};
			}
			Edges out;
			out.left = Flash::Number(rect, "x"sv);
			out.top = Flash::Number(rect, "y"sv);
			out.right = out.left + Flash::Number(rect, "width"sv);
			out.found = out.right > out.left;
			return out;
		}

		// How wide the number's digits are. It changes with the resistance and
		// with the damage type the player cycles to.
		[[nodiscard]] double DigitsWidth(Value& a_entry)
		{
			auto value = Flash::Child(a_entry, VALUE_NAME);
			return value.IsDisplayObject() ? Flash::Number(value, "textWidth"sv) : 0.0;
		}

		// Where a bar goes over an entry: from the icon's left edge to where
		// the digits end, just above the icon. The number's box is sized for
		// the longest number and ends well past the digits, which start at its
		// left margin. A readout's origin is the right end of its bar, see
		// HudParts::Readout.
		struct Placement
		{
			double x{ 0.0 };
			double y{ 0.0 };
			double width{ 0.0 };
			bool   measured{ false };
		};

		[[nodiscard]] Placement PlaceOver(Value& a_entry, double a_digits)
		{
			auto       icon = Flash::Child(a_entry, ICON_NAME);
			auto       value = Flash::Child(a_entry, VALUE_NAME);
			const auto shield = EdgesIn(icon, a_entry);
			const auto box = EdgesIn(value, a_entry);
			if (!shield.found && !box.found) {
				return Placement{ ENTRY_WIDTH, -BAR_RISE, ENTRY_WIDTH, false };
			}

			const auto left = shield.found ? shield.left : box.left;
			const auto top = shield.found ? shield.top : box.top;
			const auto right = box.found ? box.left + HudParts::FIELD_MARGIN + a_digits : shield.right;

			Placement out;
			out.x = std::max(right, left + BAR_LEAST);
			out.y = top - BAR_RISE;
			out.width = out.x - left;
			out.measured = true;
			return out;
		}

		// One bar's state, kept between frames so only a change is written.
		// The digits are the width the bar was laid out for, -1 before that.
		struct Bar
		{
			double       width{ 0.0 };
			double       digits{ -1.0 };
			std::int32_t shown{ NOT_DRAWN };
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

				if (frames++ % FRAMES_PER_READING == 0) {
					Queue();
				}

				// What changed this frame, one trace line for all 6 regions.
				std::string laid;
				std::string drawn;
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

					// The digits set where the bar ends, so a new width lays
					// it out again and fills it again.
					const auto digits = DigitsWidth(entry);
					if (digits != bar.digits) {
						const auto at = Lay(readout, entry, bar, digits);
						std::format_to(std::back_inserter(laid), "{:s}{:s} {:.0f},{:.0f} {:.0f} across{:s}", laid.empty() ? "" : ", ",
							ENTRY_NAMES[region], at.x, at.y, at.width, at.measured ? "" : " as shipped");
					}

					const auto percent = g_percent[region].load();
					if (percent == bar.shown) {
						continue;
					}
					HudParts::Readout::SetPercent(readout, bar.width, BAR_DEEP, percent);
					readout.SetMember("visible"sv, Value(percent >= 0));
					bar.shown = percent;
					std::format_to(std::back_inserter(drawn), "{:s}{:s} {:s}", drawn.empty() ? "" : ", ", ENTRY_NAMES[region],
						percent >= 0 ? std::format("{:d}%", percent) : std::string{ "hidden" });
				}

				if (!laid.empty()) {
					TraceLog::Line("menu", "Pip-Boy doll CND bars laid over {:s}", laid);
				}
				if (!drawn.empty()) {
					TraceLog::Line("menu", "Pip-Boy doll CND drawn {:s}, hidden where nothing that wears covers it", drawn);
				}
			}

			void Reset()
			{
				frames = 0;
				refused = false;
				bars.fill(Bar{});
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

			// Places a bar over its entry for the digits the number shows now.
			static Placement Lay(Value& a_readout, Value& a_entry, Bar& a_bar, double a_digits)
			{
				const auto placement = PlaceOver(a_entry, a_digits);
				a_readout.SetMember("x"sv, Value(placement.x));
				a_readout.SetMember("y"sv, Value(placement.y));
				HudParts::Readout::SetTrack(a_readout, placement.width, BAR_DEEP);

				a_bar.width = placement.width;
				a_bar.digits = a_digits;
				a_bar.shown = NOT_DRAWN;
				return placement;
			}

			std::uint32_t                frames = 0;
			bool                         refused = false;
			std::array<Bar, kRegions>    bars;
		};

		// Lives as long as the plugin, the same way as the HUD's listeners.
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
