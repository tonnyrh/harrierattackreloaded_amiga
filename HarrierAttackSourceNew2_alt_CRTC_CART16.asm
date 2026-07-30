; HARRIER ATTACK
; AMSOFT - DURELL - 1984

; CHANGES MADE TO ORIGINAL CODE
; JUMP TO START OF CODE AT &7A00 (OR &8000 FOR CRTC SPLIT SCREEN) - WAS DIRECT CALL &8111
; JOYSTICK BY DEFAULT
; FAST MEMORY COPY FOR SCROLL ON NON-CRTC VERSION
; CRTC SCROLL VERSION USES SPLIT SCREEN ON PLUS MACHINES

; IDEAS TO IMPLEMENT
; HARRIER GAME CODE AT          &0100-&3E40
;         SPLIT SCREEN ADDRESS  &4000-7FFF
;         MAIN SCREEN ADDRESS   &8000-BFFF
;         ASIC PAGED IN AT      &4000-7FFF
;         STACK AT              &C000-&C100
; MY GAME CODE AT               &C100-&CFE0

;debugscroll equ 1
;showsprites equ 1
;skiptolanding equ 1

;iscart equ 1
;iscasette equ 1


; -------------------------
; END OF DATA MARKERS
;            CART     DSK
;  CHRISA    3E50     3E30
;  CHRIS4    7F40     7F40
;  CHRIS9    D680     D680
;  CHRIS7    DA30     DA30

ifndef ISCART
  ; TEST OUR OWN ROUTINES IN THE MAIN PROGRAM
  read "AMSTRADFONT3.asm"
endif

  cartbank_addr       equ &C100
  scr_addr            equ &8000
  code_addr           equ &0100
  ; scr_set_mode - MAKE CUSTOM FUNCTION TO CLEAR SCREEN.
  ; ALSO NEED TO CHANGE edgelookuptable TO NEW SCREEN ADDRESS
  ;txt_output          equ &bb5a
  
; JUMP TABLES - FUNCTIONS
  
  writechar           equ cartbank_addr+3
  writeline           equ writechar+3
  writelinede         equ writeline+3
  writelineplainde    equ writelinede+3
  locatetext          equ writelineplainde+3
  locatetextde        equ locatetext+3
  writecursor         equ locatetextde+3
  writecharcursor     equ writecursor+3
  blinkcursor         equ writecharcursor+3
  writecharplain      equ blinkcursor+3
  
  initsound              equ writecharplain+3
  ;silencechannels        equ initsound+3
  dodingnoise            equ initsound+3
  domissilenoise         equ dodingnoise+3
  dobombnoise            equ domissilenoise+3
  doloudbombnoise        equ dobombnoise+3
  doflightnoise          equ doloudbombnoise+3
  doflaknoise            equ doflightnoise+3
  dofrigatenoise         equ doflaknoise+3
  doexplosionnoise       equ dofrigatenoise+3
  playsound_all_channels equ doexplosionnoise+3
  initmusic              equ playsound_all_channels+3
  playmusic              equ initmusic + 3
  playsound_a            equ playmusic + 3
  playsound_b            equ playsound_a + 3
  playsound_c            equ playsound_b + 3
  
; JUMP TABLES - STRINGS

  texttable           equ playsound_c+3
  titletext2          equ texttable
  skillleveltext2     equ titletext2+2
  joysticktext        equ skillleveltext2+2
  keyboardtext        equ joysticktext+2
  customtext          equ keyboardtext+2
  scoreboardheader2   equ customtext+2
  highscoretext2      equ scoreboardheader2+2
  speedtext           equ highscoretext2+2
  fueltext            equ speedtext+2
  rocketstext         equ fueltext+2
  bombstext           equ rocketstext+2
  skillleveltext3     equ bombstext+2
  redefinekeystext    equ skillleveltext3+2
  startgametext       equ redefinekeystext+2
  controlstext        equ startgametext+2
  inputmethod         equ controlstext+2
  tributetext         equ inputmethod+2
  rocketrange2        equ tributetext+2
  rocketrange3        equ rocketrange2+2
  trailstext2         equ rocketrange3+2
  trailstext3         equ trailstext2+2
  lockheighttext2     equ trailstext3+2
  armourtext          equ lockheighttext2+2
  wingmantext         equ armourtext+2
  spritelookuptable   equ wingmantext+2
  
;--------------------------------------------
; FUNCTIONS HERE COMPILE AFTER C100
  
  
;--------------------------------------------
; MAIN GAME CODE
 
; RUN GAME

; COMPILING FROM RASM DOESN'T FILL IN SPACE BEFORE FIRST ORG AUTOMATICALLY

; SPLIT SCREEN USES $4000-7FFF
org code_addr
;defs 255 ; STACK
jp start
defb "Harrier Attack Reloaded - CPSoft 27.07.2025"
nolist

;l84ea defb 0

;charcountvariable  defb 0
;rowcountervariable defb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
;l84fc defb 0,0,0,0,0,0,0,0,0,0,0
;l8507 defb 0,0
;l8509 defb 0,0
;l850b defb 0,0
;l850d defs 20
;l8521 defb 0,0,0,0,0
;l8526 defb 0,0,0,0,0,0
;l852c defb 0
;l852d defs 115
startobjecttilemap:        defb 0
startobjecttilemapplusone: defs 38
tilemap:                   defs 560
tilemapsea:                defs 41

; DATA MUST REMAIN IN THIS ORDER, AS WE CLEAR IT MANUALLY START OF NEW GAME
startofgamedata:
numberofhits:              defw 0
gamelevelprogress:         defb 0

; KEEP VARIABLES IN THIS ORDER SO WE CAN REUSE IT FOR WINGMAN BY ASSIGNING BLOCK TO IY
playermissileblock:
playermissilestatus:       defb 0
playermissileposition:     defw 0
playermissilecurrentrange: defb 0
; MY EXTRA VARIABLES FOR PLAYER
playermissilestatus2:       defb 0 ; NORMAL MISSILE = 0, MAVERICK = 1
playermaverickdirection:    defb 0 ; DIRECTION OF MISSILE, SO WE CAN KEEP MOVING IN SAME DIRECTION IF WE LOSE LOCK
playermavericklastposition: defw 0 ; LAST MAVERICK POSITION SO WE CAN ERASE TRAIL
currentplayerlocation:      defw 0
playerstartmissileposition: defw 0 ; NOT USED - ONLY NEEDED FOR WINGMAN
; KEEP VARIABLES IN THIS ORDER SO WE CAN REUSE IT FOR WINGMAN BY ASSIGNING BLOCK TO IY
bombmomentum:          defb 0
currentbomblocation:   defw 0
playerbombstatus:      defb 0

currenttime:               defw 0
wehavejected:              defb 0
parachutelocation:         defw 0
currentparachutetile:      defb 0

playerfueldrocketsbombs:   defb &0f        ; NUMBER OF BOMBS TO COUNT FOR EACH GAUGE LEVEL DECREMENT
flightmode: 
defb &10 ; FUEL UNITS?
defb &25 ; X LOC
defb &0f                      ; WHEN WE REACH ZERO ON ABOVE LEVEL, THIS BYTE GETS COPIED OVER TO IT
defb 4;5;6;&15 ; Y LOC
numberofrockets: defb 4       ; NUMBER OF BOMBS TO COUNT FOR EACH GAUGE LEVEL DECREMENT
rocketinventory: 
defb &10 ; ROCKET UNITS?
defb &11 ; X LOC?
l8835: 
defb 4                        ; WHEN WE REACH ZERO ON ABOVE LEVEL, THIS BYTE GETS COPIED OVER TO IT
defb 8;10;&18 ; Y LOC
numberofbombs: defb 4         ; NUMBER OF BOMBS TO COUNT FOR EACH GAUGE LEVEL DECREMENT
bombinventory: 
defb &10 ; BOMB UNITS?
defb &25 ; X LOC START AND END?
l883a: 
defb 4                        ; WHEN WE REACH ZERO ON ABOVE LEVEL, THIS BYTE GETS COPIED OVER TO IT
defb 8;10;&18 ; Y LOC
highscore:             defw 0
currscore:             defw 0
enemymissilestatus:    defb 0
enemymissilelocation:  defw 0

playerstatus:          defb 0
enemyplanestatus:      defb 0
enemyplanelocation:    defw 0

wingmanpowerupstatus:  defb 0
wingmanpoweruplocation: defw 0

playerspeed:           defb 0
leveldifficulty:       defb 1
l884b:                 defb 0
l884c:                 defb 0
spriteptr:             defw 0
enemyshiptimer:        defw 0
flakdamagecount:       defb 0                         ; FLAK DAMAGE COUNTER
totalflakdamagecount:  defb 100                       ; TOTAL FLAK DAMAGE COUNT WE CAN TAKE

playerfrigatestatus:   defb 0
currtime:              defb 9
l8859: defb 0
l885a: defb 0
l885b: defb 0
l885c: defb 0
l885d: defb 0
l885e: defb 0
l885f: defb 0
l8860: defb 1
l8861: defb 1
spriteblockptr: defw 0
l8864: defb 0
crashonlandingimage: defb 0

; MY EXTRA VARIABLES FOR SECOND HARRIER
wingmanbelowplayer:         defb 0 ; POSITION OF WINGMAN IN FORMATION
wingmantakeoff:             defb 0 ; 0 = ON CARRIER, 1 = TAKE OFF, 2 = LANDING WAYPOINT 1, 3 = LANDING WAYPOINT 2, 
                                   ; 4 = LANDED, 5 = TRACK ENEMY PLANE, 255 = KILLED, 254 = KILLED AND CLEARED FROM SCREEN
markplayernotlanded:        defb 0 ; RECORD IF PLAYER HAS LANDED, SO WE CAN DISABLE CONTROLS UNTIL WINGMAN LANDS
missiletargetwingman:       defb 0 ; 0 = TARGET NOT SELECTED, 1 = PLAYER 1, 2 = WINGMAN

; KEEP VARIABLES IN THIS ORDER SO WE CAN REUSE IT WITH FUNCTIONS FOR PLAYER BY ASSIGNING BLOCK TO IY
wingmanmissileblock:
wingmanmissilestatus:        defb 0
wingmanmissileposition:      defw 0
wingmanmissilecurrentrange:  defb 0
; MY EXTRA VARIABLES FOR WINGMAN
wingmanmissilestatus2:       defb 0 ; NORMAL MISSILE = 0, MAVERICK = 1
wingmanmaverickdirection:    defb 0 ; DIRECTION OF MISSILE, SO WE CAN KEEP MOVING IN SAME DIRECTION IF WE LOSE LOCK
wingmanmavericklastposition: defw 0 ; LAST MAVERICK POSITION SO WE CAN ERASE TRAIL
wingmanlocation:             defw 0 ; X Y POSITION
wingmanstartmissileposition: defw 0 ; START MISSILE POSITION, FOR CALCULATING SPEED OF MISSILE
; KEEP VARIABLES IN THIS ORDER SO WE CAN REUSE IT FOR WINGMAN BY ASSIGNING BLOCK TO IY
wingmanbombmomentum:         defb 0
wingmanbomblocation:         defw 0
wingmanbombstatus:           defb 0

endgamedata:

; GAME SETTINGS - DO NOT WIPE
wingmanon:                   defb 0  ; 0 = NORMAL, 1 = CPU, 2 = PLAYER 2

; WHY DOES BELOW CODE BREAK SCROLLING IN DSK VERSION?

;setupcarriersprite_parachutepart:
;  ; 6 - CARRIER BODY
;  ld hl,sprite_pixel_data10
;  ld de,&4600
;  ld bc,128
;  ldir
;  ex de,hl
;  ld bc,128
;  jp clearmem

setupcarriersprite:
  ; 6 - CARRIER BODY
  ld hl,sprite_pixel_data10
  ld de,&4600
  ld bc,128
  ldir
  ex de,hl
  ld bc,128
  call clearmem

  ; 7 - CARRIER BODY 2 (SAME) AS 1
  ld hl,sprite_pixel_data10
  ld de,&4700
  ld bc,128
  ldir
  ex de,hl
  ld bc,128
  call clearmem

  ; 8 - CARRIER BACK
  ld hl,sprite_pixel_data11
  ld de,&4800
  ld bc,128
  ldir
  ex de,hl
  ld bc,128
  call clearmem

  ; 9 - CARRIER FRONT
  ld hl,sprite_pixel_data12
  ld de,&4900
  ld bc,128
  ldir
  ex de,hl
  ld bc,128
  call clearmem

  ; 10 - CARRIER TOP
  ld hl,sprite_pixel_data13
  ld de,&4A00
  ld bc,256
  ldir

  ; 11 - CARRIER TOP
  ld hl,sprite_pixel_data14
  ld de,&4B00
  ld bc,256
  ldir
ret

setupsprites:

;;--------------------------------------------------
;; STEP 2 - Setup sprite pixel data
;;
;; The ASIC has internal "RAM" used to store the sprite pixel
;; data. If you want to change the pixel data for a sprite
;; then you need to copy new data into the internal "RAM".

; 0,1 - PLANE FRONT AND BACK
call settakeoffsprite

; 2 - LANDING PLANE FRONT
ld hl,sprite_pixel_data4
ld de,&4200
ld bc,128;&100
ldir
ex de,hl
ld bc,128
call clearmem

; 3 - LANDING PLANE BACK
ld hl,sprite_pixel_data3
ld de,&4300
ld bc,128;&100
ldir
ex de,hl
ld bc,128
call clearmem

; 4,5 - ENEMY PLANE FRONT AND BACK
call setenemyplanenormalsprite

call setupcarriersprite

; 12 - GUNSHIP LEFT
ld hl,sprite_pixel_data15
ld de,&4C00
ld bc,256
ldir

; 13 - GUNSHIP RIGHT
ld hl,sprite_pixel_data16
ld de,&4D00
ld bc,256
ldir

; 14 - 2ND HARRIER BACK
ld hl,sprite_pixel_data4
ld de,&4E00
ld bc,128
ldir
ex de,hl
ld bc,128
call clearmem

; 15 - 2ND HARRIER FRONT
ld hl,sprite_pixel_data3
ld de,&4F00
ld bc,128
ldir
ex de,hl
ld bc,128
call clearmem

numplussprites equ 16

;; page-out asic registers
;ld bc,&7fa0
;out (c),c

;;--------------------------------------------------
;; STEP 3 - Setup sprite palette
;;
;; The sprites use a single 15 entry sprite palette.
;; pen 0 is ALWAYS transparent.
;;
;; The sprite palette is different to the screen palette.

;; page-in asic registers to &4000-&7fff
;ld bc,&7fb8
;out (c),c

;; copy colours into ASIC sprite palette registers
ld hl,sprite_colours
ld de,&6422
ld bc,15*2
ldir

;; page-out asic registers
;ld bc,&7fa0
;out (c),c

;;--------------------------------------------------
;; STEP 4 - Setup sprite properties
;;
;; Each sprite has properties which define the x,y coordinates 
;; and x,y magnification.

;; page-in asic registers to &4000-&7fff
;ld bc,&7fb8
;out (c),c


;; set x coordinate for sprite 0
ld hl,0
ld (&6000),hl

;; set y coordinate for sprite 0
ld hl,160
ld (&6002),hl

;; set sprite x and y magnification
;; x magnification = 1
;; y magnification = 1
;ld a,%1001
xor a
ld (&6004),a

;;--------------------------------------------------
ret

hideallplussprites:
  ld b,numplussprites
  hideallplusspritesloop:
    call hideplussprite
  djnz hideallplusspritesloop
  jp hideplussprite

; INPUT
; B = SPRITE NUMBER
hideplussprite:
  push hl
  push af
  ld a,b

  ; GET CORRECT POSITION OF SPRITE NUMBER IN ASIC RAM
  rlca
  rlca
  rlca

  ;; set x coordinate for sprite 0
  ld h,&60
  ld l,a
  ;ld (hl),e

  inc l
  inc l
  ;; set y coordinate for sprite 0
  ;ld (hl),d

  inc l
  inc l
  ;; set sprite x and y magnification
  ;; x magnification = 1
  ;; y magnification = 1
  ;ld a,%1001
  ld (hl),0 ; HIDE SPRITE
  pop af
  pop hl
ret

; INPUT
; A = SPRITE NUMBER
; DE = X PIXEL POSITION
; B  = Y PIXEL POS
moveplussprite2_pixel:  
  push de
  ; GET CORRECT POSITION OF SPRITE NUMBER IN ASIC RAM
  rlca
  rlca
  rlca
  
  ;; set x coordinate for sprite 0
  ld h,&60
  ld l,a
  ld (hl),e
  inc l
  ld (hl),d

  inc l
  ;; set y coordinate for sprite 0
  ld (hl),b

  inc l
  inc l
  ;; set sprite x and y magnification
  ;; x magnification = 1
  ;; y magnification = 1
  ld a,%1001
  ld (hl),a
  pop de
ret

; INPUT
; A = SPRITE NUMBER
; DE = X PIXEL POSITION
; B  = Y PIXEL POS
moveplussprite2_pixel_mode0:  
  push de
  ; GET CORRECT POSITION OF SPRITE NUMBER IN ASIC RAM
  rlca
  rlca
  rlca

  ;; set x coordinate for sprite 0
  ld h,&60
  ld l,a
  ld (hl),e
  inc l
  ld (hl),d

  inc l
  ;; set y coordinate for sprite 0
  ld (hl),b

  inc l
  inc l
  ;; set sprite x and y magnification
  ;; x magnification = 1
  ;; y magnification = 1
  ld a,%1101
  ld (hl),a
  pop de
ret

; INPUT
; A = SPRITE NUMBER
; HL = Y X POSITION
moveplussprite2:  
  push hl
  ; GET CORRECT POSITION OF SPRITE NUMBER IN ASIC RAM
  rlca
  rlca
  rlca

  ld b,h     ; STORE HEIGHT IN B

  ld h,0     ; FIND X POS
  add hl,hl  ; DOUBLE
  add hl,hl
  add hl,hl  ; DOUBLE AGAIN TO GET X PIXEL COORDINATE
  add hl,hl
  
  ex de,hl
  ;; set x coordinate for sprite 0
  ld h,&60
  ld l,a
  ld (hl),e
  inc l
  ld (hl),d

  inc l
  ;; set y coordinate for sprite 0
  ld a,b 
  rlca      ; TRIPLE H TO GET Y POS LINE
  rlca
  rlca
  ld (hl),a

  inc l
  inc l
  ;; set sprite x and y magnification
  ;; x magnification = 1
  ;; y magnification = 1
  ld a,%1001
  ld (hl),a
  pop hl
ret

; MOVE MODE 0 RESOLUTION SPRITE
moveplussprite_mode0:  
  push hl
  ; GET CORRECT POSITION OF SPRITE NUMBER IN ASIC RAM
  rlca
  rlca
  rlca

  ld b,h     ; STORE HEIGHT IN B

  ld h,0     ; FIND X POS
  add hl,hl  ; DOUBLE
  add hl,hl
  add hl,hl  ; DOUBLE AGAIN TO GET X PIXEL COORDINATE
  add hl,hl
  
  ex de,hl
  ;; set x coordinate for sprite 0
  ld h,&60
  ld l,a
  ld (hl),e
  inc l
  ld (hl),d

  inc hl
  ;; set y coordinate for sprite 0
  ld a,b 
  rlca      ; TRIPLE H TO GET Y POS LINE
  rlca
  rlca
  ld (hl),a

  inc l
  inc l
  ;; set sprite x and y magnification
  ;; x magnification = 1
  ;; y magnification = 1
  ld a,%1101
  ld (hl),a
  pop hl
ret

scroll_right:
  ;; get current scroll offset
  ld hl,(scroll_offset)

  ;; update it
  inc hl

  ;; ensure scroll offset is in range &000-&3ff
  ld a,h
  and &3
  ld h,a

  ;; store new scroll offset. It is now ready to be written to the CRTC.
  ld (scroll_offset),hl

  ; UPDATE SCREEN OFFSETS FOR SPRITES
  ld bc,(scroll_adjustmenty)
  dec bc
  ld (scroll_adjustmenty),bc

  ; CHECK IF WE GOT TO END OF LOOP
  ld a,b
  or a
  jr nz,skipresetedgelookuptable
  ld a,c
  or a
  jr nz,skipresetedgelookuptable

  ; WE REACHED #00 FULL WRAP
  jp resetedgelookuptable

  skipresetedgelookuptable:

  ; UPDATE SCREEN OFFSETS FOR SPRITES
  ld a,(scroll_adjustmentx)
  inc a
  cp 40
  jr z,adjustdownrow
  ld (scroll_adjustmentx),a
ret

reset_scrolladjustments:
  ld bc,1024
  ld (scroll_adjustmenty),bc

  ; RESET GFX POS
  ld hl,edgelookuptableorig
  ld de,edgelookuptable
  ld bc,50;25 ; DOUBLE BYTE COPY
  ldir

  xor a
  ld (scroll_adjustmentx),a
  ;ld hl,&0000
  ld (scroll_offset),bc

  ld hl,0
  jp scr_set_offset

; EVERY TIME THE SCREEN SCROLLS A NEW LINE WE NEED TO ADJUST THE COORDINATES
; SO THAT WHEN WE PRINT NEW SPRITES AT THE RIGHT SIDE OF THE SCREEN THEY DON'T DRIFT UPWARD

adjustdownrow:
  ; RESET X OFFSET
  xor a
  ld (scroll_adjustmentx),a

  ld bc,(edgelookuptable)
  ld hl,edgelookuptable+2
  ld de,edgelookuptable
  call useLDI40
  call useLDI8         

  ld (edgelookuptable+48),bc  ; we need to move up line to compensate
ret
	
resetedgelookuptable:
  call reset_scrolladjustments
  ld bc,1024
  ld (scroll_adjustmenty),bc
ret

;;--------------------------------------------------------------------------------------------

; INPUTS
; HL = OFFSET TO SET TO
scr_set_offset:
  ld (scroll_offset),hl
  ld bc,&bc03			;; select CRTC register 3 (horizontal sync width)
  out (c),c
  inc b
  out (c),a			;; set vertical and horizontal sync widths
  ;; write scroll offset (in CRTC character width units)
  ld bc,&bc0c				;; select CRTC register 12
  out (c),c
  ld b,&bd				;; B = I/O address for CRTC register write
  ;; combine with scroll base
  ld a,(scroll_base)
  or h
  out (c),a

  ld bc,&bc0d				;; select CRTC register 13
  out (c),c
  ld b,&bd				;; B = I/O address for CRTC register write
  out (c),l
ret

update_scroll:
;; write scroll adjustment
ld a,(scroll_adjustment)

add &f6;5			;; This value alternates between &F5 and &F6
					;; and controls the horizontal position of the visible
					;; area on the monitor display. The effect these
					;; values have on the position relies on the reaction
					;; of the monitor to the horizontal sync output of
					;; the CRTC. So for some monitors this may not result
					;; in a smooth scroll as we want. This value may need
					;; adjustment for your monitor and CRTC variant.
					;;
					;; From BASIC try the following two lines to see the
					;; screen move, which is the basis of this effect:
					;;
					;; OUT &BC00,3:OUT &BD00,&F5 and
					;; OUT &BC00,3:OUT &BD00,&F6 

ld bc,&bc03			;; select CRTC register 3 (horizontal sync width)
out (c),c
inc b
out (c),a			;; set vertical and horizontal sync widths

ld hl,(scroll_offset)

;; write scroll offset (in CRTC character width units)
ld bc,&bc0c				;; select CRTC register 12
out (c),c
ld b,&bd				;; B = I/O address for CRTC register write

;; combine with scroll base
ld a,(scroll_base)
or h
out (c),a

ld bc,&bc0d				;; select CRTC register 13
out (c),c
ld b,&bd				;; B = I/O address for CRTC register write
out (c),l
ret

;;----------------------------------------------------------------------
;; scroll parameters

scroll_adjustment:  defb 0
scroll_adjustmentx: defb 0
scroll_adjustmenty: defw 1024 ; THREE WRAPS (25 x 3) RETURN US BACK TO NORMAL SCREEN

;; high byte of the screen base address
;; &00 -> screen uses &0000-&3fff
;; &10 -> screen uses &4000-&7fff
;; &20 -> screen uses &8000-&bfff
;; &30 -> screen uses &c000-&ffff

;ifdef iscart
scroll_base: defb &20; &30
;endif
;ifndef iscart
;scroll_base: defb &30
;endif

;; the scroll offset in CRTC character units
scroll_offset: defw 0;&0160 ;0

; DRAW SCREEN
drawinstrumentpanel:
  ld de,0
  call scr_set_border
  call setasicpalettemenu
  call splitscreen

  ; DRAW INSTRUMENTS
  ld bc,&7fa0 ; PAGE OUT PLUS REGISTERS
  out (c),c
  
  ld hl,&40FE;0307
  ;call convspritelocationtopixelssplittable
  ld de,(armourtext)
  call writelinede 
  
  ld hl,&40A6;0203
  ;call convspritelocationtopixelssplittable
  ld de,(highscoretext2)
  call writelinede

  ld hl,&414F;0407
  ;call convspritelocationtopixelssplittable
  ld de,(speedtext)
  ;inc l ; MOVE BYTE RIGHT
  call writelinede

  ld hl,&4178;041C
  ;call convspritelocationtopixelssplittable
  ld de,(fueltext)
  call writelinede

  ld hl,&423D;0706
  ;call convspritelocationtopixelssplittable
  ld de,(rocketstext)
  ;inc l ; MOVE BYTE RIGHT
  call writelinede

  ld hl,&4267;071B
  ;call convspritelocationtopixelssplittable
  ld de,(bombstext)
  ;inc l ; MOVE BYTE RIGHT
  call writelinede

  ld hl,&0802    ; HL = Y X LOCATION
  call drawgauge ; SPEED
  ld hl,&0816
  call drawgauge ; FUEL
  ld hl,&0502
  call drawgauge ; ROCKETS
  ld hl,&0516
  call drawgauge ; BOMBS
  ; FIX SPEED TO ZERO
  ld hl,&0502
  ld a,8;&08     ; SPRITE NUMBER
  call drawspritesplitscreen
  ld hl,&0511
  ld a,7;&07     ; SPRITE NUMBER
  call drawspritesplitscreen
  
  ; CLEAR TILEMAP WITH 1s. OPEN SKY
  ld hl,startobjecttilemap
  ld bc,600;&0258
  filltilemapwithskyobject:
    ld (hl),1
    inc hl
    dec bc
    ld a,b
    or c
  jr nz,filltilemapwithskyobject
  
  ; CLEAR TILEMAP WITH 2s. SEA
  ld b,40
  filltilemapwithseaobject:
    ld (hl),2
    inc hl
  djnz filltilemapwithseaobject
  
  ld hl,0
  ld (enemyshiptimer),hl
  ; gamelevelprogress... a =
  ;  0 - START
  ;  1 - START ENEMY SHIP
  ;  2 - ENEMY SHIP FIRED MISSILE
  ;  3 - DO LAND
  ;  4 - DISPLAY ENEMY
  ;  5 - DESCEND MOUNTAINS DOWN TO TOWN LEVEL
  ;  6 - DO FLAT TOWNLAND
  ;  7 - GENERATE BUILDING
  ;  8 - START OF PIER
  ;  9 - END OF PIER
  ; 10 - ENEMY SHIP FIRED MISSILE
  ; 11 - START FRIGATE
  ; 12 - END FRIGATE
  ; 13 - LANDING ON FRIGATE 
  ; 14 - OPEN SEA
  ; 15 -
ifdef skiptolanding
  ld a,10                          ; RESET PROGRESS IN LEVEL SCROLL
endif
ifndef skiptolanding
  xor a                            ; RESET PROGRESS IN LEVEL SCROLL
endif
  ld (gamelevelprogress),a

  ld hl,(currscore)
  call convwordtostr
  ld hl,&40B4;020a
  ;call convspritelocationtopixelssplittable
  ld de,wordtostr
  call writelinede

  ld hl,(highscore)
  call convwordtostr
  ld hl,&40DC;021e
  ;call convspritelocationtopixelssplittable
  ld de,wordtostr
  call writelinede
  
  ld bc,&7fb8 ; PAGE IN PLUS REGISTERS
  out (c),c
ret


;updatescreenloop:
;  call setupscreen
;  call controlplanemovement
;  call checkfireplayermissile
;  call checkerasebombtrail
;  call checklaunchbomb
;  call checkejectorseat
;  call checkenemyfighterapproach
;  jp checkplayerplaneexplosion

checklaunchbombwingmanpl2:
  ld a,(iy+15);(playerbombstatus)   ; ARE WE FIRING SOMETHING ALREADY
  or a
  jr nz,decreasebombmomentum

  ld a,(wingmantakeoff)             ; ARE WE STILL ALIVE?
  cp 1
  ret nz
  call testkeyspace2                ; DID PLAYER PRESS SPACE?
  ret nz
  jp dolaunchbomb                   ; WINGMAN HAS INFINITE BOMBS AS HE IS A POWERUP
 
checklaunchbombwingman:
  ld a,(wingmanon)                  ; CHECK IF WINGMAN MODE IS ENABLED
  or a
  ret z
  cp 2
  jr z,checklaunchbombwingmanpl2
  
  ld a,(iy+15);(playerbombstatus)   ; ARE WE FIRING SOMETHING ALREADY
  or a
  jr nz,decreasebombmomentum
  
  ; IS WINGMAN STATUS SET TO LAUNCH BOMB?
  ld a,(wingmantakeoff)
  cp 11
  ret nz ; NO, JUST RETURN
  
  ; BOMB DROPPED
  call dolaunchbomb
  jp resetwingmanflight

checklaunchbomb:
  ld a,(iy+15);(playerbombstatus)   ; ARE WE FIRING SOMETHING ALREADY
  or a
  jr nz,decreasebombmomentum

  ld a,(playerstatus)               ; IS PLAYER STILL DOING SOMETHING ELSE?
  or a
  ret nz

  inc a                             ; DO WE HAVE A BOMB TO LAUNCH?
  ld hl,bombinventory
  cp (hl) 
  ret z
  
  ld a,(playerfrigatestatus)        ; IS OUR FRIGATE NOT BOMBED?
  or a
  jr nz,launchbombifspacepressed

  ld a,(gamelevelprogress)
  cp 11                             ; START OF FRIGATE - DISABLE BOMBS ON APPROACH TO LANDING?
  ret nc 

  launchbombifspacepressed:
  call testkeyspace                 ; DID PLAYER PRESS SPACE?
  ret nz
  
  ; LAUNCH BOMB
  ld ix,numberofbombs
  call decrementgaugelevelcheckredraw
  dolaunchbomb:                     ; WINGMAN HAS INFINITE BOMBS AS HE IS A POWERUP
  l7b91:
  ld a,1
  ld (iy+15),a;(playerbombstatus),a
  ld l,(iy+8);,a(currentplayerlocation)
  ld h,(iy+9)
  ;ld (currentbomblocation),hl
  ld (iy+13),l
  ld (iy+14),h
  ld a,3 ; SET MOMENTUM OF BOMB
  ld (iy+12),a;(bombmomentum),a
  jr movebombmomentum

  decreasebombmomentum:
  dec a
  jr nz,decreasebombheight
  dec (iy+12)
  ld a,(iy+12)
  ;ld hl,(iy+12),a;bombmomentum
  ;dec (hl)
  jr nz,movebombmomentum
  ld a,2

  ld (iy+15),a;(playerbombstatus),a

  movebombmomentum:
  ld l,(iy+13)
  ld h,(iy+14)
  ;ld hl,(currentbomblocation)
  inc h
  ld c,40;&28
  ;ld (currentbomblocation),hl
  ld (iy+13),l
  ld (iy+14),h
  call checkenemyhit
  ret nz
  ld (iy+15),a;(playerbombstatus),a
  ret

  decreasebombheight:
  ;ld hl,(currentbomblocation)
  ld l,(iy+13)
  ld h,(iy+14)
  dec l
  jr nz,checkerasebombtraildescent
  xor a
  ld (iy+15),a;(playerbombstatus),a
ret

checkerasebombtraildescent:
  push hl
  xor a;ld a,79;xor a
  ld c,1 ; SKY OBJECT ID
  call drawspritecheckifsky
  pop hl
  inc h
  ld c,41;&29
  ;ld (currentbomblocation),hl
  ld (iy+13),l
  ld (iy+14),h
  call checkenemyhit
  ret nz
  ld (iy+15),a;(playerbombstatus),a
ret

checkerasebombtrail:
  ld a,(iy+15);(playerbombstatus)
  dec a ; cp 1                ; HAVE WE BEEN KILLED?
  ret nz
  ; DRAW SKY
  xor a
  ld c,1
  ;ld hl,(currentbomblocation)
  ld l,(iy+13)
  ld h,(iy+14)
  dec l
  jp drawspritecheckifsky
  
; --------------------------------------------------
; EJECT

checkejectorseat:
  ld a,(wehavejected)
  or a
  jr nz,moveparachute         ; CHECK IF WE HAVE EJECTED ALREADY

  ld a,(playerstatus)         ; IS PLAYER STILL ALIVE
  or a
  ret nz

  call testkeyesc
  ret nz

  ; PLAYER PRESSED ESCAPE
  ; EJECT FROM PLANE
  ld a,1
  ld (wehavejected),a
  ld hl,(currentplayerlocation)
  dec h
  ld (parachutelocation),hl
  ld a,47; &2f DISPLAY EJECTOR SEAT TILE
  ld (currentparachutetile),a
  ret

  moveparachute:
  dec a
  jr nz,l7c51
  ld hl,(parachutelocation)
  ld a,(gamelevelprogress)
  cp 13
  call z,drawtilesky
  dec l
  ld (parachutelocation),hl
  call getskytilemapid
  ld a,(de)
  or a
  jr z,drawparachute
  dec a
  jr z,drawparachute
  ld a,3
  ld (wehavejected),a
  ret

  drawparachute:
  ld hl,currentparachutetile
  ld a,(hl)
  inc (hl)
  cp 50          ; HAVE WE REACHED CHUTE RIGHT TILE
  jr nz,l7c48
  ld a,2
  ld (wehavejected),a
  ld a,50        ; DISPLAY CHUTE RIGHT TILE
  l7c48:
  ld c,1 ; SKY OBJECT ID
  ld hl,(parachutelocation)
  jp drawspritecheckifsky

l7c51:
  dec a
  ret nz
  ld hl,(parachutelocation)
  dec l
  inc l
  jr nz,l7c68
  ld a,4
  ld (wehavejected),a
  ld a,(gamelevelprogress)
  cp 13                     ; LANDING ON FRIGATE
  jp z,drawtilesky
  ret

  l7c68:
  ld a,(gamelevelprogress)
  cp 13                     ; LANDED ON FRIGATE
  jr z,l7c70
  dec l                     ; ERASE TRAIL OF PARACHUTE
  l7c70:
  push hl
  xor a
  ld c,1                    ; SKY OBJECT ID
  call drawspritecheckifsky ; ERASE TRAIL OF PARACHUTE
  pop hl
  inc h
  ld a,(gamelevelprogress)
  cp 13                     ; IF LANDING ON FRIGATE
  jr nz,l7c81
  dec l      
  l7c81:
  ld (parachutelocation),hl
  call getskytilemapid
  ld a,(de)
  or a       ; CP 0
  jr z,drawtilechutestraight ; DISPLAY CHUTE STRAIGHT TILE
  dec a      ; CP 1
  jr z,drawtilechutestraight ; DISPLAY CHUTE STRAIGHT TILE
  ld a,4
  ld (wehavejected),a
ret

drawtilechutestraight:
  ld a,49 ; DISPLAY CHUTE STRAIGHT TILE
  ld c,1  ; SKY OBJECT ID
  jp drawspritecheckifsky

drawtilesky:
  xor a
  ld c,1 ; SKY OBJECT ID
  push hl
  call drawspritecheckifsky
  pop hl
ret

status_enemyplanehit equ 3

checkenemyfighterapproach:
  ld a,(playerstatus)       ; IS PLAYER STILL ALIVE
  or a
  ret nz

  ld a,(enemyplanestatus)   ; STATUS - 0 = NOT ON SCREEN, 1 = ON APPROACH, 2 = FIRED MISSILE, 3 = PLANE HIT, 4 = PLANE DESTROYED
  or a
  jr z,checkifplayerplanedestroyedbyenemymissile
  cp 3
  jr z,checkifplayerplanedestroyedbyenemymissile

  ld a,(iy+15);(playerbombstatus)
  or a
  jr z,checkifenemyplanedestroyedbymissile

  ;ld hl,(currentbomblocation)
  ld l,(iy+8)
  ld h,(iy+9)
  ld de,(enemyplanelocation)
  call checkiftwocoordinatescollide
  jr nz,checkifenemyplanedestroyedbymissile
  ; ENEMY PLANE DESTROYED BY BOMB
  call drawtilesky
  ld a,status_enemyplanehit
  ld (enemyplanestatus),a
  xor a
  ld (iy+15),a;(playerbombstatus),a
  ld a,75   ; SCORE
  call explosionnoise
  jr checkifenemyplanedestroyedbyplayerplane

  checkifenemyplanedestroyedbymissile:
  ld a,(iy+0);(playermissilestatus)
  or a
  jr z,checkifenemyplanedestroyedbyplayerplane
  
  ld l,(iy+1);(playermissileposition)
  ld h,(iy+2)
  ld de,(enemyplanelocation)
  call checkiftwocoordinatescollide
  jr nz,checkifenemyplanedestroyedbyplayerplane
  ; ENEMY PLANE DESTROYED BY MISSILE
  call drawtilesky
  ld a,status_enemyplanehit
  ld (enemyplanestatus),a
  ;xor a
  ld (iy+0),0;a;(playermissilestatus),a
  ld a,75   ; SCORE
  call explosionnoise
  jr checkifplayerplanedestroyedbyenemymissile

  checkifenemyplanedestroyedbyplayerplane:
  ld hl,(enemyplanelocation)
  ld de,(currentplayerlocation)
  ld a,d
  cp h
  jr nz,checkifplayerplanedestroyedbyenemymissile
  ld a,e
  inc a
  sub l
  jr c,checkifplayerplanedestroyedbyenemymissile
  cp 3
  jr nc,checkifplayerplanedestroyedbyenemymissile
  call checkplayerplanemovement
  ld a,status_enemyplanehit
  ld (enemyplanestatus),a
  ld a,1:deathstatus2              ; PLAYER KILLED
  ld (playerstatus),a
  jp doexplosionnoise

  checkifplayerplanedestroyedbyenemymissile:
  ld a,(enemymissilestatus)
  or a
  ret z
  ld hl,(enemymissilelocation)
  ld de,(currentplayerlocation)
  call checkiftwocoordinatescollide
  ret nz
  call checkplayerplanemovement
  xor a
  ld (enemymissilestatus),a
  ld a,1:deathstatus3                  ; PLAYER KILLED
  ld (playerstatus),a                  ; HIT
  jp doexplosionnoise

