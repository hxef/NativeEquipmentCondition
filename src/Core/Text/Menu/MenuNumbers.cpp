#include "Core/Text/Lines.h"

#include "Core/Settings.h"

// The names and help lines of the Balance numbers on the MCM page, see
// MenuLine in Text.h. A help line says what the number does, with a value or 2
// to picture it.
namespace Text
{
	namespace
	{
		constexpr Line WEAR_RATE_NAME[]{
			{ "en", "Weapon wear speed" },
			{ "fr", "Vitesse d'usure des armes" },
			{ "de", "Abnutzungstempo von Waffen" },
			{ "it", "Velocità di usura delle armi" },
			{ "es", "Velocidad de desgaste de armas" },
			{ "esmx", "Velocidad de desgaste de armas" },
			{ "ptbr", "Velocidade de desgaste das armas" },
			{ "pl", "Szybkość zużycia broni" },
			{ "ru", "Скорость износа оружия" },
			{ "ja", "武器の消耗速度" },
			{ "zhhant", "武器損耗速度" },
			{ "zhhans", "武器损耗速度" },
		};

		constexpr Line WEAR_RATE_HELP[]{
			{ "en", "How fast weapons wear. 1 is normal, 2 is twice as fast and 0 stops wear." },
			{ "fr", "Vitesse d'usure des armes. 1 est normal, 2 est 2 fois plus rapide et 0 arrête l'usure." },
			{ "de", "Wie schnell sich Waffen abnutzen. 1 ist normal, 2 doppelt so schnell, 0 stoppt die Abnutzung." },
			{ "it", "Quanto in fretta si usurano le armi. 1 è normale, 2 è il doppio e 0 ferma l'usura." },
			{ "es", "Lo rápido que se desgastan las armas. 1 es normal, 2 es el doble de rápido y 0 detiene el desgaste." },
			{ "esmx", "Qué tan rápido se desgastan las armas. 1 es normal, 2 es el doble de rápido y 0 detiene el desgaste." },
			{ "ptbr", "Quão rápido as armas se desgastam. 1 é o normal, 2 é o dobro e 0 impede o desgaste." },
			{ "pl", "Jak szybko zużywa się broń. 1 to normalnie, 2 to 2 razy szybciej, a 0 wyłącza zużycie." },
			{ "ru", "Как быстро изнашивается оружие. 1 означает обычную скорость, 2 вдвое быстрее, а 0 останавливает износ." },
			{ "ja", "武器が消耗する速さです。1で通常、2で2倍の速さ、0で消耗しなくなります" },
			{ "zhhant", "武器損耗的速度。1為正常，2為2倍速度，0則停止損耗。" },
			{ "zhhans", "武器损耗的速度。1为正常，2为2倍速度，0则停止损耗。" },
		};

		constexpr Line ARMOR_WEAR_RATE_NAME[]{
			{ "en", "Armor wear speed" },
			{ "fr", "Vitesse d'usure des armures" },
			{ "de", "Abnutzungstempo von Rüstungen" },
			{ "it", "Velocità di usura delle armature" },
			{ "es", "Velocidad de desgaste de armaduras" },
			{ "esmx", "Velocidad de desgaste de armaduras" },
			{ "ptbr", "Velocidade de desgaste das armaduras" },
			{ "pl", "Szybkość zużycia pancerzy" },
			{ "ru", "Скорость износа брони" },
			{ "ja", "アーマーの消耗速度" },
			{ "zhhant", "裝甲損耗速度" },
			{ "zhhans", "装甲损耗速度" },
		};

