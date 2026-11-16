#include "UI/Hud/PickupBar/Bar.h"

#include "Condition/Condition.h"
#include "Core/Settings.h"
#include "Core/TraceLog/TraceLog.h"
#include "UI/Flash.h"
#include "UI/Hud/HudParts/HudParts.h"
#include "UI/Hud/PickupBar/Look.h"
#include "UI/Roles/Rollover.h"

#include <REX/CONVERT.h>
#include <REX/W32/KERNEL32.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace PickupBar
{
	namespace
	{
		using Scaleform::GFx::Value;
		namespace Readout = HudParts::Readout;

		// -------------------------------------------------------------------
		// How the bar looks
		// -------------------------------------------------------------------

		// The name the readout carries.
		constexpr const char* READOUT_NAME = "NEC_PickupCondition_mc";

		// The bar's own size in the prompt's units, where the name is written
		// at size 20: 44 across, as long as the HUD CND bar on the ammo
		// counter's divider, and as deep as the AP meter's bar at that size, 6.
		constexpr double NAME_SIZE = 20.0;
		constexpr double BAR_LENGTH = 44.0;
		constexpr double BAR_DEEP = Readout::BAR_DEEP * NAME_SIZE;

		// The word fits the 14 between the name and the hints, level with the
		// bar and 6 past its end.
		constexpr Readout::Label WORD{ .size = 14.0, .gap = 6.0 };

		// A percent and a place no frame holds, so the first frame always
		// draws.
		constexpr std::int32_t NOT_DRAWN = -2;
		constexpr double       NOWHERE = std::numeric_limits<double>::quiet_NaN();

		// A nan typed into the ini counts as 0.
		double Offset(const Settings::Live<float>& a_setting)
		{
			const auto value = a_setting.GetValue();
			return std::isfinite(value) ? value : 0.0;
		}

		// The text in capitals, each letter raised on its own with no
		// language's rules, as vanilla's prompt raises the name it prints.
		// Text that will not convert stays as it is.
		std::wstring Capitals(std::string_view a_text)
		{
			std::wstring wide;
			if (!REX::UTF8_TO_UTF16(a_text, wide)) {
				return wide;
			}
			const auto   size = static_cast<std::int32_t>(wide.size());
			std::wstring capitals(wide.size(), L'\0');
			const auto   written = REX::W32::LCMapStringEx(REX::W32::LOCALE_NAME_INVARIANT, REX::W32::LCMAP_UPPERCASE, wide.data(), size, capitals.data(), size, nullptr, nullptr, 0);
			return written == size ? capitals : wide;
		}

		// The name without a sorter's tag in front: 1 bracket group that opens
		// it, [..], (..), {..} or |..|, and the spaces after it, so
		// [10mm] 10mm Pistol gives 10mm Pistol. A name with no such group, or
		// with nothing after it, stays as it is.
		std::string_view Untagged(std::string_view a_name)
		{
			constexpr std::string_view OPENS = "[({|";
			constexpr std::string_view CLOSES = "])}|";
			const auto kind = a_name.empty() ? std::string_view::npos : OPENS.find(a_name.front());
			const auto close = kind == std::string_view::npos ? std::string_view::npos : a_name.find(CLOSES[kind], 1);
			const auto rest = close == std::string_view::npos ? std::string_view::npos : a_name.find_first_not_of(' ', close + 1);
			return rest == std::string_view::npos ? a_name : a_name.substr(rest);
		}

		// Whether a_text is a pile's count as vanilla prints it after the
		// name, a space and a number in brackets, such as " (3)".
		bool IsCount(std::wstring_view a_text)
		{
			if (a_text.size() < 4 || !a_text.starts_with(L" (") || !a_text.ends_with(L')')) {
				return false;
			}
			const auto digits = a_text.substr(2, a_text.size() - 3);
			return digits.find_first_not_of(L"0123456789") == std::wstring_view::npos;
		}

		// Whether the prompt's a_text names a_name. The prompt prints the name
		// the game built, in capitals and with a count after it for a pile of
		// several, so the name only has to be in it once both are in
		// capitals. Some prompts print the name without a sorter's tag, see
		// Untagged, so the name without it counts too, but only as the whole
		// text or with a count after it, so another item never matches.
		bool Names(std::string_view a_text, std::string_view a_name)
		{
			const auto text = Capitals(a_text);
			if (text.find(Capitals(a_name)) != std::wstring::npos) {
				return true;
			}
			const auto untagged = Untagged(a_name);
			if (untagged.size() == a_name.size()) {
				return false;
			}
			const auto name = Capitals(untagged);
			const auto after = std::wstring_view{ text }.substr(std::min(name.size(), text.size()));
			return !name.empty() && text.starts_with(name) && (after.empty() || IsCount(after));
		}

		// -------------------------------------------------------------------
		// The frame listener
		// -------------------------------------------------------------------

		// Called by the HUD every frame through an enterFrame listener on the
		// readout, which sits in the prompt's clip.
		class FrameListener final : public Scaleform::GFx::FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				Value readout;
				if (a_params.argCount < 1 || !a_params.args[0].GetMember("currentTarget"sv, &readout) || !readout.IsDisplayObject()) {
					return;
				}

				// The HUD menu is listed a frame after its movie loads. The
				// prompt's own part is found with it and lasts as long.
				if (!colored && a_params.movie) {
					if (auto* menu = HudParts::MenuFor(*a_params.movie)) {
						target = HudParts::AddColorTarget(*menu, readout);
						rolloverPart = HudParts::RolloverPart(*menu);
						colored = true;
						TraceLog::Line("menu", "Pickup CND is a HUD part of its own, in the gameplay HUD colour, {:s}",
							rolloverPart ? "and follows the prompt's warning colour" : "but found no pickup prompt part to follow the warning colour by");
					}
				}

				const auto prompt = Flash::Member(readout, "parent"sv);
				const auto name = Roles::Rollover::Name(prompt);
				const auto hints = Roles::Rollover::Hints(prompt);

				// The prompt has to name the item NEC read, see Names.
				const auto kept = Kept();
				const auto wanted = Settings::NecOn() && Settings::bPickupBar.GetValue() && kept && kept->percent >= 0 && hints.IsDisplayObject() &&
				                    Flash::Bool(prompt, "visible"sv) &&
				                    Names(Flash::String(name, "text"sv), kept->name);
				if (!wanted) {
					Show(readout, false);
					return;
				}

				// Laying out again means measuring text, so it waits for the
				// player to size or flip the bar.
				const auto size = Readout::SizeFrom(Settings::fPickupBarLength, Settings::fPickupBarThickness);
				const auto flipped = Settings::bCndBarsFlipped.GetValue();
				if (size != laidOutSize || flipped != laidOutFlipped) {
					width = BAR_LENGTH * size.length;
					deep = BAR_DEEP * size.thickness;
					Readout::SetLabel(readout, WORD);
					if (flipped) {
						Readout::MirrorLabel(readout, width);
					}
					Readout::SetTrack(readout, width, deep);
					if (size != laidOutSize) {
						TraceLog::Line("menu", "Pickup CND bar sized to {:g} times its length and {:g} times its thickness, {:.0f} across and {:.1f} deep",
							size.length, size.thickness, width, deep);
					}
					laidOutSize = size;
					laidOutFlipped = flipped;
					shownPercent = NOT_DRAWN;
					shownMark = Readout::Mark{};
				}
				Readout::FollowWordSwitch(readout, Settings::bPickupBarShowsWord, wordShown, "Pickup"sv);

				// The bar is centred under the name and the word hangs off its
				// right end, or its left end while flipped, so turning the word
				// off moves nothing. It sits halfway down the gap above the
				// hints, read from the movie each frame since a long name wraps
				// upward. The readout's origin is the bar's right end. Whole
				// pixels keep the bar's edges sharp.
				// The movie's y runs down, so above 0 is taken off to go up.
				const auto centre = Flash::Number(name, "x"sv) + (Flash::Number(name, "width"sv) / 2.0);
				const auto below = Flash::Number(name, "y"sv) + Flash::Number(name, "height"sv);
				const auto x = std::round(centre + (width / 2.0) + Offset(Settings::fPickupBarX));
				const auto y = std::round(((below + Flash::Number(hints, "y"sv)) / 2.0) - Offset(Settings::fPickupBarY));
				if (x != shownX || y != shownY) {
					readout.SetMember("x"sv, Value(x));
					readout.SetMember("y"sv, Value(y));
					shownX = x;
					shownY = y;
					shownPercent = NOT_DRAWN;
					shownMark = Readout::Mark{};
				}

				// A weapon's bar marks the weapon point and an armor or
				// clothing piece's the armor point, a food's nothing. The mark
				// is as wide as half the bar's depth before its thickness
				// slider.
				const auto mark = kept->kind ? Readout::PenaltyMark(*kept->kind, BAR_DEEP, BAR_DEEP / 2.0) : Readout::Mark{};
				if (kept->percent != shownPercent || kept->name != shownName || mark != shownMark) {
					Readout::SetPercent(readout, width, deep, kept->percent, Readout::StillEnd(Readout::Still::kLeftEnd, laidOutFlipped), mark);
					Readout::TellFlipped(laidOutFlipped, saidFlipped, "Pickup CND bar"sv);
					if (kept->percent != shownPercent || kept->name != shownName) {
						TraceLog::Line("menu", "Pickup CND bar drawn at {:d}% for {:s}, its right end at {:.0f},{:.0f} in the prompt, the name's middle at {:.1f}",
							kept->percent, kept->name, x, y, centre);
					}
					shownPercent = kept->percent;
					shownName = kept->name;
					shownMark = mark;
					TellMark(mark, *kept);
				}

				// The game paints the prompt's name and hints in the warning
				// colour while taking the item is stealing, and the bar joins
				// them.
				const auto stealing = rolloverPart && rolloverPart->displayWarningColor;
				if (stealing != warned && HudParts::SetWarningColor(target, stealing)) {
					warned = stealing;
					TraceLog::Line("menu", "Pickup CND bar {:s}", stealing ? "in the warning colour, taking is stealing" : "back in the HUD colour");
				}
				Show(readout, true);
			}

			void Reset()
			{
				laidOutSize = Readout::Size{ 0.0, 0.0 };
				laidOutFlipped = false;
				saidFlipped = false;
				shownPercent = NOT_DRAWN;
				shownName.clear();
				shownMark = Readout::Mark{};
				saidMark = Readout::Mark{};
				saidName.clear();
				shownX = NOWHERE;
				shownY = NOWHERE;
				shown.reset();
				wordShown.reset();
				colored = false;
				target = nullptr;
				rolloverPart = nullptr;
				warned = false;
			}

		private:
			// Names the mark when it changes, and each new item while either
			// point is from 0.01 to 0.99, so an item with no mark is named too.
			// The shown percent is forgotten whenever the prompt moves, and a
			// new name moves it, so the trace keeps its own mark and name.
			void TellMark(const Readout::Mark& a_mark, const Looked& a_kept)
			{
				const auto marking = Condition::StartPercent(Condition::Kind::kWeapon) != 0 || Condition::StartPercent(Condition::Kind::kArmor) != 0;
				if (a_mark == saidMark && (!marking || a_kept.name == saidName)) {
					return;
				}
				saidMark = a_mark;
				saidName = a_kept.name;
				if (a_mark.at != 0 && a_kept.kind) {
					TraceLog::Line("menu", "Pickup CND bar marks {:s} penalties from {:d}% {:s} {:.2f} wide for {:s}", *a_kept.kind == Condition::Kind::kArmor ? "armor" : "weapon",
						a_mark.at, a_mark.line ? "with a line" : "with notches", a_mark.wide, a_kept.name);
				} else {
					TraceLog::Line("menu", "Pickup CND bar marks no penalty start for {:s}", a_kept.name);
				}
			}

			void Show(Value& a_readout, bool a_shown)
			{
				if (shown != a_shown) {
					a_readout.SetMember("visible"sv, Value(a_shown));
					shown = a_shown;
					TraceLog::Line("menu", "Pickup CND bar {:s}", a_shown ? "shows" : "hides");
				}
			}

			Readout::Size       laidOutSize{ 0.0, 0.0 };
			bool                laidOutFlipped = false;
			bool                saidFlipped = false;
			double              width = 0.0;
			double              deep = 0.0;
			std::int32_t        shownPercent = NOT_DRAWN;
			std::string         shownName;
			Readout::Mark       shownMark;
			double              shownX = NOWHERE;
			double              shownY = NOWHERE;
			std::optional<bool> shown;
			std::optional<bool> wordShown;
			bool                colored = false;

			// Only a name for HudParts to look the target up by, see
			// HudParts::SetWarningColor.
			const RE::BSGFxShaderFXTarget* target = nullptr;

			// The vanilla pickup prompt part, whose warning colour the bar
			// follows, and whether the bar is in that colour now.
			const RE::HUDComponentBase* rolloverPart = nullptr;
			bool                        warned = false;

			// The mark and the item the trace last named, forgotten only in
			// Reset.
			Readout::Mark saidMark;
			std::string   saidName;
		};

		FrameListener g_frameListener;
	}

	// -------------------------------------------------------------------
	// Building the bar
	// -------------------------------------------------------------------

	void AddBar(Scaleform::GFx::Movie& a_movie)
	{
		auto prompt = Roles::Rollover::Find(a_movie);
		if (!prompt) {
			REX::WARN("The HUD has no pickup prompt at vanilla's place, so it shows no CND bar. A HUD replacer may have moved it.");
			return;
		}

		// Inside the prompt's clip the bar moves and hides with the prompt. The
		// HUD draws only inside its colour targets' rectangles, and the prompt's
		// cover just its name and its button hints, so the bar gets its own
		// from the frame listener. Nothing is added to the HUD until the whole
		// readout is built.
		auto readout = Readout::Create(a_movie, READOUT_NAME);
		if (!readout.IsDisplayObject() || !prompt->Invoke("addChild", std::array{ readout })) {
			REX::WARN("The HUD's pickup prompt would not take a CND bar, so it shows none.");
			return;
		}

		// A new HUD movie starts with nothing drawn.
		g_frameListener.Reset();

		Value listener;
		a_movie.CreateFunction(&listener, &g_frameListener);
		if (!readout.Invoke("addEventListener", std::array{ Value("enterFrame"), listener })) {
			REX::WARN("The HUD refused the pickup CND bar's frame listener, so the pickup prompt shows no CND bar.");
			return;
		}
		TraceLog::Line("menu", "HUDMenu.swf loaded, the pickup prompt carries a CND bar");
	}
}
