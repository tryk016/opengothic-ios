# Rozbiór budżetu klatki — Tab A9 / Mali-G57 MC2 (2026-07-31)

Wszystkie liczby zmierzone na Samsung Tab A9 (`R83Y81NE23H`, Helio G99, Mali-G57 MC2),
`scene=world`, komnata Xardasa po nowej grze, profil `vidResIndex=2`,
`shadowResolution=512`, `sightValue=2`. Odczyt z `frame_p50_ms` linii `PERF`
oraz `present_p95_ms` linii `PERF-CPU`. Nic tutaj nie pochodzi z menu.

## 0. Zanim cokolwiek dało się zmierzyć: telemetria była ucięta

`Tempest::Log` formatuje do stałego bufora `Context::buffer[256]` i **milcząco**
obcina resztę — bez żadnego znacznika, linia po prostu się kończy. Linia `PERF`
ma ~600 znaków, więc **wszystko od `cpu_tick_p95_ms` w dół — cały rozkład CPU,
pamięć, liczniki NPC — nigdy nie dotarło do logcat ani do `log.txt`**.
Wcześniejsze sesje czytały linię, która wyglądała na kompletną i nie była.

Naprawione dwutorowo: `apply-patches.sh` (h) podnosi bufor do 2048,
a `mainwindow.cpp` rozbija telemetrię na `PERF` / `PERF-CPU` / `PERF-SYS`
plus jednorazowe `PERF-CFG`, żeby dane przetrwały też na niezałatanym silniku.

## 1. Pomiary

Narzędzia dodane w tej sesji (wszystkie domyślnie no-op, tylko do pomiarów):

| Klucz `[INTERNAL]` | Znaczenie |
|---|---|
| `androidPrimCap` | 1..64, skaluje `vertexCount` w `cluster_init.comp` — liniowo skaluje liczbę wywołań vertex shadera, nie ruszając liczby draw calli, bindingów ani passu widoczności |
| `androidShadowSkip` | liczba bliskich kaskad pozostawionych bez alokacji |
| `androidPassSkip` | bitmaska etapów do pominięcia (HiZ, SSAO, fog, lights, CMAA2, shadow resolve, translucent) |

| Konfiguracja | `frame_p50` | `present_p95` | Δ vs baseline |
|---|---|---|---|
| baseline | **65.1 ms** | 51.2 ms | — |
| `primCap=16` (¼ trójkątów) | 54.0 ms* | — | −10.4 |
| `primCap=1` (−98.4% trójkątów) | 42.5 ms* | — | **−22.0** |
| `shadowSkip=1` (jedna kaskada mniej) | 61.4 ms* | — | **−3.0** |
| `passSkip=127` (7 etapów) | 51.1 ms | 36.2 ms | **−14.0** |
| `passSkip=4` (tylko fog) | 63.4 ms | 49.3 ms | −1.7 |
| `passSkip=2` (tylko SSAO) | 65.5 ms | 50.1 ms | ~0 |
| `passSkip=16` (tylko CMAA2) | 65.0 ms | — | ~0 |
| `passSkip=64` (tylko translucent/woda/odbicia) | 61.9 ms | — | −3.2 |
| `passSkip=8` (tylko lights) | 60.9 ms | — | −4.2 |
| `passSkip=128` (per-klatkowe LUT-y nieba) | 63.1 ms | — | −2.2 |
| `passSkip=256` (drawSky) | 65.5 ms | — | ~0 |
| `passSkip=1` (HiZ, **po poprawce**) | 71.6 ms | — | **+6.3 (gorzej!)** |
| `primCap=1` + `passSkip=127` | **33.4 ms** | 3.6–7.9 ms | −31.7 |

\* zmierzone na poprzednim buildzie, którego baseline wynosił 64.4 ms (~1% niżej); porównywalne.

Stała scena: `drawCmd=23`, `meshletSlots=109568`, `clusters=44935`, `npc=1053`.

## 2. Jesteśmy GPU-bound — twardo

Dwa niezależne dowody:

- `PERF-CPU`: `tick=7.7` `anim=4.7` `pose=2.2` `encode=5.5` `submit=0.4`,
  **`present=51.2 ms`**. Praca CPU to ~14 ms z 65 ms klatki; reszta to
  czekanie na GPU.
- `top -H` na procesie w świecie: wątek główny **21.4%**, `mali-cmar-backend`
  14.2%, **600% z 800% bezczynne**.

W ostatnim wierszu tabeli `present` spada do ~4 ms i klatka ląduje na 33.4 ms
przy 29.6 FPS — czyli **na limicie 30 FPS**. Przy zdjętej geometrii i zdjętych
siedmiu etapach GPU ma zapas.

