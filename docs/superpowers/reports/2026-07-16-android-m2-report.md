# Raport z prac — Android M2: od „crashuje po 2 min" do grywalnego portu

Data: 2026-07-15 → 2026-07-16
Gałąź: `android` (worktree `E:\claude\opengothic-android`) — `master`/iOS nietknięte
Sprzęt docelowy: **Samsung Galaxy Tab A9 (SM-X115)** — Mediatek Helio G99 / **Mali-G57 MC2** / 3,5 GB / Android 15 (`R83Y81NE23H`)
Sprzęt kontrolny: **Samsung Galaxy A23 5G** — Snapdragon 695 / **Adreno 619** / 3,4 GB (`R5CT92SB0YL`)

## Wynik

**Port Gothica II: Noc Kruka jest grywalny na realnym, budżetowym Androidzie.** Ładuje pełny
Khorinis, renderuje poprawnie w landscape, prowadzi dialogi po polsku, reaguje na dotyk
(menu → Nowa gra → świat → wybór kwestii dialogowej) i przeżywa wielominutowe sesje bez awarii.

Stan wejściowy sesji: świat ładował się częściowo, gra padała na SIGSEGV po ~2 min, obraz był
obrócony o 90°, dotyk nieużywalny (testy wymagały obejścia `-nomenu`).

## Naprawione blokery

| # | Objaw | Prawdziwa przyczyna | Commit |
|---|---|---|---|
| 1 | SIGSEGV ~1–2 min w świecie | null-tekstura w tablicy bindless; Mali nie ma `VK_EXT_robustness2` | `350abba7` (potem cofnięty, patrz §3) |
| 2 | Zawis ładowania świata, 100% CPU | **nieskończona pętla** `loadTextureAnim` | `353050a3` |
| 3 | SIGSEGV ~2 min **po** loadzie, w renderowaniu | **null-e wypełniające** tablicę bindless (`roundup` do 1024) | **`58d7ec37`** |
| 4 | Czerwone pancerze NPC | zepsuty łańcuch fallbacku wariantów w `solveTex` | **`54ec164f`** |
| 5 | Obraz obrócony 90°, dotyk nie trafia | `preTransform` deklarował pre-rotację, której nie było | **`db669283`** |
| 6 | Klockowate tekstury | mip-cap 256 px (dodany na podstawie **błędnej** diagnozy) | **`824b6da5`** |

### 1+2+4 — jeden hack, trzy problemy (najważniejsza lekcja sesji)

Crash #1 zdiagnozowałem poprawnie: [vdescriptorarray.cpp:90](lib/Tempest/Engine/gapi/vulkan/vdescriptorarray.cpp:90)
wpisuje `VK_NULL_HANDLE` jako imageView dla null-tekstur, a Mali-G57 **nie ma `VK_EXT_robustness2`
(nullDescriptor)** → `vkUpdateDescriptorSets` na null imageView = UB → crash w sterowniku.
(Ścieżka buforowa w tym samym pliku **ma** guard `!hasRobustness2` — teksturowa go nie ma. To realna
luka odporności Tempesta na mobilne GPU.)

**Ale naprawiłem to w złej warstwie.** Zmusiłem `Resources::implLoadTexture`, by nigdy nie zwracał
null-a, tylko `&fallback`. To zmieniło **kontrakt null-a w całym silniku** i wywołało lawinę:

