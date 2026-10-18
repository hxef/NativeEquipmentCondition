#include "Core/Text/Lines.h"

#include "Core/Settings.h"

// The names and help lines of the Balance numbers on the MCM page after the
// wear speeds, which are in MenuWear.cpp, see MenuLine in Text.h. A help line
// says what the number does, with a value or 2 to picture it.
namespace Text
{
	namespace
	{
		constexpr Line DAMAGE_FLOOR_NAME[]{
			{ "en", "Damage at 0 condition" },
			{ "fr", "Dégâts à l'état 0" },
			{ "de", "Schaden bei Zustand 0" },
			{ "it", "Danni a condizioni 0" },
			{ "es", "Daño con estado 0" },
			{ "esmx", "Daño con estado 0" },
			{ "ptbr", "Dano com condição 0" },
			{ "pl", "Obrażenia przy stanie 0" },
			{ "ru", "Урон при состоянии 0" },
			{ "ja", "状態0でのダメージ" },
			{ "zhhant", "狀況為0時的傷害" },
			{ "zhhans", "状况为0时的伤害" },
		};

		constexpr Line DAMAGE_FLOOR_HELP[]{
			{ "en", "How much damage a weapon keeps at 0 condition. At 1, wear never lowers it." },
			{ "fr", "Part des dégâts qu'une arme garde à l'état 0. À 1, l'usure ne les réduit jamais." },
			{ "de", "Wie viel Schaden eine Waffe bei Zustand 0 behält. Bei 1 senkt die Abnutzung ihn nie." },
			{ "it", "Quanti danni conserva un'arma a condizioni 0. A 1, l'usura non li riduce mai." },
			{ "es", "Cuánto daño conserva un arma con estado 0. Con 1, el desgaste nunca lo reduce." },
			{ "esmx", "Cuánto daño conserva un arma con estado 0. Con 1, el desgaste nunca lo reduce." },
			{ "ptbr", "Quanto dano uma arma mantém com condição 0. Com 1, o desgaste nunca o reduz." },
			{ "pl", "Ile obrażeń zachowuje broń w stanie 0. Przy 1 zużycie nigdy ich nie obniża." },
			{ "ru", "Сколько урона сохраняет оружие в состоянии 0. При 1 износ его не снижает." },
			{ "ja", "状態0の武器に残るダメージの割合です。1にすると、消耗してもダメージは下がりません" },
			{ "zhhant", "武器在狀況為0時保留多少傷害。設為1時，損耗不會降低傷害。" },
			{ "zhhans", "武器在状况为0时保留多少伤害。设为1时，损耗不会降低伤害。" },
		};

		constexpr Line WEAPON_START_NAME[]{
			{ "en", "Weapon penalties below" },
			{ "fr", "Malus des armes en dessous de" },
			{ "de", "Abzüge bei Waffen unter" },
			{ "it", "Penalità delle armi sotto" },
			{ "es", "Penalización de armas por debajo de" },
			{ "esmx", "Penalización de armas por debajo de" },
			{ "ptbr", "Penalidades das armas abaixo de" },
			{ "pl", "Kary dla broni poniżej" },
			{ "ru", "Штрафы оружия ниже" },
			{ "ja", "武器のペナルティが始まる状態" },
			{ "zhhant", "武器懲罰開始的狀況" },
			{ "zhhans", "武器惩罚开始的状况" },
		};

