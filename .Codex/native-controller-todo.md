# Natywne sterowanie kontrolerem na iOS — checkpointy

Cel: zastąpić symulowanie klawiatury dla analogów bezpośrednim, klatkowym
stanem kontrolera przekazywanym do silnika. Docelowa platforma to wyłącznie iOS
16.4+; sterowanie klawiaturą pozostaje niezależne i nie ogranicza projektu API.

## 0. Zasady bezpieczeństwa

- [x] Pracować tylko lokalnie na `codex/upstream-safe-fixes`.
- [x] Nie wykonywać push do GitHuba bez osobnej zgody użytkownika.
- [x] Nie zmieniać `/Users/patryk/Developer/opengothic-ios-metal`.
- [x] Zachować istniejące niezwiązane zmiany w drzewie roboczym.
- [x] Przed pierwszą partią zapisać punkt wycofania: `d337bac51663ef0eb648b0910dee349b6719837e`; pliki sterowania nie miały lokalnego diffu.

## 1. Rozpoznanie

- [x] Prześledzić obecną ścieżkę `GameController -> GamepadState -> GamepadInput -> PlayerControl`.
- [x] Potwierdzić, że lewy analog nadal generuje akcje `Forward/Back/RotateL/RotateR` jak klawiatura.
- [x] Potwierdzić, że obrót ma osobny analog, ale ruch przód/tył pozostaje cyfrowy.
- [x] Zbadać rozwiązanie z `Oliver472/opengothic-ios`: nie ma fizycznego GameController; joystick także symuluje klawisze, więc nie jest źródłem implementacji.
- [x] Porównać rozwiązanie z zachowaniem aktualnego Apple GameController SDK.
- [x] Ustalić kontrakt etapu 1: ciągłe osie w `PlayerControl`, dyskretne adaptery tylko dla UI/interakcji, bez skalowania root-motion.

## 2. Nowy model wejścia

- [x] Dodać czysty stan analogowy lewego drążka przekazywany do `PlayerControl` co tick.
- [x] Zastosować radialną martwą strefę i ponowne skalowanie zakresu bez lepkiego stanu kierunku.
- [x] Rozdzielić ciągłe osie od zboczy przycisków; przyciski pozostają zdarzeniami.
- [x] Wyzerować osie natychmiast po wejściu w martwą strefę, utracie kontrolera, utracie aktywności i zmianie świata/kontekstu.
- [x] Usunąć symulowane `Forward/Back/RotateL/RotateR` z normalnego ruchu postaci.
- [x] Zostawić małą, świadomą ścieżkę dyskretną tylko tam, gdzie mechanika naprawdę wymaga kroków: menu, dialog, wytrych i ewentualnie drabina.

## 3. Zachowanie w grze

- [ ] Ruch z miejsca zaczyna się bez szarpnięcia i kończy po puszczeniu drążka.
- [ ] Małe wychylenia nie powodują samoczynnego chodu.
- [ ] Obrót ma płynną prędkość zależną od wychylenia.
- [ ] Prawy analog steruje kamerą bez dryfu i skoków po wolnej klatce.
- [ ] Wczytanie świata, pauza, inventory i powrót aplikacji nie przenoszą starego stanu osi.
- [ ] Sterowanie działa poprawnie podczas walki, pływania, nurkowania, wspinania, używania drabiny i wytrychów.
- [ ] Rozłączenie i ponowne połączenie kontrolera nie zostawia żadnej akcji aktywnej.

## 4. Weryfikacja

- [x] Dodać i uruchomić test czystej funkcji deadzone/skalowania, w tym środka, narożników i wartości `NaN`.
- [x] Zbudować Release dla fizycznego iPhone'a; target projektu to iOS 15.0, więc zachowuje zgodność z iOS 16.4.
- [x] Sprawdzić diff pod kątem błędów formatowania; `git diff --check` przechodzi bez uwag.
- [ ] Zainstalować lokalną kopię testową przez kontrolowany `device_guard`.
- [ ] Test urządzenia: neutral, delikatne wychylenie, pełny ruch, szybka zmiana kierunku i puszczenie.
- [ ] Test urządzenia: menu -> świat, save/load, background/foreground, disconnect/reconnect.
- [ ] Test urządzenia: walka wręcz, łuk/magia, drabina, wytrych, pływanie i kamera.
- [x] Dopisać wynik i znane ograniczenia do `.Codex/session-log.md`.

Testy urządzenia są wstrzymane na życzenie użytkownika. Końcowy build ze zmianami
sterowania nie został zainstalowany ani uruchomiony na telefonie. `device_guard`
został całkowicie zatrzymany i nie będzie ponownie uruchamiany w tym zadaniu.

## 5. Dopiero po akceptacji użytkownika

- [ ] Przygotować mały lokalny commit z samym sterowaniem.
- [ ] Push/release wykonać wyłącznie po wyraźnej zgodzie użytkownika.
