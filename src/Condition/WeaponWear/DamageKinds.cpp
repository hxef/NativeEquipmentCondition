#include "Condition/WeaponWear/DamageKinds.h"

#include "Core/Snapshot.h"

#include <algorithm>
#include <format>
#include <memory>
#include <string>
#include <utility>

namespace WeaponWear
{
	namespace
	{
		// Read by wear on other threads, measured again with the ballistic
		// weapon. Numbers and form IDs only, so nothing to unload.
		Snapshot<DamageKinds> g_kinds;

		const DamageKind* Find(const DamageKinds& a_kinds, std::uint32_t a_type)
		{
			const auto at = std::ranges::find(a_kinds, a_type, &DamageKind::type);
			return at != a_kinds.end() ? &*at : nullptr;
		}

		// A damage type as the log names it, by what its resistance is called
		// in the player's language.
		std::string Phrase(std::uint32_t a_type)
		{
			const auto* type = RE::TESForm::GetFormByID<RE::BGSDamageType>(a_type);
			const auto* resistance = type ? type->data.resistance : nullptr;
			const auto  name = resistance ? RE::TESFullName::GetFullName(*resistance) : ""sv;
			return name.empty() ? std::format("the damage type [{:08X}]", a_type) :
			                      std::format("the damage {:s} stops [{:08X}]", name, a_type);
		}

		void LogFirst(const DamageKinds& a_kinds, std::size_t a_few)
		{
			for (const auto& kind : a_kinds) {
				REX::INFO("An ordinary weapon that deals mostly {:s} hits for {:.0f}, and {:d} weapons wear against it.",
					Phrase(kind.type), kind.median, kind.weapons);
			}
			if (a_few == 1) {
				REX::INFO("1 weapon deals mostly a damage type fewer than {:d} weapons deal, so it wears against the ordinary weapon.",
					MIN_KIND_WEAPONS);
			} else if (a_few > 1) {
				REX::INFO("{:d} weapons deal mostly a damage type fewer than {:d} weapons deal, so they wear against the ordinary weapon.",
					a_few, MIN_KIND_WEAPONS);
			}
		}

		// A line only for a number that moved as the log prints it.
		void LogAgain(const DamageKinds& a_now, const DamageKinds& a_before)
		{
			for (const auto& kind : a_now) {
				const auto* was = Find(a_before, kind.type);
				const auto  said = std::format("{:.0f}", kind.median);
				if (!was) {
					REX::INFO("An ordinary weapon that deals mostly {:s} now hits for {:s}, as a mod changed weapons after game data loaded.",
						Phrase(kind.type), said);
				} else if (const auto saidBefore = std::format("{:.0f}", was->median); said != saidBefore) {
					REX::INFO("An ordinary weapon that deals mostly {:s} now hits for {:s} instead of {:s}, as a mod changed weapons after game data loaded.",
						Phrase(kind.type), said, saidBefore);
				}
			}
			for (const auto& kind : a_before) {
				if (!Find(a_now, kind.type)) {
					REX::INFO("Weapons that deal mostly {:s} now wear against the ordinary weapon, as fewer than {:d} weapons deal it.",
						Phrase(kind.type), MIN_KIND_WEAPONS);
				}
			}
		}
	}

	void PublishKinds(DamageKinds a_kinds, std::size_t a_few, bool a_again)
	{
		const auto before = g_kinds.Get();
		const auto now = std::make_shared<const DamageKinds>(std::move(a_kinds));
		g_kinds.Publish(now);
		if (!a_again) {
			LogFirst(*now, a_few);
		} else {
			LogAgain(*now, before ? *before : DamageKinds{});
		}
	}

	std::optional<float> OwnReference(std::uint32_t a_kind)
	{
		const auto  kinds = a_kind != 0 ? g_kinds.Get() : nullptr;
		const auto* kind = kinds ? Find(*kinds, a_kind) : nullptr;
		return kind ? std::optional{ kind->median } : std::nullopt;
	}
}
