# Plan naprawy budżetu klatki na Androidzie

**Cel:** 65.1 ms → 33 ms (30 FPS) na Tab A9 / Mali-G57 MC2, `scene=world`.
Trzeba zdjąć **~32 ms**. Patrz zastrzeżenie na końcu Etapu 1: samą optymalizacją
dochodzimy w okolice 45 ms, reszta wymaga cięcia treści. Dane wejściowe:
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

## Etap 1 — rozbiór 51 ms GPU ✅ ZROBIONE

Wynik (każdy etap mierzony osobno, baseline 65.1 ms):

| Pozycja | Koszt |
|---|---|
| geometria (wszystkie 4 przebiegi) | **~22 ms** |
| lights | 4.2 ms |
| translucent / woda / odbicia | 3.2 ms |
| per-klatkowe LUT-y nieba | 2.2 ms |
| fog | 1.7 ms |
| SSAO, CMAA2, shadow resolve, drawSky | ~0 każdy |
| HiZ | **−6.3 ms — zarabia na siebie** |

**Bramka zaliczona.** Suma zdejmowalnych pozycji (22 + 4.2 + 3.2 + 2.2 + 1.7
≈ 33 ms) plus podłoga 33.4 ms domyka baseline 65.1 ms. Nie ma nieprzypisanego
bloku — wcześniejsze „~15 ms" brało się z odejmowania skażonego odczytu HiZ.

**Konsekwencja dla celu.** Podłoga 33.4 ms *już jest* 30 FPS, a wszystko powyżej
niej to geometria plus te pięć pozycji. Żeby zejść do 33 ms trzeba by usunąć
niemal **całość** obu — czego zwykłą optymalizacją się nie da. Realistycznie:
2A daje ~14 ms, 2B może kilka, Etap 3 kilka. To prowadzi do okolic 45 ms
(~22 FPS). **Stabilne 30 FPS w tej scenie i przy tych ustawieniach wymaga
dodatkowo cięcia treści** (zasięg rysowania, liczba kaskad, ustawienia) —
i to trzeba powiedzieć wprost, zamiast obiecywać 30 FPS z samej optymalizacji.

## Etap 2 — **PRZEBUDOWANY**: nakładanie CPU/GPU, potem tańsza geometria

Pierwotny Etap 2 (jedna komenda indeksowana na meshlet) jest **martwy**:
`[caps] multiDrawIndirect=0 ... maxDrawIndirectCount=1` na Mali-G57. Sześć
patchy było gotowych i zweryfikowanych; bramka 2.2 je zatrzymała. Kolejność
poniżej idzie za zmierzonym zyskiem, nie za elegancją.

### 2A. Nakładanie CPU/GPU ❌ SPRAWDZONE I ODRZUCONE

Hipoteza o braku nakładania CPU/GPU i ~14 ms do wzięcia **nie potwierdziła się**:
odroczenie akwizycji swapchaina dało `frame_p50 = 65.2` przy baseline 65.1,
a `fence_miss` pozostał zerowy. Patch wycofany. Szczegóły i błąd metodologiczny
(sumowanie p95 i porównywanie do p50) opisane w raporcie, §5a.

Nie wracamy tu bez nowego, twardszego dowodu.

### 2B. Tańszy wierzchołek (część z 22 ms, bez zmian w Tempeście)

Rysunek indeksowany odpada, ale koszt **per wywołanie** vertex shadera zostaje
do wzięcia. Dziś `pullVertex` to 9 osobnych skalarnych odczytów `float` z SSBO,
plus payload, nagłówek meshletu i indeks — przy 192 wywołaniach na meshlet
i czterech przebiegach.

- [ ] **2B.1** Przepakować `Vertex` z 9 floatów (36 B) na 16–24 B: pozycja
  `vec3`, normalna spakowana (oct16 lub 10:10:10), uv jako half2, kolor rgba8.
  Odczyty jako `uvec4` zamiast skalarów: **9 odczytów → 2**.
