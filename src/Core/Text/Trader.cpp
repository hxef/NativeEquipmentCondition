#include "Core/Text/Text.h"

#include "Core/Text/Lines.h"

// What a trader says about a repair: the line over the question, the buttons,
// why a press is refused and what is said once the caps have changed hands.
namespace Text
{
	namespace
	{
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
		// stop where they do and that the limit is for this kind alone, since
		// the same trader may repair another kind further. A trader who can go
		// all the way says so in words, since "up to 100%" reads as a limit.
		// One table per kind, since the kind sits inside the sentence and some
		// languages change its ending to fit the verb: Polish names armor
		// pancerzom here, pancerze in the next table and pancerzy in the
		// refusal.
		constexpr Line REPAIR_UP_TO_FULL_WEAPONS[]{
			{ "en", "This trader can restore weapons to full condition." },
			{ "fr", "Ce marchand peut remettre les armes en parfait état." },
			{ "de", "Dieser Händler kann Waffen vollständig instand setzen." },
			{ "it", "Questo commerciante può riportare le armi in condizioni perfette." },
			{ "es", "Este comerciante puede dejar las armas en perfecto estado." },
			{ "esmx", "Este comerciante puede dejar las armas en perfecto estado." },
			{ "ptbr", "Este comerciante pode deixar armas em perfeito estado." },
			{ "pl", "Ten handlarz przywraca broni pełny stan." },
			{ "ru", "Этот торговец может починить оружие до идеального состояния." },
			{ "ja", "この商人は武器を完全な状態まで修理できます" },
			{ "zhhant", "這名商人可以將武器修復至完美狀況。" },
			{ "zhhans", "这名商人可以将武器修复至完美状况。" },
		};

		constexpr Line REPAIR_UP_TO_FULL_ARMOR[]{
			{ "en", "This trader can restore armor to full condition." },
			{ "fr", "Ce marchand peut remettre les armures en parfait état." },
			{ "de", "Dieser Händler kann Rüstungen vollständig instand setzen." },
			{ "it", "Questo commerciante può riportare le armature in condizioni perfette." },
			{ "es", "Este comerciante puede dejar las armaduras en perfecto estado." },
			{ "esmx", "Este comerciante puede dejar las armaduras en perfecto estado." },
			{ "ptbr", "Este comerciante pode deixar armaduras em perfeito estado." },
			{ "pl", "Ten handlarz przywraca pancerzom pełny stan." },
			{ "ru", "Этот торговец может починить броню до идеального состояния." },
			{ "ja", "この商人はアーマーを完全な状態まで修理できます" },
			{ "zhhant", "這名商人可以將裝甲修復至完美狀況。" },
			{ "zhhans", "这名商人可以将装甲修复至完美状况。" },
		};

		constexpr Line REPAIR_UP_TO_FULL_CLOTHING[]{
			{ "en", "This trader can restore clothing to full condition." },
			{ "fr", "Ce marchand peut remettre les vêtements en parfait état." },
			{ "de", "Dieser Händler kann Kleidung vollständig instand setzen." },
			{ "it", "Questo commerciante può riportare i vestiti in condizioni perfette." },
			{ "es", "Este comerciante puede dejar la ropa en perfecto estado." },
			{ "esmx", "Este comerciante puede dejar la ropa en perfecto estado." },
			{ "ptbr", "Este comerciante pode deixar roupas em perfeito estado." },
			{ "pl", "Ten handlarz przywraca ubraniom pełny stan." },
			{ "ru", "Этот торговец может починить одежду до идеального состояния." },
			{ "ja", "この商人は服を完全な状態まで修理できます" },
			{ "zhhant", "這名商人可以將服飾修復至完美狀況。" },
			{ "zhhans", "这名商人可以将服饰修复至完美状况。" },
		};

		constexpr Line REPAIR_UP_TO_WEAPONS[]{
			{ "en", "This trader can repair weapons up to {0}% condition." },
			{ "fr", "Ce marchand peut réparer les armes jusqu'à {0}\u00A0% d'état." },
			{ "de", "Dieser Händler kann Waffen bis zu {0}\u00A0% Zustand reparieren." },
			{ "it", "Questo commerciante può riparare le armi fino al {0}% di condizioni." },
			{ "es", "Este comerciante puede reparar las armas hasta el {0}% de estado." },
			{ "esmx", "Este comerciante puede reparar las armas hasta el {0}% de estado." },
			{ "ptbr", "Este comerciante pode consertar armas até {0}% de condição." },
			{ "pl", "Ten handlarz naprawia broń do {0}% stanu." },
			{ "ru", "Этот торговец может починить оружие до {0}% состояния." },
			{ "ja", "この商人は武器を状態{0}%まで修理できます" },
			{ "zhhant", "這名商人最多可將武器的狀況修理至{0}%。" },
			{ "zhhans", "这名商人最多可将武器的状况修理至{0}%。" },
		};