		constexpr Line WEAPON_START_HELP[]{
			{ "en", "Below this condition, wear lowers weapon stats. At 0.5, a weapon works as new down to half condition." },
			{ "fr", "Sous cet état, l'usure affaiblit une arme. À 0.5, elle reste comme neuve jusqu'à la moitié de son état." },
			{ "de", "Unter diesem Zustand schwächt Abnutzung eine Waffe. Bei 0.5 wirkt sie bis zum halben Zustand wie neu." },
			{ "it", "Sotto queste condizioni, l'usura indebolisce un'arma. A 0.5, funziona come nuova fino a metà condizioni." },
			{ "es", "Bajo este estado, el desgaste debilita un arma. Con 0.5, funciona como nueva hasta la mitad de su estado." },
			{ "esmx", "Bajo este estado, el desgaste debilita un arma. Con 0.5, funciona como nueva hasta la mitad de su estado." },
			{ "ptbr", "Abaixo desta condição, o desgaste enfraquece a arma. Com 0.5, ela fica como nova até metade da condição." },
			{ "pl", "Poniżej tego stanu zużycie osłabia broń. Przy 0.5 broń działa jak nowa aż do połowy stanu." },
			{ "ru", "Ниже этого состояния износ ослабляет оружие. При 0.5 оружие как новое до половины состояния." },
			{ "ja", "この状態を下回ると、消耗で性能が下がります。0.5では半分の状態まで新品同様です" },
			{ "zhhant", "低於此狀況時，損耗會降低武器的性能。設為0.5時，武器在狀況降到一半前都如同全新。" },
			{ "zhhans", "低于此状况时，损耗会降低武器的性能。设为0.5时，武器在状况降到一半前都如同全新。" },
		};

		constexpr Line ARMOR_FLOOR_NAME[]{
			{ "en", "Protection at 0 condition" },
			{ "fr", "Protection à l'état 0" },
			{ "de", "Schutz bei Zustand 0" },
			{ "it", "Protezione a condizioni 0" },
			{ "es", "Protección con estado 0" },
			{ "esmx", "Protección con estado 0" },
			{ "ptbr", "Proteção com condição 0" },
			{ "pl", "Ochrona przy stanie 0" },
			{ "ru", "Защита при состоянии 0" },
			{ "ja", "状態0での防御力" },
			{ "zhhant", "狀況為0時的防護" },
			{ "zhhans", "状况为0时的防护" },
		};

		constexpr Line ARMOR_FLOOR_HELP[]{
			{ "en", "How much protection armor keeps at 0 condition. At 1, wear never lowers it." },
			{ "fr", "Part de la protection qu'une armure garde à l'état 0. À 1, l'usure ne la réduit jamais." },
			{ "de", "Wie viel Schutz eine Rüstung bei Zustand 0 behält. Bei 1 senkt die Abnutzung ihn nie." },
			{ "it", "Quanta protezione conserva un'armatura a condizioni 0. A 1, l'usura non la riduce mai." },
			{ "es", "Cuánta protección conserva una armadura con estado 0. Con 1, el desgaste nunca la reduce." },
			{ "esmx", "Cuánta protección conserva una armadura con estado 0. Con 1, el desgaste nunca la reduce." },
			{ "ptbr", "Quanta proteção uma armadura mantém com condição 0. Com 1, o desgaste nunca a reduz." },
			{ "pl", "Ile ochrony zachowuje pancerz w stanie 0. Przy 1 zużycie nigdy jej nie obniża." },
			{ "ru", "Сколько защиты сохраняет броня в состоянии 0. При 1 износ ее не снижает." },
			{ "ja", "状態0のアーマーに残る防御力の割合です。1にすると、消耗しても防御力は下がりません" },
			{ "zhhant", "裝甲在狀況為0時保留多少防護。設為1時，損耗不會降低防護。" },
			{ "zhhans", "装甲在状况为0时保留多少防护。设为1时，损耗不会降低防护。" },
		};

		constexpr Line ARMOR_START_NAME[]{
			{ "en", "Armor penalties below" },
			{ "fr", "Malus des armures en dessous de" },
			{ "de", "Abzüge bei Rüstung unter" },
			{ "it", "Penalità delle armature sotto" },
			{ "es", "Penalización de armaduras por debajo de" },
			{ "esmx", "Penalización de armaduras por debajo de" },
			{ "ptbr", "Penalidades das armaduras abaixo de" },
			{ "pl", "Kary dla pancerza poniżej" },
			{ "ru", "Штрафы брони ниже" },
			{ "ja", "アーマーのペナルティが始まる状態" },
			{ "zhhant", "裝甲懲罰開始的狀況" },
			{ "zhhans", "装甲惩罚开始的状况" },
		};

