# OpenGothic iOS — zasady pracy

## Projekt i stack

- Projekt jest forkiem `Try/OpenGothic`, rozszerzonym o obsługę i optymalizacje iOS.
- Główny kod jest w C++ i Objective-C++; budowanie jest konfigurowane przez CMake.
- Kod iOS znajduje się w `ios/` oraz w plikach i sekcjach oznaczonych `__IOS__`.
- Zależności Git są zapisane jako submoduły w `.gitmodules`.

## Bezpieczna synchronizacja z upstream

- `origin` wskazuje na `tryk016/opengothic-ios`, a `upstream` na `Try/OpenGothic`.
- Nie scalać bezpośrednio `upstream/master` do `master`.
- Małe poprawki przenosić pojedynczo na osobną gałąź przez cherry-pick.
- Po każdej partii uruchomić co najmniej konfigurację/build iOS oraz test uruchomienia gry.
- Duże refaktory renderera, zmiany Tempest i przenoszenie `game/` do `common/` analizować jako osobne etapy.
- Chronić dodatki iOS, w szczególności Metal, sterowanie dotykowe/gamepad, audio, pamięć, safe area i haptykę.

## System agentów

- Główny agent koordynuje pracę, zachowuje istniejące zmiany użytkownika i raportuje ryzyko.
- Do zadań delegowanych używać osobnych, jasno wydzielonych zakresów plików.
- Nie cofać cudzych zmian i nie wykonywać destrukcyjnych operacji Git bez zgody użytkownika.
- Każdą istotną sesję i decyzję synchronizacyjną dopisywać do `.Codex/session-log.md`.

## Weryfikacja

- Przed zmianami sprawdzić czystość drzewa roboczego i zapisać punkt bazowy.
- Preferować małe, odwracalne commity.
- Konflikt rozwiązany tekstowo nie oznacza poprawności; wymagany jest build i test na urządzeniu/symulatorze.
