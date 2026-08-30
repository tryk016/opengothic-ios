# Session log

## 2026-08-29 — analiza synchronizacji upstream

- Sklonowano `https://github.com/tryk016/opengothic-ios.git` do projektu.
- Dodano remote `upstream`: `https://github.com/Try/OpenGothic.git`.
- Punkt wspólny: `a2318ba228c1f334ff90a4c8558fdb3742f858a5`.
- Fork: 110 własnych commitów; upstream: 42 własne commity.
- Pełny merge uznano za ryzykowny ze względu na refaktor renderera i przeniesienie `game/` do `common/`.
- Wytypowano małe poprawki błędów do osobnego cherry-pick i weryfikacji.
- Bazowy szablon `~/Codex/templates/AGENTS.md` nie był dostępny na tym komputerze; utworzono minimalny lokalny zestaw zasad.
- Pobrano wszystkie submoduły projektu.
- Prawdziwy test cherry-pick potwierdził 8 commitów nakładających się bez konfliktów: `c9d39760`, `f05fe698`, `a8a6c7cd`, `3c7c996b`, `08ad6bff`, `a9b62ab7`, `cd3ccd41`, `22fa5f56`.
- Dwa przydatne commity wymagają ręcznego portu: `5b275b38` (ruch/skok) i `5c6fa0f9` (quick-load podczas dialogu).
- Ośmiocommitowa paczka oraz czysty `master` zatrzymują się identycznie podczas konfiguracji CMake na Xcode beta: OpenAL nie wykrywa PThreads. Błąd nie został wprowadzony przez kandydatów upstream.
- Nie utworzono gałęzi integracyjnej, nie zmieniono `master` i niczego nie wysłano do GitHuba.

## 2026-08-29 — lokalne zastosowanie bezpiecznej paczki

- Utworzono wyłącznie lokalną gałąź `codex/upstream-safe-fixes` od `master`.
- Nałożono 8 wcześniej sprawdzonych commitów; gałąź jest 8 commitów przed `master`.
- `git diff --check` nie zgłasza błędów formatowania.
- Xcode wykrywa podłączony `iPhone (Patryk)` oraz ważną tożsamość Apple Development.
- Xcode beta wymaga jeszcze instalacji systemowych komponentów obsługi urządzeń.
- Konfiguracja CMake nadal wymaga naprawy wykrywania PThreads/OpenAL; problem występuje także na bazowym `master`.
- Nie wykonano push do GitHuba ani instalacji aplikacji na telefonie.

## 2026-08-29 — naprawa konfiguracji Xcode 27

- Ustalono właściwą przyczynę fałszywego błędu PThreads: Tempest nadpisywał iOS deployment target z `15.0` na niewspierane przez Xcode 27 `12.0`.
- Rozszerzono `ios/patches/apply-patches.sh`, aby Tempest zachowywał deployment target projektu na iOS, nie zmieniając dotychczasowego minimum macOS.
- Skrypt jest idempotentny i przechodzi `bash -n`.
- Świeża konfiguracja CMake wykryła `pthread.h`, `HAVE_PTHREAD`, `Threads`, CoreAudio i Neon.
- Pełny build Release dla `iphoneos` zakończył się `BUILD SUCCEEDED`.
- Nie wykonano push do GitHuba ani instalacji aplikacji na telefonie.

## 2026-08-29 — podpisanie i instalacja na iPhonie

- Xcode został zalogowany do konta Apple zespołu `RMJWWPF379` i automatycznie utworzył profil deweloperski.
- Podpisany build Release dla urządzenia zakończył się `BUILD SUCCEEDED`.
- Pierwsza próba instalacji wykryła konflikt podpisu ze starszą aplikacją; żadnej istniejącej aplikacji ani jej danych nie usunięto.
- Dodano konfigurowalny `OPENGOTHIC_IOS_BUNDLE_IDENTIFIER`, domyślnie zachowujący dotychczasowe `opengothic.gothic2`.
- Kopię testową zbudowano jako `opengothic.gothic2.codex`, zainstalowano na `iPhone (Patryk)` i pomyślnie uruchomiono.
- Nie wykonano push do GitHuba.

## 2026-08-29 — widoczność katalogu Documents