		constexpr Line ARMOR_START_HELP[]{
			{ "en", "Below this condition, wear lowers armor protection. At 0.5, armor protects as new down to half condition." },
			{ "fr", "Sous cet état, l'usure affaiblit l'armure. À 0.5, elle protège comme neuve jusqu'à la moitié de son état." },
			{ "de", "Unter diesem Zustand senkt Abnutzung den Schutz. Bei 0.5 bleibt er bis zum halben Zustand voll." },
			{ "it", "Sotto queste condizioni, l'usura indebolisce l'armatura. A 0.5, protegge come nuova fino a metà condizioni." },
			{ "es", "Bajo este estado, el desgaste debilita la armadura. Con 0.5, es como nueva hasta la mitad de su estado." },
			{ "esmx", "Bajo este estado, el desgaste debilita la armadura. Con 0.5, es como nueva hasta la mitad de su estado." },
			{ "ptbr", "Abaixo desta condição, o desgaste enfraquece a armadura. Com 0.5, fica como nova até meia condição." },
			{ "pl", "Poniżej tego stanu zużycie obniża ochronę pancerza. Przy 0.5 pancerz chroni jak nowy aż do połowy stanu." },
			{ "ru", "Ниже этого состояния износ ослабляет броню. При 0.5 броня как новая до половины состояния." },
			{ "ja", "この状態を下回ると、消耗で防御力が下がります。0.5では半分の状態まで新品同様です" },
			{ "zhhant", "低於此狀況時，損耗會降低裝甲的防護。設為0.5時，裝甲在狀況降到一半前都如同全新。" },
			{ "zhhans", "低于此状况时，损耗会降低装甲的防护。设为0.5时，装甲在状况降到一半前都如同全新。" },
		};

		constexpr Line VALUE_EXPONENT_NAME[]{
			{ "en", "Price drop when worn" },
			{ "fr", "Baisse de prix avec l'usure" },
			{ "de", "Preisverfall bei Abnutzung" },
			{ "it", "Prezzo ridotto dall'usura" },
			{ "es", "Pérdida de valor por desgaste" },
			{ "esmx", "Pérdida de valor por desgaste" },
			{ "ptbr", "Queda de preço com desgaste" },
			{ "pl", "Spadek ceny przy zużyciu" },
			{ "ru", "Падение цены при износе" },
			{ "ja", "消耗による値下がり" },
			{ "zhhant", "損耗降低價格" },
			{ "zhhans", "损耗降低价格" },
		};

		// 35% is what half condition sells for at 1.5, of the full price.
		constexpr Line VALUE_EXPONENT_HELP[]{
			{ "en", "How fast worn items lose value. At 1.5, an item at half condition is worth 35%. 0 keeps the full price." },
			{ "fr", "Vitesse de perte de valeur des objets usés. À 1.5, un objet à moitié usé vaut 35\u00A0%. 0 garde le prix plein." },
			{ "de", "Wie schnell Abgenutztes an Wert verliert. Bei 1.5 ist halb Abgenutztes 35\u00A0% wert, bei 0 bleibt der Preis." },
			{ "it", "Quanto in fretta l'usura abbassa il valore. A 1.5, a metà condizioni vale il 35%. 0 mantiene il prezzo pieno." },
			{ "es", "Lo rápido que se deprecia lo desgastado. Con 1.5, a mitad de estado vale el 35%. 0 mantiene el precio." },
			{ "esmx", "Qué tan rápido se deprecia lo desgastado. Con 1.5, a mitad de estado vale el 35%. 0 mantiene el precio." },
			{ "ptbr", "Quão rápido itens desgastados perdem valor. Com 1.5, a meia condição valem 35%. 0 mantém o preço." },
			{ "pl", "Jak szybko zużyte przedmioty tracą wartość. Przy 1.5 zużyte w połowie są warte 35%. 0 zachowuje cenę." },
			{ "ru", "Как быстро дешевеют изношенные предметы. При 1.5 полуизношенный стоит 35%. 0 сохраняет цену." },
			{ "ja", "消耗したアイテムの値下がりの速さです。1.5では状態半分で35%、0で下がりません" },
			{ "zhhant", "損耗物品失去價值的速度。設為1.5時，狀況一半的物品值原價35%。設為0則保持原價。" },
			{ "zhhans", "损耗物品失去价值的速度。设为1.5时，状况一半的物品值原价35%。设为0则保持原价。" },
		};