		constexpr Line ARMOR_WEAR_RATE_HELP[]{
			{ "en", "How fast armor wears, on you and on NPCs. 1 is normal, 2 is twice as fast and 0 stops wear." },
			{ "fr", "Vitesse d'usure des armures, sur vous comme sur les PNJ. 1 est normal, 2 est 2 fois plus rapide et 0 arrête l'usure." },
			{ "de", "Wie schnell sich Rüstungen abnutzen, bei dir und bei NPCs. 1 ist normal, 2 doppelt so schnell, 0 stoppt die Abnutzung." },
			{ "it", "Quanto in fretta si usurano le armature, tue e dei PNG. 1 è normale, 2 è il doppio e 0 ferma l'usura." },
			{ "es", "Lo rápido que se desgastan las armaduras, en ti y en los demás. 1 es normal, 2 es el doble de rápido y 0 detiene el desgaste." },
			{ "esmx", "Qué tan rápido se desgastan las armaduras, en ti y en los demás. 1 es normal, 2 es el doble de rápido y 0 detiene el desgaste." },
			{ "ptbr", "Quão rápido as armaduras se desgastam, em você e nos NPCs. 1 é o normal, 2 é o dobro e 0 impede o desgaste." },
			{ "pl", "Jak szybko zużywają się pancerze, twoje i NPC. 1 to normalnie, 2 to 2 razy szybciej, a 0 wyłącza zużycie." },
			{ "ru", "Как быстро изнашивается броня, на вас и на NPC. 1 означает обычную скорость, 2 вдвое быстрее, а 0 останавливает износ." },
			{ "ja", "自分とNPCのアーマーが消耗する速さです。1で通常、2で2倍の速さ、0で消耗しなくなります" },
			{ "zhhant", "裝甲損耗的速度，包括你和其他角色的裝甲。1為正常，2為2倍速度，0則停止損耗。" },
			{ "zhhans", "装甲损耗的速度，包括你和其他角色的装甲。1为正常，2为2倍速度，0则停止损耗。" },
		};

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
			{ "fr", "Vitesse de perte de valeur des objets usés. À 1.5, un objet à moitié usé vaut 35\u00A0% de son prix. 0 garde le prix plein." },
			{ "de", "Wie schnell Abgenutztes an Wert verliert. Bei 1.5 ist ein halb abgenutzter Gegenstand 35\u00A0% wert. Bei 0 bleibt der volle Preis." },
			{ "it", "Quanto in fretta l'usura abbassa il valore. A 1.5, un oggetto a metà condizioni vale il 35%. 0 mantiene il prezzo pieno." },
			{ "es", "Lo rápido que pierden valor los objetos desgastados. Con 1.5, a mitad de estado valen el 35%. 0 mantiene el precio completo." },
			{ "esmx", "Qué tan rápido pierden valor los objetos desgastados. Con 1.5, a mitad de estado valen el 35%. 0 mantiene el precio completo." },
			{ "ptbr", "Quão rápido itens desgastados perdem valor. Com 1.5, um item com metade da condição vale 35%. 0 mantém o preço total." },
			{ "pl", "Jak szybko zużyte przedmioty tracą wartość. Przy 1.5 przedmiot zużyty w połowie jest wart 35%. 0 zachowuje pełną cenę." },
			{ "ru", "Как быстро дешевеют изношенные предметы. При 1.5 предмет, изношенный наполовину, стоит 35% цены. 0 сохраняет полную цену." },
			{ "ja", "消耗したアイテムが価値を失う速さです。1.5では状態が半分のアイテムは本来の価格の35%になり、0では値下がりしません" },
			{ "zhhant", "損耗物品失去價值的速度。設為1.5時，狀況一半的物品價值為原價的35%。設為0則保持原價。" },
			{ "zhhans", "损耗物品失去价值的速度。设为1.5时，状况一半的物品价值为原价的35%。设为0则保持原价。" },
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
			{ "ja", "状態0のフルオートの武器に残る発射速度の割合です。1にすると、消耗しても遅くなりません" },
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
			{ "en", "How many criticals a weapon keeps at 0 condition, for your meter and for NPCs. At 1, wear never lowers them." },
			{ "fr", "Part des critiques qu'une arme garde à l'état 0, pour votre jauge et pour les PNJ. À 1, l'usure ne les réduit jamais." },
			{ "de", "Wie viele kritische Treffer eine Waffe bei Zustand 0 behält, für deinen Krit-Balken und für NPCs. Bei 1 bleiben alle." },
			{ "it", "Quanti critici conserva un'arma a condizioni 0, per la tua barra e per i PNG. A 1, l'usura non li riduce mai." },
			{ "es", "Cuántos críticos conserva un arma con estado 0, para ti y para los demás. Con 1, el desgaste nunca los reduce." },
			{ "esmx", "Cuántos críticos conserva un arma con estado 0, para ti y para los demás. Con 1, el desgaste nunca los reduce." },
			{ "ptbr", "Quantos críticos uma arma mantém com condição 0, na sua barra e para NPCs. Com 1, o desgaste nunca os reduz." },
			{ "pl", "Ile trafień krytycznych zachowuje broń w stanie 0, w twoim wskaźniku i u NPC. Przy 1 zużycie nigdy ich nie obniża." },
			{ "ru", "Сколько критических атак сохраняет оружие в состоянии 0, для вашей шкалы КРИТ и для NPC. При 1 износ их не снижает." },
			{ "ja", "状態0の武器に残るクリティカルの割合で、自分のメーターとNPCの両方に適用されます。1にすると、消耗しても減りません" },
			{ "zhhant", "武器在狀況為0時保留多少爆擊，對你的爆擊條和其他角色都適用。設為1時，損耗不會減少爆擊。" },
			{ "zhhans", "武器在状况为0时保留多少爆击，对你的爆击条和其他角色都适用。设为1时，损耗不会减少爆击。" },
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
			{ "en", "How many components a workbench repair takes. 0.5 halves the cost, 2 doubles it and 0 makes every repair free." },
			{ "fr", "Le nombre de composants que demande une réparation à l'établi. 0.5 divise le coût par 2, 2 le double et 0 rend toute réparation gratuite." },
			{ "de", "Wie viele Komponenten eine Reparatur an der Werkbank braucht. 0.5 halbiert die Kosten, 2 verdoppelt sie, 0 macht jede Reparatur kostenlos." },
			{ "it", "Quanti componenti richiede una riparazione al banco da lavoro. 0.5 dimezza il costo, 2 lo raddoppia e 0 rende gratis ogni riparazione." },
			{ "es", "Cuántos componentes necesita una reparación en el banco de trabajo. 0.5 reduce el coste a la mitad, 2 lo duplica y 0 hace gratis toda reparación." },
			{ "esmx", "Cuántos componentes necesita una reparación en el banco de trabajo. 0.5 reduce el costo a la mitad, 2 lo duplica y 0 hace gratis toda reparación." },
			{ "ptbr", "Quantos componentes um conserto na bancada exige. 0.5 corta o custo pela metade, 2 o dobra e 0 deixa todo conserto grátis." },
			{ "pl", "Ilu komponentów wymaga naprawa w pracowni. 0.5 zmniejsza koszt o połowę, 2 go podwaja, a 0 sprawia, że każda naprawa jest darmowa." },
			{ "ru", "Сколько компонентов нужно для ремонта на верстаке. 0.5 вдвое снижает стоимость, 2 удваивает ее, а 0 делает любой ремонт бесплатным." },
			{ "ja", "作業台での修理に必要な部品の量です。0.5で半分、2で2倍、0ですべての修理が無料になります" },
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
			{ "en", "Above this condition, MEND at a workbench puts an item right for free. 100 turns that off and 0 mends everything but a broken item." },
			{ "fr", "Au-dessus de cet état, ENTRETENIR à l'établi remet un objet en état gratuitement. 100 désactive cela et 0 entretient tout sauf un objet cassé." },
			{ "de", "Über diesem Zustand bessert AUSBESSERN an der Werkbank einen Gegenstand kostenlos aus. 100 schaltet das ab, 0 bessert alles außer Kaputtem aus." },
			{ "it", "Sopra queste condizioni, AGGIUSTA al banco da lavoro sistema un oggetto gratis. 100 lo disattiva e 0 aggiusta tutto tranne un oggetto rotto." },
			{ "es", "Por encima de este estado, RETOCAR en el banco de trabajo arregla un objeto gratis. 100 lo desactiva y 0 retoca todo salvo un objeto roto." },
			{ "esmx", "Por encima de este estado, RETOCAR en el banco de trabajo arregla un objeto gratis. 100 lo desactiva y 0 retoca todo salvo un objeto roto." },
			{ "ptbr", "Acima desta condição, RETOCAR na bancada conserta um item de graça. 100 desativa isso e 0 retoca tudo menos um item quebrado." },
			{ "pl", "Powyżej tego stanu POPRAW w pracowni naprawia przedmiot za darmo. 100 to wyłącza, a 0 poprawia wszystko poza zepsutym przedmiotem." },
			{ "ru", "Выше этого состояния ПОДПРАВИТЬ на верстаке чинит предмет бесплатно. 100 отключает это, а 0 подправляет все, кроме сломанного предмета." },
			{ "ja", "この状態より上のアイテムは、作業台の手入れで無料で直せます。100で無料の手入れはなくなり、0で壊れたもの以外はすべて手入れできます" },
			{ "zhhant", "狀況高於此值時，在工作台修補物品不需任何花費。設為100則不再免費修補，設為0則除了損壞的物品外都能修補。" },
			{ "zhhans", "状况高于此值时，在工作台修补物品不需任何花费。设为100则不再免费修补，设为0则除了损坏的物品外都能修补。" },
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
			{ &Settings::fWearRateMult, WEAR_RATE_NAME, WEAR_RATE_HELP },
			{ &Settings::fArmorWearRateMult, ARMOR_WEAR_RATE_NAME, ARMOR_WEAR_RATE_HELP },
			{ &Settings::fDamageFloor, DAMAGE_FLOOR_NAME, DAMAGE_FLOOR_HELP },
			{ &Settings::fArmorFloor, ARMOR_FLOOR_NAME, ARMOR_FLOOR_HELP },
			{ &Settings::fValueExponent, VALUE_EXPONENT_NAME, VALUE_EXPONENT_HELP },
			{ &Settings::fFireRateFloor, FIRE_RATE_FLOOR_NAME, FIRE_RATE_FLOOR_HELP },
			{ &Settings::fCritMeterFloor, CRIT_METER_FLOOR_NAME, CRIT_METER_FLOOR_HELP },
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
