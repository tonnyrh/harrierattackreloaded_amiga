;org &0100
org &C100;8000
nolist
;scraddr equ &C000
; MAIN JUMP TABLE
jp main
jp writecharf            ;03
jp writelinef            ;06
jp writelinedef          ;09
jp writelineplaindef;panelde  ;0C
jp locatetextf           ;0F
jp locatetextfde         ;12
jp writecursorf          ;15
jp writecharcursorf      ;18
jp blinkcursorf          ;1B
jp writecharplainf       ;

jp initsoundf            ;1E
;jp silencechannelsf      ;21
jp 0;dodingnoise            ;24

ifdef HARRIERATTACK
jp domissilenoisef         ;27
jp dobombnoisef;missilesoundc;2A
jp doloudbombnoisef
jp doflightnoisef          ;2D
jp doflacknoisef            ;30
jp dofrigatenoisef         ;33
jp doexplosionnoisef        ;36
endif
jp playsound_all_channelsf  ;39
jp initmusicf
jp playmusicf
jp playsound_a
jp playsound_b
jp playsound_c

ifdef HARRIERATTACK
texttablef:
  defw titletext2f
  defw skillleveltext2f
  defw joysticktextf
  defw keyboardtextf
  defw customtextf
  defw scoreboardheader2f
  defw highscoretext2f
  defw speedtextf
  defw fueltextf
  defw rocketstextf
  defw bombstextf
  defw skillleveltext3f
  defw redefinekeystextf
  defw startgametextf
  defw controlstextf
  defw inputmethodf
  defw tributetextf
  defw rocketrangetext2f
  defw rocketrangetext3f
  defw trailstext2f
  defw trailstext3f
  defw lockmissiletext2f
  defw armourtextf
  defw wingmantextf
  defw spritelookuptable
  ;defw smoothscrolltextf
  ;defw smoothscrolltext2f
  ;defw smoothscrolltext3f
  
; MODIFIED STRINGS
titletext2f:        defb "* HARRIER ATTACK RELOADED *",255
skillleveltext2f:   defb "Skill level:",255
rocketrangetext2f:  defb "Rocket range:",255
rocketrangetext3f:  defb "10",255
trailstext2f:       defb "Contrails:",255
lockmissiletext2f:  defb "Lock height:",255
trailstext3f:       defb "No",255
controlstextf:      defb "Input: ",255
redefinekeystextf:  defb "Redefine keys      ",255 ; SPACES CLEAR OLD REDFINED KEY TEXTS
startgametextf:     defb "Start game",255
joysticktextf:      defb "Joystick",255
keyboardtextf:      defb "Keyboard",255
customtextf:        defb "Custom  ",255 ; SPACES CLEAR JOYSTICK KEYBOARD
skillleveltext3f:   defb "1",255
scoreboardheader2f: defb "NAME      LEVEL      HITS     SCORE",255
highscoretext2f:    defb "SCORE; 000000  HIGH SCORE; 000000",255
speedtextf:         defb "SPEED",255
fueltextf:          defb "FUEL",255
rocketstextf:       defb "ROCKETS",255
bombstextf:         defb "BOMBS",255
inputmethodf:       defb "Joystick",255
tributetextf:       defb " Harrier Attack! - Game concept by Robert White, founder and director of Durell Software. Program code by Mike Richardson. Published by Amsoft 1984. Plus adaption by Chris Perver 2025. Hope you enjoy! ",255
armourtextf:        defb "ARMOUR",255
wingmantextf:       defb "Wingman:",255
;smoothscrolltextf:  defb "Movement: ",255
;smoothscrolltext2f: defb "Classic",255
;smoothscrolltext3f: defb "Pixels ",255
endif

locatetable:
  defw &8000 ;1
  defw &8050 ;2 
  defw &80a0 ;3
  defw &80f0 ;4
  defw &8140 ;5
  defw &8190 ;6
  defw &81e0 ;7
  defw &8230 ;8
  defw &8280 ;9
  defw &82d0 ;10
  defw &8320 ;11
  defw &8370 ;12
  defw &83c0 ;13
  defw &8410 ;14
  defw &8460 ;15
  defw &84b0 ;16
  defw &8500 ;17
  defw &8550 ;18
  defw &85a0 ;19
  defw &85f0 ;20
  defw &8640 ;21
  defw &8690 ;22
  defw &86e0 ;23
  defw &8730 ;24
  defw &8780 ;25

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

; INPUT
; H = X ROW
; L = Y COL
; OUTPUT
; HL = SCREEN POS

; HL, BC, A CORRUPT
locatetextf:
  ld a,h
  push af
  ld a,l
  rlca ; DOUBLE FOR LOOKUP
  ld hl,locatetable
  ld b,0
  ld c,a
  add hl,bc
  ld a,(hl)
  inc hl
  ld h,(hl)
  ld l,a
  pop af
  rlca ; DOUBLE FOR HORIZ BYTES  
  ld c,a
  add hl,bc
ret
 
; INPUT
; H = X ROW
; L = Y COL
; OUTPUT
; DE = SCREEN POS

locatetextfde:
  push hl

  ld a,h
  push af
  ld a,l
  rlca ; DOUBLE FOR LOOKUP
  ld hl,locatetable
  ld b,0
  ld c,a
  add hl,bc
  ld a,(hl)
  inc hl
  ld h,(hl)
  ld l,a
  pop af
  rlca ; DOUBLE FOR HORIZ BYTES  
  ld c,a
  add hl,bc

  ld d,h
  ld e,l
  pop hl
ret

xyloc: defw 0   
	
; INPUTS
; A  = CHAR
; DE = SCREEN POS
writecharf:
  push hl
  sub 32             ; START WITH SPACE
  rlca               ; DOUBLE FOR LOOKUP
  ld hl,amsfonttable
  ld l,a
  ; MOVE TO ACTUAL TABLE DATA IN HL
  ld a,(hl)
  inc hl
  ld h,(hl)
  ld l,a
  push de
  ; WRITE TO SCREEN
  call writecharpixellinered2
  call writecharpixellinered2
  call writecharpixellinered
  call writecharpixellinered
  call writecharpixellinered
  call writecharpixelline
  call writecharpixelline
  call writecharpixelline
  pop de
  inc de ; MOVE TO NEXT CHAR SPACE
  inc de
  pop hl
ret

; INPUTS
; A  = CHAR
; DE = SCREEN POS
writecharplainf:
  push hl
  sub 32             ; START WITH SPACE
  rlca               ; DOUBLE FOR LOOKUP
  ld hl,amsfonttable
  ld l,a
  ; MOVE TO ACTUAL TABLE DATA IN HL
  ld a,(hl)
  inc hl
  ld h,(hl)
  ld l,a
  push de
  ; WRITE TO SCREEN
  call writecharpixellineplain
  call writecharpixellineplain
  call writecharpixellineplain
  call writecharpixellineplain
  call writecharpixellineplain
  call writecharpixellineplain
  call writecharpixellineplain
  call writecharpixellineplain
  pop de
  inc de ; MOVE TO NEXT CHAR SPACE
  inc de
  pop hl
ret

; INPUT
; A = CHAR
; DE = SCREEN POS
writecharcursorf:
  call writecharf
  jp writecursorf

; RECORD LAST POSITION SO WE CAN BLINK IT
cursorpos: defw 0

blinkcursorf:
  ld de,(cursorpos)
  ld a,d
  or a                  ; DON'T BLINK IF WE DON'T HAVE A POSITION
  ret z
  ld b,8
  writecursorloop2:
    ; WRITE TO SCREEN
	push de
    ld a,(de)
    xor &0f
    ld (de),a
    inc de
    ld a,(de)
    xor &0f
    ld (de),a
	pop de
  
    ex de,hl              ; MOVE TO NEXT PIXEL LINE DOWN
    call scr_next_line_hl
    ex de,hl
  djnz writecursorloop2
ret

; INPUTS
; DE = SCREEN POS
; XOR SAME POSITION TO ERASE AGAIN
writecursorf:
  push de
  push hl
  
  ld b,8
  writecursorloop:
    ; WRITE TO SCREEN
	push de
    ld a,(de)
    xor &0f
    ld (de),a
    inc de
    ld a,(de)
    xor &0f
    ld (de),a
	pop de
  
    ex de,hl              ; MOVE TO NEXT PIXEL LINE DOWN
    call scr_next_line_hl
    ex de,hl
  djnz writecursorloop 
  
  pop hl
  pop de
  ld (cursorpos),de
ret


; MULTICOLOURED FONT
; DE = LINE TO WRITE, ENDING IN 255
; HL = SCREEN POS
writelinedef:
  ex de,hl
; INPUT
; HL = LINE TO WRITE, ENDING IN 255
; DE = SCREEN POS
writelinef:
  ld a,(hl)
  cp 255
  ret z
  call writecharf
  inc hl
  jr writelinef
  
; PLAIN FONT
; DE = LINE TO WRITE, ENDING IN 255
; HL = SCREEN POS
writelineplaindef:
  ex de,hl
; INPUT
; HL = LINE TO WRITE, ENDING IN 255
; DE = SCREEN POS
writelineplainf:
  ld a,(hl)
  cp 255
  ret z
  call writecharplainf
  inc hl
  jr writelineplainf  

;mytext: defb "CHRIS PERVER",255
main:
;   ld hl,&0909
;   call locatetextf
;   ld de,mytext
;   jp writelinedef

writecharpixellineplain:
  push de
  ; LEFT BYTE
  ld a,(hl)
  ld (de),a
  
  ; RIGHT BYTE
  inc de
  inc hl
  
  ld a,(hl)
  ld (de),a
  inc hl

  pop de
  ex de,hl              ; MOVE TO NEXT PIXEL LINE DOWN
  call scr_next_line_hl
  ex de,hl
ret

writecharpixelline:
  push de
  ; LEFT BYTE
  ld a,(hl)
  or %00001111          ; SET MASK - REMOVE BACKGROUND COLOUR
  ld (de),a
  
  ; RIGHT BYTE
  inc de
  inc hl
  
  ld a,(hl)
  or %00001111          ; SET MASK - REMOVE BACKGROUND COLOUR
  ld (de),a
  inc hl

  pop de
  ex de,hl              ; MOVE TO NEXT PIXEL LINE DOWN
  call scr_next_line_hl
  ex de,hl
ret


writecharpixellinered:
  push de
  ; LEFT BYTE
  ld a,(hl)
  and %00001111
  ld (de),a
  
  ; RIGHT BYTE
  inc de
  inc hl
  
  ld a,(hl)
  and %00001111
  ld (de),a
  inc hl

  pop de
  ex de,hl              ; MOVE TO NEXT PIXEL LINE DOWN
  call scr_next_line_hl
  ex de,hl
ret