		constexpr Line FIRE_RATE_FLOOR_NAME[]{
			{ "en", "Fire rate at 0 condition" },
			{ "fr", "Cadence de tir à l'état 0" },
			{ "de", "Feuerrate bei Zustand 0" },
			{ "it", "Cadenza di fuoco a condizioni 0" },
			{ "es", "Cadencia de tiro con estado 0" },
			{ "esmx", "Cadencia de tiro con estado 0" },
			{ "ptbr", "Cadência de tiro com condição 0" },
			{ "pl", "Szybkostrzelność przy stanie 0" },
			{ "ru", "Скорострельность при состоянии 0" },
			{ "ja", "状態0での発射速度" },
			{ "zhhant", "狀況為0時的射速" },
			{ "zhhans", "状况为0时的射速" },
		};

		constexpr Line FIRE_RATE_FLOOR_HELP[]{
			{ "en", "How fast an automatic weapon still fires at 0 condition. At 1, wear never slows it." },
			{ "fr", "La cadence de tir qu'une arme automatique garde à l'état 0. À 1, l'usure ne la ralentit jamais." },
			{ "de", "Wie schnell eine automatische Waffe bei Zustand 0 noch feuert. Bei 1 bremst die Abnutzung sie nie." },
			{ "it", "Quanta cadenza di fuoco conserva un'arma automatica a condizioni 0. A 1, l'usura non la rallenta mai." },
			{ "es", "Lo rápido que sigue disparando un arma automática con estado 0. Con 1, el desgaste nunca la frena." },
			{ "esmx", "Qué tan rápido sigue disparando un arma automática con estado 0. Con 1, el desgaste nunca la frena." },
			{ "ptbr", "Quanta cadência de tiro uma arma automática mantém com condição 0. Com 1, o desgaste nunca a reduz." },
			{ "pl", "Jak szybko broń automatyczna strzela jeszcze w stanie 0. Przy 1 zużycie nigdy jej nie spowalnia." },
			{ "ru", "Как быстро еще стреляет автоматическое оружие в состоянии 0. При 1 износ его не замедляет." },
			{ "ja", "状態0のフルオート武器に残る発射速度の割合です。1で消耗しても遅くなりません" },
			{ "zhhant", "自動武器在狀況為0時仍保有多少射速。設為1時，損耗不會降低射速。" },
			{ "zhhans", "自动武器在状况为0时仍保有多少射速。设为1时，损耗不会降低射速。" },
		};

		constexpr Line CRIT_METER_FLOOR_NAME[]{
			{ "en", "Criticals at 0 condition" },
			{ "fr", "Coups critiques à l'état 0" },
			{ "de", "Kritische Treffer bei Zustand 0" },
			{ "it", "Critici a condizioni 0" },
			{ "es", "Críticos con estado 0" },
			{ "esmx", "Críticos con estado 0" },
			{ "ptbr", "Críticos com condição 0" },
			{ "pl", "Trafienia krytyczne przy stanie 0" },
			{ "ru", "Критические атаки при состоянии 0" },
			{ "ja", "状態0でのクリティカル" },
			{ "zhhant", "狀況為0時的爆擊" },
			{ "zhhans", "状况为0时的爆击" },
		};

