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