### Skąd bierze się te 30 FPS

Cap **jest nasz i siedzi w `Gothic.ini`**: `[ENGINE] zMaxFpsMode=1`, zasiewany
przez profil Androida (`gothic.cpp:146/167/230`). Na Androidzie
`fpsLimits[] = {0,30,60}` (`mainwindow.cpp:517`), a egzekucja to
`sleep_until(androidFrameStart + period)` w pętli klatki. Drugiego limitu nie ma.

**Ale zdjęcie go nic nie zmienia.** Przy `zMaxFpsMode=0` (potwierdzone w
`PERF-CFG`: `fps_limit=0`) podłoga to nadal **33.2–33.6 ms przy 29.6–29.9 FPS**;
zmienia się tylko to, gdzie stoimy: `present_p95` rośnie z ~4 ms do **19–20 ms**,
bo czekanie przenosi się ze `sleep_until` do prezentacji. 33.3 ms to dokładnie
**dwa okresy vsync na panelu 60 Hz**.

Wnioski:
- Pomiar podłogi **nie był zafałszowany** — 33.4 (z capem) vs 33.2 (bez) to ta
  sama liczba. Wniosek „trzeba zdjąć ~32 ms GPU" stoi.
- Przy celu 30 FPS cap jest **nieszkodliwy**; miałby znaczenie dopiero, gdybyśmy
  celowali w 60.
- Tym, co przy podłodze blokuje 60 FPS, jest **CPU (~14 ms)**, nie GPU —
  co czyni Etap 4 (tick NPC) warunkiem jakiegokolwiek przyszłego celu 60 FPS.

## 3. Gdzie idzie te 51 ms GPU

- **Geometria: ~22 ms.** Usunięcie 98.4% trójkątów zabiera 22 ms.
- **Siedem etapów post/oświetlenia: ~14 ms** łącznie. Rozkład wewnątrz jest
  płaski i **nie ma tu jednego grubego winowajcy**: lights 4.2 ms,
  translucent/woda/odbicia 3.2 ms, fog 1.7 ms, SSAO ~0, CMAA2 ~0. Pozostałe
  ~4.9 ms to HiZ i shadow resolve — do domknięcia w Etapie 1 planu.
  Wniosek praktyczny: tych 14 ms **nie da się odzyskać jednym cięciem**, każdy
  kawałek trzeba brać osobno i każdy kosztuje jakąś funkcję wizualną.
- **Reszta: ~15 ms** — G-buffer, ambient, niebo, prezentacja/kompozytor.

Cel 30 FPS to 33 ms. Trzeba zdjąć **~32 ms pracy GPU**. Geometria (22) plus
siedem etapów (14) to 36 ms, więc na stole *jest* dość — ale nie da się tego
wziąć z jednego miejsca i nie da się po prostu usunąć funkcji.

## 4. Dlaczego geometria kosztuje tyle — defekt strukturalny

Bez mesh shaderów (Mali ich nie ma) `main.vert` rysuje **każdy meshlet jako
nieindeksowaną instancję o `vertexCount = MaxPrim*3 = 192`**
(`cluster_init.comp:20`), a indeks i atrybuty wierzchołka wyciąga z SSBO
wewnątrz shadera:

```glsl
const uint laneID = gl_VertexIndex/3;                       // numer trójkąta
uint idx = processPrimitive(meshletId, bucketId, laneID)[gl_VertexIndex%3];
vec4 pos = processVertex(var, instanceId, meshletId, bucketId, idx);
```

Konsekwencje na kafelkowym GPU:

1. **3 wywołania vertex shadera na trójkąt, zero ponownego użycia wierzchołków.**
   Indeksowany rysunek daje ~0.6–0.9. To ~4× więcej wywołań.
2. **IDVS nie działa.** Mali (Bifrost/Valhall) dzieli VS na część pozycyjną
   i część varyingów i shaduje varyingi tylko dla wierzchołków przetrwałych
   trójkątów — ale **tylko dla rysunków indeksowanych**.
3. **Sprzętowy fetch atrybutów jest omijany** — `pullVertex` to 9 osobnych
   skalarnych odczytów `float` z SSBO na wierzchołek (23 dla skinningu),
   plus odczyt payloadu, nagłówka meshletu i indeksu.
4. **Padding jest opłacany w całości.** `vertexCount` to zawsze 192, niezależnie
   od tego, ile trójkątów meshlet naprawdę ma; `PackedMesh` dopycha też VBO do
   pełnych 64 wierzchołków.