writecharpixellinered2:
  push de
  ; LEFT BYTE
  ld a,(hl)
  ld (de),a
  
  ; RIGHT BYTE
  inc de
  inc hl
  
  ld a,(hl)
  ld (de),a
  inc hl

  pop de
  ex de,hl              ; MOVE TO NEXT PIXEL LINE DOWN
  call scr_next_line_hl
  ex de,hl
ret

align 256
amsfonttable:
  defw t1 ; SPACE
  defw t2 ; !
  defw t3
  defw t4
  defw t5
  defw t6
  defw t7
  defw t8
  defw t9
  defw t10
  defw t11
  defw t12
  defw t13
  defw t14
  defw t15
  defw t16
  defw t17 ; 1
  defw t18 ; 2
  defw t19
  defw t20
  defw t21
  defw t22
  defw t23
  defw t24
  defw t25
  defw t26
  defw t27
  defw t28
  defw t29
  defw t30 ; >
  defw t31
  defw t32
  defw t33
  defw t34
  defw t35
  defw t36
  defw t37
  defw t38
  defw t39
  defw t40 ; H
  defw t41 ; I
  ;defw t42 ; J
  defw t43 ; K
  defw t44 ; L
  defw t45 ; M
  defw t46 ; N
  defw t47
  defw t48
  defw t49
  defw t50
  defw t51
  defw t52
  defw t53
  defw t54
  defw t55
  defw t56
  defw t57
  defw t58
  defw t59
  defw t60
  defw t61
  defw t62
  defw t63
  defw t64
  defw t65
  defw t66
  defw t67
  defw t68
  defw t69
  defw t70
  defw t71
  defw t72
  defw t73
  defw t74
  defw t75
  defw t76
  defw t77
  defw t78
  defw t79
  defw t80
  defw t81
  defw t82
  ;defw t83
  defw t84
  defw t85
  defw t86
  defw t87
  defw t88
  defw t89
  defw t90
  defw t91
  defw t92
  defw t93
  defw t94
  defw t95
  defw t96
  defw t97
  defw t98
  defw t99
  defw t100
  defw t101
  
