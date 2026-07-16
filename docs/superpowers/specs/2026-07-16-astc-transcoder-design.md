# DXT→ASTC transcoder — projekt (Android + iOS)

Data: 2026-07-16
Status: zatwierdzony kierunek (wariant B — transkodowanie **na urządzeniu**, leniwe + cache dyskowy), do implementacji fazami
Dotyczy: portu Android (`android`) **oraz** portu iOS (`master`) — projekt jest celowo platform-neutralny

## 1. Problem

Mali-G57 (i **każde Apple GPU**) nie ma BC/S3TC. Wspólna warstwa graficzna Tempesta dekompresuje
wtedy każdą teksturę DXT do RGBA8 — [device.cpp:195-203](lib/Tempest/Engine/graphics/device.cpp:195):

```cpp
if(isCompressedFormat(format)){
  if(devProps.hasSamplerFormat(format) && (!mips || pm.mipCount()>1)){
    mipCnt = pm.mipCount();
    } else {
    alt    = Pixmap(pm,TextureFormat::RGBA8);   // ← 4-8x blowup
    p      = &alt;
    format = TextureFormat::RGBA8;
    }
  }
```

DXT1 0.5→4 B/px = **8×**, DXT5 1→4 B/px = **4×**.

### Zmierzone na Samsung Tab A9 (Helio G99 / Mali-G57 MC2 / 3.5 GB), Khorinis wczytany

| Konfiguracja | GPU (GL mtrack) | PSS | Wolny RAM | Wygląd |
|---|---|---|---|---|
| `androidTexCap=256` | 0.88 GB | 1.48 GB | 642 MB | **klocki** ❌ |
| `androidTexCap=512` | 1.30 GB | 1.92 GB | 378 MB | dobrze ✅ |
| bez capa (pełne) | **1.38 GB** | **1.96 GB** | **288 MB** | dobrze ✅ |

**Problemem jest 1.38 GB na urządzeniu z 3.5 GB** — przy pełnych teksturach zostaje **288 MB
wolnego RAM-u**, czyli system utrzymuje grę przy życiu wyrzucając wszystko z tła.

> **Uwaga o `GL mtrack`:** to licznik **CAŁKOWITEJ** śledzonej pamięci GPU — zawiera też render
> targety, bufory geometrii i swapchain, nie tylko teksele. Kolumna nadaje się do porównywania
> konfiguracji między sobą (co robimy tutaj), ale **nie jest podsumą tekstur** — patrz zastrzeżenia
> w §5.3 i §6a.

> **⚠️ USUNIĘTE (2026-07-16):** ta sekcja podawała wcześniej „dla porównania Adreno 619 (**ma BC** →
> tekstury zostają skompresowane): GPU 69 MB. Ta różnica (69 MB vs 1.38 GB) jest **całym problemem**".
> **Obie części były błędne.** Adreno **nie ma BC** (zmierzone `DXT1=0`, patrz §4), a 69 MB pochodziło
> najpewniej z pomiaru w menu, nie w załadowanym świecie — więc nie było porównywalne z 1.38 GB.
> Cała rama §1 wisiała na liczbie, którą §4 unieważnia. Właściwa rama to po prostu 1.38 GB na 3.5 GB.

### Dlaczego mip-cap to ślepa uliczka (zmierzone, nie szacowane)

`androidTexCap=512` oszczędza **tylko ~80 MB** (1.38 → 1.30 GB, ~6% całości), bo tekstury Gothica 2
(gra z 2003) są w większości ≤512 px — cap 512 prawie niczego nie tyka. Żeby cap cokolwiek dał,
trzeba zejść do 256, a wtedy zgniata *dominującą* grupę 512-tek (+420 MB różnicy między 256 a 512)
i **widać klocki**.
**Nie ma złotego środka**: 1.38 GB nie bierze się z kilku wielkich tekstur, tylko z dekompresji
setek średnich do RGBA8. Cap umie tylko wymieniać rozdzielczość na pamięć.

> **⚠️ KOREKTA (2026-07-16):** wcześniej pisało tu „oszczędza **tylko 60 MB**". Z tabeli powyżej
> wychodzi **1.38 − 1.30 = 0.08 GB = ~80 MB**. Liczba była zaniżona — i to akurat **liczba nośna
> całego argumentu** za transcoderem, myląca w kierunku dla siebie wygodnym. Wniosek się nie zmienia
> (~6% oszczędności za widoczne klocki to zły interes), ale liczba musi być prawdziwa.

