# Adreno 619: SIGSEGV kompilatora shaderów — śledztwo

Data pierwotnego śledztwa: 2026-07-17

Status zaktualizowany: 2026-07-20

**Status: potwierdzony deterministyczny crash kompilatora shaderów sterownika
Qualcomm. Rozszerzona bisekcja nie znalazła obejścia. Pełne shadery zostały
przywrócone, a dalsze prace nad tym urządzeniem są odłożone za optymalizacją
docelowego Mali-G57.**

Sterownik nie powinien kończyć procesu przez SIGSEGV, więc jest to rzeczywisty
błąd sterownika. Dotychczasowe testy nie dowodzą jednak, że zmiana SPIR-V,
layoutu deskryptorów lub uproszczenie ścieżki non-bindless nie może ominąć
wadliwej ścieżki kompilatora.

Urządzenie docelowe portu, Mali-G57, nie wykazuje tego problemu i pozostaje
grywalne.

## Objaw

Samsung Galaxy A23 5G:

- GPU: Adreno 619;
- sterownik Samsunga: 512.548.0;
- Vulkan: 1.3.128.

Proces deterministycznie otrzymuje SIGSEGV w `libllvm-glnext.so`, wewnątrz
`vkCreateGraphicsPipelines`, podczas tworzenia pierwszego teksturowanego
pipeline'u 3D nowej gry.

Pozorny czas „około 2:05 po loadzie” odpowiada długości intro. Podczas wideo
nie renderuje się scena 3D. Pominięcie intro dotykiem skraca reprodukcję do
około 40 sekund.

Instrumentacja pierwszych użyć pipeline'ów wykazała:

- dwa warianty `lnd_d` z pustym fragment shaderem kompilują się;
- pierwszy wariant z fragment shaderem używającym tekstury kończy proces;
- po pominięciu HiZ crash przenosi się na pierwszy teksturowany pipeline
  gbuffera.

Obserwowany wzorzec to fragment shader łączący varyingi, sampling tekstury i
odczyt SSBO. Składniki tej kombinacji nie zostały jeszcze odizolowane jeden po
drugim.

## Wykonane eksperymenty

| # | Hipoteza | Eksperyment | Wynik |
|---|---|---|---|
| 1 | ASTC powoduje crash | `androidTexCap=512`, czyli ścieżka bez transcodingu | Identyczny crash |
| 2 | Problem dotyczy wyłącznie HiZ | `hizNoOccluders=1` | Crash przeniósł się do gbuffera |
| 3 | Winna jest runtime-sized tablica obrazów | `texture2D textureMain[1]`, SPIR-V sprawdzony | Identyczny crash |
| 4 | Winna jest dowolna tablica obrazów | Pojedyncze `texture2D` | Identyczny crash |
| 5 | Winna jest para osobny image + sampler | Combined `sampler2D` i odpowiednie rebindingi | Identyczny crash |
| 6 | Uszkodzony cache shaderów sterownika | Wyczyszczenie danych aplikacji | Identyczny crash |
| 7 | Można wymusić bindless | Pomiar `nonUniformIndexing=0` | Sprzęt nie udostępnia wymaganej ścieżki |
| 8 | Nieprawidłowy interfejs SPIR-V | Disassembly glslang: zgodne lokacje i `Flat` | Nie znaleziono błędu w sprawdzanym interfejsie |
| 9 | Zależność od stanu gry | Powtarzane przebiegi | Deterministyczny |
| 10 | VVL wskaże błąd dokładnie przy crashu | Warstwa Khronos 1.4.350.1 na urządzeniu | Brak nowych komunikatów między loadem a crashem; błędy istnieją przy starcie |
| 11 | Standardowa optymalizacja SPIR-V ominie problem | `spirv-opt -O` v2025.1 | Identyczny crash |

## Ważna korekta dotycząca Vulkan Validation

Pierwotna wersja raportu nazywała przebieg „spec-clean”. To było zbyt mocne.
Warstwa walidacyjna nie zgłosiła nowego VUID dokładnie między załadowaniem
świata a crashem, ale przy starcie procesu zgłosiła realne problemy:

- `VK_KHR_spirv_1_4` bez wymaganej zależności
  `VK_KHR_shader_float_controls`;
- capabilities descriptor indexing (`ShaderNonUniform`,
  `RuntimeDescriptorArray`, `StorageBufferArrayNonUniformIndexing`) w
  shaderach tworzonych na urządzeniu bez odpowiednich cech;
- flagi `UPDATE_AFTER_BIND_POOL` i `PARTIALLY_BOUND` bez włączonych features.

Nie potwierdzono, że te naruszenia bezpośrednio powodują crash graphics
pipeline'u, ale również ich nie wykluczono. Layouty utworzone przy starcie są
później używane przez renderer. Przed ostatecznym zgłoszeniem „czysty bug
sterownika” aplikacja powinna dojść do zera istotnych komunikatów VVL.