amsfont:
t1:
db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
t2:
db #1e,#0f,#1e,#0f,#1e,#0f,#1e,#0f
db #1e,#0f,#0f,#0f,#1e,#0f,#0f,#0f
t3:
db #2d,#4b,#2d,#4b,#0f,#0f,#0f,#0f
db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
t4:
db #2d,#4b,#2d,#4b,#78,#e1,#2d,#4b
db #2d,#4b,#78,#e1,#2d,#4b,#2d,#4b
t5:
db #3c,#c3,#4b,#a5,#4b,#87,#3c,#c3
db #0f,#a5,#4b,#a5,#3c,#c3,#0f,#0f
t6:
db #0f,#0f,#69,#2d,#69,#4b,#0f,#87
db #1e,#0f,#2d,#69,#4b,#69,#0f,#0f
t7:
db #3c,#87,#4b,#4b,#2d,#87,#1e,#0f
db #2d,#a5,#4b,#4b,#3c,#a5,#0f,#0f
t8:
db #0f,#4b,#0f,#87,#1e,#0f,#0f,#0f
db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
t9
db #0f,#4b,#0f,#87,#1e,#0f,#1e,#0f
db #1e,#0f,#0f,#87,#0f,#4b,#0f,#0f
t10:
db #1e,#0f,#0f,#87,#0f,#4b,#0f,#4b
db #0f,#4b,#0f,#87,#1e,#0f,#0f,#0f
t11:
db #0f,#0f,#4b,#2d,#2d,#4b,#1e,#87
db #2d,#4b,#4b,#2d,#0f,#0f,#0f,#0f
t12:
db #0f,#0f,#0f,#87,#0f,#87,#3c,#e1
db #0f,#87,#0f,#87,#0f,#0f,#0f,#0f
t13:
db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
db #0f,#0f,#0f,#4b,#0f,#87,#1e,#0f
t14:
db #0f,#0f,#0f,#0f,#0f,#0f,#78,#e1
db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
t15:
db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
db #0f,#0f,#0f,#0f,#0f,#87,#0f,#0f
t16:
db #0f,#2d,#0f,#4b,#0f,#87,#1e,#0f
db #2d,#0f,#4b,#0f,#87,#0f,#0f,#0f
t17:
db #78,#c3,#87,#69,#87,#a5,#96,#2d
db #a5,#2d,#c3,#2d,#78,#c3,#0f,#0f
t18:
db #0f,#87,#1e,#87,#2d,#87,#0f,#87
db #0f,#87,#0f,#87,#0f,#87,#0f,#0f
t19:
db #3c,#c3,#4b,#2d,#0f,#2d,#3c,#c3
db #4b,#0f,#4b,#0f,#78,#e1,#0f,#0f
t20:
db #3c,#c3,#4b,#2d,#0f,#2d,#1e,#c3
db #0f,#2d,#4b,#2d,#3c,#c3,#0f,#0f
t21:
db #1e,#87,#2d,#87,#4b,#87,#87,#87
db #f0,#e1,#0f,#87,#0f,#87,#0f,#0f
t22:
db #78,#e1,#4b,#0f,#4b,#0f,#3c,#c3
db #0f,#2d,#4b,#2d,#3c,#c3,#0f,#0f
t23:
db #3c,#c3,#4b,#0f,#4b,#0f,#78,#c3
db #4b,#2d,#4b,#2d,#3c,#c3,#0f,#0f
t24:
db #78,#e1,#0f,#2d,#0f,#4b,#0f,#87
db #1e,#0f,#2d,#0f,#4b,#0f,#0f,#0f
t25:
db #3c,#c3,#4b,#2d,#4b,#2d,#3c,#c3
db #4b,#2d,#4b,#2d,#3c,#c3,#0f,#0f
t26:
db #3c,#c3,#4b,#2d,#4b,#2d,#3c,#e1
db #0f,#2d,#0f,#2d,#3c,#c3,#0f,#0f
t27:
db #0f,#0f,#0f,#0f,#0f,#87,#0f,#87
db #0f,#0f,#0f,#87,#0f,#87,#0f,#0f
t28:
db #0f,#0f,#0f,#0f,#0f,#87,#0f,#87
db #0f,#0f,#0f,#87,#1e,#0f,#0f,#0f
t29:
db #0f,#4b,#0f,#87,#1e,#0f,#2d,#0f
db #1e,#0f,#0f,#87,#0f,#4b,#0f,#0f
t30:
db #0f,#0f,#0f,#0f,#78,#e1,#0f,#0f
db #0f,#0f,#78,#e1,#0f,#0f,#0f,#0f
t31:
db #2d,#0f,#1e,#0f,#0f,#87,#0f,#4b
db #0f,#87,#1e,#0f,#2d,#0f,#0f,#0f
t32:
db #3c,#c3,#4b,#2d,#0f,#2d,#0f,#4b
db #0f,#87,#0f,#0f,#0f,#87,#0f,#0f
t33:
db #3c,#c3,#4b,#2d,#5a,#e1,#5a,#2d
db #5a,#e1,#4b,#0f,#3c,#c3,#0f,#0f
t34:
db #1e,#87,#2d,#4b,#4b,#2d,#4b,#2d
db #78,#e1,#4b,#2d,#4b,#2d,#0f,#0f
t35:
db #78,#c3,#4b,#2d,#4b,#2d,#78,#c3
db #4b,#2d,#4b,#2d,#78,#c3,#0f,#0f
t36:
db #3c,#c3,#4b,#2d,#87,#0f,#87,#0f
db #87,#0f,#4b,#2d,#3c,#c3,#0f,#0f
t37:
db #78,#87,#4b,#4b,#4b,#2d,#4b,#2d
db #4b,#2d,#4b,#4b,#78,#87,#0f,#0f
t38:
db #78,#e1,#4b,#0f,#4b,#0f,#78,#87
db #4b,#0f,#4b,#0f,#78,#e1,#0f,#0f
t39:
db #78,#e1,#4b,#0f,#4b,#0f,#78,#87
db #4b,#0f,#4b,#0f,#4b,#0f,#0f,#0f
t40:
db #1e,#c3,#2d,#2d,#4b,#0f,#4b,#0f
db #4b,#69,#2d,#2d,#1e,#c3,#0f,#0f
t41:
db #4b,#2d,#4b,#2d,#4b,#2d,#78,#e1
db #4b,#2d,#4b,#2d,#4b,#2d,#0f,#0f
;t42:
;db #4b,#2d,#4b,#2d,#4b,#2d,#78,#e1
;db #4b,#2d,#4b,#2d,#4b,#2d,#0f,#0f
t43:
db #78,#c3,#1e,#0f,#1e,#0f,#1e,#0f
db #1e,#0f,#1e,#0f,#78,#c3,#0f,#0f
t44:
db #1e,#e1,#0f,#4b,#0f,#4b,#0f,#4b
db #0f,#4b,#87,#4b,#78,#87,#0f,#0f
t45:
db #4b,#2d,#4b,#4b,#4b,#87,#78,#0f
db #4b,#87,#4b,#4b,#4b,#2d,#0f,#0f
t46:
db #4b,#0f,#4b,#0f,#4b,#0f,#4b,#0f
db #4b,#0f,#4b,#0f,#78,#e1,#0f,#0f
t47:
db #c3,#69,#a5,#a5,#96,#2d,#87,#2d
db #87,#2d,#87,#2d,#87,#2d,#0f,#0f
t48:
db #87,#2d,#c3,#2d,#a5,#2d,#96,#2d
db #87,#a5,#87,#69,#87,#2d,#0f,#0f
t49:
db #3c,#87,#4b,#4b,#87,#2d,#87,#2d
db #87,#2d,#4b,#4b,#3c,#87,#0f,#0f
t50:
db #78,#c3,#4b,#2d,#4b,#2d,#78,#c3
db #4b,#0f,#4b,#0f,#4b,#0f,#0f,#0f
t51:
db #3c,#87,#4b,#4b,#87,#2d,#87,#2d
db #87,#a5,#4b,#4b,#3c,#a5,#0f,#0f
t52:
db #78,#c3,#4b,#2d,#4b,#2d,#78,#c3
db #4b,#4b,#4b,#2d,#4b,#2d,#0f,#0f
t53:
db #3c,#c3,#4b,#2d,#4b,#0f,#3c,#c3
db #0f,#2d,#4b,#2d,#3c,#c3,#0f,#0f
t54:
db #78,#c3,#1e,#0f,#1e,#0f,#1e,#0f
db #1e,#0f,#1e,#0f,#1e,#0f,#0f,#0f
t55:
db #4b,#2d,#4b,#2d,#4b,#2d,#4b,#2d
db #4b,#2d,#4b,#2d,#3c,#c3,#0f,#0f
t56:
db #4b,#2d,#4b,#2d,#4b,#2d,#4b,#2d
db #4b,#2d,#2d,#4b,#1e,#87,#0f,#0f
t57:
db #87,#2d,#87,#2d,#87,#2d,#87,#2d
db #96,#2d,#a5,#a5,#c3,#69,#0f,#0f
t58:
db #87,#2d,#4b,#4b,#2d,#87,#1e,#0f
db #2d,#87,#4b,#4b,#87,#2d,#0f,#0f
t59:
db #4b,#4b,#4b,#4b,#4b,#4b,#2d,#87
db #1e,#0f,#1e,#0f,#1e,#0f,#0f,#0f
t60:
db #f0,#e1,#0f,#4b,#0f,#87,#1e,#0f
db #2d,#0f,#4b,#0f,#f0,#e1,#0f,#0f
t61:
db #3c,#c3,#2d,#0f,#2d,#0f,#2d,#0f
db #2d,#0f,#2d,#0f,#3c,#c3,#0f,#0f
t62:
db #87,#0f,#4b,#0f,#2d,#0f,#1e,#0f
db #0f,#87,#0f,#4b,#0f,#2d,#0f,#0f
t63:
db #3c,#c3,#0f,#4b,#0f,#4b,#0f,#4b
db #0f,#4b,#0f,#4b,#3c,#c3,#0f,#0f
t64:
db #1e,#87,#3c,#c3,#78,#e1,#78,#e1
db #1e,#87,#1e,#87,#1e,#87,#0f,#0f
t65:
db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
db #0f,#0f,#0f,#0f,#0f,#0f,#f0,#f0
t66:
db #3c,#0f,#1e,#87,#0f,#c3,#0f,#0f
db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
t67:
db #0f,#0f,#0f,#0f,#3c,#c3,#4b,#2d
db #4b,#2d,#4b,#69,#3c,#a5,#0f,#0f
t68:
db #4b,#0f,#4b,#0f,#78,#c3,#4b,#2d
db #4b,#2d,#4b,#2d,#3c,#c3,#0f,#0f
t69:
db #0f,#0f,#0f,#0f,#3c,#c3,#4b,#2d
db #4b,#0f,#4b,#2d,#3c,#c3,#0f,#0f
t70:
db #0f,#2d,#0f,#2d,#3c,#e1,#4b,#2d
db #4b,#2d,#4b,#2d,#3c,#c3,#0f,#0f
t71:
db #0f,#0f,#0f,#0f,#3c,#c3,#4b,#2d
db #78,#e1,#4b,#0f,#3c,#c3,#0f,#0f
t72:
db #1e,#c3,#2d,#2d,#2d,#0f,#78,#0f
db #2d,#0f,#2d,#0f,#2d,#0f,#0f,#0f
t73:
db #0f,#0f,#0f,#0f,#3c,#e1,#4b,#2d
db #4b,#2d,#3c,#e1,#0f,#2d,#78,#c3
t74:
db #4b,#0f,#4b,#0f,#5a,#c3,#69,#2d
db #4b,#2d,#4b,#2d,#4b,#2d,#0f,#0f
t75:
db #0f,#87,#0f,#0f,#0f,#87,#0f,#87
db #0f,#87,#0f,#87,#0f,#87,#0f,#0f
t76:
db #0f,#2d,#0f,#0f,#0f,#2d,#0f,#2d
db #0f,#2d,#0f,#2d,#4b,#2d,#3c,#c3
t77:
db #2d,#0f,#2d,#0f,#2d,#2d,#2d,#4b
db #3c,#87,#2d,#4b,#2d,#2d,#0f,#0f
t78:
db #1e,#0f,#1e,#0f,#1e,#0f,#1e,#0f
db #1e,#0f,#1e,#0f,#0f,#87,#0f,#0f
t79:
db #0f,#0f,#0f,#0f,#69,#c3,#96,#2d
db #96,#2d,#96,#2d,#87,#2d,#0f,#0f
t80:
db #0f,#0f,#0f,#0f,#5a,#c3,#69,#2d
db #4b,#2d,#4b,#2d,#4b,#2d,#0f,#0f
t81:
db #0f,#0f,#0f,#0f,#3c,#c3,#4b,#2d
db #4b,#2d,#4b,#2d,#3c,#c3,#0f,#0f
t82:
db #0f,#0f,#0f,#0f,#78,#c3,#4b,#2d
db #4b,#2d,#78,#c3,#4b,#0f,#4b,#0f
;t83:
;db #0f,#0f,#0f,#0f,#78,#c3,#4b,#2d
;db #4b,#2d,#78,#c3,#4b,#0f,#4b,#0f
t84:
db #0f,#0f,#0f,#0f,#3c,#e1,#4b,#2d
db #4b,#2d,#3c,#e1,#0f,#2d,#0f,#2d
t85:
db #0f,#0f,#0f,#0f,#5a,#c3,#69,#2d
db #4b,#0f,#4b,#0f,#4b,#0f,#0f,#0f
t86:
db #0f,#0f,#0f,#0f,#3c,#c3,#4b,#0f
db #3c,#c3,#0f,#2d,#3c,#c3,#0f,#0f
t87:
db #2d,#0f,#2d,#0f,#78,#87,#2d,#0f
db #2d,#0f,#2d,#2d,#1e,#c3,#0f,#0f
t88:
db #0f,#0f,#0f,#0f,#4b,#2d,#4b,#2d
db #4b,#2d,#4b,#2d,#3c,#c3,#0f,#0f
t89:
db #0f,#0f,#0f,#0f,#4b,#2d,#4b,#2d
db #4b,#2d,#2d,#4b,#1e,#87,#0f,#0f
t90:
db #0f,#0f,#0f,#0f,#87,#2d,#96,#2d
db #96,#2d,#96,#2d,#69,#c3,#0f,#0f
t91:
db #0f,#0f,#0f,#0f,#4b,#2d,#2d,#4b
db #1e,#87,#2d,#4b,#4b,#2d,#0f,#0f
t92:
db #0f,#0f,#0f,#0f,#4b,#2d,#4b,#2d
db #4b,#2d,#3c,#e1,#0f,#2d,#78,#c3
t93:
db #0f,#0f,#0f,#0f,#78,#e1,#0f,#4b
db #0f,#87,#1e,#0f,#78,#e1,#0f,#0f
t94:
db #0f,#e1,#1e,#0f,#1e,#0f,#69,#0f
db #1e,#0f,#1e,#0f,#0f,#e1,#0f,#0f
t95:
db #2d,#0f,#2d,#0f,#2d,#0f,#2d,#0f
db #2d,#0f,#2d,#0f,#2d,#0f,#0f,#0f
t96:
db #78,#0f,#0f,#87,#0f,#87,#0f,#69
db #0f,#87,#0f,#87,#78,#0f,#0f,#0f
t97:
db #78,#69,#d2,#c3,#0f,#0f,#0f,#0f
db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
t98:
db #2d,#0f,#4b,#0f,#4b,#0f,#87,#0f
db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
t99:
db #0f,#0f,#1e,#0f,#1e,#0f,#0f,#87
db #0f,#87,#0f,#4b,#0f,#4b,#0f,#4b
t100:
db #0f,#0f,#2d,#0f,#2d,#0f,#78,#0f
db #78,#0f,#f0,#87,#f0,#87,#d2,#87
t101:   ; ARMOUR
db %00001111,%00001111 ; 00000000 SKY
db %00001111,%00001111 ; 00001111 BLACK
db %11110000,%11110000 ; 11110000 GREEN
db %11110000,%11110000 ; 11111111 BLUE / WHITE
db %00000000,%00000000
db %11111111,%11111111
db %00001111,%00001111
db %00001111,%00001111
;db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
;db #0f,#0f,#0f,#1e,#0f,#1e,#0f,#1e
;db #87,#87,#87,#87,#87,#87,#f0,#87
;db #87,#87,#87,#c3,#87,#e1,#f0,#e1
;db #0f,#1e,#0f,#1e,#0f,#1e,#0f,#1e
;db #0f,#1e,#0f,#69,#0f,#4b,#0f,#4b
;db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
;db #0f,#0f,#0f,#1e,#0f,#3c,#0f,#3c
;db #0f,#4b,#0f,#4b,#0f,#4b,#0f,#4b
;db #0f,#4b,#0f,#3c,#0f,#1e,#0f,#1e
;db #f0,#e1,#f0,#d2,#f0,#d2,#f0,#d2
;db #f0,#d2,#f0,#d2,#f0,#d2,#f0,#d2
;db #0f,#1e,#0f,#2d,#0f,#2d,#0f,#2d
;db #0f,#2d,#0f,#2d,#0f,#2d,#0f,#2d
;db #0f,#4b,#0f,#2d,#0f,#2d,#0f,#2d
;db #0f,#2d,#0f,#2d,#0f,#2d,#0f,#2d
;db #0f,#b4,#1e,#d2,#3c,#d2,#78,#d2
;db #f0,#d2,#f0,#d2,#f0,#d2,#f0,#d2
;db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
;db #0f,#0f,#0f,#1e,#0f,#3c,#0f,#f0
;db #1e,#f0,#3c,#f0,#78,#f0,#0f,#0f
;db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
;db #f0,#d2,#f0,#d2,#e1,#1e,#0f,#1e
;db #0f,#1e,#0f,#1e,#0f,#0f,#0f,#0f
;db #0f,#2d,#0f,#2d,#0f,#2d,#0f,#2d
;db #0f,#2d,#0f,#2d,#0f,#0f,#0f,#0f
;db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
;db #0f,#0f,#0f,#0f,#0f,#3c,#0f,#0f
;db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
;db #0f,#0f,#0f,#0f,#0f,#0f,#87,#0f
;db #f0,#d2,#f0,#d2,#f0,#c3,#f0,#c3
;db #f0,#c3,#f0,#c3,#f0,#87,#78,#0f
;db #0f,#2d,#0f,#2d,#0f,#2d,#0f,#2d
;db #0f,#2d,#0f,#2d,#0f,#69,#0f,#87
;db #87,#0f,#c3,#0f,#e1,#0f,#f0,#0f
;db #f0,#87,#f0,#c3,#f0,#e1,#f0,#f0
;db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
;db #0f,#0f,#0f,#0f,#0f,#0f,#87,#0f
;db #c3,#0f,#e1,#0f,#f0,#0f,#0f,#0f
;db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
;db #f0,#f0,#f0,#f0,#3c,#f0,#0f,#0f
;db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
;db #5a,#0f,#d2,#87,#d2,#c3,#d2,#e1
;db #d2,#f0,#0f,#78,#0f,#0f,#0f,#0f
;db #a5,#0f,#2d,#0f,#2d,#0f,#2d,#0f
;db #2d,#0f,#69,#0f,#69,#0f,#0f,#0f
;db #a5,#0f,#2d,#0f,#2d,#0f,#2d,#0f
;db #2d,#0f,#69,#0f,#69,#0f,#0f,#0f
;db #0f,#87,#0f,#0f,#0f,#0f,#0f,#0f
;db #0f,#0f,#1e,#0f,#1e,#0f,#2d,#0f
;db #0f,#0f,#0f,#0f,#0f,#1e,#0f,#3c
;db #0f,#78,#0f,#78,#0f,#0f,#0f,#0f
;db #b4,#2d,#5a,#5a,#b4,#2d,#78,#d2
;db #b4,#a5,#78,#1e,#a5,#2d,#5a,#d2
;db #0f,#0f,#0f,#0f,#0f,#0f,#f0,#0f
;db #f0,#87,#1e,#87,#1e,#87,#1e,#87
;db #1e,#87,#1e,#87,#1e,#87,#f0,#87
;db #f0,#87,#1e,#87,#1e,#87,#1e,#87
;db #0f,#0f,#0f,#0f,#0f,#0f,#f0,#f0
;db #f0,#f0,#1e,#87,#1e,#87,#1e,#87
;db #1e,#87,#1e,#87,#1e,#87,#f0,#f0
;db #f0,#f0,#1e,#87,#1e,#87,#1e,#87
;db #1e,#0f,#3c,#87,#69,#c3,#c3,#69
;db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
;db #0f,#c3,#1e,#87,#3c,#0f,#0f,#0f
;db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
;db #69,#69,#69,#69,#0f,#0f,#0f,#0f
;db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
;db #3c,#c3,#4b,#2d,#4b,#0f,#f0,#0f
;db #4b,#0f,#4b,#0f,#f0,#e1,#0f,#0f
;db #3c,#87,#4b,#4b,#b4,#a5,#a5,#2d
;db #b4,#a5,#4b,#4b,#3c,#87,#0f,#0f
;db #78,#e1,#f0,#4b,#f0,#4b,#78,#4b
;db #3c,#4b,#3c,#4b,#3c,#4b,#0f,#0f
;db #1e,#e1,#3c,#0f,#3c,#87,#69,#c3
;db #3c,#87,#1e,#87,#f0,#0f,#0f,#0f
;db #1e,#87,#1e,#87,#0f,#c3,#0f,#0f
;db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
;db #4b,#0f,#c3,#0f,#4b,#4b,#4b,#c3
;db #5a,#4b,#1e,#e1,#0f,#4b,#0f,#0f
;db #4b,#0f,#c3,#0f,#4b,#c3,#5a,#2d
;db #4b,#4b,#0f,#87,#1e,#e1,#0f,#0f
;db #e1,#0f,#1e,#0f,#69,#2d,#1e,#69
;db #e1,#a5,#0f,#f0,#0f,#2d,#0f,#0f
;db #0f,#0f,#1e,#87,#1e,#87,#78,#e1
;db #1e,#87,#1e,#87,#78,#e1,#0f,#0f
;db #1e,#87,#1e,#87,#0f,#0f,#78,#e1
;db #0f,#0f,#1e,#87,#1e,#87,#0f,#0f
;db #0f,#0f,#0f,#0f,#0f,#0f,#78,#e1
;db #0f,#69,#0f,#69,#0f,#0f,#0f,#0f
;db #1e,#87,#0f,#0f,#1e,#87,#3c,#0f
;db #69,#69,#69,#69,#3c,#c3,#0f,#0f
;db #1e,#87,#0f,#0f,#1e,#87,#1e,#87
;db #1e,#87,#1e,#87,#1e,#87,#0f,#0f
;db #0f,#0f,#0f,#0f,#78,#3c,#d2,#e1
;db #c3,#c3,#d2,#e1,#78,#3c,#0f,#0f
;db #78,#c3,#c3,#69,#c3,#69,#f0,#c3
;db #c3,#69,#c3,#69,#f0,#87,#c3,#0f
;db #0f,#0f,#69,#69,#69,#69,#3c,#c3
;db #69,#69,#69,#69,#3c,#c3,#0f,#0f
;db #3c,#c3,#69,#0f,#69,#0f,#3c,#c3
;db #69,#69,#69,#69,#3c,#c3,#0f,#0f
;db #0f,#0f,#0f,#0f,#1e,#e1,#3c,#0f
;db #78,#c3,#3c,#0f,#1e,#e1,#0f,#0f
;db #3c,#87,#69,#c3,#c3,#69,#f0,#e1
;db #c3,#69,#69,#c3,#3c,#87,#0f,#0f
;db #0f,#0f,#c3,#0f,#69,#0f,#3c,#0f
;db #3c,#87,#69,#c3,#c3,#69,#0f,#0f
;db #0f,#0f,#0f,#0f,#69,#69,#69,#69
;db #69,#69,#78,#c3,#69,#0f,#69,#0f
;db #0f,#0f,#0f,#0f,#0f,#0f,#f0,#e1
;db #69,#c3,#69,#c3,#69,#c3,#0f,#0f
;db #0f,#0f,#0f,#0f,#0f,#0f,#78,#e1
;db #d2,#87,#d2,#87,#78,#0f,#0f,#0f
;db #0f,#3c,#0f,#69,#0f,#c3,#3c,#c3
;db #69,#69,#3c,#c3,#69,#0f,#c3,#0f
;db #0f,#3c,#0f,#69,#0f,#c3,#69,#69
;db #69,#69,#3c,#c3,#69,#0f,#c3,#0f
;db #0f,#0f,#e1,#69,#3c,#c3,#1e,#87
;db #3c,#87,#69,#c3,#c3,#78,#0f,#0f
;db #0f,#0f,#0f,#0f,#69,#69,#c3,#3c
;db #d2,#b4,#d2,#b4,#78,#e1,#0f,#0f
;db #f0,#e1,#c3,#69,#69,#0f,#3c,#0f
;db #69,#0f,#c3,#69,#f0,#e1,#0f,#0f
;db #0f,#0f,#78,#c3,#c3,#69,#c3,#69
;db #c3,#69,#69,#c3,#e1,#e1,#0f,#0f
;db #1e,#87,#3c,#0f,#69,#0f,#c3,#0f
;db #87,#0f,#0f,#0f,#0f,#0f,#0f,#0f
;db #1e,#87,#3c,#0f,#69,#0f,#c3,#0f
;db #87,#0f,#0f,#0f,#0f,#0f,#0f,#0f
;db #1e,#87,#0f,#c3,#0f,#69,#0f,#3c
;db #0f,#1e,#0f,#0f,#0f,#0f,#0f,#0f
;db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#1e
;db #0f,#3c,#0f,#69,#0f,#c3,#1e,#87
;db #0f,#0f,#0f,#0f,#0f,#0f,#87,#0f
;db #c3,#0f,#69,#0f,#3c,#0f,#1e,#87
;db #1e,#87,#3c,#c3,#69,#69,#c3,#3c
;db #87,#1e,#0f,#0f,#0f,#0f,#0f,#0f
;db #1e,#87,#0f,#c3,#0f,#69,#0f,#3c
;db #0f,#3c,#0f,#69,#0f,#c3,#1e,#87
;db #0f,#0f,#0f,#0f,#0f,#0f,#87,#1e
;db #c3,#3c,#69,#69,#3c,#c3,#1e,#87
;db #1e,#87,#3c,#0f,#69,#0f,#c3,#0f
;db #c3,#0f,#69,#0f,#3c,#0f,#1e,#87
;db #1e,#87,#3c,#0f,#69,#0f,#c3,#1e
;db #87,#3c,#0f,#69,#0f,#c3,#1e,#87
;db #1e,#87,#0f,#c3,#0f,#69,#87,#3c
;db #c3,#1e,#69,#0f,#3c,#0f,#1e,#87
;db #1e,#87,#3c,#c3,#69,#69,#c3,#3c
;db #c3,#3c,#69,#69,#3c,#c3,#1e,#87
;db #c3,#3c,#e1,#78,#78,#e1,#3c,#c3
;db #3c,#c3,#78,#e1,#e1,#78,#c3,#3c
;db #0f,#3c,#0f,#78,#0f,#e1,#1e,#c3
;db #3c,#87,#78,#0f,#e1,#0f,#c3,#0f
;db #c3,#0f,#e1,#0f,#78,#0f,#3c,#87
;db #1e,#c3,#0f,#e1,#0f,#78,#0f,#3c
;db #c3,#c3,#c3,#c3,#3c,#3c,#3c,#3c
;db #c3,#c3,#c3,#c3,#3c,#3c,#3c,#3c
;db #a5,#a5,#5a,#5a,#a5,#a5,#5a,#5a
;db #a5,#a5,#5a,#5a,#a5,#a5,#5a,#5a
;db #f0,#f0,#f0,#f0,#0f,#0f,#0f,#0f
;db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
;db #0f,#3c,#0f,#3c,#0f,#3c,#0f,#3c
;db #0f,#3c,#0f,#3c,#0f,#3c,#0f,#3c
;db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
;db #0f,#0f,#0f,#0f,#f0,#f0,#f0,#f0
;db #c3,#0f,#c3,#0f,#c3,#0f,#c3,#0f
;db #c3,#0f,#c3,#0f,#c3,#0f,#c3,#0f
;db #f0,#f0,#f0,#e1,#f0,#c3,#f0,#87
;db #f0,#0f,#e1,#0f,#c3,#0f,#87,#0f
;db #f0,#f0,#78,#f0,#3c,#f0,#1e,#f0
;db #0f,#f0,#0f,#78,#0f,#3c,#0f,#1e
;db #0f,#1e,#0f,#3c,#0f,#78,#0f,#f0
;db #1e,#f0,#3c,#f0,#78,#f0,#f0,#f0
;db #87,#0f,#c3,#0f,#e1,#0f,#f0,#0f
;db #f0,#87,#f0,#c3,#f0,#e1,#f0,#f0
;db #a5,#a5,#5a,#5a,#a5,#a5,#5a,#5a
;db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
;db #0f,#a5,#0f,#5a,#0f,#a5,#0f,#5a
;db #0f,#a5,#0f,#5a,#0f,#a5,#0f,#5a
;db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
;db #a5,#a5,#5a,#5a,#a5,#a5,#5a,#5a
;db #a5,#0f,#5a,#0f,#a5,#0f,#5a,#0f
;db #a5,#0f,#5a,#0f,#a5,#0f,#5a,#0f
;db #a5,#a5,#5a,#4b,#a5,#87,#5a,#0f
;db #a5,#0f,#4b,#0f,#87,#0f,#0f,#0f
;db #a5,#a5,#5a,#5a,#2d,#a5,#1e,#5a
;db #0f,#a5,#0f,#5a,#0f,#2d,#0f,#1e
;db #0f,#1e,#0f,#2d,#0f,#5a,#0f,#a5
;db #1e,#5a,#2d,#a5,#5a,#5a,#a5,#a5
;db #0f,#0f,#87,#0f,#4b,#0f,#a5,#0f
;db #5a,#0f,#a5,#87,#5a,#4b,#a5,#a5
;db #78,#e1,#f0,#f0,#96,#96,#f0,#f0
;db #b4,#d2,#c3,#3c,#f0,#f0,#78,#e1
;db #78,#e1,#f0,#f0,#96,#96,#f0,#f0
;db #c3,#3c,#b4,#d2,#f0,#f0,#78,#e1
;db #3c,#87,#3c,#87,#f0,#e1,#f0,#e1
;db #f0,#e1,#1e,#0f,#3c,#87,#0f,#0f
;db #1e,#0f,#3c,#87,#78,#c3,#f0,#e1
;db #78,#c3,#3c,#87,#1e,#0f,#0f,#0f
;db #69,#c3,#f0,#e1,#f0,#e1,#f0,#e1
;db #78,#c3,#3c,#87,#1e,#0f,#0f,#0f
;db #1e,#0f,#3c,#87,#78,#c3,#f0,#e1
;db #f0,#e1,#1e,#0f,#3c,#87,#0f,#0f
;db #0f,#0f,#3c,#c3,#69,#69,#c3,#3c
;db #c3,#3c,#69,#69,#3c,#c3,#0f,#0f
;db #0f,#0f,#3c,#c3,#78,#e1,#f0,#f0
;db #f0,#f0,#78,#e1,#3c,#c3,#0f,#0f
;db #0f,#0f,#78,#e1,#69,#69,#69,#69
;db #69,#69,#69,#69,#78,#e1,#0f,#0f
;db #0f,#0f,#78,#e1,#69,#69,#69,#69
;db #69,#69,#69,#69,#78,#e1,#0f,#0f
;db #0f,#0f,#78,#e1,#78,#e1,#78,#e1
;db #78,#e1,#78,#e1,#78,#e1,#0f,#0f
;db #0f,#f0,#0f,#78,#0f,#d2,#78,#87
;db #c3,#c3,#c3,#c3,#c3,#c3,#78,#87
;db #3c,#c3,#69,#69,#69,#69,#69,#69
;db #3c,#c3,#1e,#87,#78,#e1,#1e,#87
;db #0f,#c3,#0f,#c3,#0f,#c3,#0f,#c3
;db #0f,#c3,#3c,#c3,#78,#c3,#3c,#87
;db #1e,#87,#1e,#c3,#1e,#e1,#1e,#b4
;db #1e,#87,#78,#87,#f0,#87,#78,#0f
;db #96,#96,#5a,#a5,#2d,#4b,#c3,#3c
;db #c3,#3c,#2d,#4b,#5a,#a5,#96,#96
;db #1e,#0f,#3c,#87,#3c,#87,#3c,#87
;db #3c,#87,#3c,#87,#78,#c3,#d2,#69
;db #1e,#87,#3c,#c3,#78,#e1,#f0,#f0
;db #1e,#87,#1e,#87,#1e,#87,#1e,#87
;db #1e,#87,#1e,#87,#1e,#87,#1e,#87
;db #f0,#f0,#78,#e1,#3c,#c3,#1e,#87
;db #1e,#0f,#3c,#0f,#78,#0f,#f0,#f0
;db #f0,#f0,#78,#0f,#3c,#0f,#1e,#0f
;db #0f,#87,#0f,#c3,#0f,#e1,#f0,#f0
;db #f0,#f0,#0f,#e1,#0f,#c3,#0f,#87
;db #0f,#0f,#0f,#0f,#1e,#87,#3c,#c3
;db #78,#e1,#f0,#f0,#f0,#f0,#0f,#0f
;db #0f,#0f,#0f,#0f,#f0,#f0,#f0,#f0
;db #78,#e1,#3c,#c3,#1e,#87,#0f,#0f
;db #87,#0f,#e1,#0f,#f0,#87,#f0,#e1
;db #f0,#87,#e1,#0f,#87,#0f,#0f,#0f
;db #0f,#2d,#0f,#e1,#3c,#e1,#f0,#e1
;db #3c,#e1,#0f,#e1,#0f,#2d,#0f,#0f
;db #3c,#87,#3c,#87,#96,#2d,#78,#c3
;db #1e,#0f,#2d,#87,#2d,#87,#2d,#87
;db #3c,#87,#3c,#87,#1e,#0f,#f0,#e1
;db #1e,#0f,#2d,#87,#4b,#4b,#87,#2d
;db #3c,#87,#3c,#87,#1e,#2d,#78,#c3
;db #96,#0f,#2d,#87,#2d,#4b,#2d,#2d