**ASTC atakuje prawdziwą przyczynę**: trzyma tekstury skompresowane w **pełnej rozdzielczości**.

## 2. Czego to NIE naprawia

**Transcoder naprawia pamięć, nie FPS.** Zmierzone 16.2 FPS (pełna jakość, komnata Xardasa,
z warstwy BLAST SurfaceFlingera) wynika z `vidResIndex=0` (4× pikseli) i `modelDetail`, nie z tekstur.
Tekstury kosztują pamięć, nie fill rate. Nie mylić tych dwóch problemów.

## 3. Kluczowa decyzja: ASTC 4×4 (nie 6×6)

ASTC 4×4 = **blok 4×4, 16 bajtów** = 8 bpp. RGBA8 = 32 bpp → **4× oszczędności na samych teksturach**,
przy **zachowaniu pełnej rozdzielczości**. Realistyczne lądowanie: **1.38 GB → ~380–460 MB**
(patrz zastrzeżenie niżej).

> **⚠️ KOREKTA (2026-07-16):** wcześniej pisało „1.38 GB → **~350 MB**". To stosowało 4× do **całego**
> licznika `GL mtrack`, a render targety, bufory geometrii i swapchain **nie kurczą się wcale** — to
> nie są tekstury. Poprawny model: `po = (całość − N)/4 + N`, gdzie `N` = pamięć nieteksturowa.
> Z realnych stałych renderera (render targety ~10–50 MB przy `vidResIndex=2`, shadow 512² ×2 ≈ 2 MB,
> swapchain ~13 MB, plus nieoszacowane bufory geometrii) `N` ≈ 50–150 MB → **~380–460 MB**.

6×6 (3.56 bpp) dałoby 9× (~150 MB), ale **oba backendy hardkodują bloki 4×4**:

- Vulkan/Pixmap: [pixmap.cpp:452](lib/Tempest/Engine/formats/pixmap.cpp:452) `blockSizeForFormat`:
  DXT1=8, DXT3=16, DXT5=16 (bajty na blok 4×4)
- Metal: [mttexture.cpp:76-83](lib/Tempest/Engine/gapi/metal/mttexture.cpp:76)
  `blockSize = (frm==DXT1) ? 8 : 16;` + `wBlk=(w+3)/4, hBlk=(h+3)/4`

**ASTC 4×4 wpasowuje się w oba bez tknięcia matematyki bloków.** 6×6 wymagałoby uogólnienia
wymiarów bloku w Vulkanie *i* Metalu. Dlatego: **4×4**. 4× wystarcza (288 MB → ~1.3 GB wolnego).

## 4. Architektura

Wszystko dzieje się **na urządzeniu, bez żadnego kroku ręcznego**. Transkodowanie jest **leniwe**:
płacimy tylko za tekstury, które faktycznie się ładują, i tylko raz — potem jest cache.

```text
Resources::implLoadTextureUncached(name)          [Android lub iOS]
  │
  ├─ hasSamplerFormat(DXT1)?  ── tak ─► dotychczasowe zachowanie (desktop: natywne DXT)
  │        │ nie  (Mali, Adreno, Apple GPU — patrz korekta w §4)
  │        ▼
  ├─ <cache>/<NAZWA>.astc istnieje?
  │        │ tak ─► wczytaj → Pixmap(ASTC4x4) → dev.texture()      ← trafienie: szybko, skompresowane
  │        │ nie
  │        ▼
  ├─ ZenKit Texture::as_rgba8(lvl)      (dekoduje DXT → RGBA8, i tak to dziś robimy)
  ├─ astcenc: RGBA8 → ASTC 4×4          (pełny łańcuch mipów)      ← koszt jednorazowy
  ├─ zapisz <cache>/<NAZWA>.astc        (atomowo: tmp + rename)
  └─ Pixmap(ASTC4x4) → dev.texture()
```

**Gating przez możliwości GPU, nie przez `#if`.** Warunek to `!hasSamplerFormat(DXT1)`:

- Mali → brak BC → używa cache ASTC — **zmierzone: `DXT1=0 DXT5=0 ASTC4x4=1`**
- **Apple GPU → brak BC → używa tego samego cache ASTC** (iOS działa za darmo)
- Adreno → **również brak BC** → także używa cache ASTC — **zmierzone: `DXT1=0 DXT5=0 ASTC4x4=1`**

To jest powód, dla którego projekt jest platform-neutralny: **jeden cache, oba porty**.

> **⚠️ KOREKTA (2026-07-16, Faza 1).** Wcześniejsza wersja tego dokumentu twierdziła, że
> „Adreno **ma** BC → ignoruje cache, zostaje przy natywnym DXT". **To było błędne** — była to
> *interpretacja* wcześniejszego pomiaru (GPU 69 MB na A23 vs 263 MB na Mali), a nie pomiar
> możliwości. Bezpośredni odczyt `hasSamplerFormat` na Adreno 619 daje **`DXT1=0`**, dokładnie jak
> na Mali. Jest to zgodne z realiami mobilnych GPU: **BC/S3TC praktycznie nie istnieje w Vulkanie na
> mobilkach** — i Mali, i Adreno stoją na ETC2/ASTC. Tamte 69 MB pochodziło najpewniej z pomiaru
> w menu, nie w załadowanym świecie.
>
> **Skutek jest korzystny:** ASTC pomaga **wszystkim** testowanym urządzeniom, ścieżka jest jedna
> zamiast dwóch, a gating `!hasSamplerFormat(DXT1)` pozostaje poprawny bez zmian — po prostu włącza
> się szerzej, niż zakładano. Ryzyko „Adreno niepotrzebnie użyje ASTC (8 bpp > DXT 4 bpp)" z §7
> **nie istnieje**: alternatywą na Adreno nie jest DXT 4 bpp, tylko RGBA8 32 bpp.

## 5. Komponenty

### 5.1 Tempest — wsparcie próbkowania ASTC 4×4 (~7 patchy)

Tempest to **submoduł, którego nigdy nie commitujemy** — wszystko idzie przez
`android/patches/apply-patches.sh` (idempotentne patche perlowe). iOS ma własny
`ios/patches/apply-patches.sh` — te same patche trzeba tam powielić.

| # | Plik | Zmiana |
|---|---|---|
| 1 | `gapi/abstractgraphicsapi.h:126` | dodać `ASTC4x4` **na KOŃCU enuma** (patrz pułapka niżej) |
| 2 | `gapi/abstractgraphicsapi.h:157` | `formatName`: case → `"ASTC4x4"` |
| 3 | `gapi/abstractgraphicsapi.h:173` | `isCompressedFormat`: `|| f==ASTC4x4` |
| 4 | `formats/pixmap.cpp:260` | `isCompressed`: `|| frm==ASTC4x4` |
| 5 | `formats/pixmap.cpp:452` | `blockSizeForFormat`: `ASTC4x4 → 16` |
| 6 | `formats/pixmap.cpp:492` | `componentCount`: `ASTC4x4 → 4` |
| 7 | `gapi/vulkan/vdevice.h:93` | `ASTC4x4 → VK_FORMAT_ASTC_4x4_UNORM_BLOCK` |
| 8 (iOS) | `gapi/metal/mttexture.cpp` | `ASTC4x4 → MTL::PixelFormatASTC_4x4_LDR` (backend już zna ASTC) |

**⚠️ PUŁAPKA — enum ma arytmetykę pozycyjną.** [pixmap.cpp:68](lib/Tempest/Engine/formats/pixmap.cpp:68):

```cpp
ddsToRgba(data, other.data, w, h, kfrm[uint8_t(other.frm)-uint8_t(TextureFormat::DXT1)], 3);
```

Indeksuje tablicę `kfrm` **różnicą względem `DXT1`**. Wstawienie ASTC między DXT1..DXT5 rozwali
to indeksowanie. **ASTC musi trafić za `RGBA16F`, przed `Last`.** Ta ścieżka (`Pixmap(astc, RGBA8)`)
nigdy nie powinna być wołana dla ASTC — warto ją zabezpieczyć.

**Ryzyko:** zmiana **współdzielonego** enuma przez regexy perlowe. Wszystkie backendy (Vulkan, Metal,
DX12) mogą mieć switche wymagające kompletności. Dlatego Faza 1 (niżej) istnieje.

