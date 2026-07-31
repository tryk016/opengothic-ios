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

## Etap 2 — **PRZEBUDOWANY**: nakładanie CPU/GPU, potem tańsza geometria

Pierwotny Etap 2 (jedna komenda indeksowana na meshlet) jest **martwy**:
`[caps] multiDrawIndirect=0 ... maxDrawIndirectCount=1` na Mali-G57. Sześć
patchy było gotowych i zweryfikowanych; bramka 2.2 je zatrzymała. Kolejność
poniżej idzie za zmierzonym zyskiem, nie za elegancją.

### 2A. Nakładanie CPU/GPU (~14 ms, zero kosztu jakości) — **najwyższy priorytet**

Klatka to dokładnie `CPU 14 + present 51 = 65`. `VSwapchain::present()` kończy
się zachłannym `acquireNextImage()`, który blokuje na dwóch fence'ach i akwizycji,
więc pętla gry stoi zamiast liczyć następną klatkę.

- [ ] **2A.1** Przenieść `acquireNextImage()` z końca `present()` na początek
  następnej klatki (tuż przed enkodowaniem), patchem w `apply-patches.sh`.
- [ ] **2A.2** Zweryfikować poprawność: `fence_miss` przestanie być zerem —
  to oczekiwane i pożądane. Sprawdzić brak artefaktów i brak warstw walidacyjnych
  skarżących się na czas życia semaforów.
- [ ] **2A.3** Pomiar. Oczekiwanie: 65 → ~51 ms. **Bramka:** jeśli zysk < 5 ms,
  model jest zły — wycofać patch, nie kombinować dalej.

Ryzyko: to dotyka synchronizacji swapchaina, czyli najłatwiejszego miejsca na
zawieszenie lub artefakty. Robimy to jako osobny commit, łatwy do wycofania.

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
