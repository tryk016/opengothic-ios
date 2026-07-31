# Roadmapa: obejście crasha kompilatora Adreno 619 na poziomie silnika

Data: 2026-07-21
Gałąź: `android` (worktree `E:\claude\opengothic-android`).

**⛔ OGRANICZENIE (użytkownik, 2026-07-21):** fork `github.com/tryk016/Tempest`, branch
`renderer-ios`, jest **wyłącznie pod iOS** — NIE wolno tam niczego mergować ani tworzyć w nim
gałęzi roboczych Androida. Jeśli zmiany silnika przestaną mieścić się w `apply-patches.sh`,
trzeba najpierw uzgodnić z użytkownikiem osobny byt (własny fork/worktree), a nie sięgać po ten fork.

## Cel i zasada

Doprowadzić Galaxy A23 (Adreno 619 / blob Samsunga 512.548.0 / Vulkan 1.3.128) do pierwszej
poprawnej klatki świata, obchodząc deterministyczny SIGSEGV w `libllvm-glnext.so` wewnątrz
`vkCreateGraphicsPipelines`. **Każdy etap = JEDNA zmienna → jeden build CI → jeden test na
urządzeniu → jasna bramka pass/fail.** Kolejność: najtańsze i najwyżej rokujące najpierw.

## Co już wiadomo (nie powtarzać)

- Śledztwo z 2026-07-17 + rozszerzona bisekcja z 2026-07-20: **zmiany źródła shaderów nie omijają
  crasha**. Neutralizacja pierwszego pipeline'u przenosi crash na następny materiałowy
  (`DrawCommands::drawCommon`) — więc problem to KLASA pipeline'u, nie jeden shader.
- `libllvm-glnext` = optymalizator LLVM sterownika Qualcomma → crash jest w fazie **optymalizacji**,
  nie parsowania. To przesuwa punkt ciężkości ze źródła shadera na **parametry tworzenia pipeline'u**.
- Tempest używa **dynamic rendering** (`VkPipelineRenderingCreateInfoKHR`, [vpipeline.cpp:132/295](../../lib/Tempest/Engine/gapi/vulkan/vpipeline.cpp)),
  bez pipeline cache (`VK_NULL_HANDLE`, [vpipeline.cpp:303](../../lib/Tempest/Engine/gapi/vulkan/vpipeline.cpp)),
  a `pipelineInfo.flags` jest domyślnie 0 ([vpipeline.cpp:277](../../lib/Tempest/Engine/gapi/vulkan/vpipeline.cpp)).
- Mali-G57 (urządzenie docelowe) DZIAŁA — używa ścieżki bindless; Adreno nie ma
  `nonUniformIndexing`, więc idzie ścieżką slot. **Każda zmiana silnika MUSI przejść regresję na Mali.**
- Skrót repro: tapnięcie w intro pomija wideo → crash w ~40 s zamiast ~2:05.
- Agenci-subagenci chwilowo niedostępni (miesięczny limit wydatków) — roadmapa wykonalna inline.

## Mechanizm zmian (kiedy apply-patches.sh, kiedy fork)

- **Faza 1 (jednolinijkowe flagi):** przez `android/patches/apply-patches.sh`, na pinie submodułu
  `61b58f7` — zero tarcia, zgodne z dotychczasowym przepływem. **Fork niepotrzebny.**
- **Faza 2+ (przepisanie ścieżki render pass / timestampy GPU):** perl patche stają się
  niepraktyczne → potrzebny osobny byt na zmiany Tempesta. **NIE `tryk016/Tempest`** (iOS-only,
  patrz ograniczenie wyżej) — do uzgodnienia z użytkownikiem, gdy realnie tam dojdziemy.

---

## FAZA 0 — Fundament (bez zmian silnika, bez ryzyka regresji)

- [x] **0.1/0.2 ANULOWANE** — zakładały branch roboczy w `tryk016/Tempest`. Ten fork jest iOS-only
  (ograniczenie użytkownika 2026-07-21). Dopóki zmiany mieszczą się w `apply-patches.sh`, żaden
  fork Tempesta nie jest potrzebny; gdy przestaną, ustalamy osobny byt z użytkownikiem.
