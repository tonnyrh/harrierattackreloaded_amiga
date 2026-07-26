## 1. Fix Repeating Sound issue

"Jeg merker også at lydene under spilling som å droppe bombe eller skyte missil kan repeteres flere ganger"

Mest sannsynlig: Fire 1-signalet flimrer mellom av/på, og Pressed() tolker hver ny overgang som et nytt trykk. Det er også en separat svakhet i SFX-avspilleren: Paula får hele sample+stillhetsbufferen som loop, ikke en permanent stille repeat-peker.

Slik finner du hvilken feil det er

Se på bombe-/rakettelleren:

Synker telleren flere ganger: inputknappen retrigges.
Synker telleren bare én gang, men lyden gjentas: Paula/SFX-bufferen looper.
For rakettlyden: SFX_FIRE brukes også når fienden skyter, ved linje ca. 5551.
1. Legg inn debounce på Fire 1

Dagens Pressed() reagerer etter bare én lav/høy måling:

static UBYTE Pressed(UBYTE now, UBYTE previous) {
    return now && !previous;
}

POT-linjen og mekaniske knapper kan bounce. Krev at Fire 1 er stabil i to frames:

static UBYTE joyFire1Stable;
static UBYTE joyFire1LastRaw;
static UBYTE joyFire1StableFrames;

static UBYTE ReadJoyFire1Debounced(void) {
    UBYTE raw = !(custom->potinp & (1 << 14));

    if (raw == joyFire1LastRaw) {
        if (joyFire1StableFrames < 2)
            joyFire1StableFrames++;
    } else {
        joyFire1LastRaw = raw;
        joyFire1StableFrames = 0;
    }

    if (joyFire1StableFrames >= 2)
        joyFire1Stable = raw;

    return joyFire1Stable;
}

Bruk:

input->bomb = keyBomb || MouseRight() || ReadJoyFire1Debounced();

Nullstill variablene i InitInput() og behold:

custom->potgo = 0xff00;
2. SFX-systemet er fortsatt teknisk loopende

startPendingSfxChannel() setter AUDxLEN til hele engangsbufferen:

custom->aud[channel].ac_ptr = (volatile UWORD*)data;
custom->aud[channel].ac_len = byteLength >> 1;
custom->dmacon = DMAF_SETCLR | sfxDmaBit(channel);

Paula repeterer denne bufferen hvis frame-telleren ikke stopper kanalen før sample+stillhet er ferdig. Den korrekte langsiktige løsningen er:

Start det virkelige samplet.
Etter DMA-start, sett repeat-peker til et stille ord i chip-RAM.
La Paula repetere bare stillhet når samplet er ferdig.

Eksempeldata:

EMBED_CHIP sfxSilenceLoop[] = { 0, 0 };

Deretter må AUDxLC/AUDxLEN skiftes til:

custom->aud[channel].ac_ptr = (volatile UWORD*)sfxSilenceLoop;
custom->aud[channel].ac_len = 1;

etter at Paula har rukket å hente den opprinnelige samplepekeren. Ikke skriv repeat-pekeren umiddelbart uten den nødvendige DMA-forsinkelsen.

Jeg ville først lagt inn Fire 1-debounce. Dersom ammunisjonstelleren fortsatt bare synker én gang mens lyden gjentas, er neste grep å erstatte 4096-byte stillhetspadding med korrekt stille repeat-peker.


## 2. Fix flak gun mechanism

"Så lurer jeg på om du kan se hvordan flak gun mekanismene fungerer i Amstrad koden"


Ja. Amstrad-koden viser at «flak gun» egentlig ikke er en vanlig kanon som skyter et prosjektil fra bakken.

Slik fungerer launchflakattack:

Rutinen kjøres kontinuerlig fra checkenemyattacks.
Flak tillates bare over land og i by-/bygningsseksjoner.
En tilfeldig høyde mellom omtrent rad 0–15 velges.
Koden undersøker en posisjon ved høyre skjermkant.
Dersom posisjonen fortsatt er sky, plasseres et flak-objekt direkte der:
objekt-ID: 10
grafikk: sprite 57 eller 58
Flaklyden spilles idet objektet opprettes.
Flaket har ingen egen prosjektilebevegelse. Det følger verdenens vanlige scrolling mot venstre.

Treffmekanikken er også litt spesiell:

Et flaktreff dreper ikke spilleren umiddelbart.
flakdamagecount økes med én.
Flyets health-visning oppdateres.
Spilleren dør når telleren når totalflakdamagecount.
Tålegrensen beregnes som omtrent 25 - 2 × difficulty, altså cirka 23 treff på laveste og 15 på høyeste vanskelighetsgrad.
Health/påfyll nullstiller flakskaden.
Wingman og fiendtlige missiler behandler flak som ufarlig/passérbart.

I byområder settes flakterskelen til 10, så der kommer flak merkbart oftere. Over vanlig land brukes noen tellere (l884b, l884c, l885d og l8864) for å variere tettheten.

Dette betyr at Amiga-implementasjonen ikke bør binde flak direkte til en bestemt bakke-kanon eller et ferdiggenerert terrengpunkt. En mer tro CPC-modell vil være:

Forsøk å generere flak når en ny kolonne kommer inn på høyre side.
Velg tilfeldig høyde i luftområdet.
Generer bare hvis tile-posisjonen er sky.
Bruk høyere sannsynlighet i byseksjoner.
La scrollsystemet flytte flaket.
Spill lyd én gang ved opprettelse, ikke hvert bilde flaket eksisterer.

Det siste punktet er viktig: dersom Amiga-koden spiller flaklyden mens objekt-ID 10 blir behandlet eller tegnet, vil lyden repeteres. Den bør bare trigges på overgangen «tom sky → nytt flak».

Kilden er her: HarrierAttackSourceNew2_alt_CRTC_CART16.asm.

Hva om spiller skyter på flak?
Jobbet i 40s

Kjernepoeng: I Amstrad-versjonen kan spilleren treffe flak, men flaket blir ikke ødelagt. Skuddet/bomben forsvinner uten eksplosjon eller poeng.

Årsaken i checkenemyhit er:

ld a,(de)           ; Flak har objekt-ID 10
dec a               ; A blir 9
cp 9
jr z,bombhitsealand

Deretter:

bombhitsealand:
  pop hl
  jr returnzero

Det betyr:

Flaket blir stående i objektkartet.
Spillervåpenet tegnes ikke videre og forsvinner.
Ingen eksplosjonslyd spilles.
Ingen røyk tegnes.
Ingen poeng gis.

Kommentaren PIER OBJECT ID ved cp 9 er misvisende fordi koden allerede har utført dec a. Sammenligningen treffer derfor opprinnelig objekt-ID 10, altså flak.

I den vedlagte Amiga-koden gjør du derimot dette for både rakett og bombe:

if (rocketCell.id == HAR_OBJ_FLAK) {
    clearFlakOrSmokeAtColumnRow(...);
    game->bonusScore += FLAK_SCORE_VALUE;
    ...
}

Amiga-versjonen lar altså spilleren skyte ned flak, gir poeng og spiller trefflyd. Det avviker fra Amstrad-mekanikken.

Min anbefaling for en tro port er å la våpenet stoppe mot flak, men ikke fjerne flaket eller gi poeng. Eventuelt kan dagens oppførsel beholdes som en bevisst forbedring, men da er det ikke original CPC-logikk. Se Amstrad-kilden.

## 3. Correct landscape generation

Hvordan Amstrad bygger landskapet

Amstrad-koden bygger i praksis verden fortløpende:

Del	Virkemåte
Nivåfase	gamelevelprogress velger sjø, kyst, land, by osv.
Ny kolonne	Landskap genereres når en ny kolonne kommer inn fra høyre
Landhøyde	Fire modi: flat, én rad ned, én rad opp eller plasser mål
Bakkeobjekter	Radar, launcher, gun og tank velges pseudo-tilfeldig
Objektkart	Hver posisjon inneholder sky, sjø, land, mål, flak osv.
Visning	Kolonnen tegnes i skjermkartet og flyttes videre av scrollingen

Høyden endres bare én rad om gangen. Det gir sammenhengende bakker i stedet for tilfeldig, hakkete terreng.

Flak inngår ikke egentlig i selve landskapsgenereringen. launchflakattack kjøres separat etterpå og forsøker å legge flak inn i en ledig skycelle nær høyre skjermkant.

Hvordan Amiga-versjonen gjør det nå

Amiga-porten har en god og mer maskintilpasset løsning:

LevelSegmentDef bestemmer nivåfase og terrengtype.
generateCpcLandHeightTable() forhåndsberegner landhøyde og bakkemål.
objectCellForWorldColumnTile() bygger hver celle logisk fra:
sky/sjø
terreng
ødelagte områder og røyk
faste nivåobjekter
prosedyregenererte mål og flak
Nye kolonner tegnes gradvis inn i en sirkulær bitplanbuffer.
Copper endrer bitplanpeker og BPLCON1 for pikseljevn scrolling.
Bare endrede kolonner tegnes på nytt ved treff og ødeleggelser.

Dette er en god Amiga-arkitektur. Den unngår full skjermtegning hvert bilde, og arbeidsmengden for en ny kolonne fordeles over flere bilder basert på scrollhastigheten.

Hva som bør beholdes

Behold følgende omtrent uendret:

Den deterministiske landhøydetabellen.
Segmentinndelingen mellom sjø, land og by.
Ringbufferen og gradvis kolonnerendering.
Copper-basert grov- og finscrolling.
dirtyRedrawWorldColumn() ved faktiske endringer.

Selve scrollsystemet er allerede betydelig bedre tilpasset Amiga enn en direkte oversettelse av Amstrad-koden ville vært.

Hva som bør endres nå
1. Fjern flak fra de forhåndsgenererte landskapstabellene

Disse bør fases ut:

cpcLandFlakTable[]
cpcTownFlakTable[]
generateCpcTownFlakTable()
cpcLandProceduralFlak()
cpcTownProceduralFlak()

Dagens flak er bundet til bakkeobjekter og byblokker flere kolonner fremover. Det stemmer ikke med Amstrad, der flak oppstår som en separat angrepshendelse.

2. Generer flak ved høyre synlige skjermkant

Når scrollingen passerer til en ny tilekolonne:

rightColumn = (game->scrollX >> 3) + GAME_MAP_WIDTH;

Kjør ett flakforsøk for denne kolonnen:

static void trySpawnFlak(GameState* game)
{
    LONG column = (game->scrollX >> 3) + GAME_MAP_WIDTH;
    WORD row = randomValue & 15;

    if (!stageAllowsFlak(game))
        return;

    if (!flakFrequencyAllowsSpawn(game))
        return;

    if (worldCellIsSky(column, row))
        addRuntimeFlak(column, row, 57 + (randomValue & 1));
}

Flaket bør lagres i en liten runtime-liste med worldColumn, tileY og tile. Det trenger ingen egen bevegelse; vanlig scroll flytter det visuelt.

3. Skill røyk og flak

Nå brukes HAR_OBJ_FLAK også for vrakrøyk:

outCell->id = HAR_OBJ_FLAK;
outCell->tile = smokeTile;

Dette bør splittes:

#define HAR_OBJ_FLAK  10
#define HAR_OBJ_SMOKE 11

Ellers kan kollisjonskode, våpen og lyd feilaktig behandle røyk som flak.

4. Gjør flak uødeleggelig med spillervåpen

I originalen stopper våpenet når det treffer flak, men flaket blir stående. Fjern derfor dette for både rakett og bombe:

clearFlakOrSmokeAtColumnRow(...);
game->bonusScore += FLAK_SCORE_VALUE;
updateHudValues(game);

Ønsket behandling:

if (rocketCell.id == HAR_OBJ_FLAK) {
    game->rocketShot.active = 0;
    return changed;
}

Tilsvarende for bomben. Ingen poeng, eksplosjon, røyk eller trefflyd.

5. Spill flaklyd kun ved vellykket opprettelse

Flytt lyden inn i selve spawn-rutinen:

if (addRuntimeFlak(column, row, tile))
    playSfx(SFX_FLAK_POP);

Da kan updateFlakPopSound() fjernes. Det blir både enklere og hindrer at samme flak utløser lyd flere ganger når kolonnen undersøkes eller tegnes på nytt.

Sekundær optimalisering

objectCellForWorldColumnTile() og drawDirectColumnRangeObjects() går gjennom hele harLevelObjects for hver celle eller kolonne. Ringbufferen begrenser kostnaden, men dette er fortsatt den tydeligste gjenværende CPU-optimaliseringen.

En senere forbedring kan være en indeks per verdenskolonne:

firstObjectForColumn[column]
objectCountForColumn[column]

Da undersøkes bare objektene som faktisk ligger i aktuell kolonne. Dette er imidlertid lavere prioritet enn å rette flakmodellen.


## 4. Korrigreing under generering av Landskap

