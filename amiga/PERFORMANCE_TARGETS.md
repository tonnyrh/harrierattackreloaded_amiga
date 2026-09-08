# Realistiske ytelsesmål og historiske referanser

Vurdert 2026-09-08. Dette er et grunnlag for videre arbeid, ikke en erklæring
om at spillet er ferdig optimalisert eller at brukeren har godkjent lavere mål.

## Konklusjon

Jevn 50 Hz scrolling er et relevant og historisk oppnåelig kvalitetsmål for
et Amiga-actionspill. Det beviser ikke at all vår nåværende spilllogikk,
grafikk og verstefallsbelastning kan kjøres hvert 20. ms på A500 uten større
endringer. A1200-målet støttes allerede av våre målte ruter. A500-målet er
plausibelt, men fortsatt uavklart under tung kamp.

Det finnes ikke én universell grense for «akseptabel Amiga-FPS». Historiske
spill brukte forskjellige frekvenser for scrolling, styring og objekttegning.
Følgende er utviklernes egne beskrivelser, ikke våre emulatorbenchmarks:

| Spill | Dokumentert løsning | Betydning for prosjektet |
| --- | --- | --- |
| Hybris | Martin Pedersen sier at spillet skal holde bildefrekvensen; de fleste objektene er hardware-sprites. | 50 Hz er mulig når grafikken organiseres rundt maskinvaren. |
| Battle Squadron | Større, mer fargerike fiender tegnes som BOB-er annenhver ramme; sprite-objektene følger en annen takt. | «Spillet kjører i 50 Hz» betyr ikke nødvendigvis at alle objekter tegnes 50 ganger/s. |
| SWIV | Ronald Pieket Weeserik bekrefter 25 FPS som et bevisst valg for mer action. | 25 FPS har et historisk presedensgrunnlag, men oppfyller ikke vårt mål om 50 Hz scrolling. |
| James Pond 2: RoboCod | Chris Sorrell beskriver 50 Hz spilllogikk med separat rendering som ikke alltid rekker hver ramme. | Spillhastighet og tegningsfrekvens må vurderes separat. |

Kilder: [Martin Pedersen om Hybris og Battle Squadron](https://codetapper.com/amiga/interviews/martin-pedersen/),
[Ronald Pieket Weeserik om SWIV](https://codetapper.com/amiga/interviews/ronald-pieket-weeserik/),
[Chris Sorrell om RoboCod](https://codetapper.com/amiga/interviews/chris-sorrell/).
Intervjuene er retrospektive. Spillenes andre skjermoppsett, objekter og
effekter gjør dem til arkitekturreferanser, ikke direkte ytelsesfasiter.
Vi utleder ikke Battle Squadrons nøyaktige scrolltakt fra spritefrekvensen.

## Parametere vi skal vurdere

Dette er prosjektets anbefalte vurderingskriterier, ikke en historisk standard:

- **Scrolling og styring:** behold 50 Hz som mål. Én PAL-periode er omtrent
  20 ms; to er omtrent 40 ms. Gjentatte 20/40 ms-intervaller kan gi rykk selv
  med høyt gjennomsnitt. Jevn 25 Hz er en annen opplevelse, ikke automatisk
  bedre enn vår nåværende rendering og ikke en stilltiende reservebeslutning.
- **Normal flyging og byoverganger:** mål om én oppdatering per periode,
  uten registrerte tapte perioder på de definerte testrutene.
- **Tung kamp, skade og krasj:** samme 50 Hz ambisjon, men rapporter oppnådde
  resultater separat. Mål lengste gap, andel tapte perioder og sammenhengende
  klynger av forsinkelser. Et godt FPS-gjennomsnitt er ikke et beståttstempel.
- **Sekundære animasjoner:** 25 Hz kan vurderes der den visuelle forskjellen
  er akseptabel. Det må ikke endre kollisjoner, våpentakt, spillhastighet eller
  verdensposisjoner. En slik arkitekturendring krever egne paritetstester.
- **Oppstart:** skill lasting før aktivt spill fra de første synlige
  scrollrammene. Ikke skjul det rapporterte oppstartsproblemet ved bare å
  filtrere vekk de første sekundene i statistikken.
- **Visuell kontroll:** ingen tearing, rester etter BOB-er, feil overlapp
  eller palettskift. Bruk kontinuerlig opptak/observasjon, ikke bare bilder.
  Vertsskjermen må presentere PAL-takten korrekt; emulatorens interne 50 Hz
  alene beviser ikke jevn bevegelse på skjermen. Se også emulatorprosjektets
  [FS-UAE README om 50 Hz og synkronisering](https://sources.debian.org/src/fs-uae/3.0.5%2Bdfsg-1/README).

## Sammenholdt med våre målinger

Sammenligningsintervallet er skill 5, hastighet 15, scroll 1000–15000,
Wingman 2, fiender 3x, våpenstress og Enhanced-grafikk.

| Variant | Gjennomsnitt av loggvinduer | Svakeste vindu | Største feltgap |
| --- | ---: | ---: | ---: |
| A500 normal, destroyed_target_rows_skill5 | 45,59 | 38 | 2 |
| A500 eksperiment, hardware_player_rocket_skill5 | 46,90 | 42 | 2 |
| A1200 normal, A1200_destroyed_target_rows_skill5 | 50,00 | 50 | 1 |

CSV-ene ligger i `.tmp/amiga-parity-results/`, med navnene over som suffiks.
A500 bruker 51 respektive 50 loggvinduer, A1200 47. Tallene er avrundede
spilloppdateringer per sekund i tidsvinduer, ikke individuelle rammetider
eller direkte mål på inputforsinkelse. Den eksperimentelle rakettvarianten
er avslått som standard og mangler full visuell verifikasjon.

A500-profilen er **512 KB Chip + 512 KB Slow, totalt 1 MB**, PAL og
syklusnøyaktig 68000, uten Fast RAM. Dette er ikke en uoppgradert 512 KB A500.
A1200-profilen bruker 68020 med 24-bits adressering, AGA og 2 MB Chip uten
Fast RAM. Et kompatibilitetsløfte for en annen minnekonfigurasjon krever en
egen test. Emulerte resultater er ikke dokumentert testing på fysisk maskin.

Prioriteringen er derfor færre bakgrunnsgjenopprettinger, hardware-sprites
og bedre fordeling av arbeidet før mer assembly. Historiske løsninger og
vår rakettest støtter den retningen. De garanterer ikke stabil 50 Hz i alle
scener. Enkeltstående mikrooptimaliseringer uten målbar forbedring avsluttes;
eventuelle kompromisser beskrives eksplisitt før målet endres.
