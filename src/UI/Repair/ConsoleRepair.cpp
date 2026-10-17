#include "UI/Repair/ConsoleRepair.h"

#include "Condition/Condition.h"
#include "Condition/Equipped.h"
#include "Condition/Repair.h"
#include "Core/CallPatch/CallPatch.h"
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
		// The word that turns the command on armor.
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

		// The help srm shows while it is NEC's.
		constexpr const char HELP[] = "Sets the weapon in hand, or with the word armor every worn piece, to a condition in percent, or to full without one";

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

		// An item the command sets, with the health it had and the copies in
		// its stack. why is the reason a piece from Equipped::LeftoverPieces
		// takes no part, and null for one that wears.
		struct Target
		{
			RE::TESBoundObject* object{ nullptr };
			float               before{ Condition::INVALID_HEALTH };
			const char*         why{ nullptr };
			std::uint32_t       count{ 1 };
		};

		// Picks a stack of the item that holds a health, worn or carried. Each
		// clear takes it off one copy, so the copies of a stack are cleared
		// one by one, in any order.
		class HoldsHealth final :
			public RE::BGSInventoryItem::StackDataCompareFunctor
		{
		public:
			bool CompareData(const RE::BGSInventoryItem::Stack& a_stack) override
			{
				if (Condition::HealthOf(&a_stack) < 0.0F) {
					return false;
				}
				found = true;
				return true;
			}

			// Whether a stack was picked.
			bool found{ false };
		};

		// The weapon in hand, or every piece of armor worn that takes part and
		// then every carried piece that kept a health it no longer should.
		[[nodiscard]] std::vector<Target> Targets(RE::PlayerCharacter* a_player, bool a_armor)
		{
			std::vector<Target> out;
			if (a_armor) {
				for (const auto& piece : Equipped::ArmorPieces(a_player)) {
					if (piece.armor) {
						out.push_back({ piece.armor, piece.health });
					}
				}
				for (const auto& piece : Equipped::LeftoverPieces(a_player)) {
					if (piece.armor) {
						out.push_back({ piece.armor, piece.health, Condition::WhyNoCondition(*piece.armor), piece.count });
					}
				}
			} else if (auto* weapon = Equipped::Weapon(a_player)) {
				out.push_back({ weapon, Equipped::WeaponHealth(a_player, weapon) });
			}
			return out;
		}

		// Sets the equipped copy of a target that wears to a_level percent. A
		// stack with no extra data list is skipped, see Equipped::EquippedStack.
		void Set(RE::PlayerCharacter& a_player, RE::ConsoleLog* a_console, const Target& a_target, std::uint32_t a_level)
		{
			const auto              name = RE::TESFullName::GetFullName(*a_target.object);
			Equipped::EquippedStack find;
			Restore::Write(a_player, *a_target.object, find, a_level);
			if (find.count == 0) {
				TraceLog::Line("wear", "{:s} [{:08X}] is equipped with no reading to write to", name, a_target.object->formID);
				if (a_console) {
					a_console->Log("{:s} has no reading to write to.", name);
				}
				return;
			}

			TraceLog::Line("wear", "{:s} [{:08X}] {:.6f} -> {:.6f} from the console", name, a_target.object->formID, a_target.before,
				Condition::FromPercent(a_level));
			if (a_console) {
				a_console->Log("{:s} is at {:d}% condition.", name, a_level);
			}
		}

		// Takes the health off every copy in a leftover target's stack.
		void Clear(RE::PlayerCharacter& a_player, RE::ConsoleLog* a_console, const Target& a_target)
		{
			std::uint32_t cleared = 0;
			for (; cleared < a_target.count; ++cleared) {
				HoldsHealth find;
				Restore::Clear(a_player, *a_target.object, find);
				if (!find.found) {
					break;
				}
			}
			if (cleared == 0) {
				return;
			}

			const auto name = RE::TESFullName::GetFullName(*a_target.object);
			TraceLog::Line("wear", "{:s} [{:08X}] {:.6f} -> none from the console, {:s}", name, a_target.object->formID, a_target.before, a_target.why);
			if (a_console) {
				a_console->Log("{:s} does not wear, so its old condition is cleared.", name);
			}
		}

		// The command's own function before NEC, null for none, and whether
		// the place still runs. Once it is cut, srm hands the call on to it.
		RE::SCRIPT_FUNCTION::ExecuteFunction_t* _Execute = nullptr;
		CallPatch::LinkBase                     g_srmLink;

		// The command and its help and parameters as NEC found them, which
		// Settle puts back.
		struct Found
		{
			RE::SCRIPT_FUNCTION*  command = nullptr;
			const char*           help = nullptr;
			std::uint16_t         count = 0;
			RE::SCRIPT_PARAMETER* parameters = nullptr;
		};

		Found g_found;

		bool ExecuteHk(const RE::SCRIPT_PARAMETER* a_parameters, const char* a_compiledParams, RE::TESObjectREFR* a_refObject,
			RE::TESObjectREFR* a_container, RE::Script* a_script, RE::ScriptLocals* a_scriptLocals, float& a_result,
			std::uint32_t& a_offset)
		{
			if (!g_srmLink.Live()) {
				return !_Execute ||
				       _Execute(a_parameters, a_compiledParams, a_refObject, a_container, a_script, a_scriptLocals, a_result, a_offset);
			}

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
			// lock itself.
			for (const auto& target : targets) {
				if (target.why) {
					Clear(*player, console, target);
				} else {
					Set(*player, console, target, asked->level);
				}
			}

			// The HUD reads the weapon again by itself, and the Pip-Boy lists
			// each written item again, see ItemCards.h. The kind's cards are
			// rebuilt as well, once per command, the apparel cards for armor.
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

		// The help and the parameters go with the command, so they stay as
		// they are when the command is left alone, and Settle puts them back
		// once its place is cut. A command with no function of its own can be
		// taken too. srm never hands the call on while its place holds, see
		// CallPatch::NEVER_HANDS_ON.
		const auto held = CallPatch::PatchPointer(reinterpret_cast<std::uintptr_t>(&command->executeFunction),
			reinterpret_cast<std::uintptr_t>(&ExecuteHk), "console srm", Part::kNone, CallPatch::NEVER_HANDS_ON, &g_srmLink);
		if (!held) {
			REX::WARN("The console command srm stays as it was, with no repairs from NEC.");
			return;
		}
		_Execute = reinterpret_cast<RE::SCRIPT_FUNCTION::ExecuteFunction_t*>(*held);

		g_found = { command, command->helpString, command->paramCount, command->parameters };
		command->helpString = HELP;
		command->SetParameters(PARAMETERS);

		REX::INFO("The console command srm repairs the weapon in hand, or with the word armor every worn piece, or sets them to the percent typed after it.");
	}

	// Each is put back only while it is still NEC's, so another DLL mod that
	// set its own keeps it.
	void Settle()
	{
		auto* const command = g_found.command;
		if (!command || g_srmLink.Live()) {
			return;
		}
		if (command->helpString == HELP) {
			command->helpString = g_found.help;
		}
		if (command->parameters == PARAMETERS) {
			command->paramCount = g_found.count;
			command->parameters = g_found.parameters;
		}
	}
}
