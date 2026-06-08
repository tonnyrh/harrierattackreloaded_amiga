; EACH OF THE NOTES ON THE PIANO MATCHED WITH THEIR 
; EQUIVALENT PERIOD

; OCTAVE -3
mC  equ 3822
mCs equ 3608
mD  equ 3405
mDs equ 3214
mE  equ 3034
mF  equ 2863
mFs equ 2703
mG  equ 2551
mGs equ 2408
mA  equ 2273
mAs equ 2145
mB  equ 2025
; OCTAVE -2
nC  equ 1911
nCs equ 1804
nD  equ 1703
nDs equ 1607
nE  equ 1517
nF  equ 1432
nFs equ 1351
nG  equ 1276
nGs equ 1204
nA  equ 1136
nAs equ 1073
nB  equ 1012
; OCTAVE -1
oC  equ 956
oCs equ 902
oD  equ 851
oDs equ 804
oE  equ 758
oF  equ 716
oFs equ 676
oG  equ 638
oGs equ 602
oA  equ 568
oAs equ 536
oB  equ 506
; MIDDLE C
pC  equ 478
pCs equ 451
pD  equ 426
pDs equ 402
pE  equ 379
pF  equ 358
pFs equ 338
pG  equ 319
pGs equ 301
pA  equ 284
pAs equ 268
pB  equ 253
; OCTAVE 1
qC  equ 239
qCs equ 225
qD  equ 213
qDs equ 201
qE  equ 190
qF  equ 179
qFs equ 169
qG  equ 159
qGs equ 150
qA  equ 142
qAs equ 134
qB  equ 127
; OCTAVE 2
rC  equ 119
rCs equ 113
rD  equ 106
rDs equ 100
rE  equ 95
rF  equ 89
rFs equ 84
rG  equ 80
rGs equ 75
rA  equ 71
rAs equ 67
rB  equ 63
; OCTAVE 3
sC  equ 60
sCs equ 56
sD  equ 53
sDs equ 50
sE  equ 47
sF  equ 45
sFs equ 42
sG  equ 40
sGs equ 38
sA  equ 36
sAs equ 34
sB  equ 32
; OCTAVE 4
tC  equ 30
tCs equ 28
tD  equ 27
tDs equ 25
tE  equ 24
tF  equ 22
tFs equ 21
tG  equ 20
tGs equ 19
tA  equ 18
tAs equ 17
tB  equ 16

;standalone equ 1
;harrierattack equ 1
ifdef standalone
org &1000
nolist
mc_wait_flyback     equ &bd19
jp dosound
endif

;ifdef harrierattack

; IDEA?
; NYBBLES USED FOR TONE
; 00-0
; NYBBLE USED FOR INSTRUMENT SELECTION
; --0-

; INPUT
; HL = LOCATION OF INTERRUPT SPEED ADDRESS
intloc: defw 0
initmusicf:
  ld (intloc),hl            ; STORE LOCATION OF MUSIC INTERRUPT SPEED SO WE CAN CHANGE IT
  call initsoundf           ; RESET CHANNEL BUFFERS AND CLEAR CHANNELS
  ld a,(defaultmusicspeed)  ; SET DEFAULT MUSIC SPEED
  ld (hl),a
  loopmusic:
  ; GO TO START OF MUSIC
  ld bc,musicscore
  ld (musicpos-2),bc
  ; SET TONES
  jp set_mixer_music
ifdef HARRIERATTACK
defaultmusicspeed: defb 13
endif
ifndef HARRIERATTACK
defaultmusicspeed: defb 17
endif
unsethalfbeat:
  ld a,(defaultmusicspeed)
  dosetbeat:
  ex de,hl
  ld hl,(intloc)
  ld (hl),a      ; SET NEW INTERRUPT SPEED
  ex de,hl
  inc hl
  jr getnextnote
  
ifdef HARRIERATTACK
sethalfbeat:
  ld a,6
  jr dosetbeat
endif
ifndef HARRIERATTACK
sethalfbeat:
  ld a,8
  jr dosetbeat
endif
  
; FUNCTION TO READ SCORE AND PLAY SOUNDS EVERY nth OF SECOND
playmusicf:
  ld hl,0:musicpos
  getnextnote:
  ; READ SOUND FOR CHANNEL A
  ld a,(hl)
  or a
  jr z,skipnosoundchannela
  dec a;cp 1                   ; LOOP
  jr z,loopmusic
  dec a;cp 2                   ; NO NOTES TO PLAY ON THIS BEAT
  jr z,skipnosoundchannelc2
  dec a;cp 3                   ; SET HALF BEAT - DOUBLES MUSIC INTERRUPT SPEED
  jr z,sethalfbeat
  dec a;cp 4
  jr z,unsethalfbeat

  ; LOAD TONE INTO SOUND EFFECT
  ld c,(hl)
  inc hl
  ld b,(hl)
  ld (soundeffect_a_tone),bc
  
  ; LOAD SOUND INTO INTERRUPT
  push hl
  ld hl,instrument_a
  ld de,soundbuffera
  call ldir14
  pop hl
  
  ; NEXT CHANNEL
  jr skipnosoundchannela2
  skipnosoundchannela:
  inc hl
  skipnosoundchannela2:
  inc hl

  ld a,(hl)
  or a
  jr z,skipnosoundchannelb
  
  ; LOAD TONE INTO SOUND EFFECT
  ld c,a
  inc hl
  ld b,(hl)
  ld (soundeffect_b_tone),bc
  
  ; LOAD SOUND INTO INTERRUPT
  push hl
  ld hl,instrument_b
  ld de,soundbufferb
  call ldir14
  pop hl
  
  ; NEXT CHANNEL
  jr skipnosoundchannelb2
  skipnosoundchannelb:
  inc hl
  skipnosoundchannelb2:
  inc hl
  
  ld a,(hl)
  or a
  jr z,skipnosoundchannelc
  
  ; LOAD TONE INTO SOUND EFFECT
  ld c,a
  inc hl
  ld b,(hl)
  ld (soundeffect_c_tone),bc
  
  ; LOAD SOUND INTO INTERRUPT
  push hl
  ld hl,instrument_c
  ld de,soundbufferc
  call ldir14
  pop hl
  
  ; NEXT CHANNEL
  inc hl
  ld (musicpos-2),hl
ret
  skipnosoundchannelc:
  inc hl
  skipnosoundchannelc2:
  inc hl

  ld (musicpos-2),hl
ret
  
  
ldir14:
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
ret
  
; INPUT
; HL = SOUND EFFECT TO PLAY
;dosounda:
  ;push af
  ;push de
;  call makesound_a
  ;pop de
  ;pop af
;ret  
; INPUT
; HL = SOUND EFFECT TO PLAY
;dosoundb:
  ;push af
  ;push de
;  call makesound_b
  ;pop de
  ;pop af
;ret  
; INPUT
; HL = SOUND EFFECT TO PLAY
;dosoundc:
  ;push af
  ;push de
;  call makesound_c
  ;pop de
  ;pop af
;ret  
  
; SOUND EFFECTS FOR MUSIC
; MUST BE ABLE TO CHANGE TONE!
  
; STATUS. 
;   BIT 0 SET   = SET UP SOUND
;   BIT 0 UNSET = NO CHANGE TO SOUND NEEDED
;   BIT 1 SET   = DURATION SET
;   BIT 2 SET   = VOLUME FADE SET
;   BIT 3 SET   = CHANNEL IN USE
;   BIT 4 SET   = TONE STEP UP EACH FRAME
;   BIT 5 SET   = TONE STEP DOWN EACH FRAME

; PIANO
instrument_a: 
  defb %00000101 ; SET UP SOUND, WE HAVE FADE
  defb 0         ; TIMER
  defb 0         ; CURRENT TIME
  defb 3         ; VOLUME STEP FADE
  defb 0         ; VOLUME STEP COUNT
  defb 0         ; TONE STEP UP OR DOWN
  soundeffect_a_tone:
  defb %11111111 ; TONE - LOWER 8 BITS 
  defb %00000000 ; TONE - UPPER 4 BITS
  defb %00000000 ; NOISE %---NNNNN 
  defb %00001111 ; VOLUME / ENVELOPE ---EVVVV
  defb %00000000 ; REPEAT SOUND WHEN FINISHED
  defb %00000000 ; TONE START COPY - LOWER 8 BITS
  defb %00000000 ; TONE START COPY - UPPER 8 BITS
  defb %00000000 ; VOLUME COPY
  
; PIANO
instrument_b: 
  defb %00000101 ; SET UP SOUND, WE HAVE FADE
  defb 0         ; TIMER
  defb 0         ; CURRENT TIME
  defb 3         ; VOLUME STEP FADE
  defb 0         ; VOLUME STEP COUNT
  defb 0         ; TONE STEP UP OR DOWN
  soundeffect_b_tone:
  defb %11111111 ; TONE - LOWER 8 BITS 
  defb %00000000 ; TONE - UPPER 4 BITS
  defb %00000000 ; NOISE %---NNNNN 
  defb %00001111 ; VOLUME / ENVELOPE ---EVVVV
  defb %00000000 ; REPEAT SOUND WHEN FINISHED
  defb %00000000 ; TONE START COPY - LOWER 8 BITS
  defb %00000000 ; TONE START COPY - UPPER 8 BITS
  defb %00000000 ; VOLUME COPY
  
; PIANO
instrument_c: 
  defb %00000101 ; SET UP SOUND, WE HAVE FADE
  defb 0         ; TIMER
  defb 0         ; CURRENT TIME
  defb 3         ; VOLUME STEP FADE
  defb 0         ; VOLUME STEP COUNT
  defb 0         ; TONE STEP UP OR DOWN
  soundeffect_c_tone:
  defb %11111111 ; TONE - LOWER 8 BITS 
  defb %00000000 ; TONE - UPPER 4 BITS
  defb %00000000 ; NOISE %---NNNNN 
  defb %00001111 ; VOLUME / ENVELOPE ---EVVVV
  defb %00000000 ; REPEAT SOUND WHEN FINISHED
  defb %00000000 ; TONE START COPY - LOWER 8 BITS
  defb %00000000 ; TONE START COPY - UPPER 8 BITS
  defb %00000000 ; VOLUME COPY

