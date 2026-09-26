#include "Core/Text/Text.h"

#include "Core/Text/Lines.h"

// The tips the loading screen shows now and then, see LoadingTips.h. Each is a
// fact about play told in the game's own voice, as if condition had always
// been part of it, and names the benches, caps, raiders and Gunners as the
// vanilla loading screens do in each language. Japanese ends without 。, as
// the game's own screens do, and French puts a no-break space before ? as they
// do.
namespace Text
{
	namespace
	{
		// What wear does.
		constexpr Line WEAR[]{
			{ "en", "Weapons and armor lose condition as they are used. A worn weapon deals less damage, worn armor offers less protection, and both are worth fewer caps." },
			{ "fr", "L'état des armes et des armures se dégrade à l'usage. Une arme usée inflige moins de dégâts, une armure usée protège moins, et toutes deux valent moins de capsules." },
			{ "de", "Der Zustand von Waffen und Rüstungen verschlechtert sich durch Gebrauch. Eine abgenutzte Waffe richtet weniger Schaden an, eine abgenutzte Rüstung schützt weniger, und beide sind weniger Kronkorken wert." },
			{ "it", "Le condizioni di armi e armature peggiorano con l'uso. Un'arma usurata infligge meno danni, un'armatura usurata protegge meno ed entrambe valgono meno tappi." },
			{ "es", "El estado de las armas y las armaduras empeora con el uso. Un arma desgastada causa menos daño, una armadura desgastada protege menos y ambas valen menos chapas." },
			{ "esmx", "El estado de las armas y las armaduras empeora con el uso. Un arma desgastada causa menos daño, una armadura desgastada protege menos y ambas valen menos tapas." },
			{ "ptbr", "A condição de armas e armaduras piora com o uso. Uma arma desgastada causa menos dano, uma armadura desgastada protege menos e ambas valem menos tampas." },
			{ "pl", "Stan broni i pancerzy pogarsza się wraz z użytkowaniem. Zużyta broń zadaje mniejsze obrażenia, zużyty pancerz słabiej chroni, a jedno i drugie jest warte mniej kapsli." },
			{ "ru", "Состояние оружия и брони ухудшается по мере использования. Изношенное оружие наносит меньше урона, изношенная броня хуже защищает, и то и другое стоит меньше крышек." },
			{ "ja", "武器とアーマーの状態は使うほど悪化する。消耗した武器は与えるダメージが減り、消耗したアーマーは防御力が落ちる。どちらもキャップでの価値が下がる" },
			{ "zhhant", "武器和裝甲的狀況會隨著使用而下降。損耗的武器傷害較低，損耗的裝甲防護力較差，兩者能換得的瓶蓋也會變少。" },
			{ "zhhans", "武器和装甲的状况会随着使用而下降。损耗的武器伤害较低，损耗的装甲防护力较差，两者能换得的瓶盖也会变少。" },
		};

