; ---------------------------------------------------------------------------
; Mode7Plane.sfc -- a Mode 7 ground plane, for judging per-layer 3D depth.
;
; Draws a wrapping checkerboard through the Mode 7 transform, with the matrix
; rewritten every scanline by HDMA so the plane recedes: the top of the plane
; walks 8192 texels across the screen and the bottom walks 256, which is the
; 1:1 scale. That range is the whole of what the fork reads a Mode 7 scanline's
; distance from, so a build giving Mode 7 depth shows the shift growing from
; nothing at the bottom of the screen to nearly the slot's full value at the
; top, and a build that does not shows the two eyes identical.
;
;   ca65 --cpu 65816 -o mode7plane.o mode7plane.s
;   ld65 -C mode7plane.cfg -o Mode7Plane.sfc mode7plane.o
; ---------------------------------------------------------------------------

.p816
.smart

; The scale the bottom scanline is drawn at, in 8.8 fixed point: 1:1, one texel
; per pixel, which is the distance the fork calls the screen plane.
NEAR_SCALE      = $0100
SCREEN_BOTTOM   = 223

; Where the plane's scale runs away to. Kept inside a signed 16-bit matrix
; entry, and the scanlines above it are pinned there rather than drawn at a
; scale that would wrap the register.
FAR_SCALE       = $2000
HORIZON         = 64

; How many colours the bands cycle through. A power of two, so the column can
; pick one with a mask.
COLOURS         = 8

; scale(y) = SCALE_NUM / (y - HORIZON), so that scale(SCREEN_BOTTOM) is 1:1.
SCALE_NUM       = NEAR_SCALE * (SCREEN_BOTTOM - HORIZON)

; The first scanline whose own scale is already inside FAR_SCALE.
FIRST_SLOPED    = HORIZON + (SCALE_NUM / FAR_SCALE) + 1

.segment "ZEROPAGE"
tmp:            .res 2

.segment "CODE"

reset:
        sei
        clc
        xce                     ; 65816 native mode
        rep #$38                ; 16-bit A and index, decimal off
        ldx #$1fff
        txs

        jsr init_registers
        jsr load_vram
        jsr load_palette
.ifndef FLAT
        jsr start_hdma
.endif

        sep #$20
        .a8
        lda #$07
        sta $2105               ; BGMODE 7
        lda #$00
        sta $211a               ; M7SEL: wrap the plane, no repeated tile 0
        lda #$01
        sta $212c               ; main screen: BG1
        lda #$00
        sta $212d               ; sub screen: nothing
        lda #$0f
        sta $2100               ; display on, full brightness

        lda #$80
        sta $4200               ; NMI on, so the palette is rewritten in vblank

forever:
        wai
        bra forever

; ---------------------------------------------------------------------------
; A game writes CGRAM constantly; this draws one still picture and would
; otherwise write it once and never again. Renderers that cache Mode 7
; characters against the palette they were told about have nothing to correct
; themselves with if that one write lands at the wrong moment, so it is
; repeated every frame.
; ---------------------------------------------------------------------------
nmi:
        rep #$38
        pha
        phx
        sep #$20
        .a8
        lda $4210               ; acknowledge the NMI
        jsr load_palette
        rep #$30
        plx
        pla
        rti

; ---------------------------------------------------------------------------
; Everything the PPU has to be told before the first frame.
; ---------------------------------------------------------------------------
.proc init_registers
        sep #$20
        .a8
        lda #$8f
        sta $2100               ; forced blank while VRAM is written

        stz $2101               ; no sprites
        stz $2102
        stz $2103
        stz $2105
        stz $2106
        stz $2107
        stz $2108
        stz $2109
        stz $210a
        stz $210b
        stz $210c

        ; Mode 7's own scroll and centre. The centre sits in the middle of the
        ; plane so the scanline spreads either side of it.
        stz $210d
        stz $210d               ; M7HOFS = 0 (written twice)
        stz $210e
        stz $210e               ; M7VOFS = 0

.ifdef FLAT
        stz $211f
        stz $211f               ; M7X = 0
        stz $2120
        stz $2120               ; M7Y = 0

        ; A flat plane, minified 2:1, so every texel it samples stays inside the
        ; 1024-square Mode 7 texture. Nothing rewrites the matrix per scanline,
        ; so every scanline walks the same span and the plane is one distance
        ; from end to end.
        stz $211b
        lda #$02
        sta $211b               ; M7A = $0200
        stz $211e
        lda #$02
        sta $211e               ; M7D = $0200
.else
        lda #$80
        sta $211f
        stz $211f               ; M7X = 128
        lda #$80
        sta $2120
        stz $2120               ; M7Y = 128
.endif

        ; B and C stay zero: the plane only scales, it does not rotate, so a
        ; scanline's span is its A entry alone.
        stz $211c
        stz $211c               ; M7B = 0
        stz $211d
        stz $211d               ; M7C = 0

        stz $2123               ; no windows
        stz $2124
        stz $2125
        stz $2126
        stz $2127
        stz $2128
        stz $2129
        stz $212a
        stz $212b
        stz $212e
        stz $212f
        lda #$30
        sta $2130               ; no colour math
        stz $2131
        lda #$e0
        sta $2132
        stz $2133

        stz $4200               ; no NMI or auto-joypad yet
        lda #$ff
        sta $4201
        stz $420b
        stz $420c
        rep #$20
        .a16
        rts