; CHECK IF TWO COORDINATES ARE THE SAME,
; OR IF THEY ARE THE SAME AFTER THE NEXT SCREEN SCROLL
; INPUT
; HL = PLAYER LOCATION
; DE = ENEMY LOCATION
; OUTPUT
checkiftwocoordinatescollide:
  ld a,h
  cp d
  ret nz
  ld a,l
  cp e
  ret z
  dec a
  cp e 
ret

checkplayerplaneexplosion:
  ld a,(playerstatus)         ; IS PLAYER STILL ALIVE
  or a
  ret z

  dec a
  jp nz,planedestructionsequence

  ; WE HAVE CRASHED 
  ld b,0
  call hideplussprite
  inc b
  call hideplussprite

  ld a,(wehavejected)
  dec a
  jr nz,displayplanebrokeapart
  ld hl,(currscore)
  ld de,&0064
  add hl,de
  ld (currscore),hl
  call convwordtostr
  
  ld bc,&7fa0 ; PAGE OUT PLUS REGISTERS
  out (c),c
  
  ld hl,&40b4;020a
  ;call convspritelocationtopixelssplittable
  ld de,wordtostr
  call writelinede
  
  ld bc,&7fb8 ; PAGE IN PLUS REGISTERS
  out (c),c

  displayplanebrokeapart:
  ld hl,(currentplayerlocation)
  ld a,h
  cp #0f
  jr c,l7d73
  dec h 
  l7d73:
  push hl 
  ld de,planebrokepart1data
  ld a,h
  cp 10
  jr nc,l7d86 
  ld de,planebrokepart3data
  cp 5
  jr c,l7d86
  ld de,planebrokepart2data
  l7d86:
  ld a,2                     ; PLAYER STATUS SMASHED
  ld (playerstatus),a
  ld hl,planesmashedpart1location
  ex de,hl
  ld bc,&0015
  ldir
  pop hl
  inc l
  ld (planesmashedpart1location),hl              ; RECORD START LOCATION OF PLANE PARTS
  ld (planesmashedpart2location),hl              ; RECORD START LOCATION OF PLANE PARTS
  ld (planesmashedpart3location),hl              ; RECORD START LOCATION OF PLANE PARTS
  dec l
  ld a,67     ; DISPLAY PLANE BROKE TILE 1
  ld c,1      ; SKY OBJECT ID
  push hl
  call drawspritecheckifsky
  pop hl
  inc l
  call getskytilemapid
  ld a,(de)
  cp 12
  jr nz,planedestructionsequence
  xor a         ; DISPLAY SKY TILE
  ld c,1        ; SKY OBJECT ID
  call drawspritecheckifsky
  
  planedestructionsequence:
  ld b,3
  ld ix,planesmashedpart1location
  playerplanedestructionloop:
    push bc
    bit 0,(ix+5)
    jr z,l7e1c
    ld l,(ix+0)
    ld h,(ix+1)
    dec l
    push hl
    xor a                     ; DISPLAY SKY TILE
    ld c,1                    ; SKY OBJECT ID
    call drawspritecheckifsky ; ERASE PLAYER PLANE DESTRUCTION TRAIL
    pop hl
    dec (ix+6)
    jr z,l7e18
    ld a,(ix+3)
    add h
    ld h,a
    inc a
    jr z,l7e18
    cp 17 
    jr nc,l7e18
    ld a,(ix+#04)
    add l
    ld l,a
    cp 40
    jr nc,l7e18
    call getskytilemapid
    ld a,(de)
    or a
    jr z,l7dfc
    dec a
    jr z,l7dfc
    cp 9
    jr nz,l7e18
    l7dfc:
    inc (ix+2)
    ld a,(ix+2)
    cp 70 
    jr nz,l7e0b
    ld a,67                    ; PLANE BROKE 1
    ld (ix+2),a
    l7e0b:
    ld (ix+0),l
    ld (ix+1),h
    ld c,1                     ; SKY OBJECT ID
    call drawspritecheckifsky  ; DRAW PLANE BROKE
    jr l7e1c
   
    l7e18:
    ld (ix+5),0
    l7e1c:
    pop bc
    ld de,&0007
    add ix,de
  djnz playerplanedestructionloop

  ld ix,planesmashedpart1location  
  ld a,(ix+5)
  or (ix+12)
  or (ix+19)
  ret nz
  ld a,3                      ; PLAYER STATUS DIED
  ld (playerstatus),a
  ld bc,&0bb8                 ; FLASH BORDER
  borderflashloop:
    push bc
    call strangedelay
    ld a,r                    ; RANDOM BORDER COLOUR
    ld d,a
    ld e,a
    call scr_set_border
    pop bc
    dec bc
    ld a,c
    or b
  jr nz,borderflashloop
ret
 
; DATA
planebrokepart1data:
  defb &00,&00,&43,&ff,&01,&01,&0a,&00
  defb &00,&44,&00,&02,&01,&0f,&00,&00
  defb &45,&ff,&02,&01,&14

; DATA
planebrokepart2data:
  defb &00,&00,&43,&01,&02,&01,&0e,&00
  defb &00,&44,&00,&01,&01,&19,&00,&00
  defb &45,&ff,&01,&01,&07

; DATA
planebrokepart3data:
  defb &00,&00,&43,&00,&02,&01,&0d,&00
  defb &00,&44,&01,&02,&01,&14,&00,&00
  defb &45,&02,&01,&01,&09

; DATA
planesmashedpart1location: 
  defb 0,0  ; Y X LOCATION
  defb 0,0,0,0,0

; DATA
planesmashedpart2location: 
  defb 0,0  ; Y X LOCATION
  defb 0,0,0,0,0

; DATA
planesmashedpart3location:
  defb 0,0  ; Y X LOCATION
  defb 0,0,0,0,0

checkwingmanlanded:
  ; IS WINGMAN BEING CONTROLLED BY PLAYER 2
  ld a,(wingmanon)
  cp 2
  ret nz
  ; IS WINGMAN STILL FLYING AROUND
  ld a,(wingmantakeoff)
  cp 2
  ret nz
  
  ; IF MANUAL FLIGHT, CHECK HE LANDED RIGHT
  ld hl,(wingmanlocation)

  ; CHECK VERTICAL
  ld a,h 
  cp #0d
  ret nz 
 
  ; CHECK HORIZONTAL
  ld a,l
  sub 21
  ret c
  
  ld a,l
  sub 31
  ret nc
  
  ; WINGMAN HAS LANDED
  jp setwingmanlanded

landinghoverloop:
  call timedelay
  call timercountdown
  call checkejectorseat
  call flightnoise
  
  ; MY LANDING HOVER LOOP FUNCTIONS
  call controlwingmanfunc
  call checkwingmanlanded
  call erasewingmanwakelanding
  call drawwingmanplane
  
  ld a,(flightmode)
  ld hl,(currentplayerlocation)
  dec a
  jr nz,spritemovementlanding
  launchejectorseat:
  inc h
  ld (currentplayerlocation),hl
  dec h
  jr clearplanetraillanding

spritemovementlanding:
  ld a,(wehavejected)
  or a
  jr nz,launchejectorseat

  push hl
  call timedelay
  call checkejectorseat
  pop de
  
  ; DISABLE CONTROLS IF PLAYER LANDED - NEEDED AS WINGMAN MAY STILL BE TRYING TO LAND
  ld a,(markplayernotlanded)
  cp 1
  jr z,skipchecklanding       ; PLAYER NOT LANDED YET
  
  ld a,(wingmantakeoff)
  or a
  jp z,skipwingmanlandedcheck ; WINGMAN LANDED TOO
  cp 254
  jp z,skipwingmanlandedcheck ; WINGMAN HAS DIED, DON'T WAIT FOR HIM
  jr landinghoverloop         ; PLAYER LANDED, WINGMAN NOT LANDED
  
  skipchecklanding:
  
  call testkeyright
  jr nz,notpressedright
  inc e                  ; MOVE RIGHT
  notpressedright:
  call testkeyleft
  jr nz,notpressedleft
  dec e                  ; MOVE LEFT
  notpressedleft:
  call testkeyup
  jr nz,notpressedup
  dec d                  ; MOVE UP
  notpressedup:
  call testkeydown
  jr nz,notpresseddown
  inc d                  ; MOVE DOWN
  notpresseddown:
  ld a,e
  or a
  jr nz,l7ef5            
  inc e                  ; DO NOT GO PAST LEFT SIDE OF SCREEN?
  l7ef5:
  cp 38
  jr nz,l7efa
  dec e                  ; DO NOT GO PAST RIGHT SIDE OF SCREEN?
  l7efa:
  ld a,d
  or a                   
  jr nz,l7eff
  inc d                  ; DO NOT GO PAST TOP SIDE OF SCREEN?

  ; UPDATE PLAYER LOCATION VARIABLE
  l7eff:
  ld hl,(currentplayerlocation)
  ld (currentplayerlocation),de
  push hl
  or a
  sbc hl,de
  pop hl
  jr z,landinghoverloop

  clearplanetraillanding:
  xor a
  ld c,1 ; SKY OBJECT ID
  push hl
  call drawspritecheckifsky
  pop hl
  xor a
  ld c,1 ; SKY OBJECT ID
  push hl
  inc l
  call drawspritecheckifsky
  ld hl,(currentplayerlocation)
  call getskytilemapid
  ld a,(de)
  or a
  jr z,l7f49
  dec a
  jr z,l7f49

  l7f2a:
  ld a,(wehavejected)
  dec a
  jr nz,wecrashedonlanding
  ld hl,(currscore)
  ld de,&0064
  add hl,de
  ld (currscore),hl
  
  call convwordtostr
  
  ld bc,&7fa0 ; PAGE OUT PLUS REGISTERS
  out (c),c
  
  ld hl,&40b4;020a
  ;call convspritelocationtopixelssplittable
  ld de,wordtostr
  call writelinede
  
  ld bc,&7fb8 ; PAGE IN PLUS REGISTERS
  out (c),c

  wecrashedonlanding:
  ld a,38     ; ENEMY PLANE BROKE IMAGE
  ld (crashonlandingimage),a
  pop hl
  jr l7f54

l7f49:
  inc de
  ld a,(de)
  or a
  jr z,l7f51
  dec a
  jr nz,l7f2a
  l7f51:
  ld a,9
  pop de

  l7f54: ; DRAW PLAYER HARRIER
  push hl
  xor a
  ld c,12;&0c
  call drawplusspritecheckifsky
  pop hl

  ld a,(crashonlandingimage)
  or a
  jr nz,crashonlanding

  ld a,(flightmode)
  dec a
  jp z,landinghoverloop

  ld a,(wehavejected)
  or a
  jp nz,landinghoverloop

  ; CHECK IF LANDED CORRECTLY?
  ; *** THIS MAY NEED TO BE ADJUSTED FOR SCROLL?

  ; CHECK VERTICAL

  ld a,h 
  cp #0d
  jp nz,landinghoverloop

  ;ld a,l
  ;sub #30;15
  ;jp c,landinghoverloop

  ;cp 3;#03
  ;jp nc,landinghoverloop

  ; CHECK HORIZONTAL

  ; CHECK WE HAVEN'T LANDED SHORT
  ld a,l
  sub 21;#15;15 START OF CARRIER
  jp c,landinghoverloop

  ; CHECK WE HAVEN'T LANDED TOO FAR FORWARD
  ld a,l
  sub 30;#19;15
  jp nc,landinghoverloop

  ; RECORD PLAYER LANDED, SO CONTROLS ARE DISABLED UNTIL WINGMAN LANDS PLANE
  xor a;ld a,1
  ld (markplayernotlanded),a

  ; IF WINGMAN IS STILL IN AIR, WAIT FOR HIM TO LAND
  ld a,(wingmanon)
  or a
  jr z,skipwingmanlandedcheck
  
  ld a,(wingmantakeoff)
  or a
  jp nz,landinghoverloop ; WINGMAN NOT LANDED YET
  
  skipwingmanlandedcheck:
  
  ; ------------------------------------
  ; WE HAVE LANDED!
  ; ------------------------------------

  ld hl,(currscore)
  ld de,&00c8
  add hl,de
  ld (currscore),hl

  call convwordtostr
  
  ld bc,&7fa0 ; PAGE OUT PLUS REGISTERS
  out (c),c

  ld hl,&40B4;020a
  ;call convspritelocationtopixelssplittable
  ld de,wordtostr
  call writelinede
  
  ld bc,&7fb8 ; PAGE IN PLUS REGISTERS
  out (c),c

  jp dofrigatenoise

; PLAYER STATUS 
; 0 = ALIVE
; 1 = HIT
; 2 = PLANE SMASHING APART
; 3 = END OF GAME

crashonlanding:
  call setmyplanebrokesprite
  ld a,3                     ; PLAYER STATUS KILLED
  ld (playerstatus),a
  jp doexplosionnoise

setenemyplanebrokesprite:
  push hl
  ; BROKE PLANE FRONT
  ld hl,sprite_pixel_data7
  ld de,&4400
  ld bc,128;&100
  ldir

  ; BROKE PLANE BACK
  ld hl,sprite_pixel_data8
  ld de,&4500
  ld bc,128;&100
  ldir
  pop hl
ret

setwingmanbrokesprite:
  push hl
  ; BROKE PLANE FRONT
  ld hl,sprite_pixel_data7
  ld de,&4F00
  ld bc,128;&100
  ldir

  ; BROKE PLANE BACK
  ld hl,sprite_pixel_data8
  ld de,&4E00
  ld bc,128;&100
  ldir
  pop hl
ret

setenemyplanenormalsprite:
  push hl
  ; LANDING PLANE FRONT
  ld hl,sprite_pixel_data5
  ld de,&4400
  call copyplusspritetoasic

  ; LANDING PLANE BACK
  ld hl,sprite_pixel_data6
  ld de,&4500
  call copyplusspritetoasic
  pop hl
ret

setlandingsprite:
  ; LANDING PLANE FRONT
  ld hl,sprite_pixel_data4
  ld de,&4000
  ld bc,128;&100
  ldir

  ; LANDING PLANE BACK
  ld hl,sprite_pixel_data3
  ld de,&4100
  ld bc,128;&100
  ldir

  ; WINGMAN
  
  ; LANDING PLANE FRONT
  ld hl,sprite_pixel_data19
  ld de,&4E00
  ld bc,128;&100
  ldir
  
  ; TAIL COLOURS
  ;ld hl,&4E00
  ;ld (hl),9
  ;ld hl,&4E00+&0010
  ;ld (hl),8
  ;ld hl,&4E00+&0011
  ;ld (hl),9
  
  ;ld hl,&4E00+&0010+&0010
  ;ld (hl),7
  ;ld hl,&4E00+&0011+&0010
  ;ld (hl),8
  ;ld hl,&4E00+&0012+&0010
  ;ld (hl),9

  ; LANDING PLANE BACK
  ld hl,sprite_pixel_data20
  ld de,&4F00
  ld bc,128;&100
  ldir 
ret

copyplusspritetoasic:
  ld bc,128;&100
  ldir
  ex de,hl
  ld bc,128
  jp clearmem

settakeoffsprite:
  ; LANDING PLANE FRONT
  ld hl,sprite_pixel_data1
  ld de,&4000
  call copyplusspritetoasic

  ; LANDING PLANE BACK
  ld hl,sprite_pixel_data2
  ld de,&4100
  jp copyplusspritetoasic
  
  ;ld a,(wingmantakeoff) ; DON'T MAKE WINGMAN TAKE OFF IF NOT ENABLED OR NOT TAKEN OFF
  ;or a
  ;ret z
  
  ; WINGMAN
settakeoffspritewingman:
  ; LANDING PLANE BACK
  ld hl,sprite_pixel_data17
  ld de,&4E00
  call copyplusspritetoasic
  
  ; TAIL COLOURS
  ;ld hl,&4E00
  ;ld (hl),7
  ;ld hl,&4E00+&0010
  ;ld (hl),8
  ;ld hl,&4E00+&0011
  ;ld (hl),8
  
  ;ld hl,&4E00+&0010+&0010
  ;ld (hl),9
  ;ld hl,&4E00+&0011+&0010
  ;ld (hl),9
  ;ld hl,&4E00+&0012+&0010
  ;ld (hl),9

  ; LANDING PLANE FRONT
  ld hl,sprite_pixel_data18
  ld de,&4F00
  jp copyplusspritetoasic

setmyplanebrokesprite:
  ; LANDING PLANE FRONT
  ld hl,sprite_pixel_data7
  ld de,&4000
  ld bc,128;&100
  ldir

  ; LANDING PLANE BACK
  ld hl,sprite_pixel_data8
  ld de,&4100
  ld bc,128;&100
  ldir
ret

currentskilllevel: defb 1

menufirejoystick:
  ld a,(menuselectitem)  ; ACTIVATE WHEN ARROW AT ITEM
  or a
  jp z,setskillleveljoy
  cp 3
  jp z,redefinekeys
jp waitkeypress
menurightjoystick:
  ld a,(menuselectitem)  ; ACTIVATE WHEN ARROW AT ITEM
  dec a;cp 1
  jr z,incrementskilllevel
  dec a;cp 2
  jp z,setkeyboardinput
  dec a
  dec a;cp 4 
  jr z,incrementrocketrange
  dec a;cp 5
  jp z,incrementcontrails
  dec a;cp 6
  jp z,incrementlockheight
  dec a;cp 7
  jp z,incrementwingman
jp waitkeypress
menuleftjoystick:
  ld a,(menuselectitem)  ; ACTIVATE WHEN ARROW AT ITEM
  dec a;cp 1
  jr z,decrementskilllevel
  dec a;cp 2
  jp z,setjoystickinput
  dec a
  dec a;cp 4 
  jr z,decrementrocketrange
  dec a;cp 5
  jp z,decrementcontrails
  dec a;cp 6
  jp z,decrementlockheight
  dec a;cp 7
  jp z,decrementwingman
jp waitkeypress
incrementskilllevel:
  ld a,(currentskilllevel)
  inc a
  cp 6
  jp z,waitkeypress
  ld (currentskilllevel),a
  jr drawskilllevel
decrementskilllevel:
  ld a,(menuselectitem)  ; ACTIVATE WHEN ARROW AT ITEM
  cp 1
  jp nz,waitkeypress
  ld a,(currentskilllevel)
  dec a
  or a
  jp z,waitkeypress
  ld (currentskilllevel),a
drawskilllevel:
  add #30                 ; CONVERT INTEGER TO ASCII CHARACTER
  ld hl,(skillleveltext3) ; POINTER TO A POINTER IN A TABLE!
  ld (hl),a               ; STORE INPUT METHOD
  ld hl,&100D
  call locatetext
  ld de,(skillleveltext3)
  call writelinede
  jp waitkeypress
decrementrocketrange:
  ld a,(rocketrange-1)
  cp 10                   ; DON'T GO UNDER 10
  jp z,waitkeypress
  dec a
  jr dosetrocketrange
incrementrocketrange:
  ld a,(rocketrange-1)
  cp 20                  ; DON'T GO OVER 20
  jp z,waitkeypress
  inc a
  dosetrocketrange:
  ld (rocketrange-1),a
  call displayrocketrange
  jp waitkeypress
incrementcontrails:
  ld a,79                ; SPRITE ID
  jr doincdeccontrails
decrementcontrails:
  xor a
  doincdeccontrails:
  ld (contrail1-1),a
  ld (contrail2-1),a
  ld (contrail3-1),a
  ld (contrail4-1),a
  call displaycontrailstatus
  jp waitkeypress
incrementlockheight:
  ld a,1
  jr dolockheighttoplayer
decrementlockheight:
  xor a
  dolockheighttoplayer:
  ld (lockinmissileheighttoplayer-1),a  ; PLAYER
  xor a
  ld (lockinmissileheighttoplayer2-1),a ; WINGMAN - LOCKING HEIGHT DOESN'T WORK FOR CPU CONTROL
  call displaylockheightstatus
  jp waitkeypress
displayrocketrange:
  ld a,(rocketrange-1)
  ld h,0
  ld l,a
  call convwordtostr
  ld hl,&250C
  call locatetext
  ld de,wordtostr+3
  jp writelinede

displaycontrailstatus:
  ld hl,&220D
  call locatetext
  ld a,(contrail1-1)
  or a
  jr z,displayno
  ld de,textyes
  jp writelinede
  displayno:
  ld de,textno
  jp writelinede
  displayno2:
  ld de,textno2
  jp writelinede

displaylockheightstatus:
  ld hl,&240E
  call locatetext
  ld a,(lockinmissileheighttoplayer-1)
  showstatus:
  or a
  jr z,displayno
  ld de,textyes
  jp writelinede
  
showstatuswingman:
  or a
  jr z,displayno2
  cp 1
  jr z,displaycpu
  ld de,textpl2
  jp writelinede
  displaycpu:
  ld de,textcpu
  jp writelinede
  
displaywingmanstatus:
  ld hl,&200F
  call locatetext
  ld a,(wingmanon)
  jr showstatuswingman
  
incrementwingman:
  ld a,(wingmanon)
  inc a
  cp 3
  jp z,waitkeypress
  jr dosetwingman

  ;ld a,1
  ;jr dosetwingman
decrementwingman:
  ld a,(wingmanon)
  dec a
  cp 255
  jp z,waitkeypress
  dosetwingman:
  ld (wingmanon),a
  call displaywingmanstatus
  jp waitkeypress

textyes: defb "On ",255
textno:  defb "Off",255
textcpu: defb "CPU     ",255
textpl2: defb "Player 2",255
textno2: defb "Off     ",255

menuselectitem:     defb 0     ; CURRENT MENU ITEM SELECTED
menuselectposition: defw &0A0C ; POSITION OF ARROW

resetselectedmenuitem:
  xor a
  jr savemenuselection
movemenuarrowdown:
  ld a,(menuselectitem)
  inc a
  cp 8
  jp z,waitkeypress ; DON'T GO OVER 5
  jr savemenuselection
movemenuarrowup:
  ld a,(menuselectitem)
  dec a
  cp 255
  jp z,waitkeypress ; DON'T GO UNDER 0
savemenuselection:
  ld (menuselectitem),a
  dec a;cp 1
  jr z,select_skilllevel
  dec a;cp 2
  jr z,select_controls
  dec a;cp 3
  jr z,select_redefinekeys
  dec a;cp 4
  jr z,select_rocketrange
  dec a;cp 5
  jr z,select_contrails
  dec a;cp 5
  jr z,select_lockheight
  dec a;cp 6
  jr z,select_wingman
select_startgame:
  ld hl,&010C
  jr drawitemarrow
select_skilllevel:
  ld hl,&010D
  jr drawitemarrow
select_controls:
  ld hl,&010E
  jr drawitemarrow 
select_rocketrange:
  ld hl,&150C
  jr drawitemarrow 
select_contrails:
  ld hl,&150D
  jr drawitemarrow 
select_lockheight:
  ld hl,&150E
  jr drawitemarrow 
select_wingman:
  ld hl,&150F
  jr drawitemarrow 
select_redefinekeys:
  ld hl,&010F
drawitemarrow:
  push hl
  ld hl,&010C
  call clearitemarrow
  ld hl,&010D
  call clearitemarrow
  ld hl,&010E
  call clearitemarrow
  ld hl,&010F
  call clearitemarrow
  ld hl,&150C
  call clearitemarrow
  ld hl,&150D
  call clearitemarrow
  ld hl,&150E
  call clearitemarrow
  ld hl,&150F
  call clearitemarrow
  pop hl
  
  call locatetextde
  ld a,">"
  call writechar
jr waitkeypress

clearitemarrow:
  call locatetextde
  ld a," "
  jp writechar

wipestartofgamedata:
  ; WIPE START OF GAME DATA UP TO KEYPRESSES
  ld hl,startofgamedata
  ld bc,endgamedata-startofgamedata ;70
  jp clearmem
;  xor a
;  l7fbb:
;    ld (hl),a
;    inc hl
;  djnz l7fbb
;ret

waitkeypressoptions:
  waitkeypress:
    call km_wait_key
    ; WAIT UNTIL USER UNPRESSES KEY TO REGISTER IT - NO KEY REPEAT
    call km_wait_keyrelease
    
    cp "J"
    jp z,setjoystickinput
    cp "K"
    jp z,setkeyboardinput
	cp "R"
	jp z,redefinekeys
    cp "1"
	jr z,setskilllevelkeys
	cp "2"
	jr z,setskilllevelkeys
	cp "3"
	jr z,setskilllevelkeys
	cp "4"
	jr z,setskilllevelkeys
	cp "5"
	jr z,setskilllevelkeys
	cp "u"
	jp z,movemenuarrowup
	cp "d"
	jp z,movemenuarrowdown
	cp "l" ; LEFT JOYSTICK
	jp z,menuleftjoystick
	cp "r" ; RIGHT JOYSTICK
	jp z,menurightjoystick
	cp 10 ; ENTER
	jp z,menufirejoystick
	cp 13 ; RETURN
	jp z,menufirejoystick
	cp "f" ; FIRE
	jp z,menufirejoystick
	
ifdef debug
	push af
    ld hl,&1E0F
    call locatetext
	pop af
    call writechar
	dec de
	dec de	
endif
	
  jr waitkeypressoptions
  
  setskillleveljoy:
  ld a,(currentskilllevel)
  dec a
  jr donesetskilllevel
  
  setskilllevelkeys:
  sub #31                          ; CONVERT ASCII TO INTEGER
  jr c,waitkeypress                ; DON'T LET ANY OTHER NUMBER COUNT FOR SKILL LEVEL
  cp 5
  jr nc,waitkeypress               ; DON'T LET ANY OTHER NUMBER COUNT FOR SKILL LEVEL

  donesetskilllevel:
  push af
  call disablesecondinterrupt      ; STOP SCROLLING MESSAGE
  call disablemusic                ; STOP MENU MUSIC
  call wipestartofgamedata
  pop af
  
  inc a
  ld hl,(topscorepoints)
  ld (highscore),hl
  ld (leveldifficulty),a
 
  call kl_time_please
  ld (currtime),hl
  
  ; RESTORE START FUEL ROCKETS BOMBS
  call replenishmissilesfuel
  
  call drawinstrumentpanel
  call setasicpalettegame           ; MAKE SURE WE ARE STARTING WITH DAY COLOURS
  call resetenemylandlocationlock   ; MAKE SURE THERE IS NO LOCK CARRIED OVER FROM LAST GAME
  call resetscrollingcustomwaypoint
  call resetfirstmissionvariable    ; DON'T FADE PALETTE ON FIRST TAKEOFF
  call setlinesandbits              ; COPY INPUT KEYS TO FUNCTIONS
  xor a
  call ClearScreen
  
  call disablescrollsecondharrier   ; DISABLE SCROLLING HARDWARE SPRITES
  call disablescrollsecondgunship

  ; DRAW WAVES ON SEA
  ld hl,&0f00  ; Y X LOCATION
  drawseatilesloop:
    ld a,r    ; GET RANDOM WAVE TILE BASED ON RANDOM R REGISTER  
    and 3     ; RESTRICT TO 3 OPTIONS, 0 1 2
    add 3     ; ADD 3 TO GET BASE TILE
    ld c,2    ; SEA OBJECT ID
    push hl
    call drawspritecheckifsky             
    pop hl
    inc l
    ld a,&28  ; HAVE WE REACHED END OF SCREEN?
    cp l
  jr nz,drawseatilesloop
  
  ; RESET PLAYER AND WINGMAN POSITIONS
  ld hl,&0d09                      ; START LOCATION OF PLAYER
  ld (currentplayerlocation),hl
  ld l,&0f                         ; START LOCATION OF WINGMAN
  ld (wingmanlocation),hl
  call resetplayerposition
  ;di
  ;call setupsprites                ; MAKE SURE INTERRUPT DOESN'T STOP SPRITE SET UP
  ;ld bc,interrupttable             ; RESET INTERRUPT POSITION
  ;ld (interrupttableptr),bc 
  ;call mc_wait_flyback             ; MAKE SURE INTERRUPT STARTS AT RIGHT PLACE AGAIN
  ;ei
  
  ; MOVE FRIGATE ON SCREEN  
  call movefrigateonscreen         ; SCROLL FRIGATE SPRITE ON SCREEN
  ld hl,&0C07                      ; WRITE FRIGATE OBJECT MAP DIRECTLY TO MAP
  call writefrigatetilemap         ; MAKE SURE TILE MAP ENABLES US TO BOMB OR CRASH INTO IT
  call dofrigatenoise
  call enablescrollsecondharrier   ; MAKE SURE FUNCTION IS REENABLED FOR SCROLLING SECOND HARRIER OFFSCREEN
 
  newlevelloop:

  checkliftoff:
    call testkeyup
  jr nz,checkliftoff
  ;call spawnhealthpowerup
  
  call startpalettefade
  call kl_time_please
  ld (currenttime),hl

  call settakeoffsprite                   ; MAKE SURE WE ARE FLYING AN UNBROKEN PLANE
  ld a,1
  ld (markplayernotlanded),a              ; SHOW PLAYER HAS NOT LANDED
  ld a,20
  ld (scrollfrigateoffscreencountdown),a  ; RESET SCROLL AMOUNT TO SCROLL CARRIER OFFSCREEN, THEN DISABLE INTERRUPT ROUTINE

  gameloop:
    ; ORIGINAL GAME LOOP FUNCTIONS
    call scrollscenery
    call checkenemyattacks
	
    ld iy,playermissileblock
    call checkfireplayermissile
    call checkerasebombtrail
    call checklaunchbomb
	
	ld iy,wingmanmissileblock
	call checkfirewingmanmissile
	call checkerasebombtrail
	call checklaunchbombwingman
	
	ld iy,playermissileblock
	
    call checkejectorseat
    call checkenemyfighterapproach
	;call movewingmanpowerup
    call checkplayerplaneexplosion
	
	; MY GAME LOOP FUNCTIONS
	;call erasewingmanwakescrolling
	;call drawwingmanplane
	
    ld a,(gamelevelprogress)
    cp 13                                 ; LANDING ON FRIGATE
    jp z,beginlandingapproach
    gotoendgameloop:
    ld a,(playerstatus)
    cp 3                                  ; IS PLAYER ALIVE?
  jr c,gameloop                           ; IF LESS THAN 3, DO LOOP
jp gameended

; -------------------------------------------------------------
; WINGMAN AI

; WINGMAN STATUS - wingmantakeoff
; A = 0 ON CARRIER
; A = 1 IN FLIGHT
; A = 2 LANDING HEAD TOWARDS FIRST WAYPOINT - MUST NOT BE INTERRUPTED WITH DIFFERENT WAYPOINT
; A = 3 LANDING HEAD TOWARDS SECOND WAYPOINT - MUST NOT BE INTERRUPTED WITH DIFFERENT WAYPOINT
; A = 4 WINGMAN IS LANDED
; A = 5 TRACK ENEMY PLANE
; A = 6 TRACK ENEMY PLANE 2ND WAYPOINT
; A = 7 REACHED ENEMY PLANE - FIRE MISSILE
; A = 8 HEAD TOWARDS CUSTOM WAYPOINT
; A = 9 REACHED CUSTOM WAYPOINT
; A = 10 HEAD TOWARDS SCROLLING CUSTOM WAYPOINT
; A = 11 REACHED SCROLLING CUSTOM WAYPOINT

; LANDING WAYPOINTS
wingman1stwaypoint: defw &0518 ; IN AIR ABOVE CARRIER
wingman2ndwaypoint: defw &0d1d;6;1d ; ON CARRIER
wingman2ndwaypointb: defw &0d16;d;8 ; ON CARRIER

; IF THE WINGMAN IS IDLE AND THERE IS A TARGET, START A BOMBING RUN - SOMETIMES!
checkwingmandobombingrun:
  ld a,(wingmanon)            ; ONLY IF CPU CONTROLLING HIM
  cp 1
  ret nz
  
  ; ONLY DO BOMBING RUN IF WE ARE FLYING IDLE
  ld a,(wingmantakeoff)
  cp 1
  ret nz

  ; CHECK STATUS OF THIS TARGET LOCK
  ld a,(enemylandlocationlock+2)
  or a ; LOCK BEING IGNORED, OR CHOSEN ALREADY
  ret nz
  
  ; RANDOMLY CHOOSE WHETHER TO DO BOMBING RUN ON THIS TARGET
  ld a,r
  and %00000011
  ld (enemylandlocationlock+2),a
  ; RND 1 TO 7 = THIS TARGET BEING IGNORED BY WINGMAN
  or a  
  ret nz

  ; THIS TARGET CHOSEN BY WINGMAN

  ; CHECK IF WE HAVE LOCK TO TARGET
  ld a,(enemylandlocationlock)
  or a
  ret z
  ; COPY TARGET LOCATION TO SCROLLING LOCK
  push hl
  ld hl,(enemylandlocationlock)
  dec h  ; FLY ABOVE TARGET
  dec h
  dec h
  dec h
  dec l
  dec l
  dec l  ; ALLOW FOR BOMB INERTIA
  dec l
  dec l
  ld (scrollingcustomwaypoint-2),hl
  pop hl  
  ; SET STATUS OF WINGMAN TO FOLLOW TARGET
  ld a,10
  ld (wingmantakeoff),a
ret

; SAME AS STANDARD CUSTOM WAYPOINT, BUT IS DECREMENTED WHEN SCREEN SCROLLS
wingmanmovetowardsscrollingcustomwaypoint: ; wingmantakeoff = 8/9
  ld de,0:scrollingcustomwaypoint
  jp findwaypointnoreverseflight
wingmanreachedscrollingwaypointdropbomb:
  call resetscrollingcustomwaypoint ; CLEAR WAYPOINT SO WE DON'T TARGET IT AGAIN
  jp resetwingmanflight
wingmanmovetowardscustomwaypoint: ; wingmantakeoff = 8/9
  ld de,0:customwaypoint
  jr findwaypoint
wingmanreachedplanefiremissile:
  ld iy,wingmanmissileblock
  xor a  
  ld (iy+4),a;(playermissilestatus2),a            ; RECORD IF MAVERICK OR SIDEWINDER - MOVES DIFFERENTLY
  call firewingmanmissile ; FIRE NORMAL MISSILE
  ; MOVE WINGMAN BACK TO STANDARD FLIGHT
  jp resetwingmanflight

; FIND ENEMY PLANE HEIGHT
wingmantrackenemyplane2ndwaypoint:
  ; CHECK IF ENEMY PLANE IS STILL TRACKABLE - IF NOT, GO TO NORMAL FLIGHT
  ld a,(missiletargetwingman)
  or a
  jp z,resetwingmanflight

  ; CHECK IF WE HAVE REACHED HEIGHT OF ENEMY PLANE AND ARE IN FIRING DISTANCE
  ; IF SO, INCREMENT WAYPOINT TO FIRE MISSILE
  ld de,(enemyplanelocation)
  ld bc,(wingmanlocation)
  
  ; CHECK TO SEE IF WE HAVE REACHED ALTITUDE OF ENEMY PLANE
  ld a,d
  cp b
  jr nz,notfoundenemyplaneheight
  ; CHECK IF WE ARE IN FIRING RANGE
  ld a,e
  sub c
  sub 10
  jp c,incrementwingmanwaypoint
  
  notfoundenemyplaneheight:
  ; IF TOO HIGH FOR TRACKING, ABORT!
  sub 2
  jp c,incrementwingmanwaypoint
  ld de,(enemyplanelocation)
  dec e ; ONLY TRACK LOCATION IN FRONT OF PLANE SO WE DON'T COLLIDE WITH IT
  dec e
  dec e
  dec e
  dec e
  dec e
  jr findwaypoint
  
is1stpass:         defb 0
is1stpasswaypoint: defw 0
wingmantrackenemyplanefirstpass:
  ld a,(is1stpass)
  or a
  jr nz,wingmantrackenemyplane2
  
  ; RECORD WE HAVE MADE FIRST PASS
  ; THIS RECORDS A FIXED COORDINATE BASED ON THE LOCATION THE WINGMAN WAS AT
  ; WHEN THE TRACKING FUNCTION WAS INITIATED
  ; THIS JUST SPEEDS WINGMAN UP INITIALLY TO AVOID COLLIDING WITH PLAYER PLANE
  ld a,1
  ld (is1stpass),a
  ld de,(wingmanlocation)
  inc e 
  inc e
  inc e
  inc e
  inc e
  inc e
  ld (is1stpasswaypoint),de
wingmantrackenemyplane2:
  ld de,(is1stpasswaypoint)
  jr findwaypoint
wingmanlandoncarrier:
  ld de,(wingman1stwaypoint)
  jr findwaypoint
wingmanlandoncarrier2:
  ld hl,(wingman2ndwaypoint) ; CGECK IF PLAYER 1 STOLE OUR SPACE
  call getskytilemapid
  ld a,(de)
  cp 12
  jr z,selectsecondlandingpad
  inc de
  cp 12
  jr z,selectsecondlandingpad
  ld de,(wingman2ndwaypoint)
  jr findwaypoint
  selectsecondlandingpad:    ; CHOOSE ALTERNATE PAD
  ld de,(wingman2ndwaypointb)
findwaypoint:
  ld bc,(wingmanlocation)
  
  ld a,c ; ABORT WAYPOINT IF WE ARE TOO CLOSE TO SCREEN EDGE
  sub 37
  jp nc,resetwingmanflight
  
  call getdirectionfromcoords    ; GET DIRECTION IN A
  or a                           ; WE REACHED WAYPOINT
  jr z,continuefindwaypoint
  recheckfindwaypoint:
  call checkwingmanradar         ; MAKE SURE THERE ARE NO OBSTABLES TO MOVE
  or a                           ; CAN'T MOVE - TRY DIFFERENT DIRECTION
  jr z,foundobstruction
  jr skipincrementwaypoint
  continuefindwaypoint:
  jr z,incrementwingmanwaypoint
  skipincrementwaypoint:         ; DON'T INCREMENT WAYPOINT AS WE HAVE TAKEN EVASIVE ACTION TO AVOID OBJECT
  ld hl,movewingmanspritetable
  call vectortablelaunchcode     ; MOVE WINGMAN TOWARD NEXT WAYPOINT
  ld (wingmanlocation),hl
ret

; FOUND OBSTRUCTION ON WAY TO WAYPOINT
; TRY RANDOM DIRECTION TO GET PAST?
foundobstruction:
  ld a,r
  and %00000111
  jr recheckfindwaypoint

; SAME AS ABOVE, ONLY IF DIRECTION TO WAYPOINT IS TO REVERSE,
; DON'T ALLOW IT. THIS IS BECAUSE THE SCREEN IS SCROLLING AT THE SAME TIME
; AND WE WILL NEVER REACH IT
findwaypointnoreverseflight:
  ld bc,(wingmanlocation)
  call getdirectionfromcoords    ; GET DIRECTION IN A
  or a                           ; REACHED WAYPOINT
  jr z,incrementwingmanwaypoint
  cp 6
  jr z,incrementwingmanwaypoint
  cp 7
  jr z,incrementwingmanwaypoint
  cp 8
  jr z,incrementwingmanwaypoint
  jr continuefindwaypoint
  
; SIGNAL WE HAVE REACHED A WAYPOINT
incrementwingmanwaypoint:
  xor a             ; RESET FIRST PASS VARIABLE
  ld (is1stpass),a
  
  ld a,(wingmantakeoff)
  inc a
  ld (wingmantakeoff),a
  cp 4
  jr z,setwingmanlanded
  cp 7
  jp z,wingmanreachedplanefiremissile
  cp 9
  jp z,resetwingmanflight
ret
; STOP MOVING WINGMAN AS WE ARE ON THE CARRIER
setwingmanlanded:
  xor a
  ld (wingmantakeoff),a
  
  ; RESET PIXEL POSITION OF WINGMAN SO WE CAN SCROLL IT AS LANDED ON CARRIER
  ;ld hl,16+16+16+16+16+16+16+16+16+16+16+16+16+16+16+16+16+16+16+16+16+16+16+16+16+16+16+16+16
  ;ld (secondplayerstartx),hl  
ret
  
wingmanstilloncarrier:
  ld a,(markplayernotlanded)   ; DON'T TAKE OFF, PLAYER STILL ON FRIGATE
  or a
  ret z
  
  xor a
  ld a,(wingmanlocation)       ; GET X POS OF WINGMAN
  ld b,a
  ld a,(currentplayerlocation) ; GET X POS OF PLAYER
   
  sub b
  ret c                        ; WINGMAN NOT PASSED PLAYER YET
  
  ld a,(playerstatus)          ; PLAYER NOT TAKEN OFF YET
  cp 1
  ret z
  
  ; PLAYER PASSED US ON DECK, SET FOR TAKE OFF
  ; BUT ONLY IF CPU IS PLAYING
  ; OTHERWISE PLAYER 2 MUST TAKEOFF BEFORE OUT OF VIEW
  
  ; AND ONLY IF WE AREN'T TRYING TO LAND AND PLAYER HAS LANDED IN FRONT OF US

  ld a,(wingmanon)
  cp 1
  jr z,skipcheckplayer2liftoff
  call testkeyup2              ; HAS PLAYER 2 PRESSED UP?
  ret nz
  skipcheckplayer2liftoff:
  ld a,1
  ld (wingmantakeoff),a         ; SHOW WINGMAN HAS TAKEN OFF
  jp settakeoffspritewingman    ; SET TAKE OFF SPRITE FOR WINGMAN

hidebrokenwingmanplane:
  ; WINGMAN SHOT DOWN
  ld b,14
  call hideplussprite
  inc b
  call hideplussprite
  ld a,254              ; MARK AS CLEARED FROM SCREEN
  ld (wingmantakeoff),a
ret

; ERASE WINGMAN WAKE, BUT ACCOUNTING FOR LAST POSITION HAVING SCROLLED
erasewingmanwakescrolling:
  ld a,(wingmantakeoff)   ; CHECK IF WINGMAN IS LAUNCHED AND ALIVE
  or a
  ret z
  cp 254
  ret z
  ; NEED TO ERASE OBJECT MAP WINGMAN LEAVES IN WAKE
  ld hl,(backupwingmanlocation)
  dec l
  ld c,1 ; SKY OBJECT ID
  xor a
  call drawspritecheckifsky                       ; ERASE PLAYER PLANE TRAIL BACK
  ld hl,(backupwingmanlocation)
  ld c,1 ; SKY OBJECT ID
  xor a
  jp drawspritecheckifsky                       ; ERASE PLAYER PLANE TRAIL FRONT
  
; ERASE WINGMAN WAKE, BUT ACCOUNTING FOR LAST POSITION NOT HAVING SCROLLED
erasewingmanwakelanding:
  ; ERASE WAKE
  ld hl,(backupwingmanlocation)
  call drawemptyskytile
  ld hl,(backupwingmanlocation)
  inc l
  jp drawemptyskytile

; CHECK PLAYER 2 KEYS FOR CONTROLLING WINGMAN
checkwingmankeys:
  ld d,b
  ld e,c
  
  call testkeyright2
  jr nz,notpressedrightp2
  inc e                  ; MOVE RIGHT
  
  ; DON'T MOVE PAST RIGHT BORDER
  ld a,e
  cp 38
  jr nz,skipresetright
  dec e
  skipresetright:
  
  notpressedrightp2:
  call testkeyleft2
  jr nz,notpressedleftp2
  dec e                  ; MOVE LEFT
  
  ; DON'T MOVE PAST LEFT BORDER
  ld a,e
  or a
  jr nz,skipresetleft
  inc e
  skipresetleft:
  
  notpressedleftp2:
  call testkeyup2
  jr nz,notpressedupp2
  dec d                  ; MOVE UP
 
  ; DON'T MOVE PAST TOP BORDER
  ld a,d
  or a
  jr nz,skipresettop
  inc d
  skipresettop:
  
  notpressedupp2:
  call testkeydown2
  jr nz,notpresseddownp2
  inc d                  ; MOVE DOWN
  notpresseddownp2:

  call getdirectionfromcoords ; GET DIRECTION IN A
  jr setwingmandirection2
  
; CONTROL SECOND HARRIER MOVEMENT - CPU
controlwingmanfunc:
  ld a,(wingmanon)      ; DON'T CONTROL WINGMAN IF NOT ENABLED
  or a
  ret z
  ld a,(wingmantakeoff) ; DON'T CONTROL WINGMAN IF NOT IN AIR
  cp 254
  ret z                 ; KILLED AND CLEARED
  or a
  jp z,wingmanstilloncarrier;ret z                 ; STILL ON RUNWAY - DON'T MOVE
  ; CHECK IF WE NEED TO TAKE OFF
  ; NEED TO CHECK IF IT'S DESTROYED, EVEN IF NOT ACTIVATED, OTHERWISE BROKEN IMAGE STAYS ON SCREEN
  cp 255                ; KILLED IN ACTION
  jr z,hidebrokenwingmanplane
 
  ; STORE BACKUP WINGMAN LOCATION IN CASE IF NEXT MOVE COLLIDES WITH PLAYER
  ld bc,(wingmanlocation)
  ld (backupwingmanlocation),bc
 
  ld a,(wingmanon)      ; DON'T CONTROL WINGMAN IF PLAYER CONTROLLING
  cp 2
  jr z,checkwingmankeys ; XXX MAKE KEYS CONTROL WINGMAN HERE
 
  ; DETECT AND AVOID ONCOMING OBJECTS
  ;call checkwingmanlongrangeradar  
  
  ld a,(wingmantakeoff) ; DON'T CONTROL WINGMAN IF NOT IN AIR

  
  ;or a
  ;jr z,wingmanstilloncarrier
  cp 2                  ; HEAD TO FIRST LANDING WAYPOINT
  jp z,wingmanlandoncarrier
  cp 3                  ; HEAD TO SECOND LANDING WAYPOINT
  jp z,wingmanlandoncarrier2
  ; cp 4 - WINGMAN LANDED
  cp 5                  ; INTERCEPT ENEMY PLANE
  jp z,wingmantrackenemyplanefirstpass
  cp 6                  ; INTERCEPT ENEMY PLANE 2ND WAYPOINT
  jp z,wingmantrackenemyplane2ndwaypoint
  cp 8                  ; MOVE TOWARDS CUSTOM WAYPOINT - EVASIVE MANOUVRES
  jp z,wingmanmovetowardscustomwaypoint
  cp 10                 ; SAME AS ABOVE BUT TARGET SCROLLS WITH SCREEN
  jp z,wingmanmovetowardsscrollingcustomwaypoint
  ; cp 6 - FIRE MISSILE

  ; GENERAL FLIGHT POSITION

  ; GET LOCATION OF PLAYER PLANE
  ;ld bc,(wingmanlocation)       ; CURRENT LOCATION
  ld de,(currentplayerlocation) ; DESTINATION

  ; GET DIRECTION TOWARD PLAYER
  ld a,(wingmanbelowplayer)
  cp 1
  jr z,setwingmanbelowplayer
  dec e  ; OFFSET DIRECTION TOP LEFT OF PLAYER
  dec e
  dec e
  dec d
  dec d
  dec d
  ;inc d
  ; IF WINGMAN DESTINATION COORDINATE IS OFF TOP OF SCREEN (255), CHANGE COURSE
  ld a,d
  cp 255
  jr z,setwingmanbelowplayer
  cp 254
  jr z,setwingmanbelowplayer
  jr setwingmandirection
  setwingmanbelowplayer:
  dec e  ; OFFSET DIRECTION BOTTOM LEFT OF PLAYER
  dec e
  dec e
  inc d
  inc d
  inc d
  setwingmandirection:
  call getdirectionfromcoords ; GET DIRECTION IN A
  call checkwingmanradar      ; CHECK WHICH DIRECTIONS ARE FREE FROM OBSTACLES
  
  setwingmandirection2:
  ; MOVE WINGMAN
  ld hl,movewingmanspritetable
  call vectortablelaunchcode
  ld (wingmanlocation),hl
ret

;testcount: defb 0

backupwingmanlocation: defw 0

; INPUT
; A = DIRECTION TO MOVE
  
movewingmanspritetable:
  defw dontmovewingman
  defw movewingmanup
  defw movewingmanupright
  defw movewingmanright
  defw movewingmandownright
  defw movewingmandown
  defw movewingmandownleft
  defw movewingmanleft
  defw movewingmanupleft

dontmovewingman:
  ld hl,(wingmanlocation)
  ret
movewingmanup:
  ld hl,(wingmanlocation)
  dec h
  ret
movewingmanupright:
  ld hl,(wingmanlocation)
  inc l
  dec h
  ret
movewingmanright: 
  ld hl,(wingmanlocation)
  inc l
  ret
movewingmandownright:
  ld hl,(wingmanlocation)
  inc l
  inc h
  ret
movewingmandown:
  ld hl,(wingmanlocation)
  inc h
  ret
movewingmandownleft:
  ld hl,(wingmanlocation)
  dec l
  inc h
  ret
movewingmanleft:
  ld hl,(wingmanlocation)
  dec l
  ret
movewingmanupleft:
  ld hl,(wingmanlocation)
  dec l
  dec h
  ret
  
drawwingmanplane:
  ld a,(wingmanon)       ; DON'T DRAW IF WE AREN'T ENABLED
  or a
  ret z

  ld a,(wingmantakeoff)
  cp 254
  ret z                  ; DON'T DRAW IF WE CRASHED AND BURNED
  cp 255
  ret z                  ; DON'T DRAW IF WE CRASHED AND BURNED
  
  ld hl,(wingmanlocation)
  call getskytilemapid
  
  push de
  ld a,(de)  ; CHECK BACK OF PLANE
  call checkwingmanagainstobjectmap
  pop de

  inc de     ; CHECK FRONT OF PLANE
  ld a,(de)
  call checkwingmanagainstobjectmap

  ld c,20   ; WINGMAN PLANE OBJECT ID
  ld a,14   ; SPRITE ID FOR WINGMAN PLANE
  ld hl,(wingmanlocation)
  jp drawplusspritecheckifsky

; --------------------------------------------------------------

; WRITE FRIGATE OBJECT MAP TO SCREEN SO PLAYER CAN BOMB IT
; INPUT
; HL = START LOCATION OF SHIP
writefrigatetilemap:
  push hl
  call getskytilemapid
  ld hl,writefrigatetilemap1
  ld bc,12
  ldir
  pop hl
  inc h
  push hl
  call getskytilemapid
  ld hl,writefrigatetilemap2
  ld bc,12
  ldir
  pop hl
  inc h
  call getskytilemapid
  ld hl,writefrigatetilemap3
  ld bc,12
  ldir
ret

; OBJECT MAP OF FRIGATE SHAPE TO WRITE TO TILEMAP
writefrigatetilemap1: defb 1,1,1,1,1,4,4,1,1,1,1,1
writefrigatetilemap2: defb 1,1,1,1,4,4,4,4,1,1,1,1
writefrigatetilemap3: defb 4,4,4,4,4,4,4,4,4,4,4,4

; SHOW FLAK DAMAGE METER

displayhealth:
  ld bc,&7fa0 ; PAGE OUT PLUS REGISTERS
  out (c),c
  
  ld de,&410C;030E
  ;call convspritelocationtopixelssplittable
  ;ld d,b
  ;ld e,c
  
  ld a,(totalflakdamagecount)
  ld b,a
  ld c,0 ; KEEP COUNT
  dooolooop:
    ld a,130
	push de
    call writecharplain
	pop de
	inc e
	inc e
	inc c
  djnz dooolooop
  
  ; FILL REMAINDER OF ARMOUR AREA WITH BLANK SPACE TO OVERWRITE DIFFERENCE IN LEVELS

  dooolooop2:
    ld a,c
    cp 23
	jr z,skipblankarmour
    ld a," "
	push de
    call writecharplain
	pop de
	inc e
	inc e
	inc c
  jr dooolooop2
  
  skipblankarmour:
  
  ld bc,&7fb8 ; PAGE IN PLUS REGISTERS
  out (c),c  
ret  

updatehealth:
  ld bc,(flakdamagecount)
  ld a,b
  sub c  ; GET DIFFERENCE IN A
  ret c  ; IF MORE THAN TOTAL, DON'T DISPLAY
  
  ld bc,&7fa0 ; PAGE OUT PLUS REGISTERS
  out (c),c
  
  add 14  ; SET X LOCATION TO START AT
  ld l,a
  ld h,3
  call convspritelocationtopixelssplittable
  
  ld d,b
  ld e,c
  ld a," "
  ; DRAW BLANK CHAR
  call writecharplain
  
  ld bc,&7fb8 ; PAGE IN PLUS REGISTERS
  out (c),c  
ret

replenishmissilesfuel:
  ld hl,fuelrocketsbombsstart
  ld de,playerfueldrocketsbombs
  ld bc,15;&000f
  ldir
  ld a,(leveldifficulty)
  ld b,a
  add 3
  ld (numberofbombs),a
  ld (l883a),a
  srl a
  ld (numberofrockets),a
  ld (l8835),a
  ld a,b
  add a
  ld b,a
  ld a,&19
  sub b                          ; MAKE GAME HARDER BY REDUCING HITS ALLOWED BY FLAK
  ld (totalflakdamagecount),a
  ; SHOW ARMOUR LEVEL
  jp displayhealth

beginlandingapproach:
  ld a,(playerstatus) ; IS PLAYER ALIVE?
  or a
  jp nz,gotoendgameloop

  call setlandingsprite
  ld a,(wingmantakeoff)
  or a   ; NOT FLYING
  jr z,skipsetwingmanlandingapproach
  cp 254 ; KILLED IN ACTION
  jr z,skipsetwingmanlandingapproach
  inc a
  ld (wingmantakeoff),a
  skipsetwingmanlandingapproach:

  call landinghoverloop

  ld a,(playerstatus) ; IS PLAYER ALIVE?
  or a
  jp nz,docheckejectorseat

  call timedelay
  call timedelay
  call timedelay
  call timedelay
  ld a,(leveldifficulty)
  cp 5
  jp z,skipincleveldifficulty
  inc a
  ld (leveldifficulty),a
  
  skipincleveldifficulty:

  call replenishmissilesfuel

  ld (playerstatus),a
  xor a
  ld (flakdamagecount),a
  
  ; REDRAW FILLED INVENTORY 
  ld bc,&7fa0 ; PAGE OUT PLUS REGISTERS
  out (c),c
  
  ld hl,&0802    ; HL = Y X LOCATION
  call drawgauge ; SPEED
  ld hl,&0816
  call drawgauge ; FUEL
  ld hl,&0502
  call drawgauge ; ROCKETS
  ld hl,&0516
  call drawgauge ; BOMBS
  ld hl,&0802
  ld a,8         ; SPRITE NUMBER
  call drawspritesplitscreen
  ld hl,&0511
  ld a,7         ; SPRITE NUMBER
  call drawspritesplitscreen
  
  ld bc,&7fb8 ; PAGE IN PLUS REGISTERS
  out (c),c

  ; SCROLL SCREEN FOR TAKEOFF
  ; PLAYER MUST START AT SPECIFIC HORIZONTAL LOCATION
  ; AS HIS X LOCATION IS MATCHED TO HIS SPEED.
  ; SO WE SCROLL FRIGATE UNTIL PLAYER 1 PLANE REACHES THIS STARTING POSITION
  ld a,(currentplayerlocation)
  sub 9
  ld b,a
  scrollrightfortakeoffloop:
    push bc
    call scrollscenery

    ; SCROLL PLAYER LANDED SPRITE WITH FRIGATE
    ld hl,(currentplayerlocation)
    dec l
    ld (currentplayerlocation),hl
	
    xor a
    call moveplussprite2
    inc l
    ld a,1
    call moveplussprite2
	
    call timedelay
    call timedelay
    pop bc
  djnz scrollrightfortakeoffloop

  ld hl,0
  ld (enemyshiptimer),hl
  xor a
  ld (gamelevelprogress),a
  xor a
  ld (playerstatus),a
jp newlevelloop

docheckejectorseat:
  ld a,(wehavejected)
  or a
  jr z,gameended

  cp 3
  jr nc,gameended
  call checkejectorseat
  call timedelay
jr docheckejectorseat

gameended:
  call timedelay
  call timedelay
  call timedelay
  di                            ; MAKE SURE INTERRUPT DOESN'T STOP SPRITE SETUP
  call hideallplussprites
  ld bc,interrupttable          ; RESET INTERRUPT POSITION
  ld (interrupttableptr),bc
  call mc_wait_flyback          ; MAKE SURE INTERRUPT STARTS AT RIGHT PLACE AGAIN
  ei
  call enterhighscore
jp backtostart

; ENTRY ADDRESS
; START MENU
start:
  di
  call initsound    ; CLEAR SOUND BUFFERS IN CASE THEY HAVE SPURIOUS BYTES
  call drawsplit    ; CLEAR SPLIT SCREEN INSTRUMENT PANEL AREA OF MEMORY
  call waitanykey  
  ld a,&0f          ; PEN BLACK
  call ClearScreen  ; CLEAR SCREEN
  call scr_set_mode
  
  call unlockasic
  
  ld bc,&7fb8       ; PAGE IN PLUS REGISTERS
  out (c),c
  
  call hideallplussprites
  call setupsprites
  call drawinstrumentpanel
    
  ; CHECK WHICH SPRITES DO WHAT

  ifdef showsprites
    ;ld hl,0
	;ld a,10 
	;call drawspritecheckifsky
	
    ld hl,0
    ld b,98
    ld a,0
    loopy:
      push af
      push hl
      push bc
      call drawspritecheckifsky2
      pop bc
      pop hl
      pop af
      inc a
	  inc l
    djnz loopy
  endif
	
  ld a,&C3; JP
  ld hl,&0038
  ld (hl),a
  inc hl
  ld bc,myinterrupt
  ld (hl),c
  inc hl
  ld (hl),b
  call mc_wait_flyback ; WAIT FLYBACK SO WE START INTERRUPTS AT TOP OF SCREEN
  ei

  ; MAKE SURE KEYS FOR JOYSTICK ARE DEFAULT
  call setjoystickbits
  backtostart:
  call drawmenu
  jp waitkeypressoptions

setcustominput:
  ld hl,(customtext)
  jr drawinputmethod
setkeyboardinput:
  call setkeyboardbits
  ld hl,(keyboardtext)
  jr drawinputmethod
setjoystickinput:
  call setjoystickbits
  ld hl,(joysticktext)
  drawinputmethod:
  ld de,(inputmethod)
  ld bc,8
  ldir
  ld hl,&0A0E
  call locatetext
  ld de,(inputmethod)
  call writelinede
jp waitkeypress

; KEEP TRACK OF IT SO WE CAN SCROLL IT OUT OF SIGHT WHEN WE TAKE OFF
;secondharrierlocation: defw 0
gunshiplocation:       defw &0d29

scrollsecondgunship:
  ld hl,(gunshiplocation)
  dec l
  ld (gunshiplocation),hl
  ld a,l
  cp 255-16 ; OFFSCREEN! DISABLE THIS ROUTINE
  jr z,disablescrollsecondgunship
  inc h  
  inc l
  jp drawnewgunship
  
scrollsecondharrier:
  ; SCROLL FRIGATE
  ld hl,(frigatestartx)
  ld bc,16
  sbc hl,bc
  ld (frigatestartx),hl
  call drawnewcarrier
  
  ; SLOW WINGMAN HERE IF PLAYED BY PLAYER 2
  ; GIVES ILLUSION OF SPEED WHILE TAKING OFF AND LANDING
  
  ; SCROLL SECOND HARRIER IF NOT FLYING
  ld a,(wingmantakeoff)
  ;or a
  ;jr nz,skipscrollsecondharrier ; IF WINGMAN HAS NOT TAKEN OFF, SCROLL HARRIER
  cp 254
  jr z,skipscrollsecondharrier   ; DON'T SCROLL IF WINGMAN IS DEAD  
  
  ; DON'T SLOW IF CPU PLAYING HIM - DOESN'T WORK AS HE GETS LEFT BEHIND
  ld a,(wingmanon)
  cp 1
  jr nz,skipcheckifcpuflying
  ld a,(wingmantakeoff)
  or a
  jr nz,skipscrollsecondharrier
  skipcheckifcpuflying:
  
  ld hl,(wingmanlocation)
  dec l
  ld a,l
  cp 0
  jr nz,skipdisablewingmanoffscreen
  
  ; DISABLE WINGMAN IF PLAYER PASSES HIM - BUT ONLY IF BEING PLAYED
  
  ld a,(wingmanon)
  or a
  jr z,skipdisablewingmanoffscreen
  
  ; DISABLE WINGMAN SO THEY CAN RESPAWN
  call wingmandestroyed
  ld a,254
  ld (wingmantakeoff),a
  skipdisablewingmanoffscreen:
  ld (wingmanlocation),hl

  ld a,14
  call moveplussprite2
  inc l
  ld a,15
  call moveplussprite2
  
  skipscrollsecondharrier:
  
  ld a,(scrollfrigateoffscreencountdown)
  dec a
  ld (scrollfrigateoffscreencountdown),a
  or a
  jr z,disablescrollsecondharrier
  
  ;ld a,l
  ;cp 255-18 ; IF OFFSCREEN! DISABLE THIS ROUTINE
  ;jr z,disablescrollsecondharrier
ret
  ;inc h  
  ;inc l
  ;jp drawnewcarrier
  
scrollfrigateoffscreencountdown: defb 20
  
scrollgunship:
  ld hl,(gunshiplocation)
  dec l
  ld (gunshiplocation),hl
  ld a,l
  cp 255-16 ; IF OFFSCREEN! DISABLE THIS ROUTINE
  jr z,disablescrollsecondgunship
  jp drawnewgunship

disablescrollsecondharrier:
  ld hl,scrollsecondharrierfunction-3
  jr dodisablehardwarespritescroll
disablescrollsecondgunship:
  ld hl,scrollsecondgunshipfunction-3
  dodisablehardwarespritescroll:
  ld (hl),0
  inc hl
  ld (hl),0
  inc hl
  ld (hl),0
ret

movesecondharrierlandingfrigate:
  call setupcarriersprite       ; MAKE SURE PARACHUTE POWERUP SPRITE IS GONE
  ld hl,640                                   ; START AT EDGE OF SCREEN AND SCROLL IN
  ld (frigatestartx),hl

  ; ONLY SCROLL ON SECOND HARRIER IF NOT BEING PLAYED
  ld a,(wingmanon)
  or a
  jr nz,skipmovesecondplayeronscreen
  
  ld hl,640+16+16+16+16+16+16+16+16+16 ; START AT EDGE OF SCREEN AND SCROLL IN
  ld (secondplayerstartx),hl
  
  ;ld hl,(wingmanlocation)
  ld a,&31                       ; START AT EDGE OF SCREEN AND SCROLL IN
  ld (wingmanlocation),a;hl

  skipmovesecondplayeronscreen:
  
; ENABLE SCROLLING OF HARDWARE SPRITE AT START OF GAME TO MOVE SECOND HARRIER OFFSCREEN
enablescrollsecondharrier:
  ld a,(gamelevelprogress)       ; CHECK IF CARRIER HAS BEEN BLOWN UP, IF SO, DON'T SCROLL ON SCREEN
  cp 14
  ret z
  ; CALL = &CD
  ; REENABLE SECOND HARRIER SCROLL FUNCTION
  ld hl,scrollsecondharrierfunction-3
  ld (hl),&CD
  inc hl
  ld bc,scrollsecondharrier
  ld (hl),c
  inc hl
  ld (hl),b
ret
; ENABLE SCROLLING OF HARDWARE SPRITE AT START OF GAME TO MOVE SECOND HARRIER OFFSCREEN
enablescrollsecondgunship:
  ; CALL = &CD
  ; REENABLE SECOND HARRIER SCROLL FUNCTION
  ld hl,scrollsecondgunshipfunction-3
  ld (hl),&CD
  inc hl
  ld bc,scrollsecondgunship
  ld (hl),c
  inc hl
  ld (hl),b
ret
movegunshiponscreen:
  ld hl,(gunshiplocation)
  ld l,&29                       ; START AT EDGE OF SCREEN AND SCROLL IN
  ld (gunshiplocation),hl
  
  ; MAKE TARGET FOR MAVERICK
  call updateenemylandlocationlock ; RECORD ENEMY POSITION FOR MAVERICK MISSILES TO LOCK ON
  call checkwingmandobombingrun    ; ALLOW WINGMAN TO BOMB TARGET
  ; ENABLE SCROLLING OF HARDWARE SPRITE AT START OF GAME TO MOVE SECOND HARRIER OFFSCREEN

  ; CALL = &CD
  ; REENABLE SECOND HARRIER SCROLL FUNCTION
  ld hl,scrollsecondgunshipfunction-3
  ld (hl),&CD
  inc hl
  ld bc,scrollgunship
  ld (hl),c
  inc hl
  ld (hl),b
ret

resetplayerposition:
  ld bc,-170
  ld (playerstartx),bc
  ld bc,-170+16+16+16+16+16+16+16
  ld (secondplayerstartx),bc
  
  	; DISPLAY BACK OF SECOND HARRIER
    ;ld de,(secondplayerstartx)
    ;ld a,14
    ;ld b,100+4
    ;call moveplussprite2_pixel
    ;; DISPLAY FRONT OF SECOND HARRIER
	;ld de,(secondplayerstartx)
    ;ld a,15
	;ld b,100+4
    ;call moveplussprite2_pixel
ret

resetfrigatescrollposition:
  ; USED FOR SCROLLING BY PIXEL
  ld bc,-204
  ld (frigatestartx),bc

  ld a,20
  ld (scrollfrigateoffscreencountdown),a
  jp setwingmanlanded ; RESET WINGMAN TAKEOFF STATUS
  ;xor a
  ;ld (wingmantakeoff),a  
 
  ; USED FOR SCROLLING BY CHAR
  ;ld hl,&0c14
  ;ld a,l
  ;sub 10 ; POSITION X ON DECK OF FRIGATE
  ;ld l,a
  ;inc h
  ;ld (wingmanlocation),hl

; START LOCATION FOR HARDWARE SPRITES
playerstartx:       defw -170
frigatestartx:      defw -204
frigatestarty:      defb 80
secondplayerstartx: defw -170+16+16+16+16+16+16+16

movefrigateonscreen:
  call setupcarriersprite  ; MAKE SURE WE ARE USING CARRIER HARDWARE SPRITES 
  call setlandingsprite                  ; AND PLANE LANDING SPRITES
  call resetfrigatescrollposition

  ; LOAD X POSITION OF PLAYER
  ld b,150
  movefrigateonscreenloopy:
    push bc
	
    call mc_wait_flyback ; DELAY SCROLL
	
	; DISPLAY BACK OF FIRST HARRIER
    ld de,(playerstartx)
	inc de ; MODE 1 MOVEMENT
	inc de
	ld (playerstartx),de
    xor a
    ld b,100+4
    call moveplussprite2_pixel
    ; DISPLAY FRONT OF FIRST HARRIER
	ld hl,(playerstartx)
	ld bc,16
	add hl,bc
	ld d,h
	ld e,l
    ld a,1
	ld b,100+4
    call moveplussprite2_pixel

	; DISPLAY BACK OF SECOND HARRIER
    ld de,(secondplayerstartx)
	inc de ; MODE 1 MOVEMENT
	inc de
	ld (secondplayerstartx),de
    ld a,14
    ld b,100+4
    call moveplussprite2_pixel
    ; DISPLAY FRONT OF SECOND HARRIER
	ld hl,(secondplayerstartx)
	ld bc,16
	add hl,bc
	ld d,h
	ld e,l
    ld a,15
	ld b,100+4
    call moveplussprite2_pixel

	; MOVE FRIGATE
    ld de,(frigatestartx)
	inc de ; MODE 1 MOVEMENT
	inc de
	ld (frigatestartx),de
	call drawnewcarrier
	
	pop bc
  djnz movefrigateonscreenloopy
ret

drawnewcarrier:
  ld c,0        ; C = X
  ld de,&0804   ; D = SPRITE, E = Y
  call drawcarrierblock_mode1
  ld c,2        ; X
  ld de,&0604   ; Y
  call drawcarrierblock_mode0
  ld c,4+2      ; X
  ld de,&0704   ; Y
  call drawcarrierblock_mode0 ; CARRIER BODY MODE 0 - WIDE 
  ld c,3+2      ; X
  ld de,&0A02   ; SPRITE
  call drawcarrierblock_mode1 ; CARRIER TOP
  ld c,4+3      ; X
  ld de,&0B02   ; SPRITE
  call drawcarrierblock_mode1 ; CARRIER TOP
  ld c,7+3      ; X
  ld de,&0904   ; SPRITE
  jp drawcarrierblock_mode1 ; CARRIER FRONT
  ;ld c,6+3      ; X
  ;ld de,&0203   ; Y
  ;call drawcarrierblock_mode1 ; HARRIER FRONT
  ;ld c,7+3      ; X
  ;ld de,&0303   ; Y
  ;jp drawcarrierblock_mode1   ; HARRIER BACK

; D = SPRITE NUMBER TO DRAW
; C = 8x8 BLOCK POSITION FROM LEFT CORNER
; E = 8x8 BLOCK POSITION FROM TOP
drawcarrierblock_mode1:
  ld hl,(frigatestartx)
  ld a,c
  rlca ; CONVERT BLOCK ID TO PIXEL COLUMN NUMBERS (MODE 2 PIXELS WIDE) AND ADD TO LEFT POS
  rlca
  rlca
  rlca
  ld b,0
  ld c,a
  add hl,bc ; GET NEW X POS IN HL
  
  ld a,(frigatestarty)
  ld b,a
  ld a,e
  rlca ; CONVERT BLOCK ID TO PIXEL ROW NUMBERS AND ADD TO TOP POS
  rlca
  rlca
  add b
  ld b,a    ; GET NEW Y POS
  
  ld a,d    ; GET SPRITE NUMBER
  ld d,h    ; GET POS IN DE
  ld e,l
  jp moveplussprite2_pixel

; D = SPRITE NUMBER TO DRAW
; C = 8x8 BLOCK POSITION FROM LEFT CORNER
; E = 8x8 BLOCK POSITION FROM TOP
drawcarrierblock_mode0:
  ld hl,(frigatestartx)
  ld a,c
  rlca ; CONVERT BLOCK ID TO PIXEL COLUMN NUMBERS (MODE 2 PIXELS WIDE) AND ADD TO LEFT POS
  rlca
  rlca
  rlca
  ld b,0
  ld c,a
  add hl,bc ; GET NEW X POS IN HL
  
  ld a,(frigatestarty)
  ld b,a
  ld a,e
  rlca ; CONVERT BLOCK ID TO PIXEL ROW NUMBERS AND ADD TO TOP POS
  rlca
  rlca
  add b
  ld b,a    ; GET NEW Y POS
  
  ld a,d    ; GET SPRITE NUMBER
  ld d,h    ; GET POS IN DE
  ld e,l
  jp moveplussprite2_pixel_mode0
	
	
; INPUT
; HL = LOCATION TO START DRAWING BODY
drawnewgunship:
    dec l
    ld a,12
    call moveplussprite2 ; GUNSHIP LEFT
    inc l
	inc l
    ld a,13
    jp moveplussprite2 ; GUNSHIP RIGHT

  
; DATA
;frigatesprite:
;defb &00,&00,&0c,&00,&0a,&02,&00,&09
;defb &02,&00,&00,&02,&11,&0f,&02,&10
;defb &0e,&02,&00,&0d,&02,&00,&00,&02
;defb &00,&00,&02,&00,&0a,&02,&00,&09
;defb &02,&00,&00,&0b,&00,&00,&00,&ff

; ABOVE FRIGATE WITH BACK AND FRONT HARRIER REMOVED
;frigatesprite:
;defb &00,&00,&0c,&00,0,&02,&00,0
;defb &02,&00,&00,&02,&11,&0f,&02,&10
;defb &0e,&02,&00,&0d,&02,&00,&00,&02
;defb &00,&00,&02,&00, 0 ,&02,&00, 0
;defb &02,&00,&00,&0b,&00,&00,&00,&ff

; STRANGE TIME DELAY
timedelay:
  push af
  push bc
  ld a,1
  ld b,120
  dowaittimer:
    inc a
	or a
    jr nz,dowaittimer
    ld a,1
  djnz dowaittimer
  pop bc
  pop af
ret

checkfirewingmanmissile:
  ; ARE WE CONTROLLED BY PLAYER?
  ld a,(wingmanon)
  cp 2
  ret nz
  ; ARE WE STILL ALIVE TO FIRE IT
  ld a,(wingmantakeoff)        
  cp 1
  ret nz
  ; ARE WE FIRING SOMETHING ALREADY?
  ld a,(iy+0);(playermissilestatus) ; MAKE SURE WE AREN'T FIRING MISSILE ALREADY - ONLY ONE LOCATION VARIABLE!
  or a
  ret nz
  
  call testkeyfire2           ; DID PLAYER 2 PRESS FIRE?
  ret nz
  call testkeyleft2           ; LEFT PLUS FIRE = FIRE AIR TO GROUND 
  jr nz,notfiredmaverickp2
  ld a,1                     ; FIRED MAVERICK
  jp continuefiringmissile
  notfiredmaverickp2:
  xor a                      ; FIRED NORMAL MISSILE
  jp continuefiringmissile

checkfireplayermissile:
  ld a,(iy+0);(playermissilestatus) ; MAKE SURE WE AREN'T FIRING MISSILE ALREADY - ONLY ONE LOCATION VARIABLE!
  or a
  ret nz

  ld a,(playerstatus)        ; ARE WE STILL ALIVE TO FIRE IT
  or a
  ret nz

  ld a,(playerfrigatestatus) ; ARE WE TRYING TO LAND? STOP PLAYER FIRING MISSILE
  or a
  jr nz,testfiredmissile

  ld a,(gamelevelprogress)   ; ARE WE TRYING TO LAND? STOP PLAYER FIRING MISSILE
  cp 11
  ret nc

  testfiredmissile:
  call testkeyfire           ; DID PLAYER PRESS FIRE?
  ret nz
  call testkeyleft           ; LEFT PLUS FIRE = FIRE AIR TO GROUND 
  jr nz,notfiredmaverick
  
  ; CHECK IF WE HAVE LOCK TO TARGET
  ld a,(enemylandlocationlock)
  or a
  jr z,notfiredmaverick
 
  ld a,(rocketinventory)     ; MAKE SURE WE HAVE MISSILES TO FIRE
  cp 1
  ret z
  
  ; FIRED MAVERICK AIR TO GROUND
  ld a,1
  jr continuefiringmissile
  ; FIRED SIDEWINDER
  notfiredmaverick:
  ld a,(rocketinventory)     ; MAKE SURE WE HAVE MISSILES TO FIRE
  cp 1
  ret z

  xor a  
  continuefiringmissile:
  ld (iy+4),a;(playermissilestatus2),a            ; RECORD IF MAVERICK OR SIDEWINDER - MOVES DIFFERENTLY
  ; DECREMENT NUMBER OF ROCKETS
  ld ix,numberofrockets                  ; REMOVE ROCKET FROM INVENTORY
  call decrementgaugelevelcheckredraw
  firewingmanmissile:                    ; FIRE MISSILE FROM WINGMAN PLANE, DON'T DECREASE INVENTORY
  ;ld a,2
  ; SET CURRENT RANGE AS 2 TO AVOID HITTING OWN PLANE
  ld (iy+3),2;a;(playermissilecurrentrange),a
  ; SET STATUS AS LAUNCHED MISSILE
  ld (iy+0),2;a;(playermissilestatus),a
  
  ;ld hl,(currentplayerlocation)
  ; INITIALIZE MISSILE POSITION FROM PLANE POSITION
  ld l,(iy+8)
  ld h,(iy+9)
  ld (iy+1),l;(playermissileposition),hl
  ld (iy+2),h
  ; RECORD START MISSILE POSITION FOR CALCULATING SPEED FOR WINGMAN
  ld (iy+10),l
  ld (iy+11),h
  jp domissilenoise

; FUEL, ROCKETS, BOMBS, GAUGES START DATA
fuelrocketsbombsstart:
  db #14 ; FUEL
  db #10
  db #25
  db #0f
  db 5;6   ; #15  Y LOC
  startrockets:
  db #04 ; ROCKETS
  db #10
  db #11
  db #04
  db 8;10  ; #18 - Y LOC
  startbombs:
  db #04 ; BOMBS
  db #10
  db #25
  db #04
  db 8;10  ; #18 - Y LOC

; INPUTS A = PEN
ClearScreen:
  ld hl,scr_addr
  ; DRAW 160 HORIZONTAL LINES
  call drawhoriz32
  call drawhoriz64
  drawhoriz64:
  call drawhoriz32
  drawhoriz32:
  call drawhoriz16
  drawhoriz16:
  call drawhoriz8
  drawhoriz8:
  call drawhoriz4
  drawhoriz4:
  call DrawHorizLine
  call DrawHorizLine
  call DrawHorizLine
; DRAW HORIZONTAL LINE ALL WAY ACROSS SCREEN
; INPUTS
; HL = SCREEN MEMORY LOCATION
; A = LINE COLOUR
; A PRESERVED
DrawHorizLine:
  push af
  push hl
  ld (hl),a ; LINE COL
  ld d,h
  ld e,l ; COPY HL TO DE
  inc e
  call useLDI79
  pop hl
  call scr_next_line_hl
  pop af
ret

; SPEED UP LDIR
useLDI40:
  call useLDI20
  jp useLDI20

useLDI80:
  call useLDI
useLDI79:
  call useLDI50
useLDI29:
  ldi
useLDI28:
  ldi
useLDI27:
  call useLDI20
  jr useLDI7
useLDI16:
  call useLDI8
  jr useLDI8
useLDI199:
  call useLDI60
  call useLDI79
useLDI60:
  call useLDI10
useLDI50:
  call useLDI20
useLDI30:
  call useLDI10
useLDI20:
  call useLDI10
useLDI10:
  ldi
useLDI9:
  ldi
useLDI8:
  ldi
useLDI7:
  ldi
useLDI6:
  ldi
useLDI5:
  ldi
useLDI4:
  ldi
useLDI3:
  ldi
  ldi
useLDI:
  ldi
ret

enterhighscore:
  call initsound ; CLEAR BUFFERS AND SILENCE CHANNELS
  ld a,&0f             ; BLACK PEN
  call ClearScreen
  call reset_scrolladjustments

  ld hl,(currscore)
  ld de,(lastscore)
  ld a,l
  sub e
  ld a,h
  sbc d
  jp c,drawmenu                    ; IF NO HIGH SCORE, JUST DRAW MENU
  
  ld ix,lastname  
  ld de,12;&000c                   ; LENGTH OF SCORE ENTRY
  ld b,7;10                        ; 10 SCORES TO MOVE DOWN TO MAKE ROOM FOR NEW SCORE
  checkhighestscoreloop:           ; CHECK WHERE TO INSERT NEW HIGH SCORE
    ld a,l
    sub (ix+10)
    ld a,h
    sbc (ix+11)
    jr c,foundhighestscore
    add ix,de
  djnz checkhighestscoreloop
  
  foundhighestscore:
  push ix          ; MOVE IX TO HL
  pop hl
  or a             ; CHECK IF SCORE SAME?
  sbc hl,de
  push hl

  l8276:
  ld de,lastname
  ld a,7;10
  sub b
  add a
  add a
  ld b,a
  add a
  add b
  jr z,savescorestoboard
  ld c,a
  ld b,0
  ld hl,scoreboardnames
  ldir

  savescorestoboard:
  pop hl
  ld b,6                  ; SIX CHARS IN LENGTH
  erasenamewithplacemarkerloop:
    ld (hl),&80           ; ERASE NAME WITH PLACE MARKERS
    inc hl
  djnz erasenamewithplacemarkerloop
  ld a,(leveldifficulty)
  add #30                 ; CONVERT INTEGER TO ASCII CHARACTER
  ld (hl),a
  inc hl
  ld de,(numberofhits)    ; SAVE HITS
  ld (hl),d
  inc hl
  ld (hl),e
  inc hl            
  inc hl  
  ld bc,(currscore)       ; SAVE SCORE
  ld (hl),c
  inc hl
  ld (hl),b
ret

currscrollchar: defw 0
halfcharscroll: defb 0           ; BOOLEAN

enabletributescroll:
  ld a,&CD         ; CALL = &CD
  ld bc,scrolltext ; secondinterruptcommand
  ld (secondinterruptcommand-1),a
  ld (secondinterruptfunction-2),bc
resettributescroll:
  ld hl,(tributetext)
  ld (currscrollchar),hl
ret

; ENABLE MUSIC IN MENU
enablemusic:
  ld hl,thirdinterruptspeed-1   ; PASS LOCATION OF INTERRUPT SPEED TO FUNCTION
  call initmusic                ; CLEAR BUFFERS AND CHANNELS AND RESET MUSIC TO START OF SCORE
  ld a,&CD                      ; CALL = &CD
  ld bc,playmusic               ; secondinterruptcommand
  ld (thirdinterruptcommand-1),a
  ld (thirdinterruptfunction-2),bc
ret
; DISABLE MUSIC
disablemusic:
  call initsound; call silencechannels ; CLEAR SOUND CHANNELS AND BUFFERS
  xor a                ; CALL = &CD
  ld bc,0              ; secondinterruptcommand
  ld (thirdinterruptcommand-1),a
  ld (thirdinterruptfunction-2),bc
ret

scrolltext:
  ld a,(halfcharscroll)
  inc a
  ld (halfcharscroll),a
  cp 1
  jr z,resethalfscroll  ; JUST SCROLL AS WE DON'T HAVE ENOUGH ROOM FOR LETTER
  
  ld hl,(currscrollchar)
  ld a,(hl)
  cp 255 ; END OF LINE - GO BACK TO START
  jr z,resettributescroll

  call scrolltribute
  ld de,tributetextaddr+78
  call writechar ; A = CHAR, DE = SCREEN POS

  ld hl,(currscrollchar)
  inc hl
  ld (currscrollchar),hl
ret
tributetextaddr equ scr_addr + &0050
resethalfscroll:
  ld a,255
  ld (halfcharscroll),a
scrolltribute:
  ld hl,tributetextaddr+1
  ld de,tributetextaddr
  call useLDI79
  ex de,hl
  ld (hl),&0f
  ex de,hl
  ld hl,tributetextaddr+1+&800
  ld de,tributetextaddr+&800
  call useLDI79
  ex de,hl
  ld (hl),&0f
  ex de,hl
  ld hl,tributetextaddr+1+&800+&800
  ld de,tributetextaddr+&800+&800
  call useLDI79
  ex de,hl
  ld (hl),&0f
  ex de,hl
  ld hl,tributetextaddr+1+&800+&800+&800
  ld de,tributetextaddr+&800+&800+&800
  call useLDI79
  ex de,hl
  ld (hl),&0f
  ex de,hl
  ld hl,tributetextaddr+1+&800+&800+&800+&800
  ld de,tributetextaddr+&800+&800+&800+&800
  call useLDI79
  ex de,hl
  ld (hl),&0f
  ex de,hl
  ld hl,tributetextaddr+1+&800+&800+&800+&800+&800
  ld de,tributetextaddr+&800+&800+&800+&800+&800
  call useLDI79
  ex de,hl
  ld (hl),&0f
  ex de,hl
  ld hl,tributetextaddr+1+&800+&800+&800+&800+&800+&800
  ld de,tributetextaddr+&800+&800+&800+&800+&800+&800
  call useLDI79
  ex de,hl
  ld (hl),&0f
  ex de,hl
  ld hl,tributetextaddr+1+&800+&800+&800+&800+&800+&800+&800
  ld de,tributetextaddr+&800+&800+&800+&800+&800+&800+&800
  call useLDI79
  ex de,hl
  ld (hl),&0f
ret

drawmenu:
  call enablemusic         ; ENABLE INTERRUPT TO PLAY MUSIC
  
  ld de,0
  call scr_set_border
  call setasicpalettemenu
  
  ld hl,&800C
  ;ld hl,&0600
  ;call locatetext
  ld de,(titletext2)
  call writelinede

  ld hl,&80A4;&0202
  ;call locatetext
  ld de,(scoreboardheader2)
  call writelinede

  ld de,&8142;hl,&0104
  ;call locatetextde
  ld hl,topscorename
  call printscores
  ld de,&8192;hl,&0105
  ;call locatetextde
  ld hl,scoreboard9
  call printscores
  ld de,&81E2;hl,&0106
  ;call locatetextde
  ld hl,scoreboard8
  call printscores
  ld de,&8232;hl,&0107
  ;call locatetextde
  ld hl,scoreboard7
  call printscores
  ld de,&8282;hl,&0108
  ;call locatetextde
  ld hl,scoreboard6
  call printscores
  ld de,&82D2;hl,&0109
  ;call locatetextde
  ld hl,scoreboard5
  call printscores

  ld de,&8322;hl,&010a
  ;call locatetextde
  ld hl,lastname
  call printscores

  ; WRITE START GAME
  ld hl,&83C6;030C
  ;call locatetext
  ld de,(startgametext)
  call writelinede

  ; WRITE SKILL TITLE
  ld hl,&8416;030D
  ;call locatetext
  ld de,(skillleveltext2)
  call writelinede

  ; WRITE SKILL LEVEL
  ld hl,&8430;100D
  ;call locatetext
  ld de,(skillleveltext3)
  call writelinede

  ; -------------------

  ; WRITE ROCKET RANGE TITLE
  ld hl,&83EE;170C
  ;call locatetext
  ld de,(rocketrange2)
  call writelinede

  ; WRITE ROCKET RANGE
  call displayrocketrange
  
  ; WRITE CONTRAILS TITLE
  ld hl,&843E;170D
  ;call locatetext
  ld de,(trailstext2)
  call writelinede

  ; WRITE CONTRAILS
  call displaycontrailstatus
  
  ; WRITE LOCK HEIGHT TITLE
  ld hl,&848E;170E
  ;call locatetext
  ld de,(lockheighttext2)
  call writelinede

  ; WRITE LOCK HEIGHT
  call displaylockheightstatus
  
  ; WRITE WINGMAN TITLE
  ld hl,&84DE;170F
  ;call locatetext
  ld de,(wingmantext)
  call writelinede
  
  ; WRITE WINGMAN
  call displaywingmanstatus
  
  ; WRITE CONTROLS LEVEL
  ld hl,&8466;030E
  ;call locatetext
  ld de,(controlstext)
  call writelinede
  
  ; WRITE JOYSTICK OR KEYS
  ld hl,&8474;0A0E
  ;call locatetext
  ld de,(inputmethod)
  call writelinede
  
  ; WRITE REDEFINE KEYS TITLE
  ld hl,&84B6;030F
  ;call locatetext
  ld de,(redefinekeystext)
  call writelinede
  
  call enabletributescroll    ; ENABLE TRIBUTE SCROLL
  
  jp resetselectedmenuitem

; PLAYER PRESSED ENTER BEFORE 6 CHARS OF NAME ARE PRINTED
; FILLED REST OF NAME WITH SPACES
; INPUT
; HL = CURRENT POSITION IN SCOREBOARD NAME ENTRY
; DE = SCREEN POS
finishnameentryloop:
  pop hl
  pop bc

  call writecursor 
  finishnameentryloop2:
  ld a,&80
  cp (hl)
  jp nz,nameentrycomplete
  ld (hl)," "
  inc hl
  ld a," "       
  call writechar  ; MOVE ON A SPACE CHARACTER - MAKE SURE TO ERASE CURSOR
jr finishnameentryloop2
delcharnameentry:
  pop hl
  pop bc         ; CHECK NUMBER OF CHARACTERS PLAYER HAS ENTERED
  ld a,b         ; MAKE SURE WE DON'T DELETE TOO FAR!
  sub 6          ; IF IT IS ZERO, GO BACK TO START
  or a
  jr z,printscores2
  
  push bc
  push hl
  dec hl
  ld (hl),&80
  dec de              
  dec de
  ld a," "
  call writechar ; CLEAR CURRENT CHAR AND CURSOR
  ld a," "
  call writechar       ; CLEAR CURSOR
  dec de
  dec de
  dec de
  dec de
  call writecursor     ; REDRAW CURSOR
 
  pop hl
  dec hl
  pop bc
  inc b                ; MAKE SURE WE CAN STILL TYPE UP TO 6 CHARS
  jr printnamestrloop

currentjoykey: defb 0

decrementcurrentkey:
  ld a,(currentjoykey)
  or a
  jr z,setcurrentjoykeya
  dec a
  
  ; DON'T GO BEYOND BOUNDS OF ALPHABET
  sub " "
  jr c,getanotherkey
  add " "
  
  ld (currentjoykey),a
  jr displaycurrentjoykey
incrementcurrentkey:
  ld a,(currentjoykey)
  or a
  jr z,setcurrentjoykeya
  inc a
  
  ; DON'T GO BEYOND BOUNDS OF ALPHABET
  sub "Z"+1
  jr nc,getanotherkey
  add "Z"+1
  
  ld (currentjoykey),a
  jr displaycurrentjoykey
setcurrentjoykeya:
  ld a,"A"
  ld (currentjoykey),a
  displaycurrentjoykey:
  push af
  call writechar
  pop af
  dec de
  dec de
  jr getanotherkey
  
joymovenextchar:
  ld a,(currentjoykey)
  or a ; NO KEY SELECTED, JUST ENTER
  jr z,finishnameentryloop
  
  push af
  call writecharcursor
  xor a
  ld (currentjoykey),a
  pop af
jr storekeymovenext
;printnocursor:  
;  pop af
;  call writechar
;  dec de
;  dec de
;jr storekeymovenext

; INPUT
; HL = SCOREBOARD ENTRY
; DE = SCREEN POS
; PRINT SCORE BOARD
printscores:
    ; RESET CURRENT KEY FOR JOYSTICK ENTRY
    xor a
    ld (currentjoykey),a
    
 	dec de
	dec de
    printscores2:

    ; NAME
    ld b,6 ; STOP IF WE REACH 6 CHARS
    printnamestrloop:
	  push bc
	  push hl
	  
      ld a,(hl)
	  cp &80
	  jr nz,skipnameentry
      
	  ; PRINT CURSOR SO PLAYER KNOWS TO ENTER NAME
	  call writecursor
	  call enableblinkcursor
	  
      getanotherkey:	
	    call km_wait_key   ; GET KEYPRESS
	    call km_wait_keyrelease
		  
	    cp 10 ; ENTER
	    jp z,finishnameentryloop
	    cp 13 ; ENTER
	    jp z,finishnameentryloop
	    cp 7  ; DEL
	    jp z,delcharnameentry
	    cp "l" ; JOY LEFT
	    jp z,delcharnameentry
		cp "f" ; JOY RIGHT
		jr z,joymovenextchar
	    cp "u" ; JOY UP
		jr z,incrementcurrentkey
		cp "d" ; JOY DOWN
		jp z,decrementcurrentkey
		
		; ONLY ACCEPT ALPHANUMERIC KEYS
	    sub "Z"+1
        jr nc,getanotherkey
        add "Z"+1
	    sub " "
        jr c,getanotherkey
        add " "
		
	    push af               ; BACK UP CHAR
	    pop bc                ; CHECK IF THIS IS LAST CHAR 
	    ld a,b
	    push bc
	    or a                  ; IF SO, DON'T PRINT CURSOR
	    jr z,printnocursor2
	    pop af
	    push af
        call writecharcursor
	    pop af
		jr storekeymovenext
	    printnocursor2:       ; PRINT NO CURSOR ON THE LAST LETTER SO IT DOESN'T TRASH SCREEN
	    push af
	    call writechar
	    pop af
	    jr storekeymovenext

	  skipnameentry:
	  push af
      call writechar
	  pop af

	  storekeymovenext:
	  pop hl	  
	  ld (hl),a
	  
      pop bc
      inc hl
    djnz printnamestrloop
	
    nameentrycomplete:
	call disablesecondinterrupt ; STOP BLINKING CURSOR
	; ERASE LAST CHARACTER IF ENTERED BY JOYSTICK
	push hl
	call checkcheatcode
    pop hl

	ld a," "
	call writechar
	nameentrycomplete3:
    ; CLEAR FROM NAME TO START OF LEVEL
    ld b,7         
    drawnameloop:
      inc de
      inc de
    djnz drawnameloop

    ; PRINT LEVEL
    ld a,(hl)
    inc hl
    call writechar
    
    ; CLEAR FROM LEVEL TO END OF ROW
    ld b,&8;1a ; 26 SPACES
    drawhitsloop:
      inc de
      inc de
    djnz drawhitsloop
    
    ; PRINT HITS
    push hl
    ld a,(hl)
    inc hl
    ld l,(hl)
    ld h,a

    call convwordtostr
    ld hl,wordtostr
    call writeline
    pop hl
    inc hl
    inc hl
	inc hl ; NOT SURE WHY SO MANY SPACES ARE NEEDED, HOW BIG DOES THE SCORE GET?

    ; MAKE SPACE
    ld b,&4;1a ; 26 SPACES
    drawspaceloop:
      inc de
      inc de
    djnz drawspaceloop

    ; PRINT SCORE
    ld a,(hl)
    inc hl
    ld h,(hl)
    ld l,a
    call convwordtostr
    ld hl,wordtostr
    call writeline
    ld a,"0"         ; WRITE LAST 0. SCORE IS MULTIPLES OF TEN
    jp writechar

checkcheatcode:
  dec hl
  ld a,"E"
  cp (hl)
  ret nz
  dec hl
  ld a,"M"
  cp (hl)
  ret nz
  dec hl
  ld a,"P"
  cp (hl)
  ret nz
  dec hl
  ld a,"L"
  cp (hl)
  ret nz
  dec hl
  ld a,"E"
  cp (hl)
  ret nz
  dec hl
  ld a,"H"
  cp (hl)
  ret nz
  xor a
  ld (deathstatus1-1),a
  ld (deathstatus2-1),a
  ld (deathstatus3-1),a
  ld (deathstatus4-1),a
ret

wordtostr: defb 0,0,0,0,0,255

; DATA
;l837c: defb &08,&20,&08
; AMSOFT
lastname:  defb "CPSOFT","0",0,0,0
lastscore: defb 10,0
; DURELL
scoreboardnames: 
;scoreboard2:    defb "DURELL","0",0,0,0,10,0
;scoreboard3:    defb "AMSOFT","0",0,0,0,10,0
;scoreboard4:    defb "CPSOFT","0",0,0,0,10,0
scoreboard5:    defb "DURELL","0",0,0,0,10,0
scoreboard6:    defb "AMSOFT","0",0,0,0,10,0
scoreboard7:    defb "CPSOFT","0",0,0,0,10,0
scoreboard8:    defb "DURELL","0",0,0,0,10,0
scoreboard9:    defb "AMSOFT","0",0,0,0,10,0
topscorename:   defb "CPSOFT","0",0,0,0
topscorepoints: defb 10,0

scrollscenery:;setupscreen:
  ;ld ix,l891f ; NOT SURE IF THIS IS NEEDED?
  ;ld (ix+#00),0
  ;res 0,(ix+#01)

  ; SCROLL PLAYING AREA BY CRTC
  call scroll_right
  call update_scroll
  
  ; FUNCTIONS ENABLED WHEN SCROLLING STATIONARY HARDWARE SPRITES INTO AND OUT OF SCREEN
  call scrollsecondharrier:scrollsecondharrierfunction
  call scrollsecondgunship:scrollsecondgunshipfunction
  ;l8884:
  ;  ld a,(currentplayerlocation+1) ; GET PLAYER HEIGHT 
  ;  cp (ix+#00)
  ;  call z,checkplayerplanemovement

  ;  ld a,(currentplayerlocation+1)
  ;  cp (ix+#00)
  ;  call z,scrollobjecttilemap

  ;  ; ONLY CHECK MOVEMENT AND OBJECT MAP AFTER SCROLLING COMPLETE COLUMN OF TILES
  ;  inc (ix+#00)
  ;  ld a,16
  ;  cp (ix+#00)
  ;jr nz,l8884

  ; THIS ROUTINE CLEARS PLAYER PLANE OBJECT FROM MAP
  ; WINGMAN FUNCTION NEEDS TO SEE PLAYER TO AVOID HIM???
  call checkplayerplanemovement
  call controlwingmanfunc
  call movewingmanpowerup

  ;call checkwingmanaltitude
  call scrollobjecttilemap

	; MY GAME LOOP FUNCTIONS
	; SCROLL COORDINATES AFTER OBJECT MAP SCROLL
    call scrollenemylandlocationlock ; SCROLL ENEMY LOCATION FOR MAVERICK MISSILES
    call scrollcustomwaypoint        ; SCROLL TARGET FOR CUSTOM WAYPOINTS FOR WINGMAN
    call scrollwingmanpoweruplocation 
  
	call erasewingmanwakescrolling
	call drawwingmanplane
	
	
  
  ld iy,playermissileblock              ; USE PLAYER MISSILE BLOCK FOR FIRING MISSILES
  call checkplayermissilemove
  ld iy,wingmanmissileblock
  call checkwingmanmissilemove         ; REPEAT MISSILE MOVEMENT FUNCTIONS FOR WINGMAN
  ;call erasemissiletrail
  ; DOUBLE MISSILE SPEED MOVE TO COMPENSATE FOR SCREEN SCROLL
  ;call checkplayermissilemove ; REPEAT MISSILE MOVEMENT FUNCTIONS FOR WINGMAN
  ;call erasemissiletrail
  ;ld iy,playermissileblock

  ; LOAD 255 INTO FRESH EDGE OF TILEMAP - WILL BE REPLACED BY SKY TILES
  ld hl,tilemap
  ld b,16
  ld de,&0028
  ld c,255
  l88c7:
    ld (hl),c
    add hl,de
  djnz l88c7
  
  ld hl,(enemyshiptimer)
  inc hl
  ld (enemyshiptimer),hl
  
  call genrandomhl             ; GENERATE RANDOMISED LAND HEIGHT + ENEMIES
  call docountdowntoenemyship             
  call drawrandomcloudsprite

  ; DRAW FRESH EDGE OF TILEMAP

  ld hl,&1027   ; Y X LOCATION - RIGHT SIDE OF SCREEN AT BOTTOM
  ; START AT BOTTOM TO REDUCE REDRAW FLICKER FROM SCROLLING
  drawfreshskytiles:
    push hl
    call getskytilemapid
    ld a,(de)
    inc a
    jr nz,skipdrawingemptyskytiles

    ; CLEAR SKY TILES ON EDGE OF SCREEN
    ld c,1 ; SKY OBJECT ID
    ld b,0 ; GRAPHIC ID
    call drawspritecheckifsky3

    skipdrawingemptyskytiles:
    pop hl
    dec h
    ld a,h
    cp 255                     ; DRAW TILES THAT HAVE 255 SET AS OBJECT
  jr nz,drawfreshskytiles
  
  ld a,(scrollsceneryafterdeath) ; CONTINUE TO SCROLL SCENERY 7 TILES AFTER WE CRASH 
  or a
  ret z
  dec a
  ld (scrollsceneryafterdeath),a
ret

checkenemyattacks:
  call launchflakattack
  call launchmissile       ; LAUNCH PLANE OR MISSILE FROM BOAT
  call launchenemyplane    ; MOVE ENEMY PLANE?
  call timercountdown
  jp checkplayerspeed

;l891f: defb 0,0,1

; RANDOMIZE LAND HEIGHT + ENEMIES
genrandomhl:
  ld hl,(currtime)
  ld d,h
  ld e,l
  add hl,hl
  add hl,hl
  add hl,hl
  add hl,hl
  push hl
  add hl,hl
  ex (sp),hl
  or a
  sbc hl,de
  pop bc
  add hl,bc
  add hl,hl
  add hl,hl
  add hl,hl
  add hl,de
  add hl,hl
  add hl,hl
  add hl,de
  ld de,#0029
  add hl,de
  ld (currtime),hl
ret

splitscreenaddrlookup:
  defw &4000 ; 1
  defw &4050 ; 2
  defw &40A0 ; 3
  defw &40F0 ; 4
  defw &4140 ; 5
  defw &4190 ; 6
  defw &41E0 ; 7
  defw &4230 ; 8
  defw &4280 ; 9
  defw &42D0 ; 10
  defw &4320 ; 11
  defw &4370 ; 12
  defw &43C0 ; 13
  defw &4410 ; 14
  defw &4460 ; 15
  defw &44B0 ; 16
  defw &4500 ; 17
  defw &4550 ; 18
  defw &45A0 ; 19
  defw &45F0 ; 20
  defw &4640 ; 21
  defw &4690 ; 22
  defw &46E0 ; 23
  defw &4730 ; 24
  defw &4780 ; 25

; INPUT
; HL = CHARACTER LOCATION
; BC = PIXEL LOCATION
convspritelocationtopixelssplittable:
  push af
  push hl
  ld c,h
  ld b,0
  ld hl,splitscreenaddrlookup
  add hl,bc ; DOUBLE FOR 2 BYTE LOOKUP LIST
  add hl,bc
  ld c,(hl)
  inc hl
  ld b,(hl)
  pop hl
  
  ld a,l
  rlca      ; DOUBLE BYTES FOR MODE 1
  ld h,0 
  ld l,a
  add hl,bc ; ADD HORIZONTAL BYTES (DOUBLE CHAR WIDTH FOR MODE 1)
  ld b,h    ; LOAD INTO BC
  ld c,l
  pop af
ret

; INPUTS
; HL = Y X GRID LOCATION
; OUTPUTS
; BC = SCREEN LOCATION

;139 T STATES

edgelookuptable:
  defw &8000+78 ;1
  defw &8050+78 ;2 
  defw &80a0+78 ;3
  defw &80f0+78 ;4
  defw &8140+78 ;5
  defw &8190+78 ;6
  defw &81e0+78 ;7
  defw &8230+78 ;8
  defw &8280+78 ;9
  defw &82d0+78 ;10
  defw &8320+78 ;11
  defw &8370+78 ;12
  defw &83c0+78 ;13
  defw &8410+78 ;14
  defw &8460+78 ;15
  defw &84b0+78 ;16
  defw &8500+78 ;17
  defw &8550+78 ;18
  defw &85a0+78 ;19
  defw &85f0+78 ;20
  defw &8640+78 ;21
  defw &8690+78 ;22
  defw &86e0+78 ;23
  defw &8730+78 ;24
  defw &8780+78 ;25
edgelookuptableend:  defw 0

edgelookuptableorig:
  defw &8000+78
  defw &8050+78
  defw &80a0+78
  defw &80f0+78
  defw &8140+78
  defw &8190+78
  defw &81e0+78
  defw &8230+78
  defw &8280+78
  defw &82d0+78
  defw &8320+78
  defw &8370+78
  defw &83c0+78
  defw &8410+78
  defw &8460+78
  defw &84b0+78
  defw &8500+78
  defw &8550+78
  defw &85a0+78
  defw &85f0+78
  defw &8640+78
  defw &8690+78
  defw &86e0+78
  defw &8730+78
  defw &8780+78

convspritelocationtopixelscrtctable:
  push af
  push hl
  ld c,h
  ld b,0
  ld hl,edgelookuptable
  add hl,bc ; DOUBLE FOR 2 BYTE LOOKUP LIST
  add hl,bc
  ld c,(hl)
  ;ld c,a
  inc hl
  ld b,(hl)
  ;ld b,a

  ld a,(scroll_adjustmentx)
  ld h,0
  ld l,a
  add hl,hl
  ;add hl,hl
  add hl,bc
  ;add hl,bc ; DOUBLE (2 BYTES = 1 CRTC CHAR)
  ;ld bc,78  ; RIGHT HAND COLUMN
  ;add hl,bc

  ld b,h
  ld c,l

ifdef debugscroll
  ; PAUSE BEFORE CRASH
  ld hl,(scroll_offset)
  ld de,&0182
  sbc hl,de
  jr nc,fixwrap
endif

  pop hl
  pop af
ret

convspritelocationtopixelscrtc:
  push af
  ld a,h
  add a
  ld c,a
  add a
  add a
  add c
  ld b,l
  push hl
  ld l,a
  xor a
  ld h,a
  add hl,hl
  add hl,hl
  add hl,hl
  ld a,b
  add a
  add l
  ld c,a
  ld a,h
  adc &00
  or &80 ; SCREEN STARTING AT 8000
  ld b,a

  ; ABOVE CODE CALCULATES PIXEL POSITION OF SPRITE IN BC
  ; FOR NORMAL UNSCROLLED SCREEN

  ; ----------------------------------------------------

  ; BELOW CODE TAKES PIXEL POSITION AND ADDS SCROLL OFFSET
  ; AND WRAPS BACK TO C000 IF WE GO OVER FFFF

  ; ADJUST BC, ADD OFFSET AND WRAP IF OVER FFFF
  ; -------------------------------------------
  ld hl,(scroll_offset)
  add hl,hl  ; DOUBLE - 2 BYTES PER CRTC UNIT
  add hl,bc
  ld b,h
  ld c,l

ifdef debugscroll
  ; PAUSE BEFORE CRASH
  ld hl,(scroll_offset)
  ld de,&0182
  sbc hl,de
  jr nc,fixwrap
endif

  pop hl
  pop af
ret

ifdef debugscroll
fixwrap:
  nop
  pop hl
  pop af
ret
endif

; FASTER USING TABLE
; 70 T STATES

; INPUTS 
; A = SPRITE NUMBER
; OUTPUT
; DE = SPRITE DATA 
getspritedata:
  ld h,0
  ld l,a
  add hl,hl ; DOUBLE FOR LOOKUP
  ld de,(spritelookuptable)
  add hl,de
  ld e,(hl)
  inc hl
  ld d,(hl)
ret

; CONVERTS CHARACTER LOCATION ON SCREEN
; TO OBJECT TILE GRID LOCATION
; INPUTS
; H = Y COORD
; L = X COORD
; OUTPUTS
; DE = TILE GRID LOCATION
getskytilemapid:
  push hl
  ld a,h
  ld e,l
  add a
  ld d,a
  add a
  add a
  add d
  ld l,a
  ld h,0
  ld d,0
  add hl,hl
  add hl,hl
  add hl,de
  ld de,startobjecttilemap ; ADD BASE OF TILE GRID
  add hl,de
  ex de,hl
  pop hl
ret

; INPUTS
; A = TILE ID

; H = Y COORD
; L = X COORD

; C = OBJECT ID TO INSERT INTO TILE GRID
drawspritecheckifsky:
  ld b,a                            ; STORE TILE ID IN B
  call getskytilemapid              ; GET TILE GRID PTR IN DE
  ld a,(de)
  or a                              ; SKIP IF CLOUDS OBSCURE US
  ret z
drawspritecheckifsky2:              ; SKIP TILE ID LOOKUP IF WE ALREADY HAVE IT
  ld a,c                            ; LOAD OBJECT ID INTO TILE GRID
  ld (de),a
  ld a,b                            ; RESTORE TILE ID IN A
  drawsprite:
  call convspritelocationtopixelscrtc   ; CONVERT HL CHAR LOCATION TO BC PIXEL LOCATION
  call getspritedata                ; GET SPRITE FROM TABLE INTO DE
  call drawtile                     ; DRAW TILE TO SCREEN LOCATION IN BC

ifdef debugscroll
  ; SHOW LOCATION 1,1 TILE FOR SCROLL WRAP
  ld hl,0
  ld a,1
  ld bc,scr_addr;&c000
  call getspritedata                ; GET SPRITE FROM TABLE INTO DE
  call drawtile                     ; DRAW TILE TO SCREEN LOCATION IN BC
endif
ifdef debuglock
  ; SHOW LOCATION FOR MISSILE LOCK
  ld hl,(enemylandlocationlock)
  call convspritelocationtopixelscrtc   ; CONVERT HL CHAR LOCATION TO BC PIXEL LOCATION
  ld a,1
  call getspritedata                ; GET SPRITE FROM TABLE INTO DE
  call drawtile                     ; DRAW TILE TO SCREEN LOCATION IN BC
endif
ret

anddrawsprite:
  call convspritelocationtopixelscrtc   ; CONVERT HL CHAR LOCATION TO BC PIXEL LOCATION
  call getspritedata                    ; GET SPRITE FROM TABLE INTO DE
  jp anddrawtile                        ; DRAW TILE TO SCREEN LOCATION IN BC

drawspritesplitscreen:
  call convspritelocationtopixelssplittable   ; CONVERT HL CHAR LOCATION TO BC PIXEL LOCATION
  call getspritedata                          ; GET SPRITE FROM TABLE INTO DE
  jp drawtile                                 ; DRAW TILE TO SCREEN LOCATION IN BC

; ONLY USED FOR EDGE OF TILEMAP DRAWING
drawspritecheckifsky3a:
  ld b,a                            ; STORE TILE ID IN B
  call getskytilemapid              ; GET TILE GRID PTR IN DE
  ld a,(de)
  cp 1;or a                         ; SKIP IF JUST EMPTY SKY
  ret z
drawspritecheckifsky3:              ; SKIP TILE ID LOOKUP IF WE ALREADY HAVE IT
  ld a,c                            ; LOAD OBJECT ID INTO TILE GRID
  ld (de),a
  ld a,b                            ; RESTORE TILE ID IN A
  call convspritelocationtopixelscrtctable   ; CONVERT HL CHAR LOCATION TO BC PIXEL LOCATION
  call getspritedata                ; GET SPRITE FROM TABLE INTO DE
ifndef debugscroll
  jp drawtile
endif
ifdef debugscroll

  call drawtile                     ; DRAW TILE TO SCREEN LOCATION IN BC
  ; SHOW LOCATION 1,1 TILE FOR SCROLL WRAP
  ld hl,0
  ld a,1
  ld bc,scr_addr;&c000; call convspritelocationtopixelscrtc
  call getspritedata                ; GET SPRITE FROM TABLE INTO DE
  jp drawtile                       ; DRAW TILE TO SCREEN LOCATION IN BC
endif

enemyplanelastlocationde: defw 0

; INPUTS
; H = Y COORD
; L = X COORD
; A = TILE ID
; C = OBJECT ID TO INSERT INTO TILE GRID
drawplusspritecheckifskyenemyplane:
  push hl
  ; CLEAR LAST POSITION OF ENEMY PLANE ON OBJECT MAP  
  ld hl,(enemyplanelastlocationde)
  push af
  ld a,(hl)
  or a
  jr z,skipcloud1
  ld (hl),1     ; ERASE WITH SKY, IF CLOUDS DON'T OBSCURE
  skipcloud1:
  inc hl
  ld a,(hl)
  or a
  jr z,skipcloud2
  ld (hl),1
  skipcloud2:
  pop af
  pop hl
  push hl
  
  push bc
  push af
  ld b,a                            ; STORE TILE ID IN B
  call getskytilemapid              ; GET TILE GRID PTR IN DE
  
  ld (enemyplanelastlocationde),de  ; STORE LAST POS OF ENEMY PLANE SO WE CAN ERASE IT AGAIN.
  
  push de
  ld a,(de)
  or a                              ; SKIP IF CLOUDS OBSCURE
  call z,hideplussprite
  or a
  jr z,skipmovesprite
  
  ld a,c                            ; LOAD OBJECT ID INTO TILE GRID
  ld (de),a
  ld a,b                            ; RESTORE TILE ID IN A
  call moveplussprite2
  skipmovesprite:
  pop de
  pop af
  pop bc
  pop hl

  ; MOVE TO NEXT PART OF SPRITE  
  inc l
  inc a
  
  ld b,a                            ; STORE TILE ID IN B
  inc de                            ; GET TILE GRID PTR IN DE
  ld a,(de)
  or a                              ; SKIP IF CLOUDS OBSCURE
  jp z,hideplussprite
  ld a,c                            ; LOAD OBJECT ID INTO TILE GRID

  ld (de),a
  ld a,b                            ; RESTORE TILE ID IN A
  jp moveplussprite2
  
; DISPLAY PLUS SPRITE THAT IS JUST SINGLE BLOCK
drawplusspritesinglecheckifsky:
  ld b,a                            ; STORE TILE ID IN B
  ;dec l                             ; ADJUST FOR CRTC
  call getskytilemapid              ; GET TILE GRID PTR IN DE
  ld a,(de)
  or a                              ; SKIP IF CLOUDS OBSCURE
  jp z,hideplussprite
  ;or a
  ;jr z,skipmovesprite2
  ld a,c                            ; LOAD OBJECT ID INTO TILE GRID
  ld (de),a
  ld a,b                            ; RESTORE TILE ID IN A
  jp moveplussprite2
  
; INPUTS
; H = Y COORD
; L = X COORD
; A = FIRST SPRITE ID
; C = OBJECT ID TO INSERT INTO TILE GRID
drawplusspritecheckifsky:
  push hl
  push bc
  push af
  ld b,a                            ; STORE TILE ID IN B
  call getskytilemapid              ; GET TILE GRID PTR IN DE
  push de
  ld a,(de)
  or a                              ; SKIP IF CLOUDS OBSCURE
  call z,hideplussprite
  or a
  jr z,skipmovesprite2
  
  ld a,c                            ; LOAD OBJECT ID INTO TILE GRID
  ld (de),a
  ld a,b                            ; RESTORE TILE ID IN A
  call moveplussprite2
  skipmovesprite2:
  pop de
  pop af
  pop bc
  pop hl

  ; MOVE TO NEXT PART OF SPRITE  
  inc l
  inc a
  
  ld b,a                            ; STORE TILE ID IN B
  inc de;call getskytilemapid2      ; GET TILE GRID PTR IN DE
  ld a,(de)
  or a                              ; SKIP IF CLOUDS OBSCURE
  jp z,hideplussprite
  ld a,c                            ; LOAD OBJECT ID INTO TILE GRID

  ld (de),a
  ld a,b                            ; RESTORE TILE ID IN A
  jp moveplussprite2
 
; WE NEED TO ADJUST THE ACTUAL TILE LOCATION TO DRAW TO THE SCREEN
; FOR THE PLAYER PLANE SPRITE
; SCROLLING BY COPYING SCREEN MEMORY LEAVES CHARACTER LOCATIONS UNALTERED
; AND SO THE TRAIL IS ALWAYS ONE CHARACTER BEHIND
; SCROLLING BY CRTC ALTERS CHARACTER LOCATIONS BY ONE CHARACTER
; SO THE TRAIL IS THE CURRENT LOCATION
; SO WE ADJUST THE LOCATION BEFORE DRAWING TO THE SCREEN 

; INPUTS
; H = Y COORD
; L = X COORD
; A = TILE ID
; C = OBJECT ID TO INSERT INTO TILE GRID
;drawspritecheckifsky_crtc_adjusted:
;  ld b,a                                ; STORE TILE ID IN B
;  call getskytilemapid                  ; GET TILE GRID PTR IN DE
;  ld a,(de)
;  or a                                  ; SKIP IF JUST EMPTY SKY
;  ret z
;  ld a,c                                ; LOAD OBJECT ID INTO TILE GRID
;  ld (de),a
;  ld a,b                                ; RESTORE TILE ID IN A
;  drawsprite_crtc_adjusted:
;  dec l                                 ; ADJUST X COORD FOR CRTC SCROLL                           
;  call convspritelocationtopixelscrtc   ; CONVERT HL CHAR LOCATION TO BC PIXEL LOCATION
;  call getspritedata                    ; GET SPRITE FROM TABLE INTO DE
;  jp drawtile                           ; DRAW TILE TO SCREEN LOCATION IN BC

; DRAW A TILE
; INPUTS
; DE = SPRITE DATA
; BC = PIXEL LOCATION

; LOOP IS 90 T STATES

drawtile:
  ; HL IS PIXEL LOCATION
  ; DE IS SPRITE DATA
  ld h,b
  ld l,c
  ld a,(de)
  ld (hl),a
  inc de
  inc l
  ld a,(de)
  ld (hl),a
  inc de  
  dec l
  call scr_next_line_hl
  ld a,(de)
  ld (hl),a
  inc de
  inc l
  ld a,(de)
  ld (hl),a
  inc de  
  dec l
  call scr_next_line_hl
  ld a,(de)
  ld (hl),a
  inc de
  inc l
  ld a,(de)
  ld (hl),a
  inc de  
  dec l
  call scr_next_line_hl
  ld a,(de)
  ld (hl),a
  inc de
  inc l
  ld a,(de)
  ld (hl),a
  inc de  
  dec l
  call scr_next_line_hl
  ld a,(de)
  ld (hl),a
  inc de
  inc l
  ld a,(de)
  ld (hl),a
  inc de  
  dec l
  call scr_next_line_hl
  ld a,(de)
  ld (hl),a
  inc de
  inc l
  ld a,(de)
  ld (hl),a
  inc de  
  dec l
  call scr_next_line_hl
  ld a,(de)
  ld (hl),a
  inc de
  inc l
  ld a,(de)
  ld (hl),a
  inc de  
  dec l
  call scr_next_line_hl
  ld a,(de)
  ld (hl),a
  inc de
  inc l
  ld a,(de)
  ld (hl),a
ret


; DRAW A TILE, BUT USE AND TO CANCEL OUT EXISTING PENS
; INPUTS
; DE = SPRITE DATA
; BC = PIXEL LOCATION

; LOOP IS 90 T STATES
anddrawtile:
  ; HL IS PIXEL LOCATION
  ; DE IS SPRITE DATA
  ld h,b
  ld l,c
  ld a,(de)
  ld b,(hl)
  and b
  ld (hl),a
  inc de
  inc l
  ld a,(de)
  ld b,(hl)
  and b
  ld (hl),a
  inc de  
  dec l
  call scr_next_line_hl
  ld a,(de)
  ld b,(hl)
  and b
  ld (hl),a
  inc de
  inc l
  ld a,(de)
  ld b,(hl)
  and b
  ld (hl),a
  inc de  
  dec l
  call scr_next_line_hl
  ld a,(de)
  ld b,(hl)
  and b
  ld (hl),a
  inc de
  inc l
  ld a,(de)
  ld b,(hl)
  and b
  ld (hl),a
  inc de  
  dec l
  call scr_next_line_hl
  ld a,(de)
  ld b,(hl)
  and b
  ld (hl),a
  inc de
  inc l
  ld a,(de)
  ld b,(hl)
  and b
  ld (hl),a
  inc de  
  dec l
  call scr_next_line_hl
  ld a,(de)
  ld b,(hl)
  and b
  ld (hl),a
  inc de
  inc l
  ld a,(de)
  ld b,(hl)
  and b
  ld (hl),a
  inc de  
  dec l
  call scr_next_line_hl
  ld a,(de)
  ld b,(hl)
  and b
  ld (hl),a
  inc de
  inc l
  ld a,(de)
  ld b,(hl)
  and b
  ld (hl),a
  inc de  
  dec l
  call scr_next_line_hl
  ld a,(de)
  ld b,(hl)
  and b
  ld (hl),a
  inc de
  inc l
  ld a,(de)
  ld b,(hl)
  and b
  ld (hl),a
  inc de  
  dec l
  call scr_next_line_hl
  ld a,(de)
  ld b,(hl)
  and b
  ld (hl),a
  inc de
  inc l
  ld a,(de)
  ld b,(hl)
  and b
  ld (hl),a
ret



; INPUTS
; H = Y POS
; L = X POS
drawgauge:
  ld b,15 ; LENGTH &0f
  drawgaugeloopsplitscreen:
    ld a,7;&07          ; DRAW EMPTY GAUGE SPRITE
    push bc
    push hl
    call drawspritesplitscreen
    pop hl
    inc l
    pop bc
  djnz drawgaugeloopsplitscreen
  ld a,8;&08
  jp drawspritesplitscreen       ; DRAW GAUGE MARKER

; INPUTS
; DE = SPRITE BLOCK
; HL = Y X LOCATION
; BC = HEIGHT/WIDTH??
drawspriteblock:
;  ret
  push bc
  ld a,(de)
  or a
  jr z,skipemptytile
  push de
  push hl
  call drawspritecheckifsky
  pop hl
  pop de
  skipemptytile:
  inc de
  inc h
  pop bc
  djnz drawspriteblock
ret

; BELOW FUNCTION ONLY USED FOR EDGE OF SCREEN GRAPHICS
; INPUTS
; DE = SPRITE BLOCK
; HL = Y X LOCATION
; BC = HEIGHT/WIDTH??
drawspriteblock3:
  push bc
  ld a,(de)
  or a
  jr z,skipemptytile3
  push de
  push hl
  call drawspritecheckifsky3a
  pop hl
  pop de
  skipemptytile3:
  inc de
  inc h
  pop bc
  djnz drawspriteblock3
ret

docountdowntoenemyship:
  ld hl,(enemyshiptimer)
  ld a,(gamelevelprogress)
  or a
  jr nz,l90c4
  call drawseatiles;l90ad

  ; COUNT DOWN TO ENEMY SHIP SPRITE
  ld de,countdownenemyship ; &0032 
  sbc hl,de
  ret c
  
  ; DISPLAY ENEMY SHIP
  call movegunshiponscreen
  
  ld hl,enemyshipsprite
  ld (spriteptr),hl
  xor a
  inc a
  ld (gamelevelprogress),a
ret

countdownenemyship equ 50
countdownclouds    equ 80
countdownstartland equ 100  
countdownbuildtown equ 200
countdownfrigate   equ 250
countdownlanding   equ 270

drawseatiles:
;  ret
  push hl
  ld hl,&0f27 ; Y X LOCATION (RIGHT MOST TILE)
  ld c,2      ; OBJECT TILE ID
  ld a,r      ; RANDOMISE SEA TILE BASED ON R REGISTER
  and #03     ; ADD 3 BASE TILES TO NUMBER WE GET
  add 3
  call drawspritecheckifsky3a       
  pop hl
  ld a,countdownclouds;80;&50
  ld (cloudseapalettechangecount),a ; COUNTDOWN TO CLOUDS - MUST BE PAST SEA AS USES SAME PEN
ret

; DATA?
cloudseapalettechangecount: defb 5
iscloudpalette:             defb 0

l90c4:  
  dec a
  jr nz,startoffalklandisland
  call drawseatiles

  ld de,(spriteptr)
  ld a,(de)
  inc a
  jr nz,insertenemyshipinobjectmaponly;insertenemyship

  ld a,(gamelevelprogress)
  dec a
  ret nz

  ; START POSITION OF EXOSET MISSILE
  ld hl,&0d24              ; Y X LOCATION
  ld (enemymissilelocation),hl
  ld a,1;&01
  ld (enemymissilestatus),a
  inc a
  ld (gamelevelprogress),a
  ret

  ; START POSITION OF ENEMY SHIP

;  insertenemyship:
;  ld hl,&0c27              ; Y X LOCATION
;  ld b,3;&03
;  ld c,7;&07
;  call drawspriteblock3
;  ld (spriteptr),de
;ret
insertenemyshipinobjectmaponly:
  ld hl,&0c27              ; Y X LOCATION
  ld b,3
  ld c,7 ; OBJECT ID
  call insertspriteinobjectmaponly
  ld (spriteptr),de
ret

; DATA
enemyshipsprite:
  defb &00,&00,&14,&00,&12,&15
  defb &00,&13,&16,&00,&00,&17,&ff

startoffalklandisland:
  dec a
  jr nz,l9134
  call drawseatiles
  ld de,countdownstartland;&0064                ; NUMBER OF TILES TO COUNT AFTER ENEMY BOAT BEFORE STARTING LAND
  sbc hl,de
  ret c

  ld hl,0                    ; STARTING LAND
  ld (enemyshiptimer),hl
  ld a,3
  ld (gamelevelprogress),a
  ld a,&0e
  ld (l885d),a

  ld hl,&0f27                ; SET X Y
  ld c,3                     ; LAND OBJECT ID
  ld a,1                     ; DRAW SEA/LAND JOIN
  call drawspritecheckifsky
  ld hl,&0e27                ; SET X Y
  ld c,3                     ; LAND OBJECT ID
  ld a,24                    ; DRAW HILL UP TILE FOR SEA/LAND JOIN
  jp drawspritecheckifsky

l9134:
  dec a
  jp nz,l9206
  ld a,(l8859)
  rra
  rra
  and #03
  ld (l8860),a
  jr nz,l9167

drawflatterrain:
  ; DRAW FLAT TERRAIN TILE
  ld a,(l885d)
  ld h,a
  ld a,r  ; RANDOMISED GRASS TYPE BASED ON R REGISTER
  and 3
  add #20 ; ADD 32 TILES TO NUMBER WE GET

  ; DRAW SURFACE OF TERRAIN TILES
  l914e:
  ld l,&27
  push hl
  ld c,3   ; LAND OBJECT ID
  call drawspritecheckifsky3a
  pop hl
  ld a,&0f
  sub h
  ld b,a
  inc h
  ; FILL IN MAIN TERRAIN BLOCKS WITH GREEN
  ld de,solidlandspriteblock
  ld c,3
  call drawspriteblock3
  jp l91e3 

l9167:
  dec a          ; COUNTDOWN TO HILLS ?
  jr nz,l9181
  l916a:
  ld a,r         ; RANDOMISE HILL ?
  rra
  and #03
  add #1c
  ld hl,(l885c)
  ld l,a
  ld a,h
  cp #0e
  jr z,drawflatterrain
  inc a
  ld (l885d),a
  ld a,l
  jr l914e

l9181:
  dec a           ; COUNTDOWN TO ...
  jr nz,checkinsertenemylandtile     ; OR ELSE INSERT ENEMY LAND TILE
  ld a,(l8859)
  rra
  and 3
  add #18
  ld hl,(l885c)
  ld l,a
  ld a,(leveldifficulty)
  ld d,a
  ld a,5
  sub d
  add 7
  cp h
  ld a,h
  jr z,drawflatterrain
  dec a
  dec h
  ld (l885d),a
  ld a,l
  jr l914e
  
checkinsertenemylandtile:
  ld a,(l8861)
  and 1
  jr z,insertenemylandtile
  xor a
  ld (l8860),a
  jr drawflatterrain


; ONLY UPDATE MISSILE LOCK IF WE ARE NOT CURRENTLY FIRING MISSILE
; STOPS MISSILE LOSING LOCK ON OBJECT AFTER FIRING
; INPUT
; HL = NEW LOCK FOR LAND MISSILE
updateenemylandlocationlock:
  ld a,(iy+0);(playermissilestatus)
  or a
  ret nz
  push hl
  inc h  ; START FROM 0, NOT 1
  ;dec l  ; START FROM 0, NOT 1
  ld (enemylandlocationlock),hl
  ; RESET STATUS FOR WINGMAN BOMBING RUNS
  ; IF STATUS IS 1, THEN WINGMAN HAS CHOSEN TO IGNORE THIS TARGET
  ; IF STATUS IS 2, THEN HE IS ON HIS WAY TO BOMB IT
  xor a                            
  ld (enemylandlocationlock+2),a
  pop hl
ret
enemylandlocationlock:
  defb 0  ; X LOCATION
  defb 0  ; Y LOCATION
  defb 0  ; WINGMAN BOMBING RUN STATUS - 0 = NOT SET, 1 = SKIP, 2 = BOMB TARGET

scrollenemylandlocationlock:
  ld a,(enemylandlocationlock)
  or a   ; DON'T SCROLL IF NO ENEMY VISIBLE
  ret z
  dec a
  cp 5;1
  ;cp 15  ; DON'T SCROLL IF PAST PLAYER PLANE
  jr z,resetenemylandlocationlock
  ld (enemylandlocationlock),a
ret
; CLEAR LOCK AT START OF LEVEL
resetenemylandlocationlock:
  ld bc,0
  ld (enemylandlocationlock),bc
  xor a
  ld (enemylandlocationlock+2),a
ret

scrollwingmanpoweruplocation:
  ; CHECK IF WE HAVE LOCATIONS TO SCROLL
  ld a,(wingmanpowerupstatus)
  or a
  ret z
  ; SCROLL WITH SCREEN
  
  ld hl,(wingmanpoweruplocation)
  dec l
  ld (wingmanpoweruplocation),hl
  
  ; DISABLE IF SCROLLED OUT OF SCREEN  
  ld a,l
  or a
  jp z,destroywingmanpowerup
  
  ; NEED TO MOVE HARDWARE SPRITE EACH TIME WE SCROLL
  ; DRAW POWERUP
drawwingmanpowerup:
  ld a,6
  ;ld a,49                               ; CHUTE
  ld c,21                                ; WINGMAN POWERUP OBJECT
  jp drawplusspritesinglecheckifsky

scrollcustomwaypoint:
  ld a,(scrollingcustomwaypoint-2)
  or a   ; DON'T SCROLL IF NO ENEMY VISIBLE
  ret z
  dec a
  cp 5;1
  ;cp 15  ; DON'T SCROLL IF PAST PLAYER PLANE
  jr z,resetscrollingcustomwaypoint
  ld (scrollingcustomwaypoint-2),a
ret
; CLEAR LOCK AT START OF LEVEL
resetscrollingcustomwaypoint:
  ld bc,0
  ld (scrollingcustomwaypoint-2),bc
ret


;debuglock equ 1
ifdef debuglock
; DEBUG SHOW CURRENT LOCK ON SCREEN
clearcurrentenemylandlocationlock:
  ld a,(enemylandlocationlock)
  or a
  ret z
  ld hl,(enemylandlocationlock)
  dec h
  call convspritelocationtopixelscrtc   ; CONVERT HL CHAR LOCATION TO BC PIXEL LOCATION
  ld a,1;0
  call getspritedata                    ; GET SPRITE FROM TABLE INTO DE
  jp drawtile                           ; DRAW TILE TO SCREEN LOCATION IN BC
endif
insertenemylandtile:
  ld a,r                                ; RANDOMIZE ENEMY LAND TILE
  rra
  and #0c
  ld l,a
  rra
  rra
  ld (l884b),a
  add l
  ld l,a
  ld h,0
  ld de,enemylandsprites
  add hl,de
  ex de,hl
  ld a,4
  ld (gamelevelprogress),a
  
  l91cb:
  ld hl,(l885c)
  ld l,&27
  push hl
  dec h
  dec h
  ld bc,&0208                    ; 08 = ENEMY OBJECT ID
 
ifdef debuglock 
  push bc
  push hl
  push de
  call clearcurrentenemylandlocationlock
  pop de
  pop hl
  pop bc
endif
  call updateenemylandlocationlock ; RECORD ENEMY POSITION FOR MAVERICK MISSILES TO LOCK ON
  call checkwingmandobombingrun    ; IF WINGMAN IS FLYING, ALLOW HIM TO BOMB TARGET
  
  call drawspriteblock3
  ld (l885e),de
  pop hl
  ld a,1
  jp l914e

l91e3:
  ld a,(l8860)
  ld (l8861),a
  ld a,(leveldifficulty)
  ld l,0
  ld h,a
  ld de,&012c
  add hl,de
  ex de,hl
  ld hl,(enemyshiptimer)
  sbc hl,de                  ; COUNTDOWN TO ????
  ret c
  ld a,5                     ; DESCEND TO TOWN LEVEL
  ld (gamelevelprogress),a
  ld hl,0
  ld (enemyshiptimer),hl
ret

l9206:
  dec a
  jr nz,checkbuildportstanley
  ld de,(l885e)
  ld a,3
  ld (gamelevelprogress),a
jr l91cb

; BLOCK OF ENEMY SPRITES THAT CAN BE PLACED ON LAND - UP TO 2x2 SQUARE
; SELECTED BY RANDOM
enemylandsprites:
  defb &00,&2a,&00,&00,255 ; RADAR
  defb &00,&2b,&00,&00,255 ; MISSILE LAUNCHER
  defb &00,&2c,&00,&00,255 ; GUN
  defb &00,&2d,&00,&2e,255 ; TANK

; DATA
solidlandspriteblock:
  defb &01,&01,&01,&01,&01,&01,&01,&01
  defb &01,&01,&01

checkbuildportstanley:
  dec a
  jr nz,buildportstanley 
  
  ld a,(l885d) ; CHECK GROUND LEVEL TO SEE IF WE CAN BUILT PORT STANLEY
  cp #0e
  jp nz,l916a
  
  ; SWAP COLOURS TO DO TOWN LIGHTING
  ld hl,(duskpal+6)
  ld (backuppal),hl
  ld hl,(nightpal+6)
  ld (backuppal2),hl
  ld hl,&0FF0
  ld (duskpal+6),hl
  ld (nightpal+6),hl
  
  call startpalettefade
  
  call drawflatterrain
  ld a,6
  ld (gamelevelprogress),a
  ld a,255
  ld (cloudseapalettechangecount),a
  xor a
  ld (iscloudpalette),a  
ret

buildportstanley:
  dec a
  jr nz,checkbuildpier
  ld hl,(enemyshiptimer)
  ld de,countdownbuildtown           ; COUNTDOWN TIMER TO BUILD TOWN
  sbc hl,de
  jp nc,setcloudcolourtosea
  ld a,r                             ; RANDOMISE BUILDING ON R REGISTER
  rra
  rra
  and #07
  add a
  ld hl,townspritestable
  add l
  ld l,a
  jr nc,l9269
  inc h
  l9269:
  ld e,(hl)
  inc hl
  ld d,(hl)
  ld (spriteblockptr),de             ; TOWN BUILDING TO DRAW
  call drawflatterrain
  ld a,7
  ld (gamelevelprogress),a
ret

checkbuildpier:
  dec a
  jp nz,buildpier
  
  ; RESTORE COLOURS TO STOP TOWN LIGHTING
  ld hl,(backuppal)
  ld (duskpal+6),hl
  ld hl,(backuppal2)
  ld (nightpal+6),hl

  ld b,5
  ld hl,&0b27 ; Y X LOCATION
  ld c,9
  ld de,(spriteblockptr)
  call drawspriteblock3
  ld a,3
  ld (tilemapsea),a ; LOAD 3 INTO OBJECT TILEMAP - SEA?
  ld (spriteblockptr),de
  ld a,(de)
  inc a
  ret nz
  ld a,6
  ld (gamelevelprogress),a
ret

; A LOOKUP TABLE OF SPRITE BLOCKS FOR EACH BUILDING TYPE IN THE TOWN
townspritestable:
  defw blk0
  defw blk1
  defw blk2
  defw blk3
  defw blk4
  defw blk5
  defw blk6
  defw blk7

blk0: db #00,#00,#42,#01,#01,#ff
blk1: db #00,#00,#46,#01,#01,#00,#00,#47,#01,#01,#ff
blk2: db #00,#3b,#40,#01,#01,#00,#3c,#3f,#01,#01,#ff
blk3: db #00,#00,#2a,#01,#01,#00,#2a,#41,#01,#01,#3b,#40,#40,#01,#01,#3d,#40,#3f,#01,#01,#3c,#40,#40,#01,#01,#ff
blk4: db #00,#3e,#40,#01,#01,#3b,#40,#3f,#01,#01,#3d,#40,#40,#01,#01,#3c,#40,#40,#01,#01,#00,#3e,#3f,#01,#01,#ff
blk5: db #00,#00,#2d,#01,#01,#00,#00,#2e,#01,#01,#ff
blk6: db #40,#40,#40,#01,#01,#40,#40,#3f,#01,#01,#ff
blk7: db #00,#2b,#40,#01,#01,#00,#3b,#3f,#01,#01,#00,#3c,#40,#01,#01,#00,#2c,#40,#01,#01,#ff

; SPRITE NUMBERS???
pendata: defb "JHIJHIJHIJHI",255

setcloudcolourtosea:
  ld hl,&0e27                       ; Y X LOCATION
  ld c,3                            ; WIDTH
  ld de,solidlandspriteblock        ; BUILD GROUND SLOPE INTO SEA TO LINK TO PIER
  ld b,2                            ; HEIGHT
  call drawspriteblock3
  ld a,8                            ; ADVANCE GAME LEVEL PROGRESS
  ld (gamelevelprogress),a
  ld hl,pendata
  ld (spriteptr),hl

  xor a
  ld (iscloudpalette),a
ret

dobuildsea:
  dec a 
  jr nz,moveendfrigateonscreen
  call drawseatiles
  ld de,countdownfrigate                      ; COUNTDOWN TILES TO END FRIGATE
  sbc hl,de
  ret c
  ld a,(playerfrigatestatus)                  ; DID WE BLOW THE FRIGATE UP?
  or a
  ld a,11                                     ; START APPROACHING LAST FRIGATE
  jr z,beginapproachlandingfrigate
  ld a,14                                     ; WE BLEW UP SHIP AT START, JUST SHOW OPEN SEA
  beginapproachlandingfrigate:
  ld (gamelevelprogress),a
  ld hl,endfrigatesprite
  ld (spriteptr),hl
  jp movesecondharrierlandingfrigate

moveendfrigateonscreen:
  ;call disablescrollsecondgunship
  ;; HIDE ENEMY SHIP
  ;ld b,12
  ;call hideplussprite
  ;inc b
  ;call hideplussprite
  
  dec a
  jr nz,drawremainingseaafterenedfrigate
  call drawseatiles
  call scrollendfrigateonscreenobjectonly
  ld a,(de)
  inc a
  ret nz
  ld a,12                                 ; SLOW DOWN FOR APPROACH
  ld (gamelevelprogress),a
ret

; WE NEED TO SCROLL THE OBJECT MAP OF THE FRIGATE ON THE SCREEN SO WE CAN LAND ON IT
scrollendfrigateonscreenobjectonly:
  ld de,(spriteptr)
  ld a,(de)
  inc a
  jr nz,insertfrigateshipinobjectmap

  ld a,(gamelevelprogress)
  dec a
ret nz

insertfrigateshipinobjectmap:
  ld hl,&0c27              ; Y X LOCATION
  ld b,3
  ld c,4 ; OBJECT ID - OWN SHIP
  call insertspriteinobjectmaponly
  ld (spriteptr),de
ret

insertspriteinobjectmaponly:
  push bc
  ld a,(de)
  or a
  jr z,skipemptytile3a
  push de
  push hl
  call insertspriteblockinobjectmaponly
  pop hl
  pop de
  skipemptytile3a:
  inc de
  inc h
  pop bc
  djnz insertspriteinobjectmaponly
ret

; ONLY USED FOR EDGE OF TILEMAP DRAWING
insertspriteblockinobjectmaponly:
  call getskytilemapid                       ; GET TILE GRID PTR IN DE
  ld a,(de)
  cp 1;or a                                  ; SKIP IF JUST EMPTY SKY
  ret z
  ld a,c                                     ; LOAD OBJECT ID INTO TILE GRID
  ld (de),a
  
  xor a ; EMPTY SKY - SCROLLING THE SEA CAUSES IT TO APPEAR OVER FRIGATE SO WE MUST DRAW SOMETHING
  call convspritelocationtopixelscrtctable   ; CONVERT HL CHAR LOCATION TO BC PIXEL LOCATION
  call getspritedata                         ; GET SPRITE FROM TABLE INTO DE
  jp drawtile                                ; DRAW TILE TO SCREEN LOCATION IN BC

drawremainingseaafterenedfrigate:
  dec a
  jp nz,drawseatiles
  call drawseatiles
  ld de,countdownlanding 
  sbc hl,de                  ; COUNTDOWN TO LANDING ROUTINE
  ret c
  ld a,13                    ; LANDING ON FRIGATE
  ld (gamelevelprogress),a
ret

; FRIGATE REVERSED, SO IT CAN COME IN SCREEN FROM OPPOSITE SIDE
;endfrigatesprite:
;  defb &00,&00,&0b,&00,&00,&02,&00,&00
;  defb &02,&00,&00,&02,&00,&00,&02,&00
;  defb &0d,&02,&10,&0e,&02,&11,&0f,&02
;  defb &00,&00,&02,&00,&09,&02,&00,&0a  
;  defb &02,&00,&00,&02,&00,&00,&0c,&ff
  
; AS ABOVE BUT WITH NO SECOND HARRIER
endfrigatesprite:
  defb &00,&00,&0b,&00,&00,&02,&00,&00
  defb &02,&00,&00,&02,&00,&00,&02,&00
  defb &0d,&02,&10,&0e,&02,&11,&0f,&02
  defb &00,&00,&02,&00,0,&02,&00,0  
  defb &02,&00,&00,&02,&00,&00,&0c,&ff

buildpier:
  dec a
  jr nz,checkbuildmoresea
  call drawseatiles
  ld de,(spriteptr)
  ld a,(de)
  inc de
  ld (spriteptr),de
  cp 255
  jr z,markendofpier
  ld c,9 
  ld hl,&0e27                        ; Y X LOCATION
  jp drawspritecheckifsky3a

markendofpier:
  ld a,9                             ; END OF PIER
  ld (gamelevelprogress),a
  
  call movegunshiponscreen
  
  ld hl,enemyshipsprite
  ld (spriteptr),hl
ret

checkbuildmoresea:
  dec a
  jp nz,dobuildsea
  call drawseatiles
  ld de,(spriteptr)
  ld a,(de)
  inc a
  jr z,marklastenemyshipfiredmissile
  ld hl,&0c27              ; Y X LOCATION
  ld c,7
  ld b,3 ; OBJECT ID - ENEMY SHIP
  ; ENEMY SHIP 
  call insertspriteinobjectmaponly
  ld (spriteptr),de
ret

marklastenemyshipfiredmissile:
  ld a,10                       ; ENEMY SHIP FIRED MISSILE
  ld (gamelevelprogress),a
  ld a,(enemymissilestatus)
  or a
  ret nz
  ld hl,&0d24                   ; Y X LOCATION
  ld (enemymissilelocation),hl
  inc a
  ld (enemymissilestatus),a
ret

; SET BLUE SEA INK TO WHITE FOR CLOUDS

drawrandomcloudsprite:
  ld a,(l885a)
  or a
  jr nz,drawcloudsprite

  ld a,(cloudseapalettechangecount)
  or a
  jr z,skipsetcloudpalette

  dec a
  ld (cloudseapalettechangecount),a
  ret nz

  ; SET SEA COLOUR INK TO WHITE FOR CLOUDS
  ld a,1
  ld (iscloudpalette),a  
  ret

  skipsetcloudpalette:
  ld a,(currtime)
  and #70
  ret nz
  ld a,(l8859)
  ld de,cloudspriteblock1
  bit 3,a
  jr nz,l944c
  ld de,cloudspriteblock2
  l944c:
  ld (l885b),de
  and #03
  ld (l885a),a
ret

drawcloudsprite:
  ld h,a         ; SET LOCATION
  ld l,&27       ; RIGHT SIDE OF SCREEN
  ld de,(l885b)
  ld a,(de)
  inc a
  jr nz,l9465
  ld (l885a),a
  ret            ; REACHED END OF CLOUD BLOCK

  l9465:
  ld b,3         ; SET SIZE
  ld c,0         ; CLOUD OBJECT ID
  call drawspriteblock3
  ld (l885b),de  ; GET LOCATION IN CLOUD SO WE KNOW WHEN TO STOP GENERATING IT
ret

; DATA
cloudspriteblock1:
  defb &00,&4d,&50,&00,&4e,&51,&00,&4f
  defb &52,&00,&53,&55,&00,&54,&56,&ff
cloudspriteblock2:
  defb &00,&4d,&50,&00,&4e,&51,&00,&4f  
  defb &52,&57,&5b,&5d,&58,&03,&5e,&59
  defb &03,&5f,&5a,&5c,&60,&53,&55,&00
  defb &54,&56,&00,&ff

launchflakattack:
  ld a,(playerstatus) ; ARE WE STILL ALIVE?
  or a
  ret nz

  ld hl,l8864
  ld a,(l884b)
  inc a
  bit 1,a
  jr z,l94bc
  ld a,4
  ld (hl),a
  ld (l884b),a
  ld a,(l885d)
  dec a
  dec a
  dec a
  ld (l884c),a
  
  l94bc:
  ld a,(hl)
  or a
  ld a,(gamelevelprogress)
  jr nz,l94cd

  l94c3:
  cp 7                          ; GENERATING BUILDING
  ret nz
  ld a,10
  ld (l884c),a
  jr l94d2

  l94cd:
  cp 3                          ; DOING LAND
  jr nz,l94c3
  dec (hl)

  l94d2:
  ld a,(l8859)
  rlca
  rlca
  rlca
  rlca
  and #0f
  ld h,a
  ld a,(l884c)
  cp h
  ret c
  ld l,32
  call getskytilemapid
  ld a,(de)
  dec a                           ; CP 1 - TILE IS SKY
  ret nz
  ld b,57                         ; FLAK SPRITE
  ld a,r
  bit 0,a
  jr z,drawflaksprite
  inc b                           ; RANDOMLY CHOOSE SECOND FLAK SPRITE
  drawflaksprite:                 ; DRAW FLAK
  ld a,b
  ld c,10                         ; FLAK OBJECT ID
  call drawspritecheckifsky
  jp doflaknoise

; ENEMY AIRPLANE MISSILE
launchmissile:
  ld a,(enemymissilestatus)
  or a
  ret z
  dec a                           ; CP 1
  jr nz,movemissile               ; IF LAUNCHED, MOVE MISSILE
  call heatseekposition
  ld c,11                         ; OBJECT ID
  call drawspritecheckifsky       ; DRAW MISSILE POINTING DOWN LEFT
  ld a,2
  ld (enemymissilestatus),a
ret

movemissile:
  call eraseenemymissiletrail
  call heatseekposition 
  ld c,11                         ; MISSILE OBJECT ID
  jp drawspritecheckifsky         ; MOVE MISSILE

eraseenemymissiletrail:
  ld hl,(enemymissilelocation)
  dec l
  ld a,0:contrail1
  ld c,1                          ; SKY OBJECT ID
  jp drawspritecheckifsky;_crtc_adjusted         ; ERASE MISSILE TRAIL

heatseekposition:
  ld hl,(enemymissilelocation)
  dec l
  jr z,disableenemymissile                      ; ERASE MISSILE IF OFF SCREEN - DOUBLED TO ACCELERATE TOWARDS PLAYER
  dec l
  jr z,disableenemymissile                      ; ERASE MISSILE IF OFF SCREEN?
  ld b,0
  ld a,(playerstatus)
  or a
  jr nz,l9568                                   ; IF PLAYER ALIVE, SEEK PLAYER?
  
  ; TARGET PLAYER OR WINGMAN BASED ON WHICH TARGET ENEMY PLANE HAS ALREADY SELECTED 
  ld a,(missiletargetwingman)
  cp 2
  jr z,enemymissiletargetwingman
  ld de,(currentplayerlocation)
  jr continuetargetenemymissile2
  enemymissiletargetwingman:
  ld de,(wingmanlocation)
  
  continuetargetenemymissile2:
  ld a,l
  sub e
  ld b,55                                       ; MISSILE SPRITE ID POINTING UP LEFT
  jr c,l9568
  cp 5
  jr c,l9568
  ld a,d
  cp h
  jr z,l955e
  jr c,l955d
  inc h
jr l955e

l955d:
  dec h
  l955e:
  cp h
  jr z,l9568
  jr c,launchheatseekingmissile
  dec b
jr l9568

; ENEMY SHIP LAUNCHES MISSILE
launchheatseekingmissile:
  push hl
  push bc
  push de
  call domissilenoise
  pop de
  pop bc
  pop hl

  ; AIM MISSILE SPRITE IN DIRECTION IT IS FLYING

  ld b,53   ; SPRITE ID FOR MISSILE POINTING NORTH WEST
  l9568:
  ld (enemymissilelocation),hl
  push de
  call getskytilemapid
  ld a,(de)
  pop de
  ld c,a
  ld a,55   ; SPRITE ID FOR MISSILE POINTING WEST
  cp b
  jr nz,aimheatseakingmissile
  ld a,c
  
  ; MY OWN ROUTINE FOR CHECKING IF WINGMAN IS HIT BY ENEMY MISSILE
  cp 20     ; ENEMY MISSILE HIT WINGMAN
  jr nz,skipmissilehitwingman
  call wingmandestroyed
  jr killenemymissile
  
  skipmissilehitwingman:
  cp 12     ; ENEMY MISSILE HIT PLAYER 1 HARRIER
  jr nz,aimheatseakingmissile

  ; MISSILE HIT PLAYER 1 HARRIER
  call checkplayerplanemovement
  ld a,1:deathstatus1
  ld (playerstatus),a
  
  killenemymissile:
  call doexplosionnoise
  disableenemymissile:
  xor a
  ld (enemymissilestatus),a
  pop hl
ret

dowingmandestroyed2:
  call wingmandestroyed
  jr killenemymissile
dopowerupdestroyed2:
  call destroywingmanpowerup
  jr killenemymissile

; CHECK ENEMY MISSILE AGAINST WHAT OBJECT IT HIT
; INPUT
; B = 
; C = OBJECT ID
aimheatseakingmissile:
  ld a,c
  cp 1                   
  jr z,l95a6                 ; HIT AIR
  or a                  
  jr z,l95a6                 ; HIT CLOUD 
  cp 10                 
  jr z,l95a6                 ; HIT FLAK
  cp 5
  jr z,killmissile2          ; HIT PLAYER MISSILE
  cp 6
  jr z,killbomb              ; HIT BOMB
  cp 20
  jr z,dowingmandestroyed2   ; HIT WINGMAN
  cp 21
  jr z,dopowerupdestroyed2   ; HIT POWER UP

  call drawsmokesprite       ; ANYTHING ELSE
  jr disableenemymissile     ; MISSILE DESTROYED

killmissile2:
  xor a
  ld (iy+0),a;(playermissilestatus),a
l95a6:
  ld a,b
  or a
ret

killbomb:
  xor a
  ld (iy+15),a;(playerbombstatus),a
  jr l95a6
  
drawsmokesprite:
  push hl
  ld a,&34                 ; DRAW SMOKE 2
  ld c,10
  call drawspritecheckifsky
  pop hl
  dec h
  call getskytilemapid
  ld a,(de)
  dec a
  ret nz
  ld a,51                  ; DRAW SMOKE 1
  ld c,10
  jp drawspritecheckifsky

; BOMB CREATES CRATER IN GROUND
drawholesprite:
  push hl
  ld a,97                  ; DRAW HOLE
  call anddrawsprite
  pop hl
  ret

; HL = Y X LOCATION
drawemptyskytile: 
  ld c,1 ; SKY OBJECT ID
  xor a
  jp drawspritecheckifsky_crtc_adjusted

spawnid: defb 0

launchenemyplane:
  ld a,(enemyplanestatus)
  or a
  jp nz,moveenemyplaneapproach
  
  ld a,(gamelevelprogress)
  cp 2
  ret c
  cp 8
  ret nc
  
  ld a,(enemymissilestatus)       ; IS MISSILE STILL ACTIVE? IF YES, DON'T LAUNCH PLANE
  or a
  ret nz
  
  ld a,(playerstatus)             ; ARE WE STILL ALIVE? IF NO, DON'T LAUNCH PLANE
  or a
  ret nz
   
  ld hl,(currentplayerlocation)   ; TRY TO SPAWN CLOSE TO TOP OF SCREEN - BASED ON DIFFICULTY LEVEL?
  ld a,(leveldifficulty)
  ld c,a
  ld a,11
  sub c
  ld c,a
  ld a,h
  cp c
  ret nc
  
  ld a,r                           ; RANDOMIZE SPAWN TIME?
  and #0f
  ret nz
  
  ld a,(currtime)                  ; RANDOMIZE ALTITUDE ENTRY POINT
  and 3;
  ld h,a                           ; MAKE ENEMY PLANE APPEAR ON SCREEN
  ld l,&26                         ; RIGHT HAND BORDER LOCATION

  ; ------------------------------
  ;  SPAWN WINGMAN POWERUP
  
  ld a,(wingmanpowerupstatus)
  or a
  jr nz,spawnenemyplane            ; DON'T SPAWN POWERUP AS WE ALREADY HAVE MOVING POWERUP
    
  ; SPAWN WINGMAN POWERUP
  ; CHECK IF WINGMAN HAS BEEN KILLED, IF SO, SOMETIMES SPAWN WINGMAN POWERUP

  ld a,(wingmantakeoff)
  cp 254                           ; WINGMAN KILLED
  jr z,spawnwingmanpowerup;spawnenemyplane  	
	
  ;ld b,r                          ; DON'T GENERATE POWERUP ALL THE TIME, DO ENEMY PLANE INSTEAD
  ld a,r
  sub 100
  jr c,spawnenemyplane

  ld a,(spawnid)
  inc a
  ld (spawnid),a  
  cp 1
  jr z,spawnhealthpowerup
  cp 2
  jr z,spawnrocketspowerup
  cp 3
  jr z,spawnbombspowerup
  cp 4
  jr z,spawnrocketspowerup
  cp 5
  jr z,spawnbombspowerup
  xor a
  ld (spawnid),a  
  jr spawnenemyplane
  
  ; ------------------------------
  ;  SPAWN ENEMY PLANE

  spawnenemyplane:
  ld (enemyplanelocation),hl
  call setenemyplanenormalsprite   ; MAKE SURE WE DISPLAY UNBROKE PLANE
  ld a,4                           ; ENEMY PLANE FRONT
  ld c,&0d
  call drawplusspritecheckifsky

  ld a,1                           ; ON APPROACH
  ld (enemyplanestatus),a
  
  ; ------------------------------
  ;  WINGMAN INTERCEPT CODE BELOW
  
  ld a,(wingmantakeoff)
  cp 1                             ; IF WE AREN'T TAKEN OFF, OR CRASHED, OR DOING SOMETHING ELSE
  ret nz
  
  ; IF ENEMY PLANE HAS TARGETTED WINGMAN, FORCE WINGMAN TO INTERCEPT
  ld a,(missiletargetwingman)
  cp 1
  jr z,skipwingmanforcedintercept
  ld a,5
  ld (wingmantakeoff),a           
  
  ; OTHERWISE ALLOW WINGMAN TO TRACK ENEMY PLANE - SOMETIMES!
  skipwingmanforcedintercept:
  ld a,r
  and %00000001
  or a
  ret z
  ld a,5
  ld (wingmantakeoff),a            
ret

; SPAWN WINGMAN POWER UP
; INPUT
; HL = LOCATION
spawnwingmanpowerup:
  ld a,1                            ; WINGMAN POWERUP
  call setpalettewingmanpowerup
  dospawnpowerup:
  ld (wingmanpowerupstatus),a
  ld (wingmanpoweruplocation),hl
  ; MAKE SURE WE HAVE RIGHT IMAGERY
  setupparachutesprite:
  ; SPRITE 6
  ld hl,sprite_pixel_data21
  ld de,&4600
  ld bc,128
  ldir
  ex de,hl
  ld bc,128
  jp clearmem

spawnhealthpowerup:
  ld a,2                            ; HEALTH POWERUP
  call setpalettehealthpowerup
  jr dospawnpowerup
spawnrocketspowerup:
  ld a,3                            ; ROCKETS POWERUP
  call setpaletterocketsowerup
  jr dospawnpowerup  
spawnbombspowerup:
  ld a,4                            ; ROCKETS POWERUP
  call setpalettebombspowerup
  jr dospawnpowerup  
  
  ; PEN -GBR
setpalettebombspowerup:
  ld bc,&00f0
  jr dosetpalettepowerup
setpaletterocketsowerup:
  ld bc,&000f
  jr dosetpalettepowerup
setpalettehealthpowerup:
  ld bc,&0ff0
  jr dosetpalettepowerup
setpalettewingmanpowerup:
  ld bc,&0f00
  dosetpalettepowerup:
  ld (&6422+28),bc
  ret

wingmanpowerupspeed: defb 0
movewingmanpowerup:
  ; CHECK IF WE HAVE POWERUP TO MOVE
  ld a,(wingmanpowerupstatus)
  or a
  ret z
  
  ; CHECK COLLISION EVEN IF WE HAVEN'T MOVED
  ld hl,(wingmanpoweruplocation)
  inc h
  call getskytilemapid              ; SEE IF WE HIT LAND OR OTHER OBJECT
  ld a,(de)                         
  or a                              ; HIT CLOUD
  jr z,movewingmanpowerup2
  cp 1                              ; SKY
  jr z,movewingmanpowerup2
  cp 10                             ; IGNORE FLAK
  jr z,movewingmanpowerup2  
  cp 12                             ; HIT PLAYER HARRIER
  jp z,checkactivatepowerup        
  cp 20                             ; IGNORE WINGMAN
  jr z,movewingmanpowerup2  
  ; HIT OTHER OBJECT - DESTROY WINGMAN POWERUP OBJECT
  destroywingmanpowerup:
  ; ERASE WAKE
  ld hl,(wingmanpoweruplocation)
  xor a
  ld c,1
  call drawspritecheckifsky_crtc_adjusted
  
  ld hl,0
  ld (wingmanpoweruplocation),hl
  xor a
  ld (wingmanpowerupstatus),a
  ld (wingmanpowerupspeed),a
  ld b,6
  jp hideplussprite
  
  movewingmanpowerup2:
  ; SLOW DESCENT
  ld a,(wingmanpowerupspeed)
  cp 5;10
  jr z,continuemovewingmanpowerup
  inc a
  ld (wingmanpowerupspeed),a
  ret  
  
  continuemovewingmanpowerup:
  xor a
  ld (wingmanpowerupspeed),a
  
  ; ERASE WAKE
  xor a
  ld c,1
  ld hl,(wingmanpoweruplocation)
  call drawspritecheckifsky_crtc_adjusted
  
  ; MOVE POWERUP
  ld hl,(wingmanpoweruplocation)
  inc h
  ld (wingmanpoweruplocation),hl
ret


moveenemyplaneapproach:
  dec a
  jr nz,enemyplaneretreatafterfire

  ld hl,(enemyplanelocation)
  dec l
  ld (enemyplanelocation),hl
  ld a,4                           ; ENEMY PLANE FRONT
  ld c,13
  push hl
  call drawplusspritecheckifskyenemyplane
  pop hl

  ; CHECK IF WINGMAN FLYING - IF SO TARGET HIM SOMETIMES
  ld a,(missiletargetwingman)
  cp 1
  jr z,skiptargetwingmanenemymissile
  cp 2
  jr z,targetwingmanlocation

  ; NO TARGET SELECTED - CHOOSE PLAYER OR WINGMAN
  ; CHOICE FIXED UNTIL MISSILE IS DESTROYED
  ld a,1
  ld (missiletargetwingman),a ; SET DEFAULT TO TARGET PLAYER
  ld a,(wingmantakeoff)       ; IF WINGMAN IS NOT IN FLIGHT, DEFAULT TO PLAYER
  cp 5                        ; STATUS WINGMAN IS TRACKING PLANE
  jr z,trytotargetwingman
  cp 1                        ; WINGMAN IS IN FLIGHT
  jr nz,skiptargetwingmanenemymissile
  
  trytotargetwingman:
  ld a,r                      ; RANDOMLY CHOOSE 0 OR 1
  and %00000001
  inc a                       ; MAKE RESULT 1 OR 2 - PLAYER OR WINGMAN
  ld (missiletargetwingman),a
  
  targetwingmanlocation:
  ld de,(wingmanlocation)
  jr continuetargetenemymissile
  skiptargetwingmanenemymissile:
  ld de,(currentplayerlocation)
  continuetargetenemymissile:
  ld a,l
  sub e
  cp 10                           ; IF 10 SQUARES AWAY FROM PLAYER
  jr nc,enemyplaneexitscreen      ; EXIT SCREEN
  ld a,2                          
  ld (enemyplanestatus),a         ; FIRE MISSILE STATUS
  
  ld a,(enemymissilestatus)
  or a
  ret nz                          ; IF WE HAVE LAUNCHED MISSILE ALREADY, SKIP THIS
  inc a                           ; LAUNCH MISSILE
  ld (enemymissilestatus),a
  dec l
  ld (enemymissilelocation),hl    ; SET MISSILE JUST AHEAD OF ENEMY PLANE
  jp domissilenoise

; INPUT
; H = HEIGHT OF ENEMY PLANE
; D = HEIGHT OF PLAYER
enemyplaneexitscreen:
  ld a,h
  cp d                                ; IF ON LEVEL WITH PLAYER? KEEP LEVEL
  ret z
  jr c,moveenemyplanedownrow       

  dec h                               ; MOVE ENEMY PLANE UP A ROW 
  jr checkflakobstructionenemyplane
  moveenemyplanedownrow:
  inc h                               ; MOVE ENEMY PLANE DOWN A ROW
  checkflakobstructionenemyplane:
  call getskytilemapid                ; CHECK FOR FLAK OBSTRUCTION - RESULT IN DE
  ld a,(de)
  inc de                             
  or a
  jr z,checksecondaryobstruction      ; NO OBSTRUCTION
  dec a                               ; THERE WAS AN OBSTRUCTION - DON'T MOVE UP A ROW
  ret nz

  checksecondaryobstruction:
  ld a,(de)
  or a
  jr z,updateenemyplaneposition       ; NO OBSTRUCTION, UPDATE PLANE

  dec a
  ret nz
  
  updateenemyplaneposition:
  
  ld (enemyplanelocation),hl

  ld a,4                                  ; ENEMY PLANE FRONT
  ld c,13
  jp drawplusspritecheckifskyenemyplane

enemyplaneretreatafterfire:
  dec a                         
  jr nz,enemyplanecollided

  ld hl,(enemyplanelocation)
  dec l                                   ; MOVE UP A ROW

  jr z,markenemyplaneoffscreen
  ld (enemyplanelocation),hl

  ld a,4                                  ; ENEMY PLANE FRONT
  ld c,13
  push hl
  call drawplusspritecheckifskyenemyplane ; SHOW LEFT AND UP MOVEMENT TO MAKE MOTION BETTER - NEEDED
  pop hl

  bit 0,l
  ret z
  dec h
  inc h
  jr z,markenemyplaneoffscreen            ; CHECK IF OFF SCREEN
  dec h                                   ; MOVE ENEMY PLANE UP A ROW TO EXIT SCREEN
  jr checkflakobstructionenemyplane

  ; ENEMY PLANE HAS FLOWN ABOVE TOP OF SCREEN - ERASE LAST TRAIL OF IT
  markenemyplaneoffscreen:                ; MARK PLANE OFFSCREEN?
  ld b,4
  call hideplussprite
  inc b
  call hideplussprite
  
  xor a                                   ; ENEMY PLANE OFFSCREEN
  ld (enemyplanestatus),a
  ld (missiletargetwingman),a             ; RESET TARGETTING MECHANISM
ret

status_enemyplanecollided equ 4

enemyplanecollided:
  dec a
  jr nz,enemyplanehit
  ld a,status_enemyplanecollided
  ld (enemyplanestatus),a
  
  ld hl,(enemyplanelocation)
  dec l
  ld (enemyplanelocation),hl

  jp setenemyplanebrokesprite

enemyplanehit:
  ld hl,(enemyplanelocation)

  ; ERASE PLANE ON OBJECT MAP SO WE CAN'T CRASH INTO IT STILL
  push hl
  xor a
  ld c,1
  call drawspritecheckifsky
  pop hl
  push hl
  dec l
  xor a
  ld c,1
  call drawspritecheckifsky
  pop hl

  ; PLANE IS GONE
  xor a
  ld (enemyplanestatus),a
  ld (missiletargetwingman),a   ; RESET TARGETTING MECHANISM
  
  ; HIDE THE HARDWARE SPRITE PLANE ONCE IT'S HIT
  ld b,4
  call hideplussprite
  inc b
  jp hideplussprite

;numberofbombs: defb 4
;l8838: 
;defb &10 ; BOMB UNITS?
;defb &25 ; X LOC START AND END?
;l883a: 
;defb 4
;defb 10;&18 ; Y LOC


; INPUT
; IX +0 =
; IX +1 =
; IX +3 = 
decrementgaugelevelcheckredraw:
  dec (ix+0)                  ; 
  ret nz                      ; IF NOT ZERO, DON'T DO ANYTHING?     (DEC COUNTER)
  ld a,(ix+3)                 ; IF IX+0 = ZERO, LOAD IX+3 INTO IX+0 (FILL UP COUNTER AGAIN)
  ld (ix+0),a
  dec (ix+1)                  ; DEC IX+1                            (DEC GAUGE LEVEL)
  jr nz,redrawgaugelevel      ; IF IX+1 = ZERO, REDRAW GAUGE LEVEL  (IF NOT ZERO, REDRAW GAUGE)
  inc (ix+1)                  ; INC IX+1
ret

; DECREMENT GAUGE LEVEL?
; INPUT
; IX +2 = X POS
; IX +4 = Y POS 
redrawgaugelevel:
  ;di
  ld bc,&7fa0 ; PAGE OUT PLUS REGISTERS
  out (c),c
  
  ld l,(ix+2) ; GET X Y LOCATION OF DIAL
  ld h,(ix+4)
  ld a,7  ; SET CLEAR GAUGE SPRITE
  push hl
  call drawspritesplitscreen
  ld a,8  ; SET DIAL GAUGE SPRITE
  pop hl
  dec l
  ld (ix+2),l
  call drawspritesplitscreen
  
  ld bc,&7fb8 ; PAGE IN PLUS REGISTERS
  out (c),c
  ;ei
ret

timercountdown:
  call kl_time_please
  ld de,(currenttime)
  or a                            ; IF WE REACHED 0, RECORD NEW TIMER
  ld (currenttime),hl
  sbc hl,de
  ld b,h
  ld a,h
  or a                            ; IF DIFFERENCE BETWEEN OLD AND NEW TIMES
  jr nz,deductfuel
  ld (currenttime),de
  ret

  deductfuel:
  ld ix,playerfueldrocketsbombs              ; DEC FUEL GAUGE?
  decrementfuelguageloop:
    push bc
    call decrementgaugelevelcheckredraw
    pop bc
  djnz decrementfuelguageloop
ret

checkplayerspeed:
  ld a,(playerfrigatestatus)
  or a
  jr nz,l972e

  ld a,(gamelevelprogress)
  cp 11        ; START OF LANDING FRIGATE
  jr c,l972e
  ld a,255    
  ld e,a
  jr l9753

l972e:
  ld a,(playerstatus)
  or a
  jr nz,makeborderflash

  ld a,(wehavejected)
  cp 2
  jr nc,makeborderflash

  ld e,0
  call testkeyright         ; MOVE PLAYER RIGHT - ACCELERATE
  jr nz,l9746
  inc e
  l9746:
  call testkeyleft          ; MOVE PLAYER LEFT - DECELERATE   
  jr nz,l974f
  dec e
  l974f:
  ld a,e
  or a
  jr z,makeborderflash

  l9753:
  call l97b2
  makeborderflash:
  ld a,(leveldifficulty)
  ld b,a
  ld a,5
  sub b
  rrca
  rrca
  rrca
  inc a
  ld b,a
  srl a
  srl a
  add 4
  ld c,a
  ld a,(playerstatus)
  inc a
  bit 1,a
  jr z,l977c
  push de
  ld a,r ; RANDOM BORDER COLOUR
  ld d,a
  ld e,a
  call scr_set_border
  pop de
  l977c:
    push bc
    call strangedelay
    pop bc
  djnz l977c
  
  ld a,(playerspeed)
  ld b,a
  ld a,&0f 
  sub b 
  rrca
  rrca
  rrca
  rrca
  inc a
  ld b,a
  srl a
  srl a
  add 4
  ld c,a
  ld a,(playerstatus)
  inc a
  bit 1,a
  jr z,l97aa
  push de
  ld a,r ; RANDOM BORDER COLOUR
  ld d,a
  ld e,a
  call scr_set_border
  pop de
  l97aa:
    push bc
    call strangedelay
    pop bc
  djnz l97aa
ret

l97b2:
  ld b,a
  ld a,(playerspeed)
  add b
  inc a
  jr nz,redrawspeedgaugelevel
  ld (playerspeed),a
ret

redrawspeedgaugelevel:  
  dec a
  cp 16                   ; DON'T ALLOW PLANE TO GO TO RIGHT SIDE OF SCREEN
  ret nc
  ld (playerspeed),a
  add 2 
  ld l,a                  ; SET LOCATION OF GAUGE DIAL
  ld h,5;6
  
  ld bc,&7fa0             ; PAGE OUT PLUS REGISTERS
  out (c),c
  
  ld a,8                  ; SET CLEAR GAUGE SPRITE
  
  push de
  push hl
  call drawspritesplitscreen
  pop hl
  pop de
  inc l                   ; MOVE TO NEXT CHAR SPACE
  bit 7,e
  jr nz,l97da             ; DON'T GO PAST BOTTOM OF GAUGE
  dec l
  dec l
  l97da:
  ld a,7                  ; SET DIAL GAUGE SPRITE
  call drawspritesplitscreen
  
  ld bc,&7fb8             ; PAGE IN PLUS REGISTERS
  out (c),c
ret

; TIMED DELAY
strangedelay:
  ld bc,&0032
  ld hl,&0000
  push hl
  pop de
  ldir
ret

lastplayerlocation: defw 0

checkplayerplanemovement:
  ;bit 0,(ix+#01)                               ; DELAY SCROLL FUNCTION BY EVERY OTHER CALL?
  ;ret nz
  ;set 0,(ix+#01)
  ;push hl
  ;push de

  ld a,(playerstatus)                          ; ARE WE STILL ALIVE
  or a
  jr nz,updatemissilelocation

  ; NEED TO ERASE OBJECT MAP PLAYER LEAVES IN WAKE
  ld hl,(currentplayerlocation)
  ld (lastplayerlocation),hl
  ;  ld c,1 ; SKY OBJECT ID
  ;ld a,2;xor a
  ;call drawspritecheckifsky_crtc_adjusted
  ;call drawemptyskytile                        ; ERASE PLAYER PLANE TRAIL BACK
  ;ld hl,(currentplayerlocation)
  ;inc l
  ;  ld c,1 ; SKY OBJECT ID
  ;ld a,2;xor a
  ;call drawspritecheckifsky_crtc_adjusted
  ;call drawemptyskytile                        ; ERASE PLAYER PLANE TRAIL FRONT
  ;ld hl,(currentplayerlocation)
  push hl

  ld a,(flightmode)
  cp 1 
  jr z,pilotejected
  ld a,(wehavejected)
  cp 2 
  jr nc,pilotejected
  call testkeyup                                ; CLIMB
  jr nz,notpressedup2

  ; CHANGE OBJECT MAP LOCATION
  pop hl
  dec h
  push hl

  notpressedup2:
  call testkeydown                              ; DID PLAYER PRESS DOWN
  pop hl
  jr nz,notpresseddown2
  inc h                                         ; MOVE PLANE DOWN
  jr notpresseddown2
  
  pilotejected:
  pop hl
  ld a,(enemyshiptimer)
  bit 0,a
  jr z,notpresseddown2
  inc h                                         ; IF TIMER RUNS OUT? FORCE PLANE TO CRASH?

  notpresseddown2:
  ld a,h
  or a                                          ; IF PLAYER REACHES TOP OF SCREEN, MOVE DOWN AGAIN
  jr nz,updateplayerlocationbasedonspeed
  inc h

  updateplayerlocationbasedonspeed:             ; SET HORIZONTAL POSITION BASED ON SPEED
  ld a,(playerspeed)
  srl a
  add 8 
  ld l,a
  ld (currentplayerlocation),hl

  ;-------------------------------------------------------------------------
  ; ERASE MISSILE TRAIL PLAYER

  updatemissilelocation:
  ; ERASE MISSILE TRAIL PLAYER
  ld a,(iy+0);(playermissilestatus)
  or a
  jr z,skipupdateplayermissilelocation 
  
  ld l,(iy+1);  
  ld h,(iy+2);l,(playermissileposition)
  xor a
  ld c,1 ; SKY OBJECT ID
  
  ; ERASE MISSILE TRAIL
  ; IF SCROLLING BY CRTC, WE NEED TO ADJUST THIS AS THE TILE POSITIONS ARE
  ; ALTERED WHEN WE SCROLL, UNLIKE SCROLLING BY SCREEN MEMORY COPY
  call drawspritecheckifsky_crtc_adjusted
  
  ; CHECK IF LOCKING MISSILE HEIGHT TO PLAYER
  ld a,1:lockinmissileheighttoplayer
  cp 1
  jr nz,skiplockrocketheight
  ld a,(iy+4)
  or a
  jr nz,skiplockrocketheight ; SKIP LOCK HEIGHT IF WE FIRED MAVERICK
  
  ; LOCK MISSILE HEIGHT TO PLAYER
  ld a,(iy+9);(currentplayerlocation+1)
  ld (iy+2),a;(playermissileposition+1),a ; HEIGHT
  skiplockrocketheight:
  skipupdateplayermissilelocation:

  ;-------------------------------------------------------------------------
  ; ERASE MISSILE TRAIL WINGMAN
  
  ld iy,wingmanmissileblock

  ld a,(iy+0);(playermissilestatus)
  or a
  jr z,skipupdatewingmanmissilelocation2
  
  ld l,(iy+1);  
  ld h,(iy+2);l,(playermissileposition)
  xor a
  ld c,1 ; SKY OBJECT ID
  
  ; ERASE MISSILE TRAIL
  ; IF SCROLLING BY CRTC, WE NEED TO ADJUST THIS AS THE TILE POSITIONS ARE
  ; ALTERED WHEN WE SCROLL, UNLIKE SCROLLING BY SCREEN MEMORY COPY
  call drawspritecheckifsky_crtc_adjusted
  
  ; CHECK IF LOCKING MISSILE HEIGHT TO PLAYER
  ld a,0:lockinmissileheighttoplayer2
  cp 1
  jr nz,skiplockrocketheight2
  ld a,(iy+4)
  or a
  jr nz,skiplockrocketheight2 ; SKIP LOCK HEIGHT IF WE FIRED MAVERICK
  
  ; LOCK MISSILE HEIGHT TO WINGMAN
  ld a,(iy+9);(currentplayerlocation+1)
  ld (iy+2),a;(playermissileposition+1),a ; HEIGHT
  skiplockrocketheight2:
  skipupdatewingmanmissilelocation2:
  ld iy,playermissileblock
  
  ;ld a,(currentplayerlocation+1) ; NOT SURE IF THIS IS NEEDED - SLOWS ROCKETS 
  ;cp (ix+#00)
  ;call c,scrollobjecttilemap
  ;pop de
  ;pop hl
ret

;erasemissiletrail:
;  ; ERASE MISSILE TRAIL
;  ld a,(iy+0);(playermissilestatus)
;  or a
;  ret z
;  
;  ld l,(iy+1);  
;  ld h,(iy+2);l,(playermissileposition)
;  xor a
;  ld c,1 ; SKY OBJECT ID
;  
;  ; ERASE MISSILE TRAIL
;  ; IF SCROLLING BY CRTC, WE NEED TO ADJUST THIS AS THE TILE POSITIONS ARE
;  ; ALTERED WHEN WE SCROLL, UNLIKE SCROLLING BY SCREEN MEMORY COPY
;  jp drawspritecheckifsky_crtc_adjusted

; INPUTS
; A = TILE ID
; C = OBJECT ID
; H = Y COORD
; L = X COORD
drawspritecheckifsky_crtc_adjusted:
  ld b,a                                ; STORE TILE ID IN B
  call getskytilemapid                  ; GET TILE GRID PTR IN DE
  ld a,(de)
  or a                                  ; SKIP IF JUST EMPTY SKY
  ret z
  ld a,c                                ; LOAD OBJECT ID INTO TILE GRID
  ld (de),a
  ld a,b                                ; RESTORE TILE ID IN A
  drawsprite_crtc_adjusted:
  dec l                                 ; ADJUST X COORD FOR CRTC SCROLL                           
  call convspritelocationtopixelscrtc   ; CONVERT HL CHAR LOCATION TO BC PIXEL LOCATION
  call getspritedata                    ; GET SPRITE FROM TABLE INTO DE
  jp drawtile                           ; DRAW TILE TO SCREEN LOCATION IN BC
  
flightnoise:
  ld a,(flightmode)
  dec a                  ; ARE WE IN FLIGHT HOVER MODE?
  ret z                  ; IF SO, NO SOUND
  ld a,(playerspeed)
  ld b,a
  ld a,&10               ; ADJUST TONE USING FLIGHT SPEED
  sub b
  ld b,a
  ld a,(currentplayerlocation+1)
  add b
  jp doflightnoise

scrollobjecttilemap:

  
  ; SCROLL TILEMAP?
  ld hl,startobjecttilemapplusone
  ld de,startobjecttilemap
  ld bc,639
  ldir
  ld a,(playerstatus)
  or a                             ; IF PLAYER NOT DEAD, JUMP TO CHECK MISSILE MOVE?
  ret nz;jr nz,checkplayermissilemove
  call flightnoise
drawplayerplane:
  ;ld ix,l891f
  
  ; NEED TO ERASE OBJECT MAP PLAYER LEAVES IN WAKE
  ;ld hl,(currentplayerlocation)
  ld hl,(lastplayerlocation)
  dec l
    ld c,1 ; SKY OBJECT ID
  xor a
  ;call drawspritecheckifsky_crtc_adjusted
  call drawspritecheckifsky;_crtc_adjusted;drawemptyskytile                        ; ERASE PLAYER PLANE TRAIL BACK
  ld hl,(lastplayerlocation)
  ;inc l
    ld c,1 ; SKY OBJECT ID
  xor a
  ;call drawspritecheckifsky_crtc_adjusted
  call drawspritecheckifsky;_crtc_adjusted;drawemptyskytile                        ; ERASE PLAYER PLANE TRAIL FRONT
  ;ld hl,(currentplayerlocation)
  ;push hl
  
  
  ld hl,(currentplayerlocation)
  call getskytilemapid
  ld a,(de)  ; CHECK BACK OF PLANE
  push de
  call checkplayeragainstobjectmap
  pop de
  ret nz;jr nz,checkplayermissilemove
  inc de ; CHECK FRONT OF PLANE
  ld a,(de)
  call checkplayeragainstobjectmap
  ret nz;jr nz,checkplayermissilemove
;  ld a,&4b    ; HARRIER TILE 1
  ld c,12      ; PLAYER PLANE OBJECT ID
  ld hl,(currentplayerlocation)
  xor a
  jp drawplusspritecheckifsky

; -------------------------------------------------------
; MISSILE CODE

; THIS WAS ORIGINALLY PART OF ABOVE FUNCTION, BUT WE SEPARATED IT
; THIS ENABLES US TO USE THE SAME FUNCTION FOR THE WINGMAN AS THE PLAYER
checkwingmanmissilemove:
  ld a,(iy+0);(playermissilestatus)      ; HAS MISSILE BEEN FIRED?
  or a
  ret z
  ;ld hl,playermissilecurrentrange       ; ADVANCE MISSILE ONE SQUARE
  inc (iy+3);inc (hl)
  
  ld a,(iy+4);(playermissilestatus2)     ; CHECK IF MAVERICK OR SIDEWINDER - MOVES DIFFERENTLY
  cp 1
  jr z,playermissilemovemaverick         ; PLAYER FIRED MAVERICK
  cp 2                                   
  jp z,playermissilemovemaverickguidance ; MAVERICK NOW ON GUIDANCE SYSTEM
  
  ld a,10:rocketrange2a                  ; LENGTH OF ROCKET RANGE - 10 DEFAULT
  cp (iy+3);(hl)                         ; DEACTIVATE MISSILE IF WE REACH RANGE
  jr nz,wingmanmissilemove
  ; KILL MISSILE
  ld (iy+0),0
ret

; THIS FUNCTION DOESN'T EXACTLY WORK FOR WINGMAN
; FOR PLAYER, MISSILE LOCATION IS BASED ON CURRENT SPEED OF PLAYER, PLUS INC'D COUNTER FOR RANGE

; WINGMAN SPEED IS INDEPENDENT OF PLAYER SPEED WHICH MAKES WINGMAN ROCKETS APPEAR "STILL"
; SO WE NEED TO BASE THE SPEED OF THE WINGMAN ROCKETS ON THE PLAYER SPEED INSTEAD

wingmanmissilemove:
  ld a,(iy+3);(hl)                       ; GET CURRENT RANGE OF MISSILE
  ld l,(iy+10)                           ; GET START MISSILE POSITION
  add l                                  ; ADD CURRENT MISSILE RANGE TO PLANE X LOCATION

  ld l,a;                                ; UPDATE MISSILE POSITION
  ld (iy+1),l;
  ld h,(iy+2);

  ld c,56                                ; PLAYER MISSILE SPRITE POINTING RIGHT
  push hl                         
  dec l                                  ; NOT SURE IF WE NEED THIS FOR WINGMAN MISSILE?
  call checkenemyhit
  jp nz,eraseplayermissiletrail

  ld (iy+0),a
  pop hl
ret

; THIS WAS ORIGINALLY PART OF ABOVE FUNCTION, BUT WE SEPARATED IT
; THIS ENABLES US TO USE THE SAME FUNCTION FOR THE WINGMAN AS THE PLAYER
checkplayermissilemove:
  ld a,(iy+0);(playermissilestatus)      ; HAS MISSILE BEEN FIRED?
  or a
  ret z
  ;ld hl,playermissilecurrentrange       ; ADVANCE MISSILE ONE SQUARE
  inc (iy+3);inc (hl)
  
  ld a,(iy+4);(playermissilestatus2)     ; CHECK IF MAVERICK OR SIDEWINDER - MOVES DIFFERENTLY
  cp 1
  jr z,playermissilemovemaverick         ; PLAYER FIRED MAVERICK
  cp 2                                   
  jr z,playermissilemovemaverickguidance ; MAVERICK NOW ON GUIDANCE SYSTEM
  
  ld a,10:rocketrange                    ; LENGTH OF ROCKET RANGE - 10 DEFAULT
  cp (iy+3);(hl)                         ; DEACTIVATE MISSILE IF WE REACH RANGE
  jr nz,playermissilemove
  killmissile:
  ;xor a
  ld (iy+0),0;a;(playermissilestatus),a
ret

; THIS FUNCTION DOESN'T EXACTLY WORK FOR WINGMAN
; FOR PLAYER, MISSILE LOCATION IS BASED ON CURRENT SPEED OF PLAYER, PLUS INC'D COUNTER FOR RANGE

; WINGMAN SPEED IS INDEPENDENT OF PLAYER SPEED WHICH MAKES WINGMAN ROCKETS APPEAR "STILL"
; SO WE NEED TO BASE THE SPEED OF THE WINGMAN ROCKETS ON THE PLAYER SPEED INSTEAD

playermissilemove:
  ld a,(iy+3);(hl)                       ; GET CURRENT RANGE OF MISSILE
  ld l,(iy+8)
  ;ld h,(iy+9)
  ;ld hl,(currentplayerlocation)         ; GET X Y POSITION OF PLAYER
  add l                                  ; ADD CURRENT MISSILE RANGE TO PLANE X LOCATION

  ld l,a;
  ld (iy+1),l;
  ld h,(iy+2);
  ;ld hl,(playermissileposition)
  ;ld l,a
  ;ld (playermissileposition),hl
  playermissilecheckenemyhit:
  ld c,56                         ; PLAYER MISSILE SPRITE POINTING RIGHT
  push hl                         
  dec l                           ; DEC L NOT WORKING FOR WINGMAN?
  call checkenemyhit
  jp nz,eraseplayermissiletrail
  ld (iy+0),a;(playermissilestatus),a
  pop hl
ret

playermissilemovemaverick:
  ;ld a,(enemylandlocationlock) ; CHECK IF WE STILL HAVE LOCK
  ;or a
  ;ret z

  ; WE NEED TO POSITION MISSILE ACCORDING TO PLAYER LOCATION BECAUSE OTHERWISE PLAYER CAN
  ; FLY INTO BACK OF OWN MISSILE AND BE KILLED
  
  ld a,(iy+3);(playermissilecurrentrange)              ; GET CURRENT RANGE OF MISSILE
  sub 10
  jr nc,startplayermissilemovemaverickguidance  ; IF FAR ENOUGH AWAY FROM PLANE, START GUIDANCE SYSTEM
  add 10
  ld l,(iy+8)
  ld h,(iy+9)
  ;ld hl,(currentplayerlocation)                 ; GET Y POSITION OF PLAYER
  add l                                         ; ADD CURRENT MISSILE RANGE TO PLANE X LOCATION

  ld l,a;
  ld (iy+1),l;
  ld h,(iy+2);
  ;ld hl,(playermissileposition)
  ;ld l,a
  ;ld (playermissileposition),hl

  ld c,56                                       ; PLAYER MISSILE SPRITE POINTING RIGHT
  push hl
  dec l
  call checkenemyhit
  jp nz,eraseplayermissiletrail
  call resetenemylandlocationlock               ; RESET LOCK - ENEMY DESTROYED
  ld (iy+0),a;(playermissilestatus),a
  pop hl
ret
startplayermissilemovemaverickguidance:
  ;ld a,2
  ld (iy+4),2;a;(playermissilestatus2),a     ; START GUIDANCE SYSTEM
playermissilemovemaverickguidance:
  ; GUIDE MAVERICK TOWARDS TARGET
  ld a,(enemylandlocationlock) ; CHECK IF WE STILL HAVE LOCK
  or a
  jr z,skipgetnewmaverickdirection

  ; GET DIRECTION TO MOVE TOWARD TARGET LOCK
  ld c,(iy+1);
  ld b,(iy+2);
  ;ld bc,(playermissileposition)
  ld de,(enemylandlocationlock)
  call getdirectionfromcoords ; GET DIRECTION IN A
  or a ; IF WE ARRIVE AT DESTINATION DON'T STOP MOVING! KEEP OLD DIRECTION SO MISSILE DOESN'T STAND STILL
  jr z,skipgetnewmaverickdirection
  ld (iy+5),a;(playermaverickdirection),a

  skipgetnewmaverickdirection:
  ld a,(iy+5);(playermaverickdirection)
  ld hl,maverickmovetable
  call vectortablelaunchcode
  jp drawmaverickmissile

maverickmovetable:
  defw dontmovemaverick
  defw movemaverickup
  defw movemaverickupright
  defw movemaverickright
  defw movemaverickdownright
  defw movemaverickdown
  defw movemaverickdownleft
  defw movemaverickleft
  defw movemaverickupleft

dontmovemaverick:
  ld l,(iy+1);
  ld h,(iy+2);
  ;ld hl,(playermissileposition)
  ;ld (playermavericklastposition),hl
  ld (iy+6),l;
  ld (iy+7),h;
  
  dec (iy+1);l
  ld c,56                                   ; SPRITE ID TO DRAW IF WE HAVEN'T BANGED ANYTHING
  ret
; SCREEN POSITIONS AUTOMATICALLY DECREMENT WHEN THE SCREEN SCROLLS
; SO WE NEED TO ACCOUNT FOR THAT WHEN WE MOVE OUR MISSILE
movemaverickup:
  ld l,(iy+1);
  ld h,(iy+2);
  ;ld hl,(playermissileposition)
  ;ld (playermavericklastposition),hl
  ld (iy+6),l;
  ld (iy+7),h;
  
  dec (iy+2);h
  dec (iy+1);l
  ld c,101                                  ; SPRITE ID TO DRAW IF WE HAVEN'T BANGED ANYTHING
  ret
movemaverickupright:
  ld l,(iy+1);
  ld h,(iy+2);
  ;ld hl,(playermissileposition)
  ;ld (playermavericklastposition),hl
  ld (iy+6),l;
  ld (iy+7),h;
  
  dec (iy+2);dec h
  ld c,98                                  ; SPRITE ID TO DRAW IF WE HAVEN'T BANGED ANYTHING
  ret
movemaverickright:
  ld l,(iy+1);
  ld h,(iy+2);
  ;ld hl,(playermissileposition)
  ;ld (playermavericklastposition),hl
  ld (iy+6),l;
  ld (iy+7),h;
  
  ld c,56                                  ; SPRITE ID TO DRAW IF WE HAVEN'T BANGED ANYTHING
  ret
movemaverickdownright:
  ld l,(iy+1);
  ld h,(iy+2);
  ;ld hl,(playermissileposition)
  ;ld (playermavericklastposition),hl
  ld (iy+6),l;
  ld (iy+7),h;
  
  inc (iy+2);inc h
  ld c,99                                  ; SPRITE ID TO DRAW IF WE HAVEN'T BANGED ANYTHING
  ret
movemaverickdown:
  ld l,(iy+1);
  ld h,(iy+2);
  ;ld hl,(playermissileposition)
  ;ld (playermavericklastposition),hl
  ld (iy+6),l;
  ld (iy+7),h;
  
  inc (iy+2);inc h
  dec (iy+1);dec l
  ld c,100                                 ; SPRITE ID TO DRAW IF WE HAVEN'T BANGED ANYTHING
  ret
movemaverickdownleft:
  ld l,(iy+1);
  ld h,(iy+2);
  ;ld hl,(playermissileposition)
  ;ld (playermavericklastposition),hl
  ld (iy+6),l;
  ld (iy+7),h;
  
  inc (iy+2);inc h
  dec (iy+1);dec l
  dec (iy+1);dec l
  ld c,55                                  ; SPRITE ID TO DRAW IF WE HAVEN'T BANGED ANYTHING
  ret
movemaverickleft:
  ld l,(iy+1);
  ld h,(iy+2);
  ;ld hl,(playermissileposition)
  ;ld (playermavericklastposition),hl
  ld (iy+6),l;
  ld (iy+7),h;
  
  dec (iy+1);dec l
  dec (iy+1);dec l
  ld c,55                                  ; SPRITE ID TO DRAW IF WE HAVEN'T BANGED ANYTHING
  ret
movemaverickupleft:
  ld l,(iy+1);
  ld h,(iy+2);
  ;ld hl,(playermissileposition)
  ;ld (playermavericklastposition),hl
  ld (iy+6),l;
  ld (iy+7),h;
  
  dec (iy+2);dec h
  dec (iy+1);dec l
  dec (iy+1);dec l
  ld c,53                                  ; SPRITE ID TO DRAW IF WE HAVEN'T BANGED ANYTHING
  ret
drawmaverickmissile:
  ;ld (playermissileposition),hl            ; STORE NEW POSITION
  ld l,(iy+1);,l
  ld h,(iy+2);,h
  call checkenemyhit
  jr nz,eraseplayermavericktrail
  ld (iy+0),a;(playermissilestatus),a
ret
eraseplayermavericktrail:
  ; ERASE MAVERICK TRAIL
  ;ld hl,(playermavericklastposition)
  ld l,(iy+6);
  ld h,(iy+7);
  dec l
  ld a,0:contrail4
  ld c,1   ; SKY OBJECT ID
  jp drawspritecheckifsky;_crtc_adjusted

; WORK OUT DIRECTION OF TRAVEL FROM TWO SETS OF COORDINATES

; INPUTS
; B = CURR Y COORD
; C = CURR X COORD
; D = DEST Y COORD
; E = DEST X COORD
; OUTPUTS
; A = DIRECTION, 1 = NORTH, 2 = NE
; REACHED DESTINATION
; A = 0

; DIRECTION TABLE
; X DIRECTIONS
; 8 1 2
; 7 0 3
; 6 5 4

updir:   defb 8,1,2
middir:  defb 7,0,3
downdir: defb 6,5,4

getdirectionfromcoords:
  xor a ; RESET CARRY FLAG?
  ld a,d
  sbc b
  jr c,setup
  or a 
  jr z,setmid

  setdown:
    ld hl,downdir+1
    jr gethdir
  setup:
    ld hl,updir+1
    jr gethdir
  setmid:
    ld hl,middir+1

  gethdir:
    xor a ; RESET CARRY FLAG?
    ld a,e
    sbc c
    jr c,setleft
    or a
    jr z,setdir

    setright:
    inc hl
    jr setdir
    setleft:
    dec hl
    setdir:
    ld a,(hl)
    ret

eraseplayermissiletrail:
  pop hl
  push hl
  dec l
  dec l
  ld a,0:contrail2
  ld c,1   ; SKY OBJECT ID
  call drawspritecheckifsky;_crtc_adjusted
  pop hl
  
  push hl
  dec l
  ld a,0:contrail3
  ld c,1   ; SKY OBJECT ID
  call drawspritecheckifsky;_crtc_adjusted
  pop hl
  
  ld c,56  ; MISSILE POINTING RIGHT
  call checkenemyhit
  ret nz
  ld (iy+0),a;(playermissilestatus),a
ret

; ------------------------------------------------------------
; COLLISION DETECTION

checkplayeragainstobjectmap:
  or a    ; CP 0  - CLOUD OBJECT
  ret z   
  dec a   ; CP 1  - SKY OBJECT
  ret z
  cp 19   ; CP 20 - WINGMAN
  ret z;jr z,playercollidedwithwingman
  cp 20   ; CP 21 - POWERUP
  jr z,checkactivatepowerup
  cp 9    ; CP 10 - FLAK OBJECT
  jp nz,planehitbyobject
  ; REGISTER FLAK DAMAGE
  ld hl,flakdamagecount
  inc (hl)
  call updatehealth
  ld a,(totalflakdamagecount)
  cp (hl)                       ; IF WE REACH TOTAL DAMAGE
  jp z,planehitbyobject         ; PLANE IS DESTROYED
  xor a
ret

; ------------------------------------------------------------
; POWER UPS

checkactivatehealth:
  ; RECOVER HEALTH
  xor a
  ld (flakdamagecount),a
  call displayhealth
  ; ERASE POWERUP STATUS
  jp destroywingmanpowerup
  
checkactivaterockets:
  ; RECOVER ROCKETS
  ld a,&10
  ld (numberofrockets),a
  ; ERASE POWERUP STATUS
  jp destroywingmanpowerup

checkactivatebombs:
  ; RECOVER ROCKETS
  ld a,&10
  ld (numberofbombs),a
  ; ERASE POWERUP STATUS
  jp destroywingmanpowerup

; PLAYER COLLIDED WITH WINGMAN HELP OBJECT
; SUMMON WINGMAN
checkactivatepowerup:
  ld a,(wingmanpowerupstatus)
  cp 2                           ; HEALTH POWER UP
  jr z,checkactivatehealth
  cp 3                           ; ROCKETS POWER UP
  jr z,checkactivaterockets
  cp 4                           ; BOMBS POWER UP
  jr z,checkactivatebombs
  ; WINGMAN POWERUP
  ld a,(wingmantakeoff)          ; ONLY ACTIVATE WINGMAN IF DEAD OR NOT ALREADY AIRBORN
  cp 254
  jr z,activatewingman
  or a
  jr z,activatewingman
ret

activatewingman:
  ; RESET WINGMAN POSITION TO ENTER AT TOP LEFT SIDE OF SCREEN
  ld hl,&0100
  ld (wingmanlocation),hl
  ld (backupwingmanlocation),hl
  
  ; SPAWN ABOVE OR BELOW PLAYER DEPENDING ON WHERE PLAYER IS
  ld a,(currentplayerlocation)
  sub 6
  jr c,dosetwingmanbelowplayer
  xor a
  jr dosetwingmanbelowplayer2
  dosetwingmanbelowplayer:
  ld a,1
  dosetwingmanbelowplayer2:
  ld (wingmanbelowplayer),a
  
  ; ENTER PLAYER FLYING FORMATION
  ld a,1
  ld (wingmantakeoff),a
  ; MAKE UNBROKEN WINGMAN SPRITE
  call settakeoffspritewingman 
  ; ERASE WINGMAN POWERUP STATUS
  jp destroywingmanpowerup

; PLAYER COLLIDED WITH WINGMAN - DESTROY BOTH PLANES
playercollidedwithwingman:
  call wingmandestroyed
  jp planehitbyobject

wingmanavoidsky:
  ld a,1;xor a
  jr setwingmanposition
  forcewingmanavoidsea:
  xor a;ld a,1
  setwingmanposition:
  ld (wingmanbelowplayer),a
ret

;showtileposition:
;push hl
;push af
;push de
;push bc
;;ld hl,(enemylandlocationlock)
;;  dec h
;call convspritelocationtopixelscrtc   ; CONVERT HL CHAR LOCATION TO BC PIXEL LOCATION
;ld a,2;14;1;0
;call getspritedata                    ; GET SPRITE FROM TABLE INTO DE
;call drawtile                           ; DRAW TILE TO SCREEN LOCATION IN BC
;pop bc
;pop de
;pop af
;pop hl
;ret

;cleartileposition:
;push hl
;push af
;push de
;push bc
;ld hl,(enemylandlocationlock)
;  dec h
;call convspritelocationtopixelscrtc   ; CONVERT HL CHAR LOCATION TO BC PIXEL LOCATION
;ld a,0;2;14;1;0
;call getspritedata                    ; GET SPRITE FROM TABLE INTO DE
;call drawtile                           ; DRAW TILE TO SCREEN LOCATION IN BC
;pop bc
;pop de
;pop af
;pop hl
;ret

debugcoldetect equ 1


result: defb " ",255

dodebuga:
push af
push hl
push bc
push de
  ld hl,&40FE+14;0307
  dodebugmain:
  add 48 ; CONVERT TO ASCII
  ld (result),a
  
  ; DRAW INSTRUMENTS
  ld bc,&7fa0 ; PAGE OUT PLUS REGISTERS
  out (c),c

  ld de,result
  call writelinede 
  
  ld bc,&7fb8 ; PAGE IN PLUS REGISTERS
  out (c),c 
pop de
pop bc
pop hl
pop af
ret

wingmandirectioncheck: defb 0
;seconddirectioncheck:  defb 0

wingmanmoveblocked2:
  xor a
;  ld (seconddirectioncheck),a   ; RESET DIRECTION CHECK COUNTER
ret
wingmanmoveokay:
  ld a,(wingmandirectioncheck)
ret
; TRY OPPOSITE DIRECTION
; XXX NEED TO CHECK HERE FOR MISSILE, PLANE, GROUND OBJECT AND MOVE ACCORDINGLY
wingmanmoveblocked:
  xor a ; WAY IS BLOCKED
  ;call dodebugc
ret

avoidskyrouteblocked:
  call wingmanavoidsky
  jp wingmanmoveblocked
avoidsearouteblocked:
  ld a,(wingmantakeoff)  ; IF WE ARE TRYING TO LAND, DON'T WORRY ABOUT SEA
  cp 3
  jp z,wingmanmoveokay
  call forcewingmanavoidsea
  jp wingmanmoveblocked

; CHECK OBJECT MAP AROUND WINGMAN SPRITE BASED ON DESIRED DIRECTION
; PREVENTS WINGMAN DELIBERATELY COLLIDING WITH OBSTACLE

; INPUTS
; A = DIRECTION TO CHECK
; OUTPUTS
; A = DIRECTION ALLOWED - 0 IF NOT
checkwingmanradar:
  or a ; DON'T MOVE
  jp z,docheckwingmanstationary4
  ;jr z,checkmoveright ; XXX - STILL NEED TO CHECK AHEAD OF PLANE FOR MISSILES, ETC
;'  ret z
  
  ld (wingmandirectioncheck),a ; BACK UP DESIRED DIRECTION
  ld hl,(wingmanlocation)      ; GET CURRENT LOCATION
   
  cp 1 
  jr z,checkmoveup
  cp 2 
  jr z,checkmoveupright
  cp 3 
  jr z,checkmoveright
  cp 4 
  jr z,checkmovedownright
  cp 5 
  jr z,checkmovedown
  cp 6
  jr z,checkmovedownleft
  cp 7
  jr z,checkmoveleft
  ;cp 8
  checkmoveupleft:
  dec l
  ld a,l ; DON'T MOVE OFFSCREEN IF COORD IS ZERO
  or a   
  jr z,avoidskyrouteblocked
  cp 255
  jr z,avoidskyrouteblocked
  checkmoveup:
  dec h
  ld a,h ; DON'T MOVE OFFSCREEN
  or a   
  jr z,avoidskyrouteblocked
  jp docheckwingmanmove4
  
  ; CHECK FIRST TILE
  docheckwingmanmove2: 
  call getskytilemapid    ; GET OJECT IN DE
  ld a,(de)  
  cp 1                    ; SKY
  jr z,checkwingmansecondtileabove
  or a                    ; CLOUD
  jr z,checkwingmansecondtileabove
  cp 10                   ; FLAK
  jr z,checkwingmansecondtileabove
  jp wingmanmoveblocked
  ; CHECK SECOND TILE
  checkwingmansecondtileabove:
  inc de
  ld a,(de)  
  cp 1                    ; SKY
  jp z,wingmanmoveokay
  or a                    ; CLOUD
  jp z,wingmanmoveokay
  cp 10                   ; FLAK
  jp z,wingmanmoveokay
  jp wingmanmoveblocked
  
  checkmoveupright:
  inc l
  ld a,l ; DON'T MOVE TOO FAR TO RIGHT
  cp 30   
  jp z,wingmanmoveblocked
  jr checkmoveup
  
  checkmovedownright:
  ld a,l ; DON'T MOVE TOO FAR TO RIGHT
  cp 30   
  jp z,wingmanmoveblocked
  inc l
  jr checkmovedown
  
  checkmoveright:
  ;call dodebuga
  inc l
  ld a,l ; DON'T MOVE TOO FAR TO RIGHT
  cp 30   
  jp z,wingmanmoveblocked
  inc l
  ld a,l ; DON'T MOVE TOO FAR TO RIGHT
  cp 30   
  jp z,wingmanmoveblocked
  jr docheckwingmanmove4

  checkmovedownleft:
  dec l
  ld a,l ; DON'T MOVE OFFSCREEN
  or a   
  jp z,wingmanmoveblocked
  checkmovedown:
  inc h
  ld a,h ; DON'T MOVE TOO FAR DOWN
  cp 13   
  jp z,avoidsearouteblocked
  jr docheckwingmanmove4;2

  checkmoveleft:
  dec l
  ld a,l ; DON'T MOVE OFFSCREEN
  or a   
  jp z,wingmanmoveblocked
  cp 255
  jp z,avoidskyrouteblocked
  dec l
  ld a,l ; DON'T MOVE OFFSCREEN
  or a   
  jp z,wingmanmoveblocked
  jr docheckwingmanmove2behind;docheckwingmanmove2
  
; CHECK BEHIND FOUR TILES FOR OBJECTS 
; CHECK AHEAD FOUR TILES FOR OBJECTS
; CHECK FIRST TILE
docheckwingmanmove2behind: 
  call getskytilemapid    ; GET OJECT IN DE
  ld a,(de)  
  cp 1                    ; SKY
  jr z,checkwingman2tilebehind
  or a                    ; CLOUD
  jr z,checkwingman2tilebehind
  cp 10                   ; FLAK
  jr z,checkwingman2tilebehind
  jp wingmanmoveblocked
  
  ; CHECK SECOND TILE
  checkwingman2tilebehind:
  dec de
  ld a,(de)  
  cp 1                    ; SKY
  jp z,checkaheadwhilereversing
  or a                    ; CLOUD
  jp z,checkaheadwhilereversing
  cp 10                   ; FLAK
  jp z,checkaheadwhilereversing
  jp avoidoncomingobject;wingmanmoveblocked

; EVEN THOUGH WE ARE MOVING LEFT,
; WE STILL NEED TO CHECK IN FRONT OF US
; IN CASE WE ARE TARGETTED BY ENEMY MISSILE OR PLANE
checkaheadwhilereversing:
  ld hl,(wingmanlocation); MOVE BACK TO WINGMAN PLANE LOCATION
  inc l
  inc l
; CHECK AHEAD FOUR TILES FOR OBJECTS
; CHECK FIRST TILE
docheckwingmanmove4: 
  call getskytilemapid    ; GET OJECT IN DE
  ld a,(de)  
  cp 1                    ; SKY
  jr z,checkwingman2tile
  or a                    ; CLOUD
  jr z,checkwingman2tile
  cp 10                   ; FLAK
  jr z,checkwingman2tile
  jp wingmanmoveblocked
  
  ; CHECK SECOND TILE
  checkwingman2tile:
  inc de
  ld a,(de)  
  cp 1                    ; SKY
  jr z,checkwingman3tile
  or a                    ; CLOUD
  jr z,checkwingman3tile
  cp 10                   ; FLAK
  jr z,checkwingman3tile
  jp avoidoncomingobject;wingmanmoveblocked
  
  ; CHECK THIRD TILE
  checkwingman3tile:
  inc de
  ld a,(de)  
  cp 1                    ; SKY
  jr z,checkwingman4tile
  or a                    ; CLOUD
  jr z,checkwingman4tile
  cp 10                   ; FLAK
  jr z,checkwingman4tile
  jp avoidoncomingobject;wingmanmoveblocked
  
  ; CHECK FOURTH TILE
  checkwingman4tile:
  inc de
  ld a,(de)  
  cp 1                    ; SKY
  jp z,wingmanmoveokay
  or a                    ; CLOUD
  jp z,wingmanmoveokay
  cp 10                   ; FLAK
  jp z,wingmanmoveokay
avoidoncomingobject:
  ld a,(wingmandirectioncheck) ; GET DESIRED DIRECTION
  cp 3 ; DIRECTLY IN FRONT MOVING FORWARD
  jr z,avoidoncomingobjectifdirectionahead
  cp 7 ; DIRECTLY IN FRONT MOVING BACKWARD
  jr z,avoidoncomingobjectifdirectionahead
  or a ; DIRECTLY IN FRONT NOT MOVING
  jr z,avoidoncomingobjectifdirectionahead
  jp wingmanmoveblocked
  
avoidoncomingobjectifdirectionahead:
  ; DECIDE WHETHER TO MOVE UP OR DOWN BASED ON ALTITUDE  
  ld a,(wingmanlocation)    ; GET HEIGHT OF WINGMAN
  sub 6
  jr c,takeevasiveactiondown
  takeevasiveactionup:
  dec h
  inc l
  inc l
  jr takeevasiveaction
  takeevasiveactiondown:
  inc h
  inc l
  inc l
  takeevasiveaction:  
  ld a,(wingmantakeoff)     ; ONLY IF WE ARE STILL ALIVE
  cp 254
  ret z
  cp 255
  ret z
  cp 2  ; DON'T INTERRUPT LANDING SEQUENCE
  ret z
  cp 3  ; DON'T INTERRUPT LANDING SEQUENCE
  ret z
  cp 4  ; DON'T INTERRUPT LANDING SEQUENCE
  ret z
  
  ld (customwaypoint-2),hl
  ld a,8                    ; MOVE TO WAYPOINT
  ld (wingmantakeoff),a
  scf
ret  
  
  
  
; CHECK AHEAD FOUR TILES FOR OBJECTS
; WE ARE ONLY LOOKING FOR OBSTACLES, WE DON'T NEED TO MOVE ANYWHERE
docheckwingmanstationary4:
  ld hl,(wingmanlocation)      ; GET CURRENT LOCATION
  inc l
  inc l
 
  call getskytilemapid    ; GET OJECT IN DE
  ld a,(de)  
  cp 1                    ; SKY
  jr z,checkwingman2tilestationary
  or a                    ; CLOUD
  jr z,checkwingman2tilestationary
  cp 10                   ; FLAK
  jr z,checkwingman2tilestationary
  jp avoidoncomingobjectifdirectionahead
  
  ; CHECK SECOND TILE
  checkwingman2tilestationary:
  inc de
  ld a,(de)  
  cp 1                    ; SKY
  jr z,checkwingman3tilestationary
  or a                    ; CLOUD
  jr z,checkwingman3tilestationary
  cp 10                   ; FLAK
  jr z,checkwingman3tilestationary
  jp avoidoncomingobjectifdirectionahead;wingmanmoveblocked
  
  ; CHECK THIRD TILE
  checkwingman3tilestationary:
  inc de
  ld a,(de)  
  cp 1                    ; SKY
  jr z,checkwingman4tilestationary
  or a                    ; CLOUD
  jr z,checkwingman4tilestationary
  cp 10                   ; FLAK
  jr z,checkwingman4tilestationary
  jp avoidoncomingobjectifdirectionahead;wingmanmoveblocked
  
  ; CHECK FOURTH TILE
  checkwingman4tilestationary:
  inc de
  ld a,(de)  
  cp 1                      ; SKY
  jp z,wingmanmoveblocked2  ; RETURN 0 AS DIRECTION AS WE AREN'T MOVING ANYWHERE
  or a                      ; CLOUD
  jp z,wingmanmoveblocked2  ; RETURN 0 AS DIRECTION AS WE AREN'T MOVING ANYWHERE
  cp 10                     ; FLAK
  jp z,wingmanmoveblocked2  ; RETURN 0 AS DIRECTION AS WE AREN'T MOVING ANYWHERE
  jp avoidoncomingobjectifdirectionahead  
  
; WINGMAN COLLIDED WITH OBJECT
checkwingmanagainstobjectmap:
  or a    ; CP 0  - CLOUD OBJECT
  ret z   
  cp 1    ; CP 1  - SKY OBJECT
  ret z
  cp 10   ; CP 10 - FLAK OBJECT
  ret z;jr nz,planehitbyobject
  cp 12   ; HARRIER ID
  ret z ;jp z,playercollidedwithwingman;restorebackupwingmanlocation
  ;cp 4    ; OWN FRIGATE ID?
  ;ret z
  cp 20
  ret z   ; OWN WINGMAN ID
  cp 21
  ret z   ; POWERUP
  cp 13
  jr z,enemyplanecollidedwithwingman
dowingmandestroyed:
  xor a
  call explosionnoise
  jp wingmandestroyed

enemyplanecollidedwithwingman:
  ; ENEMY PLANE DESTROYED BY COLLISION WITH WINGMAN
  call drawtilesky
  ld a,status_enemyplanehit
  ld (enemyplanestatus),a
  jp dowingmandestroyed

;checkwingmantrackenemyplane:
;  ; ONLY TRACK IF WE ARE IDLE
;  ; I.E. NOT ALREADY TRACKING IT, OR AREN'T IN THE AIR, OR CRASHED
;  ld a,(wingmantakeoff)      
;  cp 1;5                       ; WE ARE ALREADY TRACKING IT
;  ret nz
;  ;cp 6                        ; WE ARE ALREADY TRACKING IT ON THE SECOND WAYPOINT
;  ;ret z
;  ;or a                        ; WE AREN'T IN THE AIR
;  ;ret z
;  ;cp 254                      ; WE CRASHED
;  ;ret z  
;  ;cp 255                      ; WE CRASHED
;  ;ret z
  ;
;  ld a,(enemyplanestatus)      ; ONLY TRACK ENEMY PLANE IF ON APPROACH TO PLAYER
;  cp 1
;  ret nz
  
;  ld a,(enemyplanelocation)    ; IF PLANE IS NOT TOO HIGH TO TRACK
;  sub 4;
;  jr c,resetwingmanflight
;  jr nz,resetwingmanflight

  ; FIRE MISSILE WHEN IN RANGE 
 
  ; call FIRE MISSILE
 
;  ret
; ENEMY PLANE NO LONGER ON APPROACH, STOP TRACKING IT
resetwingmanflight:
  ld a,(wingmantakeoff)        ; ONLY RESET IF WE HAVEN'T BEEN KILLED!
  cp 255
  ret z
  cp 254
  ret z
  ld a,1 
  ld (wingmantakeoff),a        ; SET TO NORMAL FLIGHT
ret


;  jp movewingmanup
wingmandestroyed:
  ld hl,(wingmanlocation)
  dec l
  ;call drawemptyskytile
  ; ERASE WINGMAN ON OBJECT MAP SO WE CAN'T CRASH INTO IT STILL
  xor a
  ld c,1
  call drawspritecheckifsky
  ld hl,(wingmanlocation)
  ;inc l
  ;dec l
  xor a
  ld c,1
  call drawspritecheckifsky
  ;call drawemptyskytile

  ; PLANE IS GONE
  call setwingmanbrokesprite
  ld a,255                     ; WINGMAN DESTROYED  = 255 = MARK KILLED UPDATE SPRITE, 254 = KILLED AND CLEARED FROM SCREEN
  ld (wingmantakeoff),a
ret

ownmissilehitwingman:
  call wingmandestroyed
  xor a
  call explosionnoise
  pop hl
  jr returnzero
  

; A = OBJECT COLLIDED WITH
; 0 = CLOUD
; 1 = SKY
; 2 = LAND
; 3 = FRIGATE
; 6 = SHIP
; 7 = AIRCRAFT MISSILE / GUN EMPLACEMENT
; 10 = AIRCRAFT MISSILE

planehitbyobject:
  ld a,1:deathstatus4               ; KILLED
  ld (playerstatus),a
  call doexplosionnoise
  or a
ret

; CHECK IF BOMB OR MISSILE HIT SOMETHING?
; INPUTS
; HL = X Y COORDINATE
; C  = TILE ID TO DRAW
checkenemyhit:
  push hl
  call getskytilemapid            ; CHECK MAP FOR COLLISION
  
  ld a,(de)
  or a                            ; CP 0 = IS CLOUD
  jr z,drawplayermissilebomb
  dec a                           ; CP 1 = IS SKY
  jr z,drawplayermissilebomb 
  cp 1;#01  ; SEA OBJECT ID
  jr z,bombhitsealand
  cp 2;#02  ; LAND OBJECT ID
  jr z,bombhitlandmakehole;bombhitsealand
  cp 9;#09  ; PIER OBJECT UD
  jr z,bombhitsealand
  cp 10;#0a ; SMOKE / FLAK
  jr z,playermissilehitenemymissile
  cp 12;#0c ; ENEMY PLANE
  jr z,bombhitenemyplane
  cp 19;      WINGMAN
  jr z,ownmissilehitwingman;wingmandestroyed
  cp 20;      WINGMAN POWERUP
  jp z,dododestroywingmanpowerup
  cp 6;#06  ; MISSILE / BOMB ID
  jr z,bombhitenemyship
  cp 3;#03  ; OWN FRIGATE ID
  jr z,bombhitownfrigate
  pop hl
  
  ld a,(gamelevelprogress)
  cp 5;#05
  ld a,35;&23                     ; 350 POINTS IF IN TOWN
  jr nc,missilehitenemyplane
  ld a,10                         ; DEFAULT IS DESTRUCTABLE OBJECT - 100 POINTS
  ; MISSILE / BOMB HIT DESTRUCTABLE OBJECT
  missilehitenemyplane:
  call explosionnoise
  call drawsmokesprite
  returnzero:
  xor a
ret

dododestroywingmanpowerup:
  xor a
  call explosionnoise
  pop hl
  jp destroywingmanpowerup

; FIRE MISSILE BOMB
; INPUT
; HL = SPRITE LOCATION
; C  = TILE ID TO DRAW
drawplayermissilebomb:
  pop hl
  ld a,c
  ld c,6 ; OBJECT ID
  call drawspritecheckifsky
  ld a,1 
  or a
ret

bombhitownfrigate:
  ld a,1
  ld (playerfrigatestatus),a
  xor a
  pop hl
  call explosionnoise
  call drawsmokesprite
  jr returnzero

bombhitenemyship:
  pop hl
  ld a,50              ; SET SCORE FOR HIT
  call explosionnoise
  call drawsmokesprite
  jr returnzero
  
; BOMB HAS NO EFFECT
bombhitsealand:
  pop hl
  jr returnzero

; BOMB MAKES CRATER IN LAND
bombhitlandmakehole:
  pop hl
  call drawholesprite
  call dobombnoise
  jr returnzero
  
bombhitenemyplane:
  ld a,status_enemyplanehit
  ld (enemyplanestatus),a
  ld a,75              ; SET SCORE FOR HIT
  call explosionnoise
  pop hl
  jr returnzero
  
playermissilehitenemymissile:
  xor a
  ld (enemymissilestatus),a
  ld a,1               ; SET SCORE FOR HIT
  call explosionnoise
  xor a
  ld c,1               ; SKY OBJECT ID
  pop hl
  call drawspritecheckifsky
  jr returnzero

scrollsceneryafterdeath: defb 0

; INPUTS
; A = SCORE TO ADD
explosionnoise: 
  push hl
  ld hl,(numberofhits)
  inc hl                        ; INCREASE NUMBER OF HITS
  ld (numberofhits),hl
  push af
  call doloudbombnoise          ; MAKE EXPLOSION NOISE
  ld a,7                        ; REPEAT SCROLL SCENERY BY 7
  ld (scrollsceneryafterdeath),a
  pop af
  ld e,a
  ld d,0
 
  ld hl,(currscore)
  add hl,de                     ; INCREASE SCORE
  ld (currscore),hl
  call convwordtostr            ; DRAW SCORE ON SCREEN
  
  ld bc,&7fa0                   ; PAGE OUT PLUS REGISTERS
  out (c),c
 
  ld hl,&40B4;020a
  ;call convspritelocationtopixelssplittable
  ;   jrrs:
  ;jr jrrs
  
  ld de,wordtostr
  call writelinede

  ld bc,&7fb8                   ; PAGE IN PLUS REGISTERS
  out (c),c

  pop hl
ret

interruptcounter2:      defb 0
interruptcounter3:      defb 0
interrupt_stack:        defs 256
interrupt_stack_start:

timepleasehl:           defw 0
timepleasede:           defw 0

;asicpagedout:           defb 0 ; NOTIFY IF WE HAVE ASIC PAGED IN OR OUT WHEN CALLING INTERRUPT

interrupttableptr: defw interrupttable
interrupttable:
  jp fiftiethofasecondinterrupt5
  jp fiftiethofasecondinterrupt4
  jp fiftiethofasecondinterrupt6
  jp fiftiethofasecondinterrupt2
  jp fiftiethofasecondinterrupt3
  jp fiftiethofasecondinterrupt

myinterrupt:
  ld (previous_stack+1),sp
  ld sp,interrupt_stack_start

  ; SWITCH TO SHADOW REGISTERS
  exx
  ex af,af'

  ; CHECK IF ASIC PAGED IN OR OUT
  ld a,(&4000)
  cp &0f ; NOT PAGED OUT - THIS AREA IS THE INSTRUMENT PANEL
  jr nz,skippageinasic
  ld bc,&7fb8 ; PAGE IN PLUS REGISTERS
  out (c),c  
  ld a,1      ; REMEMBER TO PAGE IT OUT AGAIN
  ld (asicpagedout-1),a
  skippageinasic:
  
  ; RECORD TIME EVERY 300th OF A SECOND
  
  ld bc,(timepleasehl)
  inc bc
  ld (timepleasehl),bc
  ld a,b
  or c   ; CHECK IF BC = 0, IF SO ZERO IS SET
  jr nz,skipsettimepleasede
  ; INC DE AS WE HAVE MADE A CYCLE OF HL
  ld bc,(timepleasede)
  inc bc
  ld (timepleasede),bc
  skipsettimepleasede:
  
  ld hl,(interrupttableptr)
  jp (hl)
  
  finishinterrupt:
  ld hl,(interrupttableptr)
  inc hl
  inc hl
  inc hl
  ld (interrupttableptr),hl
  finishinterrupt2:
  
  ; PAGE OUT PLUS REGISTERS IF THAT'S THE WAY WE FOUND THEM!
  ld a,0:asicpagedout;(asicpagedout)
  or a
  jr z,skippageoutasic
  xor a
  ld (asicpagedout-1),a
  ld bc,&7fa0 
  out (c),c 
  skippageoutasic:
  
  exx
  ex af,af'
  previous_stack:
  ld sp,0
  ei
ret
fiftiethofasecondinterrupt2:
  ld hl,palettemainscreen
  ld de,&6400
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
jp finishinterrupt
fiftiethofasecondinterrupt3:
  ld hl,paletteinstruments
  ld de,&6400
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  call playsound_b
jp finishinterrupt
fiftiethofasecondinterrupt4: 
  ; READ KEYBOARD EVERY 50th OF A SECOND
        ;di              ;1 ##%%## C P C   VERSION ##%%##
        ld hl,matrix_buffer    ;3
        ld bc,#f782     ;3
        out (c),c       ;4
        ld bc,#f40e     ;3
        ld e,b          ;1
        out (c),c       ;4
        ld bc,#f6c0     ;3
        ld d,b          ;1
        out (c),c       ;4
        ld c,0          ;2
        out (c),c       ;4
        ld bc,#f792     ;3
        out (c),c       ;4
        ld a,#40        ;2
        ld c,#4a        ;2 44
loopb:  ld b,d          ;1
        out (c),a       ;4 select line
        ld b,e          ;1
        ini             ;5 read bits and write into KEYMAP
        inc a           ;1
        cp c            ;1
        jr c,loopb      ;2/3 9*16+1*15=159
        ld bc,#f782     ;3
        out (c),c       ;4                
jp finishinterrupt

fiftiethofasecondinterrupt6:
  ld hl,palettesky
  ld de,&6400
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  call playsound_c
jp finishinterrupt
fiftiethofasecondinterrupt5:
  ld hl,palettesky2
  ld de,&6400
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  ldi
  call playsound_a
jp finishinterrupt

fiftiethofasecondinterrupt: 
  ld hl,interrupttable
  ld (interrupttableptr),hl
  
  ld a,(interruptcounter2)
  inc a
  ld (interruptcounter2),a

  cp 13:thirdinterruptspeed
  jp nz,finishinterrupt2
mysecondinterrupt:
  xor a
  ld (interruptcounter2),a
  defb 0:thirdinterruptcommand    ; &CD = CALL
  defw 0:thirdinterruptfunction   ; playmusic
  
  defb 0:secondinterruptcommand   ; &CD = CALL
  defw 0:secondinterruptfunction  ; fadepalette
jp finishinterrupt2

kl_time_please:
  ld hl,(timepleasehl)
  ld de,(timepleasede)
ret

; INPUT
; DE = BORDER COLOUR - 0,G,B,R
scr_set_border:
  push hl
  ld hl,&6420 ; &6420/21 BORDER COLOUR
  ld (hl),e   ; WRITE PALETTE DATA TO REGISTERS
  inc hl
  ld (hl),d
  pop hl
ret

unlockasic:
  ; SET INTERRUPT FUNCTION - STOPS BASIC RE-LOCKING ASIC
;  di
  ld a,&C9 ; C9 = RET, C3 = JP
  ld hl,&0038
  ld (hl),a

  ;; unlock ASIC so we can access ASIC registers

  ld b,&bc
  ld hl,sequence
  ld e,17
  seq:
    ld a,(hl)
    out (c),a
    inc hl
    dec e
  jr nz,seq
;  ei 
ret

;; sequence to unlock asic
sequence: defb &ff,&00,&ff,&77,&b3,&51,&a8,&d4,&62,&39,&9c,&46,&2b,&15,&8a,&cd,&ee

splitscreen:

;;---------------------------------------------------
;; STEP 2 - Set display start address for display before split

;; this address corresponds to a start of &c000
ld hl,&2000 
; &3000 = start screen address of &c000
; &1000 = start screen address of &8000
; &0000 = start screen address of &0000
; &4000 = start screen address of &0000
; &2000 = start screen address of &8000

;; set display start address (high)
ld bc,&bc0c
out (c),c
inc b
out (c),h

;; set display start address (low)
ld bc,&bc0d
out (c),c
inc b
out (c),l

;;---------------------------------------------------
;; STEP 3 - Set display start address for display after split
;;
;; - The split is refreshed by the ASIC, so as soon as it has been initialised
;;   it will remain active until it is disabled (by writing 0 as the scan-line for the split)
;;   No extra CPU time is required to maintain the split.
;;
;; - The split address is defined the same as the CRTC display start address
;;
;; - The split address is in the same order as the CRTC display start address 
;;   (high byte followed by low byte)

;; page-in asic registers to &4000-&7fff
;ld bc,&7fb8
;out (c),c

;; set scan-line that split will activate on
ld a,135-8
ld (&6801),a

;; set screen address of split:
;; 0000 corresponds to start of &4000
; B000 CORRESPONDS TO C000
; 9000 CORRESPONDS TO 4000
ld hl,&9000 ;(0000)

ld a,h
ld (&6802),a
ld a,l
ld (&6803),a

ret

splitadr equ &4000

drawsplit:
  ld hl,splitadr
  ld b,18 ; FILL IN GAUGE AREA
  ld a,&0f
  drawsplitloop:
    push bc
    call DrawHorizLine
	call DrawHorizLine
    call DrawHorizLine
	call DrawHorizLine
    pop bc
  djnz drawsplitloop
ret

ifdef ISCART
scr_next_line_hl:
  ld a,h              ; &BC26=&0C13 (&0970)  [98] SCR_NEXT_LINE (Step a screen address down one line)
  add a,&08
  ld h,a
  and &38
  ret nz
  ld a,h
  sub &40
  ld h,a
  ld a,l
  add a,&50
  ld l,a
  ret nc
  inc h
  ld a,h
  and #7
  ret nz
  ld a,h
  sub #8
  ld h,a
ret
endif

; --------------------------------------------------------------
; CUSTOM ASSEMBLY ROUTINES FOR BUILDING MENU, KEYBOARD AND SOUND
; --------------------------------------------------------------

scr_set_mode:
  ld bc,&7f00+128+4+8+1 ; SET MODE TO 1 (LAST DIGIT SETS MODE)
  out (c),c
ret

; SCORE
; CONVERT A 16 BIT HEX TO STRING - EG FOR PLAYER MONEY
; INPUTS
; HL = NUMBER TO CONVERT

convwordtostr:
  ld bc,10000
  call convwordtostrloop
  ld (wordtostr+0),a  ; SAVE RESULT IN STRING
  ld bc,1000
  call convwordtostrloop
  ld (wordtostr+1),a  ; SAVE RESULT IN STRING
  ld bc,100
  call convwordtostrloop
  ld (wordtostr+2),a  ; SAVE RESULT IN STRING
  ld bc,10
  call convwordtostrloop
  ld (wordtostr+3),a  ; SAVE RESULT IN STRING
  ld a,l
  add 48
  ld (wordtostr+4),a  ; SAVE RESULT IN STRING
ret

; INPUTS
; BC = TENS HUNDREDS UNIT TO CHECK FOR
convwordtostrloop:
  xor a      ; RESET COUNTER
  convwordtostrloop2:
    sbc hl,bc
    inc a
  jr nc,convwordtostrloop2
  add hl,bc
  dec a
  add 48               ; CONVERT TO ASCII
ret

; -----------------------------
; KEYBOARD
; -----------------------------

;;+------------------------------------------------------
;; keyboard translation tables
;Translates keyboard row/columns to key values

;Normal key table

;;----------------------------------------------------------
keyboard_translation_tables:      ;{{Addr=$1eef Data Calls/jump count 0 Data use count 1}}
     ; BIT  0,  1,  2,  3,  4,  5,  6,  7 
		db "u","r","d","9","6","3",10, "."  ; 0 CURSOR UP, CURSOR RIGHT, CURSOR DOWN, f9, f6, f3, Enter, f.
		db "l","c","7","8","5","1","2","0"  ; 1 CURSOR LEFT, COPY, f7, f8, f5, f1, f2, f0
		db "c","[",13 ,"]","4","s",&5C,"c"  ; 2 CLEAR, [, RETURN, ], f4, SHIFT, \ (&5C), CTRL
		db "^","-","@","P",";",":","/",","  ; 3 £,=,BAR,P,;,colon,/,COMMA
		db "0","9","O","I","L","K","M","."  ; 4 0,9,O,I,L,K,M,DOT
		db "8","7","U","Y","H","J","N"," "  ; 5 8,7,U,Y,H,J,N,SPACE
		db "6","5","R","T","G","F","B","V"  ; 6 6,5,R,T,G,F,B,V
		db "4","3","E","W","S","D","C","X"  ; 7 4,3,E,W,S,D,C,X
		db "1","2","e","Q","t","A","c","Z"  ; 8 1,2,ESC,Q,TAB,A,CAPS,Z
		db "u","d","l","r","f","g","h", 7   ; 9 UP, DOWN, LEFT, RIGHT, FIRE 1, FIRE 2, UNUSED, DELETE
end_keyboard_translation_tables:


;        defb $f0,$f3,$f1,$89,$86,$83,$8b,$8a
;        defb $f2,$e0,$87,$88,$85,$81,$82,$80
;        defb $10,$5b,$0d,$5d,$84,$ff,$5c,$ff
;        defb $5e,$2d,$40,$70,$3b,$3a,$2f,$2e
;        defb $30,$39,$6f,$69,$6c,$6b,$6d,$2c
;        defb $38,$37,$75,$79,$68,$6a,$6e,$20
;        defb $36,$35,$72,$74,$67,$66,$62,$76
;        defb $34,$33,$65,$77,$73,$64,$63,$78
;        defb $31,$32,$fc,$71,$09,$61,$fd,$7a
;        defb $0b,$0a,$08,$09,$58,$5a,$ff,$7f
;
;;Shifted key table
;        defb $f4,$f7,$f5,$89,$86,$83,$8b,$8a
;        defb $f6,$e0,$87,$88,$85,$81,$82,$80
;        defb $10,$7b,$0d,$7d,$84,$ff,$60,$ff
;        defb $a3,$3d,$7c,$50,$2b,$2a,$3f,$3e
;        defb $5f,$29,$4f,$49,$4c,$4b,$4d,$3c
;        defb $28,$27,$55,$59,$48,$4a,$4e,$20
;        defb $26,$25,$52,$54,$47,$46,$42,$56
;        defb $24,$23,$45,$57,$53,$44,$43,$58
;        defb $21,$22,$fc,$51,$09,$41,$fd,$5a
;        defb $0b,$0a,$08,$09,$58,$5a,$ff,$7f
;
;;Control key table
;        defb $f8,$fb,$f9,$89,$86,$83,$8c,$8a
;        defb $fa,$e0,$87,$88,$85,$81,$82,$80
;        defb $10,$1b,$0d,$1d,$84,$ff,$1c,$ff
;        defb $1e,$ff,$00,$10,$ff,$ff,$ff,$ff
;        defb $1f,$ff,$0f,$09,$0c,$0b,$0d,$ff
;        defb $ff,$ff,$15,$19,$08,$0a,$0e,$ff
;        defb $ff,$ff,$12,$14,$07,$06,$02,$16
;        defb $ff,$ff,$05,$17,$13,$04,$03,$18
;        defb $ff,$7e,$fc,$11,$e1,$01,$fe,$1a
;        defb $ff,$ff,$ff,$ff,$ff,$ff,$ff,$7f


; Z80 BIT *,a COMMANDS AS MACHINE CODE - LITTLE ENDIAN
bit0a equ &47CB
bit1a equ &4FCB
bit2a equ &57CB
bit3a equ &5FCB
bit4a equ &67CB
bit5a equ &6FCB
bit6a equ &77CB
bit7a equ &7FCB
       
txt_pressakeyfor:  defb "Ply 1 key for ",255
txt_pressakeyfor2: defb "Ply 2 key for ",255
txt_up:            defb "up   ",255
txt_down:          defb "down ",255
txt_left:          defb "left ",255
txt_right:         defb "right",255
txt_fire:          defb "fire ",255
txt_bomb:          defb "bomb ",255
txt_eject:         defb "eject",255
  
; REDEFINE KEYS
redefinekeys:
  ld hl,redefinekeytable
  ld (redefinekeyptr),hl
  
  ld hl,&030F
  call locatetext
  ld de,txt_pressakeyfor
  call writelinede

  call doredefinekeys
  
  ; ONLY ASK PLAYER 2 KEYS IF TWO PLAYER MODE SELECTED
  ld a,(wingmanon)
  cp 2
  jp nz,setcustominput
  
  ; PLAYER 2
  ld hl,&030F
  call locatetext
  ld de,txt_pressakeyfor2
  call writelinede
  
  call doredefinekeys
  
  jp setcustominput

doredefinekeys:
  ld hl,&110F
  ld de,txt_up
  call redefinekey

  ld hl,&110F
  ld de,txt_down
  call redefinekey
  
  ld hl,&110F
  ld de,txt_left
  call redefinekey
 
  ld hl,&110F
  ld de,txt_right
  call redefinekey
  
  ld hl,&110F
  ld de,txt_fire
  call redefinekey
  
  ld hl,&110F
  ld de,txt_bomb
  call redefinekey

  ld hl,&110F
  ld de,txt_eject
  call redefinekey
  
  ; REWRITE REDEFINE KEYS TITLE
  ld hl,&030F
  call locatetext
  ld de,(redefinekeystext)
  jp writelinede

; INPUT 
; HL = POSITION ON SCREEN
; DE = TEXT TO PRINT
redefinekey:
  call locatetext           ; WRITE PROMPT FOR KEY
  call writelinede  
  waitforupkey:
    call km_wait_key_id     ; GET KEY FROM USER
    call km_wait_keyrelease
  jr nc,waitforupkey
  dec b  ; START LINE FROM 0 NOT 1 FOR LOOKUP
  ld d,b
  ld e,c
; INPUT
; D = LINE, E = BITS
savelinebitsinkeytable:
  ; GET LINE FROM TABLE
  ld a,d
  ld hl,tableoflines
  call vectorlookuphl
  ld b,h ; GET IN DE
  ld c,l

  ld hl,(redefinekeyptr)
  ld (hl),c
  inc hl
  ld (hl),b
  inc hl
  ld (redefinekeyptr),hl
  
  ; GET BITS FROM TABLE
  ld a,e
  ld hl,tableofbits
  call vectorlookuphl
  ld b,h ; GET IN BC
  ld c,l
  
  ld hl,(redefinekeyptr)
  ld (hl),c
  inc hl
  ld (hl),b
  inc hl  
  ld (redefinekeyptr),hl
ret

; REDEFINABLE JOYSTICK AND KEYS TABLE
; THIS IS USED IN THE ACTUAL GAME REGARDLESS OF INPUT METHOD 
; THE KEYS FOR KEYBOARD OR JOYSTICK ARE COPIED INTO IT
redefinekeyptr: defw 0

redefinekeytable:
  ; PLAYER 1
  defw 0,0
  defw 0,0
  defw 0,0
  defw 0,0
  defw 0,0
  defw 0,0
  defw 0,0
  ; PLAYER 2
  defw 0,0
  defw 0,0
  defw 0,0
  defw 0,0
  defw 0,0
  defw 0,0
  defw 0,0

; STANDARD JOYSTICK AND KEYS CONFIGURATION
definekeysjoystick:
  ; PLAYER 1
  defw matrix_buffer+9,bit0a
  defw matrix_buffer+9,bit1a
  defw matrix_buffer+9,bit2a
  defw matrix_buffer+9,bit3a
  defw matrix_buffer+9,bit5a ; FIRE 1
  defw matrix_buffer+9,bit4a ; FIRE 2
  defw matrix_buffer+8,bit2a
  ; PLAYER 2
  defw matrix_buffer+6,bit0a
  defw matrix_buffer+6,bit1a
  defw matrix_buffer+6,bit2a
  defw matrix_buffer+6,bit3a
  defw matrix_buffer+6,bit5a ; FIRE 1
  defw matrix_buffer+6,bit4a ; FIRE 2
  defw matrix_buffer+8,bit2a
definekeyskeyboard:
  ; PLAYER 1
  defw matrix_buffer+0,bit7a
  defw matrix_buffer+0,bit6a
  defw matrix_buffer+8,bit7a
  defw matrix_buffer+7,bit7a
  defw matrix_buffer+1,bit7a
  defw matrix_buffer+5,bit7a
  defw matrix_buffer+8,bit2a
  ; PLAYER 2
  defw matrix_buffer+6,bit5a
  defw matrix_buffer+6,bit7a
  defw matrix_buffer+8,bit7a
  defw matrix_buffer+7,bit7a
  defw matrix_buffer+1,bit7a
  defw matrix_buffer+5,bit7a
  defw matrix_buffer+8,bit2a
  
; TABLES MAKE IT EASIER TO LOOK UP ADRESSES FROM AN INTEGER LINE OR BIT
tableoflines:
  defw matrix_buffer+0
  defw matrix_buffer+1
  defw matrix_buffer+2
  defw matrix_buffer+3
  defw matrix_buffer+4
  defw matrix_buffer+5
  defw matrix_buffer+6
  defw matrix_buffer+7
  defw matrix_buffer+8
  defw matrix_buffer+9
tableofbits:
  defw bit0a
  defw bit1a
  defw bit2a
  defw bit3a
  defw bit4a
  defw bit5a
  defw bit6a
  defw bit7a

; JOYSTICK KEYS
setkeyboardbits:
  ld hl,definekeyskeyboard
  jr copykeyboardlinesbits 
setjoystickbits:
  ld hl,definekeysjoystick
  copykeyboardlinesbits:
  ld de,redefinekeytable
  ld bc,14*4
  ldir
ret
; COPY KEYS LINES AND BITS INTO FUNCTIONS TO CHECK FOR THEM
setlinesandbits:
  ; PLAYER 1
  ; UP
  ld bc,(redefinekeytable+0)
  ld (testkeybuffercommandup-2),bc
  ld bc,(redefinekeytable+2)
  ld (testkeybitcommandup-2),bc
  ; DOWN
  ld bc,(redefinekeytable+4)
  ld (testkeybuffercommanddown-2),bc
  ld bc,(redefinekeytable+6)
  ld (testkeybitcommanddown-2),bc
  ; LEFT
  ld bc,(redefinekeytable+8)
  ld (testkeybuffercommandleft-2),bc
  ld bc,(redefinekeytable+10)
  ld (testkeybitcommandleft-2),bc
  ; RIGHT
  ld bc,(redefinekeytable+12)
  ld (testkeybuffercommandright-2),bc
  ld bc,(redefinekeytable+14)
  ld (testkeybitcommandright-2),bc
  ; FIRE
  ld bc,(redefinekeytable+16)
  ld (testkeybuffercommandfire-2),bc
  ld bc,(redefinekeytable+18)
  ld (testkeybitcommandfire-2),bc
  ; BOMB
  ld bc,(redefinekeytable+20)
  ld (testkeybuffercommandspace-2),bc
  ld bc,(redefinekeytable+22)
  ld (testkeybitcommandspace-2),bc
  ; EJECT
  ld bc,(redefinekeytable+24)
  ld (testkeybuffercommandesc-2),bc
  ld bc,(redefinekeytable+26)
  ld (testkeybitcommandesc-2),bc
  ; PLAYER 2
  ; UP
  ld bc,(redefinekeytable+28)
  ld (testkeybuffercommandup2-2),bc
  ld bc,(redefinekeytable+30)
  ld (testkeybitcommandup2-2),bc
  ; DOWN
  ld bc,(redefinekeytable+32)
  ld (testkeybuffercommanddown2-2),bc
  ld bc,(redefinekeytable+34)
  ld (testkeybitcommanddown2-2),bc
  ; LEFT
  ld bc,(redefinekeytable+36)
  ld (testkeybuffercommandleft2-2),bc
  ld bc,(redefinekeytable+38)
  ld (testkeybitcommandleft2-2),bc
  ; RIGHT
  ld bc,(redefinekeytable+40)
  ld (testkeybuffercommandright2-2),bc
  ld bc,(redefinekeytable+42)
  ld (testkeybitcommandright2-2),bc
  ; FIRE
  ld bc,(redefinekeytable+44)
  ld (testkeybuffercommandfire2-2),bc
  ld bc,(redefinekeytable+46)
  ld (testkeybitcommandfire2-2),bc
  ; BOMB
  ld bc,(redefinekeytable+48)
  ld (testkeybuffercommandspace2-2),bc
  ld bc,(redefinekeytable+50)
  ld (testkeybitcommandspace2-2),bc
  ; EJECT
  ld bc,(redefinekeytable+52)
  ld (testkeybuffercommandesc2-2),bc
  ld bc,(redefinekeytable+54)
  ld (testkeybitcommandesc2-2),bc
ret

; INPUT - GENERAL
; WHEN PLAYER CHANGES FROM KEYBOARD TO JOYSTICK,
; OR REDEFINES KEYS, THESE MEMORY LOCATIONS AND BIT TESTING COMMANDS ARE OVERWRITTEN WITH CORRECT VALUES
; PLAYER 1
testkeyup:
  ld hl,matrix_buffer+9:testkeybuffercommandup
  ld a,(hl)
  bit 0,a:testkeybitcommandup
ret
testkeydown:
  ld hl,matrix_buffer+9:testkeybuffercommanddown
  ld a,(hl)
  bit 1,a:testkeybitcommanddown
ret
testkeyleft:
  ld hl,matrix_buffer+9:testkeybuffercommandleft
  ld a,(hl)
  bit 2,a:testkeybitcommandleft
ret
testkeyright:
  ld hl,matrix_buffer+9:testkeybuffercommandright
  ld a,(hl)
  bit 3,a:testkeybitcommandright
ret
testkeyspace:
  ld hl,matrix_buffer+5:testkeybuffercommandspace
  ld a,(hl)
  bit 7,a:testkeybitcommandspace
ret
testkeyfire:
  ld hl,matrix_buffer+9:testkeybuffercommandfire
  ld a,(hl)
  bit 4,a:testkeybitcommandfire
ret
testkeyesc:
  ld hl,matrix_buffer+8:testkeybuffercommandesc
  ld a,(hl)
  bit 2,a:testkeybitcommandesc
ret
; PLAYER 2
testkeyup2:
  ld hl,matrix_buffer+6:testkeybuffercommandup2
  ld a,(hl)
  bit 0,a:testkeybitcommandup2
ret
testkeydown2:
  ld hl,matrix_buffer+6:testkeybuffercommanddown2
  ld a,(hl)
  bit 1,a:testkeybitcommanddown2
ret
testkeyleft2:
  ld hl,matrix_buffer+6:testkeybuffercommandleft2
  ld a,(hl)
  bit 2,a:testkeybitcommandleft2
ret
testkeyright2:
  ld hl,matrix_buffer+6:testkeybuffercommandright2
  ld a,(hl)
  bit 3,a:testkeybitcommandright2
ret
testkeyspace2:
  ld hl,matrix_buffer+6:testkeybuffercommandspace2
  ld a,(hl)
  bit 5,a:testkeybitcommandspace2
ret
testkeyfire2:
  ld hl,matrix_buffer+6:testkeybuffercommandfire2
  ld a,(hl)
  bit 4,a:testkeybitcommandfire2
ret
testkeyesc2:
  ld hl,matrix_buffer+8:testkeybuffercommandesc2
  ld a,(hl)
  bit 2,a:testkeybitcommandesc2
ret


km_wait_key:
  call mc_wait_flyback
  call km_read_key
  jr nc,km_wait_key
ret

km_wait_key_id:
  call mc_wait_flyback
  call km_read_key_id
  jr nc,km_wait_key_id
ret

km_wait_keyrelease:
  push af
  push bc
  km_wait_keyrelease2:
  call mc_wait_flyback
  ld hl,matrix_buffer
  ld b,10
  ld a,255
  km_wait_keyreleaseloop:
    cp (hl)
	jr nz,km_wait_keyrelease2
	inc hl
  djnz km_wait_keyreleaseloop
  pop bc
  pop af
ret

; INPUT
; A = KEY TO TEST FOR
km_test_key:
  ld (mytestkey-1),a
  call km_read_key
  cp 0:mytestkey
ret

km_read_key_id:
;  call read_matrix                 ; MATRIX READ VIA INTERRUPT EVERY 50TH SECOND
  ld hl,matrix_buffer+9
  ld b,10
  doloopy2:
    ld a,(hl)
	cp 255                          ; FIND FIRST KEY IN BUFFER THAT HAS BEEN PRESSED (NOT 255)
	jr z,donextline2
	
    ; WE FOUND A KEY		
    ld c,0	
	bit 0,a
	jr z,foundbit
	inc c
	bit 1,a
	jr z,foundbit
    inc c
	bit 2,a
	jr z,foundbit
    inc c
	bit 3,a
	jr z,foundbit
    inc c
	bit 4,a
	jr z,foundbit
    inc c
	bit 5,a
	jr z,foundbit
    inc c
	bit 6,a
	jr z,foundbit
    inc c
	bit 7,a
	jr z,foundbit
	
	donextline2:
	dec hl
  djnz doloopy2
  xor a ; CLEAR CARRY
ret

foundbit:
  scf
ret

km_read_key:
;  call read_matrix                 ; MATRIX READ VIA INTERRUPT EVERY 50TH SECOND
  ld hl,matrix_buffer+9
  ld b,10
  doloopy:
    ld a,(hl)
	cp 255                          ; FIND FIRST KEY IN BUFFER THAT HAS BEEN PRESSED (NOT 255)
	jr z,donextline
	
    ; WE FOUND A KEY	
    ld c,0	
	bit 0,a
	jr z,returncharfromkeymap
	inc c
	bit 1,a
	jr z,returncharfromkeymap
	inc c
	bit 2,a
	jr z,returncharfromkeymap
	inc c
	bit 3,a
	jr z,returncharfromkeymap
	inc c
	bit 4,a
	jr z,returncharfromkeymap
	inc c
	bit 5,a
	jr z,returncharfromkeymap
	inc c
	bit 6,a
	jr z,returncharfromkeymap
	inc c
	bit 7,a
	jr z,returncharfromkeymap
	
	donextline:
	dec hl
  djnz doloopy
  xor a ; CLEAR CARRY
ret

returncharfromkeymap:
  ld hl,keyboard_translation_tables
  ld a,b
  ld b,0
  add hl,bc ; MOVE HORIZONTAL BIT COLUMN IN DATA TABLE
  push hl
  ld hl,convertsinglestotens
  ld b,0
  ld c,a
  dec c     ; START FROM LINE 0 NOT 1
  add hl,bc ; MOVE VERTICAL LINE IN TABLE
  ld c,(hl)
  pop hl
  add hl,bc
  ld a,(hl) ; GET KEYPRESS
  scf       ; SET CARRY
ret

convertsinglestotens: defb 0,8,16,24,32,40,48,56,64,72,80

waitanykey:
  call read_matrix
  ld hl,matrix_buffer
  ld b,10
  dokeyloop:
    ld a,(hl)
	cp 255
	ret nz
	inc hl
  djnz dokeyloop
jr waitanykey

matrix_buffer: defs 10  ;map with 10*8 = 80 key status bits (bit=0 key is pressed)

mc_wait_flyback:
  ; WAIT FLYBACK
  ld b,&f5
  v1b3:
  in a,(c)
  rra
  jr nc,v1b3
ret

read_matrix:
  ;call waitvsync
  ; WAIT FLYBACK
  ld b,&f5
  v1b2:
  in a,(c)
  rra
  jr nc,v1b2

keyscan:
        ;di              ;1 ##%%## C P C   VERSION ##%%##
        ld hl,matrix_buffer    ;3
        ld bc,#f782     ;3
        out (c),c       ;4
        ld bc,#f40e     ;3
        ld e,b          ;1
        out (c),c       ;4
        ld bc,#f6c0     ;3
        ld d,b          ;1
        out (c),c       ;4
        ld c,0          ;2
        out (c),c       ;4
        ld bc,#f792     ;3
        out (c),c       ;4
        ld a,#40        ;2
        ld c,#4a        ;2 44
loopa:  ld b,d          ;1
        out (c),a       ;4 select line
        ld b,e          ;1
        ini             ;5 read bits and write into KEYMAP
        inc a           ;1
        cp c            ;1
        jr c,loopa      ;2/3 9*16+1*15=159
        ld bc,#f782     ;3
        out (c),c       ;4
        ;ei              ;1 8 =211 microseconds
        ret

; ------------------------------------------------------
;  PALETTE CODE

setasicpalettemenu:
  ld hl,palettemenumaster2
  ld de,palettemainscreen
  ld bc,4*2
  ldir
  ld hl,palettemenumaster
  ld de,palettesky
  ld bc,4*2
  ldir
  ld hl,palettemenumaster
  ld de,palettesky2
  ld bc,4*2
  ldir
ret
setasicpalettegame:
  ld hl,palettegamemaster
  ld de,palettemainscreen
  ld bc,4*2
  ldir
  ld hl,paletteskymaster
  ld de,palettesky
  ld bc,4*2
  ldir
  ld hl,palettesky2master
  ld de,palettesky2
  ld bc,4*2
  ldir
ret

; PALETTES USED BY INTERRUPT
;; 0,GREEN,RED,BLUE
palettemainscreen:
  defw &0A7F ; LIGHT BLUE  
  defw &0A60 ; GREEN/YELLOW
  defw &0000 ; BLACK
  defw &0009 ; DARK BLUE - USED FOR SEA AND CLOUDS
palettesky:  ; 1 INTEGER DARKER THAN SKY2
  defw &096E ; LIGHT BLUE  
  defw &0A60 ; GREEN/YELLOW
  defw &0000 ; BLACK
  defw &0FFF ; DARK BLUE - USED FOR SEA AND CLOUDS
palettesky2:
  defw &085D ; LIGHT BLUE  
  defw &0A60 ; GREEN/YELLOW
  defw &0000 ; BLACK
  defw &0FFF ; DARK BLUE - USED FOR SEA AND CLOUDS
paletteinstruments:
  defw &0AF0 ; LIGHT BLUE  
  defw &0FFA ; GREEN/YELLOW
  defw &0000 ; BLACK
  defw &00F0 ; DARK BLUE
  
; COPIES OF PALETTE TO COPY INTO INTERRUPT
;; 0,GREEN,RED,BLUE
palettegamemaster:
  defw &0A7F ; LIGHT BLUE  
  defw &0A60 ; GREEN/YELLOW
  defw &0000 ; BLACK
  defw &0009 ; DARK BLUE
palettemenumaster:
  defw &0FA0 ; LIGHT BLUE  
  defw &0FFA ; GREEN/YELLOW
  defw &0000 ; BLACK
  defw &0F00 ; DARK BLUE
palettemenumaster2:
  defw &0A0F ; LIGHT BLUE  
  defw &0F0F ; GREEN/YELLOW
  defw &0000 ; BLACK
  defw &050F ; DARK BLUE
paletteskymaster:  ; 1 INTEGER DARKER THAN SKY2
  defw &096E ; LIGHT BLUE  
  defw &0A60 ; GREEN/YELLOW
  defw &0000 ; BLACK
  defw &0FFF ; DARK BLUE - USED FOR SEA AND CLOUDS
palettesky2master:
  defw &085D ; LIGHT BLUE  
  defw &0A60 ; GREEN/YELLOW
  defw &0000 ; BLACK
  defw &0FFF ; DARK BLUE - USED FOR SEA AND CLOUDS

; PALETTE FADE FROM DAY TO DUSK TO NIGHT, REPEATED IN LOOP
; FASTEST WAY IS TO MAKE A TABLE AND READ THEM DIRECTLY INTO PALETTE AS NEEDED
; DAY TO DUSK = 5, DUSK TO NIGHT = 5, NIGHT TO DAY = 10
;  BLUE- GREEN RED
palettefadecount: defb 0 ; SET TO 5, 10, ETC IF WE NEED TO DO FADE
palettefadeptr:   defw palettefadetable ; POINTER TO CURRENT PALETTE IN USE
palettefadetable:
  ; SKY,      LAND,  BLACK, SEA,  CLOUD COLOUR
  defw &0A7F, &0A60, &0000, &0229, &0FFF ; START DAY
  defw &098D, &0960, &0000, &0229, &0FFE 
  defw &089B, &0860, &0000, &0229, &0FED 
  defw &07AA, &0760, &0000, &0239, &0FEC
  defw &06B9, &0660, &0000, &0249, &0FDB 
  duskpal:
  defw &05C8, &0560, &0000, &0259, &0FDA ; DUSK
  defw &05A7, &0450, &0000, &0248, &0EC9
  defw &0486, &0440, &0000, &0237, &0DC8 
  defw &0456, &0320, &0000, &0226, &0CB7
  defw &0345, &0210, &0000, &0225, &0BB6 
  nightpal:
  defw &4503, &0100, &0000, &0224, &0AA5 ; NIGHT
  defw &4614, &0120, &0000, &0225, &0BB6  
  defw &3625, &0320, &0000, &0226, &0CB7  
  defw &2736, &0440, &0000, &0237, &0DC8  
  defw &1747, &0550, &0000, &0248, &0EC9  
  defw &0758, &0560, &0000, &0259, &0FDA ; DAWN
  defw &0869, &0660, &0000, &0249, &0FDB 
  defw &087A, &0760, &0000, &0239, &0FEC 
  defw &097B, &0860, &0000, &0229, &0FED  
  defw &097D, &0960, &0000, &0229, &0FFE 
  ;defw &0A7F, &0A60, &0000, &0229 ; START DAY
  defw &0000 ; END OF TABLE

; SWITCH SEA PALETTE FOR TOWN LIGHTS
backuppal: defw 0
backuppal2: defw 0

; DON'T DO PALETTE FADE ON FIRST TAKEOFF, KEEP IT AS A SURPRISE
notfirstmission: defb 0

startpalettefade:
  ld a,(notfirstmission)
  or a
  jr z,setfirstmissionvariable
  ld a,5;5;10;20
  ld (palettefadecount),a
  jp enablepalettecheck
resetfirstmissionvariable:
  ; RESET FIRST MISSION VARIABLE
  ;ld a,1
  xor a
  ld (notfirstmission),a
  ; BACK TO DAY COLOURS
  ld hl,palettefadetable
  ld (palettefadeptr),hl
  ; UPDATE ASIC WITH NEW PALETTE
  jp skipresetpalettefadecount
setfirstmissionvariable:
  ld a,1
  ld (notfirstmission),a
ret

setnextpaletteland:
  ; DECREMENT PALETTE CHANGE COUNT
  ld a,(palettefadecount)
  or a
  jr z,disablesecondinterrupt
  dec a
  ld (palettefadecount),a

  ; UPDATE PALETTE FADE POINTER TO NEXT COLOURS
  ld hl,(palettefadeptr)
  ld bc,10
  add hl,bc
  ld (palettefadeptr),hl
  
  ; CHECK IF WE REACHED END OF TABLE, IF SO GO BACK TO START
  ld a,(hl)  
  or a
  jr nz,skipresetpalettefadecount
  
  ld hl,palettefadetable
  ld (palettefadeptr),hl
  
  skipresetpalettefadecount:
  ; COPY NEW COLOURS DIRECTLY INTO PALETTE USED BY INTERRUPT
  ld hl,(palettefadeptr)
  ld de,palettemainscreen
  ld bc,8
  ldir

  ; SET DARKER COLOURS FOR SKY1 2
  ld hl,(palettemainscreen)
  dec h
  dec l
  ld (palettesky),hl
  dec h
  dec l 
  ld (palettesky2),hl
  ld hl,(palettemainscreen+2)
  ld (palettesky+2),hl
  ld (palettesky2+2),hl
  
  ; SET CLOUD COLOUR BASED ON CURRENT PALETTE FADE
  ld hl,(palettefadeptr)
  inc hl
  inc hl
  inc hl
  inc hl
  inc hl
  inc hl
  inc hl
  inc hl
  ld a,(hl)
  inc hl
  ld h,(hl)
  ld l,a
   
  ; CLOUDS COLOUR
  ld (palettesky+6),hl
  ld (palettesky2+6),hl
ret

; INPUT
; HL = TABLE OF 16BIT NUMBERS
; A = INDEX
; OUTPUT
; HL = 16 BIT NUMBER FROM LIST
vectorlookuphl:
  push bc
  ;rlca ; DOES NOT WORK OVER TABLE LENGTH > 128
  ld b,0
  ld c,a
  add hl,bc
  add hl,bc
  ld a,(hl)
  inc hl
  ld h,(hl)
  ld l,a
  pop bc
ret

; VECTOR TABLE LAUNCH CODE
; INPUTS 
; A = ID NUMBER
; HL = LOOKUP TABLE
vectortablelaunchcode:
  call vectorlookuphl
  jp (hl)     ; JUMP TO VECTOR TABLE FUNCTION

enablepalettecheck:
  ld a,&CD  ; CALL = &CD
  ld bc,setnextpaletteland ; secondinterruptcommand
  jr setsecondinterrupt
; DISABLE PALETTE FADE / CURSOR BLINK
disablesecondinterrupt:
  xor a;ld a,0  ; CALL = &CD
  ld bc,0 ; secondinterruptcommand
  jr setsecondinterrupt
; ENABLE THE CURSOR TO BLINK ON THE SCOREBOARD
enableblinkcursor:
  ld a,&CD  ; CALL = &CD
  ld bc,blinkcursor ; secondinterruptcommand
  setsecondinterrupt:
  ld (secondinterruptcommand-1),a
  ld (secondinterruptfunction-2),bc
ret

; -GRB
sprite_colours:
  defw &0111			;; colour for sprite pen 1
  defw &0333			;; colour for sprite pen 2
  defw &0555			;; colour for sprite pen 3
  defw &0777			;; colour for sprite pen 4
  defw &0aaa			;; colour for sprite pen 5
  defw &0fff			;; colour for sprite pen 6
  defw &0200			;; colour for sprite pen 7
  defw &0401			;; colour for sprite pen 8
  defw &0623			;; colour for sprite pen 9
  defw &0845			;; colour for sprite pen 10
  defw &0b78			;; colour for sprite pen 11
  defw &0fff			;; colour for sprite pen 12
  defw &0ddd			;; colour for sprite pen 13
  defw &0eee			;; colour for sprite pen 14
sprite_parachute_colour:
  defw &0fff			;; colour for sprite pen 15 - parachute colour
 
;;---------------------------------------------
;; - there is one pixel per byte (bits 3..0 of each byte define the palette index for this pixel)
;; - these bytes are stored in a form that can be written direct to the ASIC
;; sprite pixel data

; POINTERS TO THE PLUS SPRITE DEFINITIONS IN AMSTRADFONT.asm
; WE HAVE RUN OUT OF ROOM HERE (0000-4000), SO THEY ARE STORED AT THE END OF MEMORY
; TO MAKE MORE ROOM FOR PROGRAM CODE

sprite_pixel_data1 equ &f000
sprite_pixel_data2 equ sprite_pixel_data1+128
sprite_pixel_data4 equ sprite_pixel_data2+128
sprite_pixel_data3 equ sprite_pixel_data4+128
sprite_pixel_data5 equ sprite_pixel_data3+128
sprite_pixel_data6 equ sprite_pixel_data5+128
sprite_pixel_data7 equ sprite_pixel_data6+128
sprite_pixel_data8 equ sprite_pixel_data7+128
sprite_pixel_data10 equ sprite_pixel_data8+128
sprite_pixel_data11 equ sprite_pixel_data10+128
sprite_pixel_data12 equ sprite_pixel_data11+128
sprite_pixel_data13 equ sprite_pixel_data12+128
sprite_pixel_data14 equ sprite_pixel_data13+256
sprite_pixel_data15 equ sprite_pixel_data14+256
sprite_pixel_data16 equ sprite_pixel_data15+256
sprite_pixel_data17 equ sprite_pixel_data16+256
sprite_pixel_data18 equ sprite_pixel_data17+128
sprite_pixel_data19 equ sprite_pixel_data18+128
sprite_pixel_data20 equ sprite_pixel_data19+128
sprite_pixel_data21 equ sprite_pixel_data20+128

; INPUT
; HL = BUFFER TO CLEAR
; BC = SIZE OF DATA TO CLEAR
clearmem:
  push hl
  dec bc
  push hl
  pop de
  inc de
  ld (hl),0
  ldir
  pop hl
ret

; EMPTY DATA - USED TO CLEAR UNUSED PORTION OF SPRITE - NEEDED AS ASIC IS NOT CLEAR BY DEFAULT ON REAL HARDWARE
;empty_sprite_data:
;defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
;defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
;defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
;defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
;defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
;defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
;defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
;defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0

defb "CHRIS4"

endofdata:
;SAVE ’myfile.bin’,start,size,DSK,’fichierdsk.dsk’




ifndef ISCART
ifndef ISCASSETTE
;defb "CHRIS4"
; RASM INSERT INTO DSK
SAVE "HARRIER2.BIN",&C100,&3EFF,DSK,"../makecartridge/build/HarrierAttackReloaded.dsk"
SAVE "HARRIER1.BIN",&0100,&3EFF,DSK,"../makecartridge/build/HarrierAttackReloaded.dsk"
endif
ifdef ISCASSETTE
save "HARRIER2.BIN",&C100,&3EFF ; LEAVE 100 FOR THE STACK FOR LOADER!
SAVE "HARRIER1.BIN",&0100,&3EFF
endif
endif
;ifndef iscasette
;save direct "HARRIER2.BIN",&C100,&0FD0 ; LEAVE 100 FOR THE STACK FOR LOADER!
;SAVE direct "HARRIER1.BIN",&0100,&3EFF
;endif
;endif