## Co pozostało w HEAD

Zweryfikowane na Mali-G57:

- `800b426b` — combined image+sampler w materiałach slot;
- `6345467e` — odpytywanie descriptor indexing promowanego do rdzenia
  Vulkan 1.2 oraz logowanie decyzji bindless;
- rewerty tymczasowej instrumentacji, validation-layer bundling i
  `spirv-opt -O` (`19ae540a`…`5c0d5bf8`).

Zmiany te nie rozwiązują Adreno, ale nie spowodowały regresji na Mali.

## Scalar slot-mode

Pierwotnie ścieżka `!BINDLESS` nie była prawdziwą ścieżką scalar slot.
Tekstura była pojedynczym combined samplerem, lecz shadery nadal deklarowały
tablice deskryptorów:

- `ibo[]`;
- `vbo[]`;
- `morphId[]`;
- `morph[]`.

Używanie w nich stałego indeksu `0` nie usuwało capability
`RuntimeDescriptorArray` z wygenerowanego SPIR-V. Commit `24d7a44f` przeniósł
IBO, VBO i bufory morph poza tablice w wariancie slot. CI od `01099bbf`
waliduje wszystkie wygenerowane moduły slot dla obu ABI i odrzuca
`RuntimeDescriptorArray` oraz `ShaderNonUniform`.

Ta poprawka usunęła rzeczywisty błąd przenośności SPIR-V, ale nie ominęła
crasha sterownika.

## Dalsza bisekcja z 2026-07-20

Eksperymenty zostały zbudowane przez CI i uruchomione na A23 pojedynczo:

- pusty/minimalny slot fragment shader;
- scalar slot IBO/VBO/morph bez runtime descriptor arrays;
- wyłączenie VSM na urządzeniu bez wymaganych descriptor features;
- minimalne moduły fragment shadera;
- Release SPIR-V bez informacji debugowych;
- depth-only pipeline bez fragment stage;
- minimalny depth vertex shader.

Minimalny depth-only vertex shader bez fragment stage przeszedł wcześniejszy
punkt HiZ. Crash przeniósł się do kolejnego tworzenia pipeline'u w
`DrawCommands::drawCommon`. To ważna korekta: problem nie jest przypisany do
jednego konkretnego shadera HiZ. Sterownik wpada w wadliwą ścieżkę ponownie,
gdy renderer dochodzi do następnego pipeline'u materiałowego.

Żaden wariant nie doprowadził A23 do pierwszej poprawnej klatki świata.
Commit `4665cd0c` usunął wszystkie moduły `TEMP_BISECT_*` i przywrócił pełne
shadery. Worktree jest czysty; nie istnieje lokalna „prawie poprawka”, którą
należałoby przypadkiem zachować.

## Stan i ewentualny następny krok

Adreno pozostaje problemem zgodności, nie priorytetem bieżącej optymalizacji
Helio G99. Jeśli śledztwo zostanie wznowione, nie należy powtarzać powyższej
drabinki. Następny użyteczny etap to:

1. dojść do zera istotnych komunikatów Vulkan Validation na A23;
2. wyeksportować dokładny SPIR-V, reflected layout i stan tworzenia pierwszego
   pipeline'u, który nadal crashuje po wariancie depth-only;
3. sprawdzić `VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT`;
4. zbudować minimalny reproducer poza OpenGothic;
5. porównać reproducer z Turnip/libadrenotools jako ręcznym, opt-in
   eksperymentem — bez automatycznego dołączania obcego sterownika do APK.

## Kryterium zakończenia śledztwa

Konkretną kombinację `Adreno 619 + Samsung 512.548.0` można oznaczyć jako
known-broken dopiero po spełnieniu wszystkich warunków:

- brak istotnych błędów Vulkan Validation;
- scalar slot-mode bez runtime descriptor arrays;
- minimalny fragment shader odtwarza crash;
- build bez debugowego SPIR-V również crashuje;
- problem odtwarza minimalny pipeline poza pełnym OpenGothic;
- ten sam reproducer działa na Mali i, jeśli dostępne, nowszym Adreno.

Nie należy na tej podstawie oznaczać całej rodziny Adreno jako
nieobsługiwanej.

## Werdykt architektoniczny

Osobny backend Android nie jest obecnie uzasadniony. Poprawna ścieżka Vulkan
non-bindless już istnieje i jest walidowana przez CI; błąd pozostał specyficzny
dla testowanego sterownika Qualcomm. Dalszy rozwój powinien upraszczać profil
renderowania mobilnego w istniejącym rendererze Tempesta, zamiast portować
silnik na GLES/ANGLE lub utrzymywać drugi backend.