5. Wszystko to **cztery razy na klatkę**: HiZ, G-buffer, Shadow0, Shadow1.

Kształt krzywej to potwierdza: 64→16 prymitywów kosztuje 0.22 ms/prymityw,
a 16→1 aż 0.77 ms/prymityw. Koszt jest silnie **per-prymityw** (binning,
pozycja, setup), nie per-piksel — co domyka wcześniejszą zagadkę, dlaczego
ćwiartowanie rozdzielczości nic nie dało.

Dobra wiadomość: **prawdziwy bufor indeksów już istnieje**
(`StaticMesh::ibo`, globalne 32-bitowe indeksy, budowany dla każdego mesha,
dziś używany wyłącznie do BLAS-ów ray query).

## 4a. HiZ: culling okluzyjny **zarabia na siebie** — nie ruszać

Pierwszy odczyt `passSkip=1` dał −8.7 ms i wyglądał na największą pojedynczą
dźwignię. Był **artefaktem**: pominięcie prepassu zostawiało piramidę hiZ ze
śmieciami, a główny pass widoczności dalej po niej cullingował, więc część tych
8.7 ms to było zwyczajne **gubienie widocznej geometrii**.

Po poprawce (brak prepassu ⇒ główny pass przełącza się na wariant tylko-frustum)
ten sam eksperyment daje **71.6 ms, czyli 6.3 ms *gorzej* od baseline**.
Wniosek się odwraca: **HiZ oszczędza ~6 ms netto** mimo że sam jest czwartym
przebiegiem geometrii. Zdjęcie go byłoby regresem.

To także realny bug, nie tylko rusztowanie pomiarowe — ścieżka pathtrace miała
dokładnie ten sam problem (nigdy nie buduje piramidy, a testowała po niej).

## 4b. `multiDrawIndirect=0` — Mali zamyka drogę do komend per-meshlet

```
[caps] multiDrawIndirect=0 drawIndirectFirstInstance=1 maxDrawIndirectCount=1
```

Mali-G57 **nie wspiera** `multiDrawIndirect`, a `maxDrawIndirectCount=1`.
Jedna komenda `VkDrawIndexedIndirectCommand` na widoczny meshlet — rdzeń
pierwotnego Etapu 2 — jest **niewykonalna na tym sprzęcie**. Sześć patchy
perlowych było gotowych i sprawdzonych; bramka je zatrzymała przed wdrożeniem.

Co z tego zostaje: `firstIndex` i `vertexOffset` są per-komenda, więc bez
`drawCount>1` nie da się dać każdemu meshletowi własnego zakresu indeksów przy
zachowaniu kompaktowania na GPU. Rysunek indeksowany z cullingiem per-meshlet
odpada; zostają tańsze warianty opisane w planie.

## 5. Korekty wcześniejszych wniosków

- **„Cienie ≈ −18 ms" było błędne.** Zdjęcie całej kaskady daje **3.0 ms**,
  a rozdzielczość cieni ~1 ms. Rekomendacja „jedna kaskada zamiast dwóch"
  z 2026-07-20 jest warta 3 ms, nie kilkanaście — wycofuję ją jako główną dźwignię.
- **„To jest geometria" trzeba doprecyzować.** Geometria to ~22 ms z 65 —
  największy pojedynczy blok, ale nie większość klatki. Zdanie z 07-31
  o dominacji geometrii było zbyt mocne.
- **Przebudowa na forward nadal nie ma sensu** — celuje w koszt fragmentowy
  i przepustowość, które zmierzone są bliskie zeru.

## 5a. Hipoteza „CPU i GPU się nie nakładają" — **zmierzona i obalona**

Wyglądało to na największe znalezisko sesji, więc zapisuję i wynik, i błąd,
który do niego doprowadził.

**Rozumowanie:** klatka rozkłada się pozornie dokładnie addytywnie —
`tick 7.7 + anim 4.7 + pose 2.2 + encode 5.5 + submit 0.4 + present 51.2 ≈ 65.1`.
Przy `MaxFramesInFlight=2` powinno być `max(CPU, GPU)`, nie suma. Winowajcą
miało być `VSwapchain::present()`, które kończyło się zachłannym
`acquireNextImage()` blokującym na dwóch fence'ach i akwizycji.

**Eksperyment:** patch `(j)` przeniósł akwizycję z końca `present()` do
pierwszego użycia indeksu obrazu, czyli za `tick`/`anim`/`pose`.