"Hvordan genereres selve gresset i landskapet med tanke på å finne de korrekte tiles når landskapet svinger, og hvordan styres egentlig høyden av landskapet?. Hva er avvikende i amiga koden?"

 Kjernepoeng: CPC-koden velger først terrenghandling for den nye kolonnen og bruker denne handlingen direkte til å velge riktig gress-/skråningstile. Amiga-koden beregner derimot høydene på forhånd og forsøker etterpå å utlede tiletypen ved å sammenligne nabokolonner. Det gir flere avvik, særlig doble skråningstiles og for lave fjell på høyere vanskelighetsgrader.

Hvordan høyden styres på CPC

Variabelen l885d er terrenghøyden, målt som tile-rad:

14 er normalt, lavt og flatt terreng.
Mindre verdi betyr høyere landskap.
13 er én tile høyere enn 14.
Terrenget endres maksimalt én tile per ny kolonne.

Ved starten av landseksjonen:

ld a,&0e
ld (l885d),a       ; start på rad 14

For hver nye kolonne velges én av fire handlinger:

Modus	Handling	Høyde
0	Flatt gress	Uendret
1	Terrenget går ned	l885d + 1, maks 14
2	Terrenget går opp	l885d - 1, ned til vanskelighetsgrensen
3	Plasser bakkeobjekt	Normalt uendret

Modusen hentes slik:

ld a,(l8859)
rra
rra
and 3

Det er noe usikkerhet rundt l8859, siden den ikke ser ut til å bli oppdatert i denne kildefilen. Amiga-kodens antakelse om at den representerer en slags langsomt varierende R-registerverdi er derfor plausibel, men ikke bevist.

Vanskelighetsgrensen

CPC beregner høyeste tillatte terreng slik:

ld a,(leveldifficulty)
ld d,a
ld a,5
sub d
add 7
cp h

Det gir følgende øverste rad:

Vanskelighet	Minste rad	Maksimal høyde over grunnlinjen
1	11	3 tiles
2	10	4 tiles
3	9	5 tiles
4	8	6 tiles
5	7	7 tiles

Dermed kan høyere vanskelighetsgrad produsere betydelig høyere landskap.

Amiga-koden har derimot:

#define CPC_LAND_PROCEDURAL_BASELINE 14
#define CPC_LAND_PROCEDURAL_FLOOR 11

Det låser alle vanskelighetsgrader til maksimalt tre tiles høyde. Dette er et tydelig avvik.

Hvordan CPC velger gress-tile

CPC velger tile samtidig som høyden endres.

Flatt terreng
ld a,r
and 3
add #20

Det velger tilfeldig mellom tile:

32, 33, 34, 35

Dette er fire variasjoner av flatt gress.

Terreng som går opp
ld a,(l8859)
rra
and 3
add #18

Det velger en tile mellom:

24, 25, 26, 27

Deretter reduseres høyderaden med én:

dec h
ld (l885d),a

Dette er «hill up»-gruppen.

Terreng som går ned
ld a,r
rra
and 3
add #1c

Det velger en tile mellom:

28, 29, 30, 31

Deretter økes høyderaden med én:

inc a
ld (l885d),a

Dette er «hill down»-gruppen.

Fyll under overflaten

Etter at overflatetilen er tegnet, fyller CPC alle radene under med tile 1:

ld de,solidlandspriteblock
ld c,3
call drawspriteblock3

Tile 1 er altså det kompakte grønne jord-/landfyllet. Bare øverste tile er gresskant eller skråning.

Det viktigste avviket i Amiga-koden

Amiga gjør dette:

previousY = terrainYForWorldColumn(worldColumn - 1, ...);
currentY  = terrainYForWorldColumn(worldColumn, ...);
nextY     = terrainYForWorldColumn(worldColumn + 1, ...);

Deretter:

if (currentY < previousY || nextY < currentY)
    return 24 + hillPhase;

if (currentY > previousY || nextY > currentY)
    return 28 + hillPhase;

Problemet er at både kolonnen før og selve høydeendringen kan bli klassifisert som skråning.

Eksempel med høydene:

14, 14, 13, 13

Amiga tolker dette omtrent slik:

Kolonne	Forrige	Nå	Neste	Resultat
1	14	14	13	Hill up
2	14	13	13	Hill up

Én høydeendring kan dermed gi to skråningstiles.

CPC gjør ikke dette. Bare kolonnen hvor handlingen «gå opp» utføres får en tile fra 24–27.