		constexpr Line REPAIR_UP_TO_ARMOR[]{
			{ "en", "This trader can repair armor up to {0}% condition." },
			{ "fr", "Ce marchand peut réparer les armures jusqu'à {0}\u00A0% d'état." },
			{ "de", "Dieser Händler kann Rüstungen bis zu {0}\u00A0% Zustand reparieren." },
			{ "it", "Questo commerciante può riparare le armature fino al {0}% di condizioni." },
			{ "es", "Este comerciante puede reparar las armaduras hasta el {0}% de estado." },
			{ "esmx", "Este comerciante puede reparar las armaduras hasta el {0}% de estado." },
			{ "ptbr", "Este comerciante pode consertar armaduras até {0}% de condição." },
			{ "pl", "Ten handlarz naprawia pancerze do {0}% stanu." },
			{ "ru", "Этот торговец может починить броню до {0}% состояния." },
			{ "ja", "この商人はアーマーを状態{0}%まで修理できます" },
			{ "zhhant", "這名商人最多可將裝甲的狀況修理至{0}%。" },
			{ "zhhans", "这名商人最多可将装甲的状况修理至{0}%。" },
		};

		constexpr Line REPAIR_UP_TO_CLOTHING[]{
			{ "en", "This trader can repair clothing up to {0}% condition." },
			{ "fr", "Ce marchand peut réparer les vêtements jusqu'à {0}\u00A0% d'état." },
			{ "de", "Dieser Händler kann Kleidung bis zu {0}\u00A0% Zustand reparieren." },
			{ "it", "Questo commerciante può riparare i vestiti fino al {0}% di condizioni." },
			{ "es", "Este comerciante puede reparar la ropa hasta el {0}% de estado." },
			{ "esmx", "Este comerciante puede reparar la ropa hasta el {0}% de estado." },
			{ "ptbr", "Este comerciante pode consertar roupas até {0}% de condição." },
			{ "pl", "Ten handlarz naprawia ubrania do {0}% stanu." },
			{ "ru", "Этот торговец может починить одежду до {0}% состояния." },
			{ "ja", "この商人は服を状態{0}%まで修理できます" },
			{ "zhhant", "這名商人最多可將服飾的狀況修理至{0}%。" },
			{ "zhhans", "这名商人最多可将服饰的状况修理至{0}%。" },
		};

		// Said when REPAIR is pressed on an item already past what this trader
		// can do for its kind. The button stays on the bar, so the player
		// learns that better traders exist.
		constexpr Line REPAIR_CEILING_WEAPONS[]{
			{ "en", "This trader can't repair weapons past {0}%." },
			{ "fr", "Ce marchand ne peut pas réparer les armes au-delà de {0}\u00A0%." },
			{ "de", "Dieser Händler kann Waffen nicht über {0}\u00A0% hinaus reparieren." },
			{ "it", "Questo commerciante non può riparare le armi oltre il {0}%." },
			{ "es", "Este comerciante no puede reparar las armas más allá del {0}%." },
			{ "esmx", "Este comerciante no puede reparar las armas más allá del {0}%." },
			{ "ptbr", "Este comerciante não consegue consertar armas além de {0}%." },
			{ "pl", "Ten handlarz nie naprawi broni powyżej {0}%." },
			{ "ru", "Этот торговец не может починить оружие выше {0}%." },
			{ "ja", "この商人は武器を{0}%までしか修理できません" },
			{ "zhhant", "這名商人最多只能將武器修理至{0}%。" },
			{ "zhhans", "这名商人最多只能将武器修理至{0}%。" },
		};

		constexpr Line REPAIR_CEILING_ARMOR[]{
			{ "en", "This trader can't repair armor past {0}%." },
			{ "fr", "Ce marchand ne peut pas réparer les armures au-delà de {0}\u00A0%." },
			{ "de", "Dieser Händler kann Rüstungen nicht über {0}\u00A0% hinaus reparieren." },
			{ "it", "Questo commerciante non può riparare le armature oltre il {0}%." },
			{ "es", "Este comerciante no puede reparar las armaduras más allá del {0}%." },
			{ "esmx", "Este comerciante no puede reparar las armaduras más allá del {0}%." },
			{ "ptbr", "Este comerciante não consegue consertar armaduras além de {0}%." },
			{ "pl", "Ten handlarz nie naprawi pancerzy powyżej {0}%." },
			{ "ru", "Этот торговец не может починить броню выше {0}%." },
			{ "ja", "この商人はアーマーを{0}%までしか修理できません" },
			{ "zhhant", "這名商人最多只能將裝甲修理至{0}%。" },
			{ "zhhans", "这名商人最多只能将装甲修理至{0}%。" },
		};