		constexpr Line CRIT_METER_FLOOR_HELP[]{
			{ "en", "How many criticals a weapon keeps at 0 condition, for your meter and NPCs. At 1, wear never lowers them." },
			{ "fr", "Part des critiques qu'une arme garde à l'état 0, pour vous et les PNJ. À 1, l'usure ne les réduit jamais." },
			{ "de", "Kritische Treffer, die eine Waffe bei Zustand 0 behält, für deinen Balken und NPCs. Bei 1 bleiben alle." },
			{ "it", "Quanti critici conserva un'arma a condizioni 0, per la tua barra e per i PNG. A 1, l'usura non li riduce mai." },
			{ "es", "Cuántos críticos conserva un arma con estado 0, para ti y los demás. Con 1, el desgaste no los reduce." },
			{ "esmx", "Cuántos críticos conserva un arma con estado 0, para ti y los demás. Con 1, el desgaste no los reduce." },
			{ "ptbr", "Quantos críticos a arma mantém com condição 0, para você e NPCs. Com 1, o desgaste não os reduz." },
			{ "pl", "Ile trafień krytycznych zachowuje broń w stanie 0, u ciebie i u NPC. Przy 1 zużycie ich nie obniża." },
			{ "ru", "Доля критических атак при состоянии 0, у вас и у NPC. При 1 износ их не снижает." },
			{ "ja", "状態0の武器に残るクリティカルの割合で、自分とNPCに適用。1で消耗しても減りません" },
			{ "zhhant", "武器在狀況為0時保留多少爆擊，對你和其他角色都適用。設為1時，損耗不會減少爆擊。" },
			{ "zhhans", "武器在状况为0时保留多少爆击，对你和其他角色都适用。设为1时，损耗不会减少爆击。" },
		};

		constexpr Line ACCURACY_FLOOR_NAME[]{
			{ "en", "Accuracy at 0 condition" },
			{ "fr", "Précision à l'état 0" },
			{ "de", "Präzision bei Zustand 0" },
			{ "it", "Precisione a condizioni 0" },
			{ "es", "Precisión con estado 0" },
			{ "esmx", "Precisión con estado 0" },
			{ "ptbr", "Precisão com condição 0" },
			{ "pl", "Celność przy stanie 0" },
			{ "ru", "Меткость при состоянии 0" },
			{ "ja", "状態0での精度" },
			{ "zhhant", "狀況為0時的準度" },
			{ "zhhans", "状况为0时的准度" },
		};

		constexpr Line ACCURACY_FLOOR_HELP[]{
			{ "en", "How accurate a gun still is at 0 condition. At 0.75, its shots spread a third wider. At 1, wear never lowers it." },
			{ "fr", "Précision d'une arme à feu à l'état 0. À 0.75, les tirs s'écartent d'un tiers de plus. À 1, aucune perte." },
			{ "de", "Präzision einer Schusswaffe bei Zustand 0. Bei 0.75 streut sie ein Drittel weiter, bei 1 wie neu." },
			{ "it", "Precisione di un'arma da fuoco a condizioni 0. A 0.75, la rosa si allarga di un terzo. A 1, nessun calo." },
			{ "es", "Precisión de un arma de fuego con estado 0. Con 0.75, se dispersa un tercio más. Con 1, no baja." },
			{ "esmx", "Precisión de un arma de fuego con estado 0. Con 0.75, se dispersa un tercio más. Con 1, no baja." },
			{ "ptbr", "Precisão de uma arma de fogo com condição 0. Com 0.75, espalha um terço a mais. Com 1, não cai." },
			{ "pl", "Celność broni palnej w stanie 0. Przy 0.75 rozrzut rośnie o jedną trzecią. Przy 1 zużycie jej nie obniża." },
			{ "ru", "Меткость оружия в состоянии 0. При 0.75 разброс на треть шире. При 1 износ ее не снижает." },
			{ "ja", "状態0の銃に残る精度の割合です。0.75でばらつきが3分の1広がり、1で下がりません" },
			{ "zhhant", "槍枝在狀況為0時保有多少準度。設為0.75時散布擴大三分之一，設為1時不會降低。" },
			{ "zhhans", "枪支在状况为0时保有多少准度。设为0.75时散布扩大三分之一，设为1时不会降低。" },
		};

