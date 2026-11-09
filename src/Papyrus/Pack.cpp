#include "Papyrus/Pack.h"

namespace Papyrus
{
	RE::BSScript::Variable PackAs(const RE::TESForm* a_form, RE::BSScript::ENUM_TYPE_ID a_type)
	{
		RE::BSScript::Variable var;
		var = nullptr;
		const auto* game = RE::GameVM::GetSingleton();
		const auto  vm = game ? game->GetVM() : nullptr;
		RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> info;
		if (!a_form || !vm || !vm->GetScriptObjectType(a_type, info) || !info) {
			return var;
		}
		auto&      handles = vm->GetObjectHandlePolicy();
		const auto handle = handles.GetHandleForObject(a_type, a_form);
		RE::BSTSmartPointer<RE::BSScript::Object> object;
		if (!vm->FindBoundObject(handle, info->GetName(), false, object, true)) {
			if (vm->CreateObject(info->name, object) && object) {
				vm->GetObjectBindPolicy().BindObject(object, handle);
			}
		}
		if (object) {
			var = std::move(object);
		}
		return var;
	}

	RE::BSScript::Variable PackItem(const Item& a_item)
	{
		if (const auto* ref = a_item.Reference()) {
			return PackAs(ref, RE::BSScript::kObjectReference);
		}
		RE::BSScript::Variable var;
		var = nullptr;
		if (a_item.Container() && a_item.UniqueID() != 0) {
			RE::BSScript::PackVariable(var, a_item);
		}
		return var;
	}
}