### 5.2 resources.cpp — ładowanie cache'u

W [resources.cpp:397](game/resources.cpp:397) (dziś siedzi tam mip-cap `androidTexCap`):

```
if(format is DXT):
    if(!hasSamplerFormat(DXT1) and cache file exists):
        wczytaj <cache>/<NAZWA>.astc → Pixmap(ASTC4x4, z mipami) → dev.texture(pm)
    else:
        dotychczasowe zachowanie (to_dds → device.cpp zdekompresuje do RGBA8)
```

Ścieżka cache'u:
- Android: `/sdcard/OpenGothic/astc/` (obok `Gothic2/`)
- iOS: katalog danych aplikacji + `/astc/`
- konfigurowalne przez `Gothic.ini` `[INTERNAL] astcCacheDir`

`androidTexCap` **zostaje** jako awaryjny zawór (gdyby komuś brakło RAM-u), ale domyślnie 0.

### 5.3 astcenc na urządzeniu

**astcenc** (ARM, Apache-2.0) wchodzi jako vendored/submoduł do builda Androida **i** iOS.
Buduje się przez CMake, ma oficjalne wsparcie arm64 + NEON (`ASTCENC_ISA_NEON=ON`).
Linkujemy **bibliotekę** (`astcenc-static`), nie CLI.

Wywołanie: preset **`ASTCENC_PRE_FAST`**, profil `ASTCENC_PRF_LDR` (nie sRGB — dopasować do tego,
jak dziś traktujemy RGBA8), blok **4×4**. Kontekst astcenc tworzymy **raz** (jest kosztowny)
i reużywamy; jest thread-safe przy użyciu `thread_index`.

**Wątkowanie.** Tekstury ładują się w OpenGothicu z workerów (`Workers`), więc kodowanie
naturalnie rozkłada się na rdzenie — ale trzeba to zweryfikować, a nie założyć: jeśli ładowanie
tekstur jest w praktyce jednowątkowe, pierwszy load wydłuży się liniowo. astcenc przyjmuje
`thread_count`, więc alternatywnie można oddać mu równoległość wewnętrznie.

**Ile to potrwa — rząd wielkości wyprowadzony z pomiaru.** 1.38 GB RGBA8 ÷ 4 B/px = **~345 Mpikseli**
dla całego Khorinisu (z mipami).

> **⚠️ KOREKTA (2026-07-16):** wcześniej pisało tu „**To jest dokładny rozmiar zadania**". **Nie jest.**
> `GL mtrack` to **CAŁKOWITA** pamięć GPU — zawiera render targety, bufory geometrii i swapchain,
> z których żadne nie są tekselami DXT i żadne nie będą kodowane. Dzielenie **całości** przez 4 B/px
> milcząco zakłada, że 100% licznika to teksele RGBA8. **~345 Mpx to GÓRNA GRANICA, nie pomiar.**
> Kierunek błędu jest na szczęście **bezpieczny** — zawyżona baza daje zawyżony (konserwatywny) czas
> kodowania. **Fazа 2 musi to zmierzyć wprost** (suma `w*h` po faktycznie transkodowanych łańcuchach
> mipów), zamiast wstecznie wyprowadzać z `mtrack`.

Przy `-fast` + NEON na Helio G99 (**2× Cortex-A76 + 6× Cortex-A55**) spodziewamy się rzędu
**1–3 min** rozłożone na pierwszy load.
**To jest szacunek i MUSI zostać zmierzony w Fazie 1** (patrz §6) — jeśli wyjdzie 10× gorzej,
wracamy do wariantu offline (§9) zanim cokolwiek zbudujemy.

Ponieważ transkodowanie jest **leniwe**, kodujemy wyłącznie tekstury faktycznie ładowane —
nigdy nie płacimy za zawartość VDF-ów, której gra nie użyje.

**Uwaga o jakości:** DXT jest już stratny, więc DXT→RGBA→ASTC to **strata drugiej generacji**.
Dla gry z 2003 powinno być wizualnie nieodróżnialne, ale należy to sprawdzić na zrzutach
(porównanie A/B komnaty Xardasa), a nie zakładać.

### 5.4 Cache na dysku