		// Repairing at a workbench, and the floor for modding, MODIFY_FLOOR in
		// Workbench/Bench.h.
		constexpr Line BENCH[]{
			{ "en", "Use a Weapons Workbench or an Armor Workbench to repair worn gear with the components it's made from. Gear must be in full condition before it can be modified." },
			{ "fr", "Utilisez un établi d'armes ou un établi d'armures pour réparer l'équipement usé avec les composants dont il est fait. Seul un équipement en parfait état peut être modifié." },
			{ "de", "An einer Waffenwerkbank oder einer Rüstungs-Werkbank kannst du abgenutzte Ausrüstung mit den Komponenten reparieren, aus denen sie besteht. Nur Ausrüstung in einwandfreiem Zustand lässt sich modifizieren." },
			{ "it", "Usa un banco da lavoro per armi o per armature per riparare l'equipaggiamento usurato con i componenti di cui è fatto. Solo l'equipaggiamento in condizioni perfette può essere modificato." },
			{ "es", "Usa un banco de trabajo para armas o para armaduras para reparar el equipo desgastado con los componentes de los que está hecho. Solo el equipo en perfecto estado se puede modificar." },
			{ "esmx", "Usa un banco de trabajo para armas o para armaduras para reparar el equipo desgastado con los componentes de los que está hecho. Solo el equipo en perfecto estado se puede modificar." },
			{ "ptbr", "Use a bancada de armas ou a bancada de armaduras para consertar equipamentos desgastados com os componentes de que são feitos. Só equipamentos em perfeito estado podem ser modificados." },
			{ "pl", "Korzystaj z pracowni rusznikarskiej lub płatnerskiej, aby naprawiać zużyty sprzęt za pomocą komponentów, z których go zbudowano. Sprzęt można modyfikować dopiero wtedy, gdy jego stan jest idealny." },
			{ "ru", "На оружейном верстаке и верстаке для создания брони можно чинить изношенное снаряжение с помощью компонентов, из которых оно собрано. Модифицировать его можно, только когда его состояние идеально." },
			{ "ja", "武器作業台やアーマー作業台で、消耗した装備をその材料となる部品で修理しよう。装備は状態が万全でないと改造できない" },
			{ "zhhant", "使用武器工作台或裝甲工作台，以製作裝備的元件修理損耗的裝備。裝備狀況需完好才能改造。" },
			{ "zhhans", "使用武器工作台或装甲工作台，以制作装备的元件修理损耗的装备。装备状况需完好才能改造。" },
		};

		// Repairing at a trader. It names the shops, since a general store with
		// a pistol on the shelf repairs nothing.
		constexpr Line TRADER[]{
			{ "en", "Short on components? Gun shops, armorers and clothiers will repair their own kind of gear for caps. The better stocked the shop, the closer to new they can bring it." },
			{ "fr", "À court de composants\u00A0? Armuriers, marchands d'armures et tailleurs réparent leur type d'équipement contre des capsules. Plus la boutique est fournie, plus la réparation se rapproche du neuf." },
			{ "de", "Keine Komponenten zur Hand? Waffenhändler, Rüstungshändler und Kleiderhändler reparieren ihre Art von Ausrüstung gegen Kronkorken. Je besser der Laden bestückt ist, desto näher kommt die Reparatur dem Neuzustand." },
			{ "it", "A corto di componenti? Armaioli, venditori di armature e sarti riparano il loro tipo di equipaggiamento in cambio di tappi. Più il negozio è fornito, più la riparazione si avvicina al nuovo." },
			{ "es", "¿Te faltan componentes? Armeros, vendedores de armaduras y sastres reparan su tipo de equipo a cambio de chapas. Cuanto mejor surtida esté la tienda, más se acercará a quedar como nuevo." },
			{ "esmx", "¿Te faltan componentes? Armeros, vendedores de armaduras y sastres reparan su tipo de equipo a cambio de tapas. Cuanto mejor surtida esté la tienda, más se acercará a quedar como nuevo." },
			{ "ptbr", "Sem componentes? Armeiros, vendedores de armaduras e alfaiates consertam o seu tipo de equipamento por tampas. Quanto mais sortida a loja, mais perto de novo eles conseguem deixá-lo." },
			{ "pl", "Brakuje ci komponentów? Rusznikarze, płatnerze i krawcy naprawiają za kapsle sprzęt, którym handlują. Im lepiej zaopatrzony sklep, tym bardziej zbliży sprzęt do stanu fabrycznego." },
			{ "ru", "Не хватает компонентов? Оружейники, бронники и портные за крышки починят вам снаряжение своего профиля. Чем богаче ассортимент магазина, тем ближе к новому будет результат." },
			{ "ja", "部品が足りない？ 武器屋やアーマー屋、服屋は、扱っている種類の装備をキャップで修理してくれる。品揃えが豊富な店ほど、新品に近い状態まで直せる" },
			{ "zhhant", "元件不夠用嗎？武器店、裝甲店和服飾店可以收取瓶蓋，修理自家經營的那類裝備。店裡貨色越齊全，就能修得越接近全新。" },
			{ "zhhans", "元件不够用吗？武器店、装甲店和服饰店可以收取瓶盖，修理自家经营的那类装备。店里货色越齐全，就能修得越接近全新。" },
		};