; CLEANSWEEP SOUND
;instrument_bold: 
;  defb %00100011 ; SET UP SOUND, WE HAVE DURATION
;  defb 20        ; TIMER
;  defb 0         ; CURRENT TIME
;  defb %00000011 ; VOLUME STEP FADE
;  defb %00000000 ; VOLUME STEP COUNT
;  defb 35        ; TONE STEP UP OR DOWN
;  soundeffect_bold_tone:
;  defb %11111111 ; TONE - LOWER 8 BITS
;  defb %00000111 ; TONE - UPPER 4 BITS
;  defb %00000000 ; NOISE %---NNNNN 
;  defb %00001010 ; VOLUME / ENVELOPE ---EVVVV
;  defb %00000000 ; TIMES TO REPEAT SOUND WHEN FINISHED
;  defb %11111111 ; TONE START COPY - LOWER 8 BITS
;  defb %00000111 ; TONE START COPY - UPPER 8 BITS
;  defb %00000000 ; VOLUME COPY  
  
; LAZER BOLT
;  defb %00010011 ; SET UP SOUND, WE HAVE DURATION
;  defb 20        ; TIMER
;  defb 0         ; CURRENT TIME
;  defb %00000011 ; VOLUME STEP FADE
;  defb %00000000 ; VOLUME STEP COUNT
;  defb 10        ; TONE STEP UP OR DOWN
;  soundeffect_c_tone:
;  defb %11111111 ; TONE - LOWER 8 BITS
;  defb %00000111 ; TONE - UPPER 4 BITS
;  defb %00000000 ; NOISE %---NNNNN 
;  defb %00001010 ; VOLUME / ENVELOPE ---EVVVV
;  defb %00000000 ; TIMES TO REPEAT SOUND WHEN FINISHED
;  defb %11111111 ; TONE START COPY - LOWER 8 BITS
;  defb %00000111 ; TONE START COPY - UPPER 8 BITS
;  defb %00000000 ; VOLUME COPY  
  
;musicpos: defw 0
ifndef harrierattack
; TONE, SOUND EFFECT
;             CHANNEL A   CHANNEL B    CHANNEL C
;------------------------------------------------------------------
; JERUSALEM RIDGE
;isjb equ 1
ifdef isjb
musicscore:
  defb 4
  defw         qA,       rE,        0 
  defb 2 ; NO CHANGE
  defw         qA,        rE,        0 
  defb 2 ; NO CHANGE
  defb 2 ; NO CHANGE
  defw         qA,        rE,        0 
  defw         qA,        rE,        0 
  defw         qA,        rE,        0 
  defb 3
  defw         pA,        0,        0 
  defw         pB,        0,        0 
  defw         qC,        0,        0 
  defw         qD,        0,        0 
  defb 4
  defw         qE,        0,        0 
  defb 3
  defw         qE,        0,        0 
  defw         qF,        0,        0 
  
  defw         qE,        0,        0 
  defw         qD,        0,        0 
  defw         qC,        0,        0 
  defw         qE,        0,        0 
  defw         qD,        0,        0 
  defw         qC,        0,        0 
  defw         qE,        0,        0 
  defw         qC,        0,        0 
  
  defw         pA,        0,        0 
  defw         pB,        0,        0 
  defw         qC,        0,        0 
  defw         qD,        0,        0 
  defw         qE,        0,        0 
  defw         qG,        0,        0 
  defw         qA,        0,        0 
  defw         qG,        0,        0 
  
  defw         qE,        0,        0 
  defw         qD,        0,        0 
  defw         qC,        0,        0 
  defw         qE,        0,        0 
  defw         qD,        0,        0 
  defw         qC,        0,        0 
  defw         pA,        0,        0 
  defw         pG,        0,        0 
  ;----
  defw         pA,        0,        0 
  defw         pB,        0,        0 
  defw         qC,        0,        0 
  defw         qD,        0,        0 
  defb 4
  defw         qE,        0,        0 
  defb 3
  defw         qE,        0,        0 
  defw         qF,        0,        0 
  
  defw         qE,        0,        0 
  defw         qD,        0,        0 
  defw         qC,        0,        0 
  defw         qE,        0,        0 
  defw         qD,        0,        0 
  defw         qC,        0,        0 
  defw         qE,        0,        0 
  defw         qC,        0,        0 
  ;----
  defw         pA,        0,        0 
  defw         pB,        0,        0 
  defw         qC,        0,        0 
  defw         qD,        0,        0 
  defw         qE,        0,        0 
  defw         qG,        0,        0 
  defw         qA,        0,        0 
  defw         qG,        0,        0 
  defw         qE,        0,        0 
  defw         qG,        0,        0
  defw         qE,        0,        0 
  defw         qD,        0,        0 
  defw         qC,        0,        0 
  defw         qE,        0,        0 
  defw         pA,        0,        0 
  defw         0,        0,        0 
  defw         0,        0,        0 
  defw         0,        0,        0 
  defw         pA,        0,        0 
  defw         0,        0,        0 

  defb 1,1
endif
iswellerman equ 1
ifdef iswellerman
  
; TONE, SOUND EFFECT
;             CHANNEL A   CHANNEL B    CHANNEL C
;------------------------------------------------------------------
; WELLERMAN
musicscore:
  ;defw         qD,        nB,        0      
  ;defb 3 ; HALF BEATS
  ;defw         qD,        oD,        oF      
  ;defw         qD,        0,         0      
  ;defw         qD,        nF,        0      
  ;defw         qE,        0,        0      
  ;defw         qF,        oD,        oF      
  ;defw         qE,        0,        0      
  ;defb 4 ; NORMAL BEATS
  ;defw         qD,        nB,        0      
  ;defw         0,         nF,        0      
  ;defw         0,         mB,        0      
  ;defw         0,         0,        0      
;------------------------------------------------------------------
; 1st VERSE
  defw         qA,         0,        0      
  defb 2 ; NO CHANGE
  ;defw         0,         0,        0      
  defw         qD,         nB,        0      
  defb 3 ; HALF BEATS
  defw         qD,         oD,        oF      
  defw         qD,         0,        0      
  defb 4 ; NORMAL BEATS
  defw         qD,         nF,        0      
  defw         qF,         oD,        oF      
  defw         qA,         nB,        0      
  defw         qA,         oD,        oF      
  defw         qA,         nF,        0      
  defw         0,         oD,        oF      
  defw         qB,         nE,        0      
  defw         qG,         oE,        oG      
  defw         qG,         nB,        0      
  defw         qB,         oE,        oG      
  
  
  defw         rD,         nB,        0      
  defw         qA,         oD,        oF      
  defw         qA,         nF,        0      
  defw         0,         oD,        oF      

;  defb 2 ; PAUSE  

  defw         qD,         nB,        0      
  defb 3 ; HALF BEATS
  defw         qD,         oD,        oF      
  defw         qD,         0,        0      
  defb 4 ; NORMAL BEATS
  defw         qD,         nF,        0      
  defw         qF,         oD,        oF      
  
  defw         qA,         nB,        0      
  defw         qA,         oD,        oF      
  defw         qA,         nF,        0      
  defw         0,         oD,        oF      
  
  
  defw         qA,         oF,        0      
  defw         qG,         oE,        0      
  defw         qF,         oD,        0      
  defw         qE,         oC,        0      
  defw         qD,         nB,        0      
  defw         0,         nF,        0      
  defw         0,         mA,        0      
  defb 2 ; NO CHANGE  

;------------------------------------------------------------------
; 2nd VERSE
  defw         nG,         qB,        rD      
  defw         oD,         oG,        0      
  defw         nG,         qB,        rD      
  defw         oD,         oG,        qB      


  defw         qF,         rC,        nD      
  defw         qA,         oF,        oD      
  defw         qA,         nA,        0      
  defw         0,         oF,        oD      

  defw         qB,         nE,        0      
  defw         qG,         oF,        oD      
  defw         qG,         nE,        0      
  defw         qB,         oF,        oD      
  defw         nB,         qF,        rD      
  defw         qA,         oF,        oD      
  defw         qA,         nF,        0      
  defw         0,         oF,        oD      
  
  
  defw         nG,         qB,        rD      
  defw         oD,         oG,        0      
  defw         nG,         qB,        rD      
  defw         oD,         oG,        qB      
  
  defw         qF,         rC,        nD      
  defw         qA,         oF,        oD      
  defw         qA,         nA,        0      
  defw         0,         oF,        oD      
  
  defw         qA,         oF,        0      
  defw         qG,         oE,        0      
  defw         qF,         oD,        0      
  defw         qE,         oC,        0      
  defw         qD,         nB,        0      
  defw         0,         nF,        0      
  defw         0,         mA,        0      
  defb 2 ; NO CHANGE      