- Potwierdzono przez kontener urządzenia, że `Documents` kopii testowej istnieje i zawiera dane gry, zapisy, `Gothic.ini` oraz logi.
- Problem z rozpoznaniem folderu wynikał z identycznej nazwy ekranowej trzech instalacji OpenGothic.
- Dodano konfigurowalny `OPENGOTHIC_IOS_DISPLAY_NAME`; domyślna nazwa pozostaje `OpenGothic`.
- Kopię `opengothic.gothic2.codex` przebudowano i zainstalowano jako `OpenGothic Test`, zachowując jej kontener danych.
- Nowa wersja została pomyślnie uruchomiona; niczego nie wysłano do GitHuba.

## 2026-08-29 — kontrolowany test na iOS 27

- Ustalono bezpośrednio z logu prywatnego `device_guard`, że wcześniejszy PID 32199 z Xcode był wielokrotnie zamykany przez zewnętrzną ochronę baterii; nie był to watchdog ani crash OpenGothic.
- Wszystkie miarodajne uruchomienia wykonano przez `device_guard.py run`; końcowy stan ochrony to `IDLE/ZERO/YES/FRESH`.
- Jedna pomocnicza próba pobrania stosu przez LLDB zakończyła aplikację sygnałem 9 podczas odłączania debuggera i nie jest traktowana jako wynik testu gry.
- Czysty zimny start starego renderera pozostawał na kompilacji Metal prawie całe 580-sekundowe okno; menu pojawiło się dopiero pod koniec. To wyjaśnia długi czarny ekran.
- Po rozgrzaniu cache'u kolejne starty przechodziły etap shaderów w kilka sekund.
- Podczas chronionych prób wielokrotnie wczytano świat; budowa packed world mesh i collision world kończyła się, a gra pozostawała stabilna przez kolejne minuty.
- Jedno uruchomienie zakończyło się po unieważnieniu połączenia CoreDevice/Mercury; wrapper utracił połączenie, po czym ochrona zamknęła aplikację. Nie był to crash gry.
- Ostatnia pięciominutowa próba obejmowała wejście do świata i zmianę opcji wideo; proces pozostał stabilny do planowego limitu testu.
- Zaobserwowano niekrytyczne ostrzeżenia o aliasach audio `ENV_NIGHT_TONSOFINSECTS` i `OW_BIRD11` oraz `TODO: apply_options_video`.
- Nie zmieniono kodu podczas testu, nie zmieniono folderu referencyjnego `/Users/patryk/Developer/opengothic-ios-metal` i niczego nie wysłano do GitHuba.

## 2026-08-29 — natywne osie kontrolera iOS

- Analiza `Oliver472/opengothic-ios` wykazała, że projekt nie obsługuje fizycznego Apple GameController, a jego ekranowy joystick także symuluje klawisze; nie przeniesiono z niego commitów.
- Normalny ruch postaci otrzymał bezpośredni stan `PadAxes` przekazywany do `PlayerControl`, radialną martwą strefę, skalowanie zakresu i histerezę przeciw dryfowi.
- Lewy analog w świecie nie generuje już syntetycznych `Forward/Back/RotateL/RotateR`; mała ścieżka dyskretna pozostała tylko dla UI oraz mechanik wymagających kroków, takich jak MOBSI, drabina i wytrych.
- Wyzerowanie osi obejmuje utratę kontrolera, zmianę kontekstu, zmianę generacji wejścia i dezaktywację sceny; dodano obsługę cyklu życia `UIScene`.
- Usunięto synchroniczne odpytywanie GameController z każdej klatki. Silnik odczytuje najnowszy snapshot publikowany przez callbacki frameworka.
- Zainicjalizowano tablice stanu ruchu, zabezpieczono osie przed `NaN` i usunięto możliwość nagromadzenia obrotu kamery po wcześniejszym wyjściu z logiki ruchu.
- Test czystej funkcji radialnej przeszedł dla środka, martwej strefy, osi, przekątnej, narożnika i `NaN`.
- Lokalny build `Gothic2Notr` Release dla `iphoneos` zakończył się `BUILD SUCCEEDED`; deployment target pozostaje na iOS 15.0, więc obejmuje iOS 16.4.
- Na życzenie użytkownika przerwano testy telefonu. Końcowego buildu sterowania nie zainstalowano ani nie uruchomiono na urządzeniu.
- `device_guard`, który wcześniej zamykał ręczne uruchomienia aplikacji, został jawnie zatrzymany; potwierdzono stan `daemon=STOPPED` i nie będzie ponownie uruchamiany w tym zadaniu.
- Folder referencyjny `/Users/patryk/Developer/opengothic-ios-metal` pozostał bez zmian. Nie utworzono commita i niczego nie wysłano do GitHuba.