**Wynik:** `frame_p50 = 65.2 ms` przy baseline 65.1 — **zero zmiany**.
`present_p95` bez zmian (~50 ms), a `fence_miss` **dalej zerowy**, czyli
nakładanie w ogóle nie powstało. Blokada po prostu przeniosła się w obrębie
`present()` z `acquireNextImage()` do `vkQueuePresentKHR`, który w trybie FIFO
i tak czeka na zwolnienie obrazu. Patch wycofany zgodnie z zapowiedzianą bramką
(zysk < 5 ms ⇒ revert, nie strojenie).

**Błąd metodologiczny, który to napędził:** sumowałem wartości **p95**
poszczególnych faz i porównywałem je do **p50** klatki. To nie jest poprawne —
p95 sumy nie jest sumą p95. Zgodność 65.1 ≈ 65.1 była zbiegiem okoliczności,
a ja awansowałem ją na fakt („zero nakładania") zamiast na hipotezę do sprawdzenia.
Do dowodzenia braku nakładania trzeba mierzyć te same percentyle albo, lepiej,
korelację w obrębie pojedynczych klatek.

Co z tego zostaje: **nie ma dowodu na 14 ms do wzięcia z pipeliningu**, a fakt
że `present` pochłania ~50 ms z 65 ms klatki jest po prostu kolejnym sposobem
powiedzenia, że jesteśmy GPU-bound — co już wiedzieliśmy z `top -H`.

## 6. NPC: AI i ruch liczone dla całej wyspy

`WorldObjects::tick` (`worldobjects.cpp:177`) woła `npc.tick(d)` dla **każdego**
z **1053** NPC w świecie, szeregowo na wątku głównym. `Npc::tick` nie ma progu
odległości — `nextAiAction`, `tickRegen`, `implLookAtNpc`, `implGoTo`,
`mvAlgo.tick` (ruch i kolizje) wykonują się dla wszystkich. Jedyny fast-path
siedzi głębiej, w `tickRoutine`:
`const bool fastPath = (aiPolicy==AiFar2 && routines.empty()); //HACK`.

Co **jest** już okiełznane: `processPolicy` jest bramkowane odległością
(3000 / 6000 jednostek), a animacja i poza są cullowane i liczone równolegle —
pełna poza tylko dla `AiNormal`, rozmówców i celu gracza, a `refreshAnimationPose`
zawęża to jeszcze do frustum kamery.

Waga: to **nie jest** dzisiejszy limiter — CPU ma 600% zapasu, a klatka stoi na
`present`. Ale przy celu 33 ms obecne ~14 ms CPU to 42% budżetu, więc stanie się
istotne dokładnie w momencie, w którym naprawimy GPU.

## 7. Woda: koszt zlokalizowany, bramka statyczna nie wystarcza

Rozdzielenie `PS_Translucent` pokazało, gdzie naprawdę siedzą te 3.2 ms:

| | Δ vs baseline |
|---|---|
| `drawGWater` | **−2.5 ms** |
| `drawReflections` | −0.5 ms |

To nie shader odbić jest drogi. `drawGWater` ma **własny render pass** i czyści
`gbufDiffuse` oraz `gbufNormal` do zera, żeby `drawReflections` odróżnił piksele
wody. Na kafelkowcu to zrzut kafelków plus czyszczenie i zapis dwóch celów
G-bufora — co klatkę, niezależnie od tego, czy woda cokolwiek narysuje.

Sprawdzone przed bramkowaniem: **`drawReflections` jest jedynym konsumentem tych
celów po `drawGWater`** (mgła i podwodne ich nie czytają), więc pominięcie
czyszczenia jest bezpieczne tylko wtedy, gdy pomija się też odbicia — inaczej
shader odbić zobaczy nieprzezroczysty G-bufor i uzna cały ekran za wodę.

**Wynik bramki statycznej: 0 ms.** `[water] world has water draw commands: 1` —
Khorinis ma zaalokowaną wodę, więc warunek „świat nie ma wody" nigdy nie zachodzi
i `frame_p50` pozostaje 65.2 ms. Było to przewidziane przed pomiarem; zmiana jest
poprawna i konserwatywna (nie może wyprodukować klatki ze znikającą wodą), ale
**sama z siebie nic nie daje**. Zysk wymaga testu frustum per klaster wody —
wtedy warunek zachodzi we wnętrzach, gdzie wody nie widać.

## 8. Bilans sesji: co przeżyło pomiar, a co nie

| Kandydat | Wielkość | Los |
|---|---|---|
| Rysunek indeksowany per meshlet | 22 ms | ❌ `multiDrawIndirect=0` na Mali |
| Nakładanie CPU/GPU | ~14 ms | ❌ zmierzone, nie istnieje (§5a) |
| Usunięcie HiZ | 8.7 ms | ❌ artefakt; HiZ **zarabia** 6.3 ms (§4a) |
| Zasięg widzenia | — | ❌ zmierzone ~0 ms (07-20) |
| „Cienie 18 ms" | — | ❌ realnie 3 ms na kaskadę |
| Bramka wody (statyczna) | 3.0 ms | ⚠️ poprawna, ale 0 ms w Khorinis |
| Przepakowanie wierzchołka | część z 22 ms | ⏳ **nietknięte, jedyna pozostała duża pozycja** |
| Bramka wody (frustum) | do 3.0 ms | ⏳ tylko wnętrza |
| LUT-y nieba rzadziej | do 2.2 ms | ⏳ |
| Tick 1053 NPC | część z 14 ms CPU | ⏳ nie jest dziś limiterem |

Trzy duże dźwignie okazały się puste. Każda została zamknięta **pomiarem**, żadna
nie weszła do builda na wiarę, a bramka przy `multiDrawIndirect` zatrzymała sześć
gotowych i przetestowanych patchy, zanim je wdrożyłem.

Realistyczny sufit tego, co zostało: rząd **8–12 ms z 65**, czyli ~55 ms (~18 FPS).
**Stabilne 30 FPS wymaga cięcia treści** — przy czym zasięg widzenia, jedyna
oczywista dźwignia treściowa, jest zmierzony jako bezwartościowy.

## 9. Pre-rotacja: bramka przeszła — kompozycja DEVICE jest osiągalna

Sonda (commit `8687a670`, wycofana zaraz po odczycie): zamieniony extent
swapchaina plus `preTransform=currentTransform`, **bez** obracania renderowania.
Obraz wyszedł bokiem — i o to chodziło, bo pytanie brzmiało wyłącznie „czy
warstwa może wyjść z kompozycji CLIENT".

```
[prerotate] currentTransform=2 extent=800x1340

przed:  clientCompositionFrames = 188781 / 190828   (98.9%)
        Layer:  CLIENT | ROT_90 | displayFrame 800x1340 | sourceCrop 1340x800

po:     clientCompositionFrames =     31 /   3494   ( 0.9%)
        Layer:  DEVICE |      0 | displayFrame 800x1340 | sourceCrop  800x1340
```

**Warstwa przeszła na DEVICE z transformacją 0** — HWC skanuje nasz bufor wprost,
a pełnoekranowy przebieg kompozycji SurfaceFlingera na Mali znika.

**Czego to jeszcze nie mówi:** wielkości zysku. Odczyty `frame_p50` z tej sondy
(64.8 i 75.2 ms) są **nieporównywalne** z baseline — swapchain jest portretowy,
więc proporcje kadru i ilość geometrii w polu widzenia są inne. Szacunek 3–6 ms
nadal pochodzi z dolnego trybu histogramu `renderEngineTiming`, nie z pomiaru
naszej klatki. Zmierzy się dopiero na poprawnej implementacji.

### Jak to wdrożyć naprawdę

Dziś projekcja, viewport, układ UI i dotyk siedzą **spójnie w przestrzeni
landscape** (tak stwierdza patch `(c3)`), a swapchain musi być portretowy.
Dwie drogi:

- **B (rekomendowana): obrót tylko na końcu.** Scena i UI renderują się jak
  dziś w landscape, a obraca dopiero ostatni przebieg zapisujący do obrazu
  swapchaina. Ponieważ tonemapping/CMAA2 to i tak pełnoekranowy trójkąt
  czytający `sceneLinear`, obrót to **transformacja UV w tym jednym shaderze —
  bez dodatkowego przebiegu i bez dodatkowej pamięci**. Zostaje do rozwiązania
  UI, które rysuje się wprost do obrazu swapchaina (`uiLayer.draw`,
  `inventory.draw`, `numOverlay.draw`) i wymaga albo własnej macierzy obrotu,
  albo pośredniego celu landscape (wtedy jeden dodatkowy przebieg, ~1–2 ms,
  zjadający część zysku).
- **A: pełna pre-rotacja silnika.** Obrót w macierzy projekcji plus przemapowanie
  UI i dotyku. Bez dodatkowego przebiegu, ale dotyka wielu miejsc i łamie
  spójność, na której dziś stoi obsługa wejścia.

Rekomendacja: **B**, i to w wariancie „UV w tonemappingu" dla sceny; dopiero
jeśli UI okaże się kłopotliwe, dołożyć dla niego pośredni cel.
