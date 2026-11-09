#include "Papyrus/Papyrus.h"

#include "Core/ScriptEvents.h"
#include "Papyrus/Log.h"
#include "Papyrus/Natives.h"

#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <variant>

// The 2 version functions and the check of NEC.pex against NEC.dll.
namespace Papyrus
{
	namespace
	{
		std::int32_t NumberOf(const REL::Version& a_version)
		{
			return a_version.major() * 10000 + a_version.minor() * 100 + a_version.patch();
		}

		// NEC.pex's answer, or the VM giving up on the call, as it does when
		// NEC.pex is missing or has no such function. Both come on the VM's
		// own thread.
		class ScriptVersion final : public RE::BSScript::IStackCallbackFunctor
		{
		public:
			void CallQueued() override {}
			void CallCanceled() override { Log::Checked(std::nullopt, NumberOf(F4SE::GetPluginVersion())); }
			void StartMultiDispatch() override {}
			void EndMultiDispatch() override {}

			void operator()(RE::BSScript::Variable a_result) override
			{
				const auto pex = a_result.is<std::int32_t>() ? std::optional{ RE::BSScript::get<std::int32_t>(a_result) } : std::nullopt;
				Log::Checked(pex, NumberOf(F4SE::GetPluginVersion()));
			}
		};
	}

	std::int32_t GetVersion(std::monostate)
	{
		Log::Calling("GetVersion");
		return NumberOf(F4SE::GetPluginVersion());
	}

	std::string GetVersionString(std::monostate)
	{
		Log::Calling("GetVersionString");
		const auto version = F4SE::GetPluginVersion();
		return std::format("{}.{}.{}", version.major(), version.minor(), version.patch());
	}

	void Load()
	{
		ScriptEvents::Load();

		// The VM runs the call at its next update, which comes at the main
		// menu too.
		auto*      game = RE::GameVM::GetSingleton();
		const auto vm = game ? game->GetVM() : nullptr;
		if (!vm) {
			REX::WARN("Papyrus: the game has no script VM, so NEC cannot ask NEC.pex its version.");
			return;
		}
		const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> answer{ new ScriptVersion };
		vm->DispatchStaticCall("NEC", "GetScriptVersion", answer);
	}
}