## 2026-08-29 — ograniczony profil shaderów i gotowa biblioteka Metal

- Zimny start blokował się przed pierwszym menu w `Renderer::resetSwapchain()`, który czekał na cały asynchroniczny katalog shaderów po zmianie świeżego profilu rozdzielczości iOS. Na iOS usunięto tę przedwczesną barierę; pełne oczekiwanie pozostaje przed wejściem do świata.
- Bink i downscale mają osobne akcesory do synchronicznego zestawu startowego, więc intro i miniatura zapisu nie wymuszają oczekiwania na katalog świata. `shared_future::get()` propaguje błąd kompilatora przed użyciem pustego pipeline.
- Produkcyjny profil iOS kompiluje VSM, RTSM, GI, SWRT i CMAA2 tylko wtedy, gdy wybrana konfiguracja może ich użyć. Wariant materiałowy VSM jest bramkowany opcją i wsparciem urządzenia. Diagnostyczne shadery ray-query są pomijane w Release, a funkcjonalny Marvin pathtrace pozostaje dostępny przy jawnym `doRayQuery`.
- Tempest deduplikuje identyczne moduły SPIR-V w pamięci urządzenia Metal, z pełnym porównaniem bytecode po szybkim hashu, więc kolizja nie może zwrócić niewłaściwego shadera. Cache ma limit LRU 16 wpisów, a moduły compute są usuwane natychmiast po utworzeniu PSO, aby nie zatrzymywać bibliotek Metal podczas wczytywania świata.
- Build generuje i pakuje 7.8 KiB `OpenGothicStartup.metallib` z bezpiecznych `triangle.vert` i `downscale.frag`. Entry pointy są nazwane prefiksem SHA-256 pełnego SPIR-V; `metal-objdump` potwierdził `og_1b4a268bceeaba50` i `og_74917a05b3218293` zgodne z plikami wejściowymi.
- Bink celowo pozostaje na runtime SPIR-V -> MSL, ponieważ jego nieskończone tablice SSBO wymagają tempestawego slotu długości 29, którego użyty CLI SPIRV-Cross nie odtwarza. Przy braku assetu, braku hasha lub odrzuceniu biblioteki działa dotychczasowy bezpieczny fallback.
- Nie dodano zapisywalnego runtime `MTLBinaryArchive`. Ewentualny read-only archive wymaga późniejszego capture i walidacji na reprezentatywnych urządzeniach.
- Do lokalnego skryptu i obu workflowów IPA dodano zależność `spirv-cross`, aby build wydania nie wyłączał optymalizacji po cichu. Skrypt shell oraz YAML obu workflowów przeszły walidację.
- Z głównego OpenGothic ręcznie przeniesiono poprawkę dangling pointer przy quick-load z aktywnego dialogu. Z Tempest przeniesiono poprawny `<new>` dla signal storage i monotoniczny `steady_clock` dla sleep.
- Jedyne masowo powielane ostrzeżenie Xcode 27 usunięto przez współczesną składnię pięciu operatorów literałów OpenAL. Pozostałe `has no symbols` dotyczą pustych lub wyłączonych backendów i nie mają ryzyka runtime.
- Generator ma osobne targety Metal dla urządzenia i symulatora; ręczna kompilacja wariantu `iphonesimulator` oraz sprawdzenie jego symboli przeszły.
- Osobna konfiguracja iOS z `OPENGOTHIC_IOS_PRECOMPILED_STARTUP_SHADERS=OFF` przeszła, potwierdzając ścieżkę buildu bez assetu.
- Przełączniki GI/VSM/RTSM/pathtrace mają na iOS nieblokującą bramkę gotowości future, więc tryb deweloperski nie może czytać pipeline'ów równolegle z workerem kompilacji.
- Pełny Release `iphoneos27.0`, arm64, target iOS 15.0 zakończył się `BUILD SUCCEEDED`; aplikacja jest podpisana, przechodzi walidację codesign i zawiera bibliotekę Metal. Ostatni przyrostowy build nie zawiera ostrzeżeń kompilatora ani błędów; pozostają tylko nieszkodliwe komunikaty linkera o pustych obiektach wyłączonych backendów. `git diff --check` przeszedł w repo głównym i Tempest.
- Końcowy niezależny review Sol XHigh zakończył się PASS bez pozostałych problemów P0/P1/P2. Do pomiaru na urządzeniu pozostaje pierwszy start i koszt leniwego tworzenia PSO; obecny `.metallib` nie jest `MTLBinaryArchive`.
- Zgodnie z poleceniem nie uruchomiono ani nie zainstalowano aplikacji na telefonie, nie uruchomiono ponownie `device_guard`, nie zmieniono folderu referencyjnego, nie utworzono commita i niczego nie wysłano do GitHuba.

