#include "UI/Inventory/ItemCard/Cards.h"

#include "Condition/ArmorWear/ArmorWear.h"
#include "Condition/Condition.h"
#include "Core/Settings.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>

namespace ItemCard
{
	namespace
	{
		using Scaleform::GFx::Value;

		// Both card builders give a power armor piece 1 entry with this text,
		// the vanilla translation key for Health, and as its value a text of
		// the points the piece has left of its full, such as 25/50, cut to
		// whole points. A broken piece still gets one, 0/50.
		constexpr std::string_view HEALTH_TEXT = "$health"sv;

		// What the Pip-Boy's entry says its value is, in ValueType: 2 for a
		// number, 1 for a text.
		constexpr std::uint32_t NUMBER_VALUE = 2;

		// -------------------------------------------------------------------
		// The number
		// -------------------------------------------------------------------

		// The percent the row shows for a_stack of a_item. Nothing while the
		// switch is off or for anything but a power armor piece, which keeps
		// the card exactly as the game builds it. The game wears and repairs
		// these pieces itself, so this only reads their health.
		std::optional<double> PowerArmorPercent(const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack)
		{
			if (!Settings::bPowerArmorCardCnd.GetValue()) {
				return std::nullopt;
			}
			const auto* armor = a_item.object ? a_item.object->As<RE::TESObjectARMO>() : nullptr;
			if (!armor || !ArmorWear::IsPowerArmor(*armor)) {
				return std::nullopt;
			}

			// A piece with no health extra data reads 100, as vanilla shows it
			// at full, 50/50.
			return Condition::ShownPercent(Condition::HealthOf(a_stack));
		}

		// Called only while the trace log is open, so its texts are read only
		// then.
		void Trace(std::string_view a_site, const RE::BGSInventoryItem& a_item, std::string_view a_health, double a_percent)
		{
			TraceLog::First("menu", "{:s} shows power armor {:s} [{:08X}] Health {:s} as CND {:.0f}%", a_site,
				RE::TESFullName::GetFullName(*a_item.object), a_item.object->formID, a_health, a_percent);
		}

		// -------------------------------------------------------------------
		// The cards outside the Pip-Boy
		// -------------------------------------------------------------------

		// Whether a_entry is the Health row the game added, with its value as
		// a text.
		bool IsHealthEntry(const Value& a_entry)
		{
			const auto text = Flash::Member(a_entry, "text"sv);
			return text.IsString() && text.GetString() == HEALTH_TEXT && Flash::Member(a_entry, "value"sv).IsString();
		}

		// -------------------------------------------------------------------
		// The Pip-Boy's card
		// -------------------------------------------------------------------

		// a_entry's member a_name when it is the text both card builders write,
		// a PipboyPrimitiveValue<BSFixedStringCS>, told by its vtable, else
		// nullptr. Other texts of the Pip-Boy, such as a BSStringT, keep their
		// letters in another layout. Any other value under a_name, from
		// another mod, writes 1 trace line, and its entry stays as it is.
		const RE::PipboyPrimitiveValue<RE::BSFixedStringCS>* PipboyText(RE::PipboyObject& a_entry, const char* a_name, std::string_view a_site)
		{
			using Text = RE::PipboyPrimitiveValue<RE::BSFixedStringCS>;
			auto* member = a_entry.GetMember<RE::PipboyValue*>(RE::BSFixedString(a_name));
			if (!member) {
				return nullptr;
			}
			if (*reinterpret_cast<const std::uintptr_t*>(member) != Text::VTABLE[0].address()) {
				TraceLog::First("menu", "{:s} has a card entry whose {:s} is not vanilla's kind of text, left as it is", a_site, a_name);
				return nullptr;
			}
			return static_cast<const Text*>(member);
		}

		// a_value as a_entry's member a_name. The game's own AddMember puts it
		// in place of the member of that name and frees the old value.
		void PutMember(RE::PipboyObject& a_entry, const char* a_name, RE::PipboyValue* a_value)
		{
			const RE::BSFixedString name{ a_name };
			a_entry.AddMember(&name, a_value);
		}
	}

	void ShowPowerArmorCnd(Value& a_target, const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack, std::string_view a_site)
	{
		const auto percent = PowerArmorPercent(a_item, a_stack);
		Value      entries;
		if (!percent || !CardEntries(a_target, entries)) {
			return;
		}

		// From the end, where the game's builder adds its entries. No Health
		// row leaves the card as the game built it.
		for (auto i = entries.GetArraySize(); i-- > 0;) {
			Value entry;
			if (!entries.GetElement(i, &entry) || !IsHealthEntry(entry)) {
				continue;
			}
			// Traced first, while vanilla's text is still there.
			if (TraceLog::IsOpen()) {
				Trace(a_site, a_item, Flash::Member(entry, "value"sv).GetString(), *percent);
			}
			Flash::Set(entry, "text"sv, Value(CND_TEXT));
			Flash::Set(entry, "value"sv, Value(*percent));
			Flash::Set(entry, "showAsPercent"sv, Value(true));

			// The card turns entries into rows from last to first, so the
			// first entry becomes the highest plain row, where CND sits on
			// other armor.
			MoveEntry(entries, i, 0);
			return;
		}
	}

	void ShowPowerArmorCnd(RE::PipboyObject& a_data, const RE::BGSInventoryItem& a_item, const RE::BGSInventoryItem::Stack* a_stack,
		std::string_view a_site)
	{
		const auto percent = PowerArmorPercent(a_item, a_stack);
		auto*      entries = percent ? a_data.GetMember<RE::PipboyArray*>(RE::BSFixedString("itemCardInfoList")) : nullptr;
		if (!entries) {
			return;
		}

		// The game's builder made this array from scratch just now, so its
		// order is still free to change, as Hooks.cpp does for CND.
		auto& elements = entries->elements;
		for (auto i = elements.size(); i-- > 0;) {
			auto* value = elements[i];
			if (!value || value->GetType() != RE::PipboyValue::SERIALIZATION_DATA_TYPE::kObject) {
				continue;
			}
			auto*       entry = static_cast<RE::PipboyObject*>(value);
			const auto* text = PipboyText(*entry, "text", a_site);
			if (!text || std::string_view{ text->m_value } != HEALTH_TEXT) {
				continue;
			}

			// Like the other cards, a Health row whose value is no text stays
			// vanilla. Traced first, while vanilla's text is still there.
			const auto* shown = PipboyText(*entry, "value", a_site);
			if (!shown) {
				return;
			}
			if (TraceLog::IsOpen()) {
				Trace(a_site, a_item, std::string_view{ shown->m_value }, *percent);
			}

			// The members the game's own number entry has, written as it
			// writes them, so the Pip-Boy copies them to the card the same way.
			PutMember(*entry, "text", new RE::PipboyPrimitiveValue<RE::BSFixedStringCS>(RE::BSFixedStringCS{ CND_TEXT }, entry));
			PutMember(*entry, "value", new RE::PipboyPrimitiveValue<float>(static_cast<float>(*percent), entry));
			PutMember(*entry, "ValueType", new RE::PipboyPrimitiveValue<std::uint32_t>(NUMBER_VALUE, entry));
			PutMember(*entry, "showAsPercent", new RE::PipboyPrimitiveValue<bool>(true, entry));
			std::rotate(elements.begin(), elements.begin() + i, elements.begin() + i + 1);
			return;
		}
	}
}