- **Zawis ładowania (#2):** `loadTextureAnim` sonduje klatki animacji `TEX_A0, TEX_A1, …` i **kończy
  na null-u**. Skoro `loadTexture` przestał zwracać null — pętla kręciła się w nieskończoność,
  ładując fallback jako kolejne klatki. 100% CPU, GPU płaskie, pasek zamrożony na 27%.
- **Czerwone pancerze (#4):** [meshobjects.cpp:43](game/graphics/meshobjects.cpp:43) `solveTex` próbuje
  wariantu tekstury (`HUM_ORGL_ARMOR_V1.TGA`) i **gdy go nie ma, ma dostać `nullptr` i wrócić do
  tekstury bazowej `def`**. Nie wszystkie warianty istnieją w VDF — silnik na tym polega.
  Mój hack zwracał `&fallback`, więc `if(ntex!=nullptr)` odpalał i pancerz dostawał fallback.
  A fallback to **czysta czerwień** ([resources.cpp:94](game/resources.cpp:94): `pix[0]=255; pix[3]=255;`)
  — celowy marker „missing texture".

**Właściwy fix (`58d7ec37`) leżał gdzie indziej.** [drawbuckets.cpp:99](game/graphics/drawbuckets.cpp:99)
`tex.resize(roundup(tex.size()))` dopycha tablicę bindless do wielokrotności **1024** — a nowe
elementy to **domyślne `nullptr`**. Przy 805 kubełkach daje to **219 null-owych slotów**, których
żaden fix w warstwie zasobów nie dosięgnie (one nigdy nie przechodzą przez `loadTexture`).
Podmiana null→`fallbackTexture()` **w miejscu budowania tablicy** załatwia wszystko naraz:
sloty wypełniające, materiały bez tekstury i błędy ładowania.

Po tym hack z warstwy zasobów stał się zbędny i **został cofnięty w całości** (`54ec164f`, −28 linii),
razem z wymuszoną przez niego łatką na `loadTextureAnim`. Zweryfikowałem bezpieczeństwo: jedyna
druga tablica deskryptorów tekstur to [rtscene.cpp:127](game/graphics/rtscene.cpp:127), a Mali nie ma
sprzętowego ray-query, więc ta ścieżka nigdy nie biegnie.

> **Lekcja:** kwirki mobilnych GPU naprawiać **w warstwie GPU/bindingu**, nigdy przez zmianę
> kontraktu współdzielonej warstwy zasobów. Ten jeden „sprytny" hack kosztował dwa dodatkowe bugi
> i kilka cykli diagnostycznych, a właściwa poprawka to 5 linii we właściwym miejscu.

### 5 — obrót 90°: pomiar zamiast intuicji

Notatki (moje) twierdziły, że `findSwapExtent` zwraca **portretowy** `currentExtent` 800×1340, więc
naprawa wymaga pre-rotacji projekcji, viewportu, UI i dotyku. Zanim to zrobiłem, **zmierzyłem**
(tymczasowy log `[swapdiag]`, `cc64fccf`):

```
currentTransform=2 (ROTATE_90)   supportedTransforms=511 (IDENTITY dostępny)
currentExtent=1340x800 (LANDSCAPE!)   chosenExtent=1340x800
minImageExtent=1x1   maxImageExtent=4096x4096   (extent NIE jest zablokowany)
wm size: Physical size: 800x1340 (panel portretowy)
```

**Notatka była błędna.** Surface, extent swapchaina, layout UI i współrzędne dotyku były **już
spójnie landscape**, a silnik renderował poprawną klatkę. Zła była wyłącznie **deklaracja**:
[vswapchain.cpp:312](lib/Tempest/Engine/gapi/vulkan/vswapchain.cpp:312) `preTransform = currentTransform`
mówi „aplikacja już obróciła obraz o 90°" — a nie obróciła. Kompozytor pomijał więc swoją rotację
i wyświetlał landscape'ową klatkę bokiem na portretowym panelu.

Fix = **zadeklarować `IDENTITY`** i pozwolić kompozytorowi obrócić. **Zero pre-rotacji.**
**Dotyk naprawił się sam**, bez tknięcia kodu inputu — bo od początku trafiał tam, gdzie przycisk
jest *logicznie*; po prostu widziało się go gdzie indziej.

Pułapka: `preTransform != currentTransform` sprawia, że sterownik **trwale** zgłasza
`VK_SUBOPTIMAL_KHR`. Tempest rzuca wtedy `SwapchainSuboptimal()` w **dwóch** miejscach
([vswapchain.cpp:431](lib/Tempest/Engine/gapi/vulkan/vswapchain.cpp:431) acquire i
[:523](lib/Tempest/Engine/gapi/vulkan/vswapchain.cpp:523) present) → odtworzenie swapchaina →
identycznie suboptymalny → **zawis**. Oba miejsca neutralizują `SUBOPTIMAL` na Androidzie.
To **nie jest** to samo, co „ignoruj SUBOPTIMAL" z zamkniętego [PR #893](https://github.com/Try/OpenGothic/pull/893):
tam maskowano nieznaną niespójność, tu **sami świadomie wywołujemy** niedopasowanie, a realne zmiany
surface i tak przychodzą przez `VK_ERROR_OUT_OF_DATE_KHR` oraz `AndroidApi` (INIT/TERM/RESIZED).

> **Lekcja:** gdyby nie 8-minutowy pomiar, przepisałbym pół renderera bez potrzeby.

### 6 — tekstury: mip-cap jako ślepa uliczka (zmierzone)

Mip-cap 256 px dodałem, gdy sądziłem, że crash to **OOM**. Crash okazał się bugiem
null-descriptor. **Powód istnienia capa zniknął, został tylko koszt** — klockowate postacie.

Pomiary w komnacie Xardasa (świat wczytany):

| Konfiguracja | GPU (GL mtrack) | PSS | Wolny RAM | Wygląd |
|---|---|---|---|---|
| cap 256 | 0,88 GB | 1,48 GB | 642 MB | **klocki** ❌ |
| cap 512 | 1,30 GB | 1,92 GB | 378 MB | dobrze ✅ |
| bez capa | 1,38 GB | 1,96 GB | 288 MB | dobrze ✅ |

**Cap 512 oszczędza tylko ~80 MB** (1,38 → 1,30 GB, ~6% całości) — czyli tekstury Gothica 2 są w większości ≤512 px i cap 512
prawie niczego nie tyka. Skok 256→512 to **+420 MB**, bo tam siedzi *większość* tekstur.
**Nie ma złotego środka**: albo klocki, albo brak oszczędności. Cap potrafi tylko wymieniać
rozdzielczość na pamięć — a problem leży gdzie indziej (patrz §Transcoder).

Cap został jako strojny zawór awaryjny `[INTERNAL] androidTexCap` (domyślnie **0 = wyłączony**),
konfigurowalny z `Gothic.ini` bez rebuilda — przy pętli CI+adb każdy eksperyment na stałej
kosztował ~8 minut.

## Wydajność (zmierzona, nie szacowana)

FPS mierzony z warstwy prezentującej **BLAST** SurfaceFlingera (`dumpsys SurfaceFlinger --latency`),
bo systemowe `gfxinfo` nie widzi natywnego Vulkana renderującego do SurfaceView (raportuje 1 klatkę UI).

**Pełna jakość** (`vidResIndex=0`, `modelDetail=1.0`, bez capa), komnata Xardasa:
**16,2 FPS** (klatka 61,8 ms, zakres 48–82 ms).

Rozkład kosztów — **te trzy rzeczy płacą różnymi zasobami**, co jest kluczowe przy strojeniu:

| Ustawienie | Daje | Kosztuje |
|---|---|---|
| `androidTexCap=0` | ostre twarze/pancerze | **pamięć** (+500 MB), ~0 FPS |
| `vidResIndex=0` | ostrość całego obrazu | **FPS** — 4× więcej pikseli |
| `modelDetail=1.0` | gładsze sylwetki | trochę FPS (geometria) |

Zabójcą FPS jest `vidResIndex`, nie tekstury. Kandydat na kompromis: `vidResIndex=1` (75%) +
pełne tekstury — ostrość twarzy zostaje, render schodzi z 4× do ~2,25× pikseli.

**Uwaga metodologiczna:** próba porównania FPS cap-512 vs bez-capa była **nieważna** — zrzut ujawnił,
że jeden pomiar wypadł na ekranie tytułowym (prawie czarnym), a drugi w pełnej scenie 3D. Liczby
FPS mają sens tylko przy tej samej scenie.

## Transcoder DXT→ASTC — Faza 1 (brama decyzyjna)

**Przyczyna źródłowa 1,38 GB:** [device.cpp:195-203](lib/Tempest/Engine/graphics/device.cpp:195) —
gdy GPU nie ma BC/S3TC, **każda** tekstura DXT jest dekompresowana do RGBA8 (DXT1: 8×, DXT5: 4×).
To nie kilka wielkich tekstur, tylko **setki średnich**. Dlatego cap nie pomaga, a ASTC pomaga:
trzyma je skompresowane **w pełnej rozdzielczości** (tekstury /4 → realnie ~1,38 GB → **~380–460 MB**;
render targety, bufory i swapchain się nie kurczą — patrz spec §3).

Projekt: [`specs/2026-07-16-astc-transcoder-design.md`](../specs/2026-07-16-astc-transcoder-design.md).
Plan: [`plans/2026-07-16-astc-phase1-decision-gate.md`](../plans/2026-07-16-astc-phase1-decision-gate.md).

Faza 1 istnieje po to, by **cztery niezależne ryzyka zabójcze** sprawdzić w kilku cyklach CI (realnie
wyszły 3, ~25 min), zanim
powstanie cache, loader czy enkoder:

| # | Ryzyko | Wynik |
|---|---|---|
| 1 | Chirurgia na współdzielonym enumie rozwali backendy | ✅ patche OK, CI zielone, **GPU 1,38 GB identyczne jak przed** = nic nie przesunięte, zero crashy |
| 2 | Mali nie próbkuje ASTC 4×4 | ✅ **`DXT1=0 DXT5=0 ASTC4x4=1`** |
| 3 | astcenc nie zbuduje się na arm64 | ✅ `astcenc-neon-static` kompiluje się i linkuje pod NDK |
| 4 | Kodowanie za wolne | ✅ **129 s jednowątkowo (zmierzone)** — to samo przechodzi bramę; skalowanie wielordzeniowe **nie zmierzone** |

**BRAMA ZALICZONA W CAŁOŚCI → Faza 2 (kodowanie na urządzeniu) ma zielone światło.**

```
[astcdiag] astcenc 512x512 PRE_FAST 1-thread: 98.321 ms => 2.666 Mpx/s
           | Khorinis 345 Mpx => 129.4 s single-threaded (~21.6 s over 6 cores)
           | out=256 KiB vs rgba8=1024 KiB
```

**Pesymistycznie** (ładowanie tekstur całkowicie szeregowe) to **~2,2 min jednorazowo** przy pierwszym
loadzie — mieści się w szacunku „1–3 min" z projektu i **samo w sobie przechodzi bramę**. Wariant
offline (spec §9) okazał się niepotrzebny.

> **⚠️ DWIE KOREKTY DO POWYŻSZEGO LOGU (2026-07-16, weryfikacja adwersaryjna):**
>
> **1. „~21.6 s over 6 cores" to projekcja, nie pomiar — i to zła.** Plan specyfikował log
> `" s **if it scales** over 6 cores"`; kod ([main_android.cpp:121](game/main_android.cpp:121))
> zgubił „if it scales", a ja awansowałem to do rangi ✅ wyniku. Do tego „6" nie pasuje do niczego:
> Helio G99 to **2× A76 + 6× A55** (8 rdzeni). Benchmark biegł na **jednym** wątku, **bez przypięcia
> do rdzenia**, jako 98 ms burst na zimnym rdzeniu — nie wiadomo nawet, na jakim rdzeniu, więc żaden
> dzielnik nie jest uzasadniony. Dochodzi throttling i to, że transkodowanie jest **leniwe**, czyli
> konkuruje z ładowaniem świata. **Ryzyko #4 zaliczam na podstawie samego pomiaru 129 s.**
>
> **2. „345 Mpx" jest zahardkodowane w benchmarku** (`345.0/mpxPerSec` w kodzie) — log „potwierdzający"
> ekstrapolację **odbija własną stałą**. Mierzone jest wyłącznie `ms`. Sama baza 345 Mpx to **górna
> granica** (pochodzi z `GL mtrack` = całkowitej pamięci GPU, nie z podsumy tekstur), więc czas
> kodowania jest konserwatywny — kierunek błędu bezpieczny, ale to nie jest „dokładny rozmiar zadania".

**Kompresja 4× zmierzona na pojedynczej teksturze** (`256 KiB` vs `1024 KiB` dla 512×512). Przeniesienie
tego stosunku na zbiorczy licznik `GL mtrack` to **już arytmetyka, nie pomiar** — realne lądowanie
to **~380–460 MB**, nie ~350 MB (render targety i bufory się nie kurczą).

> **⚠️ KOREKTA:** wcześniej stało tu „1,38 GB → ~350 MB to **zmierzony fakt, nie arytmetyka**".
> To była arytmetyka. **Dokładnie ta klasa błędu, za którą krytykuję siebie 30 linii niżej** w sprawie
> 69 MB na Adreno — „dorobiłem wyjaśnienie do liczby i uznałem je za fakt". Wyłapane dopiero przez
> niezależną weryfikację, nie przeze mnie.

**Koszt bramy: 3 cykle CI (~25 min).** Jeden spalił błąd arności `astcenc_context_alloc` (4 argumenty
w 5.6.0, nie 3) — ale ten sam czerwony build **udowodnił ryzyko #3**, pokazując
`astcenc-neon-static` kompilujący się na aarch64. Czerwony build bywa lepszym dowodem niż zielony.

### Odkrycia Fazy 1

**ASTC 4×4 wpasowuje się w Tempest bez uogólniania matematyki bloków.** ASTC 4×4 to blok 4×4 i
**16 bajtów** — dokładnie to, co [pixmap.cpp:452](lib/Tempest/Engine/formats/pixmap.cpp:452) i
[mttexture.cpp:76](lib/Tempest/Engine/gapi/metal/mttexture.cpp:76) już hardkodują dla DXT3/DXT5.
6×6 kompresowałoby 9× zamiast 4×, ale wymagałoby operacji na Vulkanie **i** Metalu.

**Enum ma arytmetykę pozycyjną.** [pixmap.cpp:68](lib/Tempest/Engine/formats/pixmap.cpp:68):
`kfrm[uint8_t(frm)-uint8_t(DXT1)]` — ASTC **musi** trafić na koniec enuma, przed `Last`.

**Sondowanie możliwości jest generyczne** ([vdevice.cpp:678](lib/Tempest/Engine/gapi/vulkan/vdevice.cpp:678)):
pętla `i<Last` → `nativeFormat(i)` → `vkGetPhysicalDeviceFormatProperties` → `smpFormat |= 1ull<<i`.
Czyli `hasSamplerFormat(ASTC4x4)` zaczyna działać **za darmo** po dodaniu enuma + `nativeFormat` +
`isCompressedFormat`. Maska jest 64-bitowa, `Last` = 26 → 27. Bezpiecznie.

**Ryzyko enuma było niższe, niż zakładał spec:** Tempest **nie używa `-Werror`** (tylko wyciszenia
`-Wno-*` dla zależności trzecich), więc brakujący case w switchu to ostrzeżenie, nie błąd.
`-Werror` dotyczy wyłącznie targetu gry ([CMakeLists.txt:29](CMakeLists.txt:29)), a `game/` nie ma
żadnego switcha po `TextureFormat`.

### ⚠️ Korekta wcześniejszego ustalenia: Adreno **też nie ma BC**

Wcześniej w tej sesji zapisałem „Adreno ma BC → tekstury zostają skompresowane → GPU 69 MB".
**To była interpretacja liczby, nie pomiar** — dorobiłem wyjaśnienie i uznałem je za fakt.
Bezpośredni pomiar: **`DXT1=0` na Adreno 619**, tak samo jak na Mali. Jest to zgodne z realiami
mobilnych GPU: **BC/S3TC praktycznie nie istnieje w Vulkanie na mobilkach** — i Mali, i Adreno stoją
na ETC2/ASTC. Tamte 69 MB pochodziło najpewniej z pomiaru w menu, nie w świecie.

**Skutek: dobry.** ASTC pomoże **obu** urządzeniom, gating `!hasSamplerFormat(DXT1)` włączy się
wszędzie, ścieżka jest jedna zamiast dwóch. Spec §4/§7 wymaga korekty (Adreno nie „pominie cache").

## Metoda pracy — co się sprawdziło

**Pętla weryfikacji.** Nic nie buduje się lokalnie na Windowsie. Cykl: commit → push → GitHub
Actions (~8 min) → APK w release `latest-android` → `gh release download` → `adb install` →
`am start` → `logcat`. **Każdy fix jest weryfikowany na sprzęcie**, nie „powinno działać".

**Mierz, zanim zmienisz.** Trzy razy w tej sesji pomiar obalił moje założenie:
1. `preTransform` — extent był landscape, nie portret → oszczędziło przepisanie pół renderera.
2. `cap=512` — miał być tanim wyjściem, oszczędza ~80 MB (~6%) → obalił **moją własną** propozycję i
   potwierdził konieczność transcodera.
3. `[texfail]` — hipoteza „mip-cap rzuca wyjątki" padła (zero błędów DXT); prawdziwą przyczyną
   czerwieni był mój hack null→fallback.

**Instrumentacja tymczasowa, jawnie oznaczona i posprzątana.** Logi `[loadstage]`, `[texfail]`,
`[swapdiag]` + obejście `-nomenu`. Wszystkie z komentarzem „TEMP … revert once …".
Rusztowanie usunięte w `bb5d5096` (−90 linii); pliki z samą diagnostyką przywrócone z bazy gałęzi
po sprawdzeniu, że nie zawierają nic innego.
Diagnostyka **`[astcdiag]` celowo ZOSTAJE** w `main_android.cpp` (log caps + `astcBenchmark()` +
`#include <astcenc.h>`) jako dowód bramy Fazy 1 — jej cofnięcie jest śledzone w „Co dalej" p. 1
i w spec §6a.
> **⚠️ KOREKTA (2026-07-16):** ta sekcja wymieniała wcześniej `[astcdiag]` wśród rzeczy
> **posprzątanych** w `bb5d5096`. To **chronologicznie niemożliwe** — `[astcdiag]` powstał później
> (`8c07a4bd`/`c1597c00`), a `bb5d5096` nie mógł go usunąć. Dokument przeczył sam sobie: „Co dalej"
> p. 1 słusznie wymienia to cofnięcie jako **zaległe**.

**Patche perlowe testowane na sucho przed pushem.** Kopiuję prawdziwy plik do scratchu, puszczam
perl, `grep -c` markery, oglądam kontekst. Zły regex = 8 minut w plecy. Wszystkie 7 patchy ASTC
przeszło na sucho przed commitem.

**Weryfikacja adwersaryjna dokumentacji (2026-07-16).** Po napisaniu tych dokumentów puściłem na nie
niezależnych agentów z zadaniem **obalania** każdego twierdzenia (4 klastry: odniesienia do Tempesta,
do kodu gry, do astcenc/builda, oraz arytmetyka i spójność wewnętrzna), a każde sporne twierdzenie
przeszło jeszcze przez **niezależnego sędziego**. Wynik: **6 potwierdzonych błędów**, w tym cztery,
których sam bym nie zobaczył:

1. **„Adreno ma BC" zostało w 6 miejscach** — mimo że §4/§7 poprawiłem. `spec:34` budował na tym
   **całą ramę problemu**, a komentarz w planie **trafił do `CMakeLists.txt`**.
2. **„cap 512 oszczędza 60 MB"** — z własnej tabeli wychodzi **80 MB**. To była **liczba nośna**
   argumentu za transcoderem, myląca w kierunku dla siebie wygodnym.
3. **„345 Mpx to dokładny rozmiar zadania"** — `GL mtrack` to całkowita pamięć GPU, nie teksele.
   To górna granica. Do tego `345.0` jest **zahardkodowane** w benchmarku, więc log „potwierdzający"
   ekstrapolację odbijał własną stałą.
4. **„1,38 GB → ~350 MB" jako kryterium PASS/FAIL Fazy 2** — nieosiągalne z definicji, bo render
   targety i bufory się nie kurczą. Udany transcoder na ~420 MB byłby oceniony jako porażka.
5. **„~22 s na 6 rdzeniach" jako ✅ WYNIK** — plan mówił „**if it scales**", kod zgubił zastrzeżenie,
   a ja awansowałem projekcję do rangi pomiaru.
6. **`[astcdiag]` opisany jako posprzątany** w `bb5d5096` — chronologicznie niemożliwe.

**Najbardziej pouczające:** punkty 3 i 4 to **dokładnie ta klasa błędu, za którą krytykuję siebie
w tym samym dokumencie** w sprawie 69 MB na Adreno — „dorobiłem wyjaśnienie do liczby i uznałem je
za fakt". Napisanie tej samokrytyki **nie uchroniło mnie** przed powtórzeniem błędu dwa razy, w tym
samym tekście. Świadomość własnego błędu poznawczego nie zastępuje niezależnej kontroli.

Warto też odnotować, że **sędziowie korygowali recenzentów**: pierwszy recenzent podpierał się
surowymi wartościami KB, których nie ma w repo, i wyliczał zakresy z niezmierzonych założeń —
sędzia to wyłapał i kazał **nie wpisywać kolejnej pewnej, niezmierzonej liczby** w miejsce
poprzedniej. To ta sama dyscyplina, tylko wymierzona w recenzenta.

**Rozdzielaj tryby awarii.** Spec mówił „jeden cykl CI" dla Fazy 1; plan celowo użył **dwóch** —
chirurgia na enumie i linkowanie astcenc mają niezwiązane przyczyny padu. Opłaciło się: czerwony
build Zadania 2 od razu pokazał, że **astcenc się kompiluje**, a padło tylko moje wywołanie API.

## Gotchas (utrwalone)

- **`lib/Tempest` to submoduł — NIGDY `git add`.** Zmiany silnika idą przez
  `android/patches/apply-patches.sh` (idempotentne, `grep`-guarded, `exit 1` przy niedopasowaniu).
  `lib/astcenc` to inna sprawa — używamy go **bez modyfikacji**, więc wskaźnik submodułu jest OK.
- **`gh` zawsze z `--repo tryk016/opengothic-ios`** — gołe `gh` celuje w upstream `Try/OpenGothic` (401).
- **PowerShell 5.1:** `>` psuje binaria (BOM UTF-16) — zrzuty ekranu przez `adb shell screencap` +
  `adb pull`, nigdy `adb exec-out > plik.png`. `ConvertFrom-Json` na tablicy wymaga
  `Where-Object { $_.x.StartsWith(...) }`, nie `-like` na całości. Here-stringi łatwo psują
  wiadomości commitów — użyj `git commit -F <plik>`.
- **`Tempest::Log` nie ma przeciążenia dla `bool`** (są jawne dla int8..uint64/float/double).
  Formalnie `bool→int` to promocja i by zadziałało, ale nie warto opierać builda na rankingu
  przeciążeń — rzutuj `int()`.
- **`android.yml` ma `paths-ignore: docs/**`** — commity dokumentacji nie palą cykli CI.
- **`abiFilters 'arm64-v8a', 'x86_64'`** — każda zależność natywna musi obsłużyć oba ABI
  (NEON na x86_64 nie istnieje).
- **Marker `World load COMPLETE` wypada z bufora logcata** przy dłuższym czekaniu — „world loaded:
  False" przy GPU 1,38 GB to fałszywy alarm, nie brak loadu.

## Co dalej

1. **Cofnąć diagnostykę Fazy 1** z `main_android.cpp` (log `[astcdiag]`, `astcBenchmark()`,
   `#include <astcenc.h>`) — **zostawiając** sekcję `(f)` w `apply-patches.sh`, submoduł astcenc
   i konfigurację CMake. To jest fundament Fazy 2.
2. **Faza 2** — cache + loader + kodowanie na urządzeniu (brama zaliczona, zielone światło).
3. **Faza 3** — iOS: powielić patche w `ios/patches/apply-patches.sh` + case w Metalu.
   `resources.cpp` i cache działają bez zmian dzięki gatingowi po możliwościach GPU.
4. **Rebase na `master`** — 9 commitów zaległości. PR #893 umarł właśnie na konfliktach; nie zwlekać.
   Jeden konflikt do pogodzenia w bloku mobilnym `gothic.cpp`.
5. **Strojenie FPS** — `vidResIndex=1` jako kandydat; `sightValue=2` (60k z 300k) jest bardzo niski.
6. **Licznik FPS na ekranie** — `[GAME] showFpsCounter=1` nie odpala; `doFrate()` czyta ustawienie
   w `setupSettings()` (gothic.cpp:946, wołane z konstruktora), podejrzenie: kolejność ładowania
   `Gothic.ini`. Wymaga rebuilda z logiem.
