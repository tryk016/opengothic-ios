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
siedmiu etapach GPU ma zapas, a ogranicza nas własny cap.

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

## 5. Korekty wcześniejszych wniosków

- **„Cienie ≈ −18 ms" było błędne.** Zdjęcie całej kaskady daje **3.0 ms**,
  a rozdzielczość cieni ~1 ms. Rekomendacja „jedna kaskada zamiast dwóch"
  z 2026-07-20 jest warta 3 ms, nie kilkanaście — wycofuję ją jako główną dźwignię.
- **„To jest geometria" trzeba doprecyzować.** Geometria to ~22 ms z 65 —
  największy pojedynczy blok, ale nie większość klatki. Zdanie z 07-31
  o dominacji geometrii było zbyt mocne.
- **Przebudowa na forward nadal nie ma sensu** — celuje w koszt fragmentowy
  i przepustowość, które zmierzone są bliskie zeru.

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