Flere avvik
Område	CPC	Amiga
Skråningsvalg	Bestemmes av handlingen for aktuell kolonne	Utledes fra forrige og neste høyde
Antall skråningstiles	Én per høydeendring	Kan bli to
Tilevariant	Pseudo-tilfeldig R & 3	Avledes fra currentY & 3
Flat gressvariant	Tilfeldig 32–35	Fast mønster basert på verdenskolonnen
Maksimal høyde	Avhenger av difficulty	Alltid rad 11
Landskap mellom runder	Kan variere med kjøretiming/R	Samme forhåndsgenererte tabell
Under bakkeobjekt	CPC bruker tile 1 som overflate i denne banen	Amiga beholder vanlig gresskant

Amiga-kommentaren sier at høydebaserte skråningsfaser gir en mer sammenhengende rampe. Det kan være visuelt penere, men det er ikke det CPC-koden gjør. CPC velger tilfeldig variant innen riktig opp-/nedgruppe.

Anbefalt korrigering

Lagre tiletypen sammen med høyden under genereringen:

static UBYTE cpcLandHeightTable[CPC_LAND_PROCEDURAL_LENGTH];
static UBYTE cpcLandSurfaceTable[CPC_LAND_PROCEDURAL_LENGTH];

Genereringen bør gjøre dette direkte:

switch (mode) {
    case 0:
        surface = 32 + randomGrassVariant();
        break;

    case 1:
        if (height < CPC_LAND_PROCEDURAL_BASELINE) {
            height++;
            surface = 28 + randomHillVariant();
        } else {
            surface = 32 + randomGrassVariant();
        }
        break;

    case 2:
        if (height > difficultyFloor) {
            height--;
            surface = 24 + randomHillVariant();
        } else {
            surface = 32 + randomGrassVariant();
        }
        break;

    case 3:
        surface = 1;
        placeGroundTarget();
        break;
}

Vanskelighetsgrensen kan beregnes slik:

static UBYTE cpcLandMinimumRow(UBYTE difficulty)
{
    return (UBYTE)(12 - difficulty);
}

Deretter bør:

landSurfaceTileForColumn()

returnere den forhåndslagrede overflatetilen direkte for det prosedyregenererte landet, uten å undersøke nabohøydene.

Dette vil:

eliminere doble skråningstiles
bruke riktig opp-/nedgruppe
gi korrekt høyde per vanskelighetsgrad
bevare tilfeldig gressvariasjon
redusere arbeid under rendering, siden nabokolonner ikke lenger må beregnes

Selve ringbufferen og Copper-scrollingen trenger fortsatt ingen endring. Feilen ligger i terrengdataene som mates inn i renderereren, ikke i visningen. Amstrad-kilden.


##5 Correcting the town layout and behaviour

Slik bygges byen på CPC

Når terrenget har sunket tilbake til rad 14, går spillet over til Port Stanley:

Terrenget gjøres flatt.
Bybelysningen aktiveres gjennom startpalettefade.
En tilfeldig bygning velges blant åtte blokker.
Bygningsblokken tegnes kolonne for kolonne.
Når blokken er ferdig, velges straks en ny bygning.
Dette fortsetter helt til bytelleren utløper.

Utvalget skjer slik:

ld a,r
rra
rra
and #07

Resultatet er én av åtte blokker:

townspritestable:
  defw blk0
  defw blk1
  defw blk2
  defw blk3
  defw blk4
  defw blk5
  defw blk6
  defw blk7

Bygningene har forskjellig bredde og høyde. Når én blokk når 255-markøren, velges neste. Det blir derfor en relativt tett og variert by, ikke bare noen få isolerte hus.

Avviket i Amiga-porten

Amiga-koden henter bygningene fra:

harLevelObjects[]

og krever:

object->id == HAR_OBJ_TOWN_BLOCK
object->flags & HAR_OBJECT_FLAG_CPC_TOWN_BLOCK

Bygningene er dermed håndplassert i level_route.h. Det gir:

færre bygninger
store åpne områder
mindre variasjon
flak bare i nærheten av enkelte forhåndsplasserte blokker

Renderereren støtter allerede alle de promoterte CPC-bygningene. Problemet er derfor ikke selve tegningen, men at det finnes for få bygningsinstanser.

Rød blinkende skjerm

CPC gjør en palettovergang ved inngangen til byen:

ld hl,&0FF0
ld (duskpal+6),hl
ld (nightpal+6),hl
call startpalettefade

Etter byen gjenopprettes de gamle palettverdiene.

Dette ser ut til å være en endring av et begrenset antall by-/landfarger, ikke et fullskjerms rødt blink. Amiga-koden har ingen tilsvarende byfase eller palettfade; den bruker i hovedsak den samme gamePalette gjennom hele spillet.

Dersom hele Amiga-skjermen blinker rødt, bør det derfor behandles som en feil. Mest sannsynlige områder å kontrollere er:

Copper-listens fargeregistre.
Minneoverskriving under rendering av brede byblokker.
Byblokker som tegnes over ringbufferens wrap-grense.
Feil tiledata eller pennmapping i de promoterte CPC-byressursene.

En rask isolasjonstest er å deaktivere drawPromotedCpcTownBlockRangeAt(). Forsvinner det røde blinket, ligger feilen i blokkdata/range-rendering. Fortsetter det, ligger den sannsynligvis i Copper-, palett- eller bufferminnet.

Hvorfor det er lite flak

Amiga genererer byflak slik:

for each hand-placed town block:
    50 % chance:
        place flak four columns ahead

Få håndplasserte bygninger gir derfor automatisk få flak.

CPC gjør ikke dette. launchflakattack() kjøres kontinuerlig i byfasen og bruker:

ld a,10
ld (l884c),a

Flak er altså knyttet til aktiv byfase, ikke til antall bygninger. Det forsøkes opprettet i en tilfeldig ledig skycelle ved høyre skjermkant.

Anbefalt endring
Prioritet	Endring	Resultat
1	Generer en sammenhengende tabell med CPC-byblokker	Tett og variert by
2	Fjern flakgenerering fra bygningsplassering	Flakfrekvens uavhengig av antall hus
3	Generer flak ved høyre skjermkant under byfasen	Riktig CPC-mekanikk
4	Undersøk rødt blink som rendering-/minnefeil	Stabil visning
5	Implementer kontrollert bypalett og fade	Riktig overgang til natt/by

Bygninger egner seg godt for forhåndsgenerering fordi ringbufferen spør etter vilkårlige verdenskolonner:

typedef struct {
    UBYTE blockId;
    UBYTE localColumn;
} TownColumn;

En enklere løsning er to tabeller:

static UBYTE townBlockForColumn[CPC_TOWN_LENGTH];
static UBYTE townBlockPhaseForColumn[CPC_TOWN_LENGTH];

Genereringen velger blokk 0–7, fyller inn alle kolonnene i blokken og velger umiddelbart neste blokk. drawDirectColumnRangeObjects() kan da tegne riktig bygningsfase uten å søke gjennom alle håndplasserte objekter.

Flak bør derimot ikke ligge i denne tabellen. Det bør genereres separat når kolonnen blir synlig.

Konklusjon: Selve Amiga-rendereren har allerede kapasitet til å vise en tett CPC-by. Det som mangler er en kontinuerlig bygenerator, fasebasert flak og riktig bypalett. Den røde fullskjermsblinkingen bør feilsøkes før palettovergangen implementeres, fordi den ikke ser ut til å være en tilsiktet del av originalen.

##6. Correcting the enemy frigate and friendly carrier at the end.



1. Den fiendtlige fregatten

Amiga-porten definerer fiendeskipene som bare fire verdenskolonner:

static const EnemyShipGroupDef enemyShipGroups[] = {
    { 50, 53 },
    { 629, 632 }
};

Disse tegnes som vanlige 8×8-tiles fra harLevelObjects[]. Det finnes ingen egen funksjon som komponerer et komplett fiendeskip. Skipet er derfor avhengig av at fire håndplasserte kolonner i level_route.h inneholder nøyaktig riktige tiles og høyder.

Dette kan gi:

for kort eller sammentrykt skip
deler i feil rekkefølge
manglende baug eller hekk
tiles som ikke henger sammen vertikalt
skip som forsvinner kolonnevis når det treffes

Trefflogikken gjør også dette:

markShipColumnDestroyed(worldColumn);

Ett treff fjerner altså den konkrete kolonnen som ble truffet. GAME_ENEMY_SHIP_HP brukes ikke som samlet helse i damageEnemyShipAtColumnRow(). Resultatet kan raskt bli et skip med tilfeldige hull og en merkelig silhuett.

Anbefaling

