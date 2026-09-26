#include "Core/Text/Text.h"

#include "Core/Text/Lines.h"

// The sentences of a repair, at the bench and at a trader: the question, what
// goes over it, the buttons and what is said after.
namespace Text
{
	namespace
	{
		constexpr Line REPAIR_QUESTION[]{
			{ "en", "{0} is at {1}% condition. Repair it to:" },
			{ "fr", "{0} est à {1}\u00A0% d'état. Réparer jusqu'à\u00A0:" },
			{ "de", "{0} ist bei {1}\u00A0% Zustand. Reparieren auf:" },
			{ "it", "{0} è al {1}% di condizioni. Ripara fino a:" },
			{ "es", "{0} está al {1}% de estado. Reparar hasta:" },
			{ "esmx", "{0} está al {1}% de estado. Reparar hasta:" },
			{ "ptbr", "{0} está com {1}% de condição. Consertar até:" },
			{ "pl", "{0} jest w stanie {1}%. Napraw do:" },
			{ "ru", "{0}: состояние {1}%. Починить до:" },
			{ "ja", "{0}の状態は{1}%です。修理する状態を選択:" },
			{ "zhhant", "{0}的狀況為{1}%。修理至：" },
			{ "zhhans", "{0}的状况为{1}%。修理至：" },
		};

		// Said over the repair question when the player holds a rank of the
		// perk pricing the item. It names the rank, so the player sees their
		// own rank at work, and it counts components, since a bench takes
		// nothing else. See CraftingPerks.h.
		constexpr Line REPAIR_DISCOUNT[]{
			{ "en", "{0} {1} reduces the components required for this repair by {2}%." },
			{ "fr", "{0} {1} réduit de {2}\u00A0% les composants nécessaires à cette réparation." },
			{ "de", "{0} {1} verringert die für diese Reparatur nötigen Komponenten um {2}\u00A0%." },
			{ "it", "{0} {1} riduce del {2}% i componenti necessari per questa riparazione." },
			{ "es", "{0} {1} reduce un {2}% los componentes necesarios para esta reparación." },
			{ "esmx", "{0} {1} reduce un {2}% los componentes necesarios para esta reparación." },
			{ "ptbr", "{0} {1} reduz em {2}% os componentes necessários para este conserto." },
			{ "pl", "{0} {1} zmniejsza o {2}% liczbę komponentów potrzebnych do tej naprawy." },
			{ "ru", "{0} {1} снижает на {2}% количество компонентов, нужных для этого ремонта." },
			{ "ja", "{0} {1}は、この修理に必要な部品を{2}%減らします" },
			{ "zhhant", "{0}等級{1}使這次修理所需的元件減少{2}%。" },
			{ "zhhans", "{0}等级{1}使这次修理所需的元件减少{2}%。" },
		};

		// A floor at full says so in words, since "at least 100%" reads oddly.
		constexpr Line TOO_DAMAGED_FULL[]{
			{ "en", "Condition must be full to modify." },
			{ "fr", "L'état doit être parfait pour modifier." },
			{ "de", "Der Zustand muss einwandfrei sein, um zu modifizieren." },
			{ "it", "Le condizioni devono essere perfette per modificare." },
			{ "es", "El estado debe ser perfecto para modificar." },
			{ "esmx", "El estado debe ser perfecto para modificar." },
			{ "ptbr", "A condição deve estar perfeita para modificar." },
			{ "pl", "Stan musi być idealny, aby modyfikować." },
			{ "ru", "Для модификации состояние должно быть идеальным." },
			{ "ja", "改造するには状態が万全である必要があります" },
			{ "zhhant", "狀況需完好才能改造。" },
			{ "zhhans", "状况需完好才能改造。" },
		};

		constexpr Line TOO_DAMAGED[]{
			{ "en", "Condition must be at least {0}% to modify." },
			{ "fr", "L'état doit être d'au moins {0}\u00A0% pour modifier." },
			{ "de", "Der Zustand muss mindestens {0}\u00A0% betragen, um zu modifizieren." },
			{ "it", "Le condizioni devono essere almeno al {0}% per modificare." },
			{ "es", "El estado debe ser de al menos el {0}% para modificar." },
			{ "esmx", "El estado debe ser de al menos el {0}% para modificar." },
			{ "ptbr", "A condição deve ser de pelo menos {0}% para modificar." },
			{ "pl", "Stan musi wynosić co najmniej {0}%, aby modyfikować." },
			{ "ru", "Для модификации состояние должно быть не ниже {0}%." },
			{ "ja", "改造するには状態が{0}%以上である必要があります" },
			{ "zhhant", "狀況需達{0}%以上才能改造。" },
			{ "zhhans", "状况需达{0}%以上才能改造。" },
		};

