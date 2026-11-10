#pragma once

#include "Core/Plugin.h"

#include <cstdint>
#include <string_view>
#include <variant>

// A script's call that finds an item's inventory locked runs again at the
// VM's next update, about 1 frame later, until a try finds the lock free.
// Only the calling script waits, and its calls keep their order. A native
// that answers kFailedRetry hands its stack to the VM, which keeps it with
// the stacks it suspended and runs it again at its next update, never while
// a save, a load or a quit froze its scripts. A try's trace lines and
// warnings are held until it answers, and a try that found the lock drops
// them, but the 1st keeps its CALL line and adds 1 wait line. The try that
// answers says how many found the lock. Private to this folder.
namespace Papyrus::Waiting
{
	// Around 1 try of a script's call, which the VM runs on the stack
	// a_stack.
	void Begin(std::uint32_t a_stack);
	[[nodiscard]] RE::BSScript::IFunction::CallResult End(RE::BSScript::IFunction::CallResult a_result);

	// The fork's native function, each try of its call between Begin and
	// End.
	template <class R, class... Args>
	class Function final :
		public RE::BSScript::NativeFunction<R(std::monostate, Args...), false, R, std::monostate, Args...>
	{
	private:
		using super = RE::BSScript::NativeFunction<R(std::monostate, Args...), false, R, std::monostate, Args...>;

	public:
		Function(std::string_view a_function, R (*a_native)(std::monostate, Args...)) :
			super("NEC"sv, a_function, a_native, false)
		{}

		RE::BSScript::IFunction::CallResult Call(const RE::BSTSmartPointer<RE::BSScript::Stack>& a_stack,
			RE::BSScript::ErrorLogger& a_log, RE::BSScript::Internal::VirtualMachine& a_vm, bool a_inTasklet) const override
		{
			Begin(a_stack ? a_stack->stackID : 0);
			return End(RE::BSScript::NF_util::NativeFunctionBase::Call(a_stack, a_log, a_vm, a_inTasklet));
		}
	};

	// a_native as a function of the script NEC, for the VM's
	// BindNativeMethod, which takes it over.
	template <class R, class... Args>
	[[nodiscard]] RE::BSScript::IFunction* Make(std::string_view a_function, R (*a_native)(std::monostate, Args...))
	{
		return new Function<R, Args...>(a_function, a_native);
	}

	// Says the try this thread runs found a_holder's inventory locked, so
	// the VM runs the call again. Nothing outside a script's call.
	void Locked(const RE::TESObjectREFR& a_holder);

	// Forgets the tries of every waiting call, as a game ends. A call a save
	// kept waiting counts from its 1st try again after the load.
	void Forget();
}