		constexpr Line RANGE_FLOOR_NAME[]{
			{ "en", "Range at 0 condition" },
			{ "fr", "Portée à l'état 0" },
			{ "de", "Reichweite bei Zustand 0" },
			{ "it", "Portata a condizioni 0" },
			{ "es", "Alcance con estado 0" },
			{ "esmx", "Alcance con estado 0" },
			{ "ptbr", "Alcance com condição 0" },
			{ "pl", "Zasięg przy stanie 0" },
			{ "ru", "Дальность при состоянии 0" },
			{ "ja", "状態0での射程" },
			{ "zhhant", "狀況為0時的射程" },
			{ "zhhans", "状况为0时的射程" },
		};

		constexpr Line RANGE_FLOOR_HELP[]{
			{ "en", "How far a gun keeps its damage at 0 condition. At 0, it hits as if from afar. At 1, wear never shortens it." },
			{ "fr", "Portée des dégâts d'une arme à feu à l'état 0. À 0, elle frappe comme de loin. À 1, l'usure ne la réduit pas." },
			{ "de", "Wie weit eine Schusswaffe bei Zustand 0 Schaden trägt. Bei 0 trifft sie wie aus der Ferne, bei 1 wie neu." },
			{ "it", "Fin dove un'arma da fuoco tiene i danni a condizioni 0. A 0 colpisce come da lontano, a 1 come nuova." },
			{ "es", "Alcance del daño de un arma de fuego con estado 0. Con 0, golpea como desde lejos. Con 1, no baja." },
			{ "esmx", "Alcance del daño de un arma de fuego con estado 0. Con 0, golpea como desde lejos. Con 1, no baja." },
			{ "ptbr", "Até onde uma arma de fogo mantém o dano com condição 0. Com 0, acerta como de longe. Com 1, não cai." },
			{ "pl", "Jak daleko broń palna w stanie 0 zachowuje obrażenia. Przy 0 trafia jak z daleka, przy 1 jak nowa." },
			{ "ru", "Как далеко оружие в состоянии 0 сохраняет урон. При 0 бьет как издалека, при 1 как новое." },
			{ "ja", "状態0の銃がダメージを保つ距離の割合です。0では常に遠距離扱い、1で短くなりません" },
			{ "zhhant", "槍枝在狀況為0時維持傷害的距離比例。設為0時都像遠距離射擊，設為1時不縮短。" },
			{ "zhhans", "枪支在状况为0时保持伤害的距离比例。设为0时都像远距离射击，设为1时不缩短。" },
		};

		constexpr Line BENCH_COST_NAME[]{
			{ "en", "Workbench repair cost" },
			{ "fr", "Coût des réparations à l'établi" },
			{ "de", "Reparaturkosten an der Werkbank" },
			{ "it", "Costo riparazioni al banco da lavoro" },
			{ "es", "Coste de reparación en el banco de trabajo" },
			{ "esmx", "Costo de reparación en el banco de trabajo" },
			{ "ptbr", "Custo de conserto na bancada" },
			{ "pl", "Koszt naprawy w pracowni" },
			{ "ru", "Стоимость ремонта на верстаке" },
			{ "ja", "作業台での修理コスト" },
			{ "zhhant", "工作台修理成本" },
			{ "zhhans", "工作台修理成本" },
		};