;------------------------------------------------------------------
; 3rd VERSE
  defw         rA,         nB,        0      
  defw         rD,         oF,        oD      
  defb 3 ; SPEED UP
  defw         rD,         0,        0      
  defw         rD,         nF,        0      
  defw         rD,         0,        0      
  defw         rE,         oF,       oD      
  defw         rF,         0,        0      
  defw         rG,         0,        0
  defb 4 ; SLOW DOWN
  
  defw         rA,         nB,        0      

  defw         rB,         oF,        oD      
  defw         rA,         nF,        0      
  defw         0,         oF,        oD      
  defb 3 ; SPEED UP
  defw         rB,         nE,        0      
  defw         sD,         0,        0      
  defw         rB,         oC,        oE      
  defw         rA,         0,        0      
  defw         rG,         nB,        0      
  defw         rF,         0,        0      
  defw         rG,         oC,        oE      
  defw         rB,         0,        0      
  defb 4 ; SLOW
  defw         rA,         nB,        0      
  defw         0,         oF,        oD      
  defw         0,         nF,        0      
  defb 3 ; SPEED UP
  defw         rA,         oF,        oD      
  defw         rA,         0,        0      
  defb 4 ; SLOW
  defw         rD,         oF,        oD      
  defb 3 ; SPEED UP
  defw         rD,         0,        0      
  defw         rD,         nF,        0      
  defw         rD,         0,        0      
  defw         rE,         oF,       oD      
  defw         rF,         0,        0      
  defw         rG,         0,        0
  defb 4 ; SLOW
  defw         rA,         nB,        0      
  defw         rF,         oF,        oD      
  defw         rD,         nA,        0      
  defw         0,         oD,        oD      
  
  defb 3 ; SPEED UP
  defw         rA,         oF,        0      
  defw         rF,         0,        0      
  defw         rG,         oE,        0      
  defw         rE,         0,        0      
  defw         rF,         oD,        0      
  defw         rD,         0,        0      
  defw         rE,         oC,        0      
  defw         rC,         0,        0      
  defb 4 ; SLOW
  
  defw         rD,         nB,        0      
  defw         0,         nF,        0      
  defw         0,         mA,        0      
  defb 2 ; NO CHANGE     
  
;------------------------------------------------------------------
; 4th VERSE
  defw         nG,         qB,        rD      
  defw         oD,         oG,        0      
  defw         nG,         qB,        rD      
  defw         oD,         oG,        qB      


  defw         qF,         rC,        nD      
  defw         qA,         oF,        oD      
  defw         qA,         nA,        0      
  defw         0,         oF,        oD      
  defw         qB,         nE,        0      
  defw         qG,         oF,        oD      
  defw         qG,         nE,        0      
  defw         qB,         oF,        oD      
  defw         nB,         qF,        rD      
  defw         qA,         oF,        oD      
  defw         qA,         nF,        0      
  defw         0,         oF,        oD      
  
  defw         nG,         qB,        rD      
  defw         oD,         oG,        0      
  defw         nG,         qB,        rD      
  defw         oD,         oG,        qB      
  
  
  defw         rC,         qF,        nD      
  defw         qA,         oF,        oD      
  defw         qA,         nA,        0      
  defw         0,         oF,        oD      
  
  defw         qA,         oF,        0      
  defw         qG,         oE,        0      
  defw         qF,         oD,        0      
  defw         qE,         oC,        0      

  defw         qD,         nB,        0      
  defw         0,         nF,        0      
  defw         0,         mA,        0      
  defb 2 ; NO CHANGE    
  
  defb     1 ; LOOP
endif
endif

ifdef harrierattack

; TONE, SOUND EFFECT
;             CHANNEL A   CHANNEL B    CHANNEL C
;------------------------------------------------------------------
; LAST POST
;musicscore:
;  defw         oC,        0,        0      
;  defw         0,         0,        0      
;  defw         pG,        0,        0      
;  defw         0,         0,        0      
;  defw         0,         0,        0      
;  defw         0,         0,        0      
;  defw         oC,        0,        0      
;  defw         0,         0,        0      
;  defw         pG,        0,        0      
;  defw         0,         0,        0      
;  defw         0,         0,        0      
;  defw         0,         0,        0      
;  defw         oC,        0,        0      
;  defw         0,         0,        0      
;  defw         pG,        0,        0      
;  defw         0,         0,        0      
;  defw         pG,        0,        0      
;  defw         0,         0,        0      
;  defw         pG,        0,        0      
;  defw         0,         0,        0      
;  defw         qC,        0,        0      
;  defw         0,         0,        0      
;  defw         qE,        0,        0      
;  defw         0,         0,        0      
;  defw         pG,        0,        0      
;  defw         0,         0,        0      
;  defw         pG,        0,        0      
;  defw         pG,        0,        0      
;  defw         pG,        0,        0      
;  defw         0,         0,        0      
  
;  defb 255,255 ;LOOP
  
; TONE, SOUND EFFECT
;             CHANNEL A   CHANNEL B    CHANNEL C
;------------------------------------------------------------------
; I VOW TO THEE MY COUNTRY
musicscore:
  ;defb 3 ; HALF BEATS
  defw         pE,        oC,        0      
  defw         pG,        0,         0 
  ;defb 4 ; NORMAL SPEED
  defw         pF,        pA,        oC 
  defb 2 ; NO CHANGE
  defw         qC,        0,         0 
  defb 2 ; NO CHANGE
  defw         oB,        pF,        pB 
  defw         pG,        0,         0 
  defw         oC,        pG,        qC 
  defw         pD,        0,         0 
  defw         qC,        0,         0 
  defb 2 ; NO CHANGE
  defw         oC,        pG,        pB
  defb 2 ; NO CHANGE
  defw         oC,        pF,        pA 
  defw         pB,        0,         0 
  defw         pA,        0,         0
  defb 2 ; NO CHANGE
  defw         oB,        oD,        pG   
  defb 2 ; NO CHANGE
  defw         oC,        pE,        0  
  defb 2 ; NO CHANGE
  defb 2 ; NO CHANGE
  defb 2 ; NO CHANGE
  ;------------------------------------------------------------------
  ; NEXT LINE
  defw         oB,        pE,        0
  defw         pG,        0,         0
  defw         oC,        pF,        pA
  defb 2 ; NO CHANGE
  defw         qC,        0,         0
  defb 2 ; NO CHANGE
  defw         oB,        pF,        pB
  defw         pG,        0,         0
  defw         oC,        pG,        qC
  defw         qD,        0,         0
  defw         qE,        0,         0
  defb 2 ; NO CHANGE
  defw         pG,        qC,        qE
  defb 2 ; NO CHANGE
  defw         pF,        pA,        qE
  defw         qD,        0,         0
  defw         qC,        0,         0
  defb 2 ; NO CHANGE
  defw         pF,        pA,        qD
  defb 2 ; NO CHANGE
  defw         pE,        pG,        qC
  defb 2 ; NO CHANGE
  defb 2 ; NO CHANGE
  defb 2 ; NO CHANGE
  ;------------------------------------------------------------------
  ; NEXT LINE
  defw         oC,        pG,        0
  defw         pE,        0,         0
  defw         oD,        oB,        oG
  defb 2 ; NO CHANGE
  defw         oD,        0,         0
  defb 2 ; NO CHANGE
  defw         oC,        0,         0
  defw         pE,        0,         0
  defw         oD,        oG,        0
  defb 2 ; NO CHANGE
  defw         pG,        0,         0
  defb 2 ; NO CHANGE
  defw         oC,        pG,        0
  defw         pE,        0,         0
  defw         oB,        oD,        0
  defb 2 ; NO CHANGE
  defw         oD,        0,         0
  defb 2 ; NO CHANGE
  defw         oC,        pE,        0
  defw         oG,        0,         0
  defw         oC,        pF,        pA
  defb 2 ; NO CHANGE
  defb 2 ; NO CHANGE
  defb 2 ; NO CHANGE
  ;------------------------------------------------------------------
  ; NEXT LINE
  defw         oD,        pF,        pA
  defw         pB,        0,         0
  defw         oC,        qC,        0
  defb 2 ; NO CHANGE
  defw         qC,        0,         0
  defb 2 ; NO CHANGE
  defw         oC,        pE,        pB
  defb 2 ; NO CHANGE
  defw         oC,        pF,        pA
  defb 2 ; NO CHANGE
  defw         oC,        pG,        0
  defb 2 ; NO CHANGE
  defw         oC,        pF,        qC
  defb 2 ; NO CHANGE
  defw         oC,        pE,        0
  defb 2 ; NO CHANGE
  defw         oA,        oD,        0
  defw         oC,        0,         0
  defw         oD,        nF,        0
  defb 2 ; NO CHANGE
  defw         oA,        pF,        0
  defb 2 ; NO CHANGE
  defw         oD,        pG,        0
  defb 2 ; NO CHANGE
  defb 2 ; NO CHANGE
  defb 2 ; NO CHANGE
  ;------------------------------------------------------------------
  ; NEXT LINE
  defw         oC,        0,         0
  defw         oB,        pE,        0
  defw         pG,        0,         0
  defw         oC,        pF,        pA
  defb 2 ; NO CHANGE
  defw         qC,        0,         0
  defb 2 ; NO CHANGE
  defw         oB,        pF,        pB
  defw         pG,        0,         0
  defw         oC,        pG,        qC
  defw         qD,        0,         0
  defw         qC,        0,         0
  defb 2 ; NO CHANGE
  defw         oC,        pG,        pB
  defb 2 ; NO CHANGE
  defw         oC,        pF,        pA
  defw         pB,        0,         0
  defw         pA,        0,         0
  defb 2 ; NO CHANGE
  defw         oB,        oD,        pG
  defb 2 ; NO CHANGE
  defw         oC,        pE,        0
  defb 2 ; NO CHANGE
  defb 2 ; NO CHANGE
  defb 2 ; NO CHANGE
  ;------------------------------------------------------------------
  ; LAST LINE
  defw         oB,        pE,        0
  defw         pG,        0,         0
  defw         oC,        pF,        pA
  defb 2 ; NO CHANGE
  defw         qC,        0,         0
  defb 2 ; NO CHANGE
  defw         oB,        pF,        pB
  defw         pG,        0,         0
  defw         oC,        pG,        qC
  defw         pD,        0,         0  
  defw         qE,        0,         0
  defb 2 ; NO CHANGE
  defw         pG,        qC,        qE  
  defb 2 ; NO CHANGE
  defw         pF,        pA,        qE
  defw         qD,        0,         0
  defw         qC,        0,         0
  defb 2 ; NO CHANGE
  defw         pF,        pA,        qD  
  defb 2 ; NO CHANGE
  defw         pE,        pG,        qC
  defb 2 ; NO CHANGE
  defb 2 ; NO CHANGE
  defb 2 ; NO CHANGE
  
  defb         1,1 ; LOOP

endif

