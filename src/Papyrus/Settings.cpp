#include "Papyrus/Natives.h"

#include "Core/CallPatch/CallPatch.h"
#include "Core/Feature.h"
#include "Core/Settings.h"
#include "Papyrus/Log.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

// The feature and setting functions. Each reads NEC's own values as they
// stand at the call, so a change on the MCM page shows at the next call, and
// never a game setting.
namespace Papyrus
{
	bool FeatureRuns(std::string_view a_feature)
	{
		const auto* row = FeatureNamed(a_feature);
		if (!row || (row->on && !row->on->GetValue()) || !row->Runs()) {
			return false;
		}
		// With its switch on, a loss that leaves no piece working means another
		// DLL took all of it.
		const auto loss = CallPatch::LossOf(row->part);
		return !loss || !loss->works.empty();
	}

	bool IsFeatureOn(std::monostate, std::string a_feature)
	{
		Log::Calling("IsFeatureOn");
		return FeatureRuns(a_feature);
	}

	bool IsFeatureTakenOver(std::monostate, std::string a_feature)
	{
		Log::Calling("IsFeatureTakenOver");
		const auto* row = FeatureNamed(a_feature);
		return row && CallPatch::LossOf(row->part).has_value();
	}

	float GetSettingFloat(std::monostate, std::string a_id)
	{
		Log::Calling("GetSettingFloat");
		const auto* number = Settings::NumberById<float>(a_id);
		return number ? number->GetValue() : 0.0F;
	}

	std::int32_t GetSettingInt(std::monostate, std::string a_id)
	{
		Log::Calling("GetSettingInt");
		const auto* number = Settings::NumberById<std::int32_t>(a_id);
		return number ? number->GetValue() : 0;
	}

	bool GetSettingBool(std::monostate, std::string a_id)
	{
		Log::Calling("GetSettingBool");
		const auto* on = Settings::SwitchById(a_id);
		return on && on->GetValue();
	}
}
