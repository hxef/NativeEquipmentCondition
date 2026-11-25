#include "UI/Repair/WearInfo.h"

#include "Condition/ArmorWear/ArmorWear.h"
#include "Condition/Condition.h"
#include "Condition/WeaponWear/WeaponWear.h"
#include "Core/TraceLog/TraceLog.h"

#include <format>
#include <string>
#include <vector>

namespace WearInfo
{
	namespace
	{
		// A rate as the item card rounds it, see WearRate.cpp.
		[[nodiscard]] std::string Times(float a_rate)
		{
			if (a_rate < 0.05F) {
				return std::format("x{:.2f}", a_rate);
			}
			if (a_rate < 99.95F) {
				return std::format("x{:.1f}", a_rate);
			}
			return std::format("x{:.0f}", a_rate);
		}

		// The weapon's line. automatic is the fire rate against the ordinary
		// automatic times fAutoWearRateMult, 1 for a weapon that is not
		// automatic.
		[[nodiscard]] std::string WeaponLine(const WeaponWear::Share& a_share)
		{
			return std::format("Wears {:s}: damage {:.0f} against {:.0f} ({:s}), build x{:.2f}, automatic x{:.2f}, suppressor x{:.2f}, wear list x{:.2f}",
				Times(a_share.value), a_share.damage, a_share.reference, Times(a_share.damage / a_share.reference), a_share.build,
				a_share.pace * a_share.autoMult, a_share.suppressor, a_share.line);
		}

		// A piece's line. Health is 1 against the piece's Health share, since
		// more Health makes it last longer.
		[[nodiscard]] std::string ArmorLine(const RE::TESObjectARMO& a_armor, const ArmorWear::Share& a_share)
		{
			return std::format("{:s} wears {:s}: build x{:.2f}, Health x{:.2f}, wear list x{:.2f}", RE::TESFullName::GetFullName(a_armor),
				Times(a_share.value), a_share.build, 1.0F / a_share.lasts, a_share.line);
		}

		// The lines for what the player has equipped, read under the
		// inventory read lock, as Equipped's walks read it. A player in power
		// armor has no piece that wears.
		[[nodiscard]] std::vector<std::string> Lines(RE::PlayerCharacter& a_player, bool a_armor)
		{
			std::vector<std::string> out;
			auto*                    inv = a_player.inventoryList;
			if (!inv || (a_armor && RE::PowerArmor::ActorInPowerArmor(a_player))) {
				return out;
			}

			const RE::BSAutoReadLock l(inv->rwLock);
			inv->ForEachStack(
				[a_armor](RE::BGSInventoryItem& a_item) {
					return a_item.object && (a_armor ? a_item.object->Is(RE::ENUM_FORM_ID::kARMO) : a_item.object->IsWeapon()) &&
					       Condition::WearsOut(*a_item.object);
				},
				[&out, a_armor](RE::BGSInventoryItem& a_item, RE::BGSInventoryItem::Stack& a_stack) {
					if (!a_stack.IsEquipped()) {
						return true;  // Continue searching...
					}
					// The filter has already made sure of the kind.
					const auto* extra = a_stack.extra.get();
					if (a_armor) {
						const auto& armor = static_cast<const RE::TESObjectARMO&>(*a_item.object);
						out.push_back(ArmorLine(armor, ArmorWear::WearFactors(armor, extra)));
						return true;
					}
					out.push_back(WeaponLine(WeaponWear::WearFactors(static_cast<const RE::TESObjectWEAP&>(*a_item.object), extra)));
					return false;  // Stop iteration, the first one is the weapon in hand, as Equipped::Weapon picks it.
				});
			return out;
		}
	}

	void Print(RE::PlayerCharacter& a_player, RE::ConsoleLog* a_console, bool a_armor)
	{
		auto lines = Lines(a_player, a_armor);
		if (lines.empty()) {
			lines.emplace_back(a_armor ? "No armor worn that wears out." : "No weapon in hand that wears out.");
		}
		for (const auto& line : lines) {
			TraceLog::Line("wear", "srm wearinfo: {:s}", line);
			if (a_console) {
				a_console->Log("{:s}", line);
			}
		}
	}
}