ifdef standalone
dosound:
  call set_mixer_tone

  ;ld hl,frigate1sound
  ;#call makesound_a
  ;call waitchannel_a_nointerrupt

  ;ld hl,frigate2sound
  ;call makesound_a
  ;call waitchannel_a_nointerrupt

  call set_mixer_noise

  ;ld hl,flacksound
  ;call makesound_b

  call doaxesound;dodingsound

  call waitchannel_c_nointerrupt


  call dodingsound;dodingsound

  call waitchannel_b_nointerrupt

  call dochestsound;dodingsound

  call waitchannel_a_nointerrupt

  ;ld hl,flacksound
  ;call makesound_c
  ;call waitchannel_c_nointerrupt
  ;ld hl,flacksound
  ;call makesound_b_or_c
  ;call waitchannel_b_nointerrupt

  ;ld hl,bombsound
  ;call makesound_b_or_c
  ;call waitchannel_b_nointerrupt

  ;ld hl,flightsound
  ;call makesound_c
  ;call waitchannel_c_nointerrupt

  ;ld hl,flightsound
  ;call makesound_b_or_c
  ;ld hl,bombsound
  ;call makesound_b_or_c
  ;call waitchannel_b_nointerrupt
  ;call waitchannel_c_nointerrupt

  ld hl,silence
  call makesound_a

  ld hl,silence
  call makesound_b

  ld hl,silence
  call makesound_c
ret
endif

ifdef HARRIERATTACK
;dodingsound:
;  push af
;  push de
;  call set_mixer_tone
;  ld hl,startsound1
;  call makesound_b
;  ld hl,startsound2
;  call makesound_b
;  pop de
;  pop af
;ret  

; BLACKBOX COMMANDS
; CALLING PROGRAM DOES NOT KNOW GOINGS ON, IT ONLY SEES THESE FUNCTIONS
dofrigatenoisef:
  call set_mixer_tone
  ld hl,frigate1sound
  call makesound_a
  call waitchannel_a_interrupt
  ld hl,frigate2sound
  call makesound_a
  call waitchannel_a_interrupt
  jp set_mixer_noise

doflightnoisef:
  ld (flightnoisepitch),a
  ld hl,flightsound
  jp makesound_a
  
doflacknoisef:
  ld hl,flacksound
  jp makesound_c
 
domissilenoisef:
  ld hl,missilesound
  jp makesound_b

dobombnoisef:
  ld hl,bombsound
  jp makesound_b
  
doloudbombnoisef:
  ld hl,loudbombsound
  jp makesound_b
  
doexplosionnoisef:
  ld hl,explosionsound 
  jp makesound_b
endif

ifndef HARRIERATTACK
dodingsound:
  push af
  push de
  call set_mixer_tone
  ld hl,dingsound
  call makesound_a
  ld hl,dingsound2
  call makesound_b
  pop de
  pop af
ret  

dochestsound:
  push af
  push de
  call set_mixer_tone
  ld hl,chestsound
  call makesound_a
  pop de
  pop af
ret  

doaxesound:
  ld hl,axesound
  jr dosoundc
dodoorknock:
  ld hl,doorknocksound
  jr dosoundc
doarrowsound:
  ld hl,arrowsound
; INPUT
; HL = SOUND EFFECT TO PLAY
dosoundc:
  push af
  push de
  call makesound_c
  pop de
  pop af
ret
dosplashsound:
  ld hl,splashsound
; INPUT
; HL = SOUND EFFECT TO PLAY
dosounda:
  push af
  push de
  call makesound_a
  pop de
  pop af
ret  
endif

; CLEAR SOUND BUFFERS  
initsoundf:
  ld hl,soundbuffera
  ld (hl),0
  ld de,soundbuffera+1
  ld bc,35 ; 3 BUFFERS
  ldir

  call clearsound_a ; CLEAR CHANNELS
  call clearsound_b
  call clearsound_c
  jp set_mixer_tone
  
defb "CHRIS9"

; ---------------------------------------
; SOUND

;Reg 	Meaning 			Bit Meaning 	Details
;0 		Tone Pitch L - Channel A 		LLLLLLLL 	FINE TUNE   - Lower value = Higher pitch
;1 		Tone Pitch H - Channel A 		----HHHH 	COARSE TUNE - Lower value = Higher pitch
;2 		Tone Pitch L - Channel B 		LLLLLLLL 	FINE TUNE   - Lower value = Higher pitch
;3 		Tone Pitch H - Channel B 		----HHHH 	COARSE TUNE - Lower value = Higher pitch
;4 		Tone Pitch L - Channel C 		LLLLLLLL 	FINE TUNE   - Lower value = Higher pitch
;5 		Tone Pitch H - Channel C 		----HHHH 	COARSE TUNE - Lower value = Higher pitch
;6 		Noise Generator 				---NNNNN 	Higher = Faster noise
;7 		Mixer  							--NNNTTT   	N=Noise T=Tone (Channel --CBACBA 1=mute 0=normal)
;8 		Amplitude - Channel A 			---EVVVV 	E=Envelope (1=Enabled) VVVV=Volume
;9	 	Amplitude - Channel B 			---EVVVV 	E=Envelope (1=Enabled) VVVV=Volume
;10 	Amplitude - Channel C	 		---EVVVV 	E=Envelope (1=Enabled) VVVV=Volume
;11 	Envelope L (Volume over time)  	LLLLLLLL 	Lower=Faster Envelope
;12 	Envelope H (Volume over time)  	HHHHHHHH 	Lower=Faster Envelope
;13 	Envelope Selection 				----EEEH 	E=Envelope number E (See PDF) H=Hold

;Hardware Envelope shapes
;x indicates bit with any value (either 1 or 0)

;Bits                            Envelope shape

;3       2       1       0
;-----------------------------------------------------------------
;
;0       0       x       x       \________________________
;
;0       1       x       x       /|_______________________
;
;1       0       0       0       \|\|\|\|\|\|\|\|\|\|\|\|\
;
;1       0       0       1       \________________________
;
;1       0       1       0       \/\/\/\/\/\/\/\/\/\/\/\/\
;                                  _______________________
;1       0       1       1       \|
;
;1       1       0       0       /|/|/|/|/|/|/|/|/|/|/|/|/
;                                 ________________________
;1       1       0       1       /
;
;1       1       1       0       /\/\/\/\/\/\/\/\/\/\/\/\/
;
;1       1       1       1       /|_______________________


;Summary

;Bit 7  } Not used
;Bit 6  }
;Bit 5  }
;Bit 4  }

;Bit 3  Continue        ;} See table for envelope shapes.
;Bit 2  Attack          ;}
;Bit 1  Alternate       ;}
;Bit 0  Hold            ;}


; CHANNEL STATUS, MIXER, TONE L, TONE H, NOISE, VOL, ENV L, ENV H, SELECT ENV, 

; STATUS. 
;   BIT 0 SET   = SET UP SOUND
;   BIT 0 UNSET = NO CHANGE TO SOUND NEEDED
;   BIT 1 SET   = DURATION SET
;   BIT 2 SET   = VOLUME FADE SET
;   BIT 3 SET   = CHANNEL IN USE
;   BIT 4 SET   = TONE STEP UP EACH FRAME
;   BIT 5 SET   = TONE STEP DOWN EACH FRAME

ifdef HARRIERATTACK
frigate1sound: 
  defb %00000011 ; SET UP SOUND, WE HAVE DURATION
  defb 50        ; TIMER
  defb 0         ; CURRENT TIME
  defb %00000000 ; VOLUME STEP FADE
  defb %00000000 ; VOLUME STEP COUNT
  defb 0          ; TONE STEP UP OR DOWN
  defb %11111111 ; TONE - LOWER 8 BITS
  defb %00000111 ; TONE - UPPER 4 BITS
  defb %00000000 ; NOISE %---NNNNN 
  defb %00001010 ; VOLUME / ENVELOPE ---EVVVV
  defb %00000000 ; TIMES TO REPEAT SOUND WHEN FINISHED
  defb %00000000 ; TONE START COPY - LOWER 8 BITS
  defb %00000000 ; TONE START COPY - UPPER 8 BITS
  defb %00000000 ; VOLUME COPY

frigate2sound: 
  defb %00000011 ; SET UP SOUND, WE HAVE DURATION
  defb 50        ; TIMER
  defb 0         ; CURRENT TIME
  defb %00000000 ; VOLUME STEP FADE
  defb %00000000 ; VOLUME STEP COUNT
  defb 0         ; TONE STEP UP OR DOWN
  defb %11111111 ; TONE - LOWER 8 BITS
  defb %00001111 ; TONE - UPPER 4 BITS
  defb %00000000 ; NOISE %---NNNNN 
  defb %00001010 ; VOLUME / ENVELOPE ---EVVVV
  defb %00000000 ; REPEAT SOUND WHEN FINISHED
  defb %00000000 ; TONE START COPY - LOWER 8 BITS
  defb %00000000 ; TONE START COPY - UPPER 8 BITS
  defb %00000000 ; VOLUME COPY

flightsound:
  defb %00000011 ; SET UP SOUND, WE HAVE DURATION
  defb 180       ; TIMER
  defb 0         ; CURRENT TIME
  defb %00000000 ; VOLUME STEP FADE
  defb %00000000 ; VOLUME STEP COUNT
  defb 0         ; TONE STEP UP OR DOWN
  defb %00011111 ; TONE - LOWER 8 BITS
  defb %00010000 ; TONE - UPPER 4 BITS
flightnoisepitch:
  defb %00000000 ; NOISE %---NNNNN PITCH
  defb %00000110 ; VOLUME / ENVELOPE ---EVVVV
  defb %00000000 ; REPEAT SOUND WHEN FINISHED
  defb %00000000 ; TONE START COPY - LOWER 8 BITS
  defb %00000000 ; TONE START COPY - UPPER 8 BITS
  defb %00000000 ; VOLUME COPY

