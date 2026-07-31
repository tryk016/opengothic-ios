# Helio G99 / Mali-G57 MC2 — profil wydajności Android

Data pomiarów: 2026-07-20

Urządzenie: Samsung Galaxy Tab A9 / SM-X115, Android 15, Helio G99,
Mali-G57 MC2, około 3,5 GB pamięci dostępnej dla systemu.

## Wynik

Port jest stabilny i grywalny funkcjonalnie, ale cel stałych 30 FPS w świecie
gry nie został osiągnięty. W pokoju Xardasa bezpieczny renderer utrzymuje około
15–16 FPS. Menu utrzymuje około 29,7 FPS przy aktywnym limicie 30.

Nie znaleziono ograniczenia RAM ani throttlingu:

- proces zajmuje około 1,28–1,35 GB PSS z gotowym cache ASTC;
- status termiczny Androida pozostawał równy `0`;
- po obciążeniu raportowano około 54–56°C dla CPU/GPU, poniżej pierwszego
  progu throttlingu urządzenia;
- logika, animacje i przygotowanie komend mieszczą się wyraźnie poniżej
  budżetu 33,3 ms.

Wąskim gardłem jest wykonanie/presentacja pracy GPU, nie główna pętla logiki.

## Konfiguracja bezpiecznego profilu

```ini
[INTERNAL]
vidResIndex=2
androidTexCap=0

[ENGINE]
zCloudShadowScale=0
zMaxFpsMode=1
shadowResolution=512

[PERFORMANCE]
sightValue=2
```

Znaczenie:

- half-resolution rendering;
- ASTC 4×4 z trwałym cache;
- SSAO/cloud shadows wyłączone;
- klasyczne dynamiczne mapy cieni 512 px;
- cel 30 FPS;
- far plane 60 000.

## Pomiary SurfaceFlinger

Pomiary obejmują 126 dodatnich odstępów prezentacji warstwy BLAST. Scena
porównawcza to rozmowa/początek gry w pokoju Xardasa.

| Wariant | Średnia klatka | p50 | p95 | FPS |
|---|---:|---:|---:|---:|
| Pełna rozdzielczość, shadow 512 | 59,29 ms | 59,17 ms | 62,72 ms | 16,87 |
| Half, shadow 256 | 63,15 ms | 63,20 ms | 66,54 ms | 15,83 |
| Half, shadow 512, sight 60 km | 64,25 ms | 64,23 ms | 67,33 ms | 15,56 |
| Half, shadow 512, sight 20 km | 64,32 ms | 64,35 ms | 67,31 ms | 15,55 |

Obniżenie rozdzielczości, rozdzielczości map cieni i far plane nie poprawiło
tej sceny. Nie należy przenosić wniosku o `sightValue` na otwarte Khorinis bez
osobnego, stałego punktu pomiarowego.

## Telemetria CPU / kolejki Vulkan

Commit `4878973a` dodał pomiar p95 trzech części wywołania renderu. W
ustabilizowanej scenie świata odnotowano w przybliżeniu:

| Etap | p95 |
|---|---:|
| tick gry | 7,8–9,0 ms |
| animacje | 4,7–4,9 ms |
| odświeżenie pose | 2,3–4,2 ms |
| kodowanie komend renderera | 5,6–5,8 ms |
| `vkQueueSubmit` | 0,4 ms |
| present + następne `vkAcquireNextImageKHR` | 50,9–62,6 ms |

Brakowało fence misses. Menu dla porównania miało około 1,5 ms kodowania i
5,6 ms present/acquire, utrzymując 29,7 FPS. Długi czas present/acquire w
świecie oznacza oczekiwanie na obrazy zajęte przez GPU/kompozytor.

## Zastosowane, bezpieczne zmiany

- `zMaxFpsMode` działa na Androidzie jako Off / 30 / 60;
- pacing używa dokładnego monotonicznego `sleep_until`, bez desktopowego
  busy-spin;
- far plane reaguje na `sightValue`;
- niewidoczne/odległe NPC wykonują events-only zamiast pełnego pose;
- dialog nie wymusza pełnej animacji wszystkich 1053 NPC;
- tryb immersive usuwa paski systemowe i odzyskuje pełny obszar dotyku;
- profil zachowuje half resolution, ASTC, wyłączone SSAO i cienie 512 px.

W typowym ustabilizowanym oknie około 58 NPC wykonywało pełny pose, a około
995 przechodziło ścieżką events-only.

## Odrzucone eksperymenty

### Pominięcie HiZ i naprzemienne kaskady

Eksperyment `ad7f13a2` nie został zainstalowany na urządzeniu. Review wykrył
przed testem:

- brak czyszczenia głównego depth bufferu;
- konsumentów próbkujących niezbudowane HiZ;
- mapę cienia z poprzedniej klatki używaną z macierzą bieżącej klatki.

`4878973a` przywrócił poprawną ścieżkę, zachowując wyłącznie telemetrię.

### Wyłączenie dynamicznych map cieni

Tryb `shadowResolution=0` podniósł wskaźnik ekranowy do około 22,7 FPS, ale
oświetlenie zostało poważnie uszkodzone — scena była niemal całkowicie biała.
Eksperyment został natychmiast zatrzymany, urządzenie wróciło do 512 px, a kod
trybu został wycofany w `0e1f0a6b`.

Nie wolno przedstawiać tego wyniku jako działającej opcji wydajnościowej.

## Walidacja końcowego artefaktu

Workflow
[29746062340](https://github.com/tryk016/opengothic-ios/actions/runs/29746062340)
zbudował commit `0e1f0a6b`, zweryfikował wszystkie 84 generowane moduły
shaderów i zakończył się powodzeniem. SHA-256 APK:

```text
CBC5DBAF822467C66855682CC5EC2B3453AB529AFDC278C5C016BDC54D7D4365
```

Hash artefaktu workflow i publicznego `latest-android` był identyczny.
Dokładnie ten APK zainstalowano na SM-X115. Zweryfikowano menu, wejście do
świata, poprawne oświetlenie w pokoju Xardasa oraz Home/resume z niezmienionym
PID. Po wznowieniu nie wystąpił SIGSEGV ani utrata urządzenia Vulkan.

## Wniosek architektoniczny

Tempest Vulkan pozostaje właściwą bazą Androida. Problemem nie jest koszt
samego API ani potrzeba drugiego backendu, tylko desktopowy graf renderowania
z wieloma seriami geometrii. Oficjalne zalecenia dla mobilnych GPU kafelkowych
podkreślają koszt zbędnych passów, pracy geometrycznej i transferów między
passami. To wspiera dalszy *profil mobilny wewnątrz Tempesta*, nie osobny
renderer GLES.

Źródła:

- [Khronos — Tile-based rendering best practices](https://docs.vulkan.org/guide/latest/tile_based_rendering_best_practices.html)
- [Arm — Mali application developer best practices](https://developer.arm.com/community/arm-community-blogs/b/mobile-graphics-and-gaming-blog/posts/new-developer-guide-arm-mali-application-developer-best-practices)

Następna faza powinna użyć znaczników czasu GPU lub narzędzia AGI/Perfetto,
a następnie usuwać po jednym poprawnie odizolowanym passie. Kandydaci to
bezpieczny mobilny wariant HiZ/depth oraz pełny wariant direct-light bez map
cieni, ale każdy wymaga testu poprawności oświetlenia, głębi, GI, fog i
lifecycle. Stałe 30 FPS nie mogą być kryterium zaliczone na podstawie samego
menu albo licznika bez pomiaru p95 w świecie.

---

## Uzupełnienie 2026-07-31 — pomiary kompozytora i kwantyzacja vsync

Wszystkie liczby poniżej pochodzą z `dumpsys SurfaceFlinger` na Tab A9 podczas
gry (scena pokoju Xardasa) oraz z linii `PERF v=1` tej samej sesji.

### Kompozycja jest w 100% po stronie GPU

```
totalFrames = 329   clientCompositionFrames = 329   clientCompositionReusedFrames = 0
usesDeviceComposition = false   usesClientComposition = true
warstwa gry: CLIENT | ROT_90 | displayFrame 800x1340 | sourceCrop 1340x800
```

SurfaceFlinger komponuje **każdą** klatkę gry passem GPU na tym samym
Mali-G57, zamiast oddać warstwę sprzętowemu kompozytorowi. Ta praca nie
znajduje się w buforze komend aplikacji, więc żaden wewnętrzny timer GPU jej
nie zmierzy, a jej koszt trafia do `cpu_present_p95_ms`.

Bezpośrednia przyczyna niezdatności do overlayu jest geometryczna: bufor gry ma
1340x800, a display frame 800x1340.

### Hipoteza „obrót kompozytora kosztuje” — ODRZUCONA

Eksperyment: build bez patcha `preTransform=IDENTITY` (commit `27e4ff91`,
cofnięty w `534da025`). Kompozytor przestaje obracać, obraz prezentuje się
bokiem, ale praca GPU gry jest bajt w bajt ta sama.

| Wariant | frame_p50 | FPS | kompozycja |
|---|---:|---:|---|
| Baseline (`preTransform=IDENTITY`, kompozytor obraca) | 61,36 ms | 16,27 | CLIENT 329/329 |
| Bez obrotu (`preTransform=currentTransform`) | 64,38 ms | 15,50 | CLIENT 287/287 |

Zdjęcie obrotu **nie poprawiło** czasu klatki — wynik jest nieznacznie gorszy.
Kompozycja pozostała w 100% CLIENT, bo niezgodność geometrii bufora i display
frame trwa niezależnie od deklarowanej transformacji.

Wniosek: sam obrót nie jest istotnym kosztem. **Nie zostało natomiast
zmierzone**, ile kosztuje kompozycja CLIENT jako taka, ponieważ w żadnym
wariancie nie udało się uzyskać kompozycji DEVICE. Wymagałaby ona prawdziwej
pre-rotacji (render do bufora zgodnego z panelem), a wobec powyższego wyniku
nie ma podstaw, by oczekiwać po niej dużego zysku.

### Czas klatki jest skwantowany do vsync — to unieważnia wcześniejsze werdykty

Panel: 800x1340, **60 Hz**, vsync 16,67 ms. Histogram present-to-present
warstwy gry (800 klatek, `droppedFrames = 0`, `averageFPS = 15,997`):

```
66 ms = 630 klatek   (4 x vsync)
48 ms = 143 klatek   (3 x vsync)
50 ms = 22,  16 ms = 5,  32 ms = 1
```

Klatki lądują wyłącznie na wielokrotnościach vsync. Oznacza to, że **oszczędność
kilku milisekund jest w FPS niewidoczna**, dopóki nie przekroczy progu koszyka.

To lepiej tłumaczy wcześniejsze wyniki niż dotychczasowa interpretacja.
Zapisano tam, że half resolution, niższa rozdzielczość map cieni i krótszy far
plane „nie poprawiły” sceny. Poprawnie brzmi to: **nie przekroczyły progu
vsync**. Nie wynika z tego, że nie zmniejszyły kosztu. Wyłączenie map cieni
zadziałało (16 -> 22,7 FPS), ponieważ jako jedyne próg przekroczyło.

**Konsekwencja metodyczna: mierzyć czas, nie FPS.** Progi dla tego panelu:
< 50 ms daje 20 FPS, < 33,3 ms daje 30 FPS.

Aplikacja nie zgłasza też preferencji odświeżania
(`requestedFrameRate: {0.00 Hz}`) — nie zbadano, czy `Surface.setFrameRate`
cokolwiek tu zmienia.

### Dlaczego nie powstały timestampy GPU per pass

Projekt (timestamp na istniejących `setDebugMarker`) został sprawdzony pod
kątem wykonalności i jest wykonalny: pula zapytań mieści się w cyklu życia
`VCommandBuffer`, reset legalnie ląduje w `begin()`, a fence slotu pozwala
czytać wyniki bez stalla. Oba urządzenia wspierają timestampy (Mali 76,92 ns na
takt, 64 bity; Adreno 52,08 ns, 48 bitów).

Adwersaryjna krytyka wykazała jednak, że **liczby byłyby mylące**:

- passy, o które chodzi (`DirectSunLight`, `AmbientLight`, `Point lights`,
  `Sky`), są znacznikowane **wewnątrz jednego render passa**; na GPU
  kafelkowym praca jest wykonywana kafel po kaflu i „moment pomiędzy nimi” nie
  istnieje;
- znaczniki stoją niekonsekwentnie po obu stronach `setFramebuffer`, więc koszt
  load/store kafla trafia raz do passa własnego, raz do poprzedniego;
- czas w milisekundach nie rozdziela pracy geometrycznej od fragmentowej, a to
  jest dokładnie ten podział, od którego zależy wybór między korektą obecnego
  renderera a przepisaniem na forward.

Wiarygodną liczbą z tego mechanizmu byłby wyłącznie `gpu_span_ms` (pierwszy do
ostatniego znacznika), czyli łączny czas zajętości GPU do zestawienia z okresem
klatki. Sam ranking passów wymagałby wcześniej normalizacji położenia
znaczników względem `setFramebuffer`.

### Rekomendowany następny krok

Eksperymenty ablacyjne, każdy jako osobny build raportujący istniejące
`frame_p50_ms`, bez nowego kodu pomiarowego:

1. render świata do scyzoryka 64x64 — zabija pracę fragmentową, zostawia
   geometrię, binning i culling; rozdziela dwie główne hipotezy kosztu;
2. kolejne ablacje pojedynczych passów, zawsze z kontrolą poprawności obrazu.

Wynik należy odczytywać w milisekundach i odnosić do progów 50 ms i 33,3 ms,
nie do samego licznika FPS.

## Uzupełnienie 2026-07-31 (2) — rozdział kosztu: fragmenty czy geometria

Wszystkie pomiary poniżej pochodzą z linii `PERF v=1` przy `scene=world`, scena
pokoju Xardasa na starcie nowej gry, ta sama sesja urządzenia, wartości
`frame_p50_ms` z ustabilizowanych okien 10-sekundowych.

### Rozdzielczość głównego widoku nie ma znaczenia

| Konfiguracja | frame_p50 | FPS |
|---|---:|---:|
| `vidResIndex=2` (ćwierć pikseli, bez AA, plus pass upscale) | 64,85 ms | 15,4 |
| `vidResIndex=0` (pełna rozdzielczość, plus CMAA2) | 63,65 ms | 15,7 |

Czterokrotne zmniejszenie liczby pikseli nie daje zysku; pełna rozdzielczość
jest nawet nieznacznie szybsza mimo dodatkowego antyaliasingu
(`aaEnabled = aaPreset>0 && vidResIndex==0`, renderer.cpp:314).

**Renderer nie jest ograniczony pracą fragmentową w tej scenie.**

Wcześniejszy zapis „obniżenie rozdzielczości nie poprawiło tej sceny” był
prawdziwy, ale zbyt łagodny. Poprawnie: half resolution **pogarsza** czas
klatki, ponieważ pass upscale kosztuje więcej, niż wynosi oszczędność na
fragmentach.

### Rozdzielczość map cieni: tania w punkcie pracy, droga powyżej

| `shadowResolution` | frame_p50 | Źródło |
|---|---:|---|
| 256 | 63,15 ms | pomiar 2026-07-20 |
| 512 (obecna) | 64,85 ms | pomiar 2026-07-31 |
| 1024 | 70,10 ms | pomiar 2026-07-31 |

Przejście 256 -> 512 kosztuje około 1,1 ms, natomiast 512 -> 1024 już około
5,2 ms. Fill map cieni zaczyna być istotny dopiero powyżej obecnego ustawienia.

### Wniosek: koszt leży w ponownym przesyłaniu geometrii

Zestawienie dźwigni zmierzonych do tej pory:

| Dźwignia | Zmiana czasu klatki |
|---|---|
| Rozdzielczość głównego widoku, ćwierć pikseli | około 0 |
| Draw distance 60 km -> 20 km | około 0 (64,32 vs 64,25 ms) |
| Rozdzielczość cieni 512 -> 256 | około -1,1 ms |
| Całkowite wyłączenie map cieni | około -18 ms (16 -> 22,7 FPS) |

Jedyną dźwignią o dużym efekcie jest samo istnienie map cieni, a nie ich
rozdzielczość. Przy `ShadowLayers = 2` (resources.h:53) scena jest renderowana
dodatkowo dwa razy przez `WorldView::drawShadow`. Skoro zmiana liczby pikseli w
tych passach kosztuje około 1 ms, a ich usunięcie około 18 ms, to dominującą
częścią jest **przesyłanie i przetwarzanie geometrii, nie rasteryzacja**.

**Zastrzeżenie do liczby 18 ms:** pochodzi z eksperymentu z 2026-07-20
mierzonego licznikiem FPS, przy uszkodzonym oświetleniu, a kod tego trybu
został wycofany (`0e1f0a6b`). Należy ją traktować jako rząd wielkości, a nie
pomiar równorzędny z pozostałymi. Wymaga powtórzenia w milisekundach na
poprawnym wariancie.

### Konsekwencja dla wyboru wariantu architektonicznego

Wariant B (przepisanie na mobile forward) odzyskuje przede wszystkim
przepustowość G-bufora i koszt osobnego passa oświetlenia, czyli pracę
fragmentową i pasmo. Pomiary wskazują, że **to nie jest obecne wąskie gardło**.
Forward nadal renderowałby kaskady cieni tą samą geometrią.

Tańsze kierunki, zgodne z powyższymi danymi:

1. ograniczenie kaskad cieni do jednej;
2. uproszczona geometria dla passów cieni, na przykład agresywniejszy cull lub
   niższy poziom szczegółowości;
3. aktualizacja map cieni rzadziej niż co klatkę dla części statycznej;
4. redukcja liczby wywołań rysowania i lepsze grupowanie.

Każdy z nich wymaga osobnego pomiaru w milisekundach, w scenie świata.

### Otwarte pytanie: domyślna rozdzielczość

`vidResIndex=0` wyszło ostrzejsze i nieznacznie szybsze od obecnego domyślnego
`vidResIndex=2`. Zmiana domyślnej wartości wymaga jednak wcześniejszego pomiaru
w scenie otwartej, ponieważ wszystkie powyższe liczby pochodzą z wnętrza.
