# Zimny start iOS — profil shaderów i cache Metal

Cel: skrócić czas od uruchomienia aplikacji do widocznego menu bez przenoszenia
niedokończonego `RendererIOS`. Każda optymalizacja musi mieć bezpieczny fallback
do obecnej ścieżki SPIR-V -> MSL -> Metal.

## 0. Granice

- [x] Nie zmieniać `/Users/patryk/Developer/opengothic-ios-metal`.
- [x] Nie instalować ani nie uruchamiać buildu na telefonie w tej partii.
- [x] Nie wykonywać push ani publikacji.
- [x] Zachować deployment target iOS 15.0, a więc zgodność z iOS 16.4.

## 1. Rozpoznanie

- [x] Zmapować `GothicShader -> MtShader -> MTLLibrary -> PSO`.
- [x] Potwierdzić brak trwałego cache po stronie Tempest.
- [x] Oddzielić koszt kompilacji biblioteki MSL od tworzenia pipeline state.
- [x] Zidentyfikować opcjonalne grupy kompilowane mimo wyłączonej funkcji.
- [x] Sprawdzić API Metal i ograniczenia `MTLBinaryArchive` dla iOS 16.4.

## 2. Ograniczony profil startowy

- [x] Na iOS kompilować GI, VSM, RTSM i SWRT tylko, gdy dana funkcja jest aktywna.
- [x] Kompilować CMAA2 tylko dla wybranego presetu i pominąć shadery debugowe w profilu produkcyjnym.
- [x] Nie blokować pierwszego menu na pełnym `compileShaders()`.
- [x] Zachować pełne oczekiwanie przed pierwszym wejściem do świata.
- [x] Dodać log z liczbą/rodzajem aktywnych grup profilu.

## 3. Gotowa biblioteka Metal

- [x] Wygenerować podczas buildu małą `OpenGothicStartup.metallib` tylko dla prostych shaderów startowych bez zależnego ABI SSBO.
- [x] Nadać funkcjom unikalne entrypointy, aby można je było zlinkować w jedną bibliotekę.
- [x] Załadować bibliotekę opcjonalnie z bundle i użyć jej tylko przy zgodnym shaderze.
- [x] Przy braku, niezgodności lub błędzie automatycznie wrócić do runtime SPIR-V -> MSL.
- [x] Pozostawić Bink na ścieżce runtime: wymaga niestandardowego slotu bufora długości Tempest, którego CLI SPIRV-Cross nie odtwarza.
- [x] Nie dodawać zapisywalnego runtime `MTLBinaryArchive`; Apple zaleca cache produkcyjny jako asset przygotowany dewelopersko.

## 4. Weryfikacja lokalna

- [x] Sprawdzić symbole wygenerowanej `.metallib`.
- [x] Zbudować pełny Release `iphoneos`.
- [x] Potwierdzić obecność `OpenGothicStartup.metallib` w `.app`.
- [x] Uruchomić `git diff --check`.
- [x] Przeprowadzić review Sol XHigh — PASS, bez pozostałych P0/P1/P2.

## 5. Weryfikacja odłożona

- [ ] Pomiar urządzenia: start po świeżej instalacji do pierwszego menu.
- [ ] Pomiar urządzenia: start po rozgrzaniu cache Metal.
- [ ] Test: menu, intro Bink, wejście do świata i zapis z miniaturą.
- [ ] Opcjonalnie wygenerować read-only `MTLBinaryArchive` z reprezentatywnego przejścia świata i dołączyć jako asset.
