# Enhanced Mode Improvement Roadmap

> Status: **Pågår – første Enhanced-grafikkprototype er implementert**
>
> Målplattform: PAL Amiga 500 OCS/68000 med 512 KB Chip RAM + 512 KB expansion RAM
>
> Omfang: Forbedringene nedenfor gjelder bare **Enhanced Mode**. Classic Mode skal fortsatt følge CPC-kontrakten.

Dette dokumentet beskriver kommende sprinter for VTOL-modus og en mer fargerik Enhanced-grafikk. Det gir ikke klarsignal til å implementere dem. Arbeidet starter først etter en uttrykkelig beskjed.

## Ufravikelige rammer

- Classic Mode skal ikke få endret fysikk, drivstofforbruk, grafikk eller gameplay.
- Stabil 50 Hz på en ekte stock A500 er viktigere enn flere grafiske detaljer.
- Det skal ikke innføres et femte vist bitplan, en ekstra world-buffer eller en ny fullskjermskompositor.
- Eksisterende spritefordeling, Copper-oppsett og Paula-kanaler skal ikke endres uten en egen måling og beslutning.
- Statiske bakkeobjekter skal fortsatt bygges av tiles. De skal ikke gjøres om til BOB-er eller sprites.
- Enhanced-grafikk skal være ferdig konvertert ved bygging. Ingen kostbar piksel- eller palettkonvertering skal skje per frame.
- Kollisjonsbokser, poeng, skade, destruksjon, røyk, kratere og prosedyregenerering skal være de samme som før, med mindre en senere sprint uttrykkelig sier noe annet.
- Amiga-repoet skal bare inneholde Amiga-kode, Amiga-klare assets og relevante konverteringsverktøy. CPC-koden kan brukes som skrivebeskyttet referanse, men skal ikke forbedres eller tas inn som endret kildekode.

## Minnebudsjett og målekrav

Den eksisterende `game_tiles.bpl` er 4 080 bytes: 102 tiles × 8 rader × 5 lagrede plan. Spillet viser fire world-bitplan; det femte lagrede planet hentes ikke av playfieldet. Tile-data ligger normalt i programmets read-only/FAST-segment, ikke i den løpende Chip RAM-world-bufferen.

Følgende porter gjelder for alle sprintene:

- Behold minst **64 KB ledig Chip RAM** etter runtime-allokering i referansebygget.
- En sprint får ikke redusere ledig Chip RAM med mer enn **1 KB** fra godkjent baseline uten særskilt gjennomgang.
- Første grafikkprototype skal bruke maksimalt omtrent **2 KB** nye Enhanced-assets.
- Samlet målbudsjett for nye Enhanced-only grafikkdata er **6–8 KB**, plassert i FAST/expansion/programsegment når verktøykjeden tillater det.
- Hver relevant sprint skal rapportere endring i executable-størrelse, FAST/expansion-bruk, ledig Chip RAM og framerate/hitches.
- VTOL skal ikke kreve et nytt lydsample, en ny lydkanal eller en ny stor buffer.

## Avklarte designvalg

### VTOL

- VTOL skal være en eksplisitt Enhanced-only tilstand, ikke et resultat som slås av og på direkte for hvert enkelt frame.
- Foreslått inngang: fart 0–1 i omtrent 10 frames.
- Foreslått utgang: fart 3 eller høyere i omtrent 6–8 frames.
- Landingssekvensen skal kunne tvinge VTOL umiddelbart.
- Første versjon beholder dagens lille fremdrift på omtrent 1 piksel per frame ved fart 0 utenfor landing. Ekte stillestående hover vurderes først senere.
- Drivstofforbruk i luften skal være omtrent 3× normalt i VTOL. Parkert, landet, pauset, styrtet eller ejectet fly skal ikke bruke VTOL-drivstoff.
- Eksisterende syntetiserte Paula-motorlyd skal brukes. Overgangen skal skje med periode-/volum-crossfade over omtrent 10–15 frames.

