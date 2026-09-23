#include "UI/Hud/PowerArmorCondition/Dash.h"

#include "Core/TraceLog.h"

#include <atomic>
#include <mutex>
#include <optional>

namespace PowerArmorCondition
{
	namespace
	{
		// The 3 digits the bar is laid out from, see PowerArmorCondition.h.
		constexpr const char* RESERVE_FIRST = "AmmoCountHunds:0";
		constexpr const char* RESERVE_LAST = "AmmoCountOnes:0";
		constexpr const char* CLIP_FIRST = "ClipCountHunds:0";

		// How many digits apart the 2 ends of a row are, which turns the gap
		// between them into a digit's width.
		constexpr float DIGITS_ACROSS = 2.0F;

		// The bar runs between the 2 rows of digits, where the gameplay HUD's
		// counter draws its divider, about halfway up to the clip row. It sits
		// a little above the middle of the room the lit digits leave there,
		// since the HUD draws a dark shadow under it. It runs from the left
		// side of a 3, 4 or 7 in the first place to the right side of a 3 or 7
		// in the last. A 0 is a little wider and a 1 narrower. Measured off
		// their texture in the dash's own measure: digits across from the
		// first reserve digit and rows down from it, so they hold at any size.
		constexpr float BETWEEN_ROWS = -0.54F;
		constexpr float DIGITS_LEFT = -0.32F;
		constexpr float DIGITS_RIGHT = 2.35F;

		// The word fits inside the ammo box printed on the dash, between the
		// bar's right end and the inner side of the box's right edge, measured
		// the same way, and keeps this far from each, which leaves room for the
		// shadows the HUD draws to the right of the bar and the word.
		constexpr float BOX_RIGHT = 3.135F;
		constexpr float WORD_CLEAR = 0.1F;

		[[nodiscard]] const char* MissText(Miss a_miss)
		{
			switch (a_miss) {
			case Miss::kNoRenderer:
				return "the game is not drawing a dash";
			case Miss::kNoDash:
				return "the dash has no model on it";
			case Miss::kNoDigits:
				return "the dash's ammo digits do not carry the names the game gives them";
			case Miss::kBehindCamera:
				return "the dash sits behind the camera it is drawn through";
			case Miss::kNotFacing:
				return "the dash is not facing the camera squarely";
			default:
				return "it was measured";
			}
		}

		std::mutex        g_anchorLock;
		Anchor            g_anchor;
		std::atomic<bool> g_anchorQueued{ false };

		// Whether the dash is on screen: the player in power armor and the HUD
		// mode allowing the dash. The first is an engine call made when the
		// dash is measured, the second a byte the game writes and this reads.
		std::atomic<bool> g_inPowerArmor{ false };

		// Where a place on the dash lands on the screen. The dash lives in a
		// small scene of its own in front of its own camera, and the HUD's
		// camera knows nothing about it, so the renderer holding the dash is
		// asked for its camera.
		constexpr const char* RENDERER_NAME = "PowerArmorRenderer";

		// How close to the camera a point can be before the engine stops
		// placing it, the same value the engine uses itself.
		constexpr float ZERO_TOLERANCE = 1.0e-5F;

		[[nodiscard]] Point ScreenPoint(const RE::NiCamera& a_camera, const RE::NiPoint3& a_place)
		{
			float x = 0.0F;
			float y = 0.0F;
			float depth = 0.0F;
			const auto placed = a_camera.WorldPtToScreenPt3(a_place, x, y, depth, ZERO_TOLERANCE);

			// The engine returns a fraction across from the left and up from
			// the bottom, and a negative depth for a point behind the camera.
			// Everything here measures down from the top.
			return Point{ x, 1.0 - y, placed && depth > 0.0F };
		}

