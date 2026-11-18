#pragma once

// How a place shows an item's condition, the CND stepper of each place, and
// the Gate a row of Features.cpp turns on with. Included by Settings.h only,
// after Live. Private to this folder.
namespace Settings
{
	// How a place shows an item's condition, the value of its CND stepper on
	// the MCM page: 0 nothing, 1 a bar, 2 a percent.
	enum class Cnd
	{
		kOff,
		kBar,
		kPercent,
	};

	// The CND stepper of 1 place, such as iWeaponCnd. Any number but 0, 1 or
	// 2 reads as the form the place ships with, never as Off.
	class CndStep : public Live<std::int32_t>
	{
	public:
		CndStep(std::string_view a_section, std::string_view a_key, Cnd a_shipped) :
			Live<std::int32_t>(a_section, a_key, static_cast<std::int32_t>(a_shipped)), shipped(a_shipped)
		{}

		[[nodiscard]] Cnd Form() const
		{
			const auto value = GetValue();
			return value >= 0 && value <= static_cast<std::int32_t>(Cnd::kPercent) ? static_cast<Cnd>(value) : shipped;
		}

	private:
		Cnd shipped;
	};

	// The setting a row of Features.cpp turns on with: a switch, on while it
	// is true, or a CND stepper, on at any form but Off. Made from a pointer
	// to either, so a row names its setting the same way for both.
	class Gate
	{
	public:
		constexpr Gate() noexcept = default;
		constexpr Gate(Live<bool>* a_switch) noexcept :
			onOff(a_switch)
		{}
		constexpr Gate(CndStep* a_stepper) noexcept :
			stepper(a_stepper)
		{}

		// False for a row with no setting, one that is always on.
		constexpr explicit operator bool() const noexcept { return onOff || stepper; }

		// The switch is true, or the stepper is not 0.
		[[nodiscard]] bool On() const { return onOff ? onOff->GetValue() : stepper->GetValue() != 0; }

		// Writes false or 0 in memory only, as a part left to another mod
		// does, see CallPatch::IsYielded.
		void SetOff() const
		{
			if (onOff) {
				onOff->SetValue(false);
			} else {
				stepper->SetValue(0);
			}
		}

		[[nodiscard]] constexpr const Named* Setting() const noexcept
		{
			return onOff ? static_cast<const Named*>(onOff) : stepper;
		}

		// Off as NEC.ini writes it, false or 0, and the value as it stands.
		[[nodiscard]] constexpr std::string_view OffWord() const noexcept { return onOff ? "false"sv : "0"sv; }
		[[nodiscard]] std::string                Word() const
		{
			return onOff ? std::string{ onOff->GetValue() ? "true"sv : "false"sv } : std::to_string(stepper->GetValue());
		}

		// The switch, nullptr for a stepper, and the stepper, nullptr for a
		// switch.
		[[nodiscard]] constexpr Live<bool>*         Switch() const noexcept { return onOff; }
		[[nodiscard]] constexpr Live<std::int32_t>* Stepper() const noexcept { return stepper; }

		[[nodiscard]] constexpr bool operator==(const Gate&) const noexcept = default;

	private:
		Live<bool>*         onOff = nullptr;
		Live<std::int32_t>* stepper = nullptr;
	};
}
