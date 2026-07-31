# Plan naprawy budżetu klatki na Androidzie

**Cel:** 65.1 ms → 33 ms (30 FPS) na Tab A9 / Mali-G57 MC2, `scene=world`.
Trzeba zdjąć **~32 ms pracy GPU**. Dane wejściowe:
[raport z 2026-07-31](../reports/2026-07-31-frame-budget-decomposition.md).

**Zasady obowiązujące w całym planie:**
- Wszystko zostaje na branchu `android`. `master` i iOS nietykalne.
- `lib/Tempest` to submoduł — **nigdy** `git add`. Każda zmiana silnika idzie
  przez `android/patches/apply-patches.sh` jako idempotentny patch perlowy,
  zweryfikowany lokalnie na przypiętym commicie `61b58f71` **przed** pushem
  (jeden zły wzorzec = 14 minut CI w plecy).
- Każdy etap kończy się pomiarem w świecie (`scene=world`, komnata Xardasa,
  `frame_p50_ms`), nigdy w menu.
- Każdy etap ma bramkę: jeśli pomiar nie potwierdzi założenia, zatrzymujemy się
  i wracamy tutaj, zamiast iść dalej na wiarę.

---

## Etap 0 — infrastruktura pomiarowa ✅ ZROBIONE

`androidPrimCap`, `androidShadowSkip`, `androidPassSkip`, log `geometry:`,
naprawa 256-bajtowego bufora logu i rozbicie `PERF` na cztery linie.
Bez tego żaden z poniższych etapów nie ma jak być zweryfikowany.

---

## Etap 1 — domknąć rozbiór tych 51 ms (1 build, ~5 przebiegów)

Dziś przypisane: geometria 22 ms, siedem etapów 14 ms, **~15 ms nieprzypisane**.
Nie zaczynamy dużej przebudowy nie wiedząc, co jest w tych 15 ms.

- [ ] **1.1** Dodać bit `PS_Sky` do `PassSkip` obejmujący `prepareSky` +
  `drawSky`. To jedyny duży blok pracy **niezależnej od rozdzielczości ekranu**:
  `skyViewCldLut` to 512×256 RGBA32F liczone co klatkę, `skyViewLut` 128×64,
  a `fogLut3D` 160×90×64 fraksele. W komnacie Xardasa nieba w ogóle nie widać,
  a płacimy pełną stawkę. To najlepszy kandydat na nieprzypisane 15 ms.
- [ ] **1.2** Zmierzyć pojedynczo pozostałe bity: `lights` (8), `translucent` (64),
  `hiz` (1), `shadowResolve` (32). `hiz` jest szczególny — jego pominięcie
  wyłącza też culling okluzyjny, więc wynik może być **ujemny**; to sama w sobie
  odpowiedź na pytanie, czy HiZ się na mobile opłaca.
- [ ] **1.3** Zaktualizować tabelę w raporcie.

**Bramka:** suma przypisanych milisekund pokrywa 51 ms GPU z dokładnością ±5 ms.
Jeśli nie — szukamy dalej, zanim ruszymy Etap 2.

---

## Etap 2 — rysunek indeksowany (największa pojedyncza pozycja: 22 ms)

To jest ta „przebudowa", na którą jest zgoda. Zakres jest jednak węższy niż
„połowa renderera": ścieżka mesh-shaderowa (desktop) zostaje nietknięta,
zmienia się wyłącznie wariant bez mesh shaderów — ten, którego używa Mali.
Zmiana jest też sensowna upstreamowo, bo dotyczy każdego GPU bez mesh shaderów.

Docelowy kształt: jedna komenda `VkDrawIndexedIndirectCommand` na widoczny
meshlet, `gl_DrawID` indeksuje skompaktowany payload, `gl_VertexIndex` adresuje
wierzchołek wprost. Z shadera znikają `processMeshlet` i `processPrimitive`.

- [ ] **2.1 Tempest: API `drawIndexedIndirect`.** Patch (i) w `apply-patches.sh`:
  `abstractgraphicsapi.h` (wirtualna **z domyślnym ciałem rzucającym wyjątek**,
  żeby Metal i DX12 zostały nietknięte — to jest gwarancja, że iOS nie ucierpi),
  `vcommandbuffer.h/.cpp` (`vkCmdBindIndexBuffer` + `vkCmdDrawIndexedIndirect`),
  `encoder.h/.cpp`. Każdy wzorzec perlowy sprawdzony na kopii pliku lokalnie.
- [ ] **2.2 Tempest: włączyć feature'y.** `multiDrawIndirect` i
  `shaderDrawParameters` (dla `gl_DrawID`) nie są dziś w ogóle żądane w
  `vdevice.cpp` — dwa miejsca konstrukcji `deviceFeatures` (linie ~186 i ~428).
  Zalogować, czy sterownik je udostępnia.
  **Bramka:** log na Tab A9 potwierdza oba. Jeśli nie — Etap 2 pada, wracamy tu.
- [ ] **2.3 Wymusić tryb slot na Androidzie** (`doBindless=false`). W trybie
  bindless IBO jest tablicą deskryptorów, a `vkCmdBindIndexBuffer` przyjmuje
  jeden bufor. Liczba draw calli rośnie z 23 do kilkuset — na kafelkowcu tanio.
  **Bramka:** sam ten krok nie może kosztować więcej niż ~2 ms. Zmierzyć osobno,
  przed jakąkolwiek zmianą shadera.