		// Finds the 3 digits and lays the bar out on the dash from them, or
		// returns a miss.
		[[nodiscard]] Anchor MeasureDash()
		{
			static const RE::BSFixedString rendererName{ RENDERER_NAME };

			// The renderer is freed as the player steps out of the armor, under
			// the renderers lock taken as a writer, and drawn under it as a
			// reader. Holding it as a reader keeps the renderer and the dash
			// alive. GetByName takes it again inside, which is safe for a
			// reader.
			const RE::BSAutoReadLock renderers(RE::Interface3D::Renderer::GetRenderersLock());

			// The renderer only exists while the game draws a dash.
			auto* renderer = RE::Interface3D::Renderer::GetByName(rendererName);
			auto* camera = renderer ? renderer->nativeAspect.get() : nullptr;
			if (!camera) {
				return Anchor{ .miss = Miss::kNoRenderer };
			}

			// What the renderer was handed to draw, the dash itself, which is
			// the model this camera sees.
			RE::NiAVObject* dash = renderer->screenAttachedElementRoot.get();
			if (!dash) {
				auto* geometry = RE::PowerArmorGeometry::GetSingleton();
				dash = geometry ? geometry->paDashDials.get() : nullptr;
			}
			if (!dash) {
				return Anchor{ .miss = Miss::kNoDash };
			}

			static const RE::BSFixedString reserveFirst{ RESERVE_FIRST };
			static const RE::BSFixedString reserveLast{ RESERVE_LAST };
			static const RE::BSFixedString clipFirst{ CLIP_FIRST };

			const auto* first = dash->GetObjectByName(reserveFirst);
			const auto* last = dash->GetObjectByName(reserveLast);
			const auto* above = dash->GetObjectByName(clipFirst);
			if (!first || !last || !above) {
				return Anchor{ .miss = Miss::kNoDigits };
			}

			// The bar is laid out on the dash in its own measure and only then
			// put on the screen, so it lies on the same slanted panel as the
			// ammo box wherever the dash sways.
			const auto& origin = first->GetWorldTranslate();
			const auto  across = (last->GetWorldTranslate() - origin) / DIGITS_ACROSS;  // one digit
			const auto  down = origin - above->GetWorldTranslate();                     // one row
			const auto  between = origin + (down * BETWEEN_ROWS);

			Anchor anchor;
			anchor.start = ScreenPoint(*camera, between + (across * DIGITS_LEFT));
			anchor.end = ScreenPoint(*camera, between + (across * DIGITS_RIGHT));
			anchor.word = ScreenPoint(*camera, between + (across * (DIGITS_RIGHT + WORD_CLEAR)));
			anchor.limit = ScreenPoint(*camera, between + (across * (BOX_RIGHT - WORD_CLEAR)));
			anchor.first = ScreenPoint(*camera, origin);
			anchor.above = ScreenPoint(*camera, above->GetWorldTranslate());
			if (!anchor.start.ok || !anchor.end.ok || !anchor.word.ok || !anchor.limit.ok || !anchor.first.ok || !anchor.above.ok) {
				return Anchor{ .miss = Miss::kBehindCamera };
			}

			// A dash facing away, or one a mod rearranged, leaves nothing to
			// lay the bar against.
			if (!(anchor.end.x > anchor.start.x) || !(anchor.word.x > anchor.end.x) || !(anchor.limit.x > anchor.word.x) ||
				!(anchor.first.y > anchor.above.y)) {
				return Anchor{ .miss = Miss::kNotFacing };
			}
			anchor.miss = Miss::kNone;
			return anchor;
		}
	}

	bool InPowerArmor()
	{
		return g_inPowerArmor.load();
	}

	bool DashAllowed()
	{
		const auto* geometry = RE::PowerArmorGeometry::GetSingleton();
		return geometry && geometry->validHUDModes.canBeVisible;
	}

	void QueueAnchor()
	{
		if (g_anchorQueued.exchange(true)) {
			return;
		}

		const auto* tasks = F4SE::GetTaskInterface();
		if (!tasks) {
			g_anchorQueued = false;
			return;
		}

		tasks->AddTask([] {
			const auto inPowerArmor = RE::PowerArmor::PlayerInPowerArmor();
			const auto measured = MeasureDash();
			g_inPowerArmor = inPowerArmor;

			// Logged once each way, so the log shows the moment the dash was
			// found or lost. Nothing is logged while power armor is off.
			static std::optional<Miss> said;
			if (inPowerArmor && said != measured.miss) {
				said = measured.miss;
				if (measured.miss == Miss::kNone) {
					TraceLog::Line("menu", "Power armor dash found, the CND bar runs from {:.3f},{:.3f} to {:.3f},{:.3f} across and down the screen",
						measured.start.x, measured.start.y, measured.end.x, measured.end.y);
				} else {
					TraceLog::Line("menu", "Power armor dash cannot be measured, {:s}", MissText(measured.miss));
				}
			}

			{
				const std::scoped_lock lock{ g_anchorLock };
				g_anchor = measured;
			}
			g_anchorQueued = false;
		});
	}

	Anchor CurrentAnchor()
	{
		const std::scoped_lock lock{ g_anchorLock };
		return g_anchor;
	}
}