		// The word on the bench's REPAIR button over an item worn so little
		// that repairing it is free. Capitals, like the game's own buttons.
		constexpr Line MEND_BUTTON[]{
			{ "en", "MEND" },
			{ "fr", "ENTRETENIR" },
			{ "de", "AUSBESSERN" },
			{ "it", "AGGIUSTA" },
			{ "es", "RETOCAR" },
			{ "esmx", "RETOCAR" },
			{ "ptbr", "RETOCAR" },
			{ "pl", "POPRAW" },
			{ "ru", "ПОДПРАВИТЬ" },
			{ "ja", "手入れ" },
			{ "zhhant", "修補" },
			{ "zhhans", "修补" },
		};

		// Said when MEND repairs an item, with no confirmation box and no
		// components to show for it. Mended, not repaired, since nothing was
		// spent. One table per kind, since the kind is the subject.
		constexpr Line MENDED_WEAPON[]{
			{ "en", "Weapon mended." },
			{ "fr", "Arme entretenue." },
			{ "de", "Waffe ausgebessert." },
			{ "it", "Arma aggiustata." },
			{ "es", "Arma retocada." },
			{ "esmx", "Arma retocada." },
			{ "ptbr", "Arma retocada." },
			{ "pl", "Broń poprawiona." },
			{ "ru", "Оружие подправлено." },
			{ "ja", "武器を手入れしました" },
			{ "zhhant", "武器已修補。" },
			{ "zhhans", "武器已修补。" },
		};

		constexpr Line MENDED_ARMOR[]{
			{ "en", "Armor mended." },
			{ "fr", "Armure entretenue." },
			{ "de", "Rüstung ausgebessert." },
			{ "it", "Armatura aggiustata." },
			{ "es", "Armadura retocada." },
			{ "esmx", "Armadura retocada." },
			{ "ptbr", "Armadura retocada." },
			{ "pl", "Pancerz poprawiony." },
			{ "ru", "Броня подправлена." },
			{ "ja", "アーマーを手入れしました" },
			{ "zhhant", "裝甲已修補。" },
			{ "zhhans", "装甲已修补。" },
		};

		// The price in caps goes on the button beside the condition it buys, so
		// picking a button pays at once. The bench shows its components in the
		// game's own confirmation box instead.
		constexpr Line REPAIR_PRICE[]{
			{ "en", "{0}% for {1} caps" },
			{ "fr", "{0}\u00A0% pour {1} capsules" },
			{ "de", "{0}\u00A0% für {1} Kronkorken" },
			{ "it", "{0}% per {1} tappi" },
			{ "es", "{0}% por {1} chapas" },
			{ "esmx", "{0}% por {1} tapas" },
			{ "ptbr", "{0}% por {1} tampas" },
			{ "pl", "{0}% za {1} kaps." },
			{ "ru", "{0}% за {1} крыш." },
			{ "ja", "{0}%: {1}キャップ" },
			{ "zhhant", "{0}%，{1}枚瓶蓋" },
			{ "zhhans", "{0}%，{1}枚瓶盖" },
		};

		// Said once the caps have changed hands.
		constexpr Line REPAIR_PAID[]{
			{ "en", "Repaired to {0}% for {1} caps." },
			{ "fr", "Réparé à {0}\u00A0% pour {1} capsules." },
			{ "de", "Auf {0}\u00A0% repariert für {1} Kronkorken." },
			{ "it", "Riparato al {0}% per {1} tappi." },
			{ "es", "Reparado al {0}% por {1} chapas." },
			{ "esmx", "Reparado al {0}% por {1} tapas." },
			{ "ptbr", "Consertado para {0}% por {1} tampas." },
			{ "pl", "Naprawiono do {0}% za {1} kaps." },
			{ "ru", "Починено до {0}% за {1} крыш." },
			{ "ja", "{1}キャップで{0}%まで修理しました" },
			{ "zhhant", "已花費{1}枚瓶蓋修理至{0}%。" },
			{ "zhhans", "已花费{1}枚瓶盖修理至{0}%。" },
		};