- [ ] **2.4 `visibility_pass.comp`** zapisuje obok wpisu payloadu komendę
  `{indexCount=192, instanceCount=1, firstIndex=meshletId*192, vertexOffset=0,
  firstInstance=0}`. Indeksy w `PackedMesh` są już globalne w obrębie bucketu,
  więc `firstIndex` liczy się wprost z `meshletId`. `cluster_init.comp` zeruje
  ogon.
- [ ] **2.5 `main.vert`:** payload z `gl_DrawID` zamiast `gl_InstanceIndex`,
  `pullVertex(bucketId, gl_VertexIndex)`, usunięcie `processMeshlet`
  i `processPrimitive`. Shader staje się **krótszy** — to nie jest dokładanie
  złożoności.
- [ ] **2.6 `DrawCommands`:** `drawCommon` i `drawHiZ` bindują `ibo` bucketu
  i wołają `drawIndexedIndirect(..., maxDrawCount=cx.maxPayload)`.
  **Bramka i sedno całego etapu:** zmierzyć. Oczekiwanie: 22 ms → 8–12 ms,
  czyli **−10 do −14 ms**. Jeśli wyjdzie mniej niż −6 ms, model jest zły
  i trzeba wrócić do raportu zamiast dokładać kolejne kroki.
- [ ] **2.7** Jeśli sterownik ma `drawIndirectCount` (Vulkan 1.2 core) — użyć go,
  żeby nie przemiatać wyzerowanych slotów. Osobny pomiar.
- [ ] **2.8** Przyciąć padding: `indexCount = primCount*3` zamiast stałych 192.
  Wymaga wystawienia `primCount` per meshlet passowi widoczności. Osobny pomiar —
  przy rysunku indeksowanym zdegenerowane trójkąty są tanie, więc ten krok może
  się nie opłacić i wtedy go **nie robimy**.

**Ryzyko:** to zmienia wszystkie shadery materiałowe, a A23 (Adreno 619) już dziś
wywala się w kompilatorze sterownika. Może pomóc, może zaszkodzić — Adreno i tak
jest osobnym, nierozwiązanym wątkiem i **nie blokuje** tego etapu.

---

## Etap 3 — etapy post i oświetlenia (14 ms) oraz niebo

Kolejność ustala Etap 1. Wiadomo już, że fog to 1.7 ms, a SSAO i CMAA2 ~0 —
czyli **nie ma tu jednego grubego winowajcy** i trzeba iść za liczbami z 1.2,
a nie za intuicją.

- [ ] **3.1** Jeśli 1.1 potwierdzi, że niebo to gruby blok: liczyć `skyViewCldLut`
  i `fogLut3D` rzadziej niż co klatkę (zmieniają się z porą dnia, nie z kamerą)
  albo w niższej rozdzielczości na mobile. To praca niezależna od rozdzielczości
  ekranu, więc jest czysto zyskowna.
- [ ] **3.2** Odpuścić kaskadę cieni tylko jeśli inne rzeczy zawiodą — to
  zmierzone **3 ms** i widoczna strata jakości. Nisko na liście.

---

## Etap 4 — CPU: tick NPC (~14 ms, staje się istotny po Etapie 2)

Dziś CPU **nie** jest limiterem (wątek główny 21%, `present` 51 ms). Ale przy
celu 33 ms obecne ~14 ms CPU to 42% budżetu, więc ten etap jest warunkiem
*utrzymania* 30 FPS, nie *osiągnięcia* go. Robimy go **po** Etapie 2.

- [ ] **4.1** Wprowadzić próg odległości/polityki na wejściu `Npc::tick`,
  analogicznie do istniejącego cullingu animacji. Dziś `WorldObjects::tick`
  woła `npc.tick()` dla wszystkich **1053** NPC szeregowo, a jedyny fast-path
  siedzi głęboko w `tickRoutine`. Uwaga: to zmienia symulację świata, nie tylko
  rendering — `AiFar2` musi nadal wykonywać rutyny i przemieszczenia, inaczej
  NPC-e „zamarzną" poza zasięgiem. Krok ostrożny, z osobnym testem rozgrywki.
- [ ] **4.2** Rozważyć zrównoleglenie pętli (`Workers::parallelTasks`, tak jak
  już zrobiono z animacją). Helio G99 ma 8 rdzeni, z których 6 stoi bezczynnie.
  To bezpieczniejsze niż 4.1, bo nie zmienia semantyki — ale wymaga sprawdzenia,
  czy `Npc::tick` nie modyfikuje stanu współdzielonego.

---

## Czego świadomie **nie** robimy

- **Przepisania na forward.** Celuje w koszt fragmentowy i przepustowość,
  a zmierzone są bliskie zeru (ćwiartowanie rozdzielczości: 0 ms).
- **Redukcji rozdzielczości cieni.** Zmierzone ~1 ms.
- **Gonienia za „cienie kosztują 18 ms".** Ta liczba była błędna; realnie
  3 ms na kaskadę.
- **Merge'owania czegokolwiek do `tryk016/Tempest` branch `renderer-ios`** —
  ten fork jest wyłącznie pod iOS.