ifdef HARRIERATTACK

spritelookuptable:
  defw spr1
  defw spr2
  defw spr3
  defw spr4
  defw spr5
  defw spr6
  defw spr7
  defw spr8
  defw spr9
  defw spr10
  defw spr11
  defw spr12
  defw spr13
  defw spr14
  defw spr15
  defw spr16
  defw spr17
  defw spr18
  defw spr19
  defw spr20
  defw spr21
  defw spr22
  defw spr23
  defw spr24
  defw spr25
  defw spr26
  defw spr27
  defw spr28
  defw spr29
  defw spr30
  defw spr31
  defw spr32
  defw spr33
  defw spr34
  defw spr35
  defw spr36
  defw spr37
  defw spr38
  defw spr39
  defw spr40
  defw spr41
  defw spr42
  defw spr43
  defw spr44
  defw spr45
  defw spr46
  defw spr47
  defw spr48
  defw spr49
  defw spr50
  defw spr51
  defw spr52
  defw spr53
  defw spr54
  defw spr55
  defw spr56
  defw spr57
  defw spr58
  defw spr59
  defw spr60
  defw spr61
  defw spr62
  defw spr63
  defw spr64
  defw spr65
  defw spr66
  defw spr67
  defw spr68
  defw spr69
  defw spr70
  defw spr71
  defw spr72
  defw spr73
  defw spr74
  defw spr75
  defw spr76
  defw spr77
  defw spr78
  defw spr79
  defw spr80
  defw spr81
  defw spr82
  defw spr83
  defw spr84
  defw spr85
  defw spr86
  defw spr87
  defw spr88
  defw spr89
  defw spr90
  defw spr91
  defw spr92
  defw spr93
  defw spr94
  defw spr95
  defw spr96
  defw spr97
  defw spr98
  defw spr99
  defw spr100
  defw spr101
  defw spr102
  ;defw spr103
  ;defw spr104
  ;defw spr105
  ;defw spr106
  ;defw spr107