### Wingman og Player 2

- CPU-Wingman og Player 2 skal visuelt følge den samme globale VTOL-/landingsfasen når det er relevant.
- Eksisterende grafikk med landingshjul skal gjenbrukes der den passer.
- Felles drivstoff skal belastes én gang. Wingman skal ikke doble VTOL-forbruket.
- Det skal ikke innføres separat Wingman-drivstoff, egen motorlyd eller ny Paula-kanal.
- Uavhengig Player 2-gass og egen VTOL-tilstand er utenfor denne planen. Det ville krevd egne kontroller, HUD-regler og drivstoffmodell.

### Enhanced-grafikk

- Enhanced-utseendet skal bruke selektive tile-overstyringer, ikke en automatisk full kopi av hele Classic-atlaset.
- Første mål er tank, bil, radar, flak og én representativ bygningsgruppe.
- Variantvalg skal være deterministisk fra absolutt world column og world seed, aldri fra ringbufferposisjon.
- Det skal først gjøres en scanline-bevisst audit av hvilke penner som faktisk er ledige. Standard world-tiles bruker i dag hovedsakelig penn 0, 5, 10 og 15, men HUD, Copper, powerups og promoterte carrier-/gunship-lag kan eie andre penner.
- Kontinuerlig animasjon av bakkeobjekter utsettes. En liten flak-muzzle-flash bare ved avfyring kan vurderes etter ytelsestest.

## Sprint E1 – Baseline, penn- og minneaudit

**Status: delvis gjennomført**

### Omfang

- [ ] Lag et reproducerbart baseline-bygg for Classic og Enhanced med fast world seed.
- [ ] Registrer executable-størrelse, relevante linker-seksjoner, ledig Chip RAM, gjennomsnittlig/minimum FPS og hitches.
- [x] Dokumenter første sikre world-palettutvalg for tankprototypen: penn 0 som transparens og penn 2, 3, 4, 5, 6, 8, 10 og 13 som eksisterende OCS-farger.
- [ ] Kontroller tile-atlas, sprite-/BOB-paletter og palettbytter mellom brett.
- [ ] Lås et lite, trygt sett med penner som Enhanced-tiles kan bruke.
- [ ] Lag faste skjermbilder og telemetry-punkter for carrier, fjell, by, flak og landing.

### Ferdigkriterier

- En tabell viser minne- og ytelsesbaseline på WinUAE og ekte A500.
- Hver aktuell fargepenn har dokumentert eier og gyldig skjermområde.
- Ingen synlig funksjon eller gameplay er endret.
- Classic-kontrakt og ordinært Amiga-bygg består.

## Sprint E2 – VTOL-tilstand, drivstoff og Wingman-integrasjon

**Status: prototype pågår – tank implementert, øvrige objekter ikke startet**

### Omfang

- [ ] Innfør en eksplisitt Enhanced-only flight/VTOL state med hysterese og frame-tellere.
- [ ] La landing tvinge VTOL uten forsinkelse.
- [ ] Behold første versjons eksisterende 1-piksel-fremdrift ved fart 0 utenfor landing.
- [ ] Bruk omtrent 3× drivstoff i VTOL, men bare mens flyet er i luften.
- [ ] Sørg for korrekte reset-regler ved pause, crash, eject, respawn, landing og brettbytte.
- [ ] La CPU-Wingman og Player 2 følge relevant visuell VTOL-/landingsfase.
- [ ] Belast felles drivstoff bare én gang, uavhengig av Wingman.
- [ ] Eksponer VTOL-state og drivstoffmodus i eksisterende debug/telemetry.

### Ferdigkriterier

- Tilstanden flapper ikke rundt terskelverdiene.
- Parkert, landet og pauset spill bruker ikke VTOL-drivstoff.
- Landing, eject, respawn og brettbytte etterlater ingen gammel VTOL-state.
- Classic har identisk fart og drivstoffmodell som baseline.
- Stabil 50 Hz og ubetydelig minneøkning; ingen ny Chip-allokering.