;startsound1:
;  defb %00100011 ; SET UP SOUND, WE HAVE DURATION
;  defb 100       ; TIMER
;  defb 0         ; CURRENT TIME
;  defb %00000000 ; VOLUME STEP FADE
;  defb %00000000 ; VOLUME STEP COUNT
;  defb 2         ; TONE STEP UP OR DOWN
;  defb %00011111 ; TONE - LOWER 8 BITS
;  defb %00010000 ; TONE - UPPER 4 BITS
;  defb %00000000 ; NOISE %---NNNNN PITCH
;  defb %00001111 ; VOLUME / ENVELOPE ---EVVVV
;  defb %00000000 ; REPEAT SOUND WHEN FINISHED
;  defb %00011111 ; TONE START COPY - LOWER 8 BITS
;  defb %00010000 ; TONE START COPY - UPPER 8 BITS
;  defb %00000111 ; VOLUME COPY

;startsound2:
;  defb %00100011 ; SET UP SOUND, WE HAVE DURATION
;  defb 100       ; TIMER
;  defb 0         ; CURRENT TIME
;  defb %00000000 ; VOLUME STEP FADE
;  defb %00000000 ; VOLUME STEP COUNT
;  defb 2         ; TONE STEP UP OR DOWN
;  defb %00111111 ; TONE - LOWER 8 BITS
;  defb %00010000 ; TONE - UPPER 4 BITS
;  defb %00000000 ; NOISE %---NNNNN PITCH
;  defb %00001111 ; VOLUME / ENVELOPE ---EVVVV
;  defb %00000000 ; REPEAT SOUND WHEN FINISHED
;  defb %00011111 ; TONE START COPY - LOWER 8 BITS
;  defb %00010000 ; TONE START COPY - UPPER 8 BITS
;  defb %00000111 ; VOLUME COPY

; BOMB SOUND
bombsound: 
  defb %00000101 ; SET UP SOUND, WE HAVE FADE
  defb 40        ; TIMER
  defb 0         ; CURRENT TIME
  defb 3         ; VOLUME STEP FADE
  defb 0         ; VOLUME STEP COUNT
  defb 0         ; TONE STEP UP OR DOWN
  defb %11111111 ; TONE - LOWER 8 BITS
  defb %00000000 ; TONE - UPPER 4 BITS
  defb %00011111 ; NOISE %---NNNNN 
  defb %00000111 ; VOLUME / ENVELOPE ---EVVVV
  defb %00000000 ; REPEAT SOUND WHEN FINISHED
  defb %00000000 ; TONE START COPY - LOWER 8 BITS
  defb %00000000 ; TONE START COPY - UPPER 8 BITS
  defb %00000000 ; VOLUME COPY

; BOMB SOUND
loudbombsound: 
  defb %00000101 ; SET UP SOUND, WE HAVE FADE
  defb 40        ; TIMER
  defb 0         ; CURRENT TIME
  defb 3         ; VOLUME STEP FADE
  defb 0         ; VOLUME STEP COUNT
  defb 0         ; TONE STEP UP OR DOWN
  defb %11111111 ; TONE - LOWER 8 BITS 
  defb %00000000 ; TONE - UPPER 4 BITS
  defb %00011111 ; NOISE %---NNNNN 
  defb %00001100 ; VOLUME / ENVELOPE ---EVVVV
  defb %00000000 ; REPEAT SOUND WHEN FINISHED
  defb %00000000 ; TONE START COPY - LOWER 8 BITS
  defb %00000000 ; TONE START COPY - UPPER 8 BITS
  defb %00000000 ; VOLUME COPY

explosionsound: 
  defb %00000101 ; SET UP SOUND, WE HAVE FADE
  defb 60        ; TIMER
  defb 0         ; CURRENT TIME
  defb 6         ; VOLUME STEP FADE
  defb 0         ; VOLUME STEP COUNT
  defb 0         ; TONE STEP UP OR DOWN
  defb %11111111 ; TONE - LOWER 8 BITS
  defb %00001000 ; TONE - UPPER 4 BITS
  defb %00011111 ; NOISE %---NNNNN 
  defb %00001111 ; VOLUME / ENVELOPE ---EVVVV
  defb %00000000 ; REPEAT SOUND WHEN FINISHED
  defb %00000000 ; TONE START COPY - LOWER 8 BITS
  defb %00000000 ; TONE START COPY - UPPER 8 BITS
  defb %00000000 ; VOLUME COPY

; FLACK SOUND
flacksound: 
  defb %00000101 ; SET UP SOUND, WE HAVE FADE         ; STATUS
  defb 10        ; TIMER
  defb 0         ; CURRENT TIME
  defb 1         ; VOLUME STEP FADE
  defb %00000000 ; VOLUME STEP COUNT
  defb 0         ; TONE STEP UP OR DOWN
  defb %00111111 ; TONE - LOWER 8 BITS
  defb %00000100 ; TONE - UPPER 4 BITS
  defb %00001111 ; NOISE %---NNNNN 
  defb %00000111 ; VOLUME / ENVELOPE ---EVVVV
  defb %00000000 ; TIMES TO REPEAT SOUND WHEN FINISHED
  defb %00111111 ; TONE START COPY - LOWER 8 BITS
  defb %00000100 ; TONE START COPY - UPPER 8 BITS
  defb %00000111 ; VOLUME COPY

missilesound:
  defb %00000101 ; SET UP SOUND, WE HAVE FADE         ; STATUS
  defb 50        ; TIMER
  defb 0         ; CURRENT TIME
  defb 3         ; VOLUME STEP FADE
  defb 0         ; VOLUME STEP COUNT
  defb 0         ; TONE STEP UP OR DOWN
  defb %00011111 ; TONE - LOWER 8 BITS
  defb %00010000 ; TONE - UPPER 4 BITS
  defb %00000011 ; NOISE %---NNNNN PITCH
  defb %00000111 ; VOLUME / ENVELOPE ---EVVVV
  defb %00000000 ; REPEAT SOUND WHEN FINISHED
  defb %00000000 ; TONE START COPY - LOWER 8 BITS
  defb %00000000 ; TONE START COPY - UPPER 8 BITS
  defb %00000000 ; VOLUME COPY
endif


; STATUS. 
;   BIT 0 SET   = SET UP SOUND
;   BIT 0 UNSET = NO CHANGE TO SOUND NEEDED
;   BIT 1 SET   = DURATION SET
;   BIT 2 SET   = VOLUME FADE SET
;   BIT 3 SET   = CHANNEL IN USE
;   BIT 4 SET   = TONE STEP UP EACH FRAME
;   BIT 5 SET   = TONE STEP DOWN EACH FRAME

ifndef HARRIERATTACK
; BOMB SOUND
dingsound: 
  defb %00100101 ; SET UP SOUND, WE HAVE FADE         ; STATUS
  defb 10        ; TIMER
  defb 0         ; CURRENT TIME
  defb 1         ; VOLUME STEP FADE
  defb 0         ; VOLUME STEP COUNT
  defb 12        ; TONE STEP UP OR DOWN
  defb %00001111 ; TONE - LOWER 8 BITS
  defb %00000000 ; TONE - UPPER 4 BITS
  defb %00000000 ; NOISE %---NNNNN 
  defb %00001000 ; VOLUME / ENVELOPE ---EVVVV
  defb %00000000 ; TIMES TO REPEAT SOUND WHEN FINISHED
  defb %00001111 ; TONE START COPY - LOWER 8 BITS
  defb %00000000 ; TONE START COPY - UPPER 8 BITS
  defb %00001000 ; VOLUME COPY
  
dingsound2: 
  defb %00100101 ; SET UP SOUND, WE HAVE FADE         ; STATUS
  defb 10        ; TIMER
  defb 0         ; CURRENT TIME
  defb 1         ; VOLUME STEP FADE
  defb 0         ; VOLUME STEP COUNT
  defb 12        ; TONE STEP UP OR DOWN
  defb %00111111 ; TONE - LOWER 8 BITS
  defb %00000000 ; TONE - UPPER 4 BITS
  defb %00000000 ; NOISE %---NNNNN 
  defb %00001000 ; VOLUME / ENVELOPE ---EVVVV
  defb %00000000 ; TIMES TO REPEAT SOUND WHEN FINISHED
  defb %00111111 ; TONE START COPY - LOWER 8 BITS
  defb %00000000 ; TONE START COPY - UPPER 8 BITS
  defb %00001000 ; VOLUME COPY
  
; AXE SOUND
axesound: 
  defb %00000101 ; SET UP SOUND, WE HAVE FADE
  defb 60        ; TIMER
  defb 0         ; CURRENT TIME
  defb 3         ; VOLUME STEP FADE
  defb 0         ; VOLUME STEP COUNT
  defb 10         ; TONE STEP UP OR DOWN
  defb %11111111 ; TONE - LOWER 8 BITS
  defb %00000000 ; TONE - UPPER 4 BITS
  defb %00011111 ; NOISE %---NNNNN 
  defb %00001000 ; VOLUME / ENVELOPE ---EVVVV
  defb %00000000 ; TIMES TO REPEAT SOUND WHEN FINISHED
  defb %00000000 ; TONE START COPY - LOWER 8 BITS
  defb %00000000 ; TONE START COPY - UPPER 8 BITS
  defb %00001000 ; VOLUME COPY
  
arrowsound:
  defb %00000101 ; SET UP SOUND, WE HAVE FADE         ; STATUS
  defb 50        ; TIMER
  defb 0         ; CURRENT TIME
  defb 3         ; VOLUME STEP FADE
  defb 0         ; VOLUME STEP COUNT
  defb 0         ; TONE STEP UP OR DOWN
  defb %00011111 ; TONE - LOWER 8 BITS
  defb %00010000 ; TONE - UPPER 4 BITS
  defb %00000011 ; NOISE %---NNNNN PITCH
  defb %00000111 ; VOLUME / ENVELOPE ---EVVVV
  defb %00000000 ; TIMES TO REPEAT SOUND WHEN FINISHED
  defb %00000000 ; TONE START COPY - LOWER 8 BITS
  defb %00000000 ; TONE START COPY - UPPER 8 BITS
  defb %00000000 ; VOLUME COPY

