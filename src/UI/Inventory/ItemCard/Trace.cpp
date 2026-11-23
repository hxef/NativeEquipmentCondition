#include "UI/Inventory/ItemCard/Cards.h"

#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace ItemCard
{
	namespace
	{
		using Scaleform::GFx::Value;

		// A number as short as it will go, 48 or 7.2.
		std::string Short(double a_number)
		{
			return std::format("{:g}", a_number);
		}
	}

	// Written through First, since the listener runs at every redraw in the
	// menu, often many times a second while the list scrolls.
	void TraceCard(std::string_view a_movieFile, const Value& a_card)
	{
		Value entries;
		if (!a_card.GetMember("InfoObj"sv, &entries) || !entries.IsArray()) {
			return;
		}

		std::string text;
		const auto  size = entries.GetArraySize();
		for (std::uint32_t i = 0; i < size; i++) {
			Value entry;
			Value name;
			Value value;
			if (!entries.GetElement(i, &entry) || !entry.IsObject() ||
				!entry.GetMember("text"sv, &name) || !name.IsString()) {
				continue;
			}

			const std::string_view label{ name.GetString() };
			text += text.empty() ? "" : ", ";
			text += label;
			if (entry.GetMember("value"sv, &value)) {
				text += " ";
				text += value.IsString() ? std::string{ value.GetString() } : Short(Flash::AsNumber(value));
			}

			Value flag;
			if (entry.GetMember("showAsPercent"sv, &flag) && flag.IsBoolean() && flag.GetBoolean()) {
				text += "%";
			}
			if (const auto difference = Flash::Number(entry, "difference"sv); difference != 0.0) {
				text += std::format(" ({:s}{:s})", difference > 0.0 ? "+" : "", Short(difference));
			}
			if (Value type; (label == "$dmg"sv || label == "$dr"sv) && entry.GetMember("damageType"sv, &type)) {
				text += std::format(" type {:s}", Short(Flash::AsNumber(type)));
			}
		}
		if (!text.empty()) {
			TraceLog::First("menu", "{:s} card shows {:s}", a_movieFile, text);
		}
	}
}