		constexpr Line BENCH_COST_HELP[]{
			{ "en", "How many components a workbench repair takes. 0.5 halves the cost, 2 doubles it, 0 makes it free." },
			{ "fr", "Composants qu'exige une réparation à l'établi. 0.5 divise le coût par 2, 2 le double et 0 la rend gratuite." },
			{ "de", "Komponenten je Werkbank-Reparatur. 0.5 halbiert die Kosten, 2 verdoppelt sie, 0 macht sie kostenlos." },
			{ "it", "Componenti richiesti da una riparazione al banco. 0.5 dimezza il costo, 2 lo raddoppia e 0 la rende gratis." },
			{ "es", "Componentes por reparación en el banco. 0.5 reduce el coste a la mitad, 2 lo duplica y 0 la hace gratis." },
			{ "esmx", "Componentes por reparación en el banco. 0.5 reduce el costo a la mitad, 2 lo duplica y 0 la hace gratis." },
			{ "ptbr", "Componentes por conserto na bancada. 0.5 corta o custo pela metade, 2 o dobra e 0 deixa grátis." },
			{ "pl", "Komponenty na naprawę w pracowni. 0.5 to połowa kosztu, 2 to podwójny, a 0 to darmowa naprawa." },
			{ "ru", "Компоненты для ремонта на верстаке. 0.5 вдвое дешевле, 2 вдвое дороже, а 0 бесплатно." },
			{ "ja", "作業台の修理に必要な部品の量です。0.5で半分、2で2倍、0で無料になります" },
			{ "zhhant", "在工作台修理所需的元件數量。0.5會使花費減半，2則加倍，0則讓所有修理免費。" },
			{ "zhhans", "在工作台修理所需的元件数量。0.5会使花费减半，2则加倍，0则让所有修理免费。" },
		};

		constexpr Line FREE_MEND_NAME[]{
			{ "en", "Free mend above" },
			{ "fr", "Entretien gratuit au-dessus de" },
			{ "de", "Kostenlos ausbessern über" },
			{ "it", "Aggiusta gratis sopra" },
			{ "es", "Retoque gratis por encima de" },
			{ "esmx", "Retoque gratis por encima de" },
			{ "ptbr", "Retoque grátis acima de" },
			{ "pl", "Darmowa poprawka powyżej" },
			{ "ru", "Бесплатно подправить выше" },
			{ "ja", "無料で手入れする状態" },
			{ "zhhant", "免費修補的狀況" },
			{ "zhhans", "免费修补的状况" },
		};

		// MEND is the bench's button for a free repair, in the words of
		// MEND_BUTTON in Repair.cpp.
		constexpr Line FREE_MEND_HELP[]{
			{ "en", "Above this condition, MEND at a workbench is free. 100 turns it off, 0 mends all but broken items." },
			{ "fr", "Au-dessus de cet état, ENTRETENIR à l'établi est gratuit. 100 désactive, 0 entretient tout sauf le cassé." },
			{ "de", "Über diesem Zustand ist AUSBESSERN an der Werkbank gratis. 100: aus, 0: alles außer Kaputtem." },
			{ "it", "Sopra queste condizioni, AGGIUSTA al banco è gratis. 100 lo disattiva e 0 aggiusta tutto tranne il rotto." },
			{ "es", "Sobre este estado, RETOCAR en el banco es gratis. 100 lo desactiva y 0 retoca todo salvo lo roto." },
			{ "esmx", "Sobre este estado, RETOCAR en el banco es gratis. 100 lo desactiva y 0 retoca todo salvo lo roto." },
			{ "ptbr", "Acima desta condição, RETOCAR na bancada é grátis. 100 desativa e 0 retoca tudo menos o quebrado." },
			{ "pl", "Powyżej tego stanu POPRAW jest darmowe. 100 to wyłącza, a 0 poprawia wszystko poza zepsutym." },
			{ "ru", "Выше этого состояния ПОДПРАВИТЬ бесплатно. 100 отключает, при 0 все, кроме сломанного." },
			{ "ja", "この状態より上は作業台の手入れが無料です。100で無効、0で壊れた物以外が対象です" },
			{ "zhhant", "狀況高於此值時，在工作台修補免費。設為100則取消，設為0則損壞物品以外皆可修補。" },
			{ "zhhans", "状况高于此值时，在工作台修补免费。设为100则取消，设为0则损坏物品以外皆可修补。" },
		};