doorknocksound: 
  defb %00000101 ; SET UP SOUND, WE HAVE FADE         ; STATUS
  defb 10        ; TIMER
  defb 0         ; CURRENT TIME
  defb 1         ; VOLUME STEP FADE
  defb %00000000 ; VOLUME STEP COUNT
  defb 0         ; TONE STEP UP OR DOWN
  defb %00111111 ; TONE - LOWER 8 BITS
  defb %00000100 ; TONE - UPPER 4 BITS
  defb %00001111 ; NOISE %---NNNNN 
  defb %00000111 ; VOLUME / ENVELOPE ---EVVVV
  defb %00000010 ; TIMES TO REPEAT SOUND WHEN FINISHED
  defb %00111111 ; TONE START COPY - LOWER 8 BITS
  defb %00000100 ; TONE START COPY - UPPER 8 BITS
  defb %00000111 ; VOLUME COPY
  
splashsound: 
  defb %00000101 ; SET UP SOUND, WE HAVE FADE
  defb 40        ; TIMER
  defb 0         ; CURRENT TIME
  defb 3         ; VOLUME STEP FADE
  defb 0         ; VOLUME STEP COUNT
  defb 0         ; TONE STEP UP OR DOWN
  defb %11111111 ; TONE - LOWER 8 BITS
  defb %00000000 ; TONE - UPPER 4 BITS
  defb %00011111 ; NOISE %---NNNNN 
  defb %00000111 ; VOLUME / ENVELOPE ---EVVVV
  defb %00000000 ; TIMES TO REPEAT SOUND WHEN FINISHED
  defb %00000000 ; TONE START COPY - LOWER 8 BITS
  defb %00000000 ; TONE START COPY - UPPER 8 BITS
  defb %00000000 ; VOLUME COPY

chestsound: 
  defb %00100011 ; SET UP SOUND, WE HAVE FADE         ; STATUS
  defb 50        ; TIMER
  defb 0         ; CURRENT TIME
  defb 1         ; VOLUME STEP FADE
  defb 0         ; VOLUME STEP COUNT
  defb 80        ; TONE STEP UP OR DOWN
  defb %01111111 ; TONE - LOWER 8 BITS
  defb %00000000 ; TONE - UPPER 4 BITS
  defb %00000000 ; NOISE %---NNNNN 
  defb %00001111 ; VOLUME / ENVELOPE ---EVVVV
  defb %00000000 ; TIMES TO REPEAT SOUND WHEN FINISHED
  defb %01111111 ; TONE START COPY - LOWER 8 BITS
  defb %00000000 ; TONE START COPY - UPPER 8 BITS
  defb %00001111 ; VOLUME COPY
endif
ifdef standalone
silence: 
  defb %00000011 ; SET UP SOUND, WE HAVE DURATION
  defb 1         ; TIMER
  defb 0         ; CURRENT TIME
  defb %00000000 ; VOLUME STEP FADE
  defb %00000000 ; VOLUME STEP COUNT
  defb 0         ; TONE STEP UP OR DOWN
  defb %00000000 ; TONE - LOWER 8 BITS
  defb %00000000 ; TONE - UPPER 4 BITS
  defb %00000000 ; NOISE %---NNNNN 
  defb %00000000 ; VOLUME / ENVELOPE ---EVVVV
  defb %00000000 ; TIMES TO REPEAT SOUND WHEN FINISHED
  defb %00000000 ; TONE START COPY - LOWER 8 BITS
  defb %00000000 ; TONE START COPY - UPPER 8 BITS
  defb %00000000 ; VOLUME COPY
endif
; ------------------
; BUFFERS
; ------------------
soundbuffera:
  defb 0         ; STATUS
  defb 0         ; TIMER
  defb 0         ; CURRENT TIME
  defb 0         ; VOLUME STEP FADE
  defb 0         ; VOLUME STEP COUNT
  defb 0         ; TONE STEP UP OR DOWN
  defb %00000000 ; TONE - LOWER 8 BITS
  defb %00000000 ; TONE - UPPER 4 BITS
  defb %00000000 ; NOISE %---NNNNN 
  defb %00000000 ; VOLUME / ENVELOPE ---EVVVV
  defb %00000000 ; TIMES TO REPEAT SOUND WHEN FINISHED
  defb %00000000 ; TONE START COPY - LOWER 8 BITS
  defb %00000000 ; TONE START COPY - UPPER 8 BITS
  defb %00000000 ; VOLUME COPY

soundbufferb:
  defb 0         ; STATUS
  defb 0         ; TIMER
  defb 0         ; CURRENT TIME
  defb 0         ; VOLUME STEP FADE
  defb 0         ; VOLUME STEP COUNT
  defb 0         ; TONE STEP UP OR DOWN
  defb %00000000 ; TONE - LOWER 8 BITS
  defb %00000000 ; TONE - UPPER 4 BITS
  defb %00000000 ; NOISE %---NNNNN 
  defb %00000000 ; VOLUME / ENVELOPE ---EVVVV
  defb %00000000 ; TIMES TO REPEAT SOUND WHEN FINISHED
  defb %00000000 ; TONE START COPY - LOWER 8 BITS
  defb %00000000 ; TONE START COPY - UPPER 8 BITS
  defb %00000000 ; VOLUME COPY

soundbufferc:
  defb 0         ; STATUS
  defb 0         ; TIMER
  defb 0         ; CURRENT TIME
  defb 0         ; VOLUME STEP FADE
  defb 0         ; VOLUME STEP COUNT
  defb 0         ; TONE STEP UP OR DOWN
  defb %00000000 ; TONE - LOWER 8 BITS
  defb %00000000 ; TONE - UPPER 4 BITS
  defb %00000000 ; NOISE %---NNNNN 
  defb %00000000 ; VOLUME / ENVELOPE ---EVVVV
  defb %00000000 ; TIMES TO REPEAT SOUND WHEN FINISHED
  defb %00000000 ; TONE START COPY - LOWER 8 BITS
  defb %00000000 ; TONE START COPY - UPPER 8 BITS
  defb %00000000 ; VOLUME COPY

set_mixer_noise:
  ld a,7	    ; MIXER - %PPNNNTTT (1=off) --CBACBA PP=PORTS
  ld c,%00000111
  jp RegWrite

; A TONE, B TONE, C NOISE
set_mixer_tone:
  ld a,7	    ; MIXER - %PPNNNTTT (1=off) --CBACBA PP=PORTS
  ld c,%00111000
  jp RegWrite
  
set_mixer_music:
  ld a,7	    ; MIXER - %PPNNNTTT (1=off) --CBACBA PP=PORTS
  ld c,%00111000
  jp RegWrite

playsound_all_channelsf:
  call playsound_a
  call playsound_b
  jp playsound_c
  
ifdef standalone
waitchannel_a_nointerrupt:
  call mc_wait_flyback 
  call playsound_a
  ld a,(soundbuffera)
  or a
  ret z
jr waitchannel_a_nointerrupt
waitchannel_b_nointerrupt:
  call mc_wait_flyback 
  call playsound_b
  ld a,(soundbufferb)
  or a
  ret z
jr waitchannel_b_nointerrupt
waitchannel_c_nointerrupt:
  call mc_wait_flyback 
  call playsound_c
  ld a,(soundbufferc)
  or a
  ret z
jr waitchannel_c_nointerrupt
endif
waitchannel_a_interrupt:
  ld a,(soundbuffera)
  or a
  ret z
jr waitchannel_a_interrupt
waitchannel_b_interrupt:
  ld a,(soundbufferb)
  or a
  ret z
jr waitchannel_b_interrupt
waitchannel_c_interrupt:
  ld a,(soundbufferc)
  or a
  ret z
jr waitchannel_c_interrupt

RegWrite:
  push bc
  
  ld b,&f4
  ld c,a
  out (c),c	     ;#f4 Regnum

  ld bc,&F6C0	 ;Select REG
  out (c),c	

  ld c,&00    ;Inactive
  out (c),c

  ld c,&80	 ;Write VALUE
  out (c),c	
  pop bc

  ld b,&F4	     ;#f4 value
  out (c),c

  ld bc,&f600    ;"inactive"
  out (c),c
ret

; SEND SOUND THROUGH A CHANNEL
makesound_a:
  ld de,soundbuffera
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
ret
  ;ld bc,14
  ;ldir
;ret


;  jp playsound_a
; SEND SOUND THROUGH B OR C CHANNEL (USE C IF B IS IN USE)
; DON'T START TO PLAY IT, ALLOW INTERRUPTS TO DO THAT AUTOMATICALLY
; OTHERWISE YOU RISK SLOWING DOWN THE INTERRUPT TOO MUCH
; AND CAUSING PROBLEMS WITH THE SPLIT PALETTE
makesound_b_or_c:
  ld a,(soundbufferb)
  bit 3,a ; CHECK IF IN USE
  jr nz,makesound_c
; SEND SOUND THROUGH B CHANNEL
makesound_b:
  ld de,soundbufferb
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
ret
;  ld bc,14
;  ldir
;ret
;  jp playsound_b
; SEND SOUND THROUGH C CHANNEL
makesound_c:
  ld de,soundbufferc
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
ret
;  ld bc,14
;  ldir
;ret
;  jp playsound_c

; CLEAR SOUND FROM BUFFER AS IT HAS FINISHED PLAYING
clearsound_a:
  ld (soundbuffera),a ; SET STATUS AS 0

  ld a,8                 ; VOLUME / ENVELOPE FOR CHANNEL A ---EVVVV
  ld c,%00000000         ; E=1 MEANS ENVELOPE ON
  call RegWrite

  xor a
  ld c,%00000000         ; TONE - LOWER 8 BITS - CHANNEL B
  call RegWrite
  
  ld a,1
  ld c,%00000000         ; TONE - UPPER 4 BITS
  jp RegWrite


