# DXT→ASTC transcoder — projekt (Android + iOS)

Data: 2026-07-16
Status: zatwierdzony kierunek (wariant A — transkodowanie offline na PC), do implementacji fazami
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

Dla porównania **Galaxy A23 / Adreno 619** (ma BC → tekstury zostają skompresowane): **GPU 69 MB**.
Ta różnica (69 MB vs 1.38 GB) jest całym problemem.

### Dlaczego mip-cap to ślepa uliczka (zmierzone, nie szacowane)

`androidTexCap=512` oszczędza **tylko 60 MB**, bo tekstury Gothica 2 (gra z 2003) są w większości
≤512 px — cap 512 prawie niczego nie tyka. Żeby cap cokolwiek dał, trzeba zejść do 256, a wtedy
zgniata *dominującą* grupę 512-tek (+420 MB różnicy między 256 a 512) i **widać klocki**.
**Nie ma złotego środka**: 1.38 GB nie bierze się z kilku wielkich tekstur, tylko z dekompresji
setek średnich do RGBA8. Cap umie tylko wymieniać rozdzielczość na pamięć.

**ASTC atakuje prawdziwą przyczynę**: trzyma tekstury skompresowane w **pełnej rozdzielczości**.

## 2. Czego to NIE naprawia

**Transcoder naprawia pamięć, nie FPS.** Zmierzone 16.2 FPS (pełna jakość, komnata Xardasa,
z warstwy BLAST SurfaceFlingera) wynika z `vidResIndex=0` (4× pikseli) i `modelDetail`, nie z tekstur.
Tekstury kosztują pamięć, nie fill rate. Nie mylić tych dwóch problemów.

## 3. Kluczowa decyzja: ASTC 4×4 (nie 6×6)

ASTC 4×4 = **blok 4×4, 16 bajtów** = 8 bpp. RGBA8 = 32 bpp → **4× oszczędności**: 1.38 GB → **~350 MB**,
przy **zachowaniu pełnej rozdzielczości**.

6×6 (3.56 bpp) dałoby 9× (~150 MB), ale **oba backendy hardkodują bloki 4×4**:

- Vulkan/Pixmap: [pixmap.cpp:452](lib/Tempest/Engine/formats/pixmap.cpp:452) `blockSizeForFormat`:
  DXT1=8, DXT3=16, DXT5=16 (bajty na blok 4×4)
- Metal: [mttexture.cpp:76-83](lib/Tempest/Engine/gapi/metal/mttexture.cpp:76)
  `blockSize = (frm==DXT1) ? 8 : 16;` + `wBlk=(w+3)/4, hBlk=(h+3)/4`

**ASTC 4×4 wpasowuje się w oba bez tknięcia matematyki bloków.** 6×6 wymagałoby uogólnienia
wymiarów bloku w Vulkanie *i* Metalu. Dlatego: **4×4**. 4× wystarcza (288 MB → ~1.3 GB wolnego).

## 4. Architektura

```text
[PC / Windows]                       [urządzenie: Android lub iOS]
tools/astc-transcode.exe             OpenGothic
  ├─ ZenKit: czyta VDF                 ├─ Resources::implLoadTextureUncached
  ├─ ZenKit: Texture::as_rgba8(lvl)    │    └─ if(!hasSamplerFormat(DXT))
  │    (dekoduje DXT → RGBA8)          │         → szukaj <cache>/<NAZWA>.astc
  ├─ astcenc: RGBA8 → ASTC 4×4         │         → znaleziony: Pixmap(ASTC4x4)  ← skompresowana!
  │    (pełny łańcuch mipów)           │         → brak: dotychczasowe zachowanie (RGBA8)
  └─ zapis <NAZWA>.astc  ──push──►     └─ Tempest: próbkuje ASTC natywnie
```

**Gating przez możliwości GPU, nie przez `#if`.** Warunek to `!hasSamplerFormat(DXT1)`:

- Mali → brak BC → używa cache ASTC
- **Apple GPU → brak BC → używa tego samego cache ASTC** (iOS działa za darmo)
- Adreno → **ma** BC → ignoruje cache, zostaje przy natywnym DXT (4 bpp, lepsze niż ASTC 8 bpp)

To jest powód, dla którego projekt jest platform-neutralny: **jeden cache, oba porty**.

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

### 5.3 tools/astc-transcode — narzędzie na PC

Mały program C++, **bez Tempesta i bez Vulkana** — tylko:
- **ZenKit** (już jest submodułem): `Vfs` czyta VDF-y, `Texture::load` + `as_rgba8(lvl)` dekoduje DXT
- **astcenc** (ARM, nowy vendored/submoduł): koduje RGBA8 → ASTC 4×4