; SPRITE DATA
spr1:
db #00,#00,#00,#00,#00,#00,#00,#00 ; 0 - SKY
db #00,#00,#00,#00,#00,#00,#00,#00
spr2:
db #f0,#f0,#f0,#f0,#f0,#f0,#f0,#f0 ; 1 - LAND
db #f0,#f0,#f0,#f0,#f0,#f0,#f0,#f0
spr3:
db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f ; 2 - BLACK
db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
spr4:
db #ff,#ff,#ff,#ff,#ff,#ff,#ff,#ff ; 3 - SEA 1
db #ff,#ff,#ff,#ff,#ff,#ff,#ff,#ff
spr5:
db #aa,#dd,#ff,#ff,#ff,#ff,#ff,#ff ; 4 - SEA 2
db #ff,#ff,#ff,#ff,#ff,#ff,#ff,#ff
spr6:
db #77,#99,#ff,#ff,#ff,#ff,#ff,#ff ; 5 - SEA 3
db #ff,#ff,#ff,#ff,#ff,#ff,#ff,#ff
spr7:
db #33,#55,#ff,#ff,#ff,#ff,#ff,#ff ; 6 - GAUGE EMPTY
db #ff,#ff,#ff,#ff,#ff,#ff,#ff,#ff
spr8:
db #0f,#0f,#e1,#f0,#f0,#f0,#f0,#f0 ; 7 - GAUGE MARKER
db #f0,#f0,#f0,#f0,#f0,#f0,#e1,#f0
spr9:
db #0f,#0f,#e1,#f0,#f0,#f0,#e1,#f0 ; 8 - HARRIER REAR LANDED
db #c3,#78,#e1,#f0,#f0,#f0,#e1,#f0
spr10:
db #00,#00,#00,#00,#04,#00,#06,#00 ; 9 - HARRIER FRONT LANDED
db #07,#0f,#01,#0f,#02,#04,#00,#08
spr11:
db #00,#00,#00,#00,#00,#00,#03,#08 ; 10 - FRIGATE BACK
db #0f,#0e,#0d,#00,#01,#00,#01,#00
spr12:
db #00,#00,#07,#0f,#07,#0f,#03,#0f ; 11 - FRIGATE FRONT
db #03,#0f,#01,#0f,#01,#0f,#00,#0f
spr13:
db #0f,#0f,#0f,#0f,#0f,#0e,#0f,#0c ; 12 - FRIGATE DECK 1
db #0f,#08,#0f,#00,#0e,#00,#0c,#00
spr14:
db #00,#00,#00,#00,#00,#0f,#00,#0f ; 13 - FRIGATE DECK 2
db #00,#0f,#0f,#0f,#0e,#0f,#0f,#0f
spr15:
db #03,#0f,#03,#0f,#0f,#0f,#0f,#0f ; 14 - FRIGATE DECK 3
db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
spr16:
db #0d,#00,#0f,#00,#0f,#00,#0f,#00 ; 15 - FRIGATE DECK 4
db #0f,#00,#0f,#0f,#0f,#0d,#0f,#0f
spr17:
db #00,#04,#00,#04,#00,#04,#02,#04 ; 16 - FRIGATE DECK 5
db #01,#04,#03,#0f,#03,#0f,#03,#0f
spr18:
db #00,#00,#00,#00,#02,#00,#04,#00 ; 17 - FRIGATE DECK 6
db #02,#00,#0f,#00,#0d,#00,#0f,#00
spr19:
db #00,#00,#00,#00,#00,#00,#00,#01 ; 18 - FRIGATE DECK 7
db #00,#01,#00,#01,#00,#01,#00,#03
spr20:
db #00,#00,#00,#00,#00,#00,#00,#00 ; 19 - ENEMY SHIP FRONT
db #00,#00,#04,#00,#04,#00,#0e,#00
spr21:
db #00,#00,#01,#00,#00,#08,#00,#05 ; 20 - ENEMY SHIP MID 1
db #00,#03,#0f,#0f,#07,#0f,#03,#0f
spr22:
db #00,#02,#00,#03,#01,#0f,#09,#07 ; 21 - ENEMY SHIP MID 2
db #09,#0f,#0f,#0f,#0f,#0f,#0f,#0f
spr23:
db #0e,#00,#0e,#00,#0e,#00,#0f,#0c ; 22 - ENEMY SHIP MID 3
db #0f,#0c,#0f,#0f,#0f,#0f,#0f,#0f
spr24:
db #00,#00,#00,#00,#00,#00,#00,#00 ; 23 - ENEMY SHIP REAR
db #00,#00,#0f,#0f,#0f,#0f,#0f,#0e
spr25:
db #00,#30,#00,#f0,#10,#f0,#10,#f0 ; 24 - HILL UP 1
db #30,#f0,#30,#f0,#70,#f0,#f0,#f0
spr26:
db #00,#30,#00,#30,#30,#70,#30,#f0 ; 25 - HILL UP 2
db #70,#f0,#70,#f0,#f0,#f0,#f0,#f0
spr27:
db #00,#00,#00,#10,#00,#30,#00,#30 ; 26 - HILL UP 3
db #00,#70,#00,#70,#00,#f0,#70,#f0
spr28:
db #00,#20,#00,#30,#00,#70,#00,#70 ; 27 - HILL UP 4
db #00,#f0,#00,#f0,#10,#f0,#70,#f0
spr29:
db #c0,#00,#f0,#00,#f0,#80,#f0,#80 ; 28 - HILL DOWN 1
db #f0,#c0,#f0,#c0,#f0,#e0,#f0,#f0
spr30:
db #c0,#00,#c0,#00,#e0,#c0,#f0,#c0 ; 29 - HILL DOWN 2
db #f0,#e0,#f0,#e0,#f0,#f0,#f0,#f0
spr31:
db #00,#00,#80,#00,#c0,#00,#c0,#00 ; 30 - HILL DOWN 3
db #e0,#00,#e0,#00,#f0,#00,#f0,#e0
spr32:
db #40,#00,#c0,#00,#e0,#00,#e0,#00 ; 31 - HILL DOWN 4
db #f0,#00,#f0,#00,#f0,#80,#f0,#e0
spr33:
db #a0,#a0,#f0,#f0,#f0,#f0,#f0,#f0 ; 32 - GRASS 0
db #f0,#f0,#f0,#f0,#f0,#f0,#f0,#f0
spr34:
db #e0,#60,#f0,#f0,#f0,#f0,#f0,#f0 ; 33 - GRASS 1
db #f0,#f0,#f0,#f0,#f0,#f0,#f0,#f0
spr35:
db #30,#d0,#f0,#f0,#f0,#f0,#f0,#f0 ; 34 - GRASS 2
db #f0,#f0,#f0,#f0,#f0,#f0,#f0,#f0
spr36:
db #d0,#70,#f0,#f0,#f0,#f0,#f0,#f0 ; 35 - GRASS 3
db #f0,#f0,#f0,#f0,#f0,#f0,#f0,#f0
spr37:
db #00,#00,#00,#01,#03,#0f,#0f,#0f ; 36 - ENEMY PLANE FRONT
db #00,#01,#00,#00,#00,#00,#00,#00
spr38:
db #03,#03,#0f,#07,#0f,#0f,#0f,#0f ; 37 - ENEMY PLANE BACK
db #0f,#0c,#07,#0e,#00,#0e,#00,#00
spr39:
db #04,#04,#00,#09,#01,#0c,#03,#09 ; 38 - ENEMY PLANE BROKE FRONT
db #07,#02,#06,#01,#04,#00,#08,#04
spr40:
db #08,#00,#0e,#00,#07,#08,#07,#0b ; 39 - ENEMY PLANE BROKE BACK
db #07,#0f,#03,#0f,#03,#02,#00,#00
spr41:
db #00,#00,#00,#00,#02,#00,#00,#00 ; 40 - BOMB LAUNCHED
db #00,#08,#00,#04,#00,#00,#00,#00
spr42:
db #00,#00,#00,#00,#00,#08,#00,#00 ; 41 - BOMB DESCENDING
db #00,#08,#00,#08,#00,#00,#00,#00
spr43:
db #00,#04,#00,#04,#00,#04,#00,#08 ; 42 - RADAR
db #01,#08,#0e,#0c,#00,#0a,#00,#09
spr44:
db #02,#00,#01,#00,#02,#08,#04,#04 ; 43 - MISSILE LAUNCHER
db #02,#0b,#03,#02,#02,#00,#07,#00
spr45:
db #00,#00,#08,#00,#04,#04,#02,#08 ; 44 - GUN
db #01,#00,#03,#08,#07,#0c,#01,#0c
spr46:
db #00,#00,#02,#00,#01,#08,#00,#07 ; 45 - TANK FRONT
db #00,#03,#01,#0f,#00,#00,#00,#0f
spr47:
db #00,#00,#00,#00,#00,#00,#0c,#00 ; 46 - TANK REAR
db #0e,#00,#0f,#08,#00,#00,#0f,#00
spr48:
db #00,#00,#00,#00,#00,#00,#00,#00 ; 47 - EJECTOR SEAT
db #00,#00,#02,#00,#03,#00,#01,#00
spr49:
db #00,#00,#00,#0e,#01,#0c,#03,#0a ; 48 - CHUTE LEFT
db #07,#02,#06,#02,#05,#0e,#00,#01
spr50:
db #01,#08,#03,#0e,#0f,#0f,#08,#01 ; 49 - CHUTE 
db #04,#02,#03,#04,#00,#08,#00,#08
spr51:
db #00,#00,#07,#00,#03,#08,#05,#0c ; 50 - CHUTE RIGHT
db #04,#0e,#04,#06,#07,#0a,#08,#00
spr52:
db #01,#00,#03,#02,#07,#04,#03,#02 ; 51 - SMOKE 1
db #01,#03,#00,#0b,#00,#0a,#02,#04
spr53:
db #04,#05,#04,#01,#02,#03,#03,#03 ; 52 - SMOKE 2
db #03,#02,#0a,#06,#07,#0a,#0b,#05
spr54:
db #00,#00,#04,#00,#02,#00,#01,#00 ; 53 - MISSILE ASCENDING LEFT
db #00,#08,#00,#04,#00,#03,#00,#02
spr55:
db #00,#02,#00,#03,#00,#04,#00,#08 ; 54 - MISSILE DESCENDING LEFT
db #01,#00,#02,#00,#04,#00,#00,#00
spr56:
db #00,#00,#00,#00,#00,#00,#00,#01 ; 55 - MISSILE LEFT
db #0f,#0e,#00,#01,#00,#00,#00,#00
spr57:
db #00,#00,#00,#00,#00,#00,#08,#00 ; 56 - MISSILE RIGHT
db #07,#0f,#08,#00,#00,#00,#00,#00
spr58:
db #00,#00,#01,#00,#00,#04,#01,#00 ; 57 - FLAK 1
db #00,#08,#02,#00,#00,#00,#00,#00
spr59:
db #00,#00,#00,#00,#00,#00,#00,#08 ; 58 - FLAK 2
db #00,#01,#00,#0c,#00,#00,#00,#04
spr60:
db #00,#00,#00,#00,#00,#00,#00,#00 ; 59 - ROOF FRONT
db #01,#0f,#03,#0f,#07,#0f,#0f,#0f
spr61:
db #00,#00,#00,#00,#00,#00,#00,#00 ; 60 - ROOF BACK
db #0f,#08,#0f,#0c,#0f,#0e,#0f,#0f
spr62:
db #00,#00,#00,#00,#00,#00,#00,#00 ; 61 - SQUARE ROOF
db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f
spr63:
db #00,#00,#00,#00,#00,#00,#00,#00 ; 62 - POINTED ROOF
db #01,#08,#03,#0c,#07,#0e,#0f,#0f