- [ ] **0.3** Skrypt jednego przebiegu repro (scratchpad): install APK → `am start` → czekaj 12 s →
  tap-skip intro (`input swipe 2174 994 2174 994 120`) → poll `pidof` co 30 s → przy śmierci
  wyciągnij ostatnie `[hizdiag]`/backtrace do pliku. Bramka: skrypt odpala się i produkuje werdykt.
- [ ] **0.4** Baseline crash na aktualnym HEAD (`b94a6f1d`): odpalić 0.3, potwierdzić SIGSEGV
  w `libllvm-glnext`. Bramka: crash potwierdzony → każdy kolejny wynik jest czystym A/B.

## FAZA 1 — Dźwignie parametrów pipeline'u (via apply-patches.sh)

Kolejność wg (wartość × taniość). Po KAŻDYM kroku: build → test 0.3 → zapis wyniku → decyzja.

- [ ] **1.1 `VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT`** na graphics pipeline (tylko `#if defined(__ANDROID__)`).
  Jedna linia: `pipelineInfo.flags |= VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;` po
  [vpipeline.cpp:291](../../lib/Tempest/Engine/gapi/vulkan/vpipeline.cpp). **NAJWYŻSZY priorytet:**
  `libllvm-glnext` to optymalizator LLVM; ta flaga każe sterownikowi pominąć wadliwe passy optymalizacji.
  - Bramka: A23 dochodzi do klatki świata? TAK → potencjalny fix, przejdź do 1.4 (weryfikacja) + regresja Mali.
    NIE → zdejmij, następny krok.
- [ ] **1.2 Higiena VVL — zero istotnych VUID przy starcie.** Trzy realne naruszenia z opcji D
  (2026-07-17): (a) `VK_KHR_spirv_1_4` bez `VK_KHR_shader_float_controls` — dodać zależne rozszerzenie
  do `rqExt`; (b) capabilities descriptor-indexing (`ShaderNonUniform`/`RuntimeDescriptorArray`/
  `StorageBufferArrayNonUniformIndexing`) w bezwarunkowo kompilowanych compute shaderach na urządzeniu
  bez cech — gate wariantów compute na `hasDescIndexing`; (c) flagi layoutu `UPDATE_AFTER_BIND_POOL`/
  `PARTIALLY_BOUND` bez włączonych features — gate w vpipelinelay.cpp. Robić jako **trzy osobne
  mikro-kroki** (1.2a/b/c), każdy z własnym buildem — bo każdy może być samodzielną przyczyną
  ORAZ jest wymagany do czystego zgłoszenia B. Bramka każdego: dany VUID znika z logu VVL na A23.
  Bramka zbiorcza: zero istotnych VUID → jeśli crash nadal jest, mamy „naprawdę czysty" build do B.
- [ ] **1.3 Bez `VK_PIPELINE_CREATE_ALLOW_DERIVATIVES`/pochodnych** — sprawdzić, czy ścieżka
  derywatów ([vpipeline.cpp:333/386](../../lib/Tempest/Engine/gapi/vulkan/vpipeline.cpp)) jest aktywna
  na Androidzie i czy jej wyłączenie zmienia obraz. Tani eksperyment. Bramka: A/B crash.
- [ ] **1.4** (tylko jeśli któraś dźwignia z 1.x zadziałała) Weryfikacja fixa: pełny bieg do świata
  na A23 + soak 5 min + zrzut sceny Xardasa + **regresja Mali** (bindless nietknięty, ale sprawdzić).
  Bramka: A23 gra, Mali bez regresji → LAND (Faza 5).

## FAZA 2 — Klasyczny VkRenderPass zamiast dynamic rendering (wymaga osobnego bytu na Tempest)

Podejmowana TYLKO jeśli Faza 1 nie dała fixa. Hipoteza: wadliwa jest ścieżka dynamic-rendering
blobu Adreno, nie same shadery.

