#include "Papyrus/Natives.h"

#include "Condition/Condition.h"
#include "Core/TraceLog/TraceLog.h"
#include "Papyrus/Items.h"

#include <string>
#include <variant>

// The functions that read an item's condition. Each reads the stack under its
// holder's lock and keeps nothing.
namespace Papyrus
{
	namespace
	{
		// The item record a_form names, itself or a reference's base, or null
		// for None and any other form.
		const RE::TESBoundObject* BoundOf(RE::TESForm* a_form)
		{
			auto* record = RecordOf(a_form);
			return record ? record->As<RE::TESBoundObject>() : nullptr;
		}
	}

	bool IsTracked(std::monostate, RE::TESForm* a_base)
	{
		Log::Calling("IsTracked");
		TraceLog::Papyrus::Begin("CALL", "NEC.IsTracked base={:s}", Hex(a_base));
		const auto* base = BoundOf(a_base);
		return Answer(base && Condition::WearsOut(*base));
	}

	std::string WhyNotTracked(std::monostate, RE::TESForm* a_base)
	{
		Log::Calling("WhyNotTracked");
		TraceLog::Papyrus::Begin("CALL", "NEC.WhyNotTracked base={:s}", Hex(a_base));
		std::string why;
		if (!RecordOf(a_base)) {  // also a deleted reference or one with no base
			why = "None";
		} else if (const auto* base = BoundOf(a_base)) {
			const auto* text = Condition::WhyNoCondition(*base);
			why = text ? text : "";
		} else {
			why = "not an item";
		}
		TraceLog::Papyrus::Line("result", "\"{:s}\"", why);
		return why;
	}

	float GetCondition(std::monostate, Item a_item)
	{
		Log::Calling("GetCondition");
		TraceLog::Papyrus::Begin("CALL", "NEC.GetCondition item={:s}", Shown(a_item));
		auto percent = -1.0F;
		{
			Holding held{ a_item };
			if (const auto* found = held.found(); found && Tracked(*found)) {
				percent = PercentOf(Condition::HealthOf(found->extra));
			}
		}
		TraceLog::Papyrus::Line("result", "{:.4f}", percent);
		return percent;
	}

	bool IsBroken(std::monostate, Item a_item)
	{
		Log::Calling("IsBroken");
		TraceLog::Papyrus::Begin("CALL", "NEC.IsBroken item={:s}", Shown(a_item));
		Holding     held{ a_item };
		const auto* found = held.found();
		return Answer(found && Tracked(*found) && found->extra && found->extra->IsItemBroken());
	}

	bool IsUnrolled(std::monostate, Item a_item)
	{
		Log::Calling("IsUnrolled");
		TraceLog::Papyrus::Begin("CALL", "NEC.IsUnrolled item={:s}", Shown(a_item));
		Holding     held{ a_item };
		const auto* found = held.found();
		return Answer(found && Tracked(*found) && Condition::IsUnrolled(Condition::HealthOf(found->extra)));
	}
}