		// What an item at 0 does, see BrokenEquip.h.
		constexpr Line BROKEN[]{
			{ "en", "A weapon that breaks is put away. Broken armor stays on, bonuses and all, until you take it off. Either way, broken gear can't be equipped again until it's repaired." },
			{ "fr", "Une arme qui se brise est rangée. Une armure brisée reste portée, bonus compris, jusqu'à ce que vous l'enleviez. Dans les deux cas, un équipement brisé ne peut plus être équipé avant d'être réparé." },
			{ "de", "Eine Waffe, die kaputtgeht, wird weggesteckt. Kaputte Rüstung bleibt samt Boni angelegt, bis du sie ablegst. So oder so lässt sich kaputte Ausrüstung erst nach einer Reparatur wieder anlegen." },
			{ "it", "Un'arma che si rompe viene riposta. Un'armatura rotta resta indossata, bonus compresi, finché non la togli. In ogni caso, gli oggetti rotti non possono essere equipaggiati di nuovo finché non vengono riparati." },
			{ "es", "Un arma que se rompe se guarda. Una armadura rota sigue puesta, con sus bonificaciones, hasta que te la quites. En cualquier caso, el equipo roto no se puede volver a equipar hasta que se repare." },
			{ "esmx", "Un arma que se rompe se guarda. Una armadura rota sigue puesta, con sus bonificaciones, hasta que te la quites. En cualquier caso, el equipo roto no se puede volver a equipar hasta que se repare." },
			{ "ptbr", "Uma arma que quebra é guardada. Uma armadura quebrada continua vestida, com todos os bônus, até você tirá-la. De qualquer forma, equipamentos quebrados só podem ser equipados de novo depois de consertados." },
			{ "pl", "Broń, która się zepsuje, zostaje schowana. Zepsuty pancerz pozostaje na tobie wraz z premiami, dopóki go nie zdejmiesz. Tak czy inaczej, w zepsuty sprzęt wyposażysz się ponownie dopiero po naprawie." },
			{ "ru", "Сломавшееся оружие убирается. Сломанная броня остается на вас вместе со всеми бонусами, пока вы ее не снимете. В любом случае сломанное снаряжение нельзя снова взять в руки или надеть, пока его не починят." },
			{ "ja", "壊れた武器は自動的にしまわれる。壊れたアーマーは脱ぐまで身に着けたままで、ボーナスもそのまま残る。どちらにしても、壊れた装備は修理するまで再び装備できない" },
			{ "zhhant", "損壞的武器會被收起。損壞的裝甲會連同加成效果一直穿在身上，直到你脫下為止。無論哪種情況，損壞的裝備都必須修理後才能再次裝備。" },
			{ "zhhans", "损坏的武器会被收起。损坏的装甲会连同加成效果一直穿在身上，直到你脱下为止。无论哪种情况，损坏的装备都必须修理后才能再次装备。" },
		};

