#include "UI/Inventory/PaperDoll/Regions.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Core/Settings/Settings.h"
#include "Core/TraceLog/TraceLog.h"

#include <algorithm>
#include <array>
#include <atomic>

namespace PaperDoll
{
	namespace
	{
		// A biped slot as a bit of ArmorPiece::slots, bit 0 for slot 30.
		[[nodiscard]] constexpr std::uint32_t Slot(std::uint32_t a_slot)
		{
			return 1U << (a_slot - 30);
		}

		// The slots the vanilla doll counts a piece for, read from
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
			return a_health < 0.0F ? 100 : static_cast<std::int32_t>(Condition::WholePercent(a_health));
		}

		// The latest condition per region, written by the task and read by the
		// Pip-Boy's frames.
		std::array<std::atomic<std::int32_t>, kRegions> g_percent{ NONE, NONE, NONE, NONE, NONE, NONE };

		// Set while a check waits in the task queue.
		std::atomic<bool> g_queued{ false };
	}

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

	std::int32_t LowestPercent(std::size_t a_region)
	{
		return a_region < kRegions && Settings::NecOn() ? g_percent[a_region].load() : NONE;
	}
}