Pętla: dla każdej tekstury `*-C.TEX` w VDF-ach → dla każdego poziomu mip → `as_rgba8(lvl)` →
astcenc → zapis pliku `.astc` (standardowy nagłówek 16 B) z pełnym łańcuchem mipów.

**Uwaga o jakości:** DXT jest już stratny, więc DXT→RGBA→ASTC to **strata drugiej generacji**.
Dla gry z 2003 powinno być wizualnie nieodróżnialne, ale należy to sprawdzić na zrzutach
(porównanie A/B komnaty Xardasa), a nie zakładać.

**Uwaga o czasie:** astcenc jest wolny. Na PC (x86, wielowątkowo, preset `-fast`/`-medium`)
~2000 tekstur to minuty — akceptowalne jako krok jednorazowy. To jest **cały powód**, dla którego
wybraliśmy wariant A zamiast kodowania na urządzeniu.

### 5.4 CI

Nowy workflow `windows.yml` (`runs-on: windows-latest`) budujący **tylko** target `astc-transcode`
(bez Vulkan SDK, bez Tempesta) → publikuje `astc-transcode.exe` jako asset release'u.
Obecnie repo ma tylko `android.yml` (ubuntu) i `ios.yml` (macos) — **nie ma builda desktopowego**.

### 5.5 Workflow użytkownika

1. Pobierz `astc-transcode.exe` z release'u
2. `astc-transcode.exe -g "C:\...\Gothic II" -o astc-cache`
3. `adb push astc-cache /sdcard/OpenGothic/astc` (iOS: przez ten sam kanał co dane gry)
4. Gra sama wykryje cache i użyje go, jeśli GPU nie ma BC

## 6. Fazowanie (de-ryzykowanie)

**Faza 1 — fundament, ~1 cykl CI (8 min).**
Same patche Tempesta z 5.1 + log przy starcie: `hasSamplerFormat(ASTC4x4)` oraz `hasSamplerFormat(DXT1)`.
Bez narzędzia, bez cache'u, bez enkodera.

**Kryterium sukcesu:** logcat na Tab A9 pokazuje `ASTC4x4=true, DXT1=false`, gra działa jak dziś
(zero regresji). To weryfikuje **najbardziej kruchą część** (chirurgia na współdzielonym enumie
przez perl + realne wsparcie ASTC na Mali) zanim cokolwiek na niej zbudujemy.

Jeśli Faza 1 padnie → wiemy po 8 minutach, a nie po zbudowaniu narzędzia i enkodera.

**Faza 2 — narzędzie + cache + ładowanie.**
Dopiero gdy Faza 1 potwierdzi grunt: 5.2 + 5.3 + 5.4, weryfikacja A/B jakości i pomiar pamięci.

**Kryterium sukcesu Fazy 2:** GPU spada z 1.38 GB do ~350 MB przy **niezmienionej ostrości**
(porównanie zrzutów), wolny RAM rośnie z 288 MB do ~1.3 GB, brak regresji crashy.

**Faza 3 (opcjonalna, później).** Kodowanie na urządzeniu (astcenc na arm64 + cache + UI postępu),
gdyby kiedyś potrzebne było zero kroków na PC. Fundament z Fazy 1 jest wtedy już gotowy.

## 7. Ryzyka

| Ryzyko | Skala | Mitygacja |
|---|---|---|
| Chirurgia na współdzielonym enumie przez perl rozwala inne backendy | **wysoka** | **Faza 1** — weryfikacja za 8 min |
| Arytmetyka pozycyjna `frm - DXT1` (pixmap.cpp:68) | wysoka | ASTC **na końcu** enuma; zabezpieczyć ścieżkę |
| Podwójna strata (DXT→RGBA→ASTC) widoczna | średnia | porównanie A/B zrzutów, nie założenie |
| astcenc wolny / duży cache na dysku | niska | krok jednorazowy na PC; ~350 MB plików |
| Cache rozjeżdża się z danymi gry | niska | narzędzie uruchamiane ręcznie; ewentualnie hash w nagłówku |
| Adreno niepotrzebnie użyje ASTC (8 bpp > DXT 4 bpp) | niska | gating `!hasSamplerFormat(DXT1)` — Adreno pomija cache |

## 8. Co to daje iOS

iOS ma **dokładnie ten sam problem** (Apple GPU nie ma BC → ta sama ścieżka device.cpp:199 → RGBA8).
Dzięki gatingowi po możliwościach GPU, a nie po `#if`:

- **Ten sam cache ASTC** działa na obu portach — narzędzie na PC uruchamiasz raz
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
- **Kodowanie na urządzeniu przy ładowaniu** — astcenc za wolny, zniszczyłby czasy ładowania.
- **Mip-cap jako rozwiązanie docelowe** — zmierzone jako ślepa uliczka (§1).
- **Basis Universal / UASTC** — kolejna warstwa stratności i duża zależność; astcenc jest
  bezpośredni i referencyjny.