;     RED       BLACK     YELLOW    ORANGE
; db %11111111,%00001111,%11110000,%00000000

; 0F = 0000ffff
; 09 = 00001001 REPLACE WITH %00001111 ETC &69 ORANGE, &6F RED, &0F SKY, TO CHANGE COLOUR IN WINDOWS
; 03 = 00000011
spr64:
db #0f,#0f,#09,#09
db #09,#09,#0f,#0f ; 63 - TOWER BLOCK 1
db #0f,#0f,#03,#69
db #03,#69,#03,#0f
;db #0f,#0f,#03,#09
;db #03,#09,#03,#0f
spr65:
db #0f,#0f,#69,#09
db #6F,#09,#0f,#0f ; 64 - TOWER BLOCK 2
;db #0f,#0f,#09,#09
;db #09,#09,#0f,#0f ; 64 - TOWER BLOCK 2
db #0f,#0f,#09,#09
db #09,#09,#0f,#0f
spr66:
db #0f,#0f,#09,#09
db #09,#6F,#0f,#0f ; 65 - TOWER BLOCK 3
db #0f,#0f,#69,#09
;db #09,#09,#0f,#0f ; 65 - TOWER BLOCK 3
;db #0f,#0f,#09,#09
db #09,#09,#09,#0f
spr67:
db #00,#00,#00,#00,#00,#00,#02,#00 ; 66 - JEEP
db #02,#00,#0e,#07,#0f,#0f,#04,#02
spr68:
db #00,#00,#00,#0e,#03,#0e,#0f,#0e ; 67 - PLANE BROKE 1
db #07,#0c,#03,#08,#01,#00,#00,#00
spr69:
db #00,#08,#01,#08,#03,#0c,#07,#0c ; 68 - PLANE BROKE 2
db #03,#0e,#01,#0e,#00,#0e,#00,#00
spr70:
db #00,#00,#00,#00,#00,#0f,#03,#0f ; 69 - PLANE BROKE 3
db #07,#0f,#03,#0f,#00,#03,#00,#00
spr71:
db #00,#07,#00,#07,#07,#07,#05,#07 ; 70 - TRUCK 1
db #0f,#07,#0f,#07,#0f,#0f,#06,#00
spr72:
db #0f,#0f,#0f,#0f,#0f,#0f,#0f,#0f ; 71 - TRUCK 2
db #0f,#0f,#0f,#0f,#0f,#0f,#00,#0c
spr73:
db #00,#00,#0f,#0f,#0f,#0f,#00,#08 ; 72 - DOCK 1
db #08,#08,#04,#09,#02,#0a,#01,#0c
spr74:
db #00,#00,#0f,#0f,#0f,#0f,#07,#00 ; 73 - DOCK 2
db #0a,#08,#02,#04,#02,#02,#02,#01
spr75:
db #00,#00,#0f,#0f,#0f,#0f,#00,#07 ; 74 - DOCK 3
db #00,#0a,#01,#02,#02,#02,#04,#02 
spr76:
db #00,#00,#04,#0f,#06,#07,#07,#0f ; 75 - HARRIER FLIGHT 1
db #03,#07,#02,#07,#00,#0e,#00,#00
spr77:
db #00,#00,#00,#00,#0f,#08,#0f,#0e ; 76 - HARRIER FLIGHT 2
db #0c,#00,#08,#00,#00,#00,#00,#00
spr78:
db #00,#00,#77,#00,#33,#ff,#11,#00 ; 77 - CLOUD 1
db #00,#22,#77,#ff,#00,#ff,#33,#ff
spr79:
db #00,#00,#ff,#00,#ff,#ff,#88,#ff ; 78 - CLOUD 2
db #00,#77,#ff,#ff,#ff,#ff,#ff,#88
spr80:
db #00,#00,#00,#cc,#77,#ff,#ff,#ff ; 79 - CLOUD 3
db #ff,#ff,#ff,#ff,#ff,#ff,#00,#ff
spr81:
db #00,#00,#00,#bb,#11,#ff,#00,#00 ; 80 - CLOUD 4
db #ff,#ff,#33,#77,#00,#33,#00,#00
spr82:
db #00,#00,#cc,#ff,#ff,#ff,#ff,#ff ; 81 - CLOUD 5
db #ff,#ff,#ff,#99,#22,#00,#00,#00
spr83:
db #33,#ff,#ff,#ff,#ee,#44,#ff,#00 ; 82 - CLOUD 6
db #cc,#00,#00,#00,#00,#00,#00,#00
spr84:
db #00,#00,#88,#00,#ff,#00,#ee,#00 ; 83 - CLOUD 7
db #99,#bb,#ff,#ff,#ff,#ff,#ff,#ff
spr85:
db #00,#00,#00,#00,#11,#ff,#ff,#cc ; 84 - CLOUD 8
db #ff,#00,#ee,#00,#ff,#ee,#ff,#ff
spr86:
db #ff,#ff,#ff,#ee,#33,#ff,#00,#ff ; 85 - CLOUD 9
db #00,#33,#00,#77,#77,#ff,#00,#00
spr87:
db #ff,#00,#00,#00,#00,#00,#00,#00 ; 86 - CLOUD 10
db #ff,#cc,#ff,#00,#ff,#ee,#00,#cc
spr88:
db #00,#00,#00,#00,#00,#33,#00,#33 ; 87 - CLOUD 11
db #00,#ff,#00,#11,#77,#ff,#00,#00
spr89:
db #00,#00,#44,#ff,#dd,#ff,#ff,#ff ; 88 - CLOUD 12
db #ff,#ff,#ff,#ff,#ff,#ff,#ff,#ff
spr90:
db #00,#00,#cc,#11,#ff,#bb,#ff,#ff ; 89 - CLOUD 13
db #ff,#ff,#ff,#ff,#ff,#ff,#ff,#ff
spr91:
db #66,#00,#ff,#cc,#ff,#ff,#ff,#ff ; 90 - CLOUD 14
db #ff,#ff,#ff,#ff,#ff,#ff,#ff,#ff
spr92:
db #00,#33,#77,#ff,#ff,#ff,#ff,#ff ; 91 - CLOUD 15
db #ff,#ff,#ff,#ff,#ff,#ff,#ff,#ff
spr93:
db #ff,#ff,#ff,#ff,#ff,#88,#cc,#00 ; 92 - CLOUD 16
db #00,#00,#ff,#88,#ff,#cc,#ff,#ee
spr94:
db #ff,#ff,#ff,#ff,#00,#ff,#33,#ff ; 93 - CLOUD 17
db #ff,#ff,#11,#ff,#11,#ff,#00,#00
spr95:
db #ff,#ff,#ff,#ff,#ff,#ff,#ff,#ff ; 94 - CLOUD 18
db #ff,#ff,#ff,#ff,#dd,#99,#00,#00
spr96:
db #ff,#00,#ff,#ff,#ff,#ff,#ff,#ff ; 95 - CLOUD 19
db #ff,#ff,#ff,#cc,#cc,#00,#00,#00
spr97:
db #00,#00,#00,#00,#cc,#00,#ff,#cc ; 96 - CLOUD 20
db #00,#00,#00,#00,#00,#00,#00,#00
spr98:                             ; 97 - HOLE IN LAND SPRITE
db %00000000,%00000000 ; 00000000 SKY
db %00000000,%00000000 ; 00001111 BLACK
db %10000000,%00010000 ; 11110000 GREEN
db %11000000,%00010000 ; 11111111 BLUE / WHITE
db %11000000,%00110000
db %11000000,%00110000
db %11100000,%01110000
db %11110000,%11110000
spr99:    ; MISSILE UP RIGHT
db %00000000,%00000000 ; 00000000 SKY
db %00000000,%00000010 ; 00001111 BLACK
db %00000000,%00000100 ; 11110000 GREEN
db %00000000,%00001000 ; 11111111 BLUE / WHITE
db %00000001,%00000000
db %00000010,%00000000
db %00001100,%00000000
db %00000100,%00000000
spr100:   ; MISSILE DOWN RIGHT
db %00000100,%00000000 ; 00000000 SKY
db %00001100,%00000000 ; 00001111 BLACK
db %00000010,%00000000 ; 11110000 GREEN
db %00000001,%00000000 ; 11111111 BLUE / WHITE
db %00000000,%00001000
db %00000000,%00000100
db %00000000,%00000010
db %00000000,%00000000
spr101:   ; MISSILE DOWN
db %00000001,%00000100 ; 00000000 SKY
db %00000000,%00001000 ; 00001111 BLACK
db %00000000,%00001000 ; 11110000 GREEN
db %00000000,%00001000 ; 11111111 BLUE / WHITE
db %00000000,%00001000
db %00000000,%00001000
db %00000000,%00001000
db %00000000,%00000000
spr102:   ; MISSILE UP
db %00000000,%00000000 ; 00000000 SKY
db %00000000,%00001000 ; 00001111 BLACK
db %00000000,%00001000 ; 11110000 GREEN
db %00000000,%00001000 ; 11111111 BLUE / WHITE
db %00000000,%00001000
db %00000000,%00001000
db %00000000,%00001000
db %00000001,%00000100

