#include "UI/Inventory/PaperDoll/Numbers.h"

#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Hud/HudParts/HudParts.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <string>

namespace PaperDoll
{
	namespace
	{
		namespace Readout = HudParts::Readout;

		// -------------------------------------------------------------------
		// 1 number
		// -------------------------------------------------------------------

		// Where a region's number stands across from its readout's origin,
		// the right end of its bar, and how its letters stand by that point.
		struct Across
		{
			double         x{ 0.0 };
			Readout::Align align{ Readout::Align::kMiddle };
		};

		[[nodiscard]] Across AcrossFor(std::size_t a_region, const NumberSpot& a_spot)
		{
			const auto width = a_spot.placed.width;
			switch (a_region) {
			case kLeftArm:
			case kLeftLeg:
				return Across{ -width, Readout::Align::kStart };
			case kTorso:
				return Across{ -width + a_spot.shortWidth, Readout::Align::kEnd };
			default:
				return Across{ -width / 2.0, Readout::Align::kMiddle };
			}
		}

		// Writes a_text a_size big over its bar, its baseline on the bar's
		// bottom, and returns where its letters sit from the readout's
		// origin.
		Flash::Box Put(Value& a_readout, std::size_t a_region, const NumberSpot& a_spot, const std::string& a_text, double a_size)
		{
			const auto across = AcrossFor(a_region, a_spot);
			const auto written = Readout::ShowNumber(a_readout, a_text, Readout::Label{ .size = a_size, .gap = across.x }, across.align);
			const auto down = (a_spot.placed.deep / 2.0) - written.letters.bottom;
			auto       label = Readout::LabelOf(a_readout);
			Readout::MoveField(label, written.field.left, written.field.top + down);
			return written.letters.Moved(0.0, down);
		}
	}

	// -------------------------------------------------------------------
	// The 6 numbers
	// -------------------------------------------------------------------

	void Numbers::Draw(Scaleform::GFx::Movie& a_movie, Value& a_doll, std::vector<Value>& a_texts, NumberEntries& a_entries, bool a_relaid)
	{
		std::array<std::string, kRegions> numbers;
		auto                              changed = a_relaid;
		for (std::size_t region = 0; region < kRegions; region++) {
			const auto tenths = a_entries[region] ? LowestTenths(region) : NONE;
			numbers[region] = tenths >= 0 ? Readout::PercentText(static_cast<std::uint32_t>(tenths)) : std::string{};
			const auto spot = a_entries[region] ? std::optional{ a_entries[region]->spot } : std::nullopt;
			changed = changed || numbers[region] != shown[region].number || spot != shown[region].spot;
		}
		if (!changed) {
			return;
		}

		// Each number is written at the starting size first, and the
		// tightest room above any of them sets the size all 6 take. Its
		// capitals are measured on the drawn letters, never the field's box.
		auto        fit = 1.0;
		std::string tightUnder;
		auto        tightHigh = 0.0;
		for (std::size_t region = 0; region < kRegions; region++) {
			auto& entry = a_entries[region];
			if (!entry) {
				continue;
			}
			shown[region] = Shown{ numbers[region], entry->spot };
			if (numbers[region].empty()) {
				entry->readout.SetMember("visible"sv, Value(false));
				continue;
			}
			if (word.empty()) {
				auto label = Readout::LabelOf(entry->readout);
				word = HudParts::Translated(label, HudParts::CND_TEXT);
			}
			const auto  letters = Put(entry->readout, region, entry->spot, word + " " + numbers[region], NUMBER_SIZE);
			const auto& at = entry->spot.placed;
			const auto  room = RoomAbove(a_movie, a_doll, a_texts, entry->entry, at.y + letters.bottom, at.x + letters.left, at.x + letters.right);
			const auto  headroom = HeadroomOf(room, at.y + letters.bottom);
			const auto  capitals = letters.bottom - letters.top;
			if (headroom && capitals > 0.0 && headroom->high / capitals < fit) {
				fit = headroom->high / capitals;
				tightHigh = headroom->high;
				tightUnder = headroom->under ? headroom->under->under : std::string{ "the figure's unread clips" };
			}
			entry->readout.SetMember("visible"sv, Value(true));
		}

		const auto wanted = std::clamp(NUMBER_SIZE * std::max(fit, 0.0), NUMBER_LEAST, NUMBER_SIZE);
		if (wanted < NUMBER_SIZE) {
			for (std::size_t region = 0; region < kRegions; region++) {
				auto& entry = a_entries[region];
				if (entry && !numbers[region].empty()) {
					Put(entry->readout, region, entry->spot, word + " " + numbers[region], wanted);
				}
			}
		}

		std::string drawn;
		for (std::size_t region = 0; region < kRegions; region++) {
			if (a_entries[region] && !numbers[region].empty()) {
				std::format_to(std::back_inserter(drawn), "{:s}{:s} {:s} {:s}", drawn.empty() ? "" : ", ", ENTRY_NAMES[region], word, numbers[region]);
			}
		}
		TraceLog::Line("menu", "Pip-Boy doll CND drawn as numbers {:s}, at size {:.1f}", drawn.empty() ? std::string{ "none" } : drawn, wanted);

		// A room too small even for the least size is named once each time
		// it changes, as a bar that covers its limit is.
		const auto tight = NUMBER_SIZE * fit < NUMBER_LEAST ? std::optional{ tightHigh } : std::nullopt;
		if (tight != tightSaid) {
			tightSaid = tight;
			if (tight) {
				TraceLog::Line("menu", "Pip-Boy doll CND numbers have only {:.1f} of room under {:s}, so their least size overlaps it", *tight, tightUnder);
			}
		}
	}

	void Numbers::Forget()
	{
		shown.fill(Shown{});
		tightSaid.reset();
	}
}