## Sprint E3 – VTOL-grafikk og motorlyd

**Status: pågår – første bakkeobjektpakke er implementert og automatisk målt**

### Omfang

- [ ] Gjenbruk eksisterende Harrier-grafikk med landingshjul for Player 1.
- [ ] Gjenbruk egnet landet/Wingman-grafikk for CPU-Wingman og Player 2.
- [ ] Bytt grafisk tilstand bare ved state-overgang, ikke ved å rekonstruere den hvert frame.
- [ ] Tilpass eksisterende syntetiserte Paula-motorlyd med periode og volum.
- [ ] Crossfade vanlig motor og VTOL over omtrent 10–15 frames.
- [ ] Test takeoff, landing, pause, eject, crash, respawn og Player 2.

### Ferdigkriterier

- Landingshjul og motorlyd følger samme VTOL-state uten visuell hopping.
- Ingen klikk, popp, repetert sample eller ny lydkanal.
- Ingen spritepalett-korrupsjon etter demo, brettbytte eller retur til meny.
- Stabil 50 Hz på A500.

## Sprint E4 – Enhanced-grafikkprototype og asset-pipeline

**Status: ikke startet**

### Omfang

- [x] Lag en liten Enhanced-presentasjonsrute som lar Classic-ID-ene eie gameplay mens grafikken overstyres.
- [x] Legg til deterministisk generator for en Amiga-klar, maskert 16×16 tank bygget av fire 8×8-celler.
- [x] Lag statiske Enhanced-varianter for bil/launcher, radar og bakkekanon/flak. Bygningsgruppen gjenstår.
- [x] Bruk bare pennene som ble godkjent i E1.
- [ ] Velg kosmetisk variant deterministisk fra world seed og absolutt world column.
- [x] Behold samme 8×8-kompositor, maskering, kollisjon, destruksjon og world-state.
- [ ] Test hele byen og flakpartier ved maksimal fart på ekte A500.

### Første tankprototype – måling

- Rå Enhanced-grafikk: 160 bytes (`tank_16x16_masked.bpl`).
- Radar, launcher/bil og bakkekanon/flak: 240 bytes (`ground_targets_8x16_masked.bpl`).
- Ingen ny Chip RAM-allokering, world-buffer, spritekanal eller vist bitplan.
- Vanlig executable: 370 884 bytes før prototypen, 372 988 bytes etter rent bygg (+2 104 bytes inkludert kode og asset).
- Første komplette bakkeobjektpakke: 373 332 bytes (+344 bytes fra tankprototypen; +2 448 bytes fra opprinnelig baseline).
- Classic-kontrakten består: drivstoff, scrolling, motor, bombe, Maverick, kollisjon og carrier-tårn er uendret.
- Automatisk cycle-exact Enhanced-test, skill 1, fart 15 og seed 12040, fullførte ruten til slutt-carrieren. Etter oppstart målte den 50/50/50 FPS uten hitches og registrerte 100 prosedyregenererte mål.

### Ferdigkriterier

- Prototypen bruker maksimalt omtrent 2 KB nye read-only assets.
- Ingen ekstra world-buffer eller permanent Chip-kopi er opprettet.
- Classic viser nøyaktig eksisterende grafikk.
- Enhanced er tydelig mer fargerik uten flimmer, halve objekter eller palettlekkasje.
- Byområdet holder godkjent 50 Hz-/hitch-baseline.

## Sprint E5 – Utvidet bakke- og bygrafikk

**Status: ikke startet**

### Omfang