playsound_a:
  ld a,(soundbuffera)  ; NO CHANGE, JUST RETURN
  or a
  ret z  
  
  bit 5,a ; TONE STEP DOWN
  jr z,skiptonestepdowna
  
  ;-------------------------------------------------------------------
  ; ADJUST TONE DOWN BY STEP
  ld b,0
  ld a,(soundbuffera+5)  ; GET TONE STEP
  ld c,a
  ld hl,(soundbuffera+6) ; GET ACTUAL TONE
  sbc hl,bc              ; SUBRTRACT
  ld (soundbuffera+6),hl ; SAVE TO BUFFER
  ld a,(soundbuffera) 
  set 0,a                ; MAKE SURE WE REDO SOUND TONE
  ld (soundbuffera),a 
  skiptonestepdowna:
  
  bit 4,a ; TONE STEP UP
  jr z,skiptonestepupa

  ;-------------------------------------------------------------------
  ; ADJUST TONE UP BY STEP
  ld b,0
  ld a,(soundbuffera+5)  ; GET TONE STEP
  ld c,a
  ld hl,(soundbuffera+6) ; GET ACTUAL TONE
  add hl,bc              ; ADD TOGETHER
  ld (soundbuffera+6),hl ; SAVE TO BUFFER
  ld a,(soundbuffera) 
  set 0,a                ; MAKE SURE WE REDO SOUND TONE
  ld (soundbuffera),a 
  skiptonestepupa:

  bit 1,a
  jr z,skipchecktimera

  ;-------------------------------------------------------------------
  ; TIMER - PLAYSOUND IF 0, ELSE INCREMENT UNTIL WE REACH TIMER
  ld hl,soundbuffera+1
  
  ld a,(hl)                 ; GET TIMER
  inc hl
  cp (hl)                   ; COMPARE TO CURRENT TIME
  jr z,check_clearsound_a   ; IF  SAME, SILENCE CHANNEL IF NO REPEAT
  inc (hl)                  ; INC CURRENT TIME
  ld a,(soundbuffera)       ; GET STATUS IN A
  skipchecktimera:

  bit 2,a 
  jr z,skipcheckvolfadea

  ;-------------------------------------------------------------------
  ; VOLUME FADE - CHECK IF STEP COUNTER HAS REACHED MAX, IF SO, DEC VOLUME
  ld hl,soundbuffera+3

  ld a,(hl)                   ; GET VOLUME FADE STEPS
  inc hl                      ; MOVE TO CURRENT VOLUME
  cp (hl)                     ; COMPARE TO CURRENT VOLUME FADE
  jr nz,skipdovolumefadea     ; IF WE MATCH STEP COUNT, DEC VOLUME

  ld (hl),0                   ; RESET VOLUME FADE
  ld hl,soundbuffera+9
  dec (hl)                    ; DEC VOLUME
  ld a,(hl)                   ; CHECK IF VOLUME IS 0
  or a
  jr z,check_clearsound_a
  
  ld a,(soundbuffera)         ; GET STATUS IN A
  bit 0,a                     ; BIT 0 SET - WE NEED TO CHANGE THE AY SOUND
  jr nz,setupsoundchannela    ; SOUND IS FINISHED - RESET STATUS
  
  ; TONE UNCHANGED - JUST CHANGE VOLUME
  jr justchangevolumea
      
  skipdovolumefadea:
  inc (hl)                    ; INC CURRENT VOLUME FADE
  ld a,(soundbuffera)         ; GET STATUS IN A
  bit 0,a                     ; BIT 0 SET - WE NEED TO CHANGE THE AY SOUND
  jr nz,setupsoundchannela    ; SOUND IS FINISHED - RESET STATUS
  
  justchangevolumea:
  ; TONE UNCHANGED - JUST CHANGE VOLUME
  ld hl,soundbuffera+9 ; MOVE TO TONE LOWER

  ld a,8               ; VOLUME / ENVELOPE FOR CHANNEL A ---EVVVV
  ld c,(hl)            ; E=1 MEANS ENVELOPE ON
  jp RegWrite

  skipcheckvolfadea:
  bit 0,a ; BIT 0 SET - WE NEED TO START THE AY SOUND
  jr nz,setupsoundchannela
  ret
  
  ;-------------------------------------------------------------------
  ; CHECK IF WE NEED TO REPEAT SOUND, IF NOT, SILENCE CHANNEL
  check_clearsound_a:
  inc hl
  ld a,(hl)               ; CHECK IF WE NEED TO REPEAT SOUND
  or a
  jp z,clearsound_a       ; NO, JUST SILENCE SOUND
  
  dec a
  ld (hl),a               ; DECREMENT TIMES TO REPEAT SOUND

  ld hl,(soundbuffera+11) ; RESET TONE FROM COPY
  ld (soundbuffera+6),hl

  ld a,(soundbuffera+13)  ; RESET VOLUME FROM COPY
  ld (soundbuffera+9),a   

  xor a
  ld (soundbuffera+4),a   ; RESET VOLUME FADE COUNT
  ld (soundbuffera+2),a   ; RESET TIMER

  ;-------------------------------------------------------------------
  ; PLAY ACTUAL SOUND
  setupsoundchannela:
  ld hl,soundbuffera+6 ; MOVE TO TONE LOWER
  
  xor a
  ld c,(hl)            ; TONE - LOWER 8 BITS - CHANNEL A
  call RegWrite
  
  inc hl
  ld a,1
  ld c,(hl)            ; TONE - UPPER 4 BITS
  call RegWrite

  inc hl
  ld a,6               ; SET NOISE
  ld c,(hl)            ; %---NNNNN 
  call RegWrite
  
  inc hl
  ld a,8               ; VOLUME / ENVELOPE FOR CHANNEL A ---EVVVV
  ld c,(hl)            ; E=1 MEANS ENVELOPE ON
  call RegWrite

  ld a,(soundbuffera)
  res 0,a              ; SIGNAL WE HAVE SET UP SOUND, DON'T NEED TO DO IT AGAIN
  set 3,a              ; SIGNAL CHANNEL IN USE
  ld (soundbuffera),a

  ; bit 3 ; CHANNEL ACTIVE
ret  
  
; CLEAR SOUND FROM BUFFER AS IT HAS FINISHED PLAYING
clearsound_b:
  ld (soundbufferb),a ;SET STATUS AS 0

  ld a,9            ; VOLUME / ENVELOPE FOR CHANNEL A ---EVVVV
  ld c,%00000000         ; E=1 MEANS ENVELOPE ON
  call RegWrite

  ld a,2
  ld c,%00000000         ; TONE - LOWER 8 BITS - CHANNEL B
  call RegWrite
  
  ld a,3
  ld c,%00000000         ; TONE - UPPER 4 BITS
  jp RegWrite

playsound_b:
  ld a,(soundbufferb)  ; NO CHANGE, JUST RETURN
  or a
  ret z  
  
  bit 5,a ; TONE STEP DOWN
  jr z,skiptonestepdownb
  
  ;-------------------------------------------------------------------
  ; ADJUST TONE DOWN BY STEP
  ld b,0
  ld a,(soundbufferb+5)  ; GET TONE STEP
  ld c,a
  ld hl,(soundbufferb+6) ; GET ACTUAL TONE
  sbc hl,bc              ; SUBRTRACT
  ld (soundbufferb+6),hl ; SAVE TO BUFFER
  ld a,(soundbufferb) 
  set 0,a                ; MAKE SURE WE REDO SOUND TONE
  ld (soundbufferb),a 
  skiptonestepdownb:
  
  bit 4,a ; TONE STEP UP
  jr z,skiptonestepupb

  ;-------------------------------------------------------------------
  ; ADJUST TONE UP BY STEP
  ld b,0
  ld a,(soundbufferb+5)  ; GET TONE STEP
  ld c,a
  ld hl,(soundbufferb+6) ; GET ACTUAL TONE
  add hl,bc              ; ADD TOGETHER
  ld (soundbufferb+6),hl ; SAVE TO BUFFER
  ld a,(soundbufferb) 
  set 0,a                ; MAKE SURE WE REDO SOUND TONE
  ld (soundbufferb),a 
  skiptonestepupb:

  bit 1,a
  jr z,skipchecktimerb

  ;-------------------------------------------------------------------
  ; TIMER - PLAYSOUND IF 0, ELSE INCREMENT UNTIL WE REACH TIMER
  ld hl,soundbufferb+1
  
  ld a,(hl)                 ; GET TIMER
  inc hl
  cp (hl)                   ; COMPARE TO CURRENT TIME
  jr z,check_clearsound_b   ; IF  SAME, SILENCE CHANNEL
  inc (hl)                  ; INC CURRENT TIME
  ld a,(soundbufferb)       ; GET STATUS IN A
  skipchecktimerb:

  bit 2,a 
  jr z,skipcheckvolfadeb

  ;-------------------------------------------------------------------
  ; VOLUME FADE - CHECK IF STEP COUNTER HAS REACHED MAX, IF SO, DEC VOLUME
  ld hl,soundbufferb+3

  ld a,(hl)                   ; GET VOLUME FADE STEPS
  inc hl                      ; MOVE TO CURRENT VOLUME
  cp (hl)                     ; COMPARE TO CURRENT VOLUME FADE
  jr nz,skipdovolumefadeb     ; IF WE MATCH STEP COUNT, DEC VOLUME

  ld (hl),0                   ; RESET VOLUME FADE
  ld hl,soundbufferb+9
  dec (hl)                    ; DEC VOLUME
  ld a,(hl)                   ; CHECK IF VOLUME IS 0
  or a
  jr z,check_clearsound_b
  
  ld a,(soundbufferb)         ; GET STATUS IN A
  bit 0,a                     ; BIT 0 SET - WE NEED TO CHANGE THE AY SOUND
  jr nz,setupsoundchannelb    ; SOUND IS FINISHED - RESET STATUS
  
  ; TONE UNCHANGED - JUST CHANGE VOLUME
  jr justchangevolumeb
      
  skipdovolumefadeb:
  inc (hl)                    ; INC CURRENT VOLUME FADE
  ld a,(soundbufferb)         ; GET STATUS IN A
  bit 0,a                     ; BIT 0 SET - WE NEED TO CHANGE THE AY SOUND
  jr nz,setupsoundchannelb    ; SOUND IS FINISHED - RESET STATUS
  
  justchangevolumeb:
  ; TONE UNCHANGED - JUST CHANGE VOLUME
  ld hl,soundbufferb+9 ; MOVE TO TONE LOWER

  ld a,9               ; VOLUME / ENVELOPE FOR CHANNEL A ---EVVVV
  ld c,(hl)            ; E=1 MEANS ENVELOPE ON
  jp RegWrite

  skipcheckvolfadeb:
  bit 0,a ; BIT 0 SET - WE NEED TO START THE AY SOUND
  jr nz,setupsoundchannelb
  ret
  
  ;-------------------------------------------------------------------
  ; CHECK IF WE NEED TO REPEAT SOUND, IF NOT, SILENCE CHANNEL
  check_clearsound_b:
  inc hl
  ld a,(hl)
  ;ld a,(soundbufferb+10)  ; CHECK IF WE NEED TO REPEAT SOUND
  or a
  jp z,clearsound_b       ; NO, JUST SILENCE SOUND
  
  dec a
  ld (hl),a;ld (soundbufferb+10),a  ; DECREMENT TIMES TO REPEAT SOUND

  ld hl,(soundbufferb+11) ; RESET TONE FROM COPY
  ld (soundbufferb+6),hl

  ld a,(soundbufferb+13)  ; RESET VOLUME FROM COPY
  ld (soundbufferb+9),a   

  xor a
  ld (soundbufferb+4),a   ; RESET VOLUME FADE COUNT
  ld (soundbufferb+2),a   ; RESET TIMER
  
  ;-------------------------------------------------------------------
  ; PLAY ACTUAL SOUND
  setupsoundchannelb:
  ld hl,soundbufferb+6 ; MOVE TO TONE LOWER
  
  ld a,2
  ld c,(hl)            ; TONE - LOWER 8 BITS - CHANNEL B
  call RegWrite
  
  inc hl
  ld a,3
  ld c,(hl)            ; TONE - UPPER 4 BITS
  call RegWrite

  inc hl
  ld a,6               ; SET NOISE
  ld c,(hl)            ; %---NNNNN 
  call RegWrite
  
  inc hl
  ld a,9               ; VOLUME / ENVELOPE FOR CHANNEL B ---EVVVV
  ld c,(hl)            ; E=1 MEANS ENVELOPE ON
  call RegWrite

  ld a,(soundbufferb)
  res 0,a              ; SIGNAL WE HAVE SET UP SOUND, DON'T NEED TO DO IT AGAIN
  set 3,a              ; SIGNAL CHANNEL IN USE
  ld (soundbufferb),a

  ; bit 3 ; CHANNEL ACTIVE