## 2026-08-30 — test urządzenia profilu Metal

- Podpisany Release zainstalowano jako aktualizację `opengothic.gothic2.codex`, zachowując istniejący kontener `Documents`, zasoby i save'y.
- Test wykonano na fizycznym iPhone 15 Pro Max / Apple A17 Pro pod działającym `device_guard`; ochrona zakończyła się w stanie `FRESH`, `IDLE`, `game=ZERO`.
- Log urządzenia potwierdził załadowanie `OpenGothicStartup.metallib` oraz użycie obu funkcji `og_1b4a268bceeaba50` i `og_74917a05b3218293`.
- Profil produkcyjny zgłosił wyłączone nieaktywne grupy: VSM, RTSM, GI1, GI2, SWRT, CMAA2 i debug.
- Użytkownik wczytał świat; packed world mesh i collision world zostały zbudowane dwukrotnie, proces pozostał żywy, obraz i sterowanie działały. Nie odnotowano crasha ani błędu shaderów.
- Monitoring zakończono ręcznie po potwierdzeniu użytkownika, aby nie obciążać baterii. Nie wykonano push ani commita.

## 2026-08-30 — końcowa walidacja MetalFX i przygotowanie publikacji

- Zmiany Tempest niewidoczne w gitlinku zapisano jako `ios/patches/tempest-ios-runtime-stability.patch` i dołączono na końcu idempotentnego `apply-patches.sh`.
- Łatkę odtworzono na czystym Tempest `61b58f710b00f64d190fed2661f5762909397d1a`; wynik całego katalogu `Engine` jest identyczny z lokalnym drzewem przetestowanym na urządzeniu, a drugie uruchomienie skryptu niczego nie zmienia.
- Końcowy wariant bez MetalFX pozostaje tym samym profilem, który wcześniej przeszedł test urządzenia: `OPENGOTHIC_METALFX_SPATIAL=OFF` i `OPENGOTHIC_METALFX_TEMPORAL=OFF`.
- Osobny Release z `OPENGOTHIC_METALFX_SPATIAL=ON` oraz `OPENGOTHIC_METALFX_TEMPORAL=ON` zakończył się `BUILD SUCCEEDED`, został podpisany profilem deweloperskim i zainstalowany jako aktualizacja `opengothic.gothic2.codex` bez usuwania kontenera danych.
- Pierwsze ręczne uruchomienie MetalFX zostało po 10 sekundach planowo zakończone przez `device_guard`, ponieważ nie miało aktywnej dzierżawy; log ochrony potwierdził zewnętrzny `SIGTERM`, nie crash aplikacji.
- Miarodajny test uruchomiono przez dziesięciominutową dzierżawę guarda. Log potwierdził A17 Pro, `OpenGothicStartup.metallib`, oba gotowe entry pointy i ograniczony profil shaderów.
- Użytkownik dwukrotnie wczytał świat i potwierdził, że wszystko działa. Proces pozostał stabilny przez kilka minut, bez wyjątku, sygnału awarii ani błędu MetalFX; test zakończono kontrolowanie po potwierdzeniu.
- Oba buildy zachowują minimum iOS 15.0, więc są zgodne z iOS 16.4.
