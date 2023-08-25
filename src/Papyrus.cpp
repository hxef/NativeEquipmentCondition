
//#include "Papyrus.h"

//inline void SetSelectedReference(std::monostate, RE::TESObjectREFR* a_reference)
//{
//	const auto ui = RE::UI::GetSingleton();
//	const auto console = ui ? ui->GetMenu<RE::Console>() : nullptr;
//	if (console) {
//		RE::ObjectRefHandle handle{ a_reference };
//		console->SetCurrentPickREFR(&handle);
//	}
//}
//
//inline void Bind(RE::BSScript::IVirtualMachine& a_vm)
//{
//	const auto obj = Version::PROJECT;
//
//	BIND(SetSelectedReference);
//	//a_vm.BindNativeMethod(obj, "SetSelectedReference", SetSelectedReference);
//
//	logger::debug("bound {} script"sv, obj);
//}
//
//bool F4SEAPI Bind(RE::BSScript::IVirtualMachine* a_vm)
//{
//	if (!a_vm) {
//		return false;
//	}
//
//	Bind(*a_vm);
//
//	logger::debug("bound all scripts");
//	return true;
//}
