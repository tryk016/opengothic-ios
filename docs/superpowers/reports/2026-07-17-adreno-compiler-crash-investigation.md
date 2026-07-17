# Adreno 619: SIGSEGV kompilatora shaderów — śledztwo (2026-07-17)

Status: **potwierdzony bug sterownika, nieobejściowalny po stronie aplikacji żadną z 11 prób**.
Urządzenie docelowe portu (Mali-G57) **nie jest dotknięte** i pozostaje w pełni grywalne.

## Objaw

Galaxy A23 (Adreno 619, blob Samsunga 512.548.0, Vulkan 1.3.128): deterministyczny SIGSEGV
w `libllvm-glnext.so` (kompilator shaderów sterownika) wewnątrz `vkCreateGraphicsPipelines`,
przy pierwszej klatce 3D nowej gry. Timing „~2:05 po loadzie" to po prostu długość wideo intro —
podczas wideo nie renderuje się nic; tapnięcie w ekran pomija wideo i przyspiesza crash do ~40 s
(odkrycie użytkownika — najszybsza pętla repro).

Sekwencja z instrumentacji pierwszych użyć pipeline'ów: dwa warianty `lnd_d` (solid depth,
pusty FS) kompilują się, trzeci — pierwszy z FS czytającym teksturę — zabija kompilator.
Z pominiętym passem HiZ crash przenosi się na pierwszy pipeline gbuffera (też samplujący).
Wzorzec bez kontrprzykładu: **FS z varyings+teksturą+odczytem SSBO ⇒ crash; FS pusty ⇒ OK**.

## Macierz falsyfikacji (wszystko zmierzone na urządzeniu)

| # | Hipoteza | Eksperyment | Wynik |
|---|---|---|---|
| 1 | ASTC / Faza 2 | `androidTexCap=512` omija transcoder | crash identyczny |
| 2 | Specyfika passa HiZ | `hizNoOccluders=1` | crash w gbufferze |
| 3 | Runtime-sized tablica obrazów | `texture2D textureMain[1]` (SPIR-V zweryfikowany) | crash identyczny |
| 4 | Jakakolwiek tablica obrazów | gołe `texture2D` | crash identyczny |
| 5 | Osobny image+sampler | combined `sampler2D` + rebinding | crash identyczny |
| 6 | Uszkodzony cache shaderów sterownika | `pm clear` | crash identyczny |
| 7 | Ucieczka w bindless | pomiar: `nonUniformIndexing=0` — sprzęt nie wspiera | niedostępne |
| 8 | Nielegalny SPIR-V (interfejs/Flat) | disasm glslang -H: lokacje zgodne, Flat obecny | czysty |
| 9 | Stan gry / scena | 3/3 reprodukcje w różnych przebiegach | deterministyczny |
| 10 | **(D)** naruszenie spec w miejscu crashu | warstwa walidacyjna Khronos 1.4.350.1 wstrzyknięta on-device | **zero komunikatów od loadu do crashu — spec-clean** |
| 11 | **(F)** wzorce instrukcji SPIR-V | `spirv-opt -O` (v2025.1) nad wszystkimi shaderami | crash identyczny |

## Werdykt opcji D w szczegółach

Walidacja zgłosiła naruszenia **wyłącznie przy starcie** i wyłącznie w zestawie compute/fixed,
który **działa** (kompilator compute je toleruje): `VK_KHR_spirv_1_4` bez wymaganego
`VK_KHR_shader_float_controls`; capabilities descriptor-indexing (`ShaderNonUniform`,
`RuntimeDescriptorArray`, `StorageBufferArrayNonUniformIndexing`) w bezwarunkowo kompilowanych
compute shaderach (`visibility_pass` używa `nonuniformEXT` bez gated wariantu) na urządzeniu bez
tych cech; flagi layoutów `UPDATE_AFTER_BIND_POOL`/`PARTIALLY_BOUND` bez włączonych cech.
To realne pozycje higieniczne pod upstream — ale nie przyczyna crashu.

## Co zostało w drzewie (zweryfikowane na Mali: 8 min, animowane materiały OK, bindless nietknięty)

- **`800b426b`** — combined image+sampler w materiałach slot (forma bardziej standardowa;
  `L_Sampler` nieużywany w slot; bindowanie w `drawcommands.cpp` i `pfxbucket.cpp` parą (tex,smp))
- **`6345467e`** — sekcja (g) apply-patches.sh: detekcja descriptor-indexing promowanego do rdzenia
  w Vulkan 1.2+ (Adreno nie ogłasza stringa rozszerzenia; Tempest bramkował na stringu) +
  warunkowy `rqExt.push_back` + log `[caps]` wejść decyzji bindless
- Rewerty TEMP-ów: instrumentacja, debuggable+warstwa, spirv-opt (`19ae540a`..`5c0d5bf8`)

## Pozostałe drogi

- **(B)** Issue do Try/OpenGothic — komplet repro gotowy (backtrace, macierz, SPIR-V lokalnie
  odtwarzalny, sygnatura sterownika). Świeży upstreamowy „Hiz v2 (#952)" dotyka okolic.
- **(C-soft)** Udokumentować stary blob Adreno 619 jako known-broken i wrócić przy sprzęcie
  z nowszym sterownikiem (nowsze bloby 6xx / Adreno 7xx mają inne kompilatory).

## Nauka metodyczna

Dwie rzeczy, które w tym śledztwie zapłaciły za siebie: (1) tani eksperyment środowiskowy
(`pm clear`) powinien był pójść przed mutacjami shaderów, nie po nich; (2) warstwa walidacyjna
on-device (opcja D) dała w jednym cyklu więcej niż trzy cykle mutacji — następnym razem
zaczynać od niej, gdy podejrzany jest sterownik.
