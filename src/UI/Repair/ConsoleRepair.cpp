#include "UI/Repair/ConsoleRepair.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Condition/Repair.h"
#include "Core/ItemCards.h"
#include "Core/TraceLog.h"
#include "UI/Repair/Restore.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <system_error>
#include <vector>

namespace ConsoleRepair
{
	namespace
	{
		// The word that turns the command on the armor the player wears.
		constexpr std::string_view ARMOR = "armor"sv;

		// Room for a word as the console hands it over. A line of console
		// input is shorter than this.
		constexpr std::size_t WORD = 512;

		// 2 optional words, a percent and the word armor, in either order. Both
		// are words rather than a number and a word so that "srm armor" needs
		// no number in front. The command table keeps a pointer to this for as
		// long as the game runs.
		RE::SCRIPT_PARAMETER PARAMETERS[]{
			{ "Percent or armor", RE::SCRIPT_PARAM_TYPE::kChar, true },
			{ "Percent or armor", RE::SCRIPT_PARAM_TYPE::kChar, true },
		};

		// What was typed: the level, full where no number was, and whether the
		// word armor was among it.
		struct Asked
		{
			std::uint32_t level{ Repair::FULL };
			bool          armor{ false };
		};

		[[nodiscard]] bool SaysArmor(std::string_view a_word)
		{
			return std::ranges::equal(a_word, ARMOR, [](unsigned char a_typed, unsigned char a_known) {
				return std::tolower(a_typed) == a_known;
			});
		}

		// Nothing for a word that is neither a number nor armor, or for a
		// second number.
		[[nodiscard]] std::optional<Asked> Parse(std::string_view a_first, std::string_view a_second)
		{
			Asked out;
			bool  numbered = false;
			for (const auto word : { a_first, a_second }) {
				if (word.empty()) {
					continue;
				}
				if (SaysArmor(word)) {
					out.armor = true;
					continue;
				}

				std::int32_t percent = 0;
				const auto*  end = word.data() + word.size();
				const auto   read = std::from_chars(word.data(), end, percent);
				if (numbered || read.ec != std::errc{} || read.ptr != end) {
					return std::nullopt;
				}
				numbered = true;

				// 0 is allowed and leaves a broken item on the player. Play
				// does that to a piece of armor, which stays on at 0, and
				// never to a weapon, since the shot that breaks one puts it
				// away.
				out.level = static_cast<std::uint32_t>(std::clamp(percent, 0, static_cast<std::int32_t>(Repair::FULL)));
			}
			return out;
		}

		// One item the command sets, with the health it had.
		struct Target
		{
			RE::TESBoundObject* object{ nullptr };
			float               before{ Condition::INVALID_HEALTH };
		};

		// The weapon in hand, or every piece of armor worn that takes part.
		[[nodiscard]] std::vector<Target> Targets(RE::PlayerCharacter* a_player, bool a_armor)
		{
			std::vector<Target> out;
			if (a_armor) {
				for (const auto& piece : Equipped::ArmorPieces(a_player)) {
					if (piece.armor) {
						out.push_back({ piece.armor, piece.health });
					}
				}
			} else if (auto* weapon = Equipped::Weapon(a_player)) {
				out.push_back({ weapon, Equipped::WeaponHealth(a_player, weapon) });
			}
			return out;
		}

		bool Execute(const RE::SCRIPT_PARAMETER* a_parameters, const char* a_compiledParams, RE::TESObjectREFR* a_refObject,
			RE::TESObjectREFR* a_container, RE::Script* a_script, RE::ScriptLocals* a_scriptLocals, float&, std::uint32_t& a_offset)
		{
			// The engine writes only the words that were typed, so a bare srm
			// keeps both empty.
			char first[WORD]{};
			char second[WORD]{};
			if (!RE::Script::ParseParameters(a_parameters, a_compiledParams, a_offset, a_refObject, a_container,
					a_script, a_scriptLocals, static_cast<char*>(first), static_cast<char*>(second))) {
				return false;
			}

			auto* const console = RE::ConsoleLog::GetSingleton();
			const auto  asked = Parse(first, second);
			if (!asked) {
				if (console) {
					console->Log("srm takes a percent, the word armor, or both.");
				}
				return true;
			}

			auto* const player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return true;
			}

			const auto targets = Targets(player, asked->armor);
			if (targets.empty()) {
				const auto* none = asked->armor ? "No armor worn that wears out." : "No weapon in hand that wears out.";
				if (console) {
					console->Log("{:s}", none);
				}
				TraceLog::Line("wear", "srm found {:s}", asked->armor ? "no armor worn that wears out" : "no weapon in hand that wears out");
				return true;
			}

			// The pieces were read under the inventory lock and are written
			// once it is released, one at a time, since each write takes the
			// lock itself. A stack with no health is skipped, see
			// Equipped::EquippedStack.
			const auto health = Condition::FromPercent(asked->level);
			for (const auto& target : targets) {
				const auto             name = RE::TESFullName::GetFullName(*target.object);
				Equipped::EquippedStack find;
				Restore::Write(*player, *target.object, find, asked->level);
				if (find.count == 0) {
					TraceLog::Line("wear", "{:s} [{:08X}] is equipped with no reading to write to", name, target.object->formID);
					if (console) {
						console->Log("{:s} has no reading to write to.", name);
					}
					continue;
				}

				TraceLog::Line("wear", "{:s} [{:08X}] {:.6f} -> {:.6f} from the console", name, target.object->formID, target.before, health);
				if (console) {
					console->Log("{:s} is at {:d}% condition.", name, asked->level);
				}
			}

			// The HUD reads the weapon again by itself. The Pip-Boy keeps the
			// cards it built, see ItemCards.h, so the kind's cards are rebuilt,
			// the apparel cards for armor.
			ItemCards::Refresh(asked->armor ? RE::ENUM_FORM_ID::kARMO : RE::ENUM_FORM_ID::kWEAP);
			return true;
		}
	}

	void Install()
	{
		auto* const command = RE::SCRIPT_FUNCTION::LocateScriptCommand("ShowRepairMenu");
		if (!command) {
			REX::WARN("The console has no ShowRepairMenu command to take over, so srm repairs nothing.");
			return;
		}

		command->helpString = "Sets the weapon in hand, or with the word armor every worn piece, to a condition in percent, or to full without one";
		command->SetParameters(PARAMETERS);
		command->executeFunction = Execute;

		REX::INFO("The console command srm repairs the weapon in hand, or with the word armor every worn piece, or sets them to the percent typed after it.");
	}
}