ret

; CLEAR SOUND FROM BUFFER AS IT HAS FINISHED PLAYING
clearsound_c:
  ld (soundbufferc),a   ; SET STATUS AS 0

  ld a,10                  ; VOLUME / ENVELOPE FOR CHANNEL A ---EVVVV
  ld c,%00000000           ; E=1 MEANS ENVELOPE ON
  call RegWrite
  
  ld a,4
  ld c,%00000000           ; TONE - LOWER 8 BITS - CHANNEL B
  call RegWrite
  
  ld a,5
  ld c,%00000000           ; TONE - UPPER 4 BITS
  jp RegWrite

playsound_c:
  ld a,(soundbufferc)  ; NO CHANGE, JUST RETURN
  or a
  ret z  
  
  bit 5,a ; TONE STEP DOWN
  jr z,skiptonestepdownc
  
  ;-------------------------------------------------------------------
  ; ADJUST TONE DOWN BY STEP
  ld b,0
  ld a,(soundbufferc+5)  ; GET TONE STEP
  ld c,a
  ld hl,(soundbufferc+6) ; GET ACTUAL TONE
  sbc hl,bc              ; SUBRTRACT
  ld (soundbufferc+6),hl ; SAVE TO BUFFER
  ld a,(soundbufferc) 
  set 0,a                ; MAKE SURE WE REDO SOUND TONE
  ld (soundbufferc),a 
  skiptonestepdownc:
  
  bit 4,a ; TONE STEP UP
  jr z,skiptonestepupc

  ;-------------------------------------------------------------------
  ; ADJUST TONE UP BY STEP
  ld b,0
  ld a,(soundbufferc+5)  ; GET TONE STEP
  ld c,a
  ld hl,(soundbufferc+6) ; GET ACTUAL TONE
  add hl,bc              ; ADD TOGETHER
  ld (soundbufferc+6),hl ; SAVE TO BUFFER
  ld a,(soundbufferc) 
  set 0,a                ; MAKE SURE WE REDO SOUND TONE
  ld (soundbufferc),a 
  skiptonestepupc:

  bit 1,a
  jr z,skipchecktimerc

  ;-------------------------------------------------------------------
  ; TIMER - PLAYSOUND IF 0, ELSE INCREMENT UNTIL WE REACH TIMER
  ld hl,soundbufferc+1
  
  ld a,(hl)                 ; GET TIMER
  inc hl
  cp (hl)                   ; COMPARE TO CURRENT TIME
  jr z,check_clearsound_c   ; IF  SAME, SILENCE CHANNEL
  inc (hl)                  ; INC CURRENT TIME
  ld a,(soundbufferc)       ; GET STATUS IN A
  skipchecktimerc:

  bit 2,a 
  jr z,skipcheckvolfadec

  ;-------------------------------------------------------------------
  ; VOLUME FADE - CHECK IF STEP COUNTER HAS REACHED MAX, IF SO, DEC VOLUME
  ld hl,soundbufferc+3

  ld a,(hl)                   ; GET VOLUME FADE STEPS
  inc hl                      ; MOVE TO CURRENT VOLUME
  cp (hl)                     ; COMPARE TO CURRENT VOLUME FADE
  jr nz,skipdovolumefadec     ; IF WE MATCH STEP COUNT, DEC VOLUME

  ld (hl),0                   ; RESET VOLUME FADE
  ld hl,soundbufferc+9
  dec (hl)                    ; DEC VOLUME
  ld a,(hl)                   ; CHECK IF VOLUME IS 0
  or a
  jr z,check_clearsound_c
  
  ld a,(soundbufferc)         ; GET STATUS IN A
  bit 0,a                     ; BIT 0 SET - WE NEED TO CHANGE THE AY SOUND
  jr nz,setupsoundchannelc    ; SOUND IS FINISHED - RESET STATUS
  
  ; TONE UNCHANGED - JUST CHANGE VOLUME
  jr justchangevolumec
      
  skipdovolumefadec:
  inc (hl)                    ; INC CURRENT VOLUME FADE
  ld a,(soundbufferc)         ; GET STATUS IN A
  bit 0,a                     ; BIT 0 SET - WE NEED TO CHANGE THE AY SOUND
  jr nz,setupsoundchannelc    ; SOUND IS FINISHED - RESET STATUS
  
  justchangevolumec:
  ; TONE UNCHANGED - JUST CHANGE VOLUME
  ld hl,soundbufferc+9 ; MOVE TO TONE LOWER

  ld a,10               ; VOLUME / ENVELOPE FOR CHANNEL A ---EVVVV
  ld c,(hl)            ; E=1 MEANS ENVELOPE ON
  jp RegWrite

  skipcheckvolfadec:
  bit 0,a ; BIT 0 SET - WE NEED TO START THE AY SOUND
  jr nz,setupsoundchannelc
  ret
  
  ;-------------------------------------------------------------------
  ; CHECK IF WE NEED TO REPEAT SOUND, IF NOT, SILENCE CHANNEL
  check_clearsound_c:
  inc hl
  ld a,(hl)               ; CHECK IF WE NEED TO REPEAT SOUND
  or a
  jp z,clearsound_c       ; NO, JUST SILENCE SOUND
  
  dec a
  ld (hl),a               ; DECREMENT TIMES TO REPEAT SOUND

  ld hl,(soundbufferc+11) ; RESET TONE FROM COPY
  ld (soundbufferc+6),hl

  ld a,(soundbufferc+13)  ; RESET VOLUME FROM COPY
  ld (soundbufferc+9),a   

  xor a
  ld (soundbufferc+4),a   ; RESET VOLUME FADE COUNT
  ld (soundbufferc+2),a   ; RESET TIMER
  
  ;-------------------------------------------------------------------
  ; PLAY ACTUAL SOUND
  setupsoundchannelc:
  ld hl,soundbufferc+6 ; MOVE TO TONE LOWER
  
  ld a,4
  ld c,(hl)            ; TONE - LOWER 8 BITS - CHANNEL C
  call RegWrite
  
  inc hl
  ld a,5
  ld c,(hl)            ; TONE - UPPER 4 BITS
  call RegWrite

  inc hl
  ld a,6               ; SET NOISE
  ld c,(hl)            ; %---NNNNN 
  call RegWrite
  
  inc hl
  ld a,10              ; VOLUME / ENVELOPE FOR CHANNEL C ---EVVVV
  ld c,(hl)            ; E=1 MEANS ENVELOPE ON
  call RegWrite

  ld a,(soundbufferc)
  res 0,a              ; SIGNAL WE HAVE SET UP SOUND, DON'T NEED TO DO IT AGAIN
  set 3,a              ; SIGNAL CHANNEL IN USE
  ld (soundbufferc),a

  ; bit 3 ; CHANNEL ACTIVE
ret

defb "CHRIS7"

; ------------------------------------------------------
; MUSIC
; CHANNEL A, B, C
; DEFINE INSTRUMENT, TONE, DELAY COUNT UNTIL NEXT SOUND
;defb 1,200,2,    1,120,3,     3,100,5