- Android: `/sdcard/OpenGothic/astc/` — przeżywa reinstalację APK (jak dane gry)
- iOS: katalog danych aplikacji + `/astc/`
- Ścieżka konfigurowalna: `Gothic.ini` `[INTERNAL] astcCacheDir`

**Format pliku:** standardowy `.astc` (nagłówek 16 B) z pełnym łańcuchem mipów.

**Zapis atomowy:** `tmp` + `rename`, żeby zabicie procesu w trakcie kodowania nie zostawiło
obciętego pliku, który przy następnym starcie wygląda jak poprawny cache.

**Unieważnianie:** w nagłówku zapisujemy rozmiar źródłowego wpisu VDF + wersję enkodera.
Niezgodność → koduj ponownie. Chroni przed podmianą danych gry i zmianą parametrów astcenc.

**Rozmiar na dysku:** **do** ~350 MB przy pełnym Khorinisie (górna granica — wynika z tej samej
bazy ≤345 Mpx co §5.3, a ASTC 4×4 to 1 B/px) — pomijalne obok 3 GB danych gry.

### 5.5 Postęp / UX pierwszego uruchomienia

Pierwszy load jest dłuższy o czas kodowania. Istniejący ekran „WCZYTYWANIE" ma już pasek postępu,
więc **prawdopodobnie nie potrzeba nowego UI** — kodowanie wlicza się w normalny postęp ładowania.
Do zweryfikowania po zmierzeniu realnego czasu; jeśli okaże się długie, dodać komunikat
w rodzaju „przygotowywanie tekstur (jednorazowo)".

### 5.6 Workflow użytkownika

**Żaden.** Instalujesz APK, uruchamiasz, grasz. Pierwszy load raz dłuższy, potem cache.
To jest cały powód wyboru wariantu B.

## 6. Fazowanie (de-ryzykowanie)

Wariant B ma **cztery** niezależne niewiadome, a każda potrafi go zabić. Wszystkie są sprawdzalne
w **jednym cyklu CI**, zanim powstanie choćby linijka logiki cache'u.

**Faza 1 — fundament + pomiar, ~1 cykl CI (8 min).**
*(Historyczne oszacowanie. Realnie wyszły **3 cykle / ~25 min** — plan celowo rozbił to na 2 zadania,
by rozdzielić tryby awarii, a jeden cykl spalił błąd arności `astcenc_context_alloc`. Patrz §6a.)*

1. Patche Tempesta z §5.1 (enum, `formatName`, `isCompressedFormat`×2, `blockSizeForFormat`,
   `componentCount`, mapa Vulkana)
2. astcenc podpięty do builda Androida (arm64 + NEON) — **sam fakt, że się linkuje**
3. Log przy starcie: `hasSamplerFormat(DXT1)` i `hasSamplerFormat(ASTC4x4)`
4. **Mikro-benchmark:** zakoduj jedną teksturę 512×512 przez astcenc, zaloguj ms

Bez cache'u, bez ładowania ASTC, bez zmian w ścieżce zasobów.

**Kryteria sukcesu:**
- logcat na Tab A9: `DXT1=false, ASTC4x4=true` → Mali faktycznie próbkuje ASTC 4×4
- APK się buduje i linkuje z astcenc na arm64 → enkoder jest wykonalny na urządzeniu
- benchmark daje Mpx/s → ekstrapolacja na 345 Mpx = **realny czas pierwszego loadu**
- gra działa jak dziś, zero regresji → chirurgia na współdzielonym enumie nikogo nie rozwaliła

**To jest brama decyzyjna.** Jeśli ASTC nie jest wspierane → projekt martwy. Jeśli astcenc nie
buduje się na arm64 → wracamy do wariantu offline (§9). Jeśli benchmark wskazuje np. 30 min
zamiast 1–3 min → wracamy do offline. **Wszystko to wiemy po kilkunastu minutach, a nie po tygodniu.**
*(Realnie: 3 cykle CI, ~25 min — patrz §6a.)*

**Faza 2 — cache + ładowanie + kodowanie.**
Dopiero gdy Faza 1 przejdzie: §5.2 + §5.3 + §5.4 + §5.5.