.endproc

; ---------------------------------------------------------------------------
; Mode 7 keeps the tilemap in the low byte of each VRAM word and the character
; data in the high byte, so the same 16K words are filled twice.
; ---------------------------------------------------------------------------
.proc load_vram
        sep #$20
        .a8
        lda #$80
        sta $2115               ; step one word after the high byte is written
        rep #$20
        .a16
        stz $2116               ; from word 0

        ; Character data. Tiles 1 to 8 are each one flat colour, so the map's
        ; own pattern is the whole picture. Tile 0 is left out on purpose -- a
        ; renderer that learns which tiles are in use from writes that change
        ; the tilemap never sees a 0 written over the 0 the map starts as.
        ldx #$0000
char_loop:
        txa
        lsr
        lsr
        lsr
        lsr
        lsr
        lsr                     ; a = x / 64, the tile this byte belongs to
        cmp #COLOURS + 1
        bcc char_put            ; tiles 1..8 take their own number as a colour
        lda #$0000              ; everything above them is left blank
char_put:
        sep #$20
        .a8
        sta $2119
        rep #$20
        .a16
        inx
        cpx #16384
        bne char_loop

        ; Tilemap: 128 x 128 tiles, in blocks of four so the squares are 32
        ; pixels of plane each.
        sep #$20
        .a8
        stz $2115               ; step one word after the low byte instead
        rep #$20
        .a16
        stz $2116

        ; Bands of colour eight tiles wide, cycling through all eight. One
        ; whole cycle is 512 texels of plane, which is wider than any parallax
        ; a slider can ask for, so a shift between the two eyes can be read off
        ; the picture without the pattern repeating into an ambiguity.
        ldy #$0000              ; row
map_row:
        ldx #$0000              ; column
map_col:
        txa
        lsr
        lsr
        lsr
        and #COLOURS - 1
        inc                     ; tiles 1..8, never 0
        sep #$20
        .a8
        sta $2118
        rep #$20
        .a16
        inx
        cpx #128
        bne map_col
        iny
        cpy #128
        bne map_row
        rts
.endproc

; ---------------------------------------------------------------------------
; Three colours: the backdrop, and the two the squares are drawn in.
; ---------------------------------------------------------------------------
.proc load_palette
        sep #$20
        .a8
        stz $2121               ; from colour 0
        ldx #$0000
pal_loop:
        lda palette, x
        sta $2122
        inx
        cpx #(COLOURS + 1) * 2
        bne pal_loop
        rep #$20
        .a16
        rts
.endproc

; Colour 0 is the backdrop, and is deliberately not black: a screen showing
; nothing then looks different from a screen showing the plane's backdrop.
palette:
        .word $0800             ; 0: dark blue
        .word $7fff             ; 1: white
        .word $001f             ; 2: red
        .word $03e0             ; 3: green
        .word $7c00             ; 4: blue
        .word $03ff             ; 5: yellow
        .word $7c1f             ; 6: magenta
        .word $7fe0             ; 7: cyan
        .word $4210             ; 8: grey

; ---------------------------------------------------------------------------
; One HDMA channel per matrix entry, both walking the same table. A and D
; together are what turns the flat plane into a receding one; A alone is what
; the fork reads, since it is the whole of a scanline's horizontal span.
; ---------------------------------------------------------------------------
.proc start_hdma
        sep #$20
        .a8

        lda #$02                ; write twice, to one register
        sta $4300
        lda #$1b
        sta $4301               ; $211b, M7A
        ldx #.loword(scale_table)
        stx $4302
        lda #^scale_table
        sta $4304

        lda #$02
        sta $4310
        lda #$1e
        sta $4311               ; $211e, M7D
        ldx #.loword(scale_table)
        stx $4312
        lda #^scale_table
        sta $4314

        lda #$03                ; channels 0 and 1
        sta $420c

        rep #$20
        .a16
        rts
.endproc

.segment "RODATA"

; One entry per scanline: draw this line, then take the next entry.
scale_table:
.repeat SCREEN_BOTTOM + 1, scanline
        .byte $01
        .if scanline < FIRST_SLOPED
                .word FAR_SCALE
        .else
                .word (SCALE_NUM / (scanline - HORIZON))
        .endif
.endrepeat
        .byte $00               ; end of table

.segment "CARTINFO"
        .byte "MODE7 PLANE          "   ; 21 characters, padded with spaces
        .byte $20                       ; LoROM, slow
        .byte $00                       ; ROM only
        .byte $05                       ; 32K
        .byte $00                       ; no SRAM
        .byte $01                       ; country
        .byte $00                       ; developer
        .byte $00                       ; version
        .word $ffff                     ; checksum complement
        .word $0000                     ; checksum

.segment "VECTORS"
        ;      $ffe0 $ffe2 $ffe4 $ffe6 $ffe8  $ffea  $ffec $ffee
        .word      0,    0,    0,    0,    0,   nmi,     0,    0
        ;      $fff0 $fff2 $fff4 $fff6 $fff8  $fffa  $fffc $fffe
        .word      0,    0,    0,    0,    0,   nmi, reset,    0
