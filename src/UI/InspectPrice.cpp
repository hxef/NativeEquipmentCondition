#include "UI/InspectPrice.h"

#include "Core/TraceLog.h"
#include "UI/Flash.h"

#include <cstdint>

namespace InspectPrice
{
	namespace
	{
		using Params = Scaleform::GFx::FunctionHandler::Params;

		// What the game does before this.
		REL::Relocation<void (*)(RE::BarterMenu*, const Params&)> _Call;

		constexpr auto INSPECT_ITEM = static_cast<std::uintptr_t>(RE::ContainerMenuBase::CodeObjectFunction::kInspectItem);

		// Every call from the screen's movie. The inspect button reads the
		// highlight off the menu object, as the game's own handler does, and
		// GetItemValue sets the direction from whose item the row is before it
		// prices it.
		void CallHk(RE::BarterMenu* a_menu, const Params& a_params)
		{
			if (a_menu && reinterpret_cast<std::uintptr_t>(a_params.userData) == INSPECT_ITEM) {
				const auto index = Flash::Number(a_menu->menuObj, "selectedIndex"sv, -1.0);
				const auto inContainer = Flash::Bool(a_menu->menuObj, "containerIsSelected"sv);
				if (index >= 0.0) {
					const auto row = static_cast<std::uint32_t>(index);
					const auto price = a_menu->GetItemValue(row, inContainer);
					TraceLog::Line("menu", "Inspect on row {:d} of the {:s} half, priced at {:d} for its owner's side",
						row, inContainer ? "trader's" : "player's", price);
				}
			}
			_Call(a_menu, a_params);
		}
	}

	void Install()
	{
		REL::Relocation<std::uintptr_t> menu{ RE::BarterMenu::VTABLE[0] };
		_Call = menu.write_vfunc(0x01, CallHk);

		REX::INFO("The inspect screen at a trader prices an item for the side it is on, "
				  "what the trader charges for theirs and pays for the player's.");
	}
}