		constexpr Line REPAIR_CEILING_CLOTHING[]{
			{ "en", "This trader can't repair clothing past {0}%." },
			{ "fr", "Ce marchand ne peut pas réparer les vêtements au-delà de {0}\u00A0%." },
			{ "de", "Dieser Händler kann Kleidung nicht über {0}\u00A0% hinaus reparieren." },
			{ "it", "Questo commerciante non può riparare i vestiti oltre il {0}%." },
			{ "es", "Este comerciante no puede reparar la ropa más allá del {0}%." },
			{ "esmx", "Este comerciante no puede reparar la ropa más allá del {0}%." },
			{ "ptbr", "Este comerciante não consegue consertar roupas além de {0}%." },
			{ "pl", "Ten handlarz nie naprawi ubrań powyżej {0}%." },
			{ "ru", "Этот торговец не может починить одежду выше {0}%." },
			{ "ja", "この商人は服を{0}%までしか修理できません" },
			{ "zhhant", "這名商人最多只能將服飾修理至{0}%。" },
			{ "zhhans", "这名商人最多只能将服饰修理至{0}%。" },
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

		// Said when REPAIR is pressed while a trade is pending. The verbs are
		// the game's own for accepting and resetting a trade, and the trade is
		// the one its "$CancelTradeInProgress" names.
		constexpr Line REPAIR_TRADE_PENDING[]{
			{ "en", "Accept or reset the trade before repairing." },
			{ "fr", "Acceptez ou réinitialisez l'échange avant de réparer." },
			{ "de", "Vor der Reparatur den Handel annehmen oder zurücksetzen." },
			{ "it", "Accetta o ripristina lo scambio prima di riparare." },
			{ "es", "Acepta o restablece el intercambio antes de reparar." },
			{ "esmx", "Acepta o restablece el intercambio antes de reparar." },
			{ "ptbr", "Aceite ou restaure a negociação antes de consertar." },
			{ "pl", "Zaakceptuj lub resetuj wymianę handlową przed naprawą." },
			{ "ru", "Примите или сбросьте сделку перед ремонтом." },
			{ "ja", "修理の前に取引を承認するかリセットしてください" },
			{ "zhhant", "修理前請先接受或重設交易。" },
			{ "zhhans", "修理前请先接受或重设交易。" },
		};

		// The table of the 3 for a_trade.
		[[nodiscard]] std::span<const Line> Of(Trade a_trade, std::span<const Line> a_weapons,
			std::span<const Line> a_armor, std::span<const Line> a_clothing)
		{
			switch (a_trade) {
			case Trade::kArmor:
				return a_armor;
			case Trade::kClothing:
				return a_clothing;
			default:
				return a_weapons;
			}
		}
	}

	std::string RepairPrice(std::uint32_t a_level, std::uint32_t a_caps)
	{
		return Say(REPAIR_PRICE, a_level, a_caps);
	}

	std::string RepairPaid(std::uint32_t a_level, std::uint32_t a_caps)
	{
		return Say(REPAIR_PAID, a_level, a_caps);
	}

	std::string RepairUpTo(std::uint32_t a_ceiling, Trade a_trade)
	{
		if (a_ceiling >= FULL) {
			return Pick(Of(a_trade, REPAIR_UP_TO_FULL_WEAPONS, REPAIR_UP_TO_FULL_ARMOR, REPAIR_UP_TO_FULL_CLOTHING));
		}
		return Say(Of(a_trade, REPAIR_UP_TO_WEAPONS, REPAIR_UP_TO_ARMOR, REPAIR_UP_TO_CLOTHING), a_ceiling);
	}

	std::string RepairCeiling(std::uint32_t a_ceiling, Trade a_trade)
	{
		return Say(Of(a_trade, REPAIR_CEILING_WEAPONS, REPAIR_CEILING_ARMOR, REPAIR_CEILING_CLOTHING), a_ceiling);
	}

	std::string RepairUnaffordable()
	{
		return Pick(REPAIR_UNAFFORDABLE);
	}

	std::string RepairTradePending()
	{
		return Pick(REPAIR_TRADE_PENDING);
	}
}