endif

read "CPSoundEffectGenerator2.asm"

ifdef HARRIERATTACK

org &f000
; FLYING HARRIER - GREEN
sprite_pixel_data1:
defb 5+6,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 4+6,5+6,0,0,2+6,3+6,4+6,5+6, 0,0,0,0,0,0,0,0
defb 3+6,4+6,5+6,0,0,2+6,3+6,4+6, 0,0,0,0,0,0,0,0
defb 2+6,3+6,4+6,4+6,4+6,4+6,4+6,4+6, 0,0,0,0,0,0,0,0
defb 2+6,2+6,2+6,1+6,1+6,1+6,2+6,3+6, 0,0,0,0,0,0,0,0
defb 0,2+6,1+6,0,0,2+6,3+6,4+6, 0,0,0,0,0,0,0,0
defb 0,0,0,0,2+6,3+6,4+6,5+6, 0,0,0,0,0,0,0,0
defb 0,0,0,2+6,3+6,4+6,2+6,0, 0,0,0,0,0,0,0,0
sprite_pixel_data2:
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,6,6,0,0,0, 0,0,0,0,0,0,0,0
defb 5+6,5+6,5+6,4+6,5+6,6+6,2+6,0, 0,0,0,0,0,0,0,0
defb 4+6,4+6,3+6,2+6,1+6,4+6,3+6,2+6, 0,0,0,0,0,0,0,0
defb 4+6,5+6,3+6,2+6,1+6,2+6,2+6,0, 0,0,0,0,0,0,0,0
defb 5+6,1+6,1+6,1+6,2+6,0,0,0, 0,0,0,0,0,0,0,0
defb 1+6,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0





;defb &01,&01,&03,&04,&05,&06,&07,&08,&09,&0a,&0b,&0c,&0d,&0e,&0f,&01		;; line 0
;defb &01,&02,&01,&04,&05,&06,&07,&08,&09,&0a,&0b,&0c,&0d,&0e,&0f,&01		;; line 1
;defb &01,&02,&01,&04,&05,&06,&01,&01,&01,&01,&01,&01,&01,&0e,&0f,&01		;; line 2
;defb &01,&02,&03,&01,&01,&01,&07,&08,&09,&0a,&0b,&0c,&01,&01,&0f,&01		;; line 3
;defb &01,&02,&03,&04,&05,&06,&07,&08,&09,&0a,&0b,&0c,&0d,&0e,&0f,&01		;; line 4
;defb &01,&02,&03,&04,&05,&06,&07,&08,&09,&0a,&0b,&0c,&0d,&0e,&0f,&01		;; line 5
;defb &01,&02,&03,&04,&05,&06,&07,&08,&09,&0a,&0b,&0c,&0d,&0e,&0f,&01		;; line 6
;defb &01,&02,&03,&04,&05,&06,&07,&08,&09,&0a,&0b,&0c,&0d,&0e,&0f,&01		;; line 7

;defb &01,&02,&03,&04,&05,&06,&07,&08,&09,&0a,&0b,&0c,&0d,&0e,&0f,&01		;; line 8
;defb &01,&02,&03,&04,&05,&06,&07,&08,&09,&0a,&0b,&0c,&0d,&0e,&0f,&01		;; line 9
;defb &01,&02,&03,&04,&05,&06,&07,&08,&09,&0a,&0b,&0c,&0d,&0e,&0f,&01		;; line 10
;defb &01,&02,&03,&04,&05,&06,&07,&08,&09,&0a,&0b,&0c,&0d,&0e,&0f,&01		;; line 11
;defb &01,&02,&03,&04,&05,&06,&07,&08,&09,&0a,&0b,&0c,&0d,&0e,&0f,&01		;; line 12
;defb &01,&02,&03,&04,&05,&06,&07,&08,&09,&0a,&0b,&0c,&0d,&0e,&0f,&01		;; line 13
;defb &01,&02,&03,&04,&05,&06,&07,&08,&09,&0a,&0b,&0c,&0d,&0e,&0f,&01		;; line 14
;defb &01,&02,&03,&04,&05,&06,&07,&08,&09,&0a,&0b,&0c,&0d,&0e,&0f,&01		;; line 15

; LANDING HARRIER
sprite_pixel_data4:
defb 5+6,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 4+6,5+6,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 3+6,4+6,5+6,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 2+6,3+6,4+6,4+6,4+6,3+6,4+6,4+6, 0,0,0,0,0,0,0,0
defb 1+6,2+6,2+6,3+6,2+6,3+6,3+6,4+6, 0,0,0,0,0,0,0,0
defb 0,0,1+6,1+6,2+6,2+6,3+6,1+6, 0,0,0,0,0,0,0,0
defb 0,0,0,0,1+6,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,1+6,0,0,0, 0,0,0,0,0,0,0,0
sprite_pixel_data3:
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,4+6,5+6,6+6,0,0,0, 0,0,0,0,0,0,0,0
defb 5+6,3+6,2+6,4+6,5+6,6+6,3+6,0, 0,0,0,0,0,0,0,0
defb 2+6,3+6,2+6,1+6,3+6,3+6,4+6,3+6, 0,0,0,0,0,0,0,0
defb 1+6,1+6,1+6,1+6,1+6,1+6,1+6,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,0,1+6,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,0,1+6,0,0, 0,0,0,0,0,0,0,0



