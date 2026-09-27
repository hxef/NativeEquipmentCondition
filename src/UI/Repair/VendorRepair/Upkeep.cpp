#include "UI/Repair/VendorRepair/Upkeep.h"

#include "Condition/Repair.h"
#include "Core/CallPatch.h"
#include "Core/TraceLog.h"
#include "Gameplay/SpawnCondition/SpawnCondition.h"
#include "UI/Repair/VendorRepair/Stock.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

namespace VendorRepair
{
	namespace
	{
		// The 3 calls to TESDataHandler::BuildBarterContainer, which makes the
		// trader's side of the barter screen as it opens and again after a
		// trade and after an investment. Each restocks the trader's chests
		// first, where a restock is due.
		constexpr CallPatch::CallSite BUILD_SITES[] = {
			{ 2222703, 0x343, "barter screen opens" },
			{ RE::ID::BarterMenu::CompleteTrade.id(), 0x245, "barter trade" },
			{ 2222702, 0x137, "barter investment" },
		};

		// The 2 calls to TESObjectREFR::ResetInventory that restock a
		// trader's chests, both through the function table. The first, in
		// TESFaction::ResetVendorInventoryIfDue, is the merchant container,
		// and it runs only when a restock is due. The second, in
		// BuildBarterContainer, is each chest linked to the trader, and it
		// runs only when the first said a restock was due.
		constexpr CallPatch::CallSite RESTOCK_SITES[] = {
			{ 2207175, 0x06D, "merchant container restock" },
			{ RE::ID::TESDataHandler::BuildBarterContainer.id(), 0x1F5, "linked chest restock" },
		};

		// TESObjectREFR::ResetInventory, see CallPatch::PatchVirtualCall.
		constexpr std::size_t RESET_INVENTORY_SLOT = 0xB1;

		// The band a trader's stock rolls in rises with how far they repair its
		// kind, from 30% to 50% at the bottom of the scale, or for a kind they
		// do not repair, to 85% to 100% at the top. Each level's band starts
		// about 8% above the last and is 15% to 20% wide, so neighbouring
		// levels overlap. Nothing the upkeep rolls is worse than STOCK_WORST,
		// including the 1 in 100 that ignores its band.
		constexpr float STOCK_WORST = 0.30F;
		constexpr float STOCK_WIDEST = 0.20F;
		constexpr float STOCK_NARROWEST = 0.15F;

		// The band a trader who repairs an item to a_ceiling stocks it in, 0
		// for a trader who does not repair it.
		SpawnCondition::StockBand BandFor(std::uint32_t a_ceiling)
		{
			const auto climb = a_ceiling <= MIN_CEILING ?
			                       0.0F :
			                       std::min(1.0F, static_cast<float>(a_ceiling - MIN_CEILING) /
			                                          static_cast<float>(Repair::FULL - MIN_CEILING));
			const auto low = std::lerp(STOCK_WORST, 1.0F - STOCK_NARROWEST, climb);
			return { low, low + std::lerp(STOCK_WIDEST, STOCK_NARROWEST, climb), STOCK_WORST, a_ceiling };
		}

		// The trader whose side is being made on this thread, and how far they
		// repair each kind, worked out as their first chest restocks.
		struct Making
		{
			RE::Actor*              trader{ nullptr };
			std::optional<Ceilings> ceilings;
		};

		thread_local Making* t_making = nullptr;

		RE::ObjectRefHandle* BuildHk(RE::TESDataHandler* a_this, RE::ObjectRefHandle* a_out, RE::TESObjectREFR* a_vendor)
		{
			const REL::Relocation<decltype(&BuildHk)> original{ RE::ID::TESDataHandler::BuildBarterContainer };

			Making     making{ a_vendor ? a_vendor->As<RE::Actor>() : nullptr, std::nullopt };
			const auto outer = std::exchange(t_making, &making);
			const auto out = original(a_this, a_out, a_vendor);
			t_making = outer;
			return out;
		}

		// Marks what a_chest restocks with as the trader's stock. How far the
		// trader repairs is worked out once, at the first chest, before
		// anything of the restock arrives.
		void RestockHk(RE::TESObjectREFR* a_chest, bool a_leveledOnly)
		{
			auto* making = t_making;
			if (!making || !making->trader) {
				a_chest->ResetInventory(a_leveledOnly);
				return;
			}

			auto& trader = *making->trader;
			if (!making->ceilings) {
				// The chest the screen names, see Restock.h. A faction with no
				// merchant container is covered by the linked chests instead.
				const auto* faction = trader.vendorFaction;
				making->ceilings = CeilingsOf(faction ? faction->vendorData.merchantContainer : nullptr, trader,
					", worked out for a restock");
			}

			const auto ceilings = *making->ceilings;
			const SpawnCondition::Restock restock{
				a_chest->GetHandle(),
				Trader(&trader),
				[ceilings](const RE::TESBoundObject& a_item) { return BandFor(ceilings.Of(TradeOf(a_item))); },
			};

			const auto* base = a_chest->GetObjectReference();
			TraceLog::Begin("RESTOCK", "{:s} restocks chest {:08X} of {:08X}, and repairs {:s}", restock.trader,
				a_chest->formID, base ? base->formID : 0, Repairs(ceilings));

			const SpawnCondition::ScopedRestock marked{ restock };
			a_chest->ResetInventory(a_leveledOnly);
		}
	}

	void InstallUpkeep()
	{
		// Without the trader, the restocks roll like any other loot. Without
		// the restocks, the trader is noted and nothing more.
		const auto built = CallPatch::PatchAll(BUILD_SITES, RE::ID::TESDataHandler::BuildBarterContainer,
			CallPatch::Repeat<std::size(BUILD_SITES)>(reinterpret_cast<std::uintptr_t>(&BuildHk)),
			"The trader's half of the barter screen names the trader to their restock");

		std::size_t restocked = 0;
		for (const auto& site : RESTOCK_SITES) {
			if (CallPatch::PatchVirtualCall(site, RESET_INVENTORY_SLOT, reinterpret_cast<std::uintptr_t>(&RestockHk))) {
				restocked++;
			}
		}
		REX::INFO("A trader's chests restock in the shape the trader repairs to: {:d} of {:d} call sites.", restocked,
			std::size(RESTOCK_SITES));

		if (built == std::size(BUILD_SITES) && restocked == std::size(RESTOCK_SITES)) {
			std::string bands;
			for (const auto ceiling : { 0U, 30U, 40U, 50U, 60U, 70U, 80U, 90U, 100U }) {
				const auto band = BandFor(ceiling);
				bands += std::format("{:s}{:d}%:{:.0f}-{:.0f}%", bands.empty() ? "" : " ", ceiling, band.low * 100.0F,
					band.high * 100.0F);
			}
			REX::INFO("A trader's weapons and armor restock by how far they repair them, {:s}", bands);
		} else {
			REX::WARN("Some traders' stock will restock as worn as any loot.");
		}
	}
}