- [ ] Utvid Enhanced-varianter til flere bygninger og bakkeobjekter først etter at E4 er godkjent.
- [ ] Hold samlet Enhanced-only grafikk innenfor 6–8 KB-målet.
- [ ] Behold grafikken statisk som hovedregel.
- [ ] Vurder en enkel flak-muzzle-flash bare ved avfyring dersom målinger viser at den er trygg.
- [ ] Test destruksjon, røyk, kratere, water splash og tile-overganger for alle nye varianter.
- [ ] Test brettfarger, landing, demo, hovedmeny og Field Guide for palettregresjoner.

### Ferdigkriterier

- Ingen halv tank, halv bygning, feil destruksjon eller objekt som flytter kollisjonsflate.
- Ingen kontinuerlig animasjon eller redraw-kostnad er innført uten målegrunnlag.
- Full fart gjennom byen er stabil på ekte A500.
- Minneportene er bestått.

## Sprint E6 – Integrasjon og ekte maskinvare-validering

**Status: ikke startet**

### Omfang

- [ ] Kjør Classic og Enhanced på skill 1 og 5.
- [ ] Test solo, CPU-Wingman og Player 2.
- [ ] Test VTOL, takeoff, landing, drivstoff, radar, våpen, powerups, eject, crash og brettbytte.
- [ ] Kjør minst ti minutter på ekte A500 med maksimal fart gjennom by og tunge flakpartier.
- [ ] Kontroller at gjentatte retries, demoer og brettbytter ikke lekker Chip/FAST-minne eller korrumperer palett/sprites.
- [ ] Sammenlign sluttmålinger med E1-baseline og skriv en go/no-go-vurdering.
- [ ] Oppdater README og relevant teknisk dokumentasjon etter godkjent resultat.

### Ferdigkriterier

- Classic består kontraktstest uten Enhanced-avvik.
- Enhanced holder stabil 50 Hz uten nye periodiske hitches.
- Minst 64 KB Chip RAM er ledig etter runtime-allokering i referansebygget.
- Ingen uavklart reduksjon på mer enn 1 KB Chip RAM fra baseline.
- Alle nye funksjoner kan slås av ved å velge Classic Mode.

## Sprint E7 – Palettlåst Amiga tile-editor

**Status: implementert lokalt, avventer visuell brukerprøve før Git-push**

- [x] Lag et lite PC-verktøy som åpner de indekserte PNG-masterne med kraftig pikselforstørrelse.
- [x] Lås tegning til prosjektets eksisterende OCS-palett og vis både palettindeks og 12-bit RGB-verdi.
- [x] Støtt blyant, viskelær/transparens, fyll, speil, rutenett og side-ved-side-forhåndsvisning i 1:1-størrelse.
- [x] Støtt minst 8×8, 8×16 og 16×16 uten å introdusere et nytt runtime-format.
- [x] Eksporter de samme indekserte PNG-filene som generatorene bruker, og kjør eksisterende validator/bitplanpakker etter lagring.
- [x] Behold generatorne som reproduserbar startverdi; editoren skal redigere mastergrafikken, ikke rå `.bpl`-bytes.

## Bevisst utsatt eller ikke tillatt i denne serien

- Femte vist bitplan eller ny fullskjerms world-buffer.
- Automatisk duplisering av hele Classic-grafikkbanken.
- Runtime-remapping av tile-piksler eller globale palettbytter per objekt.
- Statiske tanks, bygg, radar eller flak som BOB-er/sprites.
- Nye spritekanaler for bakkeobjekter.
- Variantvalg basert på ringbufferindeks.
- Kontinuerlig flak-, radar- eller bygningsanimasjon før reelle A500-målinger viser rom for det.
- Egen motorlyd, Paula-kanal eller drivstoffbeholdning for Wingman.
- Uavhengig Player 2-gass/VTOL i denne roadmapen.
- Endringer i CPC-kode eller CPC-only assets.

## Planlagt rekkefølge

`E1 → E2 → E3 → E4 → E5 → E6`

Hver sprint skal avsluttes med et kjørbart spill, rapporterte måltall og en eksplisitt beslutning før neste sprint startes.