; ENEMY PLANE FLYING
sprite_pixel_data5:
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,6,5,4,0,5, 0,0,0,0,0,0,0,0
defb 0,4,4,5,4,5,5,5, 0,0,0,0,0,0,0,0
defb 4,5,4,3,3,3,3,3, 0,0,0,0,0,0,0,0
defb 0,3,3,2,2,1,3,6, 0,0,0,0,0,0,0,0
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
sprite_pixel_data6:
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,3,2,0,0,6,4, 0,0,0,0,0,0,0,0
defb 4,3,2,1,0,6,4,2, 0,0,0,0,0,0,0,0
defb 5,5,5,5,5,4,3,1, 0,0,0,0,0,0,0,0
defb 4,4,3,3,2,2,1,0, 0,0,0,0,0,0,0,0
defb 5,4,4,3,2,1,0,0, 0,0,0,0,0,0,0,0
defb 4,6,5,4,3,2,1,0, 0,0,0,0,0,0,0,0
defb 0,0,0,4,6,3,1,0, 0,0,0,0,0,0,0,0

; ENEMY PLANE BROKE
sprite_pixel_data7:
defb 0,2,0,0,0,1,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,3,0,0,1, 0,0,0,0,0,0,0,0
defb 0,0,6,3,4,1,0,0, 0,0,0,0,0,0,0,0
defb 0,6,3,4,1,0,0,4, 0,0,0,0,0,0,0,0
defb 0,6,4,3,2,0,3,0, 0,0,0,0,0,0,0,0
defb 0,4,3,2,0,0,0,1, 0,0,0,0,0,0,0,0
defb 3,3,2,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 2,0,0,0,0,2,0,0, 0,0,0,0,0,0,0,0

sprite_pixel_data8:	
defb 3,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 2,4,2,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,3,4,3,2,0,0,0, 0,0,0,0,0,0,0,0
defb 0,4,3,3,4,0,3,3, 0,0,0,0,0,0,0,0
defb 0,2,4,3,3,3,4,2, 0,0,0,0,0,0,0,0
defb 0,0,3,4,2,2,2,1, 0,0,0,0,0,0,0,0
defb 0,0,2,1,0,0,1,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0

; SECOND HARRIER
;sprite_pixel_data9:
;defb 5,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
;defb 4,5,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
;defb 3,4,5,0,0,0,0,0, 0,0,4,5,6,0,0,0
;defb 2,3,4,4,4,3,4,4, 5,3,2,4,5,6,3,0
;defb 1,2,2,3,2,3,3,4, 2,3,2,1,3,3,4,3
;defb 0,0,1,1,2,2,3,1, 1,1,1,1,1,1,1,0
;defb 0,0,0,0,1,0,0,0, 0,0,0,0,0,1,0,0
;defb 0,0,0,0,1,0,0,0, 0,0,0,0,0,1,0,0

; CARRIER BODY
sprite_pixel_data10:
defb 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1
defb 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2
defb 4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4
defb 3,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3
defb 5,4,5,4,5,3,5,4, 5,4,5,3,4,4,5,4
defb 4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4
defb 3,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3
defb 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2

; CARRIER BACK
sprite_pixel_data11:
defb 0,0,0,0,0,0,0,1, 1,1,1,1,1,1,1,1
defb 1,1,1,1,1,1,1,1, 2,2,2,2,2,2,2,2
defb 2,3,4,4,4,4,4,4, 4,4,4,4,4,4,4,4
defb 0,1,2,3,3,3,3,3, 3,3,3,3,3,3,3,3
defb 0,2,3,4,4,4,4,4, 4,4,4,4,4,4,4,4
defb 0,0,2,3,4,4,4,4, 4,4,4,4,4,4,4,4
defb 0,0,1,2,3,3,3,3, 3,3,3,3,3,3,3,3
defb 0,0,0,1,2,2,2,2, 2,2,2,2,2,2,2,2

; CARRIER FRONT
sprite_pixel_data12:
defb 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1
defb 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,1
defb 4,4,4,4,4,4,4,4, 4,4,4,4,4,4,3,0
defb 3,3,6,6,6,6,3,3, 3,3,3,3,3,2,0,0
defb 4,4,4,1,6,5,4,4, 4,4,4,4,2,0,0,0
defb 4,4,4,6,5,1,4,4, 4,4,4,2,0,0,0,0
defb 3,3,6,5,1,3,3,3, 3,3,2,0,0,0,0,0
defb 2,2,2,1,2,2,2,2, 2,1,0,0,0,0,0,0

; CARRIER TOP
sprite_pixel_data13:
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,1,0,0
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,1,0,0
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,1,0,0
defb 0,0,0,0,0,0,0,0, 0,0,1,0,0,1,0,0
defb 0,0,0,0,0,0,0,0, 0,0,0,1,0,1,0,0
defb 0,0,0,0,0,0,0,0, 0,0,1,2,2,2,2,2
defb 0,0,0,0,0,0,0,0, 0,0,1,2,3,3,3,3
defb 0,0,0,0,0,0,0,0, 0,0,1,2,3,4,4,4

defb 0,0,0,0,0,0,0,0, 0,0,1,2,3,3,3,3
defb 0,0,0,0,0,0,0,0, 0,0,1,2,3,3,3,3
defb 0,0,0,0,1,2,2,2, 2,2,2,2,2,2,2,2
defb 0,0,0,0,1,2,3,3, 3,3,3,3,3,3,3,3
defb 0,0,0,0,1,2,4,4, 4,4,4,4,4,4,4,4
defb 1,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2
defb 1,2,3,0,3,3,3,3, 3,2,2,3,3,3,2,3
defb 1,2,4,4,4,4,4,4, 4,3,3,4,4,4,4,4

; CARRIER TOP 2
sprite_pixel_data14:
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,1,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,1,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,1,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 2,2,2,1,0,0,0,0, 0,0,0,0,0,0,0,0
defb 3,3,0,2,0,0,0,0, 0,0,0,0,0,0,0,0
defb 4,4,4,3,0,0,0,0, 0,0,0,0,0,0,0,0

defb 3,3,0,2,0,0,0,0, 0,0,0,0,0,0,0,0
defb 3,3,3,2,0,0,0,0, 0,0,0,0,0,0,0,0
defb 2,2,2,2,0,0,0,0, 0,0,0,0,0,0,0,0
defb 3,3,3,2,0,0,0,0, 0,0,0,0,0,0,0,0
defb 4,4,4,2,0,0,0,0, 0,0,0,0,0,0,0,0
defb 2,2,2,2,2,2,2,1, 0,0,0,0,0,0,0,0
defb 3,3,3,3,3,3,0,2, 0,0,0,0,0,0,0,0
defb 4,4,4,4,4,4,4,2, 0,0,0,0,0,0,0,0

; GUNSHIP LEFT
sprite_pixel_data15:
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,1
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,2
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,2
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,2
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,1,1

defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,2,0
defb 0,0,0,3,0,0,0,0, 0,0,0,0,0,0,2,3
defb 0,0,0,0,2,0,0,0, 0,0,0,3,2,2,3,3
defb 0,0,0,0,0,2,0,4, 3,0,0,2,0,3,3,2
defb 0,0,0,0,0,0,1,3, 2,0,0,2,0,2,2,2
defb 1,2,2,3,3,3,4,4, 4,4,4,3,3,3,3,3
defb 0,1,2,3,4,5,6,5, 5,5,4,5,4,4,4,4
defb 0,0,1,1,2,3,5,6, 5,5,5,4,5,4,4,4

; GUNSHIP RIGHT
sprite_pixel_data16:
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,1,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,2,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 1,1,1,0,0,0,0,0, 0,0,0,0,0,0,0,0

defb 2,2,1,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 3,3,1,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 1,1,1,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 2,2,1,1,1,1,0,0, 0,0,0,0,0,0,0,0
defb 3,3,1,2,2,1,0,0, 0,0,0,0,0,0,0,0
defb 3,3,3,3,3,3,3,3, 2,3,2,2,2,2,2,1
defb 4,4,4,4,4,4,4,4, 4,4,4,4,3,2,1,1
defb 4,4,4,4,4,4,4,4, 4,4,4,3,2,1,1,0

; FLYING HARRIER WINGMAN
sprite_pixel_data_wingmanflying1:
defb 5,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 4,5,0,0,2,3,4,5, 0,0,0,0,0,0,0,0
defb 3,4,5,0,0,2,3,4, 0,0,0,0,0,0,0,0
defb 2,3,4,4,4,4,4,4, 0,0,0,0,0,0,0,0
defb 2,2,2,1,1,1,2,3, 0,0,0,0,0,0,0,0
defb 0,2,1,0,0,2,3,4, 0,0,0,0,0,0,0,0
defb 0,0,0,0,2,3,4,5, 0,0,0,0,0,0,0,0
defb 0,0,0,2,3,4,2,0, 0,0,0,0,0,0,0,0
sprite_pixel_data_wingmanflying2:
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,6,6,0,0,0, 0,0,0,0,0,0,0,0
defb 5,5,5,4,5,6,2,0, 0,0,0,0,0,0,0,0
defb 4,4,3,2,1,4,3,2, 0,0,0,0,0,0,0,0
defb 4,5,3,2,1,2,2,0, 0,0,0,0,0,0,0,0
defb 5,1,1,1,2,0,0,0, 0,0,0,0,0,0,0,0
defb 1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0

; LANDING HARRIER
sprite_pixel_data_wingmanlanded1:
defb 5,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 4,5,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 3,4,5,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 2,3,4,4,4,3,4,4, 0,0,0,0,0,0,0,0
defb 1,2,2,3,2,3,3,4, 0,0,0,0,0,0,0,0
defb 0,0,1,1,2,2,3,1, 0,0,0,0,0,0,0,0
defb 0,0,0,0,1,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,1,0,0,0, 0,0,0,0,0,0,0,0
sprite_pixel_data_wingmanlanded2:
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,4,5,6,0,0,0, 0,0,0,0,0,0,0,0
defb 5,3,2,4,5,6,3,0, 0,0,0,0,0,0,0,0
defb 2,3,2,1,3,3,4,3, 0,0,0,0,0,0,0,0
defb 1,1,1,1,1,1,1,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,0,1,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,0,1,0,0, 0,0,0,0,0,0,0,0

; PARACHUTE
sprite_pixel_data_parachute:
defb 0,0,0,15,15,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,15,15,15,15,15,0, 0,0,0,0,0,0,0,0
defb 15,15,15,15,15,15,15,15, 0,0,0,0,0,0,0,0
defb 1,0,0,0,0,0,0,1, 0,0,0,0,0,0,0,0
defb 0,1,0,0,0,0,1,0, 0,0,0,0,0,0,0,0
defb 0,0,1,1,0,1,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,1,0,0,0, 0,0,0,0,0,0,0,0
defb 0,0,0,0,1,0,0,0, 0,0,0,0,0,0,0,0

defb "CHRIS5"
endif

;SAVE direct "HARRIER2.BIN",&C100,&1000