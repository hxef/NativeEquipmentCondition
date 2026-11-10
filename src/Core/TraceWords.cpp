#include "Core/TraceWords.h"

namespace TraceLog
{
	namespace detail
	{
		std::string Name(const RE::TESForm* a_form)
		{
			if (!a_form) {
				return "nobody";
			}

			// A reference is named as shown, which a renamed or leveled actor
			// need not share with its base record.
			const char* shown = nullptr;
			if (a_form->IsActor()) {
				shown = const_cast<RE::Actor*>(static_cast<const RE::Actor*>(a_form))->GetDisplayFullName();
			}
			const auto name = shown ? std::string_view{ shown } : RE::TESFullName::GetFullName(*a_form);
			return std::format("{:s} [{:08X}]", name.empty() ? "?"sv : name, a_form->formID);
		}
	}

	std::string_view TypeName(const RE::BGSDamageType& a_type)
	{
		const char* own = a_type.GetFormEditorID();
		if (own && *own) {
			return own;
		}
		const char* resistance = a_type.data.resistance ? a_type.data.resistance->GetFormEditorIDString().c_str() : nullptr;
		return resistance && *resistance ? resistance : "a damage type";
	}
}