Fiendefregatten bør være én skip-instans med:

typedef struct {
    UWORD startColumn;
    UBYTE hp;
    UBYTE destroyed;
} EnemyShipState;

Alle grafikkdelene tegnes så lenge skipet lever. Treff reduserer samlet hp; først ved null erstattes hele skipet med en kontrollert vrakvariant eller røyk.

2. Landingsfregatten etterpå

Sluttfartøyet er egentlig spillerens landings-/servicefregatt:

HAR_OBJ_OWN_FRIGATE
HAR_OBJECT_FLAG_NATIVE_CARRIER

Den tegnes gjennom:

drawPromotedCpcCarrierRangeAt()

Samme grafiske komposisjon brukes til startskipet og sluttskipet:

back
body
body
top
top2
front

CPC-koden kommenterer uttrykkelig at sluttfregatten er reversert:

; FRIGATE REVERSED, SO IT CAN COME IN SCREEN FROM OPPOSITE SIDE
endfrigatesprite:

Amiga-versjonen ser ikke ut til å ha en egen reversert sluttvariant. Den gjenbruker samme drawPromotedCpcCarrier... som ved starten. Dermed kan baugen, overbygningen og landingsdekket vende feil vei.

Det visuelle skipet er omtrent 102–104 piksler bredt, mens den logiske bredden er:

#define WORLD_RENDER_CARRIER_WIDTH_TILES 12

Altså 96 piksler. Kollisjonsdekket bruker samtidig:

#define CARRIER_DECK_PIXEL_WIDTH 104

Det er derfor også et avvik mellom:

visuell bredde
renderbredde
objektkart
landingskollisjon
Mest sannsynlige rettelser
Prioritet	Rettelse
1	Avklar om det rare skipet er fiendeskipet ved 629–632 eller landingsskipet ved 640
2	Lag samlet helse for fiendeskipet i stedet for å slette enkeltkolonner
3	Kontroller rekkefølgen på fiendeskipets tiles i level_route.h
4	Lag en egen horisontalt reversert sluttfregatt
5	Samordne grafisk bredde, objektkart og landingsdekk til 104 piksler

Jeg mistenker sterkest at du ser en kombinasjon av et svært kort firekolonners fiendeskip og deretter en landingsfregatt som vender samme vei som startskipet. Et skjermbilde av fartøyet vil avgjøre umiddelbart hvilken av de to feilbanene det er.



## 7. Smoke generation.

"How is items, boats, guns, tanks, radars and buildings destroyed by missiles and how/when is the smoke generated?"



ore finding: On the CPC, missiles and bombs use the same tile-based collision routine. Most destructible objects are destroyed by replacing the single tile that was hit with persistent smoke. Smoke is generated immediately after the explosion and remains until it scrolls off-screen—there is no smoke timer or animation.

CPC destruction sequence

checkenemyhit reads the object-map ID at the weapon position:

call getskytilemapid
ld a,(de)

If it is destructible, the common path is:

call explosionnoise
call drawsmokesprite

This performs four actions:

Stops the missile or bomb.
Plays the loud explosion sound.
Adds score.
Replaces the impacted object tile with smoke.

Missiles and bombs generally cause identical destruction. The distinction is mainly their movement and which position they reach.

Object behaviour

The comparisons occur after dec a, so several source comments are off by one.

Original object	Object ID	Result when hit
Radar, launcher, gun, tank	8	Hit tile replaced with smoke
Town building	9	Hit tile replaced with smoke
Enemy ship/boat	7	Hit tile replaced with smoke
Own frigate	4	Marks frigate as hit; tile becomes smoke
Land	3	Crater tile 97, no smoke
Sea	2	No effect
Flak	10	Weapon disappears; flak remains
Enemy missile	11	Missile destroyed and cell changed to sky
Enemy plane	13	Plane status changed to hit; no persistent smoke here
Wingman power-up	21	Power-up destroyed with explosion
Smoke	10	Weapon disappears; existing smoke remains
Radars, guns, tanks and launchers

All these use object ID 8. CPC does not maintain separate hit points for them.

A single successful hit:

ld a,10
call explosionnoise
call drawsmokesprite

They award 10 internal score units, displayed as 100 points.

The original target tile is overwritten by smoke, so the object ceases to exist in the object map.

Buildings

Town building tiles use the default destructible path. During the town stages:

ld a,(gamelevelprogress)
cp 5
ld a,35