		// Said over a trader's question, so the player sees why the buttons
		// stop where they do. A trader who can go all the way says so in words,
		// since "up to 100%" reads as a limit. It says "this item" because a
		// trader's limit is per kind, weapons or armor, and the question above
		// it names the item.
		constexpr Line REPAIR_UP_TO_FULL[]{
			{ "en", "This trader can restore this item to full condition." },
			{ "fr", "Ce marchand peut remettre cet objet en parfait état." },
			{ "de", "Dieser Händler kann diesen Gegenstand vollständig instand setzen." },
			{ "it", "Questo commerciante può riportare questo oggetto in condizioni perfette." },
			{ "es", "Este comerciante puede dejar este objeto en perfecto estado." },
			{ "esmx", "Este comerciante puede dejar este objeto en perfecto estado." },
			{ "ptbr", "Este comerciante pode deixar este item em perfeito estado." },
			{ "pl", "Ten handlarz przywraca temu przedmiotowi pełny stan." },
			{ "ru", "Этот торговец может починить этот предмет до идеального состояния." },
			{ "ja", "この商人はこのアイテムを完全な状態まで修理できます" },
			{ "zhhant", "這名商人可以將此物品修復至完美狀況。" },
			{ "zhhans", "这名商人可以将此物品修复至完美状况。" },
		};

		constexpr Line REPAIR_UP_TO[]{
			{ "en", "This trader can repair up to {0}% condition." },
			{ "fr", "Ce marchand peut réparer jusqu'à {0}\u00A0% d'état." },
			{ "de", "Dieser Händler kann bis zu {0}\u00A0% Zustand reparieren." },
			{ "it", "Questo commerciante può riparare fino al {0}% di condizioni." },
			{ "es", "Este comerciante puede reparar hasta el {0}% de estado." },
			{ "esmx", "Este comerciante puede reparar hasta el {0}% de estado." },
			{ "ptbr", "Este comerciante pode consertar até {0}% de condição." },
			{ "pl", "Ten handlarz naprawia do {0}% stanu." },
			{ "ru", "Этот торговец может починить до {0}% состояния." },
			{ "ja", "この商人は状態{0}%まで修理できます" },
			{ "zhhant", "這名商人最多可將狀況修理至{0}%。" },
			{ "zhhans", "这名商人最多可将状况修理至{0}%。" },
		};

		// Said when REPAIR is pressed on an item already past what this trader
		// can do. The button stays on the bar, so the player learns that better
		// traders exist.
		constexpr Line REPAIR_CEILING[]{
			{ "en", "This trader can't repair past {0}%." },
			{ "fr", "Ce marchand ne peut pas réparer au-delà de {0}\u00A0%." },
			{ "de", "Dieser Händler kann nicht über {0}\u00A0% hinaus reparieren." },
			{ "it", "Questo commerciante non può riparare oltre il {0}%." },
			{ "es", "Este comerciante no puede reparar más allá del {0}%." },
			{ "esmx", "Este comerciante no puede reparar más allá del {0}%." },
			{ "ptbr", "Este comerciante não consegue consertar além de {0}%." },
			{ "pl", "Ten handlarz nie naprawi powyżej {0}%." },
			{ "ru", "Этот торговец не может починить выше {0}%." },
			{ "ja", "この商人は{0}%までしか修理できません" },
			{ "zhhant", "這名商人最多只能修理至{0}%。" },
			{ "zhhans", "这名商人最多只能修理至{0}%。" },
		};

		// Said when REPAIR is pressed and the player cannot pay for the
		// smallest step. The button is greyed already, this says why.
		constexpr Line REPAIR_UNAFFORDABLE[]{
			{ "en", "Not enough caps to repair." },
			{ "fr", "Pas assez de capsules pour réparer." },
			{ "de", "Nicht genug Kronkorken für die Reparatur." },
			{ "it", "Tappi insufficienti per riparare." },
			{ "es", "No tienes suficientes chapas para reparar." },
			{ "esmx", "No tienes suficientes tapas para reparar." },
			{ "ptbr", "Tampas insuficientes para consertar." },
			{ "pl", "Za mało kapsli na naprawę." },
			{ "ru", "Недостаточно крышек для ремонта." },
			{ "ja", "修理に必要なキャップが足りません" },
			{ "zhhant", "瓶蓋不足，無法修理。" },
			{ "zhhans", "瓶盖不足，无法修理。" },
		};

