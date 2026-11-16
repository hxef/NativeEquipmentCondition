#include "Condition/Materials/Loose.h"

#include <algorithm>
#include <utility>

namespace Materials
{
	// -----------------------------------------------------------------------
	// Which mods come off
	// -----------------------------------------------------------------------

	namespace
	{
		// The keyword a mod carries in its filter keywords when a swap must
		// never return it, read live, so a plugin that changes the default
		// object counts.
		[[nodiscard]] RE::BGSKeyword* NullMod()
		{
			const auto* defaults = RE::BGSDefaultObjectManager::GetSingleton();
			return defaults ? defaults->GetDefaultObject<RE::BGSKeyword>(RE::DEFAULT_OBJECT::kKeywordnullptrMOD) : nullptr;
		}
	}

	std::vector<LooseMod> LooseMods(const RE::ExtraDataList* a_extra)
	{
		std::vector<LooseMod> out;
		const auto* mods = a_extra ? a_extra->GetByType<RE::BGSObjectInstanceExtra>() : nullptr;
		if (!mods || !mods->values) {
			return out;
		}

		auto* keyword = NullMod();
		for (const auto& entry : mods->GetIndexData()) {
			auto* mod = entry.disabled ? nullptr : RE::TESForm::GetFormByID<RE::BGSMod::Attachment::Mod>(entry.objectID);
			auto* item = mod ? mod->GetLooseMod() : nullptr;
			if (!item || (keyword && mod->filterKeywords.HasKeyword(keyword))) {
				continue;
			}
			// A mod with no name of its own goes by its loose mod's.
			std::string name{ RE::TESFullName::GetFullName(*mod) };
			if (name.empty()) {
				name = RE::TESFullName::GetFullName(*item);
			}
			out.push_back({ mod, item, entry.rank, std::move(name), static_cast<std::uint32_t>(std::max(item->GetFormValue(), 0)) });
		}
		return out;
	}

	std::uint32_t NullModKeyword()
	{
		const auto* keyword = NullMod();
		return keyword ? keyword->GetFormID() : 0;
	}

	// -----------------------------------------------------------------------
	// Handing them over
	// -----------------------------------------------------------------------

	void Give(RE::PlayerCharacter& a_player, std::span<const LooseMod> a_mods)
	{
		// The rank goes on a list of its own, as vanilla's swap hands a loose
		// mod over, so the inventory stacks it with the one a swap gave.
		const RE::PlayerCharacter::ScopedInventoryChangeMessageContext quiet{ true, true };
		for (const auto& loose : a_mods) {
			RE::BSTSmartPointer<RE::ExtraDataList> list{ new RE::ExtraDataList() };
			list->AddExtra(new RE::ExtraModRank(loose.rank));
			a_player.AddInventoryItem(loose.item, list, 1, nullptr, nullptr, nullptr);
		}
	}
}
