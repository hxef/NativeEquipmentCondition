#include "Gameplay/ItemValue.h"

#include "Condition/Condition.h"
#include "Core/CallPatch/CallPatch.h"
#include "Core/Settings.h"
#include "Core/TraceLog.h"

#include <cmath>
#include <utility>

namespace ItemValue
{
	namespace
	{
		// Both sit inside BGSInventoryItemUtils::GetInventoryValue, which every
		// caps figure in the game comes out of: the Pip-Boy, the item card, a
		// vendor's offer, the take all button. The first reads the condition
		// through ExtraDataList::GetHealthPerc, the second prices it through
		// GamePlayFormulas::CalculateItemValue.
		constexpr CallPatch::CallSite VALUE_HEALTH_SITE{ RE::ID::BGSInventoryItemUtils::GetInventoryValue.id(), 0x030, "value health" };
		constexpr CallPatch::CallSite VALUE_SITE{ RE::ID::BGSInventoryItemUtils::GetInventoryValue.id(), 0x05D, "value" };

		// Fallout 3 and New Vegas priced a worn item with one curve from 3 game
		// settings:
		//
		//   valueMult = fItemConditionValueBase
		//             + fItemConditionValueMult * (1 - fItemConditionValueBase)
		//               * condition ^ fItemConditionValueExp
		//
		// Both shipped 0.0, 1.0 and 1.5, which is condition to the power of
		// 1.5. The exponent is fValueExponent in NEC.ini, and the other 2 stay
		// tunable here:
		//
		//   condition  price
		//   100%       100%
		//    75%        65%
		//    50%        35%
		//    25%        13%
		//    10%         3%
		//
		// Price falls faster than condition, so the first scratches cost little
		// and a weapon left to rot is worth almost nothing, which is what makes
		// a repair worth paying for.
		constexpr float VALUE_BASE = 0.0F;
		constexpr float VALUE_MULT = 1.0F;

		// fValueExponent, which stops at 0. Below 0 a worn item would sell for
		// more than a new one and a broken one for minus 2 billion caps.
		float Exponent()
		{
			const auto setting = Settings::fValueExponent.GetValue();
			return setting > 0.0F ? setting : 0.0F;
		}

		// The Fallout 3 and New Vegas price curve. No floor, unlike the damage
		// curve: a weapon has to stay worth firing at 0 condition, it does not
		// have to stay worth money. The engine rounds a price under 1 cap up to
		// 1 on its own.
		float ValueMult(float a_health)
		{
			// The same health values Condition::Share treats as new, for the
			// same reason.
			if (a_health < 0.0F || a_health >= Condition::MAX_HEALTH) {
				return 1.0F;
			}

			// A number to the power of 1.5 is the number times its square root,
			// one instruction. std::pow serves an exponent tuned away from 1.5.
			const auto exponent = Exponent();
			const auto curve = exponent == 1.5F ?
				a_health * std::sqrt(a_health) :
				std::pow(a_health, exponent);

			return VALUE_BASE + (VALUE_MULT * (1.0F - VALUE_BASE) * curve);
		}

		// -------------------------------------------------------------------
		// The price hooks
		// -------------------------------------------------------------------

		// The condition of the item being priced, handed from the first hook to
		// the second. Both sit inside one function on one thread, see
		// t_cardHealth in HealthDamage/Card.cpp.
		thread_local float t_valueHealth = Condition::INVALID_HEALTH;

		// The value health and value hooks.
		CallPatch::Held                           g_held;
		CallPatch::Link<float(RE::ExtraDataList*)> g_valueHealthLink;
		CallPatch::Link<float(float, float)>       g_valueLink;

		// Set while a caller wants prices without the wear, see
		// ScopedSoundPrice.
		thread_local bool t_soundPrice = false;

		// Reads the condition and changes nothing. The engine throws the number
		// away an instruction later, with a maxss against 1.0 that raises
		// anything below full back to full: Skyrim's model, where health only
		// rose above 1.0 through tempering. The hook below puts the condition
		// back.
		float ValueHealthHk(RE::ExtraDataList* a_extra)
		{
			if (!g_held.Runs(g_valueHealthLink)) {
				return g_valueHealthLink(a_extra);
			}
			t_valueHealth = g_valueHealthLink(a_extra);
			return t_valueHealth;
		}

		// Scales the finished price by the condition the hook above recorded.
		// The engine's formula has a multiply by condition of its own, Skyrim's
		// tempering, which only raises a price above 1.0, to 2x at
		// fHealthDataValue6, 1.6 in the executable. The clamp above keeps it at
		// 1x for any health at or below full. A sound price, one without wear,
		// skips the formula altogether, since the barter markup is in it too,
		// see ScopedSoundPrice.
		float ItemValueHk(float a_baseValue, float a_health)
		{
			if (!g_held.Runs(g_valueLink)) {
				return g_valueLink(a_baseValue, a_health);
			}

			// Read once, so an item priced without the first hook never
			// takes the condition of the one before it.
			const auto health = std::exchange(t_valueHealth, Condition::INVALID_HEALTH);
			const auto base = t_soundPrice ? a_baseValue : g_valueLink(a_baseValue, a_health);
			const auto mult = t_soundPrice ? 1.0F : ValueMult(health);

			// Opening a container prices every item in it, so this line is one
			// of the most frequent. It stays for every item with a condition,
			// since a wrong price only makes sense next to its condition.
			if (health >= 0.0F) {
				TraceLog::Line("price", "health {:.6f}  {:.0f} caps x {:.4f} = {:.0f} caps",
					health, base, mult, base * mult);
			}

			return base * mult;
		}
	}

	void Install()
	{
		// The second patch does the work and reads what the first recorded, so
		// it is not installed alone.
		g_held = CallPatch::PatchTogether({
			{ VALUE_HEALTH_SITE, RE::ID::ExtraDataList::GetHealthPerc.address(), reinterpret_cast<std::uintptr_t>(&ValueHealthHk), &g_valueHealthLink },
			{ VALUE_SITE, RE::ID::GamePlayFormulas::CalculateItemValue.address(), reinterpret_cast<std::uintptr_t>(&ItemValueHk), &g_valueLink },
		});

		if (!g_held) {
			REX::WARN("A worn item will keep selling for the price of a new one.");
		} else {
			REX::INFO("Item value falls with condition, everywhere the game prints a price.");
			const auto exponent = Exponent();
			REX::INFO("Worn items lose value with condition to the power of {:.2f}{:s}", exponent,
				exponent == 1.5F ? ", the Fallout 3 and New Vegas curve." : ".");
		}
	}

	bool Works()
	{
		return g_held.Intact();
	}

	// Nested, so asking for a sound price inside a call that already asked
	// leaves the outer one in effect.
	ScopedSoundPrice::ScopedSoundPrice() :
		_was(t_soundPrice)
	{
		t_soundPrice = true;
	}

	ScopedSoundPrice::~ScopedSoundPrice()
	{
		t_soundPrice = _was;
	}
}