		// Said when REPAIR is pressed on an item the bench has nothing to
		// rebuild from, see Workbench/Cost.h. It says where to go instead,
		// since the button stays on the bar.
		constexpr Line BENCH_CANNOT_REPAIR[]{
			{ "en", "This item can't be repaired at a workbench. A trader can restore it." },
			{ "fr", "Cet objet ne peut pas être réparé à un établi. Un marchand peut le remettre en état." },
			{ "de", "Dieser Gegenstand kann an keiner Werkbank repariert werden. Ein Händler kann ihn instand setzen." },
			{ "it", "Questo oggetto non può essere riparato a un banco da lavoro. Un commerciante può ripristinarlo." },
			{ "es", "Este objeto no se puede reparar en un banco de trabajo. Un comerciante puede restaurarlo." },
			{ "esmx", "Este objeto no se puede reparar en un banco de trabajo. Un comerciante puede restaurarlo." },
			{ "ptbr", "Este item não pode ser consertado em uma bancada. Um comerciante pode restaurá-lo." },
			{ "pl", "Tego przedmiotu nie da się naprawić w pracowni. Handlarz może go naprawić." },
			{ "ru", "Этот предмет нельзя починить на верстаке. Его может восстановить торговец." },
			{ "ja", "このアイテムは作業台では修理できません。商人なら修理できます" },
			{ "zhhant", "此物品無法在工作台修理。商人可以將其修復。" },
			{ "zhhans", "此物品无法在工作台修理。商人可以将其修复。" },
		};

		// Said at the bench for an item listed only to be repaired, which stays
		// greyed at full condition, see Workbench/Display.h.
		constexpr Line CANNOT_MODIFY[]{
			{ "en", "This item can't be modified." },
			{ "fr", "Cet objet ne peut pas être modifié." },
			{ "de", "Dieser Gegenstand kann nicht modifiziert werden." },
			{ "it", "Questo oggetto non può essere modificato." },
			{ "es", "Este objeto no se puede modificar." },
			{ "esmx", "Este objeto no se puede modificar." },
			{ "ptbr", "Este item não pode ser modificado." },
			{ "pl", "Tego przedmiotu nie można modyfikować." },
			{ "ru", "Этот предмет нельзя модифицировать." },
			{ "ja", "このアイテムは改造できません" },
			{ "zhhant", "此物品無法改造。" },
			{ "zhhans", "此物品无法改造。" },
		};
	}

	std::string RepairQuestion(std::string_view a_name, std::uint32_t a_percent)
	{
		return Say(REPAIR_QUESTION, a_name, a_percent);
	}

	std::string RepairDiscount(std::string_view a_perk, std::uint32_t a_rank, std::uint32_t a_percent)
	{
		return Say(REPAIR_DISCOUNT, a_perk, a_rank, a_percent);
	}

	std::string TooDamaged(std::uint32_t a_floor)
	{
		return a_floor >= FULL ? Pick(TOO_DAMAGED_FULL) : Say(TOO_DAMAGED, a_floor);
	}

	std::string MendButton()
	{
		return Pick(MEND_BUTTON);
	}

	std::string Mended(bool a_armor)
	{
		return Pick(a_armor ? MENDED_ARMOR : MENDED_WEAPON);
	}

	std::string RepairPrice(std::uint32_t a_level, std::uint32_t a_caps)
	{
		return Say(REPAIR_PRICE, a_level, a_caps);
	}

	std::string RepairPaid(std::uint32_t a_level, std::uint32_t a_caps)
	{
		return Say(REPAIR_PAID, a_level, a_caps);
	}

	std::string RepairUpTo(std::uint32_t a_ceiling)
	{
		return a_ceiling >= FULL ? Pick(REPAIR_UP_TO_FULL) : Say(REPAIR_UP_TO, a_ceiling);
	}

	std::string RepairCeiling(std::uint32_t a_ceiling)
	{
		return Say(REPAIR_CEILING, a_ceiling);
	}

	std::string RepairUnaffordable()
	{
		return Pick(REPAIR_UNAFFORDABLE);
	}

	std::string BenchCannotRepair()
	{
		return Pick(BENCH_CANNOT_REPAIR);
	}

	std::string CannotModify()
	{
		return Pick(CANNOT_MODIFY);
	}
}