They award 35 internal units, displayed as 350 points.

Destruction is tile-based, not building-based. A missile destroys only the building tile it directly hits. A wide building can consequently remain standing with one smoky section missing.

Enemy boats and ships

Enemy ship tiles use object ID 7:

bombhitenemyship:
  ld a,50
  call explosionnoise
  call drawsmokesprite

The hit gives 500 displayed points and replaces only the struck tile with smoke.

There is no whole-ship health counter in this CPC path. A ship is effectively destroyed one tile at a time. This means the Amiga idea of deleting individual ship columns is closer to the CPC than a conventional ship-wide HP system—although the exact graphic tiles and smoke position still need to match.

How smoke is generated

drawsmokesprite creates up to two smoke tiles:

drawsmokesprite:
  push hl
  ld a,&34        ; tile 52, lower smoke
  ld c,10         ; smoke/flak object ID
  call drawspritecheckifsky
  pop hl

  dec h           ; one tile above
  call getskytilemapid
  ld a,(de)
  dec a
  ret nz          ; only continue if original ID was sky

  ld a,51         ; upper smoke
  ld c,10
  jp drawspritecheckifsky

The result is:

        tile 51 — upper smoke, only if the cell is empty sky
        tile 52 — lower smoke, replaces the destroyed object

Despite its name, drawspritecheckifsky does not require sky. It only refuses to draw over a cloud (ID 0). It therefore overwrites the object-map entry at the impact position with object ID 10.

Smoke lifetime

Smoke has:

no timer
no animation state
no fade-out
no independent movement

It is an ordinary tile in the scrolling object map. It remains until it scrolls beyond the left edge.

A peculiar original behaviour is that smoke and flak share object ID 10. Consequently:

player weapons cannot destroy smoke
enemy missiles ignore smoke
player collision can interpret smoke as flak and apply flak damage

That may be an original shortcut or bug, but it follows directly from the object IDs.

What the Amiga version currently does
Ground targets

For radar/gun/tank/launcher hits, Amiga currently does:

markTargetDestroyedAtColumn(rocketWorldColumn);
markLandCraterAtColumnRow(rocketWorldColumn, surfaceTileY);

This removes the target and creates a ground crater, but does not create the CPC two-tile smoke column.

That differs from CPC:

CPC: smoke tiles 52 and optionally 51
Amiga: crater in the terrain
CPC: target position remains represented by smoke
Amiga: target is removed from the procedural table
Buildings

The weapon collision filters do not include:

HAR_OBJ_TOWN_BLOCK

Therefore town buildings appear to be indestructible in the current Amiga code. Missiles pass through or fail to register the building overlay because the visible building is drawn separately from the normal object cell.

This is a major missing feature in the town section.

Enemy ships

Amiga calls:

damageEnemyShipAtColumnRow()
addCpcHitSmokeAtColumnRow()

This is conceptually close to CPC:

struck ship column is removed
smoke is placed at the hit
optional second smoke tile is added to an adjacent/upper sky position

However, the function currently removes an entire world column, while CPC replaces the specific tile coordinate. If the ship contains several vertical tiles in one column, Amiga may remove more than CPC would.

Own frigate

Amiga marks it as hit and adds smoke, broadly matching CPC. Collision and rendering must nevertheless use the same ship coordinates; otherwise smoke may appear detached from the visible frigate.

Recommended Amiga corrections
Add HAR_OBJ_TOWN_BLOCK to missile and bomb collision detection.
Track destroyed building cells by (worldColumn, tileY), not only whole objects.
Replace destroyed radar/gun/tank tiles with smoke 52 plus smoke 51 above when sky.
Do not automatically create a ground crater when a target tile is destroyed.
Change ship destruction from whole-column removal to exact (column,row) removal.
Keep persistent smoke until it scrolls away; the short-lived ImpactState explosion is a separate visual effect.

A useful common function would be:

static void destroyObjectWithCpcSmoke(
    GameState* game,
    UBYTE** worldBuffers,
    LONG column,
    WORD row)
{
    markObjectCellDestroyed(column, row);
    markSmokeAtColumnRow(column, row, 52);

    if (worldCellIsSky(column, row - 1))
        markSmokeAtColumnRow(column, row - 1, 51);

    dirtyRedrawWorldColumn(worldBuffers, column);
}

The transient Amiga impact animation should play immediately, while tiles 51/52 remain in the scrolling world afterward. CPC source.