		constexpr Line TRADER_PRICE_NAME[]{
			{ "en", "Trader repair price" },
			{ "fr", "Prix des réparations chez les marchands" },
			{ "de", "Reparaturpreis beim Händler" },
			{ "it", "Prezzo riparazioni dai commercianti" },
			{ "es", "Precio de reparación de comerciantes" },
			{ "esmx", "Precio de reparación de comerciantes" },
			{ "ptbr", "Preço de conserto com comerciantes" },
			{ "pl", "Cena naprawy u handlarzy" },
			{ "ru", "Цена ремонта у торговцев" },
			{ "ja", "商人の修理価格" },
			{ "zhhant", "商人修理價格" },
			{ "zhhans", "商人修理价格" },
		};

		constexpr Line TRADER_PRICE_HELP[]{
			{ "en", "How many caps a trader asks for a repair. 0.5 halves the price and 2 doubles it." },
			{ "fr", "Le nombre de capsules qu'un marchand demande pour une réparation. 0.5 divise le prix par 2 et 2 le double." },
			{ "de", "Wie viele Kronkorken ein Händler für eine Reparatur verlangt. 0.5 halbiert den Preis, 2 verdoppelt ihn." },
			{ "it", "Quanti tappi chiede un commerciante per una riparazione. 0.5 dimezza il prezzo e 2 lo raddoppia." },
			{ "es", "Cuántas chapas pide un comerciante por una reparación. 0.5 reduce el precio a la mitad y 2 lo duplica." },
			{ "esmx", "Cuántas tapas pide un comerciante por una reparación. 0.5 reduce el precio a la mitad y 2 lo duplica." },
			{ "ptbr", "Quantas tampas um comerciante cobra por um conserto. 0.5 corta o preço pela metade e 2 o dobra." },
			{ "pl", "Ile kapsli handlarz chce za naprawę. 0.5 zmniejsza cenę o połowę, a 2 ją podwaja." },
			{ "ru", "Сколько крышек торговец просит за ремонт. 0.5 вдвое снижает цену, а 2 удваивает ее." },
			{ "ja", "商人が修理に求めるキャップの額です。0.5で半分、2で2倍になります" },
			{ "zhhant", "商人修理所收取的瓶蓋數量。0.5會使價格減半，2則加倍。" },
			{ "zhhans", "商人修理所收取的瓶盖数量。0.5会使价格减半，2则加倍。" },
		};

		constexpr MenuRow ROWS[]{
			{ &Settings::fDamageFloor, DAMAGE_FLOOR_NAME, DAMAGE_FLOOR_HELP },
			{ &Settings::fWeaponPenaltyStart, WEAPON_START_NAME, WEAPON_START_HELP },
			{ &Settings::fArmorFloor, ARMOR_FLOOR_NAME, ARMOR_FLOOR_HELP },
			{ &Settings::fArmorPenaltyStart, ARMOR_START_NAME, ARMOR_START_HELP },
			{ &Settings::fValueExponent, VALUE_EXPONENT_NAME, VALUE_EXPONENT_HELP },
			{ &Settings::fFireRateFloor, FIRE_RATE_FLOOR_NAME, FIRE_RATE_FLOOR_HELP },
			{ &Settings::fCritMeterFloor, CRIT_METER_FLOOR_NAME, CRIT_METER_FLOOR_HELP },
			{ &Settings::fAccuracyFloor, ACCURACY_FLOOR_NAME, ACCURACY_FLOOR_HELP },
			{ &Settings::fRangeFloor, RANGE_FLOOR_NAME, RANGE_FLOOR_HELP },
			{ &Settings::fBenchCostMult, BENCH_COST_NAME, BENCH_COST_HELP },
			{ &Settings::iFreeMendAbove, FREE_MEND_NAME, FREE_MEND_HELP },
			{ &Settings::fTraderPriceMult, TRADER_PRICE_NAME, TRADER_PRICE_HELP },
		};
	}

	std::span<const MenuRow> MenuNumbers()
	{
		return ROWS;
	}
}