- [ ] **2.1** Zmapować w forku, jak `VPipeline::instance` wybiera dynLay vs rpass
  ([vpipeline.cpp:293-295](../../lib/Tempest/Engine/gapi/vulkan/vpipeline.cpp)) i kto na Androidzie
  ustawia `renderPass=VK_NULL_HANDLE`. Bez zmian kodu — tylko mapa. Bramka: wiadomo, gdzie wpiąć rpass.
- [ ] **2.2** Uzgodnić z użytkownikiem miejsce na zmiany Tempesta (NIE `renderer-ios`), przełączyć
  submoduł, pierwszy pusty commit + zielone CI (dowód, że łańcuch build działa, zanim zmienimy
  renderer). Bramka: CI zielone, APK identyczny.
- [ ] **2.3** Prototyp: dla passów materiałowych na Androidzie utworzyć minimalny `VkRenderPass`
  zgodny z formatami dynamic-rendering i przekazać `rpass` zamiast `dynLay`. NAJMNIEJSZY zakres —
  najpierw sam pass HiZ (depth-only, jeden pipeline), żeby zawęzić. Bramka: A23 mija dawny punkt crashu?
- [ ] **2.4** Rozszerzyć na gbuffer/forward, jeśli 2.3 pomogło. Bramka: A23 dochodzi do świata.
- [ ] **2.5** Regresja Mali + zrzuty A/B (render pass zmienia ścieżkę OBU GPU). Bramka: brak regresji.

## FAZA 3 — Minimalny reproducer poza OpenGothic

Podejmowana jeśli Faza 1-2 nie dały fixa (crash to twardy bug sterownika). Cel: dowód + baza pod B i Turnip.

- [ ] **3.1** Standalone Vulkan app (arm64, NDK): jeden pipeline odwzorowujący crashujący —
  dynamic rendering, slot layout, FS z varyings+tekstura+SSBO, programmable vertex pulling. Bramka:
  reprodukuje SIGSEGV na A23.
- [ ] **3.2** Minimalizacja: usuwać po jednym elemencie aż crash zniknie → izolowany trigger.
  Bramka: najmniejszy pipeline, który jeszcze crashuje.
- [ ] **3.3** Kontrola: ten sam reproducer na Mali (ma przejść) i, jeśli dostępny, nowszy Adreno.

## FAZA 4 — Turnip / libadrenotools (ręczny opt-in)

- [ ] **4.1** Załadować Turnip (Mesa) przez libadrenotools jako RĘCZNY eksperyment — **bez** automatycznego
  dołączania obcego sterownika do APK. Bramka: reproducer z 3.x przechodzi na Turnip → potwierdza bug blobu.
  (To ścieżka diagnostyczna/uświadamiająca, nie plan dystrybucji.)

## FAZA 5 — Domknięcie

- [ ] **5.1 Jeśli fix znaleziony:** wyczyścić instrumentację, LAND na `android`, regresja Mali,
  zaktualizować raport + pamięć, oznaczyć Adreno jako WSPIERANY.
- [ ] **5.2 Jeśli fix NIE znaleziony:** zgłoszenie do Try/OpenGothic (opcja B) z pełnym repro
  (backtrace, macierz, reproducer z 3.x, sygnatura sterownika, wynik czystego VVL z 1.2, wynik Turnip
  z 4.1). Oznaczyć `Adreno 619 + blob 512.548.0` jako known-broken (NIE całą rodzinę Adreno).
  Mali pozostaje priorytetem i jest grywalne.

## Kryteria decyzji między fazami

- Faza 1 → 2: wszystkie dźwignie 1.x wyczerpane bez fixa.
- Faza 2 → 3: render pass nie pomógł LUB zbyt inwazyjny/regresyjny na Mali.
- 3 → 4 → 5: sekwencyjnie; 5.2 gdy 1-4 nie dały obejścia.
- Punkty wymagające decyzji użytkownika: przełączenie submodułu na fork (2.2), wysłanie issue B (5.2),
  ewentualna dystrybucja z Turnip (poza zakresem tej roadmapy).