- [ ] **2B.2** Osobny, kompaktowy strumień **tylko pozycji** dla przebiegów
  głębokościowych. To **trzy z czterech** przebiegów (HiZ, Shadow0, Shadow1).
  Uwaga: dla `T_OBJ` pozycja zależy od normalnej przez `obj.fatness` — zapakować
  normalną w czwarty komponent zamiast czytać pełny wierzchołek.
- [ ] **2B.3** Pomiar po każdym kroku osobno.

### 2C. Czego **nie** robimy

Nie usuwamy HiZ — po poprawce pomiaru okazał się **wart 6.3 ms na plus**.
Nie próbujemy rysunku indeksowanego z cullingiem per-meshlet — sprzęt nie daje.

## Etap 3 — bramki widoczności (zatwierdzone przez użytkownika)

Zakres uzgodniony: **wolno ciąć zasięg widzenia, odbicia wody gdy wody nie
widać, oraz LUT-y nieba/mgły gdy nie widać chmur ani mgły.**

**Zasięg widzenia odpada od razu** — zmierzony 2026-07-20: 60 km → 20 km dało
**~0 ms**. Nie ma tam czego ciąć, mimo że zgoda jest. Nie wydajemy na to wysiłku.

### 3.1 Woda i odbicia (część z 3.2 ms)

Dwa konkretne marnotrawstwa, oba bezwarunkowe co klatkę:
- `Renderer::drawReflections` to **pass pełnoekranowy bez żadnej bramki** —
  wykonuje się nawet gdy na ekranie nie ma ani jednego piksela wody.
- `Renderer::drawGWater` ustawia framebuffer **czyszcząc `gbufDiffuse`
  i `gbufNormal` do (0,0,0,0)**. Na kafelkowcu to zrzut kafelków plus czyszczenie
  i zapis dwóch celów G-bufora — płacone także wtedy, gdy woda nic nie narysuje.

Mechanizm bez opóźnienia o klatkę (odczyt zwrotny dałby brakujące odbicia przez
całą klatkę przy wychodzeniu nad wodę — przy 15 FPS to 65 ms widocznego błędu):
- [ ] **3.1.1** Pass widoczności zapisuje do małego bufora flagę „widać wodę".
- [ ] **3.1.2** `drawReflections` przechodzi z `cmd.draw(nullptr,0,3)` na
  `drawIndirect` z `vertexCount` zapisywanym przez GPU (0 albo 3). `drawIndirect`
  z `drawCount=1` działa na Mali — to jedyna rzecz, której `multiDrawIndirect=0`
  nie blokuje.
- [ ] **3.1.3** Czyszczenie w `drawGWater` zostaje, ale cały pass warunkujemy
  tą samą flagą; tu opóźnienie o klatkę jest akceptowalne, bo brak passu wody
  oznacza po prostu brak wody do narysowania.
- [ ] **3.1.4** Pomiar osobno dla 3.1.2 i 3.1.3.

### 3.2 LUT-y nieba i mgły (2.2 + 1.7 ms)

`skyViewLut` (128×64) i `skyViewCldLut` (512×256 RGBA32F) liczą się co klatkę
niezależnie od rozdzielczości i od tego, czy niebo jest w kadrze. `fogLut3D` to
160×90×64 fraksele.

- [ ] **3.2.1** Sygnał „widać niebo": najtańszy to najgrubszy mip piramidy hiZ —
  jeśli maksymalna głębia na całym ekranie nie sięga płaszczyzny dalekiej, nieba
  nie widać. Piramida i tak jest budowana (HiZ zostaje, patrz 2C).
- [ ] **3.2.2** Uwaga na zależności: `prepareFog` próbkuje `transLut`
  i `multiScatLut`, a `drawReflections` próbkuje `viewCldLut`. Bramkujemy tylko
  **przeliczanie per-klatkę**, nigdy jednorazowe LUT-y — inaczej mgła dostanie
  niezainicjalizowane dane.
- [ ] **3.2.3** Pomiar we wnętrzu (komnata Xardasa) **i** na zewnątrz
  (Khorinis) — na zewnątrz zysk musi być zerowy, i to jest test poprawności
  bramki, nie tylko wydajności.

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