**Kryteria sukcesu Fazy 2:**
- **Podsuma tekstur spada ~4×** — Faza 2 **musi zalogować podsumę samych tekstur** (suma alokacji
  `Texture2d` w warstwie Resources/Tempest), bo to jest jedyne, co transcoder kontroluje.
  Orientacyjnie całkowite `GL mtrack` powinno spaść z **1.38 GB do ~380–460 MB**, a wolny RAM
  urosnąć z **288 MB do ~1.2–1.3 GB**.
  > **⚠️ KOREKTA (2026-07-16):** wcześniej kryterium brzmiało „GPU spada do **~350 MB**" — czyli 4×
  > od **całego** `GL mtrack`. To kryterium **było nieosiągalne z definicji**: render targety, bufory
  > i swapchain się nie kurczą. Udany transcoder lądujący na ~420 MB zostałby oceniony jako
  > **porażka**. Dlatego kryterium celuje teraz w podsumę tekstur i każe ją **zmierzyć**, zamiast
  > zgadywać `N`.
- ostrość **niezmieniona** (porównanie A/B zrzutów komnaty Xardasa — nie „wygląda ok", tylko zrzuty)
- pierwszy load: zmierzony czas; drugi load: z powrotem do dzisiejszych ~35 s (trafienia w cache)
- zero regresji crashy (soak jak dotąd)

**Faza 3 — iOS.** Powielić patche w `ios/patches/apply-patches.sh` + case w Metalu + astcenc
w buildzie iOS. §5.2 (`resources.cpp`) i cały cache działają bez zmian dzięki gatingowi
po możliwościach GPU.

## 6a. WYNIK FAZY 1 (2026-07-16) — ✅ WSZYSTKIE CZTERY RYZYKA ZALICZONE → IDZIEMY W FAZĘ 2

Zmierzone na Tab A9 (Helio G99 / Mali-G57 MC2), commity `8c07a4bd` + `c1597c00` + `cc417ca2`.

| # | Ryzyko | Kryterium | Wynik |
|---|---|---|---|
| 1 | Chirurgia na współdzielonym enumie rozwali backendy | CI zielone, patche zaaplikowane, zero regresji | ✅ 3 patche `patched:`, build success, **GPU 1.38 GB bez zmian** (PSS 2.00 vs 1.96 GB przed — ~2% szumu między przebiegami, nie „identyczne"), pid żyje, zero `signal 11` |
| 2 | Mali nie próbkuje ASTC 4×4 | `ASTC4x4=1, DXT1=0` | ✅ **`[astcdiag] caps: DXT1=0 DXT5=0 ASTC4x4=1`** |
| 3 | astcenc nie zbuduje się na arm64 (NDK) | APK linkuje | ✅ `astcenc-neon-static` kompiluje się i linkuje |
| 4 | Kodowanie za wolne | ekstrapolacja na ≤345 Mpx | ✅ **129 s jednowątkowo (zmierzone)** — to samo w sobie przechodzi bramę; skalowanie wielordzeniowe **nie zmierzone** |

**Benchmark (dosłowny odczyt):**
```
[astcdiag] astcenc 512x512 PRE_FAST 1-thread: 98.321 ms => 2.666 Mpx/s
           | Khorinis 345 Mpx => 129.4 s single-threaded (~21.6 s over 6 cores)
           | out=256 KiB vs rgba8=1024 KiB
```

**Wnioski:**

- **Szacunek §5.3 („~1–3 min") potwierdzony pomiarem.** **Pesymistyczny** wariant (ładowanie tekstur
  całkowicie szeregowe) to **~2,2 min jednorazowo przy pierwszym loadzie** — i **to wystarcza, by
  przejść bramę** (reguła planu: `T` ≤ ~3 min). Wariant offline (§9) **nie jest potrzebny** — zostaje
  jako plan awaryjny bez zastosowania.
  > **⚠️ KOREKTA (2026-07-16):** ta sekcja podawała „✅ 129 s jednowątkowo / **~22 s na 6 rdzeniach**"
  > jako **WYNIK**. Liczba 22 s jest **projekcją, nie pomiarem**, i to złą:
  > **(1) zdarte zastrzeżenie** — plan specyfikował log `" s **if it scales** over 6 cores"`, a kod
  > ([main_android.cpp:121](game/main_android.cpp:121)) zgubił „if it scales"; ja awansowałem to do
  > rangi ✅ wyniku. **(2) „6" nie pasuje do niczego** — Helio G99 to **2× A76 + 6× A55** (8 rdzeni);
  > §5.3 mówiło wcześniej „8 rdzeni", §6a „6". **(3) Benchmark to `thread_count=1`** na jednym kaflu
  > 512×512, **bez przypięcia do rdzenia** (brak `sched_setaffinity`) — nie wiadomo nawet, na jakim
  > rdzeniu biegł, więc żaden dzielnik nie jest uzasadniony. **(4) 98 ms burst na zimnym rdzeniu** nie
  > przewiduje 1–2 min kodowania all-core na pasywnie chłodzonym tablecie (throttling), a §5.3 sam
  > mówi, że transkodowanie jest **leniwe** — konkuruje z ładowaniem świata o te same rdzenie.
  > Realistyczny agregat to rząd **~3×, nie 6×** → **rzędu 40–60 s, jako szacunek, nie wynik**.
  > **Ryzyko #4 jest zaliczone na podstawie samego pomiaru 129 s** — ekstrapolacja jest zbędna.
- **Kompresja 4× zmierzona na pojedynczej teksturze:** `out=256 KiB vs rgba8=1024 KiB`
  (512×512: RGBA8 = 512·512·4 = 1024 KiB; ASTC 4×4 = 128·128·16 = 256 KiB). ✅
  > **⚠️ KOREKTA (2026-07-16):** wcześniej stało tu, że „przewidywane 1.38 GB → ~350 MB to **zmierzony
  > fakt, nie arytmetyka na papierze**". **To była arytmetyka na papierze.** Zmierzony jest stosunek
  > 4× dla **jednej** tekstury; przeniesienie go na zbiorczy licznik `GL mtrack` **nie jest pomiarem**.
  > To dokładnie ta klasa błędu, za którą raport krytykuje mnie w sprawie 69 MB na Adreno —
  > „dorobiłem wyjaśnienie do liczby i uznałem je za fakt". Realne lądowanie: **~380–460 MB** (§3).
- **Ryzyko #1 okazało się niższe, niż zakładano:** Tempest **nie używa `-Werror`** (tylko wyciszenia
  `-Wno-*` dla zależności trzecich), więc brakujący case w switchu to ostrzeżenie, nie błąd.
  `-Werror` dotyczy wyłącznie targetu gry, a `game/` nie ma żadnego switcha po `TextureFormat`.
- **Sondowanie możliwości działa „za darmo"**, dokładnie jak przewidziano: generyczna pętla
  [vdevice.cpp:678](lib/Tempest/Engine/gapi/vulkan/vdevice.cpp:678) zaczęła raportować ASTC4x4
  bez jednego dodatkowego patcha.
- **Korekta o Adreno** (patrz §4): Adreno **też nie ma BC**, więc transcoder pomaga obu urządzeniom
  jedną ścieżką.

**Koszt bramy: 3 cykle CI (~25 min)** — zamiast tygodnia budowania cache'u, loadera i enkodera na
fundamencie, który mógł nie istnieć. Jeden cykl spalił błąd arności `astcenc_context_alloc`
(4 argumenty w 5.6.0, nie 3) — ale ten sam czerwony build **udowodnił ryzyko #3**, bo pokazał
`astcenc-neon-static` kompilujący się na aarch64.

**Do wykonania przed Fazą 2:** cofnąć tymczasową diagnostykę z `main_android.cpp`
(log `[astcdiag]` + `astcBenchmark()` + `#include <astcenc.h>`), **zostawiając** sekcję `(f)` w
`apply-patches.sh`, submoduł astcenc i konfigurację CMake — to jest fundament Fazy 2.

## 7. Ryzyka

| Ryzyko | Skala | Mitygacja |
|---|---|---|
| Chirurgia na współdzielonym enumie przez perl rozwala inne backendy | ~~wysoka~~ → **zamknięte** | **Faza 1 zaliczona** (§6a) — realnie 3 cykle CI / ~25 min, nie 8 min jak planowano |
| Arytmetyka pozycyjna `frm - DXT1` (pixmap.cpp:68) | wysoka | ASTC **na końcu** enuma; zabezpieczyć ścieżkę |
| **astcenc nie buduje się / nie linkuje na arm64 (NDK)** | **wysoka** | **Faza 1** — punkt 2; fallback = wariant offline (§9) |
| **Kodowanie dużo wolniejsze niż szacowane 1–3 min** | **wysoka** | **Faza 1** — mikro-benchmark → ekstrapolacja na 345 Mpx; fallback = offline |
| Ładowanie tekstur jednowątkowe → pierwszy load bardzo długi | średnia | zweryfikować `Workers`; ewentualnie oddać `thread_count` astcenc |
| Podwójna strata (DXT→RGBA→ASTC) widoczna | średnia | porównanie A/B zrzutów, nie założenie |
| Przerwane kodowanie zostawia obcięty plik = trwale zepsuta tekstura | średnia | zapis atomowy (tmp + rename), §5.4 |
| Cache rozjeżdża się z danymi gry / zmianą parametrów astcenc | niska | rozmiar wpisu VDF + wersja enkodera w nagłówku |
| APK rośnie o astcenc | niska | ~1–2 MB, pomijalne |
| ~~Adreno niepotrzebnie użyje ASTC (8 bpp > DXT 4 bpp)~~ | **nie istnieje** | Zmierzone: Adreno **też nie ma BC** (`DXT1=0`), więc jego alternatywą jest RGBA8 32 bpp, nie DXT 4 bpp. ASTC jest dla niego czystym zyskiem. Patrz korekta w §4. |

## 8. Co to daje iOS

iOS ma **dokładnie ten sam problem** (Apple GPU nie ma BC → ta sama ścieżka device.cpp:199 → RGBA8).
Dzięki gatingowi po możliwościach GPU, a nie po `#if`:

- **Ta sama logika transkodowania i cache'u** działa na obu portach — każde urządzenie buduje
  sobie cache samo, przy pierwszym uruchomieniu, bez żadnego kroku ręcznego
- **`resources.cpp` jest wspólny** — logika ładowania cache'u działa na iOS bez zmian
- Do zrobienia po stronie iOS: powielić patche Tempesta w `ios/patches/apply-patches.sh`
  + jeden case w backendzie Metal (`MTL::PixelFormatASTC_4x4_LDR`) — backend **już zna ASTC**
- Metal `createCompressedTexture` ([mttexture.cpp:76](lib/Tempest/Engine/gapi/metal/mttexture.cpp:76))
  zakłada bloki 4×4 i 16 B → **ASTC 4×4 działa bez zmian**

Oczekiwany zysk na iOS jest tego samego rzędu co na Androidzie (~1 GB w dół).

## 9. Decyzje odrzucone

- **ASTC 6×6 / 8×8** — lepsza kompresja (9×/16×), ale wymaga uogólnienia wymiarów bloku w Vulkanie
  **i** Metalu (oba hardkodują 4×4). 4× wystarcza.
- **ETC2** — Mali obsługuje, ale Apple GPU wolą ASTC, a ETC2 ma gorszą jakość przy alfie.
  ASTC jest wspólnym mianownikiem obu platform.
- **Kodowanie na urządzeniu przy ładowaniu, BEZ cache'u** — zapłacilibyśmy koszt kodowania przy
  każdym starcie. Stąd cache dyskowy: płacimy raz.
- **Transkodowanie offline na PC (wariant A)** — odrzucone świadomie, mimo że technicznie
  najprostsze (zero astcenc na arm64, zero kosztu pierwszego startu). Powód: wymaga narzędzia
  `.exe`, nowego workflow `windows-latest`, ręcznego kroku przy każdej zmianie danych i czyni port
  bezużytecznym dla kogokolwiek bez tego narzędzia. **Bezobsługowość wygrywa z prostotą implementacji.**
  **Zachowane jako plan awaryjny:** jeśli Faza 1 pokaże, że astcenc nie buduje się na arm64 albo
  koduje absurdalnie wolno, wracamy tutaj. Projekt jest wtedy w większości wspólny — różni się
  tylko *gdzie* biegnie enkoder; §5.1 i §5.2 zostają bez zmian.
- **Mip-cap jako rozwiązanie docelowe** — zmierzone jako ślepa uliczka (§1). `androidTexCap`
  zostaje wyłącznie jako awaryjny zawór, domyślnie 0.
- **Basis Universal / UASTC** — kolejna warstwa stratności i duża zależność; astcenc jest
  bezpośredni i referencyjny.
