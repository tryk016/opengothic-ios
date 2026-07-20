# Adreno 619: SIGSEGV kompilatora shaderów — śledztwo

Data pierwotnego śledztwa: 2026-07-17

Status zaktualizowany: 2026-07-20

**Status: potwierdzony deterministyczny crash kompilatora shaderów sterownika
Qualcomm. Jedenaście wykonanych prób nie znalazło obejścia, ale dochodzenie po
stronie aplikacji i Tempesta pozostaje otwarte.**

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

## Najważniejszy niewykonany eksperyment

Ścieżka `!BINDLESS` nie jest jeszcze prawdziwą ścieżką scalar slot. Tekstura
jest pojedynczym combined samplerem, lecz shadery nadal deklarują tablice
deskryptorów:

- `ibo[]`;
- `vbo[]`;
- `morphId[]`;
- `morph[]`.

Używanie w nich stałego indeksu `0` nie usuwa capability
`RuntimeDescriptorArray` z wygenerowanego SPIR-V. Najbardziej obiecującym
workaroundem jest wariant bez zewnętrznych tablic deskryptorów dla wszystkich
tych buforów, pozostawiający tablice wyłącznie pod `BINDLESS`.

W bieżącym reproduktorze landscape najważniejsze są IBO/VBO. Bufory morph
dotyczą wariantów obiektów morfowanych i nie zostały wskazane jako bezpośredni
składnik pierwszego crashującego pipeline'u.

## Lokalny stan diagnostyczny po raporcie

Worktree zawiera niezacommitowany eksperyment `TEMP_BISECT_A` w:

- `shader/materials/main.frag`;
- `shader/materials/materials_common.glsl`.

Eksperyment kończy slotowy fragment shader przed użyciem tekstury, varyingów
i SSBO. Nie został zbudowany przez CI ani uruchomiony na Adreno, więc nie jest
wynikiem i nie należy go traktować jako poprawki.

Sam pusty `return` może pozwolić kompilatorowi usunąć niemal całe ciało
fragment shadera. Kolejny wariant powinien zapisywać prawidłowe stałe wartości
do wymaganych attachmentów.

## Skorygowany plan

1. Uruchomić istniejący `TEMP_BISECT_A` bez innych zmian.
2. Wykonać drabinkę fragment shadera:
   - stałe wyjścia;
   - varying bez tekstury;
   - tekstura ze stałym UV;
   - tekstura z varying UV;
   - SSBO bez tekstury;
   - pełny materiał.
3. Zbudować Release SPIR-V bez `-g` i osobno ze `--strip-debug`.
4. Utworzyć prawdziwy scalar slot-mode bez tablic IBO/VBO/morph.
5. Zalogować finalny wynik refleksji Tempesta i wszystkie bindingi
   `VkDescriptorSetLayout`.
6. Wyłączyć VSM oraz inne shadery `nonuniformEXT`, gdy urządzenie nie ma
   wymaganych capabilities.
7. Usunąć wszystkie istotne błędy Vulkan Validation.
8. Sprawdzić `VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT`.
9. Przypiąć i logować wersję glslang/SPIRV-Tools w CI.
10. Jeżeli uproszczona, validation-clean ścieżka nadal crashuje, przygotować
    minimalny reproducer dla Qualcomm/upstream.

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

Osobny renderer Android nie jest obecnie uzasadniony. Najpierw należy
zbudować poprawną, prostą ścieżkę Vulkan non-bindless w istniejącym
rendererze. Ma to mniejszy koszt i większą szansę powodzenia niż portowanie
silnika na GLES/ANGLE lub tworzenie drugiego backendu.
