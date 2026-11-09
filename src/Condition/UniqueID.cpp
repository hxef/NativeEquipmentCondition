#include "Condition/UniqueID.h"

namespace UniqueID
{
	std::uint16_t Of(const RE::ExtraDataList* a_extra)
	{
		const auto* id = a_extra ? a_extra->GetByType<RE::ExtraUniqueID>() : nullptr;
		return id ? id->uniqueID : 0;
	}

	void Give(RE::ExtraDataList& a_extra, std::uint16_t a_uid, const RE::TESObjectREFR& a_holder)
	{
		a_extra.SetUniqueID(a_uid, a_holder.GetFormID());
	}

	void Clear(RE::ExtraDataList& a_extra)
	{
		a_extra.SetUniqueID(0, 0);
	}
}