		// Jams, see Jam.h.
		constexpr Line JAM[]{
			{ "en", "Guns below half condition can jam, forcing a reload at the worst possible moment. Keep the weapons you rely on in good repair." },
			{ "fr", "Sous la moitié de leur état, les armes à feu peuvent s'enrayer et vous forcer à recharger au pire moment. Gardez en bon état les armes sur lesquelles vous comptez." },
			{ "de", "Schusswaffen, deren Zustand unter die Hälfte fällt, können Ladehemmungen bekommen und dich im ungünstigsten Moment zum Nachladen zwingen. Halte die Waffen, auf die du dich verlässt, gut in Schuss." },
			{ "it", "Le armi da fuoco in condizioni inferiori alla metà possono incepparsi e costringerti a ricaricare nel momento peggiore. Tieni in buone condizioni le armi su cui fai affidamento." },
			{ "es", "Las armas de fuego con el estado por debajo de la mitad pueden encasquillarse y obligarte a recargar en el peor momento. Mantén en buen estado las armas de las que dependes." },
			{ "esmx", "Las armas de fuego con el estado por debajo de la mitad pueden encasquillarse y obligarte a recargar en el peor momento. Mantén en buen estado las armas de las que dependes." },
			{ "ptbr", "Armas de fogo com a condição abaixo da metade podem emperrar e forçar uma recarga no pior momento possível. Mantenha em bom estado as armas em que você confia." },
			{ "pl", "Broń palna w stanie poniżej połowy może się zacinać, wymuszając przeładowanie w najgorszym możliwym momencie. Dbaj o dobry stan broni, na której polegasz." },
			{ "ru", "Стрелковое оружие в состоянии ниже половины может заклинить, и перезаряжаться придется в самый неподходящий момент. Следите за состоянием оружия, на которое вы полагаетесь." },
			{ "ja", "状態が半分を下回った銃は弾詰まりを起こすことがあり、最悪のタイミングでリロードを強いられる。頼りにしている武器は良い状態に保っておこう" },
			{ "zhhant", "狀況低於一半的槍枝可能會卡彈，讓你在最糟的時機被迫重新裝填。記得讓你倚賴的武器保持良好狀況。" },
			{ "zhhans", "状况低于一半的枪支可能会卡弹，让你在最糟的时机被迫重新装填。记得让你依赖的武器保持良好状况。" },
		};

		// The condition loot spawns at, see Provenance.h. The factions are the
		// vanilla creature screens' own words.
		constexpr Line LOOT[]{
			{ "en", "Loot rarely turns up in mint condition. Raiders are hard on their gear, while well equipped factions like the Gunners look after theirs." },
			{ "fr", "Le butin est rarement en parfait état. Les pillards malmènent leur équipement, alors que les factions bien équipées comme les Artilleurs prennent soin du leur." },
			{ "de", "Beute ist selten in einwandfreiem Zustand. Raider gehen grob mit ihrer Ausrüstung um, während gut ausgerüstete Fraktionen wie die Gunner ihre pflegen." },
			{ "it", "Il bottino è raramente in condizioni perfette. I predoni maltrattano il loro equipaggiamento, mentre le fazioni ben equipaggiate come i Gunner ne hanno cura." },
			{ "es", "El botín rara vez está en perfecto estado. Los saqueadores maltratan su equipo, mientras que las facciones bien equipadas, como los Artilleros, cuidan el suyo." },
			{ "esmx", "El botín rara vez está en perfecto estado. Los saqueadores maltratan su equipo, mientras que las facciones bien equipadas, como los Artilleros, cuidan el suyo." },
			{ "ptbr", "O saque raramente está em perfeito estado. Os invasores maltratam o próprio equipamento, enquanto facções bem equipadas, como os Atiradores, cuidam do seu." },
			{ "pl", "Łupy rzadko trafiają się w idealnym stanie. Bandyci niszczą swój sprzęt, za to dobrze wyposażone frakcje, takie jak Strzelcy, dbają o swój." },
			{ "ru", "Трофеи редко попадаются в идеальном состоянии. Рейдеры не берегут свое снаряжение, а хорошо экипированные группировки вроде стрелков за своим следят." },
			{ "ja", "戦利品が完璧な状態で見つかることはめったにない。レイダーは装備を雑に扱うが、ガンナーのような装備の整った勢力は手入れを怠らない" },
			{ "zhhant", "戰利品很少是完好如新的狀況。掠奪者不愛惜裝備，而像「槍手」這樣裝備精良的派系則會好好保養。" },
			{ "zhhans", "战利品很少是完好如新的状况。掠夺者不爱惜装备，而像“枪手”这样装备精良的派系则会好好保养。" },
		};
	}

	std::string WearTip()
	{
		return Pick(WEAR);
	}

	std::string BenchTip()
	{
		return Pick(BENCH);
	}

	std::string TraderTip()
	{
		return Pick(TRADER);
	}

	std::string BrokenTip()
	{
		return Pick(BROKEN);
	}

	std::string JamTip()
	{
		return Pick(JAM);
	}

	std::string LootTip()
	{
		return Pick(LOOT);
	}
}
