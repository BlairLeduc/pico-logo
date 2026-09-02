# coresize = 0x10000
# corebase = 0x0
; 
; There is doubtless much that is wrong or missing here.  If you can help
; supply any missing bits, or correct anything wrong, please feel free to
; write me: mouse@rodents.montreal.qc.ca.
; 
; General game state.  Whether we're running in selftest is not recorded
; here, so each value here has two interpretations, depending on whether
; we're in selftest.
; 
; In selftest:
; $02 = first selftest screen (config bits, spinner line)
; $04 = second selftest screen (diagonal grid, line of characters)
; $06 = third selftest screen (crosshair, shrinking rectangle)
; $08 = fourth selftest screen (coloured lines)
; $0a = fifth selftest screen, grid with colour depending on spinner
; $0c = sixth selftest screen, blank rectangle
; 
; Not in selftest (see table at $c7da):
; $00 = entered briefly at game startup
; $02 = entered briefly at the beginning of first level of a game
; $04 = playing (including attract mode)
; $06 = entered briefly on player death (game-ending or not)
; $08 = set briefly at the beginning of each level?
; $0a = high-score, logo screen, AVOID SPIKES (etc?)
;       AVOID SPIKES: $1e->$04, $0a->gamestate, $20->$02, $80->$0123
; $0c = unused? (jump table holds $0000)
; $0e = entered briefly when zooming off the end of a level
; $12 = entering initials
; $16 = starting level selection screen
; $18 = zooming new level in
; $1a = unknown
; $1c = unknown
; $1e = unknown
; $20 = descending down tube at level end
; $22 = non-selftest service mode display
; $24 = high-score explosion
   gamestate: .byte   00
           1: .space  2
; Time-based counter
     timectr: .byte   00
           4: .byte   00
; $80 bit: set when player controlling, clear in attract mode
; $40 bit: set when player playing, clear when dead (entering score)
           5: .byte   00
     credits: .byte   00
           7: .space  1
zap_fire_shadow: .byte   00
; Shadow of optsw1.  Note value is XORed with $02 before storing.
coinage_shadow: .byte   00
optsw2_shadow: .byte   00 ; soft shadow of optsw2
           b: .space  11
 coin_string: .byte   00
  uncredited: .byte   00 ; coins accepted but not yet converted to credits
          18: .space  37
   curplayer: .byte   00
   twoplayer: .byte   00
          3f: .space  1
  p1_score_l: .byte   00
  p1_score_m: .byte   00
  p1_score_h: .byte   00
  p2_score_l: .byte   00
  p2_score_m: .byte   00
  p2_score_h: .byte   00
    p1_level: .byte   00
    p2_level: .byte   00
    p1_lives: .byte   00
    p2_lives: .byte   00
          4a: .space  2
; Bit positions:
; $08 = zap
; $10 = fire
; $20 = start 1
; $40 = start 2
; $80 = unknown (cleared at various points)
zap_fire_tmp1: .byte   00
zap_fire_debounce: .byte   00
zap_fire_new: .byte   00
zap_fire_tmp2: .byte   00
          50: .space  9
      fscale: .chunk  2 ; copied from lev_fscale[]/lev_fscale2[]
          5b: .byte   00
          5c: .byte   00
          5d: .byte   00
          5e: .byte   00
          5f: .byte   00
         y3d: .byte   00 ; copied from lev_y3d[]
          61: .byte   00
          62: .byte   00
          63: .byte   00
          64: .byte   00
          65: .byte   00
          66: .byte   00
          67: .byte   00
          68: .byte   00
          69: .byte   00
          6a: .byte   00
          6b: .byte   00
          6c: .byte   00
          6d: .byte   00
          6e: .byte   00
          6f: .byte   00
          70: .byte   00
          71: .byte   00
    curscale: .byte   00
      draw_z: .byte   00
    vidptr_l: .byte   00
    vidptr_h: .byte   00
          76: .space  41
    curlevel: .byte   00
          a0: .space  6
 enm_shotcnt: .byte   00
          a7: .space  5
      strtbl: .byte   00
    strtbl+1: .byte   00
          ae: .space  4
; Number of movement ticks between pulsar flips
pulsar_fliprate: .byte   00
; Ratio by which flipper flips at top-of-tube are accelerated relative to
; flipper flips in the tube.  If this number is 2, they are twice as fast,
; etc.  See $a141 for more.
flip_top_accel: .byte   00
          b4: .space  1
 copyr_cksum: .byte   00
copyr_vid_loc: .chunk  2
          b8: .space  5
; Pointer to RAM version of current EAROM block.  See $de{64,9a,9c,d8,ee}.
earom_memptr: .chunk  2
          bf: .space  67
p1_startchoice: .byte   00
p2_startchoice: .byte   00
 zoomspd_lsb: .byte   00
 zoomspd_msb: .byte   00
         106: .space  1
   along_lsb: .byte   00
  enemies_in: .byte   00
 enemies_top: .byte   00
   pcode_run: .byte   00
    pcode_pc: .byte   00
         10c: .byte   00
         10d: .byte   00
         10e: .byte   00
         10f: .byte   00
         110: .byte   00
  open_level: .byte   00 ; $00 if current level closed, $ff if open
     curtube: .byte   00 ; after remap[] has been applied
         113: .byte   00
         114: .byte   00
         115: .byte   00
         116: .byte   00
    flagbits: .byte   00
enm_shotspd_msb: .byte   00
; Shot holdoff time.  After firing, an enemy cannot fire until at least
; this many ticks have passed.
shot_holdoff: .byte   00
; Max # enemy shots minus one?
; See $9407, $9328, $a2d6.
 enm_shotmax: .byte   00
copyr_vid_cksum1: .byte   00
     max_enm: .byte   00
         11d: .space  3
enm_shotspd_lsb: .byte   00
         121: .space  2
; Display AVOID SPIKES when $80 bit set
         123: .byte   00
         124: .space  1
 zap_running: .byte   00
         126: .space  1
         127: .byte   00
         128: .space  1
; Indexed by enemy type.  Used at $9a15, $9a32, $9a6a.
; Computed at 9607_{13,1b,23,2b,33}.
; These appear to be minimum numbers we want for the various types; if we
; find there are fewer than this many, we'll create one of the highest type
; that satisfies that criterion.
min_flippers: .byte   00 ; flipper.  1-4:1, 5-99:0
 min_pulsars: .byte   00 ; pulsar.   1-16:0, 17-32:3, 33-99:1
 min_tankers: .byte   00 ; tanker.   1-2:0, 3:1, 4:0, 5-99:1
 min_spikers: .byte   00 ; spiker.   1-3:0, 4:1, 5-16:2, 17-19:0, 20-99:1
min_fuzzballs: .byte   00 ; fuzzball. 1 except 1-10, 17-19, 26, where 0
; Indexed by enemy type.  Max number of a given type in the tube (well, not
; really, but this is the number at which enemy creation will stop creating
; the corresponding type).
max_flippers: .byte   00 ; flipper
 max_pulsars: .byte   00 ; pulsar
 max_tankers: .byte   00 ; tanker
 max_spikers: .byte   00 ; spiker
max_fuzzballs: .byte   00 ; fuzzball
         133: .space  2
 ply_shotcnt: .byte   00
         136: .space  7
; Max-minus-cur values for each enemy.  Computed and used during creation
; of new enemies.
avl_flippers: .byte   00
 avl_pulsars: .byte   00
 avl_tankers: .byte   00
 avl_spikers: .byte   00
avl_fuzzballs: .byte   00
; Counts of the various enemy types currently alive.
  n_flippers: .byte   00
   n_pulsars: .byte   00
   n_tankers: .byte   00
   n_spikers: .byte   00
 n_fuzzballs: .byte   00
; $9475 describes how to compute an initial value for this.
; $9b5a uses this to update $0148.
; $9b8c/$9b94 negate this value when $0148 hits $0f and $c1, ascending and
; descending, respectively.
; eact_26 uses this and $0148 to set a conditional.
  pulse_beat: .byte   00
; Pulsar-pulsing variable.
; Set to $ff during init.
; Once per cycle (after all enemy movement has occurred), the computation
; $0148 += $0147 is done, and if the high bit of $0148 goes from 0 to 1
; because of this, $cd06 is called.  (See $9b56.)
; $0148 is used at $9b7c and in enemy action $26 (eact_26).  It's also
; checked at $9ce4 and $b3aa and $b71d/$b726.
     pulsing: .byte   00
; Vector of what tankers can carry.
 tanker_load: .chunk  4
         14d: .space  4
; Shot hit tolerance.  A player shot must be within this distance of an
; enemy (absolute value) in order to hit it.  Indexed by enemy type.
     hit_tol: .chunk  5
bonus_life_each: .byte   00
; Some position on tube - compare to enemy positions various places
         157: .byte   00
  init_lives: .byte   00
; Two flags that control fuzzball motion are kept here (in the $40 and $80
; bits).  See 9607_6b for setting, $9f2c/$9f4a/$9f6e for use.
fuzz_move_flg: .byte   00
wave_spikeht: .byte   00
wave_enemies: .byte   00
         15c: .space  1
; Movement type for flippers for this level.  See $9aa2 for use, 9607_5b
; for computation.
flipper_move: .byte   00
         15e: .space  1
; The chance of fuzzballs doing something in certain regions of the tube;
; see 9607_6f for computation, $9f69 for use.
fuzz_move_prb: .byte   00
; Speed figures, per-enemy-type, byte after the binary point
spd_flipper_lsb: .byte   00
spd_pulsar_lsb: .byte   00
spd_tanker_lsb: .byte   00
spd_spiker_lsb: .byte   00
spd_fuzzball_lsb: .byte   00
; Speed figures, per-enemy-type, byte before the binary point
spd_flipper_msb: .byte   00
spd_pulsar_msb: .byte   00
spd_tanker_msb: .byte   00
spd_spiker_msb: .byte   00
spd_fuzzball_msb: .byte   00
; $03 bits: difficulty (00=medium, 01=easy, 02=hard, 03=medium)
; $04 bit: rating (0 = 1,3,5,7,9; 1 = tied to high score)
; $08 bit: something unknown (comes from $20 bit of spinner/cabinet byte)
   diff_bits: .byte   00
         16b: .space  1
copyr_disp_cksum1: .byte   00
; Set to $40 to enable pulsars to fire.  See $937a, 9607_03, $9aac.
 pulsar_fire: .byte   00
         16e: .space  88
; $00 or $ff.  Used when diddling EAROM stuff; zero means copy from RAM
; to EAROM; $ff means clear both RAM and EAROM.
   earom_clr: .byte   00
; This is used only at $db5d and a number of places $de03-$de4f.
; This appears to be flag bits indicating which areas to save/load.
         1c7: .byte   00
; Used only at $de{0a,0d,18,3f} AFAICT.  Not sure what it does.
; I suspect this is read/write flag bits (set to write), one bit per area.
         1c8: .byte   00
; $03 bits (see $aba2):
;     $01 = top three sets of initials need initializing
;     $02 = top three scores need initializing
; I don't think any of its other bits are used or set, at least not in
; normal operation.  ($db65 makes me think selftest might use it.)
 hs_initflag: .byte   00
; EAROM operation in progress.
; Set to $80 to write (clear?)
; Set to $20 to read
; $40 is used internally by the $80 support code
    earom_op: .byte   00
; Offset from ($bd), cleared at $de27, incremented at $def9, used $de{76,d5}
earom_blkoff: .byte   00
; Current location in EAROM; see $de58/$de79/$defc.
   earom_ptr: .byte   00
; End of current block in EAROM; see $de5e/$de9e/$dec6.
earom_blkend: .byte   00
         1ce: .byte   00
; Checksum of block being currently read or written.
 earom_cksum: .byte   00
         1d0: .space  48
  player_seg: .byte   00 ; tube segment, 0-f, player is on
; Usually, this is is (player_seg+1)&0x0f.  But it's set to other values
; sometimes; the $80 bit seems to indicate "death in progress".
; $80 - player grabbed by flipper/pulsar
; $81 - player hit by enemy shot
; $81 - player spiked while going down tube
; I suspect that $80 means "death in progress" and, when so, $01 means
; "don't display player anyway".
         201: .space  1
player_along: .byte   00 ; player position along tube length
;                          normally $10, increases when going down tube
; Segment numbers for pending enemies.
; Set to random values $0-$f; see $9250.
 pending_seg: .chunk  64
; Video display information, one per pending enemy.
; $00 here means "no pending enemy in this slot".
; This byte breaks down as BBLLLLLL, and is used as a vscale value with
; b=BB+1 and l=LLLLLL.  This is initialized to the pending_seg value in
; the low nibble and the offset's low nibble in the high nibble, except
; that if that would give $00, $0f is used instead.  See $9250.
 pending_vid: .chunk  64
; Indexed by enemy number.
; $07 bits hold enemy type (0-4).
; $18 bits apparently mean something; see $b5bf.
; $40 bit set -> enemy_seg value increasing; clr -> decreasing (see $9eab)
; $80 bit set -> between segments (eg, flipper flipping)
         283: .chunk  7
; Indexed by enemy number.
; $80 bit set -> moving away from player, clear -> towards
; $40 means the enemy can shoot
; $03 bits determine what happens when enemy gets below $20:
;    $00 = no special action (as if $20 weren't special)
;    $01 = split into two flippers
;    $02 = split into two pulsars
;    $03 = split into two fuzzballs
         28a: .chunk  7
; Current movement pcode pc for enemies, indexed by enemy number
 enm_move_pc: .chunk  7
         298: .space  7
enemy_along_lsb: .chunk  7 ; indexed by enemy number
  shot_delay: .chunk  7 ; indexed by enemy number
 ply_shotseg: .chunk  8 ; indexed by player shot number
 enm_shotseg: .chunk  4 ; indexed by enemy shot number
   enemy_seg: .chunk  7 ; indexed by enemy number
         2c0: .space  12
; Indexed by enemy number.
; Flipper flipping: $80 plus current angle
; Flipper not flipping: segment number last flipped from
; Fuzzballs store $81 or $87 here, depending on the $40 bit of $0283,x
         2cc: .chunk  7
 ply_shotpos: .chunk  8 ; indexed by player shot number
 enm_shotpos: .chunk  4 ; indexed by enemy shot number
 enemy_along: .chunk  7 ; indexed by enemy number
         2e6: .chunk  4
         2ea: .space  4
enm_shot_lsb: .chunk  4 ; indexed by enemy shot number
; Indexed by player shot number
; Appears to affect player shot speed; see $a1a0.
; See also $a213 and $a22f.
; I suspect this is here so that shots that hit a spike can also, or
; perhaps instead, hit the spiker that is growing the spike.
         2f2: .chunk  8
         2fa: .space  8
; Indexed from 0 through 7; see $a401
         302: .chunk  8
; Indexed from 0 through 7; see $a3e2
         30a: .chunk  8
; Indexed from 0 through 7; see $a3e7
         312: .chunk  8
         31a: .space  64
         35a: .chunk  16
         36a: .chunk  16
         37a: .chunk  16
         38a: .chunk  16
; Indexed by tube segment number.
; Initialized to $00.
; Set to $80 when a spiker grows a spike.
; Set to $c0 when a player shot hits a spike.
; $80 bit seems to mean "don't draw bright dot at end"
; $40 bit seems to mean "draw mini-explosion at end"
         39a: .chunk  16
; 0=unused, 1=used once, 2=used twice
    zap_uses: .byte   00
enemies_pending: .byte   00
    spike_ht: .chunk  16
other_pl_data: .chunk  18
      tube_x: .chunk  16
      tube_y: .chunk  16
  tube_angle: .chunk  16
         3fe: .space  8
   on_time_l: .byte   00
   on_time_m: .byte   00
   on_time_h: .byte   00
 play_time_l: .byte   00
 play_time_m: .byte   00
 play_time_h: .byte   00
  games_1p_l: .byte   00
  games_1p_m: .byte   00
  games_1p_h: .byte   00
  games_2p_l: .byte   00
  games_2p_m: .byte   00
  games_2p_h: .byte   00
  secs_avg_l: .byte   00
  secs_avg_m: .byte   00
  secs_avg_h: .byte   00
; One-bit flags are kept in the low bit of these bytes; I suspect they
; are toggles for double-buffering sequences in video RAM.  See the code
; at $b310, which flips the bits and selects which table to pull a vjsr
; instruction from based on the result.
; See the code at b2c1, b2e1, b310, b33f, and b967 for more.
  dblbuf_flg: .chunk  9
         41e: .space  23
       mid_x: .chunk  16
       mid_y: .chunk  16
copyr_vid_cksum2: .byte   00
         456: .space  428
hs_whichletter: .byte   00
         603: .space  2
    hs_timer: .byte   00
hs_initials_8: .chunk  3
hs_initials_7: .chunk  3
hs_initials_6: .chunk  3
hs_initials_5: .chunk  3
hs_initials_4: .chunk  3
hs_initials_3: .chunk  3
hs_initials_2: .chunk  3
hs_initials_1: .chunk  3
         61e: .space  226
   hs_scores: .space  6
  hs_score_8: .space  3
  hs_score_7: .space  3
  hs_score_6: .space  3
  hs_score_5: .space  3
  hs_score_4: .space  3
  hs_score_3: .space  3
  hs_score_2: .space  3
  hs_score_1: .space  3
; Initial lives and points-per-life value, from optsw2.  This is a software
; shadow; when it's found to be different from the current settings, it's
; updated and the high-score list is cleared.
; See $ac20.
life_settings: .byte   00
; Difficulty settings, from zap_fire_starts.  This is a software shadow;
; when it's found to be different from the current settings, it's updated
; and the high-score list is cleared.
; See $ac20.
diff_settings: .byte   00
         720: .space  224
     col_ram: .space  1024
; $01 = right coin switch
; $02 = middle coin switch
; $04 = left coin switch (not used in arcade Tempest, I think)
; $08 = slam switch
; $10 = service switch
; $20 = diagnostic step switch
; $40 = halt bit from vector processor
; $80 = 3KHz square wave
       cabsw: .space  256
; $03 = coinage
;       $00 = 1 coin 1 credit
;       $01 = 2 coins 1 credit
;       $02 = free play
;       $03 = 1 coin 2 credits
; $0c = right coin multiplier
;       $00 = x1
;       $04 = x4
;       $08 = x5
;       $0c = x6
; $10 = middle ("left") coin multiplier
;       $00 = x1
;       $10 = x2
; $e0 = bonus coins
;       $00 = none
;       $20 = 1 each 2
;       $40 = 1 each 4
;       $60 = 2 each 4
;       $80 = 1 each 5
;       $a0 = 1 each 3
;       $c0 = demo mode
;       $e0 = demo mode freeze
      optsw1: .space  256
; $01 = minimum
;       $00 = 1 credit minimum
;       $01 = 2 credit minimum
; $06 = language
;       $00 = English
;       $02 = French
;       $04 = German
;       $06 = Spanish
; $38 = points needed per bonus life
;       $00 = 20000
;       $08 = 10000
;       $10 = 30000
;       $18 = 40000
;       $20 = 50000
;       $28 = 60000
;       $30 = 70000
;       $38 = none
; $c0 = initial lives
;       $00 = 3
;       $40 = 4
;       $80 = 5
;       $c0 = 2
      optsw2: .space  4608
      vecram: .space  4096
        3000: 48c0      vsdraw  x=+0 y=+16 z=12 ; letter A
        3002: 44c4      vsdraw  x=+8 y=+8 z=12
        3004: 5cc4      vsdraw  x=+8 y=-8 z=12
        3006: 58c0      vsdraw  x=+0 y=-16 z=12
        3008: 4418      vsdraw  x=-16 y=+8 z=off
        300a: 40c8      vsdraw  x=+16 y=+0 z=12
        300c: 5c04      vsdraw  x=+8 y=-8 z=off
        300e: c000      vrts
        3010: 4cc0      vsdraw  x=+0 y=+24 z=12 ; letter B
        3012: 40c6      vsdraw  x=+12 y=+0 z=12
        3014: 5ea2      vsdraw  x=+4 y=-4 z=10
        3016: 5ea0      vsdraw  x=+0 y=-4 z=10
        3018: 5ebe      vsdraw  x=-4 y=-4 z=10
        301a: 40da      vsdraw  x=-12 y=+0 z=12
        301c: 4006      vsdraw  x=+12 y=+0 z=off
        301e: 5ea2      vsdraw  x=+4 y=-4 z=10
        3020: 5ea0      vsdraw  x=+0 y=-4 z=10
        3022: 5ebe      vsdraw  x=-4 y=-4 z=10
        3024: 40da      vsdraw  x=-12 y=+0 z=12
        3026: 400c      vsdraw  x=+24 y=+0 z=off
        3028: c000      vrts
        302a: 4cc0      vsdraw  x=+0 y=+24 z=12 ; letter C
        302c: 40c8      vsdraw  x=+16 y=+0 z=12
        302e: 5418      vsdraw  x=-16 y=-24 z=off
        3030: 40c8      vsdraw  x=+16 y=+0 z=12
        3032: 4004      vsdraw  x=+8 y=+0 z=off
        3034: c000      vrts
        3036: 4cc0      vsdraw  x=+0 y=+24 z=12 ; letter D
        3038: 40c4      vsdraw  x=+8 y=+0 z=12
        303a: 5cc4      vsdraw  x=+8 y=-8 z=12
        303c: 5cc0      vsdraw  x=+0 y=-8 z=12
        303e: 5cdc      vsdraw  x=-8 y=-8 z=12
        3040: 40dc      vsdraw  x=-8 y=+0 z=12
        3042: 400c      vsdraw  x=+24 y=+0 z=off
        3044: c000      vrts
        3046: 4cc0      vsdraw  x=+0 y=+24 z=12 ; letter E
        3048: 40c8      vsdraw  x=+16 y=+0 z=12
        304a: 5a1e      vsdraw  x=-4 y=-12 z=off
        304c: 40da      vsdraw  x=-12 y=+0 z=12
        304e: 5a00      vsdraw  x=+0 y=-12 z=off
        3050: 40c8      vsdraw  x=+16 y=+0 z=12
        3052: 4004      vsdraw  x=+8 y=+0 z=off
        3054: c000      vrts
        3056: 4cc0      vsdraw  x=+0 y=+24 z=12 ; letter F
        3058: 40c8      vsdraw  x=+16 y=+0 z=12
        305a: 5a1e      vsdraw  x=-4 y=-12 z=off
        305c: 40da      vsdraw  x=-12 y=+0 z=12
        305e: 5a00      vsdraw  x=+0 y=-12 z=off
        3060: 400c      vsdraw  x=+24 y=+0 z=off
        3062: c000      vrts
        3064: 4cc0      vsdraw  x=+0 y=+24 z=12 ; letter G
        3066: 40c8      vsdraw  x=+16 y=+0 z=12
        3068: 5cc0      vsdraw  x=+0 y=-8 z=12
        306a: 5c1c      vsdraw  x=-8 y=-8 z=off
        306c: 40c4      vsdraw  x=+8 y=+0 z=12
        306e: 5cc0      vsdraw  x=+0 y=-8 z=12
        3070: 40d8      vsdraw  x=-16 y=+0 z=12
        3072: 400c      vsdraw  x=+24 y=+0 z=off
        3074: c000      vrts
        3076: 4cc0      vsdraw  x=+0 y=+24 z=12 ; letter H
        3078: 5a00      vsdraw  x=+0 y=-12 z=off
        307a: 40c8      vsdraw  x=+16 y=+0 z=12
        307c: 4600      vsdraw  x=+0 y=+12 z=off
        307e: 54c0      vsdraw  x=+0 y=-24 z=12
        3080: 4004      vsdraw  x=+8 y=+0 z=off
        3082: c000      vrts
        3084: 40c8      vsdraw  x=+16 y=+0 z=12 ; letter I
        3086: 401c      vsdraw  x=-8 y=+0 z=off
        3088: 4cc0      vsdraw  x=+0 y=+24 z=12
        308a: 4004      vsdraw  x=+8 y=+0 z=off
        308c: 40d8      vsdraw  x=-16 y=+0 z=12
        308e: 540c      vsdraw  x=+24 y=-24 z=off
        3090: c000      vrts
        3092: 4400      vsdraw  x=+0 y=+8 z=off ; letter J
        3094: 5cc4      vsdraw  x=+8 y=-8 z=12
        3096: 40c4      vsdraw  x=+8 y=+0 z=12
        3098: 4cc0      vsdraw  x=+0 y=+24 z=12
        309a: 5404      vsdraw  x=+8 y=-24 z=off
        309c: c000      vrts
        309e: 4cc0      vsdraw  x=+0 y=+24 z=12 ; letter K
        30a0: 4006      vsdraw  x=+12 y=+0 z=off
        30a2: 5ada      vsdraw  x=-12 y=-12 z=12
        30a4: 5ac6      vsdraw  x=+12 y=-12 z=12
        30a6: 4006      vsdraw  x=+12 y=+0 z=off
        30a8: c000      vrts
        30aa: 4c00      vsdraw  x=+0 y=+24 z=off ; letter L
        30ac: 54c0      vsdraw  x=+0 y=-24 z=12
        30ae: 40c8      vsdraw  x=+16 y=+0 z=12
        30b0: 4004      vsdraw  x=+8 y=+0 z=off
        30b2: c000      vrts
        30b4: 4cc0      vsdraw  x=+0 y=+24 z=12 ; letter M
        30b6: 5cc4      vsdraw  x=+8 y=-8 z=12
        30b8: 44c4      vsdraw  x=+8 y=+8 z=12
        30ba: 54c0      vsdraw  x=+0 y=-24 z=12
        30bc: 4004      vsdraw  x=+8 y=+0 z=off
        30be: c000      vrts
        30c0: 4cc0      vsdraw  x=+0 y=+24 z=12 ; letter N
        30c2: 54c8      vsdraw  x=+16 y=-24 z=12
        30c4: 4cc0      vsdraw  x=+0 y=+24 z=12
        30c6: 5404      vsdraw  x=+8 y=-24 z=off
        30c8: c000      vrts
        30ca: 4cc0      vsdraw  x=+0 y=+24 z=12 ; letter O; digit 0
        30cc: 40c8      vsdraw  x=+16 y=+0 z=12
        30ce: 54c0      vsdraw  x=+0 y=-24 z=12
        30d0: 40d8      vsdraw  x=-16 y=+0 z=12
        30d2: 400c      vsdraw  x=+24 y=+0 z=off
        30d4: c000      vrts
        30d6: 4cc0      vsdraw  x=+0 y=+24 z=12 ; letter P
        30d8: 40c8      vsdraw  x=+16 y=+0 z=12
        30da: 5ac0      vsdraw  x=+0 y=-12 z=12
        30dc: 40d8      vsdraw  x=-16 y=+0 z=12
        30de: 5a06      vsdraw  x=+12 y=-12 z=off
        30e0: 4006      vsdraw  x=+12 y=+0 z=off
        30e2: c000      vrts
        30e4: 4cc0      vsdraw  x=+0 y=+24 z=12 ; letter Q
        30e6: 40c8      vsdraw  x=+16 y=+0 z=12
        30e8: 58c0      vsdraw  x=+0 y=-16 z=12
        30ea: 5cdc      vsdraw  x=-8 y=-8 z=12
        30ec: 40dc      vsdraw  x=-8 y=+0 z=12
        30ee: 4404      vsdraw  x=+8 y=+8 z=off
        30f0: 5cc4      vsdraw  x=+8 y=-8 z=12
        30f2: 4004      vsdraw  x=+8 y=+0 z=off
        30f4: c000      vrts
        30f6: 4cc0      vsdraw  x=+0 y=+24 z=12 ; letter R
        30f8: 40c8      vsdraw  x=+16 y=+0 z=12
        30fa: 5ac0      vsdraw  x=+0 y=-12 z=12
        30fc: 40d8      vsdraw  x=-16 y=+0 z=12
        30fe: 4002      vsdraw  x=+4 y=+0 z=off
        3100: 5ac6      vsdraw  x=+12 y=-12 z=12
        3102: 4004      vsdraw  x=+8 y=+0 z=off
        3104: c000      vrts
        3106: 40c8      vsdraw  x=+16 y=+0 z=12 ; letter S
        3108: 46c0      vsdraw  x=+0 y=+12 z=12
        310a: 40d8      vsdraw  x=-16 y=+0 z=12
        310c: 46c0      vsdraw  x=+0 y=+12 z=12
        310e: 40c8      vsdraw  x=+16 y=+0 z=12
        3110: 5404      vsdraw  x=+8 y=-24 z=off
        3112: c000      vrts
        3114: 4004      vsdraw  x=+8 y=+0 z=off ; letter T
        3116: 4cc0      vsdraw  x=+0 y=+24 z=12
        3118: 401c      vsdraw  x=-8 y=+0 z=off
        311a: 40c8      vsdraw  x=+16 y=+0 z=12
        311c: 5404      vsdraw  x=+8 y=-24 z=off
        311e: c000      vrts
        3120: 4c00      vsdraw  x=+0 y=+24 z=off ; letter U
        3122: 54c0      vsdraw  x=+0 y=-24 z=12
        3124: 40c8      vsdraw  x=+16 y=+0 z=12
        3126: 4cc0      vsdraw  x=+0 y=+24 z=12
        3128: 5404      vsdraw  x=+8 y=-24 z=off
        312a: c000      vrts
        312c: 4c00      vsdraw  x=+0 y=+24 z=off ; letter V
        312e: 54c4      vsdraw  x=+8 y=-24 z=12
        3130: 4cc4      vsdraw  x=+8 y=+24 z=12
        3132: 5404      vsdraw  x=+8 y=-24 z=off
        3134: c000      vrts
        3136: 4c00      vsdraw  x=+0 y=+24 z=off ; letter W
        3138: 54c0      vsdraw  x=+0 y=-24 z=12
        313a: 44c4      vsdraw  x=+8 y=+8 z=12
        313c: 5cc4      vsdraw  x=+8 y=-8 z=12
        313e: 4cc0      vsdraw  x=+0 y=+24 z=12
        3140: 5404      vsdraw  x=+8 y=-24 z=off
        3142: c000      vrts
        3144: 4cc8      vsdraw  x=+16 y=+24 z=12 ; letter X
        3146: 4018      vsdraw  x=-16 y=+0 z=off
        3148: 54c8      vsdraw  x=+16 y=-24 z=12
        314a: 4004      vsdraw  x=+8 y=+0 z=off
        314c: c000      vrts
        314e: 4004      vsdraw  x=+8 y=+0 z=off ; letter Y
        3150: 48c0      vsdraw  x=+0 y=+16 z=12
        3152: 44dc      vsdraw  x=-8 y=+8 z=12
        3154: 4008      vsdraw  x=+16 y=+0 z=off
        3156: 5cdc      vsdraw  x=-8 y=-8 z=12
        3158: 5808      vsdraw  x=+16 y=-16 z=off
        315a: c000      vrts
        315c: 4c00      vsdraw  x=+0 y=+24 z=off ; letter Z
        315e: 40c8      vsdraw  x=+16 y=+0 z=12
        3160: 54d8      vsdraw  x=-16 y=-24 z=12
        3162: 40c8      vsdraw  x=+16 y=+0 z=12
        3164: 4004      vsdraw  x=+8 y=+0 z=off
        3166: c000      vrts
        3168: 400c      vsdraw  x=+24 y=+0 z=off ; space
        316a: c000      vrts
        316c: 4004      vsdraw  x=+8 y=+0 z=off ; digit 1
        316e: 4cc0      vsdraw  x=+0 y=+24 z=12
        3170: 5408      vsdraw  x=+16 y=-24 z=off
        3172: c000      vrts
        3174: 4c00      vsdraw  x=+0 y=+24 z=off ; digit 2
        3176: 40c8      vsdraw  x=+16 y=+0 z=12
        3178: 5ac0      vsdraw  x=+0 y=-12 z=12
        317a: 40d8      vsdraw  x=-16 y=+0 z=12
        317c: 5ac0      vsdraw  x=+0 y=-12 z=12
        317e: 40c8      vsdraw  x=+16 y=+0 z=12
        3180: 4004      vsdraw  x=+8 y=+0 z=off
        3182: c000      vrts
        3184: 40c8      vsdraw  x=+16 y=+0 z=12 ; digit 3
        3186: 4cc0      vsdraw  x=+0 y=+24 z=12
        3188: 40d8      vsdraw  x=-16 y=+0 z=12
        318a: 5a00      vsdraw  x=+0 y=-12 z=off
        318c: 40c8      vsdraw  x=+16 y=+0 z=12
        318e: 5a04      vsdraw  x=+8 y=-12 z=off
        3190: c000      vrts
        3192: 4c00      vsdraw  x=+0 y=+24 z=off ; digit 4
        3194: 5ac0      vsdraw  x=+0 y=-12 z=12
        3196: 40c8      vsdraw  x=+16 y=+0 z=12
        3198: 4600      vsdraw  x=+0 y=+12 z=off
        319a: 54c0      vsdraw  x=+0 y=-24 z=12
        319c: 4004      vsdraw  x=+8 y=+0 z=off
        319e: c000      vrts
        31a0: 40c8      vsdraw  x=+16 y=+0 z=12 ; digit 5
        31a2: 46c0      vsdraw  x=+0 y=+12 z=12
        31a4: 40d8      vsdraw  x=-16 y=+0 z=12
        31a6: 46c0      vsdraw  x=+0 y=+12 z=12
        31a8: 40c8      vsdraw  x=+16 y=+0 z=12
        31aa: 5404      vsdraw  x=+8 y=-24 z=off
        31ac: c000      vrts
        31ae: 4600      vsdraw  x=+0 y=+12 z=off ; digit 6
        31b0: 40c8      vsdraw  x=+16 y=+0 z=12
        31b2: 5ac0      vsdraw  x=+0 y=-12 z=12
        31b4: 40d8      vsdraw  x=-16 y=+0 z=12
        31b6: 4cc0      vsdraw  x=+0 y=+24 z=12
        31b8: 540c      vsdraw  x=+24 y=-24 z=off
        31ba: c000      vrts
        31bc: 4c00      vsdraw  x=+0 y=+24 z=off ; digit 7
        31be: 40c8      vsdraw  x=+16 y=+0 z=12
        31c0: 54c0      vsdraw  x=+0 y=-24 z=12
        31c2: 4004      vsdraw  x=+8 y=+0 z=off
        31c4: c000      vrts
        31c6: 40c8      vsdraw  x=+16 y=+0 z=12 ; digit 8
        31c8: 4cc0      vsdraw  x=+0 y=+24 z=12
        31ca: 40d8      vsdraw  x=-16 y=+0 z=12
        31cc: 54c0      vsdraw  x=+0 y=-24 z=12
        31ce: 4600      vsdraw  x=+0 y=+12 z=off
        31d0: 40c8      vsdraw  x=+16 y=+0 z=12
        31d2: 5a04      vsdraw  x=+8 y=-12 z=off
        31d4: c000      vrts
        31d6: 4008      vsdraw  x=+16 y=+0 z=off ; digit 9
        31d8: 4cc0      vsdraw  x=+0 y=+24 z=12
        31da: 40d8      vsdraw  x=-16 y=+0 z=12
        31dc: 5ac0      vsdraw  x=+0 y=-12 z=12
        31de: 40c8      vsdraw  x=+16 y=+0 z=12
        31e0: 5a04      vsdraw  x=+8 y=-12 z=off
        31e2: c000      vrts
; This is a table of vjsrs, copied out of when generating text.  It is also
; executed directly as part of one of the selftest displays.
 char_jsrtbl: a8b4      vjsr    $1168 ; space
        31e6: a865      vjsr    $10ca ; 0
        31e8: a8b6      vjsr    $116c ; 1
        31ea: a8ba      vjsr    $1174 ; 2
        31ec: a8c2      vjsr    $1184 ; 3
        31ee: a8c9      vjsr    $1192 ; 4
        31f0: a8d0      vjsr    $11a0 ; 5
        31f2: a8d7      vjsr    $11ae ; 6
        31f4: a8de      vjsr    $11bc ; 7
        31f6: a8e3      vjsr    $11c6 ; 8
        31f8: a8eb      vjsr    $11d6 ; 9
  ltr_jsrtbl: a800      vjsr    $1000 ; A
        31fc: a808      vjsr    $1010 ; B
        31fe: a815      vjsr    $102a ; C
        3200: a81b      vjsr    $1036 ; D
        3202: a823      vjsr    $1046 ; E
        3204: a82b      vjsr    $1056 ; F
        3206: a832      vjsr    $1064 ; G
        3208: a83b      vjsr    $1076 ; H
        320a: a842      vjsr    $1084 ; I
        320c: a849      vjsr    $1092 ; J
        320e: a84f      vjsr    $109e ; K
        3210: a855      vjsr    $10aa ; L
        3212: a85a      vjsr    $10b4 ; M
        3214: a860      vjsr    $10c0 ; N
        3216: a865      vjsr    $10ca ; O
        3218: a86b      vjsr    $10d6 ; P
        321a: a872      vjsr    $10e4 ; Q
        321c: a87b      vjsr    $10f6 ; R
        321e: a883      vjsr    $1106 ; S
        3220: a88a      vjsr    $1114 ; T
        3222: a890      vjsr    $1120 ; U
        3224: a896      vjsr    $112c ; V
        3226: a89b      vjsr    $1136 ; W
        3228: a8a2      vjsr    $1144 ; X
        322a: a8a7      vjsr    $114e ; Y
        322c: a8ae      vjsr    $115c ; Z
        322e: a8b4      vjsr    $1168 ; space
        3230: a91b      vjsr    $1236 ; -
        3232: a92e      vjsr    $125c ; 1/2
        3234: a91f      vjsr    $123e ; copyright
        3236: 4600      vsdraw  x=+0 y=+12 z=off ; -
        3238: 40c8      vsdraw  x=+16 y=+0 z=12
        323a: 5a04      vsdraw  x=+8 y=-12 z=off
        323c: c000      vrts
        323e: 4200      vsdraw  x=+0 y=+4 z=off ; copyright
        3240: 48c0      vsdraw  x=+0 y=+16 z=12
        3242: 42c2      vsdraw  x=+4 y=+4 z=12
        3244: 40c4      vsdraw  x=+8 y=+0 z=12
        3246: 5ec2      vsdraw  x=+4 y=-4 z=12
        3248: 58c0      vsdraw  x=+0 y=-16 z=12
        324a: 5ede      vsdraw  x=-4 y=-4 z=12
        324c: 40dc      vsdraw  x=-8 y=+0 z=12
        324e: 42de      vsdraw  x=-4 y=+4 z=12
        3250: 4206      vsdraw  x=+12 y=+4 z=off
        3252: 40dc      vsdraw  x=-8 y=+0 z=12
        3254: 44c0      vsdraw  x=+0 y=+8 z=12
        3256: 40c4      vsdraw  x=+8 y=+0 z=12
        3258: 5806      vsdraw  x=+12 y=-16 z=off
        325a: c000      vrts
        325c: 4cc8      vsdraw  x=+16 y=+24 z=12 ; 1/2
        325e: 5b19      vsdraw  x=-14 y=-10 z=off
        3260: 7200      vscale  b=2 l=0
        3262: a8b6      vjsr    $116c ; 1
        3264: 521c      vsdraw  x=-8 y=-28 z=off
        3266: a8ba      vjsr    $1174 ; 2
        3268: 7100      vscale  b=1 l=0
        326a: c02a      vrts
; Player, "nominal" picture (the one used for eg #-ships display)
        326c: 68c1      vstat   z=12 c=1 sparkle=1
        326e: 5acc      vsdraw  x=+24 y=-12 z=12
        3270: 5dd7      vsdraw  x=-18 y=-6 z=12
        3272: 43c6      vsdraw  x=+12 y=+6 z=12
        3274: 43d7      vsdraw  x=-18 y=+6 z=12
        3276: 5dd7      vsdraw  x=-18 y=-6 z=12
        3278: 5dc6      vsdraw  x=+12 y=-6 z=12
        327a: 43d7      vsdraw  x=-18 y=+6 z=12
        327c: 46cc      vsdraw  x=+24 y=+12 z=12
        327e: 0000003c  vldraw  x=+60 y=+0 z=off
        3282: c000      vrts
        3284: a937      vjsr    $126e
        3286: a93f      vjsr    $127e
; Set of 6 vertical lines (used in a selftest display)
        3288: 01004000  vldraw  x=+0 y=+256 z=4
        328c: 1f000010  vldraw  x=+16 y=-256 z=off
        3290: 01006000  vldraw  x=+0 y=+256 z=6
        3294: 1f000010  vldraw  x=+16 y=-256 z=off
        3298: 01008000  vldraw  x=+0 y=+256 z=8
        329c: 1f000010  vldraw  x=+16 y=-256 z=off
        32a0: 0100a000  vldraw  x=+0 y=+256 z=10
        32a4: 1f000010  vldraw  x=+16 y=-256 z=off
        32a8: 0100c000  vldraw  x=+0 y=+256 z=12
        32ac: 1f000010  vldraw  x=+16 y=-256 z=off
        32b0: 0100e000  vldraw  x=+0 y=+256 z=14
        32b4: c000      vrts
; draw selftest display with bunches of 6 vertical lines
        32b6: aa53      vjsr    $14a6 ; draw box around screen
        32b8: 8040      vcentre
        32ba: 1e800040  vldraw  x=+64 y=-384 z=off
        32be: 68c3      vstat   z=12 c=3 sparkle=1
        32c0: a944      vjsr    $1288
        32c2: 8040      vcentre
        32c4: 1f800040  vldraw  x=+64 y=-128 z=off
        32c8: 68c7      vstat   z=12 c=7 sparkle=1
        32ca: a944      vjsr    $1288
        32cc: 8040      vcentre
        32ce: 00800040  vldraw  x=+64 y=+128 z=off
        32d2: 68c5      vstat   z=12 c=5 sparkle=1
        32d4: a944      vjsr    $1288
        32d6: 8040      vcentre
        32d8: 1e801f60  vldraw  x=-160 y=-384 z=off
        32dc: 68c1      vstat   z=12 c=1 sparkle=1
        32de: a944      vjsr    $1288
        32e0: 8040      vcentre
        32e2: 1f801f60  vldraw  x=-160 y=-128 z=off
        32e6: 68c4      vstat   z=12 c=4 sparkle=1
        32e8: a944      vjsr    $1288
        32ea: 8040      vcentre
        32ec: 00801f60  vldraw  x=-160 y=+128 z=off
        32f0: 68c2      vstat   z=12 c=2 sparkle=1
        32f2: a944      vjsr    $1288
        32f4: 8040      vcentre
        32f6: 1f801fd0  vldraw  x=-48 y=-128 z=off
        32fa: 68c0      vstat   z=12 c=0 sparkle=1
        32fc: 01004000  vldraw  x=+0 y=+256 z=4
        3300: 1ec00010  vldraw  x=+16 y=-320 z=off
        3304: 01406000  vldraw  x=+0 y=+320 z=6
        3308: e94a      vjmp    $1294
; draw selftest display with diagonal grid and string of characters
        330a: aa53      vjsr    $14a6 ; draw box around screen
        330c: 8040      vcentre
        330e: 1de41e0c  vldraw  x=-500 y=-540 z=off
        3312: 032a83e8  vldraw  x=+1000 y=+810 z=8
        3316: 010e9eb2  vldraw  x=-334 y=+270 z=8
        331a: 1de49d66  vldraw  x=-666 y=-540 z=8
        331e: 1de4829a  vldraw  x=+666 y=-540 z=8
        3322: 010e814e  vldraw  x=+334 y=+270 z=8
        3326: 032a9c18  vldraw  x=-1000 y=+810 z=8
        332a: 000003e8  vldraw  x=+1000 y=+0 z=off
        332e: 1cd69c18  vldraw  x=-1000 y=-810 z=8
        3332: 1ef2814e  vldraw  x=+334 y=-270 z=8
        3336: 021c829a  vldraw  x=+666 y=+540 z=8
        333a: 021c9d66  vldraw  x=-666 y=+540 z=8
        333e: 1ef29eb2  vldraw  x=-334 y=-270 z=8
        3342: 1cd683e8  vldraw  x=+1000 y=-810 z=8
        3346: 8040      vcentre
        3348: 1e9a1df8  vldraw  x=-520 y=-358 z=off
        334c: e8f2      vjmp    $11e4
; draw selftest display that consists of a rectangular grid
        334e: 7100      vscale  b=1 l=0
        3350: 8040      vcentre
        3352: 1de501f4  vldraw  x=+500 y=-539 z=off
        3356: 0000bc18  vldraw  x=-1000 y=+0 z=10
        335a: 8040      vcentre
        335c: 1e3201f4  vldraw  x=+500 y=-462 z=off
        3360: 0000bc18  vldraw  x=-1000 y=+0 z=10
        3364: 8040      vcentre
        3366: 1e7f01f4  vldraw  x=+500 y=-385 z=off
        336a: 0000bc18  vldraw  x=-1000 y=+0 z=10
        336e: 8040      vcentre
        3370: 1ecc01f4  vldraw  x=+500 y=-308 z=off
        3374: 0000bc18  vldraw  x=-1000 y=+0 z=10
        3378: 8040      vcentre
        337a: 1f1901f4  vldraw  x=+500 y=-231 z=off
        337e: 0000bc18  vldraw  x=-1000 y=+0 z=10
        3382: 8040      vcentre
        3384: 1f6601f4  vldraw  x=+500 y=-154 z=off
        3388: 0000bc18  vldraw  x=-1000 y=+0 z=10
        338c: 8040      vcentre
        338e: 1fb301f4  vldraw  x=+500 y=-77 z=off
        3392: 0000bc18  vldraw  x=-1000 y=+0 z=10
        3396: 8040      vcentre
        3398: 000001f4  vldraw  x=+500 y=+0 z=off
        339c: 0000bc18  vldraw  x=-1000 y=+0 z=10
        33a0: 8040      vcentre
        33a2: 004d01f4  vldraw  x=+500 y=+77 z=off
        33a6: 0000bc18  vldraw  x=-1000 y=+0 z=10
        33aa: 8040      vcentre
        33ac: 009a01f4  vldraw  x=+500 y=+154 z=off
        33b0: 0000bc18  vldraw  x=-1000 y=+0 z=10
        33b4: 8040      vcentre
        33b6: 00e701f4  vldraw  x=+500 y=+231 z=off
        33ba: 0000bc18  vldraw  x=-1000 y=+0 z=10
        33be: 8040      vcentre
        33c0: 013401f4  vldraw  x=+500 y=+308 z=off
        33c4: 0000bc18  vldraw  x=-1000 y=+0 z=10
        33c8: 8040      vcentre
        33ca: 018101f4  vldraw  x=+500 y=+385 z=off
        33ce: 0000bc18  vldraw  x=-1000 y=+0 z=10
        33d2: 8040      vcentre
        33d4: 01ce01f4  vldraw  x=+500 y=+462 z=off
        33d8: 0000bc18  vldraw  x=-1000 y=+0 z=10
        33dc: 8040      vcentre
        33de: 021b01f4  vldraw  x=+500 y=+539 z=off
        33e2: 0000bc18  vldraw  x=-1000 y=+0 z=10
        33e6: 8040      vcentre
        33e8: 021c1e0d  vldraw  x=-499 y=+540 z=off
        33ec: 1bc8a000  vldraw  x=+0 y=-1080 z=10
        33f0: 8040      vcentre
        33f2: 021c1e71  vldraw  x=-399 y=+540 z=off
        33f6: 1bc8a000  vldraw  x=+0 y=-1080 z=10
        33fa: 8040      vcentre
        33fc: 021c1ed5  vldraw  x=-299 y=+540 z=off
        3400: 1bc8a000  vldraw  x=+0 y=-1080 z=10
        3404: 8040      vcentre
        3406: 021c1f39  vldraw  x=-199 y=+540 z=off
        340a: 1bc8a000  vldraw  x=+0 y=-1080 z=10
        340e: 8040      vcentre
        3410: 021c1f9d  vldraw  x=-99 y=+540 z=off
        3414: 1bc8a000  vldraw  x=+0 y=-1080 z=10
        3418: 8040      vcentre
        341a: 021c0001  vldraw  x=+1 y=+540 z=off
        341e: 1bc8a000  vldraw  x=+0 y=-1080 z=10
        3422: 8040      vcentre
        3424: 021c0065  vldraw  x=+101 y=+540 z=off
        3428: 1bc8a000  vldraw  x=+0 y=-1080 z=10
        342c: 8040      vcentre
        342e: 021c00c9  vldraw  x=+201 y=+540 z=off
        3432: 1bc8a000  vldraw  x=+0 y=-1080 z=10
        3436: 8040      vcentre
        3438: 021c012d  vldraw  x=+301 y=+540 z=off
        343c: 1bc8a000  vldraw  x=+0 y=-1080 z=10
        3440: 8040      vcentre
        3442: 021c0191  vldraw  x=+401 y=+540 z=off
        3446: 1bc8a000  vldraw  x=+0 y=-1080 z=10
        344a: 8040      vcentre
        344c: 021c01f5  vldraw  x=+501 y=+540 z=off
        3450: 1bc8a000  vldraw  x=+0 y=-1080 z=10
        3454: c000      vrts
; draw full-screen crosshair, used in selftest
        3456: aa45      vjsr    $148a
        3458: 00008300  vldraw  x=+768 y=+0 z=8
        345c: 02401d00  vldraw  x=-768 y=+576 z=off
        3460: 1b808000  vldraw  x=+0 y=-1152 z=8
        3464: 02401d00  vldraw  x=-768 y=+576 z=off
        3468: 00008300  vldraw  x=+768 y=+0 z=8
        346c: c000      vrts
        346e: aa45      vjsr    $148a
        3470: 008c1ec0  vldraw  x=-320 y=+140 z=off
        3474: a823      vjsr    $1046 ; E
        3476: a87b      vjsr    $10f6 ; R
        3478: a800      vjsr    $1000 ; A
        347a: a883      vjsr    $1106 ; S
        347c: a842      vjsr    $1084 ; I
        347e: a860      vjsr    $10c0 ; N
        3480: e832      vjmp    $1064 ; G
; draw cocktail-bit C
        3482: aa45      vjsr    $148a
        3484: 1fd81ed4  vldraw  x=-300 y=-40 z=off
        3488: e815      vjmp    $102a ; C
        348a: 68c0      vstat   z=12 c=0 sparkle=1
        348c: 8040      vcentre
        348e: 7100      vscale  b=1 l=0
        3490: c000      vrts
; draw box around screen with line across the middle
        3492: aa53      vjsr    $14a6
        3494: 8040      vcentre
        3496: 7100      vscale  b=1 l=0
        3498: 000001f4  vldraw  x=+500 y=+0 z=off
        349c: 0000dc18  vldraw  x=-1000 y=+0 z=12
        34a0: 01c00040  vldraw  x=+64 y=+448 z=off
        34a4: c000      vrts
; Draw box around screen (used in selftest display)
        34a6: 68c0      vstat   z=12 c=0 sparkle=1
        34a8: 7100      vscale  b=1 l=0
; Draw just the box (used in shrinking-box selftest screen)
        34aa: 8040      vcentre
        34ac: 1de41e0c  vldraw  x=-500 y=-540 z=off
        34b0: 0000c3e8  vldraw  x=+1000 y=+0 z=12
        34b4: 0438c000  vldraw  x=+0 y=+1080 z=12
        34b8: 0000dc18  vldraw  x=-1000 y=+0 z=12
        34bc: 1bc8c000  vldraw  x=+0 y=-1080 z=12
        34c0: c000      vrts
; Explosion, size 1
        34c2: 68c0      vstat   z=12 c=0 sparkle=1
        34c4: 00030007  vldraw  x=+7 y=+3 z=off
        34c8: 5df9      vsdraw  x=-14 y=-6 z=14
        34ca: 1ffd0001  vldraw  x=+1 y=-3 z=off
        34ce: 46e6      vsdraw  x=+12 y=+12 z=14
        34d0: 00011ffd  vldraw  x=-3 y=+1 z=off
        34d4: 59fd      vsdraw  x=-6 y=-14 z=14
        34d6: 1fff0003  vldraw  x=+3 y=-1 z=off
        34da: 48e0      vsdraw  x=+0 y=+16 z=14
        34dc: 1fff1ffd  vldraw  x=-3 y=-1 z=off
        34e0: 59e3      vsdraw  x=+6 y=-14 z=14
        34e2: 00010003  vldraw  x=+3 y=+1 z=off
        34e6: 46fa      vsdraw  x=-12 y=+12 z=14
        34e8: 1ffd1fff  vldraw  x=-1 y=-3 z=off
        34ec: 5de7      vsdraw  x=+14 y=-6 z=14
        34ee: 00030001  vldraw  x=+1 y=+3 z=off
        34f2: 40f8      vsdraw  x=-16 y=+0 z=14
        34f4: 4004      vsdraw  x=+8 y=+0 z=off
        34f6: c000      vrts
; Explosion, size 2
        34f8: 68c0      vstat   z=12 c=0 sparkle=1
        34fa: 4307      vsdraw  x=+14 y=+6 z=off
        34fc: 5bd2      vsdraw  x=-28 y=-10 z=12 ; x=-27 y=-12?
        34fe: 5d01      vsdraw  x=+2 y=-6 z=off
        3500: 4dcc      vsdraw  x=+24 y=+26 z=12 ; x=+24 y=+24?
        3502: 411d      vsdraw  x=-6 y=+2 z=off
        3504: 53da      vsdraw  x=-12 y=-26 z=12 ; x=-12 y=-28?
        3506: 5f03      vsdraw  x=+6 y=-2 z=off
        3508: 0020c000  vldraw  x=+0 y=+32 z=12
        350c: 5f1d      vsdraw  x=-6 y=-2 z=off
        350e: 53c6      vsdraw  x=+12 y=-26 z=12 ; x=+12 y=-28?
        3510: 4103      vsdraw  x=+6 y=+2 z=off
        3512: 4dd4      vsdraw  x=-24 y=+26 z=12 ; x=-24 y=+24?
        3514: 5d1f      vsdraw  x=-2 y=-6 z=off
        3516: 5bce      vsdraw  x=+28 y=-10 z=12 ; x=+28 y=-12?
        3518: 4301      vsdraw  x=+2 y=+6 z=off
        351a: 0000dfe0  vldraw  x=-32 y=+0 z=12
        351e: 4008      vsdraw  x=+16 y=+0 z=off
        3520: c000      vrts
; Explosion, size 3
        3522: 68c0      vstat   z=12 c=0 sparkle=1
        3524: 460e      vsdraw  x=+28 y=+12 z=off
        3526: 1fe8dfc8  vldraw  x=-56 y=-24 z=12
        352a: 5a02      vsdraw  x=+4 y=-12 z=off
        352c: 0030c030  vldraw  x=+48 y=+48 z=12
        3530: 421a      vsdraw  x=-12 y=+4 z=off
        3532: 1fc8dfe8  vldraw  x=-24 y=-56 z=12
        3536: 5e06      vsdraw  x=+12 y=-4 z=off
        3538: 0040c000  vldraw  x=+0 y=+64 z=12
        353c: 5e1a      vsdraw  x=-12 y=-4 z=off
        353e: 1fc8c018  vldraw  x=+24 y=-56 z=12
        3542: 4206      vsdraw  x=+12 y=+4 z=off
        3544: 0030dfd0  vldraw  x=-48 y=+48 z=12
        3548: 5a1e      vsdraw  x=-4 y=-12 z=off
        354a: 1fe8c038  vldraw  x=+56 y=-24 z=12
        354e: 4602      vsdraw  x=+4 y=+12 z=off
        3550: 0000dfc0  vldraw  x=-64 y=+0 z=12
        3554: 00000020  vldraw  x=+32 y=+0 z=off
        3558: c000      vrts
; Explosion, size 4
        355a: 68c0      vstat   z=12 c=0 sparkle=1
        355c: 00180038  vldraw  x=+56 y=+24 z=off
        3560: 1fd0df90  vldraw  x=-112 y=-48 z=12
        3564: 5404      vsdraw  x=+8 y=-24 z=off
        3566: 0060c060  vldraw  x=+96 y=+96 z=12
        356a: 4414      vsdraw  x=-24 y=+8 z=off
        356c: 1f90dfd0  vldraw  x=-48 y=-112 z=12
        3570: 5c0c      vsdraw  x=+24 y=-8 z=off
        3572: 0080c000  vldraw  x=+0 y=+128 z=12
        3576: 5c14      vsdraw  x=-24 y=-8 z=off
        3578: 1f90c030  vldraw  x=+48 y=-112 z=12
        357c: 440c      vsdraw  x=+24 y=+8 z=off
        357e: 0060dfa0  vldraw  x=-96 y=+96 z=12
        3582: 541c      vsdraw  x=-8 y=-24 z=off
        3584: 1fd0c070  vldraw  x=+112 y=-48 z=12
        3588: 4c04      vsdraw  x=+8 y=+24 z=off
        358a: 0000df80  vldraw  x=-128 y=+0 z=12
        358e: 00000040  vldraw  x=+64 y=+0 z=off
        3592: c000      vrts
; Player shot
        3594: 68c8      vstat   z=12 c=8 sparkle=1
        3596: 00000000  vldraw  x=+0 y=+0 z=off
        359a: 00002000  vldraw  x=+0 y=+0 z=stat
        359e: 00000007  vldraw  x=+7 y=+0 z=off
        35a2: 00002000  vldraw  x=+0 y=+0 z=stat
        35a6: 00051ffe  vldraw  x=-2 y=+5 z=off
        35aa: 00002000  vldraw  x=+0 y=+0 z=stat
        35ae: 00021ffb  vldraw  x=-5 y=+2 z=off
        35b2: 00002000  vldraw  x=+0 y=+0 z=stat
        35b6: 1ffe1ffb  vldraw  x=-5 y=-2 z=off
        35ba: 00002000  vldraw  x=+0 y=+0 z=stat
        35be: 1ffb1ffe  vldraw  x=-2 y=-5 z=off
        35c2: 00002000  vldraw  x=+0 y=+0 z=stat
        35c6: 1ffb0002  vldraw  x=+2 y=-5 z=off
        35ca: 00002000  vldraw  x=+0 y=+0 z=stat
        35ce: 1ffe0005  vldraw  x=+5 y=-2 z=off
        35d2: 00002000  vldraw  x=+0 y=+0 z=stat
        35d6: 00020005  vldraw  x=+5 y=+2 z=off
        35da: 00002000  vldraw  x=+0 y=+0 z=stat
        35de: 68c1      vstat   z=12 c=1 sparkle=1
        35e0: 0005000a  vldraw  x=+10 y=+5 z=off
        35e4: 00002000  vldraw  x=+0 y=+0 z=stat
        35e8: 000b1ffc  vldraw  x=-4 y=+11 z=off
        35ec: 00002000  vldraw  x=+0 y=+0 z=stat
        35f0: 00041ff5  vldraw  x=-11 y=+4 z=off
        35f4: 00002000  vldraw  x=+0 y=+0 z=stat
        35f8: 1ffc1ff5  vldraw  x=-11 y=-4 z=off
        35fc: 00002000  vldraw  x=+0 y=+0 z=stat
        3600: 1ff50000  vldraw  x=+0 y=-11 z=off ; x=-4 y=-11?
        3604: 00002000  vldraw  x=+0 y=+0 z=stat
        3608: 1ff50000  vldraw  x=+0 y=-11 z=off ; x=+4 y=-11?
        360c: 00002000  vldraw  x=+0 y=+0 z=stat
        3610: 0000000b  vldraw  x=+11 y=+0 z=off ; x=+11 y=-4?
        3614: 00002000  vldraw  x=+0 y=+0 z=stat
        3618: 0000000b  vldraw  x=+11 y=+0 z=off ; x=+11 y=+4?
        361c: 00002000  vldraw  x=+0 y=+0 z=stat
        3620: c000      vrts
; Cloud of dots (starfield between levels?)
        3622: 60f0      vstat   z=15 c=0 sparkle=0
        3624: ab14      vjsr    $1628
        3626: eb6f      vjmp    $16de
; Cloud of dots (starfield between levels?)
        3628: 00001fe0  vldraw  x=-32 y=+0 z=off
        362c: 00002000  vldraw  x=+0 y=+0 z=stat
        3630: 00300040  vldraw  x=+64 y=+48 z=off
        3634: 00002000  vldraw  x=+0 y=+0 z=stat
        3638: 1fd00020  vldraw  x=+32 y=-48 z=off
        363c: 00002000  vldraw  x=+0 y=+0 z=stat
        3640: 00c01fe0  vldraw  x=-32 y=+192 z=off
        3644: 00002000  vldraw  x=+0 y=+0 z=stat
        3648: 1fc01f20  vldraw  x=-224 y=-64 z=off
        364c: 00002000  vldraw  x=+0 y=+0 z=stat
        3650: 1f001ff0  vldraw  x=-16 y=-256 z=off
        3654: 00002000  vldraw  x=+0 y=+0 z=stat
        3658: 1f6000b0  vldraw  x=+176 y=-160 z=off
        365c: 00002000  vldraw  x=+0 y=+0 z=stat
        3660: 00a00140  vldraw  x=+320 y=+160 z=off
        3664: 00002000  vldraw  x=+0 y=+0 z=stat
        3668: 01201ff0  vldraw  x=-16 y=+288 z=off
        366c: 00002000  vldraw  x=+0 y=+0 z=stat
        3670: 00a01f50  vldraw  x=-176 y=+160 z=off
        3674: 00002000  vldraw  x=+0 y=+0 z=stat
        3678: 1fd01ec0  vldraw  x=-320 y=-48 z=off
        367c: 00002000  vldraw  x=+0 y=+0 z=stat
        3680: 1ed01fc0  vldraw  x=-64 y=-304 z=off
        3684: 00002000  vldraw  x=+0 y=+0 z=stat
        3688: 1ee00020  vldraw  x=+32 y=-288 z=off
        368c: 00002000  vldraw  x=+0 y=+0 z=stat
        3690: 1f800140  vldraw  x=+320 y=-128 z=off
        3694: 00002000  vldraw  x=+0 y=+0 z=stat
        3698: 00800120  vldraw  x=+288 y=+128 z=off
        369c: 00002000  vldraw  x=+0 y=+0 z=stat
        36a0: 01200040  vldraw  x=+64 y=+288 z=off
        36a4: 00002000  vldraw  x=+0 y=+0 z=stat
        36a8: 01601fc0  vldraw  x=-64 y=+352 z=off
        36ac: 00002000  vldraw  x=+0 y=+0 z=stat
        36b0: 00801ec0  vldraw  x=-320 y=+128 z=off
        36b4: 00002000  vldraw  x=+0 y=+0 z=stat
        36b8: 1fe01ee0  vldraw  x=-288 y=-32 z=off
        36bc: 00002000  vldraw  x=+0 y=+0 z=stat
        36c0: 1f001f20  vldraw  x=-224 y=-256 z=off
        36c4: 00002000  vldraw  x=+0 y=+0 z=stat
        36c8: 1ec00020  vldraw  x=+32 y=-320 z=off
        36cc: 00002000  vldraw  x=+0 y=+0 z=stat
        36d0: 1f000000  vldraw  x=+0 y=-256 z=off
        36d4: 00002000  vldraw  x=+0 y=+0 z=stat
        36d8: 01a001c0  vldraw  x=+448 y=+416 z=off
        36dc: c000      vrts
; Cloud of dots (starfield between levels?)
        36de: 00200020  vldraw  x=+32 y=+32 z=off
        36e2: 00002000  vldraw  x=+0 y=+0 z=stat
        36e6: 00201fc0  vldraw  x=-64 y=+32 z=off
        36ea: 00002000  vldraw  x=+0 y=+0 z=stat
        36ee: 1f801fc0  vldraw  x=-64 y=-128 z=off
        36f2: 00002000  vldraw  x=+0 y=+0 z=stat
        36f6: 1fa00080  vldraw  x=+128 y=-96 z=off
        36fa: 00002000  vldraw  x=+0 y=+0 z=stat
        36fe: 00800080  vldraw  x=+128 y=+128 z=off
        3702: 00002000  vldraw  x=+0 y=+0 z=stat
        3706: 00a01fe0  vldraw  x=-32 y=+160 z=off
        370a: 00002000  vldraw  x=+0 y=+0 z=stat
        370e: 00601fa0  vldraw  x=-96 y=+96 z=off
        3712: 00002000  vldraw  x=+0 y=+0 z=stat
        3716: 1fe01f60  vldraw  x=-160 y=-32 z=off
        371a: 00002000  vldraw  x=+0 y=+0 z=stat
        371e: 1f601f80  vldraw  x=-128 y=-160 z=off
        3722: 00002000  vldraw  x=+0 y=+0 z=stat
        3726: 1ee00060  vldraw  x=+96 y=-288 z=off
        372a: 00002000  vldraw  x=+0 y=+0 z=stat
        372e: 1ff00170  vldraw  x=+368 y=-16 z=off
        3732: 00002000  vldraw  x=+0 y=+0 z=stat
        3736: 01700090  vldraw  x=+144 y=+368 z=off
        373a: 00002000  vldraw  x=+0 y=+0 z=stat
        373e: 00e01f80  vldraw  x=-128 y=+224 z=off
        3742: 00002000  vldraw  x=+0 y=+0 z=stat
        3746: 00601ee0  vldraw  x=-288 y=+96 z=off
        374a: 00002000  vldraw  x=+0 y=+0 z=stat
        374e: 1f001ee0  vldraw  x=-288 y=-256 z=off
        3752: 00002000  vldraw  x=+0 y=+0 z=stat
        3756: 1ea00000  vldraw  x=+0 y=-352 z=off
        375a: 00002000  vldraw  x=+0 y=+0 z=stat
        375e: 1f200080  vldraw  x=+128 y=-224 z=off
        3762: 00002000  vldraw  x=+0 y=+0 z=stat
        3766: 1fc000e0  vldraw  x=+224 y=-64 z=off
        376a: 00002000  vldraw  x=+0 y=+0 z=stat
        376e: 00400180  vldraw  x=+384 y=+64 z=off
        3772: 00002000  vldraw  x=+0 y=+0 z=stat
        3776: 00e00020  vldraw  x=+32 y=+224 z=off
        377a: 00002000  vldraw  x=+0 y=+0 z=stat
        377e: c000      vrts
; Cloud of dots (starfield between levels?)
        3780: 00400040  vldraw  x=+64 y=+64 z=off
        3784: 00002000  vldraw  x=+0 y=+0 z=stat
        3788: 00201fc0  vldraw  x=-64 y=+32 z=off
        378c: 00002000  vldraw  x=+0 y=+0 z=stat
        3790: 1fc01f40  vldraw  x=-192 y=-64 z=off
        3794: 00002000  vldraw  x=+0 y=+0 z=stat
        3798: 1f300060  vldraw  x=+96 y=-208 z=off
        379c: 00002000  vldraw  x=+0 y=+0 z=stat
        37a0: 00500100  vldraw  x=+256 y=+80 z=off
        37a4: 00002000  vldraw  x=+0 y=+0 z=stat
        37a8: 00800020  vldraw  x=+32 y=+128 z=off
        37ac: 00002000  vldraw  x=+0 y=+0 z=stat
        37b0: 00c00020  vldraw  x=+32 y=+192 z=off
        37b4: 00002000  vldraw  x=+0 y=+0 z=stat
        37b8: 00401f20  vldraw  x=-224 y=+64 z=off
        37bc: 00002000  vldraw  x=+0 y=+0 z=stat
        37c0: 1f801f20  vldraw  x=-224 y=-128 z=off
        37c4: 00002000  vldraw  x=+0 y=+0 z=stat
        37c8: 1f401fe0  vldraw  x=-32 y=-192 z=off
        37cc: 00002000  vldraw  x=+0 y=+0 z=stat
        37d0: 1ec00080  vldraw  x=+128 y=-320 z=off
        37d4: 00002000  vldraw  x=+0 y=+0 z=stat
        37d8: 00000100  vldraw  x=+256 y=+0 z=off
        37dc: 00002000  vldraw  x=+0 y=+0 z=stat
        37e0: 00a000c0  vldraw  x=+192 y=+160 z=off
        37e4: 00002000  vldraw  x=+0 y=+0 z=stat
        37e8: 01400040  vldraw  x=+64 y=+320 z=off
        37ec: 00002000  vldraw  x=+0 y=+0 z=stat
        37f0: 01101f30  vldraw  x=-208 y=+272 z=off
        37f4: 00002000  vldraw  x=+0 y=+0 z=stat
        37f8: 00101ed0  vldraw  x=-304 y=+16 z=off
        37fc: 00002000  vldraw  x=+0 y=+0 z=stat
        3800: 1f401ef0  vldraw  x=-272 y=-192 z=off
        3804: 00002000  vldraw  x=+0 y=+0 z=stat
        3808: 1f001ff0  vldraw  x=-16 y=-256 z=off
        380c: 00002000  vldraw  x=+0 y=+0 z=stat
        3810: 1f000020  vldraw  x=+32 y=-256 z=off
        3814: 00002000  vldraw  x=+0 y=+0 z=stat
        3818: 1f4000e0  vldraw  x=+224 y=-192 z=off
        381c: 00002000  vldraw  x=+0 y=+0 z=stat
        3820: 000001a0  vldraw  x=+416 y=+0 z=off
        3824: 00002000  vldraw  x=+0 y=+0 z=stat
        3828: c000      vrts
; Cloud of dots (starfield between levels?)
        382a: 1fc01fe0  vldraw  x=-32 y=-64 z=off
        382e: 00002000  vldraw  x=+0 y=+0 z=stat
        3832: 000000a0  vldraw  x=+160 y=+0 z=off
        3836: 00002000  vldraw  x=+0 y=+0 z=stat
        383a: 00a00000  vldraw  x=+0 y=+160 z=off
        383e: 00002000  vldraw  x=+0 y=+0 z=stat
        3842: 00201f20  vldraw  x=-224 y=+32 z=off
        3846: 00002000  vldraw  x=+0 y=+0 z=stat
        384a: 1f401fa0  vldraw  x=-96 y=-192 z=off
        384e: 00002000  vldraw  x=+0 y=+0 z=stat
        3852: 1f600100  vldraw  x=+256 y=-160 z=off
        3856: 00002000  vldraw  x=+0 y=+0 z=stat
        385a: 00e000c0  vldraw  x=+192 y=+224 z=off
        385e: 00002000  vldraw  x=+0 y=+0 z=stat
        3862: 00e01f80  vldraw  x=-128 y=+224 z=off
        3866: 00002000  vldraw  x=+0 y=+0 z=stat
        386a: 00401f00  vldraw  x=-256 y=+64 z=off
        386e: 00002000  vldraw  x=+0 y=+0 z=stat
        3872: 1f201f40  vldraw  x=-192 y=-224 z=off
        3876: 00002000  vldraw  x=+0 y=+0 z=stat
        387a: 1ec00060  vldraw  x=+96 y=-320 z=off
        387e: 00002000  vldraw  x=+0 y=+0 z=stat
        3882: 1fd00140  vldraw  x=+320 y=-48 z=off
        3886: 00002000  vldraw  x=+0 y=+0 z=stat
        388a: 005000a0  vldraw  x=+160 y=+80 z=off
        388e: 00002000  vldraw  x=+0 y=+0 z=stat
        3892: 012000a0  vldraw  x=+160 y=+288 z=off
        3896: 00002000  vldraw  x=+0 y=+0 z=stat
        389a: 01201f00  vldraw  x=-256 y=+288 z=off
        389e: 00002000  vldraw  x=+0 y=+0 z=stat
        38a2: 1fe01e80  vldraw  x=-384 y=-32 z=off
        38a6: 00002000  vldraw  x=+0 y=+0 z=stat
        38aa: 1f201f20  vldraw  x=-224 y=-224 z=off
        38ae: 00002000  vldraw  x=+0 y=+0 z=stat
        38b2: 1f400020  vldraw  x=+32 y=-192 z=off
        38b6: 00002000  vldraw  x=+0 y=+0 z=stat
        38ba: 1e800040  vldraw  x=+64 y=-384 z=off
        38be: 00002000  vldraw  x=+0 y=+0 z=stat
        38c2: 00600260  vldraw  x=+608 y=+96 z=off
        38c6: 00002000  vldraw  x=+0 y=+0 z=stat
        38ca: c000      vrts
; Spiker, picture 1
        38cc: 68c5      vstat   z=12 c=5 sparkle=1
        38ce: 5f21      vsdraw  x=+2 y=-2 z=stat
        38d0: 5f3f      vsdraw  x=-2 y=-2 z=stat
        38d2: 403e      vsdraw  x=-4 y=+0 z=stat
        38d4: 423e      vsdraw  x=-4 y=+4 z=stat
        38d6: 4420      vsdraw  x=+0 y=+8 z=stat
        38d8: 4224      vsdraw  x=+8 y=+4 z=stat
        38da: 5f25      vsdraw  x=+10 y=-2 z=stat
        38dc: 5b23      vsdraw  x=+6 y=-10 z=stat
        38de: 593f      vsdraw  x=-2 y=-14 z=stat
        38e0: 5d39      vsdraw  x=-14 y=-6 z=stat
        38e2: 4238      vsdraw  x=-16 y=+4 z=stat
        38e4: 483c      vsdraw  x=-8 y=+16 z=stat
        38e6: 4923      vsdraw  x=+6 y=+18 z=stat
        38e8: 4529      vsdraw  x=+18 y=+10 z=stat
        38ea: 5d2b      vsdraw  x=+22 y=-6 z=stat
        38ec: 5525      vsdraw  x=+10 y=-22 z=stat
        38ee: 543c      vsdraw  x=-8 y=-24 z=stat
        38f0: 5a34      vsdraw  x=-24 y=-12 z=stat
        38f2: 4432      vsdraw  x=-28 y=+8 z=stat
        38f4: 4e3a      vsdraw  x=-12 y=+28 z=stat
        38f6: 4f25      vsdraw  x=+10 y=+30 z=stat
        38f8: c000      vrts
; Spiker, picture 2
        38fa: 68c5      vstat   z=12 c=5 sparkle=1
        38fc: 4121      vsdraw  x=+2 y=+2 z=stat
        38fe: 5f21      vsdraw  x=+2 y=-2 z=stat
        3900: 5e20      vsdraw  x=+0 y=-4 z=stat
        3902: 5e3e      vsdraw  x=-4 y=-4 z=stat
        3904: 403c      vsdraw  x=-8 y=+0 z=stat
        3906: 443e      vsdraw  x=-4 y=+8 z=stat
        3908: 4521      vsdraw  x=+2 y=+10 z=stat
        390a: 4325      vsdraw  x=+10 y=+6 z=stat
        390c: 5f27      vsdraw  x=+14 y=-2 z=stat
        390e: 5923      vsdraw  x=+6 y=-14 z=stat
        3910: 583e      vsdraw  x=-4 y=-16 z=stat
        3912: 5c38      vsdraw  x=-16 y=-8 z=stat
        3914: 4337      vsdraw  x=-18 y=+6 z=stat
        3916: 493b      vsdraw  x=-10 y=+18 z=stat
        3918: 4b23      vsdraw  x=+6 y=+22 z=stat
        391a: 452b      vsdraw  x=+22 y=+10 z=stat
        391c: 5c2c      vsdraw  x=+24 y=-8 z=stat
        391e: 5426      vsdraw  x=+12 y=-24 z=stat
        3920: 523c      vsdraw  x=-8 y=-28 z=stat
        3922: 5a32      vsdraw  x=-28 y=-12 z=stat
        3924: 4531      vsdraw  x=-30 y=+10 z=stat
        3926: c000      vrts
; Spiker, picture 3
        3928: 68c5      vstat   z=12 c=5 sparkle=1
        392a: 413f      vsdraw  x=-2 y=+2 z=stat
        392c: 4121      vsdraw  x=+2 y=+2 z=stat
        392e: 4022      vsdraw  x=+4 y=+0 z=stat
        3930: 5e22      vsdraw  x=+4 y=-4 z=stat
        3932: 5c20      vsdraw  x=+0 y=-8 z=stat
        3934: 5e3c      vsdraw  x=-8 y=-4 z=stat
        3936: 413b      vsdraw  x=-10 y=+2 z=stat
        3938: 453d      vsdraw  x=-6 y=+10 z=stat
        393a: 4721      vsdraw  x=+2 y=+14 z=stat
        393c: 4327      vsdraw  x=+14 y=+6 z=stat
        393e: 5e28      vsdraw  x=+16 y=-4 z=stat
        3940: 5824      vsdraw  x=+8 y=-16 z=stat
        3942: 573d      vsdraw  x=-6 y=-18 z=stat
        3944: 5b37      vsdraw  x=-18 y=-10 z=stat
        3946: 4335      vsdraw  x=-22 y=+6 z=stat
        3948: 4b3b      vsdraw  x=-10 y=+22 z=stat
        394a: 4c24      vsdraw  x=+8 y=+24 z=stat
        394c: 462c      vsdraw  x=+24 y=+12 z=stat
        394e: 5c2e      vsdraw  x=+28 y=-8 z=stat
        3950: 5226      vsdraw  x=+12 y=-28 z=stat
        3952: 513b      vsdraw  x=-10 y=-30 z=stat
        3954: c000      vrts
; Spiker, picture 4
        3956: 68c5      vstat   z=12 c=5 sparkle=1
        3958: 5f3f      vsdraw  x=-2 y=-2 z=stat
        395a: 413f      vsdraw  x=-2 y=+2 z=stat
        395c: 4220      vsdraw  x=+0 y=+4 z=stat
        395e: 4222      vsdraw  x=+4 y=+4 z=stat
        3960: 4024      vsdraw  x=+8 y=+0 z=stat
        3962: 5c22      vsdraw  x=+4 y=-8 z=stat
        3964: 5b3f      vsdraw  x=-2 y=-10 z=stat
        3966: 5d3b      vsdraw  x=-10 y=-6 z=stat
        3968: 4139      vsdraw  x=-14 y=+2 z=stat
        396a: 473d      vsdraw  x=-6 y=+14 z=stat
        396c: 4822      vsdraw  x=+4 y=+16 z=stat
        396e: 4428      vsdraw  x=+16 y=+8 z=stat
        3970: 5d29      vsdraw  x=+18 y=-6 z=stat
        3972: 5725      vsdraw  x=+10 y=-18 z=stat
        3974: 553d      vsdraw  x=-6 y=-22 z=stat
        3976: 5b35      vsdraw  x=-22 y=-10 z=stat
        3978: 4434      vsdraw  x=-24 y=+8 z=stat
        397a: 4c3a      vsdraw  x=-12 y=+24 z=stat
        397c: 4e24      vsdraw  x=+8 y=+28 z=stat
        397e: 462e      vsdraw  x=+28 y=+12 z=stat
        3980: 5b2f      vsdraw  x=+30 y=-10 z=stat
        3982: c000      vrts
; Tanker holding pulsars
        3984: 68c4      vstat   z=12 c=4 sparkle=1
        3986: 5e1b      vsdraw  x=-10 y=-4 z=off
        3988: 4822      vsdraw  x=+4 y=+16 z=stat
        398a: 5423      vsdraw  x=+6 y=-24 z=stat
        398c: 4c23      vsdraw  x=+6 y=+24 z=stat
        398e: 5822      vsdraw  x=+4 y=-16 z=stat
        3990: 00040036  vldraw  x=+54 y=+4 z=off
        3994: ecda      vjmp    $19b4
; Tanker holding fuzzballs
        3996: 68c7      vstat   z=12 c=7 sparkle=1
        3998: 4034      vsdraw  x=-24 y=+0 z=stat
        399a: 4c0c      vsdraw  x=+24 y=+24 z=off
        399c: 68c3      vstat   z=12 c=3 sparkle=1
        399e: 5420      vsdraw  x=+0 y=-24 z=stat
        39a0: 68c5      vstat   z=12 c=5 sparkle=1
        39a2: 5420      vsdraw  x=+0 y=-24 z=stat
        39a4: 4c00      vsdraw  x=+0 y=+24 z=off
        39a6: 68c1      vstat   z=12 c=1 sparkle=1
        39a8: 402c      vsdraw  x=+24 y=+0 z=stat
        39aa: 00000028  vldraw  x=+40 y=+0 z=off
        39ae: ecda      vjmp    $19b4
; Tanker holding flippers
        39b0: 00000040  vldraw  x=+64 y=+0 z=off
; Common code for all flavors of tanker
        39b4: 68c2      vstat   z=12 c=2 sparkle=1
        39b6: 00403fc0  vldraw  x=-64 y=+64 z=stat
        39ba: 1fd82000  vldraw  x=+0 y=-40 z=stat
        39be: 1fe82040  vldraw  x=+64 y=-24 z=stat
        39c2: 00003fd8  vldraw  x=-40 y=+0 z=stat
        39c6: 4c34      vsdraw  x=-24 y=+24 z=stat
        39c8: 5434      vsdraw  x=-24 y=-24 z=stat
        39ca: 00402018  vldraw  x=+24 y=+64 z=stat
        39ce: 1fc03fc0  vldraw  x=-64 y=-64 z=stat
        39d2: 00002028  vldraw  x=+40 y=+0 z=stat
        39d6: 542c      vsdraw  x=+24 y=-24 z=stat
        39d8: 00183fc0  vldraw  x=-64 y=+24 z=stat
        39dc: 1fc02040  vldraw  x=+64 y=-64 z=stat
        39e0: 00282000  vldraw  x=+0 y=+40 z=stat
        39e4: 4c2c      vsdraw  x=+24 y=+24 z=stat
        39e6: 1fc03fe8  vldraw  x=-24 y=-64 z=stat
        39ea: 00402040  vldraw  x=+64 y=+64 z=stat
        39ee: 00003fd8  vldraw  x=-40 y=+0 z=stat
        39f2: c000      vrts
; Four dots, orthogonal
        39f4: 68c1      vstat   z=12 c=1 sparkle=1
        39f6: 00000020  vldraw  x=+32 y=+0 z=off
        39fa: 0000e000  vldraw  x=+0 y=+0 z=14
        39fe: 00001fc0  vldraw  x=-64 y=+0 z=off
        3a02: 0000e000  vldraw  x=+0 y=+0 z=14
        3a06: 00200020  vldraw  x=+32 y=+32 z=off
        3a0a: 0000e000  vldraw  x=+0 y=+0 z=14
        3a0e: 1fc00000  vldraw  x=+0 y=-64 z=off
        3a12: 0000e000  vldraw  x=+0 y=+0 z=14
        3a16: 7100      vscale  b=1 l=0
        3a18: c000      vrts
; Four dots, diagonal
        3a1a: 68c1      vstat   z=12 c=1 sparkle=1
        3a1c: 00200020  vldraw  x=+32 y=+32 z=off
        3a20: 0000e000  vldraw  x=+0 y=+0 z=14
        3a24: 1fc00000  vldraw  x=+0 y=-64 z=off
        3a28: 0000e000  vldraw  x=+0 y=+0 z=14
        3a2c: 00401fc0  vldraw  x=-64 y=+64 z=off
        3a30: 0000e000  vldraw  x=+0 y=+0 z=14
        3a34: 1fc00000  vldraw  x=+0 y=-64 z=off
        3a38: 0000e000  vldraw  x=+0 y=+0 z=14
        3a3c: 7100      vscale  b=1 l=0
        3a3e: c000      vrts
; Enemy shot, picture 1
        3a40: 68c0      vstat   z=12 c=0 sparkle=1
        3a42: 000b1ff5  vldraw  x=-11 y=+11 z=off
        3a46: 43dd      vsdraw  x=-6 y=+6 z=12
        3a48: 5200      vsdraw  x=+0 y=-28 z=off
        3a4a: 5dc3      vsdraw  x=+6 y=-6 z=12
        3a4c: 400e      vsdraw  x=+28 y=+0 z=off
        3a4e: 43dd      vsdraw  x=-6 y=+6 z=12
        3a50: 4e00      vsdraw  x=+0 y=+28 z=off
        3a52: 5dc3      vsdraw  x=+6 y=-6 z=12
        3a54: 1ffb1ff5  vldraw  x=-11 y=-5 z=off
        3a58: 68c3      vstat   z=12 c=3 sparkle=1
        3a5a: 0000c000  vldraw  x=+0 y=+0 z=12
        3a5e: 401a      vsdraw  x=-12 y=+0 z=off
        3a60: 0000c000  vldraw  x=+0 y=+0 z=12
        3a64: 5a00      vsdraw  x=+0 y=-12 z=off
        3a66: 0000c000  vldraw  x=+0 y=+0 z=12
        3a6a: 4006      vsdraw  x=+12 y=+0 z=off
        3a6c: 0000c000  vldraw  x=+0 y=+0 z=12
        3a70: c000      vrts
; Enemy shot, picture 2
        3a72: 68c0      vstat   z=12 c=0 sparkle=1
        3a74: 4917      vsdraw  x=-18 y=+18 z=off
        3a76: 59c0      vsdraw  x=+0 y=-14 z=12
        3a78: 5705      vsdraw  x=+10 y=-18 z=off
        3a7a: 5cc0      vsdraw  x=+0 y=-8 z=12
        3a7c: 450d      vsdraw  x=+26 y=+10 z=off
        3a7e: 44c0      vsdraw  x=+0 y=+8 z=12
        3a80: 491b      vsdraw  x=-10 y=+18 z=off
        3a82: 44c0      vsdraw  x=+0 y=+8 z=12
        3a84: 1ff11ff5  vldraw  x=-11 y=-15 z=off
        3a88: 68c3      vstat   z=12 c=3 sparkle=1
        3a8a: 0000c000  vldraw  x=+0 y=+0 z=12
        3a8e: 5e1e      vsdraw  x=-4 y=-4 z=off
        3a90: 0000c000  vldraw  x=+0 y=+0 z=12
        3a94: 5b05      vsdraw  x=+10 y=-10 z=off
        3a96: 0000c000  vldraw  x=+0 y=+0 z=12
        3a9a: 4502      vsdraw  x=+4 y=+10 z=off
        3a9c: 0000c000  vldraw  x=+0 y=+0 z=12
        3aa0: c000      vrts
; Enemy shot, picture 3
        3aa2: 68c0      vstat   z=12 c=0 sparkle=1
        3aa4: 00031fef  vldraw  x=-17 y=+3 z=off
        3aa8: 5ddd      vsdraw  x=-6 y=-6 z=12
        3aaa: 560a      vsdraw  x=+20 y=-20 z=off
        3aac: 43c3      vsdraw  x=+6 y=+6 z=12
        3aae: 4707      vsdraw  x=+14 y=+14 z=off
        3ab0: 43c3      vsdraw  x=+6 y=+6 z=12
        3ab2: 4a16      vsdraw  x=-20 y=+20 z=off
        3ab4: 40dd      vsdraw  x=-6 y=+0 z=12
        3ab6: 1ff10003  vldraw  x=+3 y=-15 z=off
        3aba: 68c3      vstat   z=12 c=3 sparkle=1
        3abc: 0000c000  vldraw  x=+0 y=+0 z=12
        3ac0: 5c1c      vsdraw  x=-8 y=-8 z=off
        3ac2: 0000c000  vldraw  x=+0 y=+0 z=12
        3ac6: 5c04      vsdraw  x=+8 y=-8 z=off
        3ac8: 0000c000  vldraw  x=+0 y=+0 z=12
        3acc: 4404      vsdraw  x=+8 y=+8 z=off
        3ace: 0000c000  vldraw  x=+0 y=+0 z=12
        3ad2: c000      vrts
; Enemy shot, picture 4
        3ad4: 68c0      vstat   z=12 c=0 sparkle=1
        3ad6: 5c15      vsdraw  x=-22 y=-8 z=off
        3ad8: 40c4      vsdraw  x=+8 y=+0 z=12
        3ada: 5b09      vsdraw  x=+18 y=-10 z=off
        3adc: 40c4      vsdraw  x=+8 y=+0 z=12
        3ade: 4d01      vsdraw  x=+2 y=+26 z=off
        3ae0: 40c4      vsdraw  x=+8 y=+0 z=12
        3ae2: 4513      vsdraw  x=-26 y=+10 z=off
        3ae4: 40dc      vsdraw  x=-8 y=+0 z=12
        3ae6: 1ff10005  vldraw  x=+5 y=-15 z=off
        3aea: 68c3      vstat   z=12 c=3 sparkle=1
        3aec: 0000c000  vldraw  x=+0 y=+0 z=12
        3af0: 5b02      vsdraw  x=+4 y=-10 z=off
        3af2: 0000c000  vldraw  x=+0 y=+0 z=12
        3af6: 4505      vsdraw  x=+10 y=+10 z=off
        3af8: 0000c000  vldraw  x=+0 y=+0 z=12
        3afc: 421e      vsdraw  x=-4 y=+4 z=off
        3afe: 0000c000  vldraw  x=+0 y=+0 z=12
        3b02: c000      vrts
; Shot-hit-player explosion, size 6
        3b04: 7000      vscale  b=0 l=0
        3b06: ad8d      vjsr    $1b1a
; Shot-hit-player explosion, size 5
        3b08: 7040      vscale  b=0 l=64
        3b0a: ad8d      vjsr    $1b1a
; Shot-hit-player explosion, size 4
        3b0c: 7100      vscale  b=1 l=0
        3b0e: ad8d      vjsr    $1b1a
; Shot-hit-player explosion, size 3
        3b10: 7140      vscale  b=1 l=64
        3b12: ad8d      vjsr    $1b1a
; Shot-hit-player explosion, size 2
        3b14: 7200      vscale  b=2 l=0
        3b16: ad8d      vjsr    $1b1a
; Shot-hit-player explosion, size 1
        3b18: 7240      vscale  b=2 l=64
        3b1a: 68c9      vstat   z=12 c=9 sparkle=1
        3b1c: 1ff00030  vldraw  x=+48 y=-16 z=off
        3b20: 0020e040  vldraw  x=+64 y=+32 z=14
        3b24: 0008ffd0  vldraw  x=-48 y=+8 z=14
        3b28: 68cb      vstat   z=12 c=11 sparkle=1
        3b2a: 48e4      vsdraw  x=+8 y=+16 z=14
        3b2c: 41f8      vsdraw  x=-16 y=+2 z=14
        3b2e: 68ca      vstat   z=12 c=10 sparkle=1
        3b30: 4be6      vsdraw  x=+12 y=+22 z=14
        3b32: 1fecffdc  vldraw  x=-36 y=-20 z=14
        3b36: 68c9      vstat   z=12 c=9 sparkle=1
        3b38: 0034e004  vldraw  x=+4 y=+52 z=14
        3b3c: 58f2      vsdraw  x=-28 y=-16 z=14
        3b3e: 68cb      vstat   z=12 c=11 sparkle=1
        3b40: 45f2      vsdraw  x=-28 y=+10 z=14
        3b42: 1fcafffc  vldraw  x=-4 y=-54 z=14
        3b46: 68ca      vstat   z=12 c=10 sparkle=1
        3b48: 000cffbc  vldraw  x=-68 y=+12 z=14
        3b4c: 1fd0e024  vldraw  x=+36 y=-48 z=14
        3b50: 68c9      vstat   z=12 c=9 sparkle=1
        3b52: 58f6      vsdraw  x=-20 y=-16 z=14
        3b54: 5ee6      vsdraw  x=+12 y=-4 z=14
        3b56: 68cb      vstat   z=12 c=11 sparkle=1
        3b58: 53fa      vsdraw  x=-12 y=-26 z=14
        3b5a: 0012e032  vldraw  x=+50 y=+18 z=14
        3b5e: 68c9      vstat   z=12 c=9 sparkle=1
        3b60: 1fd8fffa  vldraw  x=-6 y=-40 z=14
        3b64: 42e4      vsdraw  x=+8 y=+4 z=14
        3b66: 68ca      vstat   z=12 c=10 sparkle=1
        3b68: 54e4      vsdraw  x=+8 y=-24 z=14
        3b6a: 4cec      vsdraw  x=+24 y=+24 z=14
        3b6c: 68cb      vstat   z=12 c=11 sparkle=1
        3b6e: 54ec      vsdraw  x=+24 y=-24 z=14
        3b70: 0028e004  vldraw  x=+4 y=+40 z=14
        3b74: 68ca      vstat   z=12 c=10 sparkle=1
        3b76: 1ff4e020  vldraw  x=+32 y=-12 z=14
        3b7a: 002cffec  vldraw  x=-20 y=+44 z=14
        3b7e: 00101fd0  vldraw  x=-48 y=+16 z=off
        3b82: c000      vrts
; Starfields of various sizes
        3b84: 68c0      vstat   z=12 c=0 sparkle=1
        3b86: 7500      vscale  b=5 l=0
        3b88: eb11      vjmp    $1622
        3b8a: 68c0      vstat   z=12 c=0 sparkle=1
        3b8c: 7460      vscale  b=4 l=96
        3b8e: eb11      vjmp    $1622
        3b90: 68c1      vstat   z=12 c=1 sparkle=1
        3b92: 7440      vscale  b=4 l=64
        3b94: eb11      vjmp    $1622
        3b96: 68c1      vstat   z=12 c=1 sparkle=1
        3b98: 7420      vscale  b=4 l=32
        3b9a: eb11      vjmp    $1622
        3b9c: 68c3      vstat   z=12 c=3 sparkle=1
        3b9e: 7400      vscale  b=4 l=0
        3ba0: eb11      vjmp    $1622
        3ba2: 68c3      vstat   z=12 c=3 sparkle=1
        3ba4: 7360      vscale  b=3 l=96
        3ba6: eb11      vjmp    $1622
        3ba8: 68c7      vstat   z=12 c=7 sparkle=1
        3baa: 7340      vscale  b=3 l=64
        3bac: eb11      vjmp    $1622
; Draw spiked-apart or pulsared-apart player.
; $0ffc is called before the motions between pieces; it contains a vscale
; which is adjusted with time to cause the pieces to move.
        3bae: a7fe      vjsr    $0ffc
        3bb0: 1fc80080  vldraw  x=+128 y=-56 z=off
        3bb4: 7100      vscale  b=1 l=0
        3bb6: 68c1      vstat   z=12 c=1 sparkle=1
        3bb8: 40ea      vsdraw  x=+20 y=+0 z=14
        3bba: 56e0      vsdraw  x=+0 y=-20 z=14
        3bbc: 0030e054  vldraw  x=+84 y=+48 z=14
        3bc0: 0004ffd0  vldraw  x=-48 y=+4 z=14
        3bc4: 5ce4      vsdraw  x=+8 y=-8 z=14
        3bc6: 1fe8ffc0  vldraw  x=-64 y=-24 z=14
        3bca: a7fe      vjsr    $0ffc
        3bcc: 00501fc0  vldraw  x=-64 y=+80 z=off
        3bd0: ad7f      vjsr    $1afe
        3bd2: a7fe      vjsr    $0ffc
        3bd4: 00600008  vldraw  x=+8 y=+96 z=off
        3bd8: 7100      vscale  b=1 l=0
        3bda: 68c1      vstat   z=12 c=1 sparkle=1
        3bdc: 1fe0e040  vldraw  x=+64 y=-32 z=14
        3be0: 0000e028  vldraw  x=+40 y=+0 z=14
        3be4: 0020ff98  vldraw  x=-104 y=+32 z=14
        3be8: a7fe      vjsr    $0ffc
        3bea: 00481ff8  vldraw  x=-8 y=+72 z=off
        3bee: ad7f      vjsr    $1afe
        3bf0: a7fe      vjsr    $0ffc
        3bf2: 00001fb0  vldraw  x=-80 y=+0 z=off
        3bf6: 7100      vscale  b=1 l=0
        3bf8: 68c1      vstat   z=12 c=1 sparkle=1
        3bfa: 1fe0ff90  vldraw  x=-112 y=-32 z=14
        3bfe: 1fe0e034  vldraw  x=+52 y=-32 z=14
        3c02: 48fc      vsdraw  x=-8 y=+16 z=14
        3c04: 40ee      vsdraw  x=+28 y=+0 z=14
        3c06: 0008ffe0  vldraw  x=-32 y=+8 z=14
        3c0a: 0028e048  vldraw  x=+72 y=+40 z=14
        3c0e: a7fe      vjsr    $0ffc
        3c10: 1f801fe0  vldraw  x=-32 y=-128 z=off
        3c14: ad7f      vjsr    $1afe
        3c16: a7fe      vjsr    $0ffc
        3c18: 00201f80  vldraw  x=-128 y=+32 z=off
        3c1c: ad7f      vjsr    $1afe
        3c1e: a7fe      vjsr    $0ffc
        3c20: 1fb00010  vldraw  x=+16 y=-80 z=off
        3c24: 7100      vscale  b=1 l=0
        3c26: 68c1      vstat   z=12 c=1 sparkle=1
        3c28: 58e4      vsdraw  x=+8 y=-16 z=14
        3c2a: 56ec      vsdraw  x=+24 y=-20 z=14
        3c2c: 0004e020  vldraw  x=+32 y=+4 z=14
        3c30: 4efc      vsdraw  x=-8 y=+28 z=14
        3c32: 0004ffe0  vldraw  x=-32 y=+4 z=14
        3c36: 40f4      vsdraw  x=-24 y=+0 z=14
        3c38: a7fe      vjsr    $0ffc
        3c3a: 1fc00070  vldraw  x=+112 y=-64 z=off
        3c3e: ad7f      vjsr    $1afe
        3c40: a7fe      vjsr    $0ffc
        3c42: 1fbc1ff8  vldraw  x=-8 y=-68 z=off
        3c46: 7100      vscale  b=1 l=0
        3c48: 68c1      vstat   z=12 c=1 sparkle=1
        3c4a: 5ae2      vsdraw  x=+4 y=-12 z=14
        3c4c: 1ffcffdc  vldraw  x=-36 y=-4 z=14
        3c50: 1fe4e038  vldraw  x=+56 y=-28 z=14
        3c54: 0018e030  vldraw  x=+48 y=+24 z=14
        3c58: 48e2      vsdraw  x=+4 y=+16 z=14
        3c5a: 42f6      vsdraw  x=-20 y=+4 z=14
        3c5c: 1ff4ffe0  vldraw  x=-32 y=-12 z=14
        3c60: 46f4      vsdraw  x=-24 y=+12 z=14
        3c62: a7fe      vjsr    $0ffc
        3c64: 1fe40098  vldraw  x=+152 y=-28 z=off
        3c68: ed7f      vjmp    $1afe
; fuzzball, picture 1
        3c6a: 68c3      vstat   z=12 c=3 sparkle=1
        3c6c: 46fc      vsdraw  x=-8 y=+12 z=14
        3c6e: 46e5      vsdraw  x=+10 y=+12 z=14
        3c70: 42fa      vsdraw  x=-12 y=+4 z=14
        3c72: 44e6      vsdraw  x=+12 y=+8 z=14
        3c74: 46fe      vsdraw  x=-4 y=+12 z=14
        3c76: 68c1      vstat   z=12 c=1 sparkle=1
        3c78: 5f09      vsdraw  x=+18 y=-2 z=off
        3c7a: 5de2      vsdraw  x=+4 y=-6 z=14
        3c7c: 5ce2      vsdraw  x=+4 y=-8 z=14
        3c7e: 5cfa      vsdraw  x=-12 y=-8 z=14
        3c80: 5ce2      vsdraw  x=+4 y=-8 z=14
        3c82: 58f8      vsdraw  x=-16 y=-16 z=14
        3c84: 68c5      vstat   z=12 c=5 sparkle=1
        3c86: 42ea      vsdraw  x=+20 y=+4 z=14
        3c88: 58fe      vsdraw  x=-4 y=-16 z=14
        3c8a: 40e6      vsdraw  x=+12 y=+0 z=14
        3c8c: 5afa      vsdraw  x=-12 y=-12 z=14
        3c8e: 59e4      vsdraw  x=+8 y=-14 z=14
        3c90: 40e4      vsdraw  x=+8 y=+0 z=14
        3c92: 68c2      vstat   z=12 c=2 sparkle=1
        3c94: 1ff21fd8  vldraw  x=-40 y=-14 z=off
        3c98: 46e0      vsdraw  x=+0 y=+12 z=14
        3c9a: 40fa      vsdraw  x=-12 y=+0 z=14
        3c9c: 47e3      vsdraw  x=+6 y=+14 z=14
        3c9e: 47fe      vsdraw  x=-4 y=+14 z=14
        3ca0: 5ee6      vsdraw  x=+12 y=-4 z=14
        3ca2: 48e3      vsdraw  x=+6 y=+16 z=14
        3ca4: 68c4      vstat   z=12 c=4 sparkle=1
        3ca6: 5ef8      vsdraw  x=-16 y=-4 z=14
        3ca8: 45fe      vsdraw  x=-4 y=+10 z=14
        3caa: 5cfc      vsdraw  x=-8 y=-8 z=14
        3cac: 45fe      vsdraw  x=-4 y=+10 z=14
        3cae: 58f4      vsdraw  x=-24 y=-16 z=14
        3cb0: c000      vrts
; fuzzball, picture 2
        3cb2: 68c3      vstat   z=12 c=3 sparkle=1
        3cb4: 48ff      vsdraw  x=-2 y=+16 z=14
        3cb6: 40fc      vsdraw  x=-8 y=+0 z=14
        3cb8: 42e0      vsdraw  x=+0 y=+4 z=14
        3cba: 5ffb      vsdraw  x=-10 y=-2 z=14
        3cbc: 47e3      vsdraw  x=+6 y=+14 z=14
        3cbe: 40fb      vsdraw  x=-10 y=+0 z=14
        3cc0: 5cfe      vsdraw  x=-4 y=-8 z=14
        3cc2: 68c1      vstat   z=12 c=1 sparkle=1
        3cc4: 00080044  vldraw  x=+68 y=+8 z=off
        3cc8: 42fa      vsdraw  x=-12 y=+4 z=14
        3cca: 5bfb      vsdraw  x=-10 y=-10 z=14
        3ccc: 5ae1      vsdraw  x=+2 y=-12 z=14
        3cce: 41fc      vsdraw  x=-8 y=+2 z=14
        3cd0: 58fa      vsdraw  x=-12 y=-16 z=14
        3cd2: 68c5      vstat   z=12 c=5 sparkle=1
        3cd4: 5fe1      vsdraw  x=+2 y=-2 z=14
        3cd6: 41e8      vsdraw  x=+16 y=+2 z=14
        3cd8: 5be2      vsdraw  x=+4 y=-10 z=14
        3cda: 5fe5      vsdraw  x=+10 y=-2 z=14
        3cdc: 5cfe      vsdraw  x=-4 y=-8 z=14
        3cde: 5fe6      vsdraw  x=+12 y=-2 z=14
        3ce0: 68c2      vstat   z=12 c=2 sparkle=1
        3ce2: 1fea1fc8  vldraw  x=-56 y=-22 z=off
        3ce6: 44e0      vsdraw  x=+0 y=+8 z=14
        3ce8: 46e4      vsdraw  x=+8 y=+12 z=14
        3cea: 40fc      vsdraw  x=-8 y=+0 z=14
        3cec: 46e2      vsdraw  x=+4 y=+12 z=14
        3cee: 46e6      vsdraw  x=+12 y=+12 z=14
        3cf0: 68c4      vstat   z=12 c=4 sparkle=1
        3cf2: 40f8      vsdraw  x=-16 y=+0 z=14
        3cf4: 5cfc      vsdraw  x=-8 y=-8 z=14
        3cf6: 42fc      vsdraw  x=-8 y=+4 z=14
        3cf8: 5cf8      vsdraw  x=-16 y=-8 z=14
        3cfa: c000      vrts
; fuzzball, picture 3
        3cfc: 68c3      vstat   z=12 c=3 sparkle=1
        3cfe: 47e0      vsdraw  x=+0 y=+14 z=14
        3d00: 42e3      vsdraw  x=+6 y=+4 z=14
        3d02: 44fe      vsdraw  x=-4 y=+8 z=14
        3d04: 43e5      vsdraw  x=+10 y=+6 z=14
        3d06: 44fe      vsdraw  x=-4 y=+8 z=14
        3d08: 48e4      vsdraw  x=+8 y=+16 z=14
        3d0a: 68c1      vstat   z=12 c=1 sparkle=1
        3d0c: 1fe40020  vldraw  x=+32 y=-28 z=off
        3d10: 40fa      vsdraw  x=-12 y=+0 z=14
        3d12: 58fe      vsdraw  x=-4 y=-16 z=14
        3d14: 5cfa      vsdraw  x=-12 y=-8 z=14
        3d16: 44fe      vsdraw  x=-4 y=+8 z=14
        3d18: 5af8      vsdraw  x=-16 y=-12 z=14
        3d1a: 68c5      vstat   z=12 c=5 sparkle=1
        3d1c: 5ce4      vsdraw  x=+8 y=-8 z=14
        3d1e: 40e4      vsdraw  x=+8 y=+0 z=14
        3d20: 5ce1      vsdraw  x=+2 y=-8 z=14
        3d22: 5fe7      vsdraw  x=+14 y=-2 z=14
        3d24: 59e1      vsdraw  x=+2 y=-14 z=14
        3d26: 40e7      vsdraw  x=+14 y=+0 z=14
        3d28: 68c2      vstat   z=12 c=2 sparkle=1
        3d2a: 1ff01fb8  vldraw  x=-72 y=-16 z=off
        3d2e: 44e4      vsdraw  x=+8 y=+8 z=14
        3d30: 48fc      vsdraw  x=-8 y=+16 z=14
        3d32: 42e7      vsdraw  x=+14 y=+4 z=14
        3d34: 4ae5      vsdraw  x=+10 y=+20 z=14
        3d36: 68c4      vstat   z=12 c=4 sparkle=1
        3d38: 42fc      vsdraw  x=-8 y=+4 z=14
        3d3a: 5efc      vsdraw  x=-8 y=-4 z=14
        3d3c: 42fe      vsdraw  x=-4 y=+4 z=14
        3d3e: 5ef8      vsdraw  x=-16 y=-4 z=14
        3d40: 5afc      vsdraw  x=-8 y=-12 z=14
        3d42: c000      vrts
; fuzzball, picture 4
        3d44: 68c3      vstat   z=12 c=3 sparkle=1
        3d46: 44fc      vsdraw  x=-8 y=+8 z=14
        3d48: 46e1      vsdraw  x=+2 y=+12 z=14
        3d4a: 44fd      vsdraw  x=-6 y=+8 z=14
        3d4c: 40fa      vsdraw  x=-12 y=+0 z=14
        3d4e: 44e0      vsdraw  x=+0 y=+8 z=14
        3d50: 68c1      vstat   z=12 c=1 sparkle=1
        3d52: 1ffc0038  vldraw  x=+56 y=-4 z=off
        3d56: 5efa      vsdraw  x=-12 y=-4 z=14
        3d58: 5de3      vsdraw  x=+6 y=-6 z=14
        3d5a: 5dfb      vsdraw  x=-10 y=-6 z=14
        3d5c: 5ce2      vsdraw  x=+4 y=-8 z=14
        3d5e: 5cf6      vsdraw  x=-20 y=-8 z=14
        3d60: 68c5      vstat   z=12 c=5 sparkle=1
        3d62: 5de8      vsdraw  x=+16 y=-6 z=14
        3d64: 5ce1      vsdraw  x=+2 y=-8 z=14
        3d66: 43e5      vsdraw  x=+10 y=+6 z=14
        3d68: 40e4      vsdraw  x=+8 y=+0 z=14
        3d6a: 56e2      vsdraw  x=+4 y=-20 z=14
        3d6c: 68c2      vstat   z=12 c=2 sparkle=1
        3d6e: 1fec1fd8  vldraw  x=-40 y=-20 z=off
        3d72: 44fc      vsdraw  x=-8 y=+8 z=14
        3d74: 44e4      vsdraw  x=+8 y=+8 z=14
        3d76: 44fc      vsdraw  x=-8 y=+8 z=14
        3d78: 44e6      vsdraw  x=+12 y=+8 z=14
        3d7a: 48fe      vsdraw  x=-4 y=+16 z=14
        3d7c: 68c4      vstat   z=12 c=4 sparkle=1
        3d7e: 5cf7      vsdraw  x=-18 y=-8 z=14
        3d80: 43ff      vsdraw  x=-2 y=+6 z=14
        3d82: 40fc      vsdraw  x=-8 y=+0 z=14
        3d84: 5aff      vsdraw  x=-2 y=-12 z=14
        3d86: 5efa      vsdraw  x=-12 y=-4 z=14
        3d88: c000      vrts
        3d8a: 68c0      vstat   z=12 c=0 sparkle=1
        3d8c: 7120      vscale  b=1 l=32
        3d8e: 00001fdc  vldraw  x=-36 y=+0 z=off
        3d92: a8de      vjsr    $11bc ; 7
        3d94: eed7      vjmp    $1dae ; "50"
        3d96: 68c0      vstat   z=12 c=0 sparkle=1
        3d98: 7120      vscale  b=1 l=32
        3d9a: 00001fdc  vldraw  x=-36 y=+0 z=off
        3d9e: a8d0      vjsr    $11a0 ; 5
        3da0: a865      vjsr    $10ca ; 0
        3da2: eed8      vjmp    $1db0 ; 0
        3da4: 68c0      vstat   z=12 c=0 sparkle=1
        3da6: 7120      vscale  b=1 l=32
        3da8: 00001fdc  vldraw  x=-36 y=+0 z=off
        3dac: a8ba      vjsr    $1174 ; 2
        3dae: a8d0      vjsr    $11a0 ; 5
        3db0: e865      vjmp    $10ca ; 0
; Not sure what these are.
        3db2: ad7f      vjsr    $1afe
        3db4: a002      vjsr    $0004
        3db6: a205      vjsr    $040a
        3db8: a52a      vjsr    $0a54
        3dba: a6de      vjsr    $0dbc
        3dbc: a348      vjsr    $0690
        3dbe: a47f      vjsr    $08fe
        3dc0: a66b      vjsr    $0cd6
        3dc2: a100      vjsr    $0200
        3dc4: a711      vjsr    $0e22
        3dc6: e000      vjmp    $0000 (halt)
        3dc8: a002      vjsr    $0004
        3dca: e000      vjmp    $0000 (halt)
        3dcc: 2000      vhalt
; Draw selftest display, fixed parts - the display that's shown when
; the service switch is turned on live.
        3dce: aa53      vjsr    $14a6
        3dd0: 8040      vcentre
        3dd2: 7100      vscale  b=1 l=0
        3dd4: 68c0      vstat   z=12 c=0 sparkle=1
        3dd6: 012c1f88  vldraw  x=-120 y=+300 z=off
        3dda: af21      vjsr    $1e42 ; "SECONDS "
        3ddc: a865      vjsr    $10ca ; O
        3dde: a860      vjsr    $10c0 ; N
        3de0: 1fe21f10  vldraw  x=-240 y=-30 z=off
        3de4: af21      vjsr    $1e42 ; "SECONDS "
        3de6: a86b      vjsr    $10d6 ; P
        3de8: a855      vjsr    $10aa ; L
        3dea: a800      vjsr    $1000 ; A
        3dec: a8a7      vjsr    $114e ; Y
        3dee: a823      vjsr    $1046 ; E
        3df0: a81b      vjsr    $1036 ; D
        3df2: 1fe21eb0  vldraw  x=-336 y=-30 z=off
        3df6: a8b6      vjsr    $116c ; 1
        3df8: af2a      vjsr    $1e54 ; " PLAYER GAMES"
        3dfa: 1fe21eb0  vldraw  x=-336 y=-30 z=off
        3dfe: a8ba      vjsr    $1174 ; 2
        3e00: af2a      vjsr    $1e54 ; " PLAYER GAMES"
        3e02: 1fe21eb0  vldraw  x=-336 y=-30 z=off
        3e06: af21      vjsr    $1e42 ; "SECONDS "
        3e08: a800      vjsr    $1000 ; A
        3e0a: a896      vjsr    $112c ; V
        3e0c: a823      vjsr    $1046 ; E
        3e0e: a87b      vjsr    $10f6 ; R
        3e10: a800      vjsr    $1000 ; A
        3e12: a832      vjsr    $1064 ; G
        3e14: a823      vjsr    $1046 ; E
        3e16: 1fb81e98  vldraw  x=-360 y=-72 z=off
        3e1a: 68c4      vstat   z=12 c=4 sparkle=1
        3e1c: a8a2      vjsr    $1144 ; X
        3e1e: a8b6      vjsr    $116c ; 1
        3e20: 1fe01fd0  vldraw  x=-48 y=-32 z=off
        3e24: a808      vjsr    $1010 ; B
        3e26: a865      vjsr    $10ca ; O
        3e28: a860      vjsr    $10c0 ; N
        3e2a: a890      vjsr    $1120 ; U
        3e2c: a883      vjsr    $1106 ; S
        3e2e: a8b4      vjsr    $1168 ; space
        3e30: a800      vjsr    $1000 ; A
        3e32: a81b      vjsr    $1036 ; D
        3e34: a81b      vjsr    $1036 ; D
        3e36: a823      vjsr    $1046 ; E
        3e38: a87b      vjsr    $10f6 ; R
        3e3a: 68c0      vstat   z=12 c=0 sparkle=1
        3e3c: 00e01e10  vldraw  x=-496 y=+224 z=off
        3e40: c000      vrts
; Subroutines used by $3dce
        3e42: a883      vjsr    $1106 ; S
        3e44: a823      vjsr    $1046 ; E
        3e46: a815      vjsr    $102a ; C
        3e48: a865      vjsr    $10ca ; O
        3e4a: a860      vjsr    $10c0 ; N
        3e4c: a81b      vjsr    $1036 ; D
        3e4e: a883      vjsr    $1106 ; S
        3e50: a8b4      vjsr    $1168 ; space
        3e52: c000      vrts
        3e54: a8b4      vjsr    $1168 ; space
        3e56: a86b      vjsr    $10d6 ; P
        3e58: a855      vjsr    $10aa ; L
        3e5a: a800      vjsr    $1000 ; A
        3e5c: a8a7      vjsr    $114e ; Y
        3e5e: a823      vjsr    $1046 ; E
        3e60: a87b      vjsr    $10f6 ; R
        3e62: a8b4      vjsr    $1168 ; space
        3e64: a832      vjsr    $1064 ; G
        3e66: a800      vjsr    $1000 ; A
        3e68: a85a      vjsr    $10b4 ; M
        3e6a: a823      vjsr    $1046 ; E
        3e6c: a883      vjsr    $1106 ; S
        3e6e: c000      vrts
; Routines to display the "press fire and <foo> for <bar>" strings.
        3e70: 8040      vcentre
        3e72: 68c3      vstat   z=12 c=3 sparkle=1
        3e74: 017c1e64  vldraw  x=-412 y=+380 z=off
        3e78: a86b      vjsr    $10d6 ; P
        3e7a: a87b      vjsr    $10f6 ; R
        3e7c: a823      vjsr    $1046 ; E
        3e7e: a883      vjsr    $1106 ; S
        3e80: a883      vjsr    $1106 ; S
        3e82: a8b4      vjsr    $1168 ; space
        3e84: a82b      vjsr    $1056 ; F
        3e86: a842      vjsr    $1084 ; I
        3e88: a87b      vjsr    $10f6 ; R
        3e8a: a823      vjsr    $1046 ; E
        3e8c: a8b4      vjsr    $1168 ; space
        3e8e: a800      vjsr    $1000 ; A
        3e90: a860      vjsr    $10c0 ; N
        3e92: a81b      vjsr    $1036 ; D
        3e94: a8b4      vjsr    $1168 ; space
        3e96: c000      vrts
        3e98: a8b4      vjsr    $1168 ; space
        3e9a: a88a      vjsr    $1114 ; T
        3e9c: a865      vjsr    $10ca ; O
        3e9e: a8b4      vjsr    $1168 ; space
        3ea0: a8ae      vjsr    $115c ; Z
        3ea2: a823      vjsr    $1046 ; E
        3ea4: a87b      vjsr    $10f6 ; R
        3ea6: a865      vjsr    $10ca ; O
        3ea8: a8b4      vjsr    $1168 ; space
        3eaa: c000      vrts
        3eac: af38      vjsr    $1e70 ; "PRESS FIRE AND "
        3eae: a8ae      vjsr    $115c ; Z
        3eb0: a800      vjsr    $1000 ; A
        3eb2: a86b      vjsr    $10d6 ; P
        3eb4: a8b4      vjsr    $1168 ; space
        3eb6: a82b      vjsr    $1056 ; F
        3eb8: a865      vjsr    $10ca ; O
        3eba: a87b      vjsr    $10f6 ; R
        3ebc: a8b4      vjsr    $1168 ; space
        3ebe: a883      vjsr    $1106 ; S
        3ec0: a823      vjsr    $1046 ; E
        3ec2: a855      vjsr    $10aa ; L
        3ec4: a82b      vjsr    $1056 ; F
        3ec6: a8b4      vjsr    $1168 ; space
        3ec8: a88a      vjsr    $1114 ; T
        3eca: a823      vjsr    $1046 ; E
        3ecc: a883      vjsr    $1106 ; S
        3ece: a88a      vjsr    $1114 ; T
        3ed0: 68c4      vstat   z=12 c=4 sparkle=1
        3ed2: c000      vrts
        3ed4: af38      vjsr    $1e70 ; "PRESS FIRE AND "
        3ed6: a883      vjsr    $1106 ; S
        3ed8: a88a      vjsr    $1114 ; T
        3eda: a800      vjsr    $1000 ; A
        3edc: a87b      vjsr    $10f6 ; R
        3ede: a88a      vjsr    $1114 ; T
        3ee0: a8b4      vjsr    $1168 ; space
        3ee2: a8b6      vjsr    $116c ; 1
        3ee4: af4c      vjsr    $1e98 ; " TO ZERO "
        3ee6: a88a      vjsr    $1114 ; T
        3ee8: a842      vjsr    $1084 ; I
        3eea: a85a      vjsr    $10b4 ; M
        3eec: a823      vjsr    $1046 ; E
        3eee: a883      vjsr    $1106 ; S
        3ef0: 68c4      vstat   z=12 c=4 sparkle=1
        3ef2: c000      vrts
        3ef4: af38      vjsr    $1e70 ; "PRESS FIRE AND "
        3ef6: a883      vjsr    $1106 ; S
        3ef8: a88a      vjsr    $1114 ; T
        3efa: a800      vjsr    $1000 ; A
        3efc: a87b      vjsr    $10f6 ; R
        3efe: a88a      vjsr    $1114 ; T
        3f00: a8b4      vjsr    $1168 ; space
        3f02: a8ba      vjsr    $1174 ; 2
        3f04: af4c      vjsr    $1e98 ; " TO ZERO "
        3f06: a883      vjsr    $1106 ; S
        3f08: a815      vjsr    $102a ; C
        3f0a: a865      vjsr    $10ca ; O
        3f0c: a87b      vjsr    $10f6 ; R
        3f0e: a823      vjsr    $1046 ; E
        3f10: a883      vjsr    $1106 ; S
        3f12: 68c4      vstat   z=12 c=4 sparkle=1
        3f14: c000      vrts
test_magic_tbl: .word   3eac ; "PRESS FIRE AND ZAP FOR SELF TEST"
        3f18: .word   3eac ; "PRESS FIRE AND ZAP FOR SELF TEST"
        3f1a: .word   3ed4 ; "PRESS FIRE AND START 1 TO ZERO TIMES"
        3f1c: .word   3ef4 ; "PRESS FIRE AND START 2 TO ZERO SCORES
diff_str_tbl: .word   3f32 ; MEDIUM
        3f20: .word   3f26 ; EASY
        3f22: .word   3f42 ; HARD
        3f24: .word   3f32 ; MEDIUM
        3f26: 1fe00000  vldraw  x=+0 y=-32 z=off
        3f2a: a823      vjsr    $1046 ; E
        3f2c: a800      vjsr    $1000 ; A
        3f2e: a883      vjsr    $1106 ; S
        3f30: e8a7      vjmp    $114e ; Y
        3f32: 1fe00000  vldraw  x=+0 y=-32 z=off
        3f36: a85a      vjsr    $10b4 ; M
        3f38: a823      vjsr    $1046 ; E
        3f3a: a81b      vjsr    $1036 ; D
        3f3c: a842      vjsr    $1084 ; I
        3f3e: a890      vjsr    $1120 ; U
        3f40: e85a      vjmp    $10b4 ; M
        3f42: 1fe00000  vldraw  x=+0 y=-32 z=off
        3f46: a83b      vjsr    $1076 ; H
        3f48: a800      vjsr    $1000 ; A
        3f4a: a87b      vjsr    $10f6 ; R
        3f4c: e81b      vjmp    $1036 ; D
        3f4e: 8040      vcentre
; Draw "TEMPEST" logo
        3f50: 01001e50  vldraw  x=-432 y=+256 z=off
        3f54: afbd      vjsr    $1f7a
        3f56: 00000060  vldraw  x=+96 y=+0 z=off
        3f5a: afc4      vjsr    $1f88
        3f5c: 00000024  vldraw  x=+36 y=+0 z=off
        3f60: afd1      vjsr    $1fa2
        3f62: 00000034  vldraw  x=+52 y=+0 z=off
        3f66: afe0      vjsr    $1fc0
        3f68: 004800f8  vldraw  x=+248 y=+72 z=off
        3f6c: afc4      vjsr    $1f88
        3f6e: 00280016  vldraw  x=+22 y=+40 z=off
        3f72: afea      vjsr    $1fd4
        3f74: 1fa00060  vldraw  x=+96 y=-96 z=off
        3f78: efbd      vjmp    $1f7a
; Subroutines called by "TEMPEST" logo, one per letter (T and E used twice)
        3f7a: 0080c000  vldraw  x=+0 y=+128 z=12 ; T
        3f7e: 00001fb0  vldraw  x=-80 y=+0 z=off
        3f82: 0000c0a0  vldraw  x=+160 y=+0 z=12
        3f86: c000      vrts
        3f88: 0000dfb0  vldraw  x=-80 y=+0 z=12 ; E
        3f8c: 1fc0dfec  vldraw  x=-20 y=-64 z=12
        3f90: 0000c070  vldraw  x=+112 y=+0 z=12
        3f94: 00001f90  vldraw  x=-112 y=+0 z=off
        3f98: 1fc0dfec  vldraw  x=-20 y=-64 z=12
        3f9c: 0000c084  vldraw  x=+132 y=+0 z=12
        3fa0: c000      vrts
        3fa2: 0000dfe0  vldraw  x=-32 y=+0 z=12 ; M
        3fa6: 0080c030  vldraw  x=+48 y=+128 z=12
        3faa: 40c8      vsdraw  x=+16 y=+0 z=12
        3fac: 1fa8c020  vldraw  x=+32 y=-88 z=12
        3fb0: 0058c020  vldraw  x=+32 y=+88 z=12
        3fb4: 40c8      vsdraw  x=+16 y=+0 z=12
        3fb6: 1f80c030  vldraw  x=+48 y=-128 z=12
        3fba: 0000dfe0  vldraw  x=-32 y=+0 z=12
        3fbe: c000      vrts
        3fc0: 40d8      vsdraw  x=-16 y=+0 z=12 ; P
        3fc2: 0080c000  vldraw  x=+0 y=+128 z=12
        3fc6: 0000c05c  vldraw  x=+92 y=+0 z=12
        3fca: 1fb8c01a  vldraw  x=+26 y=-72 z=12
        3fce: 0000df8a  vldraw  x=-118 y=+0 z=12
        3fd2: c000      vrts
        3fd4: 1fd8dff0  vldraw  x=-16 y=-40 z=12 ; S
        3fd8: 0000c090  vldraw  x=+144 y=+0 z=12
        3fdc: 0038c000  vldraw  x=+0 y=+56 z=12
        3fe0: 0020df90  vldraw  x=-112 y=+32 z=12
        3fe4: 0028c010  vldraw  x=+16 y=+40 z=12
        3fe8: 0000c064  vldraw  x=+100 y=+0 z=12
        3fec: 1fe0dff4  vldraw  x=-12 y=-32 z=12
        3ff0: c000      vrts
; Move to the upper right and lower left extreme corners of the screen.
; It's not clear what this is for; I speculate it's to prevent the hardware
; that deals with broken driver transistors from kicking in when certain
; displays don't (otherwise) drive the beam each side of each axis.
        3ff2: 8040      vcentre
        3ff4: 7100      vscale  b=1 l=0
        3ff6: 021c01f4  vldraw  x=+500 y=+540 z=off
        3ffa: 1bc81c18  vldraw  x=-1000 y=-1080 z=off
        3ffe: c069      vrts
   vid_coins: .space  2048
       vg_go: .space  2048
    watchdog: .space  2048
    vg_reset: .space  2048
 earom_write: .space  64
  eactl_mbst: .space  16
    earom_rd: .space  16
     mb_rd_l: .space  16
     mb_rd_h: .space  16
     mb_w_00: .chunk  32
        60a0: .space  32
      pokey1: .byte   00
        60c1: .byte   00
        60c2: .byte   00
        60c3: .byte   00
        60c4: .byte   00
        60c5: .byte   00
        60c6: .byte   00
        60c7: .byte   00
spinner_cabtyp: .byte   00
        60c9: .byte   00
 pokey1_rand: .byte   00
        60cb: .byte   00
        60cc: .byte   00
        60cd: .byte   00
        60ce: .byte   00
        60cf: .byte   00
      pokey2: .byte   00
        60d1: .byte   00
        60d2: .byte   00
        60d3: .byte   00
        60d4: .byte   00
        60d5: .byte   00
        60d6: .byte   00
        60d7: .byte   00
; $03 = Difficulty
;       $00 = Medium
;       $01 = Easy
;       $02 = Hard
;       $03 = Medium
; $04 = Rating
;       $00 = 1, 3, 5, 7, 9
;       $04 = tied to high score
; $08 = zap button
; $10 = fire button
; $20 = 1p start button
; $40 = 2p start button
; $80 = unknown (nothing?)
zap_fire_starts: .byte   00
        60d9: .byte   00
 pokey2_rand: .byte   00
        60db: .byte   00
        60dc: .byte   00
        60dd: .byte   00
        60de: .byte   00
        60df: .byte   00
   leds_flip: .space  12064
; Don't (yet) know what this data is.
        9000: .byte   02
        9001: .byte   bb
        9002: .byte   5a
        9003: .byte   30
        9004: .byte   50
        9005: .byte   ee
        9006: .byte   3d
        9007: .byte   a8
        9008: .byte   4d
        9009: 20 c5 92  jsr     $92c5
        900c: 20 34 92  jsr     set_enm_and_spikes
        900f: 20 2b 90  jsr     $902b
        9012: 20 31 a8  jsr     reset_sz
        9015: a9 fa     lda     #$fa
        9017: 85 5b     sta     $5b
        9019: a9 00     lda     #$00
        901b: 8d 06 01  sta     $0106
        901e: 85 5f     sta     $5f
        9020: a9 00     lda     #$00
        9022: 85 01     sta     $01
        9024: 60        rts
        9025: 20 1b 92  jsr     $921b
        9028: 20 c5 92  jsr     $92c5
        902b: 20 8f 92  jsr     clear_shots
        902e: 20 6f 92  jsr     $926f
        9031: 20 46 92  jsr     $9246
        9034: 20 9f 92  jsr     $929f
        9037: 20 ad 92  jsr     $92ad
        903a: 20 6e c1  jsr     $c16e
        903d: a9 ff     lda     #$ff
        903f: 8d 24 01  sta     $0124
        9042: 8d 48 01  sta     pulsing
        9045: a9 00     lda     #$00
        9047: 8d 23 01  sta     $0123
        904a: 60        rts
    state_18: a9 10     lda     #$10
        904d: 8d 02 02  sta     player_along
        9050: a9 00     lda     #$00
        9052: 85 29     sta     $29
        9054: 85 2b     sta     $2b
        9056: ad 21 01  lda     $0121
        9059: 85 2a     sta     $2a
        905b: 10 02     bpl     $905f
        905d: c6 2b     dec     $2b
        905f: a2 01     ldx     #$01
        9061: a5 2a     lda     $2a
        9063: 0a        asl     a
        9064: 66 2a     ror     $2a
        9066: 66 29     ror     $29
        9068: ca        dex
        9069: 10 f6     bpl     $9061
        906b: a5 29     lda     $29
        906d: 18        clc
        906e: 6d 22 01  adc     $0122
        9071: 8d 22 01  sta     $0122
        9074: a5 2a     lda     $2a
        9076: 65 68     adc     $68
        9078: 85 68     sta     $68
        907a: a5 2b     lda     $2b
        907c: 65 69     adc     $69
        907e: 85 69     sta     $69
        9080: a5 5f     lda     $5f
        9082: 18        clc
        9083: 69 18     adc     #$18
        9085: 85 5f     sta     $5f
        9087: a5 5b     lda     $5b
        9089: 69 00     adc     #$00
        908b: 85 5b     sta     $5b
        908d: c9 fc     cmp     #$fc
        908f: 90 05     bcc     $9096
        9091: a9 01     lda     #$01
        9093: 8d 15 01  sta     $0115
        9096: a5 5f     lda     $5f
        9098: 38        sec
        9099: e5 5d     sbc     $5d
        909b: a5 5b     lda     $5b
        909d: f0 02     beq     $90a1
        909f: e9 ff     sbc     #$ff
        90a1: d0 19     bne     $90bc
        90a3: a5 5d     lda     $5d
        90a5: 85 5f     sta     $5f
        90a7: a9 ff     lda     #$ff
        90a9: 85 5b     sta     $5b
        90ab: a9 04     lda     #$04
        90ad: 24 05     bit     $05
        90af: 30 02     bmi     $90b3
        90b1: a9 08     lda     #$08
        90b3: 85 00     sta     gamestate
        90b5: a6 3d     ldx     curplayer
        90b7: a9 00     lda     #$00
        90b9: 9d 02 01  sta     p1_startchoice,x
        90bc: a9 ff     lda     #$ff
        90be: 8d 14 01  sta     $0114
        90c1: 4c 49 97  jmp     move_player
; Existing disassembled code says of this location
; "LEVEL SELECTION CODE BEGINS HERE"
; I haven't (yet) checked this claim out.
        90c4: ad 26 01  lda     $0126 ; last game max level completed?
        90c7: a2 1c     ldx     #$1c
        90c9: ca        dex
        90ca: dd fe 91  cmp     startlevtbl,x
        90cd: 90 fa     bcc     $90c9
        90cf: a0 04     ldy     #$04
        90d1: ad 6a 01  lda     diff_bits
        90d4: 29 04     and     #$04 ; rating bit
        90d6: f0 12     beq     $90ea ; branch if 1,3,5,7,9
        90d8: ad 1d 07  lda     hs_score_1+2 ; top two digits of top score
        90db: c9 30     cmp     #$30
        90dd: 90 01     bcc     $90e0
        90df: c8        iny
        90e0: c9 50     cmp     #$50
        90e2: 90 01     bcc     $90e5
        90e4: c8        iny
        90e5: c9 70     cmp     #$70
        90e7: 90 01     bcc     $90ea
        90e9: c8        iny
        90ea: a5 09     lda     coinage_shadow
        90ec: 29 43     and     #$43 ; coinage + 1 bit of bonus coins
        90ee: c9 40     cmp     #$40 ; free play + 1/4,2/4,demo
        90f0: d0 02     bne     $90f4
        90f2: a0 1b     ldy     #$1b
        90f4: 84 29     sty     $29
        90f6: e4 29     cpx     $29
        90f8: b0 02     bcs     $90fc
        90fa: a6 29     ldx     $29
        90fc: 8e 27 01  stx     $0127
        90ff: a5 05     lda     $05
        9101: 10 05     bpl     state_1c
        9103: a9 00     lda     #$00
        9105: 8d 26 01  sta     $0126
    state_1c: a6 3f     ldx     $3f
        910a: 86 3d     stx     curplayer
        910c: f0 03     beq     $9111
        910e: 20 b2 92  jsr     $92b2
        9111: a9 04     lda     #$04
        9113: 85 7c     sta     $7c
        9115: a9 ff     lda     #$ff
        9117: 85 5b     sta     $5b
        9119: a9 00     lda     #$00
        911b: 8d 00 02  sta     player_seg
        911e: 85 51     sta     $51
        9120: 85 7b     sta     $7b
        9122: 8d 05 06  sta     hs_timer
        9125: a6 05     ldx     $05
        9127: 10 1b     bpl     $9144
        9129: a9 14     lda     #$14
        912b: 8d 05 06  sta     hs_timer
        912e: a9 ff     lda     #$ff
        9130: 8d 11 01  sta     open_level
        9133: a9 16     lda     #$16
        9135: 85 00     sta     gamestate
        9137: a9 08     lda     #$08
        9139: 85 01     sta     $01
        913b: a9 00     lda     #$00
        913d: 85 9f     sta     curlevel
        913f: 20 96 c1  jsr     setcolours
        9142: a9 10     lda     #$10 ; seconds to choose start level?
        9144: 85 04     sta     $04
        9146: 20 ad 92  jsr     $92ad
    state_16: ce 05 06  dec     hs_timer
        914c: 10 1b     bpl     $9169
        914e: f8        sed
        914f: a5 04     lda     $04
        9151: 38        sec
        9152: e9 01     sbc     #$01
        9154: 85 04     sta     $04
        9156: d8        cld
        9157: 10 04     bpl     $915d
        9159: a9 10     lda     #$10 ; fire
        915b: 85 4e     sta     zap_fire_new
        915d: c9 03     cmp     #$03
        915f: d0 03     bne     $9164
        9161: 20 fe cc  jsr     $ccfe
        9164: a9 14     lda     #$14
        9166: 8d 05 06  sta     hs_timer
        9169: 20 ab b0  jsr     $b0ab
        916c: a9 18     lda     #$18
        916e: a4 04     ldy     $04
        9170: c0 08     cpy     #$08
        9172: b0 02     bcs     $9176
        9174: a9 78     lda     #$78
        9176: 25 4e     and     zap_fire_new
        9178: f0 34     beq     $91ae
        917a: a9 00     lda     #$00
        917c: 85 4e     sta     zap_fire_new
        917e: ad 00 02  lda     player_seg
; New-level entry, used by attract mode(?) and new game start
        9181: a8        tay
        9182: a6 3d     ldx     curplayer
        9184: 9d 02 01  sta     p1_startchoice,x
        9187: b9 fe 91  lda     startlevtbl,y
        918a: 24 05     bit     $05
        918c: 30 09     bmi     $9197
        918e: a0 01     ldy     #$01
        9190: 84 48     sty     p1_lives
        9192: ad ca 60  lda     pokey1_rand
        9195: 29 07     and     #$07
        9197: 95 46     sta     p1_level,x
        9199: 85 9f     sta     curlevel
        919b: 20 96 c1  jsr     setcolours
        919e: 20 c5 92  jsr     $92c5
        91a1: 20 34 92  jsr     set_enm_and_spikes
        91a4: 20 31 a8  jsr     reset_sz
        91a7: a9 02     lda     #$02
        91a9: 85 00     sta     gamestate
        91ab: 20 ad 92  jsr     $92ad
        91ae: a5 4e     lda     zap_fire_new
        91b0: 29 07     and     #$07
        91b2: 85 4e     sta     zap_fire_new
        91b4: 60        rts
; Loads the start bonus (level choice in A) into 29/2a/2b
ld_startbonus: 0a        asl     a
        91b6: aa        tax
        91b7: a9 00     lda     #$00
        91b9: 85 29     sta     $29
        91bb: bd c6 91  lda     start_bonus,x
        91be: 85 2a     sta     $2a
        91c0: bd c7 91  lda     start_bonus+1,x
        91c3: 85 2b     sta     $2b
        91c5: 60        rts
; Start bonuses, in BCD, low 00 not stored.
 start_bonus: .word   0000
        91c8: .word   0060
        91ca: .word   0160
        91cc: .word   0320
        91ce: .word   0540
        91d0: .word   0740
        91d2: .word   0940
        91d4: .word   1140
        91d6: .word   1340
        91d8: .word   1520
        91da: .word   1700
        91dc: .word   1880
        91de: .word   2080
        91e0: .word   2260
        91e2: .word   2480
        91e4: .word   2660
        91e6: .word   3000
        91e8: .word   3400
        91ea: .word   3820
        91ec: .word   4150
        91ee: .word   4390
        91f0: .word   4720
        91f2: .word   5310
        91f4: .word   5810
        91f6: .word   6240
        91f8: .word   6560
        91fa: .word   7660
        91fc: .word   8980
; Start level numbers?  (ie, is this table maybe mapping from index
; of level-select entry chosen to actual level number?)
 startlevtbl: .byte   00
        91ff: .byte   02
        9200: .byte   04
        9201: .byte   06
        9202: .byte   08
        9203: .byte   0a
        9204: .byte   0c
        9205: .byte   0e
        9206: .byte   10
        9207: .byte   13
        9208: .byte   15
        9209: .byte   17
        920a: .byte   19
        920b: .byte   1b
        920c: .byte   1e
        920d: .byte   20
        920e: .byte   23
        920f: .byte   27
        9210: .byte   2b
        9211: .byte   2e
        9212: .byte   30
        9213: .byte   33
        9214: .byte   37
        9215: .byte   3b
        9216: .byte   3e
        9217: .byte   40
        9218: .byte   48
        9219: .byte   50
        921a: .byte   ff
        921b: a9 0e     lda     #$0e
        921d: 8d 00 02  sta     player_seg
        9220: a9 f0     lda     #$f0
        9222: 85 51     sta     $51
        9224: a9 00     lda     #$00
        9226: 8d 06 01  sta     $0106
        9229: a9 0f     lda     #$0f
        922b: 8d 01 02  sta     $0201
        922e: a9 10     lda     #$10
        9230: 8d 02 02  sta     player_along
        9233: 60        rts
set_enm_and_spikes: ad 5b 01  lda     wave_enemies
        9237: 8d ab 03  sta     enemies_pending
        923a: ad 5a 01  lda     wave_spikeht
        923d: a2 0f     ldx     #$0f
        923f: 9d ac 03  sta     spike_ht,x
        9242: ca        dex
        9243: 10 fa     bpl     $923f
        9245: 60        rts
; Initialize the pending_seg and pending_vid tables.
        9246: a9 00     lda     #$00
        9248: a2 3f     ldx     #$3f
        924a: 9d 43 02  sta     pending_vid,x
        924d: ca        dex
        924e: 10 fa     bpl     $924a
        9250: ae ab 03  ldx     enemies_pending
        9253: ca        dex
        9254: ad ca 60  lda     pokey1_rand
        9257: 29 0f     and     #$0f
        9259: 9d 03 02  sta     pending_seg,x
        925c: 8a        txa
        925d: 0a        asl     a
        925e: 0a        asl     a
        925f: 0a        asl     a
        9260: 0a        asl     a
        9261: 1d 03 02  ora     pending_seg,x
        9264: d0 02     bne     $9268
        9266: a9 0f     lda     #$0f
        9268: 9d 43 02  sta     pending_vid,x
        926b: ca        dex
        926c: 10 e6     bpl     $9254
        926e: 60        rts
; Clear all enemies?  (eg, superzap?)
        926f: a2 06     ldx     #$06
        9271: a9 00     lda     #$00
        9273: 9d df 02  sta     enemy_along,x
        9276: ca        dex
        9277: 10 fa     bpl     $9273
        9279: 8d 08 01  sta     enemies_in
        927c: 8d 09 01  sta     enemies_top
; Are these five locations maybe counts of different types of enemy?
        927f: 8d 45 01  sta     n_spikers
        9282: 8d 42 01  sta     n_flippers
        9285: 8d 44 01  sta     n_tankers
        9288: 8d 43 01  sta     n_pulsars
        928b: 8d 46 01  sta     n_fuzzballs
        928e: 60        rts
 clear_shots: a9 00     lda     #$00
        9291: a2 0b     ldx     #$0b
        9293: 9d d3 02  sta     ply_shotpos,x
        9296: ca        dex
        9297: 10 fa     bpl     $9293
        9299: 8d 35 01  sta     ply_shotcnt
        929c: 85 a6     sta     enm_shotcnt
        929e: 60        rts
; Fills 0x00 into 8 bytes at 030a, also in 0116.
; Another disassembly says this aborts enemy death sequences in progress.
        929f: a2 07     ldx     #$07
        92a1: a9 00     lda     #$00
        92a3: 9d 0a 03  sta     $030a,x
        92a6: ca        dex
        92a7: 10 fa     bpl     $92a3
        92a9: 8d 16 01  sta     $0116
        92ac: 60        rts
        92ad: a9 00     lda     #$00
        92af: 85 50     sta     $50
        92b1: 60        rts
; Swap players?
        92b2: a2 11     ldx     #$11
        92b4: bd aa 03  lda     zap_uses,x
        92b7: bc bc 03  ldy     other_pl_data,x
        92ba: 9d bc 03  sta     other_pl_data,x
        92bd: 98        tya
        92be: 9d aa 03  sta     zap_uses,x
        92c1: ca        dex
        92c2: 10 f0     bpl     $92b4
        92c4: 60        rts
        92c5: a5 9f     lda     curlevel
        92c7: c9 62     cmp     #$62
        92c9: 90 07     bcc     $92d2
        92cb: ad da 60  lda     pokey2_rand
        92ce: 29 1f     and     #$1f
        92d0: 09 40     ora     #$40
        92d2: 85 2b     sta     $2b
        92d4: e6 2b     inc     $2b
; $2b now holds the effective level number
; Loop for X from $6f down through $03 (loop ends at $931d-$9326)
        92d6: a2 6f     ldx     #$6f
        92d8: 86 37     stx     $37
        92da: a6 37     ldx     $37
        92dc: bd 07 96  lda     $9607,x
        92df: 85 3c     sta     $3c
        92e1: bd 06 96  lda     $9606,x
        92e4: 85 3b     sta     $3b
        92e6: bd 05 96  lda     $9605,x
        92e9: 85 2d     sta     $2d
        92eb: bd 04 96  lda     $9604,x
        92ee: 85 2c     sta     $2c
        92f0: a9 01     lda     #$01
        92f2: 85 38     sta     $38
        92f4: a0 00     ldy     #$00
        92f6: b1 2c     lda     ($2c),y
        92f8: 8d 5e 01  sta     $015e
        92fb: f0 1c     beq     $9319
        92fd: a5 2b     lda     $2b
        92ff: c8        iny
        9300: d1 2c     cmp     ($2c),y
        9302: c8        iny
        9303: 90 0e     bcc     $9313 ; branch if lvl # is too low
        9305: d1 2c     cmp     ($2c),y
        9307: d0 01     bne     $930a ; branch to $9313 if lvl # >= ($2c),y
        9309: 18        clc
        930a: b0 07     bcs     $9313
        930c: c8        iny
        930d: 20 77 96  jsr     $9677
        9310: 4c 19 93  jmp     $9319
        9313: 20 83 96  jsr     $9683
        9316: 18        clc
        9317: 90 dd     bcc     $92f6
        9319: a0 00     ldy     #$00
        931b: 91 3b     sta     ($3b),y
        931d: a5 37     lda     $37
        931f: 38        sec
        9320: e9 04     sbc     #$04
        9322: 85 37     sta     $37
        9324: c9 ff     cmp     #$ff
        9326: d0 b2     bne     $92da
        9328: ad 6a 01  lda     diff_bits
        932b: 29 03     and     #$03 ; difficulty
        932d: c9 01     cmp     #$01 ; easy
        932f: d0 1c     bne     $934d
        9331: ce 1a 01  dec     enm_shotmax
        9334: ad 60 01  lda     spd_flipper_lsb
        9337: 49 ff     eor     #$ff
        9339: 4a        lsr     a
        933a: 4a        lsr     a
        933b: 4a        lsr     a
        933c: 6d 60 01  adc     spd_flipper_lsb
        933f: 8d 60 01  sta     spd_flipper_lsb
        9342: a5 9f     lda     curlevel
        9344: c9 11     cmp     #$11
        9346: b0 02     bcs     $934a
        9348: c6 b3     dec     flip_top_accel
        934a: b8        clv
        934b: 50 35     bvc     $9382
        934d: c9 02     cmp     #$02 ; hard
        934f: d0 31     bne     $9382
        9351: ee 1a 01  inc     enm_shotmax
        9354: ad 1a 01  lda     enm_shotmax
        9357: c9 03     cmp     #$03
        9359: 90 05     bcc     $9360
        935b: a9 03     lda     #$03
        935d: 8d 1a 01  sta     enm_shotmax
        9360: ad 60 01  lda     spd_flipper_lsb
        9363: 4a        lsr     a
        9364: 4a        lsr     a
        9365: 4a        lsr     a
        9366: 09 e0     ora     #$e0
        9368: 6d 60 01  adc     spd_flipper_lsb
        936b: 8d 60 01  sta     spd_flipper_lsb
        936e: ad 5b 01  lda     wave_enemies
        9371: 4a        lsr     a
        9372: 4a        lsr     a
        9373: 4a        lsr     a
        9374: 6d 5b 01  adc     wave_enemies
        9377: 8d 5b 01  sta     wave_enemies
        937a: ad 6d 01  lda     pulsar_fire
        937d: 09 40     ora     #$40
        937f: 8d 6d 01  sta     pulsar_fire
        9382: ad 63 01  lda     spd_spiker_lsb
        9385: 20 e0 93  jsr     crack_speed
        9388: 8d 63 01  sta     spd_spiker_lsb
        938b: 8c 68 01  sty     spd_spiker_msb
        938e: 8e 54 01  stx     $0154 ; hit_tol + spiker
        9391: ad 20 01  lda     enm_shotspd_lsb
        9394: 20 e0 93  jsr     crack_speed
        9397: 8d 20 01  sta     enm_shotspd_lsb
        939a: 8c 18 01  sty     enm_shotspd_msb
        939d: 86 a7     stx     $a7
        939f: ad 60 01  lda     spd_flipper_lsb
        93a2: 20 e0 93  jsr     crack_speed
        93a5: 8d 60 01  sta     spd_flipper_lsb
        93a8: 8d 62 01  sta     spd_tanker_lsb
        93ab: 8c 67 01  sty     spd_tanker_msb
        93ae: 8c 65 01  sty     spd_flipper_msb
        93b1: 8e 51 01  stx     hit_tol
        93b4: 8e 53 01  stx     $0153 ; hit_tol + tanker
        93b7: 8e 52 01  stx     $0152 ; hit_tol + pulsar
        93ba: ad 60 01  lda     spd_flipper_lsb
        93bd: 0a        asl     a
        93be: 8d 64 01  sta     spd_fuzzball_lsb
        93c1: ad 65 01  lda     spd_flipper_msb
        93c4: 2a        rol     a
        93c5: 8d 69 01  sta     spd_fuzzball_msb
        93c8: a9 06     lda     #$06
        93ca: 8d 55 01  sta     $0155 ; hit_tol + fuzzball
        93cd: a9 a0     lda     #$a0
        93cf: 8d 61 01  sta     spd_pulsar_lsb
        93d2: a9 fe     lda     #$fe
        93d4: 8d 66 01  sta     spd_pulsar_msb
        93d7: a9 01     lda     #$01
        93d9: 8d 4a 01  sta     tanker_load+1
        93dc: 8d 49 01  sta     tanker_load
        93df: 60        rts
; Convert a speed value such as found in the $9607 tables to MSB and LSB
; values, and a shot hit tolerance.  Return the MSB value in A, the LSB
; value in Y, and the hit tolerance in X.
 crack_speed: a0 ff     ldy     #$ff
        93e2: 84 29     sty     $29
        93e4: 0a        asl     a
        93e5: 26 29     rol     $29
        93e7: 0a        asl     a
        93e8: 26 29     rol     $29
        93ea: 0a        asl     a
        93eb: 26 29     rol     $29
        93ed: a4 29     ldy     $29
        93ef: 48        pha
        93f0: 98        tya
        93f1: 49 ff     eor     #$ff
        93f3: 18        clc
        93f4: 69 0d     adc     #$0d
        93f6: 4a        lsr     a
        93f7: aa        tax
        93f8: 68        pla
        93f9: 60        rts
; Computation of various per-level parameters.  See the table at $9607 and
; the code at $92d6 for more.  Each chunk is commented with the address or
; symbol for the byte it computes.
; $0119
     9607_0b: .chunk  5 ; 08 01 14 50 fd (80 -3)
        93ff: .chunk  4 ; 02 15 40 14
        9403: .chunk  4 ; 02 41 63 0a
; enm_shotmax
     9607_0f: .chunk  12 ; 04 01 09
;                          01 01 01 02 03 02 02 03 03
        9413: .chunk  4 ; 02 0a 40 02
        9417: .chunk  4 ; 02 41 63 03
; spd_flipper_lsb
     9607_67: .chunk  5 ; 08 01 08 d4 fb (-44 -5)
        9420: .chunk  11 ; 04 09 10
;                          af ac ac ac a8 a4 a0 a0
        942b: .chunk  5 ; 08 11 19 af fd (-81 -3)
        9430: .chunk  5 ; 08 1a 20 9d fd (-99 -3)
        9435: .chunk  5 ; 08 21 27 94 fd (-108 -3)
        943a: .chunk  5 ; 08 28 30 92 ff (-110 -1)
        943f: .chunk  5 ; 08 31 40 88 ff (-120 -1)
        9444: .chunk  5 ; 0c 41 63 60 41
; enm_shotspd_lsb
     9607_63: .chunk  4 ; 0a 01 63 c0
; spd_spiker_lsb
     9607_5f: .chunk  4 ; 0a 01 14 00
        9451: .chunk  4 ; 0a 15 20 d0
        9455: .chunk  4 ; 0a 21 30 d8
        9459: .chunk  4 ; 0a 31 63 d0
; $0157
     9607_3b: .chunk  4 ; 02 01 20 a0
        9461: .chunk  4 ; 02 21 40 a0
        9465: .chunk  4 ; 02 41 63 c0
; $0147
     9607_3f: .chunk  4 ; 02 01 30 04
        946d: .chunk  4 ; 02 31 40 06
        9471: .chunk  4 ; 02 41 63 08
; tanker_load+2
     9607_43: .chunk  4 ; 02 01 20 01
        9479: .chunk  4 ; 02 21 28 03
        947d: .chunk  4 ; 02 29 63 02
; tanker_load+3
     9607_47: .chunk  4 ; 02 01 30 01
        9485: .chunk  4 ; 02 31 63 03
; min_spikers
     9607_2b: .chunk  7 ; 04 01 04
;                         00 00 00 01
        9490: .chunk  4 ; 02 05 10 02
        9494: .chunk  4 ; 02 11 13 00
        9498: .chunk  4 ; 02 14 20 01
        949c: .chunk  4 ; 02 23 27 01
        94a0: .chunk  4 ; 02 2c 63 01
        94a4: .byte   00
; max_spikers
     9607_2f: .chunk  9 ; 04 01 06
;                         00 00 00 02 03 04
        94ae: .chunk  4 ; 02 07 0a 04
        94b2: .chunk  4 ; 02 0b 10 03
        94b6: .chunk  4 ; 02 14 19 02
        94ba: .chunk  10 ; 04 1a 20
;                          01 02 02 02 01 01 02
; I conjecture the 35 in this next line is intended to be 23 - ie, 35
; *decimal*.  This would make sense, as level 35 is the point at which
; we start getting spikes after level 32.  But with the table as is, we
; don't get spikers until level 43 ($2b - see the following line).
; This doesn't explain the weird 27, though - having no spikers on levels
; $28 through $2a seems peculiar.
        94c4: .chunk  4 ; 02 35 27 01 (??)
        94c8: .chunk  4 ; 02 2b 63 01
        94cc: .byte   00
; min_flippers
     9607_13: .chunk  4 ; 02 01 04 01
        94d1: .chunk  4 ; 02 05 63 00
        94d5: .byte   00
; max_flippers
     9607_17: .chunk  4 ; 02 01 04 04
        94da: .chunk  4 ; 02 05 10 05
        94de: .chunk  4 ; 02 11 13 03
        94e2: .chunk  4 ; 02 14 19 04
        94e6: .chunk  4 ; 02 1a 63 05
        94ea: .byte   00
; min_tankers
     9607_23: .chunk  7 ; 04 01 04
;                         00 00 01 00
        94f2: .chunk  4 ; 02 05 10 01
        94f6: .chunk  4 ; 02 11 20 01
        94fa: .chunk  4 ; 02 21 27 01
        94fe: .chunk  4 ; 02 28 63 01
        9502: .byte   00
; max_tankers
     9607_27: .chunk  8 ; 04 01 05
;                         00 00 01 00 01
        950b: .chunk  4 ; 02 06 10 02
        950f: .chunk  4 ; 02 11 1a 01
        9513: .chunk  4 ; 02 1b 20 01
        9517: .chunk  4 ; 02 21 2c 02
        951b: .chunk  4 ; 02 2d 63 03
        951f: .byte   00
; min_pulsars
     9607_1b: .chunk  4 ; 02 11 20 02
        9524: .chunk  4 ; 02 21 63 01
        9528: .byte   00
; max_pulsars
     9607_1f: .chunk  19 ; 04 11 20
;                 05 03 02  02 02 02 02  02 02 02 02  02 02 03 04  02
        953c: .chunk  4 ; 02 21 63 03
        9540: .byte   00
; min_fuzzballs
     9607_33: .chunk  4 ; 02 0b 10 01
        9545: .chunk  4 ; 02 16 19 01
        9549: .chunk  4 ; 02 1b 63 01
        954d: .byte   00
; max_fuzzballs
     9607_37: .chunk  4 ; 02 0b 10 01
        9552: .chunk  4 ; 02 16 19 01
        9556: .chunk  4 ; 02 1b 20 01
        955a: .chunk  4 ; 02 21 27 04
        955e: .chunk  4 ; 02 28 63 03
        9562: .byte   00
; pulsar_fliprate
     9607_57: .chunk  5 ; 04 11 12
;                         28 14
        9568: .chunk  5 ; 0c 13 20 14 28
        956d: .chunk  5 ; 08 21 27 14 ff (20 -1)
        9572: .chunk  5 ; 0c 28 63 14 0a
        9577: .byte   00
; fuzz_move_flg
     9607_6b: .chunk  5 ; 0c 11 20 00 40
        957d: .chunk  5 ; 0c 21 30 40 c0
        9582: .chunk  4 ; 02 31 63 c0
        9586: .byte   00
; fuzz_move_prb
     9607_6f: .chunk  4 ; 02 01 10 dc
        958b: .chunk  4 ; 02 11 27 c0
        958f: .chunk  5 ; 08 28 40 c0 01
        9594: .chunk  4 ; 02 41 63 e6
; max_enm
     9607_4b: .chunk  4 ; 02 01 63 06
; wave_spikeht
     9607_53: .chunk  19 ; 06 01 63
;                 00 00 00 e0  d8 d4 d0 c8  c0 b8 b0 a8  a0 a0 a0 a8
        95af: .byte   a0
        95b0: .byte   9c
        95b1: .byte   9a
        95b2: .byte   98
; wave_enemies
     9607_4f: .chunk  19 ; 04 01 10
;                 0a 0c 0f  11 14 16 14  18 1b 1d 1b  18 1a 1c 1e  1b
        95c6: .chunk  5 ; 08 11 1a 14 01
        95cb: .chunk  4 ; 02 1b 27 1b
        95cf: .chunk  5 ; 08 28 30 1d 01
        95d4: .chunk  5 ; 08 31 40 1f 01
        95d9: .chunk  5 ; 08 41 50 23 01
        95de: .chunk  5 ; 08 51 63 2b 01
; flip_top_accel
     9607_07: .chunk  4 ; 02 01 14 02
        95e7: .chunk  4 ; 02 15 20 02
        95eb: .chunk  4 ; 02 21 63 03
; pulsar_fire
     9607_03: .chunk  4 ; 02 3c 63 40
        95f3: .byte   00
; flipper_move
     9607_5b: .chunk  19 ; 06 01 63
;                          07 0b 19 24  53 0b 24 19  53 87 24 19  53 07 87 24
; See the code beginning $92d6.
        9607: .ptr    9607_03
        9609: .ptr    pulsar_fire
        960b: .ptr    9607_07
        960d: .ptr    flip_top_accel
        960f: .ptr    9607_0b
        9611: .ptr    shot_holdoff
        9613: .ptr    9607_0f
        9615: .ptr    enm_shotmax
        9617: .ptr    9607_13
        9619: .ptr    min_flippers
        961b: .ptr    9607_17
        961d: .ptr    max_flippers
        961f: .ptr    9607_1b
        9621: .ptr    min_pulsars
        9623: .ptr    9607_1f
        9625: .ptr    max_pulsars
        9627: .ptr    9607_23
        9629: .ptr    min_tankers
        962b: .ptr    9607_27
        962d: .ptr    max_tankers
        962f: .ptr    9607_2b
        9631: .ptr    min_spikers
        9633: .ptr    9607_2f
        9635: .ptr    max_spikers
        9637: .ptr    9607_33
        9639: .ptr    min_fuzzballs
        963b: .ptr    9607_37
        963d: .ptr    max_fuzzballs
        963f: .ptr    9607_3b
        9641: .ptr    0x157
        9643: .ptr    9607_3f
        9645: .ptr    pulse_beat
        9647: .ptr    9607_43
        9649: .ptr    tanker_load+2
        964b: .ptr    9607_47
        964d: .ptr    tanker_load+3
        964f: .ptr    9607_4b
        9651: .ptr    max_enm
        9653: .ptr    9607_4f
        9655: .ptr    wave_enemies
        9657: .ptr    9607_53
        9659: .ptr    wave_spikeht
        965b: .ptr    9607_57
        965d: .ptr    pulsar_fliprate
        965f: .ptr    9607_5b
        9661: .ptr    flipper_move
        9663: .ptr    9607_5f
        9665: .ptr    spd_spiker_lsb
        9667: .ptr    9607_63
        9669: .ptr    enm_shotspd_lsb
        966b: .ptr    9607_67
        966d: .ptr    spd_flipper_lsb
        966f: .ptr    9607_6b
        9671: .ptr    fuzz_move_flg
        9673: .ptr    9607_6f
        9675: .ptr    fuzz_move_prb
        9677: ae 5e 01  ldx     $015e
        967a: bd 90 96  lda     $9690,x
        967d: 48        pha
        967e: bd 8f 96  lda     $968f,x
        9681: 48        pha
        9682: 60        rts
        9683: ae 5e 01  ldx     $015e
        9686: bd 9e 96  lda     $969e,x
        9689: 48        pha
        968a: bd 9d 96  lda     $969d,x
        968d: 48        pha
        968e: 60        rts
; Jump table used by code at 9677.
; ltmin = first level-test byte
; ltmax = second level-test byte
; b[] = bytes following level test bytes
; thus, we have: opcode ltmin ltmax b[0] b[1] b[2] etc...
; (loc) = contents of memory location loc
; lev = current level number
; lwb = (((lev-1)&15)+1 - level # within its block of 16 levels
        968f: .word   0000 ; not used - tested for at $92fb
        9691: .jump   968f_02 ; A = b[0]
        9693: .jump   968f_04 ; A = b[lev-ltmin]
        9695: .jump   968f_06 ; A = b[lwb-ltmin]
        9697: .jump   968f_08 ; A = b[0] + ((lev-ltmin) * b[1])
        9699: .jump   968f_0a ; A = b[0] + ($0160)
        969b: .jump   968f_0c ; A = b[(lev-ltmin)&1]
; Jump table used by code at 9683.
        969d: .word   0000 ; not used - tested for at $92fb
        969f: .jump   969d_02_0a ; Y += 2
        96a1: .jump   969d_04_06 ; Y += ltmax - ltmin + 2
        96a3: .jump   969d_04_06 ; Y += ltmax - ltmin + 2
        96a5: .jump   969d_08_0c ; Y += 3
        96a7: .jump   969d_02_0a ; Y += 2
        96a9: .jump   969d_08_0c ; Y += 3
     968f_06: a5 2b     lda     $2b
        96ad: 38        sec
        96ae: e9 01     sbc     #$01
        96b0: 29 0f     and     #$0f
        96b2: 18        clc
        96b3: 69 01     adc     #$01
        96b5: 10 02     bpl     $96b9
     968f_04: a5 2b     lda     $2b
        96b9: 84 29     sty     $29
        96bb: 88        dey
        96bc: 88        dey
        96bd: 38        sec
        96be: f1 2c     sbc     ($2c),y
        96c0: 18        clc
        96c1: 65 29     adc     $29
        96c3: a8        tay
     968f_02: b1 2c     lda     ($2c),y
        96c6: 60        rts
  969d_08_0c: c8        iny
  969d_02_0a: c8        iny
        96c9: c8        iny
        96ca: 60        rts
  969d_04_06: b1 2c     lda     ($2c),y
        96cd: 88        dey
        96ce: 38        sec
        96cf: f1 2c     sbc     ($2c),y
        96d1: 85 29     sta     $29
        96d3: 98        tya
        96d4: 38        sec
        96d5: 65 29     adc     $29
        96d7: a8        tay
        96d8: c8        iny
        96d9: c8        iny
        96da: 60        rts
     968f_0a: b1 2c     lda     ($2c),y
        96dd: 18        clc
        96de: 6d 60 01  adc     spd_flipper_lsb
        96e1: 60        rts
     968f_08: 20 f4 96  jsr     $96f4
        96e5: aa        tax
        96e6: b1 2c     lda     ($2c),y
        96e8: c8        iny
        96e9: e0 00     cpx     #$00
        96eb: f0 06     beq     $96f3
        96ed: 18        clc
        96ee: 71 2c     adc     ($2c),y
        96f0: ca        dex
        96f1: d0 fa     bne     $96ed
        96f3: 60        rts
; Set A to current level number minus base level number
        96f4: a5 2b     lda     $2b
        96f6: 84 29     sty     $29
        96f8: 88        dey
        96f9: 88        dey
        96fa: 38        sec
        96fb: f1 2c     sbc     ($2c),y
        96fd: c8        iny
        96fe: c8        iny
        96ff: 60        rts
     968f_0c: 20 f4 96  jsr     $96f4
        9703: 29 01     and     #$01
        9705: f0 01     beq     $9708
        9707: c8        iny
        9708: b1 2c     lda     ($2c),y
        970a: 60        rts
    state_04: 20 49 97  jsr     move_player
        970e: 20 3f a2  jsr     player_fire
        9711: 20 3a a8  jsr     check_zap
        9714: 20 a2 98  jsr     create_enemies
        9717: 20 1e 9b  jsr     move_enemies
        971a: 20 8f a1  jsr     move_shots
        971d: 20 a6 a2  jsr     enm_shoot
        9720: 20 54 a4  jsr     pshot_hit
        9723: 20 16 a4  jsr     $a416
        9726: 4c 04 a5  jmp     $a504
    state_20: ad 23 01  lda     $0123
        972c: 29 7f     and     #$7f ; ~$80
        972e: 8d 23 01  sta     $0123
        9731: 20 49 97  jsr     move_player
        9734: 20 f8 97  jsr     $97f8
        9737: 20 16 a4  jsr     $a416
        973a: 20 3f a2  jsr     player_fire
        973d: 20 8f a1  jsr     move_shots
        9740: ad 01 02  lda     $0201
        9743: 10 03     bpl     $9748
        9745: 20 04 a5  jsr     $a504
        9748: 60        rts
; handles player movement
 move_player: ad 01 02  lda     $0201
        974c: 10 01     bpl     $974f
        974e: 60        rts
        974f: a2 00     ldx     #$00
        9751: a5 05     lda     $05
        9753: 30 06     bmi     $975b
        9755: 20 c5 97  jsr     $97c5
        9758: b8        clv
        9759: 50 15     bvc     $9770
        975b: a5 50     lda     $50
        975d: 10 09     bpl     $9768
        975f: c9 e1     cmp     #$e1 ; -31
        9761: b0 02     bcs     $9765
        9763: a9 e1     lda     #$e1 ; -31
        9765: b8        clv
        9766: 50 06     bvc     $976e
        9768: c9 1f     cmp     #$1f ; 31
        976a: 90 02     bcc     $976e
        976c: a9 1f     lda     #$1f ; 31
        976e: 86 50     stx     $50
        9770: 85 2b     sta     $2b
        9772: 49 ff     eor     #$ff
        9774: 38        sec
        9775: 65 51     adc     $51
        9777: 85 2c     sta     $2c
        9779: ae 11 01  ldx     open_level
        977c: f0 1f     beq     $979d
        977e: c9 f0     cmp     #$f0
        9780: 90 04     bcc     $9786
        9782: a9 ef     lda     #$ef
        9784: 85 2c     sta     $2c
        9786: 45 2b     eor     $2b
        9788: 10 13     bpl     $979d
        978a: a5 2c     lda     $2c
        978c: 45 51     eor     $51
        978e: 10 0d     bpl     $979d
        9790: a5 51     lda     $51
        9792: 30 05     bmi     $9799
        9794: a9 00     lda     #$00
        9796: b8        clv
        9797: 50 02     bvc     $979b
        9799: a9 ef     lda     #$ef
        979b: 85 2c     sta     $2c
        979d: a5 2c     lda     $2c
        979f: 4a        lsr     a
        97a0: 4a        lsr     a
        97a1: 4a        lsr     a
        97a2: 4a        lsr     a
        97a3: 85 2a     sta     $2a
        97a5: 18        clc
        97a6: 69 01     adc     #$01
        97a8: 29 0f     and     #$0f
        97aa: 85 2b     sta     $2b
        97ac: a5 2a     lda     $2a
        97ae: cd 00 02  cmp     player_seg
        97b1: f0 03     beq     $97b6
        97b3: 20 b5 cc  jsr     $ccb5
        97b6: a5 2a     lda     $2a
        97b8: 8d 00 02  sta     player_seg
        97bb: a5 2b     lda     $2b
        97bd: 8d 01 02  sta     $0201
        97c0: a5 2c     lda     $2c
        97c2: 85 51     sta     $51
        97c4: 60        rts
; Find extant enemy which is highest up the tube.  Return -9 or 9 depending
; on which way we need to go to get to it, or -1 if there is no such enemy,
; or 0 if there is but we're already on the correct segment.
        97c5: a9 ff     lda     #$ff
        97c7: 85 29     sta     $29
        97c9: 85 2a     sta     $2a
        97cb: ae 1c 01  ldx     max_enm
        97ce: bd df 02  lda     enemy_along,x
        97d1: f0 08     beq     $97db
        97d3: c5 29     cmp     $29
        97d5: b0 04     bcs     $97db
        97d7: 85 29     sta     $29
        97d9: 86 2a     stx     $2a
        97db: ca        dex
        97dc: 10 f0     bpl     $97ce
        97de: a6 2a     ldx     $2a
        97e0: 30 15     bmi     $97f7
        97e2: bd b9 02  lda     enemy_seg,x
        97e5: ac 00 02  ldy     player_seg
        97e8: 20 a6 a7  jsr     $a7a6
        97eb: a8        tay
        97ec: f0 09     beq     $97f7
        97ee: 30 05     bmi     $97f5
        97f0: a9 f7     lda     #$f7
        97f2: b8        clv
        97f3: 50 02     bvc     $97f7
        97f5: a9 09     lda     #$09
        97f7: 60        rts
        97f8: ad 01 02  lda     $0201
        97fb: 10 01     bpl     $97fe
        97fd: 60        rts
        97fe: ad 06 01  lda     $0106
        9801: 30 01     bmi     $9804
        9803: 60        rts
        9804: ad 02 02  lda     player_along
        9807: c9 10     cmp     #$10
        9809: d0 03     bne     $980e
        980b: 20 ee cc  jsr     $ccee
        980e: ad 07 01  lda     along_lsb
        9811: 18        clc
        9812: 6d 04 01  adc     zoomspd_lsb
        9815: 8d 07 01  sta     along_lsb
        9818: ad 02 02  lda     player_along
        981b: 6d 05 01  adc     zoomspd_msb
        981e: 8d 02 02  sta     player_along
        9821: b0 02     bcs     $9825
        9823: c9 f0     cmp     #$f0
        9825: 90 0c     bcc     $9833
        9827: a9 0e     lda     #$0e
        9829: 85 00     sta     gamestate
        982b: 20 f2 cc  jsr     $ccf2
        982e: a9 ff     lda     #$ff
        9830: 8d 02 02  sta     player_along
        9833: ad 02 02  lda     player_along
        9836: c9 50     cmp     #$50
        9838: 90 08     bcc     $9842
        983a: ad 15 01  lda     $0115
        983d: d0 03     bne     $9842
        983f: 20 bd a7  jsr     $a7bd
        9842: a5 5c     lda     $5c
        9844: 18        clc
        9845: 6d 04 01  adc     zoomspd_lsb
        9848: 85 5c     sta     $5c
        984a: a5 5f     lda     $5f
        984c: 6d 05 01  adc     zoomspd_msb
        984f: 90 02     bcc     $9853
        9851: e6 5b     inc     $5b
        9853: c5 5f     cmp     $5f
        9855: f0 03     beq     $985a
        9857: ee 14 01  inc     $0114
        985a: 85 5f     sta     $5f
; Accelerate based on current level value.  The computation here is
; [zoomspd_msb:zoomspd_lsb] += v, where v is
; (((((curlevel<<2)&$ff)<$30)?$30:((curlevel<<2)&$ff))+$20)&$ff, which
; simplifies to (((((curlevel&63)<12)?12:curlevel)<<2)+$20)&$ff.
; This means slow zooms starting at level 56 (where level<<2 hits $e0),
; because the carry out of the +$20 add is explicitly cleared ($9869).
        985c: a5 9f     lda     curlevel
        985e: 0a        asl     a
        985f: 0a        asl     a
        9860: c9 30     cmp     #$30
        9862: 90 02     bcc     $9866 ; branch for 1-11 and 64-74
        9864: a9 30     lda     #$30
        9866: 18        clc
        9867: 69 20     adc     #$20
        9869: 18        clc
        986a: 6d 04 01  adc     zoomspd_lsb
        986d: 8d 04 01  sta     zoomspd_lsb
; Why not "bcc 1f; inc zoomspd_msb; 1:"?  I have no idea.
        9870: ad 05 01  lda     zoomspd_msb
        9873: 69 00     adc     #$00
        9875: 8d 05 01  sta     zoomspd_msb
        9878: ad 02 02  lda     player_along
        987b: c9 f0     cmp     #$f0
        987d: b0 22     bcs     $98a1
; Check for player getting spiked
; I do not understand why scan all segments here, instead of just checking
; the value for player_seg, when $9886/$9889 ensure that only player_seg's
; value actually matters anyway.
        987f: a2 0f     ldx     #$0f
        9881: bd ac 03  lda     spike_ht,x
        9884: f0 18     beq     $989e
        9886: ec 00 02  cpx     player_seg
        9889: d0 13     bne     $989e
        988b: cd 02 02  cmp     player_along
        988e: b0 0e     bcs     $989e
        9890: 20 06 cd  jsr     sound_pulsar
        9893: 20 47 a3  jsr     pieces_death
        9896: a9 00     lda     #$00
        9898: 8d 15 01  sta     $0115
        989b: 20 8f 92  jsr     clear_shots
        989e: ca        dex
        989f: 10 e0     bpl     $9881
        98a1: 60        rts
create_enemies: a0 00     ldy     #$00
        98a4: 8c 4f 01  sty     $014f
        98a7: ad 08 01  lda     enemies_in
        98aa: 18        clc
        98ab: 6d 09 01  adc     enemies_top
        98ae: cd 1c 01  cmp     max_enm
        98b1: 90 04     bcc     $98b7
        98b3: f0 02     beq     $98b7
        98b5: a0 ff     ldy     #$ff
        98b7: ad 25 01  lda     zap_running
        98ba: f0 02     beq     $98be
        98bc: a0 ff     ldy     #$ff
        98be: 84 2f     sty     $2f
        98c0: a2 3f     ldx     #$3f
        98c2: bd 43 02  lda     pending_vid,x
        98c5: f0 52     beq     $9919
        98c7: 24 2f     bit     $2f
        98c9: 30 23     bmi     $98ee
        98cb: 38        sec
        98cc: e9 01     sbc     #$01
        98ce: 9d 43 02  sta     pending_vid,x
        98d1: d0 06     bne     $98d9
        98d3: 20 23 99  jsr     $9923
        98d6: b8        clv
        98d7: 50 15     bvc     $98ee
        98d9: c9 3f     cmp     #$3f
        98db: d0 11     bne     $98ee
        98dd: bc 03 02  ldy     pending_seg,x
        98e0: ad 4f 01  lda     $014f
        98e3: 0d 4f 01  ora     $014f
        98e6: 39 38 ca  and     $ca38,y
        98e9: f0 03     beq     $98ee
        98eb: fe 43 02  inc     pending_vid,x
        98ee: bd 43 02  lda     pending_vid,x
        98f1: c9 40     cmp     #$40
        98f3: 90 14     bcc     $9909
        98f5: a5 03     lda     timectr
        98f7: 29 01     and     #$01
        98f9: d0 0b     bne     $9906
        98fb: bd 03 02  lda     pending_seg,x
        98fe: 18        clc
        98ff: 69 01     adc     #$01
        9901: 29 0f     and     #$0f
        9903: 9d 03 02  sta     pending_seg,x
        9906: b8        clv
        9907: 50 10     bvc     $9919
        9909: c9 20     cmp     #$20
        990b: 90 0c     bcc     $9919
        990d: bc 03 02  ldy     pending_seg,x
        9910: b9 38 ca  lda     $ca38,y
        9913: 0d 4f 01  ora     $014f
        9916: 8d 4f 01  sta     $014f
        9919: ca        dex
        991a: 10 a6     bpl     $98c2
        991c: ad 4f 01  lda     $014f
        991f: 8d 50 01  sta     $0150
        9922: 60        rts
        9923: a9 f0     lda     #$f0
        9925: 85 29     sta     $29
        9927: bd 03 02  lda     pending_seg,x
        992a: 85 2a     sta     $2a
        992c: 86 35     stx     $35
        992e: 20 a5 99  jsr     $99a5
        9931: a6 35     ldx     $35
        9933: a5 29     lda     $29
        9935: f0 0e     beq     $9945
        9937: 20 4d 99  jsr     $994d
        993a: f0 09     beq     $9945
        993c: ce ab 03  dec     enemies_pending
        993f: a9 00     lda     #$00
        9941: 9d 43 02  sta     pending_vid,x
        9944: 60        rts
        9945: a9 ff     lda     #$ff
        9947: 85 2f     sta     $2f
        9949: fe 43 02  inc     pending_vid,x
        994c: 60        rts
        994d: 84 36     sty     $36
        994f: ac 1c 01  ldy     max_enm
        9952: b9 df 02  lda     enemy_along,y
        9955: d0 46     bne     $999d
        9957: a5 29     lda     $29 ; along value
        9959: 99 df 02  sta     enemy_along,y
        995c: a5 2a     lda     $2a ; segment number
        995e: c9 0f     cmp     #$0f
        9960: d0 0a     bne     $996c
        9962: 2c 11 01  bit     open_level
        9965: 10 05     bpl     $996c
        9967: ad ca 60  lda     pokey1_rand
        996a: 29 0e     and     #$0e
        996c: 99 b9 02  sta     enemy_seg,y
        996f: 18        clc
        9970: 69 01     adc     #$01
        9972: 29 0f     and     #$0f
        9974: 99 cc 02  sta     $02cc,y
        9977: a9 00     lda     #$00
        9979: 99 a6 02  sta     shot_delay,y
        997c: a5 2c     lda     $2c
        997e: 99 8a 02  sta     $028a,y
        9981: a5 2d     lda     $2d
        9983: 99 91 02  sta     enm_move_pc,y
        9986: ee 08 01  inc     enemies_in
        9989: a5 2b     lda     $2b
        998b: 99 83 02  sta     $0283,y
        998e: a4 36     ldy     $36
        9990: 29 07     and     #$07
        9992: 86 36     stx     $36
        9994: aa        tax
        9995: fe 42 01  inc     n_flippers,x
        9998: a6 36     ldx     $36
        999a: a9 10     lda     #$10
        999c: 60        rts
        999d: 88        dey
        999e: 10 b2     bpl     $9952
        99a0: a4 36     ldy     $36
        99a2: a9 00     lda     #$00
        99a4: 60        rts
; Pick an enemy type to create a new enemy as.
; First, compute the number available to be created for each type.
        99a5: a9 00     lda     #$00
        99a7: a2 04     ldx     #$04
        99a9: 9d 3d 01  sta     avl_flippers,x
        99ac: ca        dex
        99ad: 10 fa     bpl     $99a9
        99af: a2 04     ldx     #$04
        99b1: bd 2e 01  lda     max_flippers,x
        99b4: 38        sec
        99b5: fd 42 01  sbc     n_flippers,x
        99b8: 90 03     bcc     $99bd
        99ba: 9d 3d 01  sta     avl_flippers,x
        99bd: ca        dex
        99be: 10 f1     bpl     $99b1
; Now, count each tanker as two of the enemy type it's holding.
; Note that this can push the availability number through zero, in which
; case it wraps around to 255.
        99c0: ac 1c 01  ldy     max_enm
        99c3: b9 df 02  lda     enemy_along,y
        99c6: f0 14     beq     $99dc
        99c8: b9 8a 02  lda     $028a,y
        99cb: 29 03     and     #$03
        99cd: f0 0d     beq     $99dc
        99cf: aa        tax
        99d0: e0 03     cpx     #$03 ; 3 means fuzzball, not tanker!
        99d2: d0 02     bne     $99d6
        99d4: a2 05     ldx     #$05
        99d6: de 3c 01  dec     $013c,x ; avl_flippers-1
        99d9: de 3c 01  dec     $013c,x ; avl_flippers-1
        99dc: 88        dey
        99dd: 10 e4     bpl     $99c3
; Take this level's maximum enemy count, plus one, and subtract off the
; counts of each type of enemy.
        99df: a2 04     ldx     #$04
        99e1: ad 1c 01  lda     max_enm
        99e4: 18        clc
        99e5: 69 01     adc     #$01
        99e7: 38        sec
        99e8: fd 42 01  sbc     n_flippers,x
        99eb: ca        dex
        99ec: 10 f9     bpl     $99e7
; Limit the number-available for each enemy type to the number we just
; computed, the total number of enemies available.  (In particular, this
; deals with the worst cases where availability has wrapped around.)
        99ee: a2 04     ldx     #$04
        99f0: dd 3d 01  cmp     avl_flippers,x
        99f3: b0 03     bcs     $99f8
        99f5: 9d 3d 01  sta     avl_flippers,x
        99f8: ca        dex
        99f9: 10 f5     bpl     $99f0
; Figure out how many enemy types have nonzero availability.
        99fb: a2 04     ldx     #$04
        99fd: a0 00     ldy     #$00
        99ff: bd 3d 01  lda     avl_flippers,x
        9a02: f0 01     beq     $9a05
        9a04: c8        iny
        9a05: ca        dex
        9a06: 10 f7     bpl     $99ff
; If no enemy types have nonzero availability, nothing to do.
        9a08: 98        tya
        9a09: f0 77     beq     $9a82
; If only one type has nonzero availability, it's easy.
        9a0b: 88        dey
        9a0c: d0 18     bne     $9a26
; Only one type possible.  Find the type and create the enemy.
        9a0e: a2 04     ldx     #$04
        9a10: bd 3d 01  lda     avl_flippers,x
        9a13: f0 0b     beq     $9a20
        9a15: bd 29 01  lda     min_flippers,x
        9a18: f0 06     beq     $9a20
        9a1a: 20 87 9a  jsr     $9a87
        9a1d: f0 01     beq     $9a20
        9a1f: 60        rts
        9a20: ca        dex
        9a21: 10 ed     bpl     $9a10
        9a23: b8        clv
        9a24: 50 5c     bvc     $9a82
; Hard case: multiple types possible.
; See if any of the minimum values are unsatisfied.
        9a26: 84 61     sty     $61
        9a28: a2 04     ldx     #$04
        9a2a: bd 3d 01  lda     avl_flippers,x
        9a2d: f0 0e     beq     $9a3d
        9a2f: bd 42 01  lda     n_flippers,x
        9a32: dd 29 01  cmp     min_flippers,x
        9a35: b0 06     bcs     $9a3d
        9a37: 20 87 9a  jsr     $9a87
        9a3a: f0 01     beq     $9a3d
        9a3c: 60        rts
        9a3d: ca        dex
        9a3e: 10 ea     bpl     $9a2a
; No unsatisfied minima.  If we can do a spiker and we can do a tanker,
; have a look at the shortest spike, and if it's less than $cc high, create
; a spiker, else create a tanker.
        9a40: ad 40 01  lda     avl_spikers
        9a43: f0 1c     beq     $9a61
        9a45: ad 3f 01  lda     avl_tankers
        9a48: f0 17     beq     $9a61
        9a4a: a4 2a     ldy     $2a
        9a4c: b9 ac 03  lda     spike_ht,y
        9a4f: d0 02     bne     $9a53
        9a51: a9 ff     lda     #$ff
        9a53: a2 03     ldx     #$03 ; spiker
        9a55: c9 cc     cmp     #$cc
        9a57: b0 02     bcs     $9a5b
        9a59: a2 02     ldx     #$02 ; tanker
        9a5b: 20 87 9a  jsr     $9a87
        9a5e: f0 01     beq     $9a61
        9a60: 60        rts
; Nothing yet.  Start at a random point and go through the list of enemies
; up to four times.  Each time through, for each type with nonzero minimum
; and availability, try to create one of it.
        9a61: ad da 60  lda     pokey2_rand
        9a64: 29 03     and     #$03
        9a66: aa        tax
        9a67: e8        inx
        9a68: a0 04     ldy     #$04
        9a6a: bd 29 01  lda     min_flippers,x
        9a6d: f0 0b     beq     $9a7a
        9a6f: bd 3d 01  lda     avl_flippers,x
        9a72: f0 06     beq     $9a7a
        9a74: 20 87 9a  jsr     $9a87
        9a77: f0 01     beq     $9a7a
        9a79: 60        rts
        9a7a: ca        dex
        9a7b: 10 02     bpl     $9a7f
        9a7d: a2 04     ldx     #$04
        9a7f: 88        dey
        9a80: 10 e8     bpl     $9a6a
        9a82: a9 00     lda     #$00
        9a84: 85 29     sta     $29
        9a86: 60        rts
; Try to create one enemy of the type found in x.  Return with Z set on
; failure, clear on success.
        9a87: 8a        txa
        9a88: 0a        asl     a
        9a89: a8        tay
        9a8a: b9 94 9a  lda     $9a94,y
        9a8d: 48        pha
        9a8e: b9 93 9a  lda     $9a93,y
        9a91: 48        pha
        9a92: 60        rts
; Jump table, used by code at 9a87, called from various places
        9a93: .jump   $9a9d ; flipper
        9a95: .jump   $9aa9 ; pulsar
        9a97: .jump   $9abb ; tanker
        9a99: .jump   $9ab7 ; spiker
        9a9b: .jump   $9ab3 ; fuzzball
        9a9d: ad 02 9b  lda     $9b02 ; flipper
        9aa0: 85 2c     sta     $2c
        9aa2: ad 5d 01  lda     flipper_move
        9aa5: a0 00     ldy     #$00 ; flipper
        9aa7: f0 4d     beq     $9af6
        9aa9: ad 03 9b  lda     $9b03 ; pulsar
        9aac: 0d 6d 01  ora     pulsar_fire
        9aaf: a0 01     ldy     #$01 ; pulsar
        9ab1: d0 3e     bne     $9af1
        9ab3: a0 04     ldy     #$04 ; fuzzball
        9ab5: d0 37     bne     $9aee
        9ab7: a0 03     ldy     #$03 ; spiker
        9ab9: d0 33     bne     $9aee
        9abb: ad ca 60  lda     pokey1_rand ; tanker
        9abe: 29 03     and     #$03
        9ac0: a8        tay
        9ac1: a9 04     lda     #$04
        9ac3: 85 2b     sta     $2b
        9ac5: 86 39     stx     $39
        9ac7: c6 2b     dec     $2b
        9ac9: 10 05     bpl     $9ad0
        9acb: a6 39     ldx     $39
        9acd: a9 00     lda     #$00
        9acf: 60        rts
        9ad0: 88        dey
        9ad1: 10 02     bpl     $9ad5
        9ad3: a0 03     ldy     #$03
        9ad5: be 49 01  ldx     tanker_load,y
        9ad8: e0 03     cpx     #$03
        9ada: d0 02     bne     $9ade
        9adc: a2 05     ldx     #$05
        9ade: bd 3c 01  lda     $013c,x
        9ae1: f0 e4     beq     $9ac7
        9ae3: a6 39     ldx     $39
        9ae5: b9 49 01  lda     tanker_load,y
        9ae8: 09 40     ora     #$40
        9aea: a0 02     ldy     #$02
        9aec: d0 03     bne     $9af1
        9aee: b9 02 9b  lda     $9b02,y
        9af1: 85 2c     sta     $2c
        9af3: b9 fd 9a  lda     $9afd,y
        9af6: 84 2b     sty     $2b
        9af8: 85 2d     sta     $2d
        9afa: a5 29     lda     $29
        9afc: 60        rts
; Values for $2d, per-enemy-type.  See $9af3.
; I think these are the initial movement p-code pc values.
; The flipper value is mostly ignored, using flipper_move instead.
        9afd: .byte   07 ; flipper
        9afe: .byte   72 ; pulsar
        9aff: .byte   07 ; tanker
        9b00: .byte   00 ; spiker
        9b01: .byte   61 ; fuzzball
; Values for $2c, per-enemy-type.  See code at $9a87 and the fragments it
; branches to.  This ends up in the $028a vector for the enemy.
        9b02: .byte   40 ; flipper
        9b03: .byte   00 ; pulsar - ORed with pulsar_fire; see $9aac
        9b04: .byte   41 ; tanker - not actually used; see $9abb..$9aec
        9b05: .byte   40 ; spiker
        9b06: .byte   00 ; fuzzball
        9b07: 84 36     sty     $36
        9b09: a5 29     lda     $29
        9b0b: c9 20     cmp     #$20
        9b0d: a5 2b     lda     $2b
        9b0f: b0 07     bcs     $9b18
        9b11: a8        tay
        9b12: 20 ee 9a  jsr     $9aee
        9b15: b8        clv
        9b16: 50 03     bvc     $9b1b
        9b18: 20 88 9a  jsr     $9a88
        9b1b: a4 36     ldy     $36
        9b1d: 60        rts
move_enemies: ad 01 02  lda     $0201
        9b21: 30 33     bmi     $9b56
        9b23: ae 1c 01  ldx     max_enm
        9b26: 86 37     stx     $37
        9b28: a6 37     ldx     $37
        9b2a: bd df 02  lda     enemy_along,x
        9b2d: f0 23     beq     $9b52
        9b2f: a9 01     lda     #$01
        9b31: 8d 0a 01  sta     pcode_run
        9b34: bd 91 02  lda     enm_move_pc,x
        9b37: 8d 0b 01  sta     pcode_pc
; This appears to be a p-code engine!
; The engine's pc is $010b, with the code itself at $a0f7.  The jump
; table at $9ba2 and the code it points to determines the actions of
; each p-opcode.  The p-machine is halted (ie, the interpreter loop
; here is exited) by setting $010a to zero.
        9b3a: ad 0b 01  lda     pcode_pc
        9b3d: a8        tay
        9b3e: b9 f7 a0  lda     $a0f7,y
        9b41: 20 98 9b  jsr     $9b98
        9b44: ee 0b 01  inc     pcode_pc
        9b47: ad 0a 01  lda     pcode_run
        9b4a: d0 ee     bne     $9b3a
        9b4c: ad 0b 01  lda     pcode_pc
        9b4f: 9d 91 02  sta     enm_move_pc,x
        9b52: c6 37     dec     $37
        9b54: 10 d2     bpl     $9b28
        9b56: ad 48 01  lda     pulsing
        9b59: 18        clc
        9b5a: 6d 47 01  adc     pulse_beat
        9b5d: a8        tay
        9b5e: 4d 48 01  eor     pulsing
        9b61: 8c 48 01  sty     pulsing
        9b64: 10 16     bpl     $9b7c
        9b66: 98        tya
        9b67: 10 06     bpl     $9b6f
        9b69: 20 06 cd  jsr     sound_pulsar
        9b6c: b8        clv
        9b6d: 50 0d     bvc     $9b7c
        9b6f: ad 43 01  lda     n_pulsars
        9b72: f0 08     beq     $9b7c
        9b74: ad 01 02  lda     $0201
        9b77: 30 03     bmi     $9b7c
        9b79: 20 02 cd  jsr     $cd02
        9b7c: ad 48 01  lda     pulsing
        9b7f: 30 07     bmi     $9b88
        9b81: c9 0f     cmp     #$0f
        9b83: b0 07     bcs     $9b8c
        9b85: b8        clv
        9b86: 50 0f     bvc     $9b97
        9b88: c9 c1     cmp     #$c1
        9b8a: b0 0b     bcs     $9b97
        9b8c: ad 47 01  lda     pulse_beat
        9b8f: 49 ff     eor     #$ff
        9b91: 18        clc
        9b92: 69 01     adc     #$01
        9b94: 8d 47 01  sta     pulse_beat
        9b97: 60        rts
        9b98: a8        tay
        9b99: b9 a3 9b  lda     $9ba3,y
        9b9c: 48        pha
        9b9d: b9 a2 9b  lda     $9ba2,y
        9ba0: 48        pha
        9ba1: 60        rts
; See $9b3a for what this jump table is.
        9ba2: .jump   eact_00 ; 00 = halt
        9ba4: .jump   eact_02 ; 02 = next byte -> $0298,x
        9ba6: .jump   eact_04 ; 04 = if $010c holds zero, skip next two bytes
        9ba8: .jump   eact_06 ; 06 = unconditional branch
        9baa: .jump   eact_08 ; 08 = if (--$0298,x) branch else skip
        9bac: .jump   eact_0a ; 0a = nop
        9bae: .jump   eact_0c ; 0c = move per its type's speed setting
;                                    also handles reaching end-of-tube
        9bb0: .jump   eact_0e ; 0e = grow spike, reverse, convert
        9bb2: .jump   eact_10 ; 10 = $00<next byte> contents -> $0298,x
        9bb4: .jump   eact_12 ; 12 = start flip
        9bb6: .jump   eact_14 ; 14 = continue/end flip
        9bb8: .jump   eact_16 ; 16 = reverse direction (segmentwise)
        9bba: .jump   eact_18 ; 18 = check and maybe grab player
        9bbc: .jump   eact_1a ; 1a = if $010c == 0, branch
        9bbe: .jump   eact_1c ; 1c = enemy-above-spike? -> $010c
        9bc0: .jump   eact_1e ; 1e = fuzzball movement?
        9bc2: .jump   eact_20 ; 20 = check for enemy-touches-player death?
        9bc4: .jump   eact_22 ; 22 = do pulsar motion
        9bc6: .jump   eact_24 ; 24 = set enemy direction towards player
        9bc8: .jump   eact_26 ; 26 = check for pulsing
     eact_00: a9 00     lda     #$00
        9bcc: 8d 0a 01  sta     pcode_run
     eact_0a: 60        rts
     eact_02: ee 0b 01  inc     pcode_pc
        9bd3: ac 0b 01  ldy     pcode_pc
        9bd6: b9 f7 a0  lda     $a0f7,y
        9bd9: 9d 98 02  sta     $0298,x
        9bdc: 60        rts
     eact_10: ee 0b 01  inc     pcode_pc
        9be0: ac 0b 01  ldy     pcode_pc
        9be3: b9 f7 a0  lda     $a0f7,y
        9be6: a8        tay
        9be7: b9 00 00  lda     gamestate,y
        9bea: 9d 98 02  sta     $0298,x
        9bed: 60        rts
     eact_04: ad 0c 01  lda     $010c
        9bf1: d0 06     bne     $9bf9
        9bf3: ee 0b 01  inc     pcode_pc
        9bf6: ee 0b 01  inc     pcode_pc
        9bf9: 60        rts
     eact_1a: ee 0b 01  inc     pcode_pc
        9bfd: ad 0c 01  lda     $010c
        9c00: d0 09     bne     $9c0b
        9c02: ac 0b 01  ldy     pcode_pc
        9c05: b9 f7 a0  lda     $a0f7,y
        9c08: 8d 0b 01  sta     pcode_pc
        9c0b: 60        rts
     eact_08: de 98 02  dec     $0298,x
        9c0f: d0 06     bne     eact_06
        9c11: ee 0b 01  inc     pcode_pc
        9c14: b8        clv
        9c15: 50 09     bvc     $9c20
     eact_06: ac 0b 01  ldy     pcode_pc
        9c1a: b9 f8 a0  lda     $a0f8,y
        9c1d: 8d 0b 01  sta     pcode_pc
        9c20: 60        rts
; Set $010c to 1 if the enemy is above the end of its segment's spike,
; 0 if not.
     eact_1c: bc b9 02  ldy     enemy_seg,x
        9c24: b9 ac 03  lda     spike_ht,y
        9c27: d0 02     bne     $9c2b
        9c29: a9 ff     lda     #$ff
        9c2b: dd df 02  cmp     enemy_along,x
        9c2e: b0 05     bcs     $9c35
        9c30: a9 00     lda     #$00
        9c32: b8        clv
        9c33: 50 02     bvc     $9c37
        9c35: a9 01     lda     #$01
        9c37: 8d 0c 01  sta     $010c
        9c3a: 60        rts
; Set $010c to $80 if we're pulsing now, or we will be four ticks in the
; future, or to $00 if not.
     eact_26: ad 47 01  lda     pulse_beat
        9c3e: 0a        asl     a
        9c3f: 0a        asl     a
        9c40: 18        clc
        9c41: 6d 48 01  adc     pulsing
        9c44: 2d 48 01  and     pulsing
        9c47: 29 80     and     #$80
        9c49: 49 80     eor     #$80
        9c4b: 8d 0c 01  sta     $010c
        9c4e: 60        rts
     eact_16: bd 83 02  lda     $0283,x
        9c52: 49 40     eor     #$40
        9c54: 9d 83 02  sta     $0283,x
        9c57: 60        rts
     eact_0c: bd 83 02  lda     $0283,x
        9c5b: 29 07     and     #$07
        9c5d: a8        tay
        9c5e: bd 8a 02  lda     $028a,x
        9c61: 30 36     bmi     $9c99
        9c63: bd 9f 02  lda     enemy_along_lsb,x
        9c66: 18        clc
        9c67: 79 60 01  adc     spd_flipper_lsb,y
        9c6a: 9d 9f 02  sta     enemy_along_lsb,x
        9c6d: bd df 02  lda     enemy_along,x
        9c70: 79 65 01  adc     spd_flipper_msb,y
        9c73: 9d df 02  sta     enemy_along,x
        9c76: cd 02 02  cmp     player_along
        9c79: f0 02     beq     $9c7d
        9c7b: b0 06     bcs     $9c83
        9c7d: 20 06 9d  jsr     0c_at_top
        9c80: b8        clv
        9c81: 50 13     bvc     $9c96
        9c83: c9 20     cmp     #$20
        9c85: b0 0f     bcs     $9c96
        9c87: bd 8a 02  lda     $028a,x
        9c8a: 29 03     and     #$03
        9c8c: f0 08     beq     $9c96
        9c8e: 8a        txa
        9c8f: 48        pha
        9c90: a8        tay
        9c91: 20 6f a0  jsr     $a06f
        9c94: 68        pla
        9c95: aa        tax
        9c96: b8        clv
        9c97: 50 1c     bvc     $9cb5
        9c99: bd 9f 02  lda     enemy_along_lsb,x
        9c9c: 38        sec
        9c9d: f9 60 01  sbc     spd_flipper_lsb,y
        9ca0: 9d 9f 02  sta     enemy_along_lsb,x
        9ca3: bd df 02  lda     enemy_along,x
        9ca6: f9 65 01  sbc     spd_flipper_msb,y
        9ca9: 9d df 02  sta     enemy_along,x
        9cac: c9 f0     cmp     #$f0
        9cae: 90 05     bcc     $9cb5
        9cb0: a9 f2     lda     #$f2
        9cb2: 9d df 02  sta     enemy_along,x
        9cb5: 60        rts
     eact_22: a0 01     ldy     #$01 ; pulsar
        9cb8: bd 8a 02  lda     $028a,x
        9cbb: 30 10     bmi     $9ccd
        9cbd: bd df 02  lda     enemy_along,x
        9cc0: cd 57 01  cmp     $0157
        9cc3: 90 02     bcc     $9cc7 ; branch if closer than $0157
        9cc5: a0 00     ldy     #$00 ; flipper
        9cc7: 20 63 9c  jsr     $9c63 ; move per speed for type Y
;                                       includes 0c_at_top call
        9cca: b8        clv
        9ccb: 50 17     bvc     $9ce4
        9ccd: 20 99 9c  jsr     $9c99 ; move away per speed for type Y
;                                       leaves enemy_along value in A
        9cd0: ac ab 03  ldy     enemies_pending
        9cd3: d0 02     bne     $9cd7
        9cd5: a9 ff     lda     #$ff
        9cd7: cd 57 01  cmp     $0157
        9cda: 90 08     bcc     $9ce4 ; branch if closer than $0157
        9cdc: bd 8a 02  lda     $028a,x
        9cdf: 49 80     eor     #$80
        9ce1: 9d 8a 02  sta     $028a,x
        9ce4: ad 48 01  lda     pulsing
        9ce7: 30 1b     bmi     $9d04
        9ce9: bd df 02  lda     enemy_along,x
        9cec: cd 57 01  cmp     $0157
        9cef: b0 13     bcs     $9d04 ; branch if farther away than $0157
        9cf1: ad 00 02  lda     player_seg
        9cf4: dd b9 02  cmp     enemy_seg,x
        9cf7: d0 0b     bne     $9d04
        9cf9: ad 01 02  lda     $0201
        9cfc: dd cc 02  cmp     $02cc,x
        9cff: d0 03     bne     $9d04
        9d01: 20 47 a3  jsr     pieces_death
        9d04: 60        rts
        9d05: .byte   16
; Reached the top of the tube.  Deal with it.
   0c_at_top: ad 02 02  lda     player_along
        9d09: 9d df 02  sta     enemy_along,x
        9d0c: bd 83 02  lda     $0283,x
        9d0f: 29 07     and     #$07
        9d11: c9 01     cmp     #$01 ; pulsar
        9d13: d0 0e     bne     $9d23
        9d15: ad ab 03  lda     enemies_pending
        9d18: f0 09     beq     $9d23
        9d1a: bd 8a 02  lda     $028a,x
        9d1d: 49 80     eor     #$80
        9d1f: 9d 8a 02  sta     $028a,x
        9d22: 60        rts
        9d23: bd 83 02  lda     $0283,x
        9d26: 10 04     bpl     $9d2c
        9d28: fe df 02  inc     enemy_along,x
        9d2b: 60        rts
        9d2c: ce 08 01  dec     enemies_in
        9d2f: ad 09 01  lda     enemies_top
        9d32: c9 01     cmp     #$01
        9d34: f0 06     beq     $9d3c
        9d36: 20 67 9d  jsr     eact_24
        9d39: b8        clv
        9d3a: 50 22     bvc     $9d5e
        9d3c: a0 06     ldy     #$06
        9d3e: b9 df 02  lda     enemy_along,y
        9d41: f0 0e     beq     $9d51
        9d43: 84 38     sty     $38
        9d45: e4 38     cpx     $38
        9d47: f0 08     beq     $9d51
        9d49: b9 df 02  lda     enemy_along,y
        9d4c: cd 02 02  cmp     player_along
        9d4f: f0 03     beq     $9d54
        9d51: 88        dey
        9d52: 10 ea     bpl     $9d3e
        9d54: b9 83 02  lda     $0283,y
        9d57: 29 40     and     #$40
        9d59: 49 40     eor     #$40
        9d5b: 9d 83 02  sta     $0283,x
        9d5e: a9 41     lda     #$41
        9d60: 8d 0b 01  sta     pcode_pc
        9d63: ee 09 01  inc     enemies_top
        9d66: 60        rts
     eact_24: bd b9 02  lda     enemy_seg,x
        9d6a: a8        tay
        9d6b: ad 00 02  lda     player_seg
        9d6e: 20 a6 a7  jsr     $a7a6
        9d71: 0a        asl     a
        9d72: bd 83 02  lda     $0283,x
        9d75: b0 05     bcs     $9d7c
        9d77: 09 40     ora     #$40
        9d79: b8        clv
        9d7a: 50 02     bvc     $9d7e
        9d7c: 29 bf     and     #$bf
        9d7e: 9d 83 02  sta     $0283,x
        9d81: 60        rts
; This code is used to continue and maybe end a flipper's flip, or other
; enemy movement from one segment to the next.
     eact_14: bc cc 02  ldy     $02cc,x
        9d85: bd 83 02  lda     $0283,x
        9d88: 29 40     and     #$40
        9d8a: d0 04     bne     $9d90
        9d8c: c8        iny
        9d8d: b8        clv
        9d8e: 50 01     bvc     $9d91
        9d90: 88        dey
        9d91: 98        tya
        9d92: 29 0f     and     #$0f
        9d94: 09 80     ora     #$80
        9d96: 9d cc 02  sta     $02cc,x
        9d99: bd 83 02  lda     $0283,x
        9d9c: 29 07     and     #$07
        9d9e: c9 04     cmp     #$04 ; fuzzball
        9da0: d0 4c     bne     $9dee
        9da2: bd cc 02  lda     $02cc,x
        9da5: 29 07     and     #$07
        9da7: d0 42     bne     $9deb
        9da9: bd cc 02  lda     $02cc,x
        9dac: 29 08     and     #$08
        9dae: f0 0b     beq     $9dbb
        9db0: bd b9 02  lda     enemy_seg,x
        9db3: 18        clc
        9db4: 69 01     adc     #$01
        9db6: 29 0f     and     #$0f
        9db8: 9d b9 02  sta     enemy_seg,x
        9dbb: bd 83 02  lda     $0283,x
        9dbe: 29 7f     and     #$7f
        9dc0: 9d 83 02  sta     $0283,x
        9dc3: a9 20     lda     #$20
        9dc5: 9d cc 02  sta     $02cc,x
        9dc8: bd 8a 02  lda     $028a,x
        9dcb: 49 80     eor     #$80
        9dcd: 9d 8a 02  sta     $028a,x
        9dd0: ad ab 03  lda     enemies_pending
        9dd3: d0 16     bne     $9deb
        9dd5: bd df 02  lda     enemy_along,x
        9dd8: cd 02 02  cmp     player_along
        9ddb: d0 06     bne     $9de3
        9ddd: 20 81 9f  jsr     $9f81
        9de0: b8        clv
        9de1: 50 08     bvc     $9deb
        9de3: bd 8a 02  lda     $028a,x
        9de6: 29 80     and     #$80
        9de8: 9d 8a 02  sta     $028a,x
        9deb: b8        clv
        9dec: 50 38     bvc     $9e26
; check if flip ended
        9dee: bc b9 02  ldy     enemy_seg,x
        9df1: bd 83 02  lda     $0283,x
        9df4: 49 40     eor     #$40
        9df6: 20 d7 9e  jsr     get_angle
        9df9: dd cc 02  cmp     $02cc,x
        9dfc: d0 28     bne     $9e26
; yes, stop flipping
        9dfe: bd 83 02  lda     $0283,x
        9e01: 29 7f     and     #$7f
        9e03: 9d 83 02  sta     $0283,x
        9e06: 29 40     and     #$40
        9e08: d0 11     bne     $9e1b
        9e0a: bd b9 02  lda     enemy_seg,x
        9e0d: 9d cc 02  sta     $02cc,x
        9e10: 38        sec
        9e11: e9 01     sbc     #$01
        9e13: 29 0f     and     #$0f
        9e15: 9d b9 02  sta     enemy_seg,x
        9e18: b8        clv
        9e19: 50 0b     bvc     $9e26
        9e1b: bd b9 02  lda     enemy_seg,x
        9e1e: 18        clc
        9e1f: 69 01     adc     #$01
        9e21: 29 0f     and     #$0f
        9e23: 9d cc 02  sta     $02cc,x
        9e26: bd 83 02  lda     $0283,x
        9e29: 29 80     and     #$80
        9e2b: 8d 0c 01  sta     $010c
        9e2e: 60        rts
     eact_18: bd 83 02  lda     $0283,x
        9e32: 30 13     bmi     $9e47
        9e34: bd b9 02  lda     enemy_seg,x
        9e37: cd 00 02  cmp     player_seg
        9e3a: d0 0b     bne     $9e47
        9e3c: bd cc 02  lda     $02cc,x
        9e3f: cd 01 02  cmp     $0201
        9e42: d0 03     bne     $9e47
        9e44: 20 3a a3  jsr     $a33a
        9e47: 60        rts
     eact_20: bd df 02  lda     enemy_along,x
        9e4b: cd 02 02  cmp     player_along
        9e4e: d0 0b     bne     $9e5b
        9e50: bd b9 02  lda     enemy_seg,x
        9e53: cd 00 02  cmp     player_seg
        9e56: d0 03     bne     $9e5b
        9e58: 20 43 a3  jsr     $a343
        9e5b: 60        rts
     eact_12: 20 ab 9e  jsr     rev_if_edge
        9e5f: bd 83 02  lda     $0283,x
        9e62: 09 80     ora     #$80
        9e64: 9d 83 02  sta     $0283,x
        9e67: 29 07     and     #$07
        9e69: c9 04     cmp     #$04 ; fuzzball
        9e6b: d0 1f     bne     $9e8c
        9e6d: bd 83 02  lda     $0283,x
        9e70: 29 40     and     #$40
        9e72: d0 05     bne     $9e79
        9e74: a9 81     lda     #$81
        9e76: b8        clv
        9e77: 50 0d     bvc     $9e86
        9e79: bd b9 02  lda     enemy_seg,x
        9e7c: 38        sec
        9e7d: e9 01     sbc     #$01
        9e7f: 29 0f     and     #$0f
        9e81: 9d b9 02  sta     enemy_seg,x
        9e84: a9 87     lda     #$87
        9e86: 9d cc 02  sta     $02cc,x
        9e89: b8        clv
        9e8a: 50 1e     bvc     $9eaa
        9e8c: bd 83 02  lda     $0283,x
        9e8f: 29 40     and     #$40
        9e91: f0 0b     beq     $9e9e
        9e93: bd b9 02  lda     enemy_seg,x
        9e96: 18        clc
        9e97: 69 01     adc     #$01
        9e99: 29 0f     and     #$0f
        9e9b: 9d b9 02  sta     enemy_seg,x
        9e9e: bd 83 02  lda     $0283,x
        9ea1: bc b9 02  ldy     enemy_seg,x
        9ea4: 20 d7 9e  jsr     get_angle
        9ea7: 9d cc 02  sta     $02cc,x
        9eaa: 60        rts
; Reverse motion direction if level open and we've run into an edge.
 rev_if_edge: ad 11 01  lda     open_level
        9eae: f0 26     beq     $9ed6
        9eb0: bd 83 02  lda     $0283,x
        9eb3: 29 40     and     #$40
        9eb5: f0 12     beq     $9ec9
        9eb7: bd b9 02  lda     enemy_seg,x
        9eba: c9 0e     cmp     #$0e
        9ebc: 90 08     bcc     $9ec6
        9ebe: bd 83 02  lda     $0283,x
        9ec1: 29 bf     and     #$bf
        9ec3: 9d 83 02  sta     $0283,x
        9ec6: b8        clv
        9ec7: 50 0d     bvc     $9ed6
        9ec9: bd b9 02  lda     enemy_seg,x
        9ecc: d0 08     bne     $9ed6
        9ece: bd 83 02  lda     $0283,x
        9ed1: 09 40     ora     #$40
        9ed3: 9d 83 02  sta     $0283,x
        9ed6: 60        rts
; Enter with motion value in A, segment number in Y; returns with angle
; value ($0-$f) in A, |ed with $80.
   get_angle: 29 40     and     #$40
        9ed9: f0 10     beq     $9eeb
        9edb: 88        dey
        9edc: 98        tya
        9edd: 29 0f     and     #$0f
        9edf: a8        tay
        9ee0: b9 ee 03  lda     tube_angle,y
        9ee3: 18        clc
        9ee4: 69 08     adc     #$08
        9ee6: 29 0f     and     #$0f
        9ee8: b8        clv
        9ee9: 50 03     bvc     $9eee
        9eeb: b9 ee 03  lda     tube_angle,y
        9eee: 09 80     ora     #$80
        9ef0: 60        rts
     eact_1e: a0 04     ldy     #$04
        9ef3: bd 8a 02  lda     $028a,x
        9ef6: 30 4b     bmi     $9f43
        9ef8: bd 9f 02  lda     enemy_along_lsb,x
        9efb: 18        clc
        9efc: 6d 64 01  adc     spd_fuzzball_lsb
        9eff: 9d 9f 02  sta     enemy_along_lsb,x
        9f02: bd df 02  lda     enemy_along,x
        9f05: 6d 69 01  adc     spd_fuzzball_msb
        9f08: 9d df 02  sta     enemy_along,x
        9f0b: cd 02 02  cmp     player_along
        9f0e: b0 09     bcs     $9f19
        9f10: ad 02 02  lda     player_along
        9f13: 9d df 02  sta     enemy_along,x
        9f16: b8        clv
        9f17: 50 11     bvc     $9f2a
        9f19: ac ab 03  ldy     enemies_pending ; if no pending, rush to top
        9f1c: f0 0b     beq     $9f29
        9f1e: a4 9f     ldy     curlevel
        9f20: c0 11     cpy     #$11
        9f22: b0 02     bcs     $9f26 ; branch if level >= 17
        9f24: c9 20     cmp     #$20
        9f26: b8        clv
        9f27: 50 01     bvc     $9f2a
        9f29: 60        rts
        9f2a: b0 11     bcs     $9f3d
        9f2c: ad 59 01  lda     fuzz_move_flg
        9f2f: 10 06     bpl     $9f37
        9f31: 20 81 9f  jsr     $9f81
        9f34: b8        clv
        9f35: 50 03     bvc     $9f3a
        9f37: 20 8a 9f  jsr     $9f8a
        9f3a: b8        clv
        9f3b: 50 03     bvc     $9f40
        9f3d: 20 5f 9f  jsr     $9f5f
        9f40: b8        clv
        9f41: 50 1b     bvc     $9f5e
        9f43: 20 99 9c  jsr     $9c99 ; move away per speed for type Y
        9f46: c9 80     cmp     #$80
        9f48: 90 11     bcc     $9f5b
        9f4a: 2c 59 01  bit     fuzz_move_flg
        9f4d: 50 06     bvc     $9f55 ; branch if level < 17
        9f4f: 20 81 9f  jsr     $9f81
        9f52: b8        clv
        9f53: 50 03     bvc     $9f58
        9f55: 20 8a 9f  jsr     $9f8a
        9f58: b8        clv
        9f59: 50 03     bvc     $9f5e
        9f5b: 20 5f 9f  jsr     $9f5f
        9f5e: 60        rts
        9f5f: bd df 02  lda     enemy_along,x
        9f62: 29 20     and     #$20
        9f64: f0 1a     beq     $9f80
        9f66: ad da 60  lda     pokey2_rand
        9f69: cd 5f 01  cmp     fuzz_move_prb
        9f6c: 90 12     bcc     $9f80
        9f6e: 2c 59 01  bit     fuzz_move_flg
        9f71: 50 0a     bvc     $9f7d
        9f73: 8a        txa
        9f74: 4a        lsr     a
        9f75: 90 13     bcc     $9f8a
        9f77: 20 81 9f  jsr     $9f81
        9f7a: b8        clv
        9f7b: 50 03     bvc     $9f80
        9f7d: 20 8a 9f  jsr     $9f8a
        9f80: 60        rts
        9f81: 20 67 9d  jsr     eact_24
        9f84: 20 4f 9c  jsr     eact_16
        9f87: 4c 99 9f  jmp     $9f99
        9f8a: bd 83 02  lda     $0283,x
        9f8d: 29 bf     and     #$bf
        9f8f: 2c ca 60  bit     pokey1_rand
        9f92: 50 02     bvc     $9f96
        9f94: 09 40     ora     #$40
        9f96: 9d 83 02  sta     $0283,x
        9f99: ad 11 01  lda     open_level
        9f9c: f0 1e     beq     $9fbc
        9f9e: bd 83 02  lda     $0283,x
        9fa1: 29 40     and     #$40
        9fa3: d0 0a     bne     $9faf
        9fa5: bd b9 02  lda     enemy_seg,x
        9fa8: c9 0f     cmp     #$0f
        9faa: b0 08     bcs     $9fb4
        9fac: b8        clv
        9fad: 50 0d     bvc     $9fbc
        9faf: bd b9 02  lda     enemy_seg,x
        9fb2: d0 08     bne     $9fbc
        9fb4: bd 83 02  lda     $0283,x
        9fb7: 49 40     eor     #$40
        9fb9: 9d 83 02  sta     $0283,x
        9fbc: a9 66     lda     #$66
        9fbe: 8d 0b 01  sta     pcode_pc
        9fc1: 4c 5f 9e  jmp     $9e5f
     eact_0e: a9 01     lda     #$01
        9fc6: 8d 0c 01  sta     $010c
        9fc9: bc b9 02  ldy     enemy_seg,x
        9fcc: b9 ac 03  lda     spike_ht,y
        9fcf: d0 05     bne     $9fd6
        9fd1: a9 f1     lda     #$f1
        9fd3: 99 ac 03  sta     spike_ht,y
        9fd6: bd df 02  lda     enemy_along,x
        9fd9: d9 ac 03  cmp     spike_ht,y
        9fdc: b0 08     bcs     $9fe6
        9fde: 99 ac 03  sta     spike_ht,y
        9fe1: a9 80     lda     #$80
        9fe3: 99 9a 03  sta     $039a,y
        9fe6: bd df 02  lda     enemy_along,x
        9fe9: c9 20     cmp     #$20
        9feb: b0 10     bcs     $9ffd
        9fed: bd 8a 02  lda     $028a,x
        9ff0: 09 80     ora     #$80
        9ff2: 9d 8a 02  sta     $028a,x
        9ff5: a9 20     lda     #$20
        9ff7: 9d df 02  sta     enemy_along,x
        9ffa: b8        clv
        9ffb: 50 2a     bvc     $a027
        9ffd: c9 f2     cmp     #$f2
        9fff: 90 26     bcc     $a027
        a001: 20 28 a0  jsr     spiker_hop
        a004: a9 f0     lda     #$f0
        a006: 9d df 02  sta     enemy_along,x
        a009: ad ab 03  lda     enemies_pending
        a00c: d0 19     bne     $a027
; If no enemies pending, turn it into a flipper-holding tanker
        a00e: bd 8a 02  lda     $028a,x
        a011: 29 fc     and     #$fc
        a013: 09 01     ora     #$01
        a015: 9d 8a 02  sta     $028a,x
        a018: bd 83 02  lda     $0283,x
        a01b: 29 f8     and     #$f8
        a01d: 09 02     ora     #$02
        a01f: 9d 83 02  sta     $0283,x
        a022: a9 00     lda     #$00
        a024: 8d 0c 01  sta     $010c
        a027: 60        rts
  spiker_hop: a9 00     lda     #$00
        a02a: 85 2d     sta     $2d
        a02c: a9 0f     lda     #$0f
        a02e: 8d 40 01  sta     avl_spikers
        a031: ad da 60  lda     pokey2_rand
        a034: 29 0f     and     #$0f
        a036: a8        tay
        a037: c0 0f     cpy     #$0f
        a039: d0 05     bne     $a040
        a03b: ad 11 01  lda     open_level
        a03e: d0 0f     bne     $a04f
        a040: b9 ac 03  lda     spike_ht,y
        a043: d0 02     bne     $a047
        a045: a9 ff     lda     #$ff
        a047: c5 2d     cmp     $2d
        a049: 90 04     bcc     $a04f
        a04b: 85 2d     sta     $2d
        a04d: 84 29     sty     $29
        a04f: 88        dey
        a050: 10 02     bpl     $a054
        a052: a0 0f     ldy     #$0f
        a054: ce 40 01  dec     avl_spikers
        a057: 10 de     bpl     $a037
        a059: a5 29     lda     $29
        a05b: 9d b9 02  sta     enemy_seg,x
        a05e: 18        clc
        a05f: 69 01     adc     #$01
        a061: 29 0f     and     #$0f
        a063: 9d cc 02  sta     $02cc,x
        a066: bd 8a 02  lda     $028a,x
        a069: 29 7f     and     #$7f
        a06b: 9d 8a 02  sta     $028a,x
        a06e: 60        rts
; Enemy has reached the $20 point in the tube.  Handle it.
        a06f: b9 df 02  lda     enemy_along,y
        a072: 85 29     sta     $29
        a074: cd 02 02  cmp     player_along
        a077: d0 0f     bne     $a088
        a079: b9 83 02  lda     $0283,y
        a07c: 29 07     and     #$07
        a07e: c9 04     cmp     #$04 ; fuzzball
        a080: f0 06     beq     $a088
        a082: ce 09 01  dec     enemies_top
        a085: b8        clv
        a086: 50 03     bvc     $a08b
        a088: ce 08 01  dec     enemies_in
        a08b: a9 00     lda     #$00
        a08d: 99 df 02  sta     enemy_along,y
        a090: b9 83 02  lda     $0283,y
        a093: 29 07     and     #$07
        a095: 86 35     stx     $35
        a097: aa        tax
        a098: de 42 01  dec     n_flippers,x
        a09b: a6 35     ldx     $35
        a09d: b9 8a 02  lda     $028a,y
        a0a0: 29 03     and     #$03
        a0a2: f0 52     beq     $a0f6
        a0a4: 38        sec
        a0a5: e9 01     sbc     #$01
        a0a7: c9 02     cmp     #$02
        a0a9: d0 02     bne     $a0ad
        a0ab: a9 04     lda     #$04
        a0ad: 85 2b     sta     $2b
        a0af: b9 b9 02  lda     enemy_seg,y
        a0b2: 38        sec
        a0b3: e9 01     sbc     #$01
        a0b5: 29 0f     and     #$0f
        a0b7: c9 0f     cmp     #$0f
        a0b9: 90 07     bcc     $a0c2
        a0bb: 2c 11 01  bit     open_level
        a0be: 10 02     bpl     $a0c2
        a0c0: a9 00     lda     #$00
        a0c2: 85 2a     sta     $2a
        a0c4: 20 07 9b  jsr     $9b07
        a0c7: a5 2d     lda     $2d
        a0c9: 8d 0b 01  sta     pcode_pc
        a0cc: ce 0b 01  dec     pcode_pc
        a0cf: a9 00     lda     #$00
        a0d1: 8d 0a 01  sta     pcode_run
        a0d4: 20 4d 99  jsr     $994d
        a0d7: f0 1d     beq     $a0f6
        a0d9: a5 2a     lda     $2a
        a0db: 18        clc
        a0dc: 69 02     adc     #$02
        a0de: 29 0f     and     #$0f
        a0e0: c9 0f     cmp     #$0f
        a0e2: d0 07     bne     $a0eb
        a0e4: 2c 11 01  bit     open_level
        a0e7: 10 02     bpl     $a0eb
        a0e9: a9 0e     lda     #$0e
        a0eb: 85 2a     sta     $2a
        a0ed: a5 2b     lda     $2b
        a0ef: 09 40     ora     #$40
        a0f1: 85 2b     sta     $2b
        a0f3: 20 4d 99  jsr     $994d
        a0f6: 60        rts
; See the comments on $9b3a for what this is.
; Spiker entry point.  Grow, reverse at $20, convert to tanker...
        a0f7: .byte   0c ; 00: move per speed
        a0f8: .byte   0e ; 01: spike, reverse, convert to tanker
        a0f9: .byte   1a ; 02: branch conditional (if converted to tanker)
        a0fa: .byte   06 ; 03:   to 07
        a0fb: .byte   00 ; 04: done
        a0fc: .byte   06 ; 05: branch
        a0fd: .byte   ff ; 06:   to 00
; Entry point for "just move up".  Used for tankers, for flippers on some
; levels, in some cases for the pieces when a tanker splits...
        a0fe: .byte   0c ; 07: move per speed
        a0ff: .byte   00 ; 08: done
        a100: .byte   06 ; 09: branch
        a101: .byte   06 ; 0a:   to 07
; Flipper entry point: move 8 times, flip, repeat.  Don't move during flip.
        a102: .byte   02 ; 0b: store in $0298,x...
        a103: .byte   08 ; 0c:   ...08
        a104: .byte   0c ; 0d: move per speed
        a105: .byte   00 ; 0e: done
        a106: .byte   08 ; 0f: if --$0298,x then branch
        a107: .byte   0c ; 10:   to 0d
        a108: .byte   12 ; 11: start flip
        a109: .byte   00 ; 12: done
        a10a: .byte   14 ; 13: continue/end flip
        a10b: .byte   04 ; 14: if $010c == 0, skip to 17
        a10c: .byte   06 ; 15: branch
        a10d: .byte   11 ; 16:   to 12
        a10e: .byte   06 ; 17: branch
        a10f: .byte   0a ; 18:   to 0b
; Flipper entry point: flip constantly, moving for one tick between flips.
        a110: .byte   0c ; 19: move per speed
        a111: .byte   00 ; 1a: done
        a112: .byte   12 ; 1b: start flip
        a113: .byte   00 ; 1c: done
        a114: .byte   14 ; 1d: continue/end flip
        a115: .byte   0c ; 1e: move per speed
        a116: .byte   04 ; 1f: if $010c == 0, skip to 22
        a117: .byte   06 ; 20: branch
        a118: .byte   1b ; 21:   to 1c
        a119: .byte   06 ; 22: branch
        a11a: .byte   18 ; 23:   to 19
; Flipper entry point: flips twice one way, three times the other, twice,
; three times, twice, three times, etc.  Move on every tick except the
; ones on which we start a flip.
        a11b: .byte   0c ; 24: move per speed
        a11c: .byte   00 ; 25: done
        a11d: .byte   02 ; 26: store in $0298,x...
        a11e: .byte   02 ; 27:   ...02
        a11f: .byte   12 ; 28: start flip
        a120: .byte   00 ; 29: done
        a121: .byte   14 ; 2a: continue/end flip
        a122: .byte   0c ; 2b: move per speed
        a123: .byte   04 ; 2c: if $010c == 0, skip to 2f
        a124: .byte   06 ; 2d: branch
        a125: .byte   28 ; 2e:   to 29
        a126: .byte   00 ; 2f: done
        a127: .byte   08 ; 30: if --$0298,x then branch
        a128: .byte   27 ; 31:   to 28
        a129: .byte   16 ; 32: reverse direction
        a12a: .byte   02 ; 33: store in $0298,x...
        a12b: .byte   03 ; 34:   ...03
        a12c: .byte   12 ; 35: start flip
        a12d: .byte   00 ; 36: done
        a12e: .byte   14 ; 37: continue/end flip
        a12f: .byte   0c ; 38: move per speed
        a130: .byte   04 ; 39: if $010c == 0, skip to 3c
        a131: .byte   06 ; 3a: branch
        a132: .byte   35 ; 3b:   to 36
        a133: .byte   00 ; 3c: done
        a134: .byte   08 ; 3d: if --$0298,x then branch
        a135: .byte   34 ; 3e:   to 35
        a136: .byte   16 ; 3f: reverse direction
        a137: .byte   06 ; 40: branch
        a138: .byte   23 ; 41:   to 24
; Action 0c jumps here upon reaching top-of-tube.
        a139: .byte   02 ; 42: store in $0298,x...
        a13a: .byte   04 ; 43:   ...04
        a13b: .byte   18 ; 44: check and maybe grab player
        a13c: .byte   00 ; 45: done
        a13d: .byte   08 ; 46: if --$0298,x then branch
        a13e: .byte   43 ; 47:   to 44
        a13f: .byte   12 ; 48: start flip
        a140: .byte   00 ; 49: done
        a141: .byte   10 ; 4a: flip_top_accel -> $0298,x
        a142: .byte   b3 ; 4b:   (value for previous)
        a143: .byte   14 ; 4c: continue/end flip
        a144: .byte   1a ; 4d: if $010c == 0, branch
        a145: .byte   41 ; 4e:   to 42
        a146: .byte   08 ; 4f: if --$0298,x then branch
        a147: .byte   4b ; 50:   to 4c
        a148: .byte   06 ; 51: branch
        a149: .byte   48 ; 52:   to 49
; Flipper entry point: for levels where flippers ride spikes.
; Move every tick.
        a14a: .byte   00 ; 53: done
        a14b: .byte   0c ; 54: move per speed
        a14c: .byte   1c ; 55: set $010c to enemy-above-spike-p
        a14d: .byte   1a ; 56: if $010c == 0, branch
        a14e: .byte   52 ; 57:   to 53
        a14f: .byte   12 ; 58: start flip
        a150: .byte   00 ; 59: done
        a151: .byte   0c ; 5a: move per speed
        a152: .byte   14 ; 5b: continue/end flip
        a153: .byte   1a ; 5c: if $010c == 0, branch
        a154: .byte   52 ; 5d:   to 53
        a155: .byte   00 ; 5e: done
        a156: .byte   06 ; 5f: branch
        a157: .byte   5a ; 60:   to 5b
; Fuzzball entry point.
        a158: .byte   1e ; 61: fuzzball movement?
        a159: .byte   20 ; 62: check for enemy-touches-player death
        a15a: .byte   00 ; 63: done
        a15b: .byte   06 ; 64: branch
        a15c: .byte   60 ; 65:   to 61
        a15d: .byte   00 ; 66: done
; eact_1e sets pc to here under some circumstances; see $9fbc.
        a15e: .byte   02 ; 67: store in $0298,x...
        a15f: .byte   03 ; 68:   ...03
        a160: .byte   20 ; 69: check for enemy-touches-player death
        a161: .byte   00 ; 6a: done
        a162: .byte   08 ; 6b: if --$0298,x then branch
        a163: .byte   68 ; 6c:   to 69
        a164: .byte   14 ; 6d: continue/end flip
        a165: .byte   1a ; 6e: if $010c == 0, branch
        a166: .byte   60 ; 6f:   to 61
        a167: .byte   06 ; 70: branch
        a168: .byte   65 ; 71:   to 66
; Pulsar entry point.
        a169: .byte   10 ; 72: pulsar_speed -> $0298,x
        a16a: .byte   b2 ; 73:   (value for previous)
        a16b: .byte   22 ; 74: do pulsar motion
        a16c: .byte   00 ; 75: done
        a16d: .byte   08 ; 76: if --$0298,x then branch
        a16e: .byte   73 ; 77:   to 74
        a16f: .byte   26 ; 78: check if pulsing
        a170: .byte   1a ; 79: if not pulsing, branch
        a171: .byte   7e ; 7a:   to 7f
        a172: .byte   22 ; 7b: do pulsar motion
        a173: .byte   00 ; 7c: done
        a174: .byte   06 ; 7d: branch
        a175: .byte   77 ; 7e:   to 78
        a176: .byte   24 ; 7f: enemy attract to player
        a177: .byte   12 ; 80: start flip
        a178: .byte   00 ; 81: done
        a179: .byte   14 ; 82: continue/end flip
        a17a: .byte   1a ; 83: if $010c == 0, branch
        a17b: .byte   71 ; 84:   to 72
        a17c: .byte   06 ; 85: branch
        a17d: .byte   80 ; 86:   to 81
; Flipper entry point: flip away from player, move four ticks, repeat.
; Move on every tick except those on which we start flips.
        a17e: .byte   24 ; 87: enemy attract to player
        a17f: .byte   16 ; 88: reverse enemy direction
        a180: .byte   12 ; 89: start flip
        a181: .byte   00 ; 8a: done
        a182: .byte   0c ; 8b: move per speed
        a183: .byte   14 ; 8c: continue/end flip
        a184: .byte   04 ; 8d: if $010c == 0, skip to 90
        a185: .byte   06 ; 8e: branch
        a186: .byte   89 ; 8f:   to 8a
        a187: .byte   02 ; 90: store in $0298,x...
        a188: .byte   04 ; 91:   ...04
        a189: .byte   00 ; 92: done
        a18a: .byte   0c ; 93: move per speed
        a18b: .byte   08 ; 94: if --$0298,x then branch
        a18c: .byte   91 ; 95:   to 92
        a18d: .byte   06 ; 96: branch
        a18e: .byte   86 ; 97:   to 87
; Handle shots
  move_shots: a2 0b     ldx     #$0b
        a191: 86 37     stx     $37
        a193: a6 37     ldx     $37
        a195: bd d3 02  lda     ply_shotpos,x
        a198: f0 45     beq     $a1df ; branch if this shot doesn't exist
        a19a: e0 08     cpx     #$08 ; enemy or friendly?
        a19c: b0 22     bcs     $a1c0 ; branch if enemy shot
; Friendly shot.  Move it down the tube.  $02f2, if set, appears to slow
; the shot down, presumably so it doesn't go off the back wall before it
; gets a chance to hit a spiker.
        a19e: 69 09     adc     #$09
        a1a0: bc f2 02  ldy     $02f2,x
        a1a3: f0 03     beq     $a1a8
        a1a5: 38        sec
        a1a6: e9 04     sbc     #$04
        a1a8: 9d d3 02  sta     ply_shotpos,x
; Check to see if it hit a spike.
        a1ab: 20 fa a1  jsr     $a1fa
        a1ae: bd d3 02  lda     ply_shotpos,x
        a1b1: c9 f0     cmp     #$f0
        a1b3: 90 08     bcc     $a1bd
; Shot went off back end of tube; destroy it
        a1b5: ce 35 01  dec     ply_shotcnt
        a1b8: a9 00     lda     #$00
        a1ba: 9d d3 02  sta     ply_shotpos,x
        a1bd: b8        clv
        a1be: 50 1f     bvc     $a1df
; Enemy shot
        a1c0: bd e6 02  lda     $02e6,x ; enm_shot_lsb-8
        a1c3: 18        clc
        a1c4: 6d 20 01  adc     enm_shotspd_lsb
        a1c7: 9d e6 02  sta     $02e6,x ; enm_shot_lsb-8
        a1ca: bd d3 02  lda     ply_shotpos,x
        a1cd: 6d 18 01  adc     enm_shotspd_msb
; Reached player's end of tube yet?
        a1d0: cd 02 02  cmp     player_along
        a1d3: b0 07     bcs     $a1dc
; Yes, at this end of tube
        a1d5: c6 a6     dec     enm_shotcnt
        a1d7: 20 e4 a1  jsr     $a1e4 ; check to see if hit player
        a1da: a9 00     lda     #$00
        a1dc: 9d d3 02  sta     ply_shotpos,x
; Next shot
        a1df: c6 37     dec     $37
        a1e1: 10 b0     bpl     $a193
        a1e3: 60        rts
; Called to see if enemy shot hit player.  Enemy shot number is in X,
; offset by 8 (which is why we see ply_shotseg,x here instead of the
; enm_shotseg,x we'd expect to).
        a1e4: ad 00 02  lda     player_seg
        a1e7: dd ad 02  cmp     ply_shotseg,x
        a1ea: d0 0d     bne     $a1f9
        a1ec: ad 01 02  lda     $0201
        a1ef: 30 08     bmi     $a1f9
        a1f1: 20 4b a3  jsr     $a34b
        a1f4: a9 81     lda     #$81
        a1f6: 8d 01 02  sta     $0201
        a1f9: 60        rts
; Called to see if player shot hit a spike.
        a1fa: bc ad 02  ldy     ply_shotseg,x
        a1fd: b9 ac 03  lda     spike_ht,y
        a200: f0 3c     beq     $a23e
        a202: bd d3 02  lda     ply_shotpos,x
        a205: d9 ac 03  cmp     spike_ht,y
        a208: 90 25     bcc     $a22f
        a20a: c9 f0     cmp     #$f0
        a20c: 90 02     bcc     $a210
        a20e: a9 00     lda     #$00
        a210: 99 ac 03  sta     spike_ht,y
        a213: fe f2 02  inc     $02f2,x
        a216: a9 c0     lda     #$c0
        a218: 99 9a 03  sta     $039a,y
        a21b: 20 f6 cc  jsr     $ccf6
        a21e: a2 ff     ldx     #$ff
        a220: a9 00     lda     #$00
        a222: 85 2a     sta     $2a
        a224: 85 2b     sta     $2b
        a226: a9 01     lda     #$01
        a228: 85 29     sta     $29
        a22a: 20 6c ca  jsr     inc_score
        a22d: a6 37     ldx     $37
        a22f: bd f2 02  lda     $02f2,x
        a232: c9 02     cmp     #$02
        a234: 90 08     bcc     $a23e
        a236: a9 00     lda     #$00
        a238: 9d d3 02  sta     ply_shotpos,x
        a23b: ce 35 01  dec     ply_shotcnt
        a23e: 60        rts
; Check to see if player fires?
 player_fire: ad 01 02  lda     $0201
        a242: 30 61     bmi     $a2a5
        a244: a5 05     lda     $05
        a246: 30 28     bmi     $a270
        a248: ad 06 01  lda     $0106
        a24b: 85 29     sta     $29
        a24d: a2 0a     ldx     #$0a
        a24f: bd db 02  lda     enm_shotpos,x
        a252: f0 14     beq     $a268
        a254: bd b5 02  lda     enm_shotseg,x
        a257: 38        sec
        a258: ed 00 02  sbc     player_seg
        a25b: 10 05     bpl     $a262
        a25d: 49 ff     eor     #$ff
        a25f: 18        clc
        a260: 69 01     adc     #$01
        a262: c9 02     cmp     #$02
        a264: b0 02     bcs     $a268
        a266: e6 29     inc     $29
        a268: ca        dex
        a269: 10 e4     bpl     $a24f
        a26b: a5 29     lda     $29
        a26d: b8        clv
        a26e: 50 04     bvc     $a274
        a270: a5 4d     lda     zap_fire_debounce
        a272: 29 10     and     #$10 ; fire
        a274: f0 2f     beq     $a2a5
        a276: a2 07     ldx     #$07
        a278: bd d3 02  lda     ply_shotpos,x
        a27b: d0 25     bne     $a2a2
        a27d: ee 35 01  inc     ply_shotcnt
        a280: ad 02 02  lda     player_along
        a283: 9d d3 02  sta     ply_shotpos,x
        a286: ad 00 02  lda     player_seg
        a289: 9d ad 02  sta     ply_shotseg,x
        a28c: ad 01 02  lda     $0201
        a28f: 9d c0 02  sta     $02c0,x
        a292: a9 00     lda     #$00
        a294: 9d f2 02  sta     $02f2,x
        a297: 20 ea cc  jsr     $ccea
        a29a: ad 02 02  lda     player_along
        a29d: 20 63 a4  jsr     $a463
        a2a0: a2 00     ldx     #$00
        a2a2: ca        dex
        a2a3: 10 d3     bpl     $a278
        a2a5: 60        rts
   enm_shoot: ad 01 02  lda     $0201
        a2a9: 30 58     bmi     $a303
        a2ab: a2 06     ldx     #$06
        a2ad: bd df 02  lda     enemy_along,x
        a2b0: f0 4e     beq     $a300
        a2b2: c9 30     cmp     #$30
        a2b4: 90 4a     bcc     $a300
        a2b6: bd 8a 02  lda     $028a,x
        a2b9: 29 40     and     #$40
        a2bb: f0 43     beq     $a300
        a2bd: de a6 02  dec     shot_delay,x
        a2c0: 10 3e     bpl     $a300
        a2c2: fe a6 02  inc     shot_delay,x
        a2c5: bd 83 02  lda     $0283,x
        a2c8: 29 80     and     #$80
        a2ca: d0 34     bne     $a300
        a2cc: ad ca 60  lda     pokey1_rand
        a2cf: a4 a6     ldy     enm_shotcnt
        a2d1: d9 04 a3  cmp     $a304,y
        a2d4: 90 2a     bcc     $a300
        a2d6: ac 1a 01  ldy     enm_shotmax
        a2d9: b9 db 02  lda     enm_shotpos,y
        a2dc: d0 1f     bne     $a2fd
        a2de: bd df 02  lda     enemy_along,x
        a2e1: 99 db 02  sta     enm_shotpos,y
        a2e4: bd b9 02  lda     enemy_seg,x
        a2e7: 99 b5 02  sta     enm_shotseg,y
        a2ea: bd cc 02  lda     $02cc,x
        a2ed: 99 c8 02  sta     $02c8,y
        a2f0: ad 19 01  lda     shot_holdoff
        a2f3: 9d a6 02  sta     shot_delay,x
        a2f6: 20 bd cc  jsr     $ccbd
        a2f9: e6 a6     inc     enm_shotcnt
        a2fb: a0 00     ldy     #$00
        a2fd: 88        dey
        a2fe: 10 d9     bpl     $a2d9
        a300: ca        dex
        a301: 10 aa     bpl     $a2ad
        a303: 60        rts
; Chance of a new shot, indexed by number of existing shots.  See $a2cc.
        a304: .byte   00
        a305: .byte   e0
        a306: .byte   f0
        a307: .byte   fa
        a308: .byte   ff
        a309: 86 37     stx     $37
        a30b: a9 ff     lda     #$ff
        a30d: 9d f2 02  sta     $02f2,x
        a310: 98        tya
        a311: 38        sec
        a312: e9 04     sbc     #$04
        a314: a8        tay
        a315: b9 b9 02  lda     enemy_seg,y
        a318: 85 2d     sta     $2d
        a31a: ad da 60  lda     pokey2_rand
        a31d: 29 07     and     #$07
        a31f: c9 03     cmp     #$03
        a321: 90 02     bcc     $a325
        a323: a9 00     lda     #$00
        a325: 48        pha
        a326: 18        clc
        a327: 69 02     adc     #$02
        a329: 20 ca a3  jsr     $a3ca
        a32c: 20 6f a0  jsr     $a06f
        a32f: 68        pla
        a330: 18        clc
        a331: 69 05     adc     #$05
        a333: aa        tax
        a334: 20 6c ca  jsr     inc_score
        a337: a6 37     ldx     $37
        a339: 60        rts
        a33a: a9 05     lda     #$05
        a33c: 20 52 a3  jsr     $a352
        a33f: ce 01 02  dec     $0201
        a342: 60        rts
        a343: a9 09     lda     #$09
        a345: d0 06     bne     $a34d
pieces_death: a9 07     lda     #$07
        a349: d0 02     bne     $a34d
        a34b: a9 ff     lda     #$ff
        a34d: 8d 3b 01  sta     $013b
        a350: a9 01     lda     #$01
        a352: 85 2c     sta     $2c
        a354: ad 02 02  lda     player_along
        a357: 85 29     sta     $29
        a359: ad 00 02  lda     player_seg
        a35c: 85 2d     sta     $2d
        a35e: 20 b0 cc  jsr     $ccb0
        a361: 20 d6 a3  jsr     $a3d6
        a364: a9 81     lda     #$81
        a366: 8d 01 02  sta     $0201
        a369: a9 01     lda     #$01
        a36b: 8d 3c 01  sta     $013c
        a36e: 60        rts
        a36f: 20 c1 cc  jsr     $ccc1
        a372: b9 db 02  lda     enm_shotpos,y
        a375: 85 29     sta     $29
        a377: b9 b5 02  lda     enm_shotseg,y
        a37a: 85 2d     sta     $2d
        a37c: a9 00     lda     #$00
        a37e: 20 d4 a3  jsr     $a3d4
        a381: a9 00     lda     #$00
        a383: 99 db 02  sta     enm_shotpos,y
        a386: c6 a6     dec     enm_shotcnt
        a388: a9 ff     lda     #$ff
        a38a: 9d f2 02  sta     $02f2,x
        a38d: 60        rts
        a38e: a9 ff     lda     #$ff
        a390: 9d f2 02  sta     $02f2,x
        a393: 98        tya
        a394: 38        sec
        a395: e9 04     sbc     #$04
        a397: a8        tay
; Kill enemy?  (Guess based on superzapper.)
        a398: b9 83 02  lda     $0283,y
        a39b: 29 c0     and     #$c0
        a39d: c9 c0     cmp     #$c0
        a39f: f0 06     beq     $a3a7
        a3a1: b9 b9 02  lda     enemy_seg,y
        a3a4: b8        clv
        a3a5: 50 08     bvc     $a3af
        a3a7: b9 b9 02  lda     enemy_seg,y
        a3aa: 38        sec
        a3ab: e9 01     sbc     #$01
        a3ad: 29 0f     and     #$0f
        a3af: 85 2d     sta     $2d
        a3b1: a9 00     lda     #$00
        a3b3: 20 ca a3  jsr     $a3ca
        a3b6: 20 6f a0  jsr     $a06f
        a3b9: b9 83 02  lda     $0283,y
        a3bc: 29 07     and     #$07
        a3be: a8        tay
        a3bf: be c5 a3  ldx     $a3c5,y
        a3c2: 4c 6c ca  jmp     inc_score
        a3c5: .byte   01
        a3c6: .byte   02
        a3c7: .byte   03
        a3c8: .byte   04
        a3c9: .byte   01
        a3ca: 48        pha
        a3cb: 20 c1 cc  jsr     $ccc1
        a3ce: b9 df 02  lda     enemy_along,y
        a3d1: 85 29     sta     $29
        a3d3: 68        pla
        a3d4: 85 2c     sta     $2c
        a3d6: 86 35     stx     $35
        a3d8: 84 36     sty     $36
        a3da: a9 00     lda     #$00
        a3dc: 85 2a     sta     $2a
        a3de: 85 2b     sta     $2b
        a3e0: a2 07     ldx     #$07
        a3e2: bd 0a 03  lda     $030a,x
        a3e5: f0 13     beq     $a3fa
        a3e7: bd 12 03  lda     $0312,x
        a3ea: c5 2a     cmp     $2a
        a3ec: 90 04     bcc     $a3f2
        a3ee: 85 2a     sta     $2a
        a3f0: 86 2b     stx     $2b
        a3f2: ca        dex
        a3f3: 10 ed     bpl     $a3e2
        a3f5: ce 16 01  dec     $0116
        a3f8: a6 2b     ldx     $2b
        a3fa: a9 00     lda     #$00
        a3fc: 9d 12 03  sta     $0312,x
        a3ff: a5 2c     lda     $2c
        a401: 9d 02 03  sta     $0302,x
        a404: a5 29     lda     $29
        a406: 9d 0a 03  sta     $030a,x
        a409: a5 2d     lda     $2d
        a40b: 9d fa 02  sta     $02fa,x
        a40e: ee 16 01  inc     $0116
        a411: a6 35     ldx     $35
        a413: a4 36     ldy     $36
        a415: 60        rts
        a416: ad 16 01  lda     $0116
        a419: f0 2c     beq     $a447
        a41b: a9 00     lda     #$00
        a41d: 8d 16 01  sta     $0116
        a420: a2 07     ldx     #$07
        a422: bd 0a 03  lda     $030a,x
        a425: f0 1d     beq     $a444
        a427: bd 12 03  lda     $0312,x
        a42a: bc 02 03  ldy     $0302,x
        a42d: 18        clc
        a42e: 79 4e a4  adc     $a44e,y
        a431: 9d 12 03  sta     $0312,x
        a434: d9 48 a4  cmp     $a448,y
        a437: 90 08     bcc     $a441
        a439: a9 00     lda     #$00
        a43b: 9d 0a 03  sta     $030a,x
        a43e: b8        clv
        a43f: 50 03     bvc     $a444
        a441: ee 16 01  inc     $0116
        a444: ca        dex
        a445: 10 db     bpl     $a422
        a447: 60        rts
        a448: .byte   10
        a449: .byte   15
        a44a: .byte   20
        a44b: .byte   20
        a44c: .byte   20
        a44d: .byte   10
        a44e: .byte   03
        a44f: .byte   01
        a450: .byte   03
        a451: .byte   03
        a452: .byte   03
        a453: .byte   03
; Check player shots to see if they hit anything.
   pshot_hit: a2 07     ldx     #$07
        a456: bd d3 02  lda     ply_shotpos,x
        a459: f0 03     beq     $a45e
        a45b: 20 63 a4  jsr     $a463
        a45e: ca        dex
        a45f: 10 f5     bpl     $a456
        a461: 60        rts
        a462: .byte   ab
; Check to see if a player shot hit an enemy or enemy shot.  X is player
; shot number, A is player shot position.
        a463: 85 2e     sta     $2e
        a465: a0 0a     ldy     #$0a ; check enemies as well as their shots
        a467: b9 db 02  lda     enm_shotpos,y
        a46a: f0 7f     beq     $a4eb
        a46c: c5 2e     cmp     $2e
        a46e: 90 05     bcc     $a475
        a470: e5 2e     sbc     $2e
        a472: b8        clv
        a473: 50 06     bvc     $a47b
        a475: a5 2e     lda     $2e
        a477: 38        sec
        a478: f9 db 02  sbc     enm_shotpos,y
        a47b: c0 04     cpy     #$04 ; enemy, or enemy shot?
        a47d: b0 12     bcs     $a491
        a47f: c5 a7     cmp     $a7
        a481: b0 0b     bcs     $a48e
        a483: b9 b5 02  lda     enm_shotseg,y
        a486: 5d ad 02  eor     ply_shotseg,x
        a489: d0 03     bne     $a48e
        a48b: 20 6f a3  jsr     $a36f
        a48e: b8        clv
        a48f: 50 5a     bvc     $a4eb
        a491: 48        pha
        a492: 84 38     sty     $38
        a494: b9 7f 02  lda     $027f,y ; $0283 - 4
        a497: 29 07     and     #$07
        a499: a8        tay
        a49a: 68        pla
        a49b: d9 51 01  cmp     hit_tol,y
        a49e: b0 49     bcs     $a4e9
        a4a0: c0 04     cpy     #$04 ; fuzzball
        a4a2: d0 1d     bne     $a4c1
        a4a4: a4 38     ldy     $38
        a4a6: b9 db 02  lda     enm_shotpos,y ; enemy_along - 4
        a4a9: cd 02 02  cmp     player_along
        a4ac: f0 10     beq     $a4be
        a4ae: bd ad 02  lda     ply_shotseg,x
        a4b1: d9 b5 02  cmp     enm_shotseg,y ; enemy_seg - 4
        a4b4: d0 08     bne     $a4be
        a4b6: b9 c8 02  lda     $02c8,y ; $02cc - 4
        a4b9: 10 03     bpl     $a4be
        a4bb: 20 09 a3  jsr     $a309
        a4be: b8        clv
        a4bf: 50 28     bvc     $a4e9
        a4c1: a4 38     ldy     $38
        a4c3: b9 c8 02  lda     $02c8,y ; $02cc - 4
        a4c6: 10 0a     bpl     $a4d2
        a4c8: b9 b5 02  lda     enm_shotseg,y ; enemy_seg - 4
        a4cb: dd c0 02  cmp     $02c0,x
        a4ce: f0 12     beq     $a4e2
        a4d0: d0 08     bne     $a4da
        a4d2: b9 db 02  lda     enm_shotpos,y ; enemy_along - 4
        a4d5: cd 02 02  cmp     player_along
        a4d8: f0 0f     beq     $a4e9
        a4da: b9 b5 02  lda     enm_shotseg,y ; enemy_seg - 4
        a4dd: dd ad 02  cmp     ply_shotseg,x
        a4e0: d0 07     bne     $a4e9
        a4e2: 86 37     stx     $37
        a4e4: 20 8e a3  jsr     $a38e
        a4e7: a6 37     ldx     $37
        a4e9: a4 38     ldy     $38
        a4eb: 88        dey
        a4ec: 30 03     bmi     $a4f1
        a4ee: 4c 67 a4  jmp     $a467
        a4f1: bd f2 02  lda     $02f2,x
        a4f4: c9 ff     cmp     #$ff
        a4f6: d0 0b     bne     $a503
        a4f8: a9 00     lda     #$00
        a4fa: 9d d3 02  sta     ply_shotpos,x
        a4fd: ce 35 01  dec     ply_shotcnt
        a500: 9d f2 02  sta     $02f2,x
        a503: 60        rts
        a504: ad 01 02  lda     $0201
        a507: 10 78     bpl     $a581
        a509: ad 35 01  lda     ply_shotcnt
        a50c: 05 a6     ora     enm_shotcnt
        a50e: 0d 16 01  ora     $0116
        a511: d0 6b     bne     $a57e
        a513: ae 1c 01  ldx     max_enm
        a516: bd df 02  lda     enemy_along,x
        a519: f0 0e     beq     $a529
        a51b: 18        clc
        a51c: 69 0f     adc     #$0f
        a51e: b0 02     bcs     $a522
        a520: c9 f0     cmp     #$f0
        a522: 90 02     bcc     $a526
        a524: a9 00     lda     #$00
        a526: 9d df 02  sta     enemy_along,x
        a529: ca        dex
        a52a: 10 ea     bpl     $a516
        a52c: a6 3d     ldx     curplayer
        a52e: b5 48     lda     p1_lives,x
        a530: c9 01     cmp     #$01
        a532: d0 20     bne     $a554
        a534: a9 00     lda     #$00
        a536: 8d 0f 01  sta     $010f
        a539: a9 01     lda     #$01
        a53b: 8d 14 01  sta     $0114
        a53e: a5 5f     lda     $5f
        a540: 38        sec
        a541: e9 20     sbc     #$20
        a543: 85 5f     sta     $5f
        a545: a5 5b     lda     $5b
        a547: e9 00     sbc     #$00
        a549: 85 5b     sta     $5b
        a54b: c9 fa     cmp     #$fa
        a54d: 18        clc
        a54e: d0 01     bne     $a551
        a550: 38        sec
        a551: b8        clv
        a552: 50 0d     bvc     $a561
        a554: ad 02 02  lda     player_along
        a557: 18        clc
        a558: 69 0f     adc     #$0f
        a55a: 8d 02 02  sta     player_along
        a55d: b0 02     bcs     $a561
        a55f: c9 f0     cmp     #$f0
        a561: 90 1b     bcc     $a57e
        a563: a9 06     lda     #$06
        a565: 85 00     sta     gamestate
        a567: 20 8f 92  jsr     clear_shots
        a56a: ad 08 01  lda     enemies_in
        a56d: 18        clc
        a56e: 6d 09 01  adc     enemies_top
        a571: 18        clc
        a572: 6d ab 03  adc     enemies_pending
        a575: c9 3f     cmp     #$3f
        a577: 90 02     bcc     $a57b
        a579: a9 3f     lda     #$3f
        a57b: 8d ab 03  sta     enemies_pending
        a57e: b8        clv
        a57f: 50 49     bvc     $a5ca
; Apparent anti-piracy provision.  If either checksum of the video RAM that
; holds the copyright message is wrong, and the P1 score is 17xxxx,
; increment one byte of page zero, based on the low two digits of the score.
; See also $b1df and $b27d.
        a581: ad 55 04  lda     copyr_vid_cksum2
        a584: 0d 1b 01  ora     copyr_vid_cksum1
        a587: f0 0a     beq     $a593
        a589: a9 17     lda     #$17
        a58b: c5 42     cmp     p1_score_h
        a58d: b0 04     bcs     $a593
        a58f: a6 40     ldx     p1_score_l
        a591: f6 00     inc     gamestate,x
; End apparent anti-piracy code
        a593: ad 06 01  lda     $0106
        a596: d0 32     bne     $a5ca
        a598: ad ab 03  lda     enemies_pending
        a59b: 0d 16 01  ora     $0116
        a59e: d0 15     bne     $a5b5
        a5a0: ac 1c 01  ldy     max_enm
        a5a3: b9 df 02  lda     enemy_along,y
        a5a6: f0 04     beq     $a5ac
        a5a8: c9 11     cmp     #$11
        a5aa: b0 09     bcs     $a5b5
        a5ac: 88        dey
        a5ad: 10 f4     bpl     $a5a3
        a5af: 20 cb a5  jsr     $a5cb
        a5b2: 20 8f 92  jsr     clear_shots
        a5b5: a5 4d     lda     zap_fire_debounce
        a5b7: 29 60     and     #$60 ; start1, start2
        a5b9: f0 0f     beq     $a5ca
        a5bb: 24 05     bit     $05
        a5bd: 10 0b     bpl     $a5ca
        a5bf: a5 09     lda     coinage_shadow
        a5c1: 29 43     and     #$43
        a5c3: c9 40     cmp     #$40
        a5c5: d0 03     bne     $a5ca
        a5c7: 20 cb a5  jsr     $a5cb
        a5ca: 60        rts
; Level "over"; start zooming down tube.
        a5cb: a9 20     lda     #$20
        a5cd: 85 00     sta     gamestate
        a5cf: ad 06 01  lda     $0106
        a5d2: 09 80     ora     #$80
        a5d4: 8d 06 01  sta     $0106
        a5d7: a9 00     lda     #$00
        a5d9: 8d 04 01  sta     zoomspd_lsb
        a5dc: 8d 07 01  sta     along_lsb
        a5df: 85 5c     sta     $5c
        a5e1: 8d 23 01  sta     $0123
        a5e4: a9 02     lda     #$02
        a5e6: 8d 05 01  sta     zoomspd_msb
; Check to see if there are any spikes of nonzero height.
        a5e9: a2 0f     ldx     #$0f
        a5eb: bd ac 03  lda     spike_ht,x
        a5ee: f0 03     beq     $a5f3
        a5f0: ee 23 01  inc     $0123
        a5f3: ca        dex
        a5f4: 10 f5     bpl     $a5eb
        a5f6: ad 23 01  lda     $0123
        a5f9: f0 17     beq     $a612
; If there are any spikes, check level.
        a5fb: a5 9f     lda     curlevel
        a5fd: c9 07     cmp     #$07
        a5ff: b0 11     bcs     $a612
; If level is low enough and there are spikes, display "AVOID SPIKES".
        a601: a9 1e     lda     #$1e ; time delay
        a603: 85 04     sta     $04
        a605: a9 0a     lda     #$0a
        a607: 85 00     sta     gamestate
        a609: a9 20     lda     #$20 ; new gamestate
        a60b: 85 02     sta     $02
        a60d: a9 80     lda     #$80
        a60f: 8d 23 01  sta     $0123
        a612: a9 ff     lda     #$ff
        a614: 8d 25 01  sta     zap_running
        a617: 60        rts
    state_24: ad 0e 01  lda     $010e
        a61b: 8d 0d 01  sta     $010d
        a61e: a2 0f     ldx     #$0f
        a620: 86 37     stx     $37
        a622: a6 37     ldx     $37
        a624: bd 83 02  lda     $0283,x
        a627: d0 0b     bne     $a634
        a629: ad 0e 01  lda     $010e
        a62c: f0 03     beq     $a631
        a62e: 20 5b a6  jsr     $a65b
        a631: b8        clv
        a632: 50 0b     bvc     $a63f
        a634: 20 a9 a6  jsr     $a6a9
        a637: 20 21 a7  jsr     $a721
        a63a: a9 ff     lda     #$ff
        a63c: 8d 0d 01  sta     $010d
        a63f: c6 37     dec     $37
        a641: 10 df     bpl     $a622
        a643: a5 03     lda     timectr
        a645: 29 01     and     #$01
        a647: d0 08     bne     $a651
        a649: ad 0e 01  lda     $010e
        a64c: f0 03     beq     $a651
        a64e: ce 0e 01  dec     $010e
        a651: ad 0d 01  lda     $010d
        a654: d0 04     bne     $a65a
        a656: a9 12     lda     #$12
        a658: 85 00     sta     gamestate
        a65a: 60        rts
        a65b: a5 03     lda     timectr
        a65d: 29 00     and     #$00
        a65f: d0 39     bne     $a69a
        a661: a9 80     lda     #$80
        a663: 9d 63 02  sta     $0263,x
        a666: 9d 83 02  sta     $0283,x
        a669: 9d a3 02  sta     $02a3,x
        a66c: ad da 60  lda     pokey2_rand
        a66f: 9d c3 02  sta     $02c3,x
        a672: 20 9b a6  jsr     plusminus_7
        a675: 9d 23 03  sta     $0323,x
        a678: ad ca 60  lda     pokey1_rand
        a67b: 9d e3 02  sta     $02e3,x
        a67e: 20 9b a6  jsr     plusminus_7
; Why this rigamarole instead of just "lda #$00" or "and #$fe" before
; calling plusminus_7, I have no idea.
        a681: 30 05     bmi     $a688
        a683: 49 ff     eor     #$ff
        a685: 18        clc
        a686: 69 01     adc     #$01
        a688: 9d 43 03  sta     $0343,x
        a68b: ad ca 60  lda     pokey1_rand
        a68e: 9d 03 03  sta     $0303,x
        a691: 20 9b a6  jsr     plusminus_7
        a694: 9d 63 03  sta     $0363,x
        a697: 20 c1 cc  jsr     $ccc1
        a69a: 60        rts
; Return with a random number in A, from 00-07 (if input A low bit is clear)
; or $f9-$00 (if input A low bit is set).
 plusminus_7: 4a        lsr     a
        a69c: ad da 60  lda     pokey2_rand
        a69f: 29 07     and     #$07
        a6a1: 90 05     bcc     $a6a8
        a6a3: 49 ff     eor     #$ff
        a6a5: 18        clc
        a6a6: 69 01     adc     #$01
        a6a8: 60        rts
        a6a9: bd e3 02  lda     $02e3,x
        a6ac: 18        clc
        a6ad: 7d 23 02  adc     $0223,x
        a6b0: 9d 23 02  sta     $0223,x
        a6b3: bd 43 03  lda     $0343,x
        a6b6: 30 0c     bmi     $a6c4
        a6b8: 7d 83 02  adc     $0283,x
        a6bb: c9 f0     cmp     #$f0
        a6bd: 90 02     bcc     $a6c1
        a6bf: a9 00     lda     #$00
        a6c1: b8        clv
        a6c2: 50 09     bvc     $a6cd
        a6c4: 7d 83 02  adc     $0283,x
        a6c7: c9 10     cmp     #$10
        a6c9: b0 02     bcs     $a6cd
        a6cb: a9 00     lda     #$00
        a6cd: a8        tay
        a6ce: bd c3 02  lda     $02c3,x
        a6d1: 18        clc
        a6d2: 7d 03 02  adc     pending_seg,x
        a6d5: 9d 03 02  sta     pending_seg,x
        a6d8: bd 23 03  lda     $0323,x
        a6db: 30 0c     bmi     $a6e9
        a6dd: 7d 63 02  adc     $0263,x
        a6e0: c9 f0     cmp     #$f0
        a6e2: 90 02     bcc     $a6e6
        a6e4: a0 00     ldy     #$00
        a6e6: b8        clv
        a6e7: 50 09     bvc     $a6f2
        a6e9: 7d 63 02  adc     $0263,x
        a6ec: c9 10     cmp     #$10
        a6ee: b0 02     bcs     $a6f2
        a6f0: a0 00     ldy     #$00
        a6f2: 9d 63 02  sta     $0263,x
        a6f5: bd 03 03  lda     $0303,x
        a6f8: 18        clc
        a6f9: 7d 43 02  adc     pending_vid,x
        a6fc: 9d 43 02  sta     pending_vid,x
        a6ff: bd 63 03  lda     $0363,x
        a702: 30 0c     bmi     $a710
        a704: 7d a3 02  adc     $02a3,x
        a707: c9 f0     cmp     #$f0
        a709: 90 02     bcc     $a70d
        a70b: a0 00     ldy     #$00
        a70d: b8        clv
        a70e: 50 09     bvc     $a719
        a710: 7d a3 02  adc     $02a3,x
        a713: c9 10     cmp     #$10
        a715: b0 02     bcs     $a719
        a717: a0 00     ldy     #$00
        a719: 9d a3 02  sta     $02a3,x
        a71c: 98        tya
        a71d: 9d 83 02  sta     $0283,x
        a720: 60        rts
        a721: a9 fd     lda     #$fd
        a723: 85 29     sta     $29
        a725: bd c3 02  lda     $02c3,x
        a728: bc 23 03  ldy     $0323,x
        a72b: 20 5d a7  jsr     $a75d
        a72e: 9d c3 02  sta     $02c3,x
        a731: 98        tya
        a732: 9d 23 03  sta     $0323,x
        a735: bd e3 02  lda     $02e3,x
        a738: bc 43 03  ldy     $0343,x
        a73b: 20 5d a7  jsr     $a75d
        a73e: 9d e3 02  sta     $02e3,x
        a741: 98        tya
        a742: 9d 43 03  sta     $0343,x
        a745: bd 03 03  lda     $0303,x
        a748: bc 63 03  ldy     $0363,x
        a74b: 20 5d a7  jsr     $a75d
        a74e: 9d 03 03  sta     $0303,x
        a751: 98        tya
        a752: 9d 63 03  sta     $0363,x
        a755: a5 29     lda     $29
        a757: d0 03     bne     $a75c
        a759: 9d 83 02  sta     $0283,x
        a75c: 60        rts
        a75d: 84 2b     sty     $2b
        a75f: 24 2b     bit     $2b
        a761: 30 0f     bmi     $a772
        a763: 38        sec
        a764: ed 88 a7  sbc     $a788
        a767: 85 2a     sta     $2a
        a769: a5 2b     lda     $2b
        a76b: e9 00     sbc     #$00
        a76d: 90 0f     bcc     $a77e
        a76f: b8        clv
        a770: 50 12     bvc     $a784
        a772: 18        clc
        a773: 6d 88 a7  adc     $a788
        a776: 85 2a     sta     $2a
        a778: a5 2b     lda     $2b
        a77a: 69 00     adc     #$00
        a77c: 90 06     bcc     $a784
        a77e: e6 29     inc     $29
        a780: a9 00     lda     #$00
        a782: 85 2a     sta     $2a
        a784: a8        tay
        a785: a5 2a     lda     $2a
        a787: 60        rts
        a788: 20 a2 0f  jsr     $0fa2
        a78b: a9 00     lda     #$00
        a78d: 9d 83 02  sta     $0283,x
        a790: ca        dex
        a791: 10 f8     bpl     $a78b
        a793: a9 20     lda     #$20
        a795: 8d 0e 01  sta     $010e
        a798: 8d 0d 01  sta     $010d
        a79b: a9 04     lda     #$04
        a79d: 85 01     sta     $01
        a79f: a9 00     lda     #$00
        a7a1: 85 68     sta     $68
        a7a3: 85 69     sta     $69
        a7a5: 60        rts
; Subtract Y from A, returning (in A) the signed difference.  If the level
; is closed, do wraparound processing; if open, don't.
        a7a6: 84 2a     sty     $2a
        a7a8: 38        sec
        a7a9: e5 2a     sbc     $2a
        a7ab: 85 2a     sta     $2a
        a7ad: 2c 11 01  bit     open_level
        a7b0: 30 09     bmi     $a7bb
        a7b2: 29 0f     and     #$0f
        a7b4: 2c bc a7  bit     $a7bc
        a7b7: f0 02     beq     $a7bb
        a7b9: 09 f8     ora     #$f8
        a7bb: 60        rts
        a7bc: .byte   08
        a7bd: a2 07     ldx     #$07
        a7bf: a9 00     lda     #$00
        a7c1: 9d fe 03  sta     $03fe,x
        a7c4: ca        dex
        a7c5: 10 fa     bpl     $a7c1
        a7c7: a9 f0     lda     #$f0
        a7c9: 8d 05 04  sta     $0405
        a7cc: a9 ff     lda     #$ff
        a7ce: 8d 15 01  sta     $0115
        a7d1: 60        rts
        a7d2: ad 15 01  lda     $0115
        a7d5: f0 59     beq     $a830
        a7d7: a9 00     lda     #$00
        a7d9: 85 29     sta     $29
; There appears to be a loop beginning here
; for ($37=7;$37>=0;$37--) running from here through a827.  I'm not sure
; just what goes on inside it, yet, though.
        a7db: a2 07     ldx     #$07
        a7dd: 86 37     stx     $37
        a7df: a6 37     ldx     $37
        a7e1: bd fe 03  lda     $03fe,x
        a7e4: f0 18     beq     $a7fe
        a7e6: 38        sec
        a7e7: e9 07     sbc     #$07
        a7e9: 90 02     bcc     $a7ed
        a7eb: c9 10     cmp     #$10
        a7ed: b0 0c     bcs     $a7fb
        a7ef: ac 15 01  ldy     $0115
        a7f2: 10 05     bpl     $a7f9
        a7f4: a9 f0     lda     #$f0
        a7f6: b8        clv
        a7f7: 50 02     bvc     $a7fb
        a7f9: a9 00     lda     #$00
        a7fb: b8        clv
        a7fc: 50 20     bvc     $a81e
        a7fe: ac 15 01  ldy     $0115
        a801: 10 1b     bpl     $a81e
        a803: 8a        txa
        a804: 18        clc
        a805: 69 01     adc     #$01
        a807: c9 08     cmp     #$08
        a809: 90 02     bcc     $a80d
        a80b: a9 00     lda     #$00
        a80d: a8        tay
        a80e: b9 fe 03  lda     $03fe,y
        a811: f0 0b     beq     $a81e
        a813: c9 d5     cmp     #$d5
        a815: b0 05     bcs     $a81c
        a817: a9 f0     lda     #$f0
        a819: b8        clv
        a81a: 50 02     bvc     $a81e
        a81c: a9 00     lda     #$00
        a81e: 9d fe 03  sta     $03fe,x
        a821: 05 29     ora     $29
        a823: 85 29     sta     $29
        a825: c6 37     dec     $37
        a827: 10 b6     bpl     $a7df
        a829: a5 29     lda     $29
        a82b: d0 03     bne     $a830
        a82d: 8d 15 01  sta     $0115
        a830: 60        rts
    reset_sz: a9 00     lda     #$00
        a833: 8d aa 03  sta     zap_uses
        a836: 8d 25 01  sta     zap_running
        a839: 60        rts
   check_zap: a5 05     lda     $05
        a83c: 10 3e     bpl     $a87c
        a83e: ad 25 01  lda     zap_running
        a841: d0 23     bne     $a866
        a843: ad 01 02  lda     $0201
        a846: 30 1b     bmi     $a863
        a848: a5 4e     lda     zap_fire_new
        a84a: 29 08     and     #$08 ; zap
        a84c: f0 15     beq     $a863
        a84e: ad aa 03  lda     zap_uses
        a851: c9 02     cmp     #$02
        a853: b0 08     bcs     $a85d
        a855: ee aa 03  inc     zap_uses
        a858: a9 01     lda     #$01
        a85a: 8d 25 01  sta     zap_running
        a85d: a5 4e     lda     zap_fire_new
        a85f: 29 77     and     #$77 ; clear zap
        a861: 85 4e     sta     zap_fire_new
        a863: b8        clv
        a864: 50 16     bvc     $a87c
        a866: ee 25 01  inc     zap_running
        a869: ae aa 03  ldx     zap_uses
        a86c: ad 25 01  lda     zap_running
        a86f: dd 83 a8  cmp     zap_length,x
        a872: 90 05     bcc     $a879
        a874: a9 00     lda     #$00
        a876: 8d 25 01  sta     zap_running
        a879: 20 88 a8  jsr     $a888
        a87c: a5 4e     lda     zap_fire_new
        a87e: 29 7f     and     #$7f
        a880: 85 4e     sta     zap_fire_new
        a882: 60        rts
  zap_length: .byte   00 ; Indexed by zapper use count
        a884: .byte   13
        a885: .byte   05
        a886: .byte   00
        a887: .byte   00
        a888: ad 25 01  lda     zap_running
        a88b: c9 03     cmp     #$03
        a88d: 90 14     bcc     $a8a3
        a88f: 29 01     and     #$01
        a891: d0 10     bne     $a8a3
        a893: ac 1c 01  ldy     max_enm
        a896: b9 df 02  lda     enemy_along,y
        a899: d0 09     bne     $a8a4
        a89b: 88        dey
        a89c: 10 f8     bpl     $a896
        a89e: a9 00     lda     #$00
        a8a0: 8d 25 01  sta     zap_running
        a8a3: 60        rts
        a8a4: b9 8a 02  lda     $028a,y
        a8a7: 29 fc     and     #$fc
        a8a9: 99 8a 02  sta     $028a,y
        a8ac: 4c 98 a3  jmp     $a398
        a8af: .byte   e1
coinage_msgs: .byte   24 ; FREE PLAY
        a8b1: .byte   26 ; 1 COIN 2 PLAYS
        a8b2: .byte   28 ; 1 COIN 1 PLAY
        a8b3: .byte   2a ; 2 COINS 1 PLAY
        a8b4: a9 01     lda     #$01
        a8b6: 85 72     sta     curscale
        a8b8: 20 6a df  jsr     vapp_scale_A,0
        a8bb: a0 05     ldy     #$05
        a8bd: 20 d1 b0  jsr     vapp_setcolor
        a8c0: a5 05     lda     $05
        a8c2: 30 26     bmi     $a8ea
        a8c4: a2 00     ldx     #$00 ; "GAME OVER"
        a8c6: a5 03     lda     timectr
        a8c8: 29 20     and     #$20
        a8ca: d0 0c     bne     $a8d8
        a8cc: a2 22     ldx     #$22 ; "INSERT COINS"
        a8ce: a5 06     lda     credits
        a8d0: f0 06     beq     $a8d8
        a8d2: 24 a2     bit     $a2
        a8d4: 30 02     bmi     $a8d8
        a8d6: a2 06     ldx     #$06 ; "PRESS START"
        a8d8: 20 14 ab  jsr     vapp_msg
        a8db: 20 0d ab  jsr     vapp_vcentre_1
        a8de: ad e4 31  lda     char_jsrtbl
        a8e1: 8d a6 2f  sta     $2fa6
        a8e4: 8d a8 2f  sta     $2fa8
        a8e7: 20 a8 aa  jsr     show_coin_stuff
        a8ea: a9 01     lda     #$01
        a8ec: a0 00     ldy     #$00
        a8ee: 20 7f a9  jsr     show_player_stuff
        a8f1: 24 05     bit     $05
        a8f3: 30 09     bmi     $a8fe
        a8f5: a5 43     lda     p2_score_l
        a8f7: 05 44     ora     p2_score_m
        a8f9: 05 45     ora     p2_score_h
        a8fb: b8        clv
        a8fc: 50 02     bvc     $a900
        a8fe: a5 3e     lda     twoplayer
        a900: f0 06     beq     $a908
        a902: a9 01     lda     #$01
        a904: a8        tay
        a905: 20 7f a9  jsr     show_player_stuff
        a908: a5 00     lda     gamestate
        a90a: c9 04     cmp     #$04
        a90c: f0 35     beq     $a943
        a90e: a9 1d     lda     #$1d
        a910: 85 3b     sta     $3b
        a912: a9 07     lda     #$07
        a914: 85 3c     sta     $3c
        a916: ae e4 cd  ldx     $cde4
        a919: 20 d7 a9  jsr     $a9d7
; Checksum the code which displays the copyright message; see $aace
; See also $c8f5.
        a91c: a0 0a     ldy     #$0a
        a91e: a9 a7     lda     #$a7
        a920: 59 ce aa  eor     $aace,y
        a923: 88        dey
        a924: 10 fa     bpl     $a920
        a926: 8d 6c 01  sta     copyr_disp_cksum1
        a929: ae e5 cd  ldx     $cde5
        a92c: a9 02     lda     #$02
        a92e: 85 38     sta     $38
        a930: a4 38     ldy     $38
        a932: b9 1b 06  lda     hs_initials_1,y
        a935: 0a        asl     a
        a936: a8        tay
        a937: b9 fa 31  lda     ltr_jsrtbl,y
        a93a: 9d 60 2f  sta     $2f60,x
        a93d: e8        inx
        a93e: e8        inx
        a93f: c6 38     dec     $38
        a941: 10 ed     bpl     $a930
        a943: a9 2f     lda     #$2f
        a945: a2 60     ldx     #$60
        a947: 20 39 df  jsr     vapp_vjsr_AX
        a94a: ad 23 01  lda     $0123
        a94d: 10 05     bpl     $a954
        a94f: a2 36     ldx     #$36 ; "AVOID SPIKES"
        a951: 20 14 ab  jsr     vapp_msg
        a954: a5 00     lda     gamestate
        a956: c9 18     cmp     #$18
        a958: d0 22     bne     $a97c
        a95a: a5 05     lda     $05
        a95c: 10 1e     bpl     $a97c
        a95e: a6 3d     ldx     curplayer
        a960: bd 02 01  lda     p1_startchoice,x
        a963: f0 0d     beq     $a972
        a965: a2 30     ldx     #$30 ; "BONUS "
        a967: 20 14 ab  jsr     vapp_msg
        a96a: a4 3d     ldy     curplayer
        a96c: be 02 01  ldx     p1_startchoice,y
        a96f: 20 c6 b0  jsr     vapp_startbonus
        a972: a2 3a     ldx     #$3a ; "SUPERZAPPER RECHARGE"
        a974: 20 14 ab  jsr     vapp_msg
        a977: a2 38     ldx     #$38 ; "LEVEL"
        a979: 20 14 ab  jsr     vapp_msg
        a97c: 60        rts
; Indexed by player number; see $a9ce
        a97d: .byte   42
        a97e: .byte   45
; On entry, A=1 and Y=player number
show_player_stuff: a6 00     ldx     gamestate
        a981: e0 04     cpx     #$04
        a983: 84 2b     sty     $2b
        a985: c4 3d     cpy     curplayer
        a987: d0 06     bne     $a98f
        a989: 24 05     bit     $05
        a98b: 10 02     bpl     $a98f
        a98d: a9 00     lda     #$00
        a98f: 09 70     ora     #$70
        a991: be de cd  ldx     $cdde,y
        a994: 9d 60 2f  sta     $2f60,x
        a997: be e0 cd  ldx     $cde0,y
        a99a: b9 48 00  lda     p1_lives,y
        a99d: 85 38     sta     $38
        a99f: f0 06     beq     $a9a7
        a9a1: c4 3d     cpy     curplayer
        a9a3: d0 02     bne     $a9a7
        a9a5: c6 38     dec     $38
        a9a7: a0 01     ldy     #$01
        a9a9: ad 84 32  lda     $3284
        a9ac: c4 38     cpy     $38
        a9ae: 90 05     bcc     $a9b5
        a9b0: f0 03     beq     $a9b5
        a9b2: ad 86 32  lda     $3286
        a9b5: 9d 60 2f  sta     $2f60,x
        a9b8: e8        inx
        a9b9: e8        inx
        a9ba: c8        iny
        a9bb: c0 07     cpy     #$07
        a9bd: 90 ea     bcc     $a9a9
        a9bf: a4 2b     ldy     $2b
        a9c1: a5 00     lda     gamestate
        a9c3: c9 04     cmp     #$04
        a9c5: d0 04     bne     $a9cb
        a9c7: c4 3d     cpy     curplayer
        a9c9: d0 30     bne     $a9fb
        a9cb: be e2 cd  ldx     $cde2,y
        a9ce: b9 7d a9  lda     $a97d,y
        a9d1: 85 3b     sta     $3b
        a9d3: a9 00     lda     #$00
        a9d5: 85 3c     sta     $3c
        a9d7: a0 02     ldy     #$02
        a9d9: 84 2a     sty     $2a
        a9db: 38        sec
        a9dc: 08        php
        a9dd: a0 00     ldy     #$00
        a9df: b1 3b     lda     ($3b),y
        a9e1: 4a        lsr     a
        a9e2: 4a        lsr     a
        a9e3: 4a        lsr     a
        a9e4: 4a        lsr     a
        a9e5: 28        plp
        a9e6: 20 fc a9  jsr     $a9fc
        a9e9: a5 2a     lda     $2a
        a9eb: d0 01     bne     $a9ee
        a9ed: 18        clc
        a9ee: a0 00     ldy     #$00
        a9f0: b1 3b     lda     ($3b),y
        a9f2: 20 fc a9  jsr     $a9fc
        a9f5: c6 3b     dec     $3b
        a9f7: c6 2a     dec     $2a
        a9f9: 10 e1     bpl     $a9dc
        a9fb: 60        rts
        a9fc: 29 0f     and     #$0f
        a9fe: a8        tay
        a9ff: f0 01     beq     $aa02
        aa01: 18        clc
        aa02: b0 01     bcs     $aa05
        aa04: c8        iny
        aa05: 08        php
        aa06: 98        tya
        aa07: 0a        asl     a
        aa08: a8        tay
        aa09: b9 e4 31  lda     char_jsrtbl,y
        aa0c: 9d 60 2f  sta     $2f60,x
        aa0f: e8        inx
        aa10: e8        inx
        aa11: 28        plp
        aa12: 60        rts
; Sets up the header for the text at the top of the screen.  Plugs in the
; level number, but none of the other variable pieces.
        aa13: a6 3e     ldx     twoplayer
        aa15: 24 05     bit     $05
        aa17: 30 0a     bmi     $aa23
        aa19: a5 43     lda     p2_score_l
        aa1b: 05 44     ora     p2_score_m
        aa1d: 05 45     ora     p2_score_h
        aa1f: f0 02     beq     $aa23
        aa21: a2 01     ldx     #$01
        aa23: a9 60     lda     #$60
        aa25: 85 74     sta     vidptr_l
        aa27: a9 2f     lda     #$2f
        aa29: 85 75     sta     vidptr_h
        aa2b: bd 66 ce  lda     $ce66,x
        aa2e: a8        tay
        aa2f: 38        sec
        aa30: 65 74     adc     vidptr_l
        aa32: 48        pha
        aa33: b9 e6 cd  lda     hdr_template,y
        aa36: 91 74     sta     (vidptr_l),y
        aa38: 88        dey
        aa39: d0 f8     bne     $aa33
        aa3b: b9 e6 cd  lda     hdr_template,y
        aa3e: 91 74     sta     (vidptr_l),y
        aa40: a5 05     lda     $05
        aa42: 10 10     bpl     $aa54
; 2fa6 holds the vjsr for the tens digit of the level number
        aa44: a9 2f     lda     #$2f
        aa46: 85 75     sta     vidptr_h
        aa48: a9 a6     lda     #$a6
        aa4a: 85 74     sta     vidptr_l
        aa4c: a5 9f     lda     curlevel
        aa4e: 18        clc
        aa4f: 69 01     adc     #$01
        aa51: 20 77 af  jsr     vapp_2dig_bin
        aa54: 68        pla
        aa55: 85 74     sta     vidptr_l
        aa57: 4c 09 df  jmp     vapp_rts
        aa5a: a2 08     ldx     #$08 ; "PLAY"
        aa5c: 20 14 ab  jsr     vapp_msg
        aa5f: 4c 69 aa  jmp     $aa69
        aa62: a9 30     lda     #$30
        aa64: a2 00     ldx     #$00 ; "GAME OVER"
        aa66: 20 17 ab  jsr     vapp_msg_at_y
        aa69: 20 92 aa  jsr     show_plyno
        aa6c: 4c e7 a8  jmp     $a8e7
        aa6f: 20 b4 a8  jsr     $a8b4
        aa72: a9 00     lda     #$00
        aa74: a2 06     ldx     #$06 ; "PRESS START"
        aa76: 4c 17 ab  jmp     vapp_msg_at_y
        aa79: a9 00     lda     #$00
        aa7b: a2 32     ldx     #$32 ; "2 CREDIT MINIMUM"
        aa7d: 20 17 ab  jsr     vapp_msg_at_y
        aa80: a5 03     lda     timectr
        aa82: 29 1f     and     #$1f
        aa84: c9 10     cmp     #$10
        aa86: b0 07     bcs     $aa8f
        aa88: a9 e0     lda     #$e0
        aa8a: a2 22     ldx     #$22 ; "INSERT COINS"
        aa8c: 20 17 ab  jsr     vapp_msg_at_y
        aa8f: 4c b4 a8  jmp     $a8b4
; show PLAYER and current player number
  show_plyno: a2 02     ldx     #$02 ; "PLAYER "
        aa94: 20 14 ab  jsr     vapp_msg
        aa97: a9 00     lda     #$00
        aa99: 20 dd b0  jsr     vapp_setscale
        aa9c: a6 3d     ldx     curplayer
        aa9e: e8        inx
        aa9f: 86 61     stx     $61
        aaa1: a9 61     lda     #$61
        aaa3: a0 01     ldy     #$01
        aaa5: 4c b1 df  jmp     vapp_multdig_y@a
show_coin_stuff: a5 09     lda     coinage_shadow
        aaaa: 29 03     and     #$03 ; coinage
        aaac: aa        tax
        aaad: bd b0 a8  lda     coinage_msgs,x
        aab0: aa        tax
        aab1: 20 14 ab  jsr     vapp_msg
        aab4: ce 6e 01  dec     $016e
        aab7: a5 0a     lda     optsw2_shadow
        aab9: 29 01     and     #$01 ; 2-credit minimum
        aabb: f0 0e     beq     $aacb
        aabd: a5 03     lda     timectr
        aabf: 29 20     and     #$20 ; flash
        aac1: d0 08     bne     $aacb
        aac3: a2 32     ldx     #$32 ; "2 CREDIT MINIMUM"
        aac5: 20 14 ab  jsr     vapp_msg
        aac8: b8        clv
        aac9: 50 03     bvc     $aace
        aacb: 20 ca ae  jsr     $aeca
        aace: a2 2c     ldx     #$2c ; "(c) MCMLXXX ATARI"
        aad0: 20 14 ab  jsr     vapp_msg
        aad3: a2 2e     ldx     #$2e ; "CREDITS "
        aad5: 20 14 ab  jsr     vapp_msg
        aad8: a5 06     lda     credits
        aada: c9 28     cmp     #$28 ; 40
        aadc: 90 04     bcc     $aae2
        aade: a9 28     lda     #$28 ; 40
        aae0: 85 06     sta     credits
        aae2: 20 77 af  jsr     vapp_2dig_bin
        aae5: a5 17     lda     uncredited
        aae7: f0 09     beq     $aaf2
        aae9: ad f4 aa  lda     $aaf4
        aaec: ae f3 aa  ldx     $aaf3
        aaef: 20 39 df  jsr     vapp_vjsr_AX
        aaf2: 60        rts
        aaf3: .word   325c ; 1/2
; Converts number in accumulator (binary) to BCD, storing two-digit BCD
; in $29 (and leaving it in $2c) on return.  Discards the hundreds digit.
  bin_to_bcd: f8        sed
        aaf6: 85 29     sta     $29
        aaf8: a9 00     lda     #$00
        aafa: 85 2c     sta     $2c
        aafc: a0 07     ldy     #$07
        aafe: 06 29     asl     $29
        ab00: a5 2c     lda     $2c
        ab02: 65 2c     adc     $2c
        ab04: 85 2c     sta     $2c
        ab06: 88        dey
        ab07: 10 f5     bpl     $aafe
        ab09: d8        cld
        ab0a: 85 29     sta     $29
        ab0c: 60        rts
; $20 $80 = vcentre (why $20? who knows.)
vapp_vcentre_1: a9 20     lda     #$20
        ab0f: a2 80     ldx     #$80
        ab11: 4c 57 df  jmp     vapp_A_X_Y=0
    vapp_msg: bd 22 d1  lda     $d122,x
vapp_msg_at_y: 86 35     stx     $35
        ab19: 85 2b     sta     $2b
        ab1b: a4 35     ldy     $35
        ab1d: b1 ac     lda     (strtbl),y
        ab1f: 85 3b     sta     $3b
        ab21: c8        iny
        ab22: b1 ac     lda     (strtbl),y
        ab24: 85 3c     sta     $3c
; If we're displaying the copyright message, save the location in video RAM
; where it's displayed, for the checksumming code at $b1df and $b27d.
        ab26: e0 2c     cpx     #$2c ; "(c) MCMLXXX ATARI"
        ab28: d0 08     bne     $ab32
        ab2a: a5 74     lda     vidptr_l
        ab2c: 85 b6     sta     copyr_vid_loc
        ab2e: a5 75     lda     vidptr_h
        ab30: 85 b7     sta     copyr_vid_loc+1
        ab32: a0 00     ldy     #$00
        ab34: b1 3b     lda     ($3b),y
        ab36: 85 2a     sta     $2a
        ab38: 20 0d ab  jsr     vapp_vcentre_1
        ab3b: a9 00     lda     #$00
        ab3d: 85 73     sta     draw_z
        ab3f: a9 01     lda     #$01
        ab41: 85 72     sta     curscale
        ab43: 20 6a df  jsr     vapp_scale_A,0
        ab46: a5 2a     lda     $2a
        ab48: a6 2b     ldx     $2b
        ab4a: 20 75 df  jsr     vapp_ldraw_A,X
        ab4d: a4 35     ldy     $35
        ab4f: b1 ac     lda     (strtbl),y
        ab51: 85 3b     sta     $3b
        ab53: c8        iny
        ab54: b1 ac     lda     (strtbl),y
        ab56: 85 3c     sta     $3c
        ab58: a6 35     ldx     $35
        ab5a: bd 21 d1  lda     $d121,x
        ab5d: 48        pha
        ab5e: 4a        lsr     a
        ab5f: 4a        lsr     a
        ab60: 4a        lsr     a
        ab61: 4a        lsr     a
        ab62: a8        tay
        ab63: 20 d1 b0  jsr     vapp_setcolor
        ab66: 68        pla
        ab67: 29 0f     and     #$0f
        ab69: 20 dd b0  jsr     vapp_setscale
        ab6c: a0 01     ldy     #$01
        ab6e: a9 00     lda     #$00
        ab70: 85 2a     sta     $2a
        ab72: b1 3b     lda     ($3b),y
        ab74: 85 2b     sta     $2b
        ab76: 29 7f     and     #$7f
        ab78: c8        iny
        ab79: 84 2c     sty     $2c
        ab7b: aa        tax
        ab7c: bd e4 31  lda     char_jsrtbl,x
        ab7f: a4 2a     ldy     $2a
        ab81: 91 74     sta     (vidptr_l),y
        ab83: c8        iny
        ab84: bd e5 31  lda     char_jsrtbl+1,x
        ab87: 91 74     sta     (vidptr_l),y
        ab89: c8        iny
        ab8a: 84 2a     sty     $2a
        ab8c: a4 2c     ldy     $2c
        ab8e: 24 2b     bit     $2b
        ab90: 10 e0     bpl     $ab72
        ab92: a4 2a     ldy     $2a
        ab94: 88        dey
        ab95: 4c 5f df  jmp     inc_vidptr
; Append a message.  X holds message number, A holds delta-x from current
; position (delta-y is zero).
        ab98: 86 35     stx     $35
        ab9a: 85 2a     sta     $2a
        ab9c: a9 00     lda     #$00
        ab9e: 85 2b     sta     $2b
        aba0: f0 99     beq     $ab3b
; Initialize the high-score list if either of the please-init bits is set.
maybe_init_hs: 20 20 ac  jsr     check_settings
        aba5: ad c9 01  lda     hs_initflag
        aba8: 29 03     and     #$03
        abaa: f0 5b     beq     $ac07
; Initialize the low five scores on the high-score list, and if the
; please-init bits are set, the upper three as well.
     init_hs: 20 20 ac  jsr     check_settings
        abaf: a9 08     lda     #$08
        abb1: 8d 00 01  sta     $0100
        abb4: ad 1b 07  lda     hs_score_1
        abb7: 0d 1c 07  ora     hs_score_1+1
        abba: 0d 1d 07  ora     hs_score_1+2
        abbd: d0 03     bne     $abc2
        abbf: 20 36 ac  jsr     hs_needs_init
        abc2: a2 17     ldx     #$17
        abc4: ad c9 01  lda     hs_initflag
        abc7: 29 01     and     #$01
        abc9: d0 02     bne     $abcd
        abcb: a2 0e     ldx     #$0e
        abcd: bd 08 ac  lda     $ac08,x
        abd0: 9d 06 06  sta     hs_initials_8,x
        abd3: ca        dex
        abd4: 10 f7     bpl     $abcd
        abd6: a2 17     ldx     #$17
        abd8: ad c9 01  lda     hs_initflag
        abdb: 29 02     and     #$02
        abdd: d0 02     bne     $abe1
        abdf: a2 0e     ldx     #$0e
        abe1: a9 01     lda     #$01
        abe3: 9d 06 07  sta     hs_score_8,x
        abe6: ca        dex
        abe7: 10 f8     bpl     $abe1
        abe9: ad c9 01  lda     hs_initflag
        abec: 29 03     and     #$03
        abee: f0 0f     beq     $abff
        abf0: a5 0a     lda     optsw2_shadow
        abf2: 29 f8     and     #$f8
        abf4: 8d 1e 07  sta     life_settings
        abf7: ad 6a 01  lda     diff_bits
        abfa: 29 03     and     #$03 ; difficulty
        abfc: 8d 1f 07  sta     diff_settings
        abff: ad c9 01  lda     hs_initflag
        ac02: 29 fc     and     #$fc
        ac04: 8d c9 01  sta     hs_initflag
        ac07: 60        rts
; Default high score initials.
; Bytes are reversed compared to the order they're displayed in.
        ac08: .chunk  3 ; BEH
        ac0b: .chunk  3 ; MJP
        ac0e: .chunk  3 ; SDL
        ac11: .chunk  3 ; DFT
        ac14: .chunk  3 ; MPH
        ac17: .chunk  3 ; RRR
        ac1a: .chunk  3 ; DES
        ac1d: .chunk  3 ; EJD
check_settings: 20 bb d6  jsr     read_optsws
        ac23: a5 0a     lda     optsw2_shadow
        ac25: 29 f8     and     #$f8 ; initial lives & points per life
        ac27: cd 1e 07  cmp     life_settings
        ac2a: d0 08     bne     $ac34
        ac2c: ad 6a 01  lda     diff_bits
        ac2f: 29 03     and     #$03 ; difficulty
        ac31: cd 1f 07  cmp     diff_settings
        ac34: f0 08     beq     $ac3e
hs_needs_init: ad c9 01  lda     hs_initflag
        ac39: 09 03     ora     #$03
        ac3b: 8d c9 01  sta     hs_initflag
        ac3e: 60        rts
    state_10: a5 05     lda     $05
        ac41: 29 bf     and     #$bf
        ac43: 85 05     sta     $05
        ac45: a5 09     lda     coinage_shadow
        ac47: 29 43     and     #$43
        ac49: c9 40     cmp     #$40
        ac4b: d0 03     bne     $ac50
        ac4d: 20 62 ca  jsr     $ca62
        ac50: 20 fb dd  jsr     $ddfb
        ac53: a9 00     lda     #$00
        ac55: 8d 01 06  sta     $0601
        ac58: a6 3e     ldx     twoplayer
        ac5a: f0 02     beq     $ac5e
        ac5c: a2 03     ldx     #$03
        ac5e: b5 42     lda     p1_score_h,x
        ac60: 85 2c     sta     $2c
        ac62: b5 41     lda     p1_score_m,x
        ac64: 85 2d     sta     $2d
        ac66: b5 40     lda     p1_score_l,x
        ac68: 85 2e     sta     $2e
        ac6a: 8a        txa
        ac6b: 29 01     and     #$01
        ac6d: 85 36     sta     $36
        ac6f: a9 00     lda     #$00
        ac71: 85 2b     sta     $2b
        ac73: a9 1a     lda     #$1a
        ac75: 85 2a     sta     $2a
        ac77: 85 29     sta     $29
        ac79: a9 00     lda     #$00
        ac7b: 8d 05 06  sta     hs_timer
        ac7e: a0 fd     ldy     #$fd
        ac80: b9 20 06  lda     $0620,y
        ac83: c5 2c     cmp     $2c
        ac85: d0 14     bne     $ac9b
        ac87: b9 1f 06  lda     $061f,y
        ac8a: c5 2d     cmp     $2d
        ac8c: d0 0d     bne     $ac9b
        ac8e: c0 52     cpy     #$52
        ac90: 90 08     bcc     $ac9a
        ac92: b9 1e 06  lda     $061e,y
        ac95: c5 2e     cmp     $2e
        ac97: b8        clv
        ac98: 50 01     bvc     $ac9b
        ac9a: 38        sec
        ac9b: b0 4f     bcs     $acec
        ac9d: c0 e8     cpy     #$e8
        ac9f: 90 1e     bcc     $acbf
        aca1: a5 29     lda     $29
        aca3: be 1e 05  ldx     $051e,y
        aca6: 99 1e 05  sta     $051e,y
        aca9: 86 29     stx     $29
        acab: a5 2a     lda     $2a
        acad: be 1f 05  ldx     $051f,y
        acb0: 99 1f 05  sta     $051f,y
        acb3: 86 2a     stx     $2a
        acb5: a5 2b     lda     $2b
        acb7: be 20 05  ldx     $0520,y
        acba: 99 20 05  sta     $0520,y
        acbd: 86 2b     stx     $2b
        acbf: a5 2d     lda     $2d
        acc1: be 1f 06  ldx     $061f,y
        acc4: 99 1f 06  sta     $061f,y
        acc7: 86 2d     stx     $2d
        acc9: a5 2c     lda     $2c
        accb: be 20 06  ldx     $0620,y
        acce: 99 20 06  sta     $0620,y
        acd1: 86 2c     stx     $2c
        acd3: c0 52     cpy     #$52
        acd5: 90 0a     bcc     $ace1
        acd7: a5 2e     lda     $2e
        acd9: be 1e 06  ldx     $061e,y
        acdc: 99 1e 06  sta     $061e,y
        acdf: 86 2e     stx     $2e
        ace1: c0 55     cpy     #$55
        ace3: 90 01     bcc     $ace6
        ace5: 88        dey
        ace6: 88        dey
        ace7: 88        dey
        ace8: d0 b3     bne     $ac9d
        acea: a0 02     ldy     #$02
        acec: ee 05 06  inc     hs_timer
        acef: c0 55     cpy     #$55
        acf1: 90 01     bcc     $acf4
        acf3: 88        dey
        acf4: 88        dey
        acf5: 88        dey
        acf6: d0 88     bne     $ac80
        acf8: a6 36     ldx     $36
        acfa: ad 05 06  lda     hs_timer
        acfd: 9d 00 06  sta     $0600,x
        ad00: ca        dex
        ad01: 30 03     bmi     $ad06
        ad03: 4c 5e ac  jmp     $ac5e
        ad06: ad 01 06  lda     $0601
        ad09: cd 00 06  cmp     $0600
        ad0c: 90 07     bcc     $ad15
        ad0e: c9 63     cmp     #$63
        ad10: b0 03     bcs     $ad15
        ad12: ee 01 06  inc     $0601
        ad15: a5 3d     lda     curplayer
        ad17: 49 01     eor     #$01
        ad19: 0a        asl     a
        ad1a: 0a        asl     a
        ad1b: 05 3d     ora     curplayer
        ad1d: 69 05     adc     #$05
        ad1f: 8d 03 06  sta     $0603
        ad22: a0 14     ldy     #$14
        ad24: ad 03 06  lda     $0603
        ad27: f0 42     beq     $ad6b
        ad29: 29 03     and     #$03
        ad2b: 85 3d     sta     curplayer
        ad2d: c6 3d     dec     curplayer
        ad2f: 4e 03 06  lsr     $0603
        ad32: 4e 03 06  lsr     $0603
        ad35: a6 3d     ldx     curplayer
        ad37: bd 00 06  lda     $0600,x
        ad3a: f0 2c     beq     $ad68
        ad3c: c9 09     cmp     #$09
        ad3e: b0 28     bcs     $ad68
        ad40: 0a        asl     a
        ad41: 18        clc
        ad42: 7d 00 06  adc     $0600,x
        ad45: 49 ff     eor     #$ff
        ad47: 38        sec
        ad48: e9 e5     sbc     #$e5
        ad4a: 8d 02 06  sta     hs_whichletter
        ad4d: 20 48 ca  jsr     $ca48
; Entering high score?
        ad50: a9 60     lda     #$60
        ad52: 8d 05 06  sta     hs_timer
        ad55: a9 00     lda     #$00
        ad57: 85 4e     sta     zap_fire_new
        ad59: 85 50     sta     $50
        ad5b: a9 02     lda     #$02
        ad5d: 8d 04 06  sta     $0604
        ad60: 20 89 a7  jsr     $a789
        ad63: a0 24     ldy     #$24
        ad65: 84 00     sty     gamestate
        ad67: 60        rts
        ad68: 4c 22 ad  jmp     $ad22
        ad6b: 84 00     sty     gamestate
        ad6d: 60        rts
; High score entry
    state_12: a9 06     lda     #$06
        ad70: 85 01     sta     $01
        ad72: a5 03     lda     timectr
        ad74: 29 1f     and     #$1f
        ad76: d0 0a     bne     $ad82
        ad78: ce 05 06  dec     hs_timer
        ad7b: d0 05     bne     $ad82
        ad7d: a0 14     ldy     #$14
        ad7f: 84 00     sty     gamestate
        ad81: 60        rts
        ad82: ae 02 06  ldx     hs_whichletter
        ad85: bd 06 06  lda     hs_initials_8,x
        ad88: 20 ce ad  jsr     track_spinner
; enforce 0..$1a (0-26, A through space) range
        ad8b: a8        tay
        ad8c: 10 05     bpl     $ad93
        ad8e: a9 1a     lda     #$1a
        ad90: b8        clv
        ad91: 50 06     bvc     $ad99
        ad93: c9 1b     cmp     #$1b
        ad95: 90 02     bcc     $ad99
        ad97: a9 00     lda     #$00
        ad99: ae 02 06  ldx     hs_whichletter
        ad9c: 9d 06 06  sta     hs_initials_8,x
        ad9f: a5 4e     lda     zap_fire_new
        ada1: 29 18     and     #$18
        ada3: a8        tay
        ada4: a5 4e     lda     zap_fire_new
        ada6: 29 67     and     #$67
        ada8: 85 4e     sta     zap_fire_new
        adaa: 98        tya
        adab: f0 20     beq     $adcd
        adad: ce 02 06  dec     hs_whichletter
        adb0: ce 04 06  dec     $0604
        adb3: 10 12     bpl     $adc7
        adb5: a6 3d     ldx     curplayer
        adb7: bd 00 06  lda     $0600,x
        adba: c9 04     cmp     #$04
        adbc: b0 03     bcs     $adc1
        adbe: 20 f7 dd  jsr     $ddf7
        adc1: 20 22 ad  jsr     $ad22
        adc4: b8        clv
        adc5: 50 06     bvc     $adcd
        adc7: ca        dex
        adc8: a9 00     lda     #$00
        adca: 9d 06 06  sta     hs_initials_8,x
        adcd: 60        rts
; Track the spinner, maybe?  Input value in A, return value in A is either
; unchanged, one higher, or one lower.  Adds $50 into $51 and clears $50.
track_spinner: 48        pha
        adcf: a5 50     lda     $50
        add1: 0a        asl     a
        add2: 0a        asl     a
        add3: 0a        asl     a
        add4: 18        clc
        add5: 65 51     adc     $51
        add7: 85 51     sta     $51
        add9: 68        pla
        adda: a4 50     ldy     $50
        addc: 30 05     bmi     $ade3
        adde: 69 00     adc     #$00
        ade0: b8        clv
        ade1: 50 02     bvc     $ade5
        ade3: 69 ff     adc     #$ff
        ade5: a0 00     ldy     #$00
        ade7: 84 50     sty     $50
        ade9: 60        rts
        adea: 20 b4 a8  jsr     $a8b4
        aded: a9 c0     lda     #$c0
        adef: a2 02     ldx     #$02 ; "PLAYER "
        adf1: 20 17 ab  jsr     vapp_msg_at_y
        adf4: ce 6e 01  dec     $016e
        adf7: 20 97 aa  jsr     $aa97
        adfa: a2 0a     ldx     #$0a ; "ENTER YOUR INITIALS"
        adfc: 20 14 ab  jsr     vapp_msg
        adff: a9 a6     lda     #$a6
        ae01: a2 0c     ldx     #$0c ; "SPIN KNOB TO CHANGE"
        ae03: 20 17 ab  jsr     vapp_msg_at_y
        ae06: a9 9c     lda     #$9c
        ae08: a2 0e     ldx     #$0e ; "PRESS FIRE TO SELECT"
        ae0a: 20 17 ab  jsr     vapp_msg_at_y
        ae0d: a2 2c     ldx     #$2c ; "(c) MCMLXXX ATARI"
        ae0f: 20 14 ab  jsr     vapp_msg
        ae12: ad 02 06  lda     hs_whichletter
        ae15: 38        sec
        ae16: ed 04 06  sbc     $0604
        ae19: 4c 4e ae  jmp     $ae4e
        ae1c: 20 b4 a8  jsr     $a8b4
        ae1f: 78        sei
        ae20: ad ca 60  lda     pokey1_rand
        ae23: ac ca 60  ldy     pokey1_rand
        ae26: 84 29     sty     $29
        ae28: 4a        lsr     a
        ae29: 4a        lsr     a
        ae2a: 4a        lsr     a
        ae2b: 4a        lsr     a
        ae2c: 45 29     eor     $29
        ae2e: 85 29     sta     $29
        ae30: ad da 60  lda     pokey2_rand
        ae33: ac da 60  ldy     pokey2_rand
        ae36: 58        cli
        ae37: 45 29     eor     $29
        ae39: 29 f0     and     #$f0
        ae3b: 45 29     eor     $29
        ae3d: 85 29     sta     $29
        ae3f: 98        tya
        ae40: 0a        asl     a
        ae41: 0a        asl     a
        ae42: 0a        asl     a
        ae43: 0a        asl     a
        ae44: 45 29     eor     $29
        ae46: 8d 1f 01  sta     $011f
        ae49: 20 26 af  jsr     $af26
        ae4c: a9 ff     lda     #$ff
        ae4e: 85 63     sta     $63
        ae50: a2 10     ldx     #$10 ; "HIGH SCORES"
        ae52: 20 14 ab  jsr     vapp_msg
        ae55: a9 01     lda     #$01
        ae57: 85 61     sta     $61
        ae59: 20 dd b0  jsr     vapp_setscale
        ae5c: a9 28     lda     #$28
        ae5e: 85 2c     sta     $2c
        ae60: a2 15     ldx     #$15
        ae62: 86 37     stx     $37
        ae64: 20 0d ab  jsr     vapp_vcentre_1
        ae67: a9 00     lda     #$00
        ae69: 85 73     sta     draw_z
        ae6b: a5 2c     lda     $2c
        ae6d: aa        tax
        ae6e: 38        sec
        ae6f: e9 0a     sbc     #$0a
        ae71: 85 2c     sta     $2c
        ae73: a9 d0     lda     #$d0
        ae75: 20 75 df  jsr     vapp_ldraw_A,X
        ae78: a0 07     ldy     #$07
        ae7a: a5 63     lda     $63
        ae7c: c5 37     cmp     $37
        ae7e: d0 02     bne     $ae82
        ae80: a0 00     ldy     #$00
        ae82: 20 d1 b0  jsr     vapp_setcolor
        ae85: a9 61     lda     #$61
        ae87: a0 01     ldy     #$01
        ae89: 20 b1 df  jsr     vapp_multdig_y@a
        ae8c: a9 a0     lda     #$a0
        ae8e: 20 6a b5  jsr     $b56a
        ae91: a9 00     lda     #$00
        ae93: 85 73     sta     draw_z
        ae95: aa        tax
        ae96: a9 08     lda     #$08
        ae98: 20 75 df  jsr     vapp_ldraw_A,X
        ae9b: e6 61     inc     $61
        ae9d: a5 37     lda     $37
        ae9f: 20 f8 ae  jsr     $aef8
        aea2: a2 00     ldx     #$00
        aea4: a9 08     lda     #$08
        aea6: 20 75 df  jsr     vapp_ldraw_A,X
        aea9: a6 37     ldx     $37
        aeab: bd 06 07  lda     hs_score_8,x
        aeae: 85 56     sta     $56
        aeb0: bd 07 07  lda     $0707,x
        aeb3: 85 57     sta     $57
        aeb5: bd 08 07  lda     $0708,x
        aeb8: 85 58     sta     $58
        aeba: a9 56     lda     #$56
        aebc: a0 03     ldy     #$03
        aebe: 20 b1 df  jsr     vapp_multdig_y@a
        aec1: c6 37     dec     $37
        aec3: c6 37     dec     $37
        aec5: c6 37     dec     $37
        aec7: 10 9b     bpl     $ae64
        aec9: 60        rts
        aeca: ad 56 01  lda     bonus_life_each
        aecd: f0 14     beq     $aee3
        aecf: 85 58     sta     $58
        aed1: a2 34     ldx     #$34 ; "BONUS EVERY "
        aed3: 20 14 ab  jsr     vapp_msg
        aed6: a9 00     lda     #$00
        aed8: 85 56     sta     $56
        aeda: 85 57     sta     $57
        aedc: a9 56     lda     #$56
        aede: a0 03     ldy     #$03
        aee0: 20 b1 df  jsr     vapp_multdig_y@a
        aee3: 18        clc
        aee4: a0 10     ldy     #$10
        aee6: a9 85     lda     #$85
        aee8: 79 75 d5  adc     $d575,y ; "(c) MCMLXXX ATARI" data
        aeeb: 88        dey
        aeec: 10 fa     bpl     $aee8
        aeee: 85 b5     sta     copyr_cksum
        aef0: 60        rts
        aef1: ad 02 06  lda     hs_whichletter
        aef4: 38        sec
        aef5: ed 04 06  sbc     $0604
        aef8: 18        clc
        aef9: 69 02     adc     #$02
        aefb: 85 38     sta     $38
        aefd: a0 00     ldy     #$00
        aeff: a9 02     lda     #$02
        af01: 85 39     sta     $39
        af03: a6 38     ldx     $38
        af05: bd 06 06  lda     hs_initials_8,x
        af08: c9 1e     cmp     #$1e
        af0a: 90 02     bcc     $af0e
        af0c: a9 1a     lda     #$1a
        af0e: 0a        asl     a
        af0f: aa        tax
        af10: bd fa 31  lda     ltr_jsrtbl,x
        af13: 91 74     sta     (vidptr_l),y
        af15: c8        iny
        af16: bd fb 31  lda     ltr_jsrtbl+1,x
        af19: 91 74     sta     (vidptr_l),y
        af1b: c8        iny
        af1c: c6 38     dec     $38
        af1e: c6 39     dec     $39
        af20: 10 e1     bpl     $af03
        af22: 88        dey
        af23: 4c 5f df  jmp     inc_vidptr
        af26: ad 00 06  lda     $0600
        af29: 0d 01 06  ora     $0601
        af2c: f0 40     beq     $af6e
        af2e: a2 12     ldx     #$12
        af30: 20 14 ab  jsr     vapp_msg
        af33: a9 63     lda     #$63
        af35: 20 71 af  jsr     $af71
        af38: a2 00     ldx     #$00
        af3a: 20 3f af  jsr     $af3f
        af3d: a2 01     ldx     #$01
        af3f: bd 00 06  lda     $0600,x
        af42: f0 2a     beq     $af6e
        af44: 48        pha
        af45: 86 2e     stx     $2e
        af47: a0 03     ldy     #$03
        af49: 20 d1 b0  jsr     vapp_setcolor
        af4c: 20 0d ab  jsr     vapp_vcentre_1
        af4f: a9 d0     lda     #$d0
        af51: a4 2e     ldy     $2e
        af53: be 6f af  ldx     $af6f,y
        af56: 20 75 df  jsr     vapp_ldraw_A,X
        af59: 68        pla
        af5a: 20 71 af  jsr     $af71
        af5d: a9 a0     lda     #$a0
        af5f: 20 6a b5  jsr     $b56a
        af62: a9 10     lda     #$10 ; +16
        af64: a2 04     ldx     #$04 ; "PLAYER "
        af66: 20 98 ab  jsr     $ab98
        af69: a6 2e     ldx     $2e
        af6b: 20 9e aa  jsr     $aa9e
        af6e: 60        rts
        af6f: c0 b0     cpy     #$b0
        af71: c9 63     cmp     #$63
        af73: 90 02     bcc     vapp_2dig_bin
        af75: a9 63     lda     #$63
vapp_2dig_bin: 20 f5 aa  jsr     bin_to_bcd
        af7a: a9 29     lda     #$29
        af7c: a0 01     ldy     #$01
        af7e: 4c b1 df  jmp     vapp_multdig_y@a
        af81: 20 48 ca  jsr     $ca48
        af84: ce 6e 01  dec     $016e
        af87: a0 03     ldy     #$03
        af89: 20 d1 b0  jsr     vapp_setcolor
        af8c: a9 01     lda     #$01
        af8e: 85 72     sta     curscale
        af90: 20 6a df  jsr     vapp_scale_A,0
        af93: a2 2c     ldx     #$2c ; "(c) MCMLXXX ATARI"
        af95: a9 60     lda     #$60 ; Y coord (normally $92 for this msg)
        af97: 20 17 ab  jsr     vapp_msg_at_y
        af9a: 20 92 aa  jsr     show_plyno
        af9d: a2 07     ldx     #$07
        af9f: 86 37     stx     $37
        afa1: a4 37     ldy     $37
        afa3: be 9b b0  ldx     selfrate_msgs,y
        afa6: 20 14 ab  jsr     vapp_msg
        afa9: c6 37     dec     $37
        afab: 10 f4     bpl     $afa1
        afad: ad 00 02  lda     player_seg
        afb0: 38        sec
        afb1: e5 7b     sbc     $7b
        afb3: 10 07     bpl     $afbc
        afb5: c6 7b     dec     $7b
        afb7: c6 7c     dec     $7c
        afb9: b8        clv
        afba: 50 25     bvc     $afe1
        afbc: d0 0d     bne     $afcb
        afbe: c6 7c     dec     $7c
        afc0: c6 7b     dec     $7b
        afc2: 10 04     bpl     $afc8
        afc4: e6 7b     inc     $7b
        afc6: e6 7c     inc     $7c
        afc8: b8        clv
        afc9: 50 16     bvc     $afe1
        afcb: a5 7c     lda     $7c
        afcd: cd 27 01  cmp     $0127
        afd0: f0 02     beq     $afd4
        afd2: b0 0d     bcs     $afe1
        afd4: 38        sec
        afd5: ed 00 02  sbc     player_seg
        afd8: d0 01     bne     $afdb
        afda: 18        clc
        afdb: b0 04     bcs     $afe1
        afdd: e6 7b     inc     $7b
        afdf: e6 7c     inc     $7c
        afe1: a5 7c     lda     $7c
        afe3: 85 3a     sta     $3a
        afe5: a2 04     ldx     #$04
        afe7: 86 37     stx     $37
        afe9: a0 05     ldy     #$05
        afeb: 20 d1 b0  jsr     vapp_setcolor
        afee: a9 00     lda     #$00
        aff0: 85 73     sta     draw_z
        aff2: 20 0d ab  jsr     vapp_vcentre_1
        aff5: a2 d8     ldx     #$d8
        aff7: a4 37     ldy     $37
        aff9: b9 96 b0  lda     $b096,y
        affc: 18        clc
        affd: 69 f8     adc     #$f8
        afff: 20 75 df  jsr     vapp_ldraw_A,X
        b002: a6 3a     ldx     $3a
        b004: bc fe 91  ldy     startlevtbl,x
        b007: c0 63     cpy     #$63
        b009: b0 37     bcs     $b042
        b00b: c8        iny
        b00c: 98        tya
        b00d: 20 77 af  jsr     vapp_2dig_bin
        b010: a0 03     ldy     #$03
        b012: 20 d1 b0  jsr     vapp_setcolor
        b015: 20 0d ab  jsr     vapp_vcentre_1
        b018: a2 ba     ldx     #$ba
        b01a: a4 37     ldy     $37
        b01c: b9 96 b0  lda     $b096,y
        b01f: 18        clc
        b020: 69 ec     adc     #$ec
        b022: 20 75 df  jsr     vapp_ldraw_A,X
        b025: a6 3a     ldx     $3a
        b027: 20 c6 b0  jsr     vapp_startbonus
        b02a: 20 0d ab  jsr     vapp_vcentre_1
        b02d: a2 cc     ldx     #$cc
        b02f: a4 37     ldy     $37
        b031: b9 96 b0  lda     $b096,y
        b034: 18        clc
        b035: 69 00     adc     #$00
        b037: 20 75 df  jsr     vapp_ldraw_A,X
        b03a: a6 3a     ldx     $3a
        b03c: bd fe 91  lda     startlevtbl,x
        b03f: 20 e1 c4  jsr     $c4e1
        b042: c6 3a     dec     $3a
        b044: c6 37     dec     $37
        b046: 10 a1     bpl     $afe9
        b048: a9 00     lda     #$00
        b04a: 85 73     sta     draw_z
        b04c: 20 0d ab  jsr     vapp_vcentre_1
        b04f: a2 1c     ldx     #$1c ; "TIME"
        b051: 20 14 ab  jsr     vapp_msg
        b054: a9 04     lda     #$04
        b056: a0 01     ldy     #$01
        b058: 20 b1 df  jsr     vapp_multdig_y@a
        b05b: a0 00     ldy     #$00
        b05d: 20 d1 b0  jsr     vapp_setcolor
        b060: 20 0d ab  jsr     vapp_vcentre_1
        b063: a2 b8     ldx     #$b8
        b065: 20 ab b0  jsr     $b0ab
        b068: 38        sec
        b069: e5 7b     sbc     $7b
        b06b: a8        tay
        b06c: b9 96 b0  lda     $b096,y
        b06f: 38        sec
        b070: e9 16     sbc     #$16
        b072: 20 75 df  jsr     vapp_ldraw_A,X
        b075: a9 e0     lda     #$e0
        b077: 85 73     sta     draw_z
        b079: a2 00     ldx     #$00
        b07b: 86 38     stx     $38
        b07d: a0 03     ldy     #$03
        b07f: 84 37     sty     $37
        b081: a4 38     ldy     $38
        b083: b9 a3 b0  lda     $b0a3,y
        b086: aa        tax
        b087: c8        iny
        b088: b9 a3 b0  lda     $b0a3,y
        b08b: c8        iny
        b08c: 84 38     sty     $38
        b08e: 20 75 df  jsr     vapp_ldraw_A,X
        b091: c6 37     dec     $37
        b093: 10 ec     bpl     $b081
        b095: 60        rts
; These appear to be X offsets of the tube pictures on the starting-level
; selection display.  See $aff9.
        b096: .byte   be
        b097: .byte   e3
        b098: .byte   09
        b099: .byte   30
        b09a: .byte   58
selfrate_msgs: .byte   14 ; "RATE YOURSELF"
        b09c: .byte   0c ; "SPIN KNOB TO CHANGE"
        b09d: .byte   0e ; "PRESS FIRE TO SELECT"
        b09e: .byte   16 ; "NOVICE"
        b09f: .byte   18 ; "EXPERT"
        b0a0: .byte   1e ; "LEVEL"
        b0a1: .byte   20 ; "HOLE"
        b0a2: .byte   1a ; "BONUS"
; Used at $b083 to draw the box around the selected level, on the starting
; level selection screen.
        b0a3: .word   2600 ; x=+0,y=+38
        b0a5: .word   0028 ; x=+40,y=+0
        b0a7: .word   da00 ; x=+0,y=-38
        b0a9: .word   00d8 ; x=-40,y=+0
        b0ab: ad 00 02  lda     player_seg
        b0ae: 20 ce ad  jsr     track_spinner
        b0b1: a8        tay
        b0b2: 10 05     bpl     $b0b9
        b0b4: a9 00     lda     #$00
        b0b6: b8        clv
        b0b7: 50 08     bvc     $b0c1
        b0b9: cd 27 01  cmp     $0127
        b0bc: 90 03     bcc     $b0c1
        b0be: ad 27 01  lda     $0127
        b0c1: 8d 00 02  sta     player_seg
        b0c4: a8        tay
        b0c5: 60        rts
vapp_startbonus: 8a        txa
        b0c7: 20 b5 91  jsr     ld_startbonus
        b0ca: a9 29     lda     #$29
        b0cc: a0 03     ldy     #$03
        b0ce: 4c b1 df  jmp     vapp_multdig_y@a
vapp_setcolor: c4 9e     cpy     curcolor
        b0d3: f0 07     beq     $b0dc
        b0d5: 84 9e     sty     curcolor
        b0d7: a9 08     lda     #$08
        b0d9: 4c 4c df  jmp     vapp_sclstat_A_Y
        b0dc: 60        rts
vapp_setscale: c5 72     cmp     curscale
        b0df: f0 05     beq     $b0e6
        b0e1: 85 72     sta     curscale
        b0e3: 4c 6a df  jmp     vapp_scale_A,0
        b0e6: 60        rts
    state_1a: a9 0a     lda     #$0a
        b0e9: 85 00     sta     gamestate
        b0eb: a9 00     lda     #$00
        b0ed: 85 02     sta     $02
        b0ef: a9 df     lda     #$df
        b0f1: 85 04     sta     $04
        b0f3: a9 12     lda     #$12
        b0f5: 85 01     sta     $01
        b0f7: a9 19     lda     #$19
        b0f9: 8d 4e 01  sta     $014e
        b0fc: a9 18     lda     #$18
        b0fe: 8d 4d 01  sta     $014d
        b101: 60        rts
        b102: a9 34     lda     #$34
        b104: a2 aa     ldx     #$aa
        b106: 20 5a b1  jsr     $b15a
        b109: ad 4e 01  lda     $014e
        b10c: c9 a0     cmp     #$a0
        b10e: b0 05     bcs     $b115
        b110: 69 14     adc     #$14
        b112: 8d 4e 01  sta     $014e
        b115: c9 50     cmp     #$50
        b117: 90 17     bcc     $b130
        b119: ad 4d 01  lda     $014d
        b11c: 18        clc
        b11d: 69 08     adc     #$08
        b11f: 8d 4d 01  sta     $014d
        b122: cd 4e 01  cmp     $014e
        b125: 90 09     bcc     $b130
        b127: a9 a0     lda     #$a0
        b129: 8d 4d 01  sta     $014d
        b12c: a9 14     lda     #$14
        b12e: 85 01     sta     $01
        b130: 60        rts
        b131: a9 3f     lda     #$3f
        b133: a2 4e     ldx     #$4e
        b135: 20 5a b1  jsr     $b15a
        b138: ad 4d 01  lda     $014d
        b13b: c9 30     cmp     #$30
        b13d: 90 05     bcc     $b144
        b13f: e9 01     sbc     #$01
        b141: 8d 4d 01  sta     $014d
        b144: c9 80     cmp     #$80
        b146: b0 11     bcs     $b159
        b148: ad 4e 01  lda     $014e
        b14b: 38        sec
        b14c: e9 01     sbc     #$01
        b14e: cd 4d 01  cmp     $014d
        b151: b0 03     bcs     $b156
        b153: ad 4d 01  lda     $014d
        b156: 8d 4e 01  sta     $014e
        b159: 60        rts
        b15a: 85 57     sta     $57
        b15c: 86 56     stx     $56
        b15e: ad 4d 01  lda     $014d
        b161: 85 37     sta     $37
        b163: ce 6e 01  dec     $016e
        b166: a5 37     lda     $37
        b168: 0a        asl     a
        b169: 0a        asl     a
        b16a: 29 7f     and     #$7f
        b16c: a8        tay
        b16d: a5 37     lda     $37
        b16f: 4a        lsr     a
        b170: 4a        lsr     a
        b171: 4a        lsr     a
        b172: 4a        lsr     a
        b173: 4a        lsr     a
        b174: 20 6c df  jsr     vapp_scale_A,Y
        b177: a5 37     lda     $37
        b179: cd 4d 01  cmp     $014d
        b17c: d0 05     bne     $b183
        b17e: a9 00     lda     #$00
        b180: b8        clv
        b181: 50 0c     bvc     $b18f
        b183: 4a        lsr     a
        b184: 4a        lsr     a
        b185: 4a        lsr     a
        b186: ea        nop
        b187: 29 07     and     #$07
        b189: c9 07     cmp     #$07
        b18b: d0 02     bne     $b18f
        b18d: a9 03     lda     #$03
        b18f: a8        tay
        b190: a9 68     lda     #$68
        b192: 20 4c df  jsr     vapp_sclstat_A_Y
        b195: a5 57     lda     $57
        b197: a6 56     ldx     $56
        b199: 20 39 df  jsr     vapp_vjsr_AX
        b19c: a5 37     lda     $37
        b19e: 18        clc
        b19f: 69 02     adc     #$02
        b1a1: 85 37     sta     $37
        b1a3: cd 4e 01  cmp     $014e
        b1a6: 90 be     bcc     $b166
        b1a8: a2 2c     ldx     #$2c
        b1aa: a9 d0     lda     #$d0
        b1ac: 20 17 ab  jsr     vapp_msg_at_y
        b1af: a9 3f     lda     #$3f
        b1b1: a2 f2     ldx     #$f2
        b1b3: 4c 39 df  jmp     vapp_vjsr_AX
        b1b6: 20 c3 c1  jsr     $c1c3
        b1b9: ad 00 20  lda     vecram
        b1bc: cd c6 ce  cmp     $cec6
        b1bf: d0 06     bne     $b1c7
        b1c1: ad 33 01  lda     $0133
        b1c4: d0 01     bne     $b1c7
        b1c6: 60        rts
        b1c7: a5 01     lda     $01
        b1c9: c9 00     cmp     #$00
        b1cb: f0 3c     beq     $b209
        b1cd: a9 00     lda     #$00
        b1cf: 20 be b2  jsr     db_init_vidptr
        b1d2: 20 32 b3  jsr     $b332
        b1d5: b0 1e     bcs     $b1f5
        b1d7: 20 0d b2  jsr     $b20d
        b1da: ad 6e 01  lda     $016e
        b1dd: f0 16     beq     $b1f5
; Anti-piracy provision.  Checksum the video RAM that holds the copyright
; message.  See also $b27d and $a581.
        b1df: a0 27     ldy     #$27
        b1e1: a9 0e     lda     #$0e
        b1e3: 38        sec
        b1e4: f1 b6     sbc     (copyr_vid_loc),y
        b1e6: 88        dey
        b1e7: 10 fb     bpl     $b1e4
        b1e9: a8        tay
        b1ea: f0 02     beq     $b1ee
        b1ec: 49 e5     eor     #$e5
        b1ee: f0 02     beq     $b1f2
        b1f0: 49 29     eor     #$29
        b1f2: 8d 55 04  sta     copyr_vid_cksum2
        b1f5: a9 00     lda     #$00
        b1f7: 20 fe b2  jsr     dblbuf_done
        b1fa: ad c4 ce  lda     $cec4
        b1fd: 8d 00 20  sta     vecram
        b200: ad c5 ce  lda     $cec5
        b203: 8d 01 20  sta     vecram+1
        b206: b8        clv
        b207: 50 03     bvc     $b20c
        b209: 4c 30 b2  jmp     $b230
        b20c: 60        rts
        b20d: a6 01     ldx     $01
        b20f: bd 19 b2  lda     $b219,x
        b212: 48        pha
        b213: bd 18 b2  lda     $b218,x
        b216: 48        pha
        b217: 60        rts
; Jump table, used just above, at $b20f
        b218: .jump   $b230
        b21a: .jump   $d804
        b21c: .jump   $b8ba
        b21e: .jump   $adea
        b220: .jump   $af81
        b222: .jump   $ae1c
        b224: .jump   $aa62
        b226: .jump   $aa5a
        b228: .jump   $aa6f
        b22a: .jump   $b102
        b22c: .jump   $b131
        b22e: .jump   $aa79
        b230: a9 07     lda     #$07
        b232: 20 be b2  jsr     db_init_vidptr
        b235: 20 86 b5  jsr     draw_player
        b238: a9 07     lda     #$07
        b23a: 20 fe b2  jsr     dblbuf_done
        b23d: a9 04     lda     #$04
        b23f: 20 be b2  jsr     db_init_vidptr
        b242: 20 5b b7  jsr     draw_shots
        b245: a9 04     lda     #$04
        b247: 20 fe b2  jsr     dblbuf_done
        b24a: a9 03     lda     #$03
        b24c: 20 be b2  jsr     db_init_vidptr
        b24f: 20 ad b5  jsr     draw_enemies
        b252: a9 03     lda     #$03
        b254: 20 fe b2  jsr     dblbuf_done
        b257: a9 06     lda     #$06
        b259: 20 be b2  jsr     db_init_vidptr
        b25c: 20 9a b7  jsr     draw_explosions
        b25f: a9 06     lda     #$06
        b261: 20 fe b2  jsr     dblbuf_done
        b264: a9 05     lda     #$05
        b266: 20 be b2  jsr     db_init_vidptr
        b269: 20 98 b4  jsr     draw_pending
        b26c: a9 05     lda     #$05
        b26e: 20 fe b2  jsr     dblbuf_done
        b271: a9 00     lda     #$00
        b273: 20 be b2  jsr     db_init_vidptr
        b276: 20 b4 a8  jsr     $a8b4
        b279: a5 05     lda     $05
        b27b: 30 0d     bmi     $b28a
; Anti-piracy provision.  When not playing, checksum the video RAM that
; holds the copyright message.  See also $b1df and $a581.
        b27d: a9 f2     lda     #$f2
        b27f: 18        clc
        b280: a0 27     ldy     #$27
        b282: 71 b6     adc     (copyr_vid_loc),y
        b284: 88        dey
        b285: 10 fb     bpl     $b282
        b287: 8d 1b 01  sta     copyr_vid_cksum1
; End anti-piracy provision.
        b28a: a9 00     lda     #$00
        b28c: 20 fe b2  jsr     dblbuf_done
        b28f: 20 67 b3  jsr     $b367
        b292: a9 01     lda     #$01
        b294: 20 be b2  jsr     db_init_vidptr
        b297: 20 c2 c5  jsr     $c5c2
        b29a: a9 01     lda     #$01
        b29c: 20 fe b2  jsr     dblbuf_done
        b29f: a9 08     lda     #$08
        b2a1: 20 be b2  jsr     db_init_vidptr
        b2a4: 20 4d c5  jsr     $c54d
        b2a7: a9 08     lda     #$08
        b2a9: 20 fe b2  jsr     dblbuf_done
        b2ac: a9 00     lda     #$00
        b2ae: 8d 14 01  sta     $0114
        b2b1: ad c2 ce  lda     $cec2
        b2b4: 8d 00 20  sta     vecram
        b2b7: ad c3 ce  lda     $cec3
        b2ba: 8d 01 20  sta     vecram+1
        b2bd: 60        rts
db_init_vidptr: aa        tax
        b2bf: 0a        asl     a
        b2c0: a8        tay
        b2c1: bd 15 04  lda     dblbuf_flg,x
        b2c4: d0 09     bne     $b2cf
        b2c6: be 7a ce  ldx     dblbuf_addr_B,y
        b2c9: b9 7b ce  lda     dblbuf_addr_A+1,y
        b2cc: b8        clv
        b2cd: 50 06     bvc     $b2d5
        b2cf: be 68 ce  ldx     dblbuf_addr_A,y
        b2d2: b9 69 ce  lda     dblbuf_addr_B+1,y
        b2d5: 86 74     stx     vidptr_l
        b2d7: 85 75     sta     vidptr_h
        b2d9: a9 00     lda     #$00
        b2db: 85 a9     sta     $a9
        b2dd: 60        rts
        b2de: aa        tax
        b2df: 0a        asl     a
        b2e0: a8        tay
        b2e1: bd 15 04  lda     dblbuf_flg,x
        b2e4: d0 09     bne     $b2ef
        b2e6: be 68 ce  ldx     dblbuf_addr_A,y
        b2e9: b9 69 ce  lda     dblbuf_addr_B+1,y
        b2ec: b8        clv
        b2ed: 50 06     bvc     $b2f5
        b2ef: be 7a ce  ldx     dblbuf_addr_B,y
        b2f2: b9 7b ce  lda     dblbuf_addr_A+1,y
        b2f5: 86 3b     stx     $3b
        b2f7: 85 3c     sta     $3c
        b2f9: a9 00     lda     #$00
        b2fb: 85 a9     sta     $a9
        b2fd: 60        rts
 dblbuf_done: 48        pha
        b2ff: 20 09 df  jsr     vapp_rts
        b302: 68        pla
        b303: aa        tax
        b304: 0a        asl     a
        b305: a8        tay
        b306: b9 8c ce  lda     dblbuf_vjsr_loc,y
        b309: 85 3b     sta     $3b
        b30b: b9 8d ce  lda     dblbuf_vjsr_loc+1,y
        b30e: 85 3c     sta     $3c
        b310: bd 15 04  lda     dblbuf_flg,x
        b313: 49 01     eor     #$01
        b315: 9d 15 04  sta     dblbuf_flg,x
        b318: d0 09     bne     $b323
        b31a: b9 9e ce  lda     dblbuf_vjmp_C,y
        b31d: be 9f ce  ldx     dblbuf_vjmp_C+1,y
        b320: b8        clv
        b321: 50 06     bvc     $b329
        b323: b9 b0 ce  lda     dblbuf_vjmp_D,y
        b326: be b1 ce  ldx     dblbuf_vjmp_D+1,y
        b329: a0 00     ldy     #$00
        b32b: 91 3b     sta     ($3b),y
        b32d: 8a        txa
        b32e: c8        iny
        b32f: 91 3b     sta     ($3b),y
        b331: 60        rts
        b332: ad c4 ce  lda     $cec4
        b335: cd 00 20  cmp     vecram
        b338: f0 05     beq     $b33f
        b33a: 8d 00 20  sta     vecram
        b33d: 38        sec
        b33e: 60        rts
        b33f: ad 15 04  lda     dblbuf_flg
        b342: d0 05     bne     $b349
        b344: a2 02     ldx     #$02
        b346: b8        clv
        b347: 50 02     bvc     $b34b
        b349: a2 08     ldx     #$08
        b34b: bd 9e ce  lda     dblbuf_vjmp_C,x
        b34e: a0 00     ldy     #$00
        b350: 8c 6e 01  sty     $016e
        b353: 91 74     sta     (vidptr_l),y
        b355: c8        iny
        b356: bd 9f ce  lda     dblbuf_vjmp_C+1,x
        b359: 91 74     sta     (vidptr_l),y
        b35b: bd 68 ce  lda     dblbuf_addr_A,x
        b35e: 85 74     sta     vidptr_l
        b360: bd 69 ce  lda     dblbuf_addr_B+1,x
        b363: 85 75     sta     vidptr_h
        b365: 18        clc
        b366: 60        rts
        b367: ad 14 01  lda     $0114
        b36a: f0 0d     beq     $b379
        b36c: a9 02     lda     #$02
        b36e: 20 be b2  jsr     db_init_vidptr
        b371: 20 0d c3  jsr     $c30d
        b374: a9 02     lda     #$02
        b376: 20 fe b2  jsr     dblbuf_done
        b379: a9 02     lda     #$02
        b37b: 20 de b2  jsr     $b2de
        b37e: a9 00     lda     #$00
        b380: a2 0f     ldx     #$0f
        b382: 9d 25 04  sta     $0425,x
        b385: ca        dex
        b386: 10 fa     bpl     $b382
        b388: ad 06 01  lda     $0106
        b38b: 30 49     bmi     $b3d6
        b38d: ae 1c 01  ldx     max_enm
        b390: bd df 02  lda     enemy_along,x
        b393: f0 3e     beq     $b3d3
        b395: a0 00     ldy     #$00
        b397: bd 83 02  lda     $0283,x
        b39a: 29 07     and     #$07
        b39c: c9 01     cmp     #$01
        b39e: d0 33     bne     $b3d3
        b3a0: c8        iny
        b3a1: 84 29     sty     $29
        b3a3: bd 83 02  lda     $0283,x
        b3a6: 29 80     and     #$80
        b3a8: d0 1c     bne     $b3c6
        b3aa: ad 48 01  lda     pulsing
        b3ad: 30 0c     bmi     $b3bb
        b3af: bd df 02  lda     enemy_along,x
        b3b2: cd 57 01  cmp     $0157
        b3b5: b0 04     bcs     $b3bb
        b3b7: e6 29     inc     $29
        b3b9: e6 29     inc     $29
        b3bb: a5 29     lda     $29
        b3bd: bc cc 02  ldy     $02cc,x
        b3c0: 19 25 04  ora     $0425,y
        b3c3: 99 25 04  sta     $0425,y
        b3c6: bc b9 02  ldy     enemy_seg,x
        b3c9: a5 29     lda     $29
        b3cb: 09 80     ora     #$80
        b3cd: 19 25 04  ora     $0425,y
        b3d0: 99 25 04  sta     $0425,y
        b3d3: ca        dex
        b3d4: 10 ba     bpl     $b390
        b3d6: a9 06     lda     #$06
        b3d8: ac 25 01  ldy     zap_running
        b3db: f0 0c     beq     $b3e9
        b3dd: 30 0a     bmi     $b3e9
        b3df: a5 03     lda     timectr
        b3e1: 29 07     and     #$07
        b3e3: c9 07     cmp     #$07
        b3e5: d0 02     bne     $b3e9
        b3e7: a9 01     lda     #$01
        b3e9: 85 29     sta     $29
        b3eb: a0 ff     ldy     #$ff
        b3ed: a2 ff     ldx     #$ff
        b3ef: 86 2c     stx     $2c
        b3f1: ad 02 02  lda     player_along
        b3f4: f0 0b     beq     $b401
        b3f6: ad 01 02  lda     $0201
        b3f9: 30 06     bmi     $b401
        b3fb: ae 00 02  ldx     player_seg
        b3fe: ac 01 02  ldy     $0201
        b401: 86 2a     stx     $2a
        b403: 84 2b     sty     $2b
        b405: ad 24 01  lda     $0124
        b408: 30 08     bmi     $b412
        b40a: 29 0e     and     #$0e
        b40c: 4a        lsr     a
        b40d: 85 2c     sta     $2c
        b40f: ce 24 01  dec     $0124
        b412: a2 0f     ldx     #$0f
        b414: a0 06     ldy     #$06
        b416: bd 25 04  lda     $0425,x
        b419: f0 0c     beq     $b427
        b41b: 29 02     and     #$02
        b41d: f0 05     beq     $b424
        b41f: a5 03     lda     timectr
        b421: 29 01     and     #$01
        b423: a8        tay
        b424: b8        clv
        b425: 50 24     bvc     $b44b
        b427: e4 2a     cpx     $2a
        b429: f0 02     beq     $b42d
        b42b: e4 2b     cpx     $2b
        b42d: d0 05     bne     $b434
        b42f: a0 01     ldy     #$01
        b431: b8        clv
        b432: 50 17     bvc     $b44b
        b434: ad 24 01  lda     $0124
        b437: 30 10     bmi     $b449
        b439: 8a        txa
        b43a: 18        clc
        b43b: 65 2c     adc     $2c
        b43d: 29 07     and     #$07
        b43f: c9 07     cmp     #$07
        b441: d0 02     bne     $b445
        b443: a9 03     lda     #$03
        b445: a8        tay
        b446: b8        clv
        b447: 50 02     bvc     $b44b
        b449: a4 29     ldy     $29
        b44b: 98        tya
        b44c: bc 76 b4  ldy     $b476,x
        b44f: 91 3b     sta     ($3b),y
        b451: ca        dex
        b452: 10 c0     bpl     $b414
        b454: a2 0f     ldx     #$0f
        b456: 2c 11 01  bit     open_level
        b459: 10 01     bpl     $b45c
        b45b: ca        dex
        b45c: a0 c0     ldy     #$c0
        b45e: bd 25 04  lda     $0425,x
        b461: 10 02     bpl     $b465
        b463: a0 00     ldy     #$00
        b465: 84 58     sty     $58
        b467: bc 87 b4  ldy     $b487,x
        b46a: b1 b0     lda     ($b0),y
        b46c: 29 1f     and     #$1f
        b46e: 05 58     ora     $58
        b470: 91 b0     sta     ($b0),y
        b472: ca        dex
        b473: 10 e7     bpl     $b45c
        b475: 60        rts
; Function unknown.  Used at $b44c.
        b476: .byte   a8
        b477: .byte   9c
        b478: .byte   92
        b479: .byte   86
        b47a: .byte   7c
        b47b: .byte   70
        b47c: .byte   66
        b47d: .byte   5a
        b47e: .byte   50
        b47f: .byte   44
        b480: .byte   3a
        b481: .byte   2e
        b482: .byte   24
        b483: .byte   18
        b484: .byte   0e
        b485: .byte   02
        b486: .byte   b2
        b487: .byte   3b
        b488: .byte   37
        b489: .byte   33
        b48a: .byte   2f
        b48b: .byte   2b
        b48c: .byte   27
        b48d: .byte   23
        b48e: .byte   1f
        b48f: .byte   1b
        b490: .byte   17
        b491: .byte   13
        b492: .byte   0f
        b493: .byte   0b
        b494: .byte   07
        b495: .byte   03
        b496: .byte   3f
        b497: .byte   1d
draw_pending: a0 0c     ldy     #$0c
        b49a: 84 9e     sty     curcolor
        b49c: a9 08     lda     #$08
        b49e: 20 4c df  jsr     vapp_sclstat_A_Y
        b4a1: a2 66     ldx     #$66
        b4a3: 20 65 c7  jsr     vapp_to(X)
        b4a6: a9 12     lda     #$12
        b4a8: 85 56     sta     $56
        b4aa: a2 3f     ldx     #$3f
        b4ac: 86 37     stx     $37
        b4ae: a0 00     ldy     #$00
        b4b0: a6 37     ldx     $37
        b4b2: bd 43 02  lda     pending_vid,x
        b4b5: d0 03     bne     $b4ba
        b4b7: 4c 49 b5  jmp     $b549
        b4ba: c9 50     cmp     #$50
        b4bc: 90 02     bcc     $b4c0
        b4be: c6 37     dec     $37
; Construct a vscale instruction based on the pending_vid,x value
        b4c0: 48        pha
        b4c1: 29 3f     and     #$3f
        b4c3: 91 74     sta     (vidptr_l),y
        b4c5: 68        pla
        b4c6: 2a        rol     a
        b4c7: 2a        rol     a
        b4c8: 2a        rol     a
        b4c9: 29 03     and     #$03
        b4cb: 18        clc
        b4cc: 69 01     adc     #$01
        b4ce: 09 70     ora     #$70
        b4d0: c8        iny
        b4d1: 91 74     sta     (vidptr_l),y
        b4d3: c8        iny
        b4d4: bd 03 02  lda     pending_seg,x
        b4d7: aa        tax
; Construct a long draw (z=0) subtracting screen centre from $03?a,x values
        b4d8: bd 8a 03  lda     $038a,x
        b4db: 38        sec
        b4dc: e5 68     sbc     $68
        b4de: 85 63     sta     $63
        b4e0: 91 74     sta     (vidptr_l),y
        b4e2: c8        iny
        b4e3: bd 7a 03  lda     $037a,x
        b4e6: e5 69     sbc     $69
        b4e8: 85 64     sta     $64
        b4ea: 29 1f     and     #$1f
        b4ec: 91 74     sta     (vidptr_l),y
        b4ee: c8        iny
        b4ef: bd 6a 03  lda     $036a,x
        b4f2: 85 61     sta     $61
        b4f4: 91 74     sta     (vidptr_l),y
        b4f6: c8        iny
        b4f7: bd 5a 03  lda     $035a,x
        b4fa: 85 62     sta     $62
        b4fc: 29 1f     and     #$1f
        b4fe: 91 74     sta     (vidptr_l),y
        b500: c8        iny
; Append a long draw, x=0 y=0 z=5
        b501: a9 00     lda     #$00
        b503: 91 74     sta     (vidptr_l),y
        b505: c8        iny
        b506: 91 74     sta     (vidptr_l),y
        b508: c8        iny
        b509: 91 74     sta     (vidptr_l),y
        b50b: a9 a0     lda     #$a0
        b50d: c8        iny
        b50e: 91 74     sta     (vidptr_l),y
        b510: c8        iny
; Append the negative of the long draw we constructed above.
        b511: a5 63     lda     $63
        b513: 49 ff     eor     #$ff
        b515: 18        clc
        b516: 69 01     adc     #$01
        b518: 91 74     sta     (vidptr_l),y
        b51a: c8        iny
        b51b: a5 64     lda     $64
        b51d: 49 ff     eor     #$ff
        b51f: 69 00     adc     #$00
        b521: 29 1f     and     #$1f
        b523: 91 74     sta     (vidptr_l),y
        b525: c8        iny
        b526: a5 61     lda     $61
        b528: 49 ff     eor     #$ff
        b52a: 18        clc
        b52b: 69 01     adc     #$01
        b52d: 91 74     sta     (vidptr_l),y
        b52f: c8        iny
        b530: a5 62     lda     $62
        b532: 49 ff     eor     #$ff
        b534: 69 00     adc     #$00
        b536: 29 1f     and     #$1f
        b538: 91 74     sta     (vidptr_l),y
        b53a: c8        iny
        b53b: c0 f0     cpy     #$f0
        b53d: 90 06     bcc     $b545
        b53f: 88        dey
        b540: 20 5f df  jsr     inc_vidptr
        b543: a0 00     ldy     #$00
        b545: c6 56     dec     $56
        b547: 30 07     bmi     $b550
        b549: c6 37     dec     $37
        b54b: 30 03     bmi     $b550
        b54d: 4c b0 b4  jmp     $b4b0
        b550: 98        tya
        b551: f0 04     beq     $b557
        b553: 88        dey
        b554: 20 5f df  jsr     inc_vidptr
; Anti-piracy code.  If the copyright string has been tampered with,
; and player 1's level is over 10, set $53 to $7a.
; I'm not sure what this does, but it can't be good. :-)
        b557: a5 b5     lda     copyr_cksum
        b559: f0 0a     beq     $b565
        b55b: a5 46     lda     p1_level
        b55d: c9 0a     cmp     #$0a
        b55f: 90 04     bcc     $b565
        b561: a9 7a     lda     #$7a
        b563: 85 53     sta     $53
; End anti-piracy code.
        b565: a9 01     lda     #$01
        b567: 4c 6a df  jmp     vapp_scale_A,0
        b56a: 48        pha
        b56b: a0 00     ldy     #$00
        b56d: 98        tya
        b56e: 91 74     sta     (vidptr_l),y
        b570: c8        iny
        b571: 91 74     sta     (vidptr_l),y
        b573: c8        iny
        b574: 91 74     sta     (vidptr_l),y
        b576: c8        iny
        b577: 68        pla
        b578: 91 74     sta     (vidptr_l),y
        b57a: a9 04     lda     #$04
        b57c: 18        clc
        b57d: 65 74     adc     vidptr_l
        b57f: 85 74     sta     vidptr_l
        b581: 90 02     bcc     $b585
        b583: e6 75     inc     vidptr_h
        b585: 60        rts
 draw_player: a9 01     lda     #$01
        b588: 85 9e     sta     curcolor
        b58a: ad 02 02  lda     player_along
        b58d: f0 1d     beq     $b5ac
        b58f: c9 f0     cmp     #$f0
        b591: b0 19     bcs     $b5ac
        b593: 85 57     sta     $57
        b595: 85 2f     sta     $2f
        b597: ad 01 02  lda     $0201
        b59a: c9 81     cmp     #$81
        b59c: f0 0e     beq     $b5ac
        b59e: ac 00 02  ldy     player_seg
        b5a1: a5 51     lda     $51
        b5a3: 4a        lsr     a
        b5a4: 29 07     and     #$07
        b5a6: 18        clc
        b5a7: 69 01     adc     #$01
        b5a9: 20 a0 bd  jsr     draw_linegfx
        b5ac: 60        rts
draw_enemies: ad 06 01  lda     $0106
        b5b0: 30 24     bmi     $b5d6
        b5b2: a2 06     ldx     #$06
        b5b4: 86 37     stx     $37
        b5b6: a6 37     ldx     $37
        b5b8: bd df 02  lda     enemy_along,x
        b5bb: f0 15     beq     $b5d2
        b5bd: 85 57     sta     $57
        b5bf: bd 83 02  lda     $0283,x
        b5c2: 29 18     and     #$18
        b5c4: 4a        lsr     a
        b5c5: 4a        lsr     a
        b5c6: 4a        lsr     a
        b5c7: 85 55     sta     $55
        b5c9: bd 83 02  lda     $0283,x
        b5cc: 29 07     and     #$07
        b5ce: 0a        asl     a
        b5cf: 20 d7 b5  jsr     $b5d7
        b5d2: c6 37     dec     $37
        b5d4: 10 e0     bpl     $b5b6
        b5d6: 60        rts
        b5d7: a8        tay
        b5d8: b9 e2 b5  lda     $b5e2,y
        b5db: 48        pha
        b5dc: b9 e1 b5  lda     $b5e1,y
        b5df: 48        pha
        b5e0: 60        rts
; Indexed by enemy type.  Draws enemy.
        b5e1: .jump   $b5eb ; flipper
        b5e3: .jump   $b71b ; pulsar
        b5e5: .jump   $b60f ; tanker
        b5e7: .jump   $b622 ; spiker
        b5e9: .jump   $b69b ; fuzzball
; Code to draw a flipper.
        b5eb: a9 03     lda     #$03
        b5ed: 85 9e     sta     curcolor
        b5ef: bd 83 02  lda     $0283,x
        b5f2: 30 0e     bmi     $b602
        b5f4: bc b9 02  ldy     enemy_seg,x
        b5f7: a6 55     ldx     $55
        b5f9: bd 0b b6  lda     $b60b,x
        b5fc: 20 a0 bd  jsr     draw_linegfx
        b5ff: b8        clv
        b600: 50 08     bvc     $b60a
        b602: 20 34 b6  jsr     $b634
        b605: a0 00     ldy     #$00
        b607: 20 cb bd  jsr     $bdcb
        b60a: 60        rts
; Graphic numbers for flippers.  Indexed by $18 bits of $0283 value.
        b60b: .byte   00
        b60c: .byte   00
        b60d: .byte   00
        b60e: .byte   00
; Code to draw a tanker.
        b60f: bd 8a 02  lda     $028a,x
        b612: 29 03     and     #$03
        b614: a8        tay
        b615: b9 1e b6  lda     $b61e,y
        b618: bc b9 02  ldy     enemy_seg,x
        b61b: 4c fd bc  jmp     graphic_at_mid
; Graphic number for tankers.  Indexed by contents value ($028a bits $03).
        b61e: .byte   1a
        b61f: .byte   1a
        b620: .byte   4a
        b621: .byte   4c
; Code to draw a spiker.
        b622: bc b9 02  ldy     enemy_seg,x
        b625: a5 03     lda     timectr
        b627: 29 03     and     #$03
        b629: 0a        asl     a
        b62a: 18        clc
        b62b: 69 12     adc     #$12
        b62d: 4c fd bc  jmp     graphic_at_mid
; Not used; a table version of the value computed by $b629-$b62b.
        b630: .byte   12
        b631: .byte   14
        b632: .byte   16
        b633: .byte   18
        b634: a5 57     lda     $57
        b636: 85 2f     sta     $2f
        b638: bc b9 02  ldy     enemy_seg,x
        b63b: b9 ce 03  lda     tube_x,y
        b63e: 85 56     sta     $56
        b640: b9 de 03  lda     tube_y,y
        b643: 85 58     sta     $58
        b645: bd cc 02  lda     $02cc,x
        b648: 29 0f     and     #$0f
        b64a: a8        tay
        b64b: a5 56     lda     $56
        b64d: 49 80     eor     #$80
        b64f: 18        clc
        b650: 79 8b b6  adc     $b68b,y
        b653: 50 09     bvc     $b65e
        b655: 10 05     bpl     $b65c
        b657: a9 7f     lda     #$7f
        b659: b8        clv
        b65a: 50 02     bvc     $b65e
        b65c: a9 80     lda     #$80
        b65e: 49 80     eor     #$80
        b660: 85 2e     sta     $2e
        b662: a5 58     lda     $58
        b664: 49 80     eor     #$80
        b666: 18        clc
        b667: 79 87 b6  adc     $b687,y
        b66a: 50 09     bvc     $b675
        b66c: 10 05     bpl     $b673
        b66e: a9 7f     lda     #$7f
        b670: b8        clv
        b671: 50 02     bvc     $b675
        b673: a9 80     lda     #$80
        b675: 49 80     eor     #$80
        b677: 85 30     sta     $30
        b679: ac 12 01  ldy     curtube
        b67c: b9 dc bc  lda     lev_fscale,y
        b67f: 85 59     sta     fscale
        b681: b9 ec bc  lda     lev_fscale2,y
        b684: 85 5a     sta     fscale+1
        b686: 60        rts
; Used - apparently overlappingly - at $b667 and $b68b.
; Maybe this is a sine wave, and the overlapping is sin-vs-cos?
        b687: .byte   00
        b688: .byte   10
        b689: .byte   1f
        b68a: .byte   28
        b68b: .byte   2c
        b68c: .byte   28
        b68d: .byte   1f
        b68e: .byte   10
        b68f: .byte   00
        b690: .byte   f0
        b691: .byte   e1
        b692: .byte   d8
        b693: .byte   d4
        b694: .byte   d8
        b695: .byte   e1
        b696: .byte   f0
        b697: .byte   00
        b698: .byte   10
        b699: .byte   1f
        b69a: .byte   28
; Code to draw a fuzzball.
        b69b: bd df 02  lda     enemy_along,x
        b69e: 85 57     sta     $57
        b6a0: bc b9 02  ldy     enemy_seg,x
        b6a3: b9 ce 03  lda     tube_x,y
        b6a6: 85 56     sta     $56
        b6a8: b9 de 03  lda     tube_y,y
        b6ab: 85 58     sta     $58
        b6ad: bd cc 02  lda     $02cc,x
        b6b0: 10 23     bpl     $b6d5
        b6b2: 98        tya
        b6b3: 18        clc
        b6b4: 69 01     adc     #$01
        b6b6: 29 0f     and     #$0f
        b6b8: a8        tay
        b6b9: b9 ce 03  lda     tube_x,y
        b6bc: 38        sec
        b6bd: e5 56     sbc     $56
        b6bf: 20 fa b6  jsr     $b6fa
        b6c2: 18        clc
        b6c3: 65 56     adc     $56
        b6c5: 85 56     sta     $56
        b6c7: b9 de 03  lda     tube_y,y
        b6ca: 38        sec
        b6cb: e5 58     sbc     $58
        b6cd: 20 fa b6  jsr     $b6fa
        b6d0: 18        clc
        b6d1: 65 58     adc     $58
        b6d3: 85 58     sta     $58
        b6d5: 20 98 c0  jsr     $c098
        b6d8: a2 61     ldx     #$61
        b6da: 20 65 c7  jsr     vapp_to(X)
        b6dd: a9 00     lda     #$00
        b6df: 85 a9     sta     $a9
        b6e1: 20 3e bd  jsr     $bd3e
        b6e4: 84 a9     sty     $a9
        b6e6: a5 03     lda     timectr
        b6e8: 29 03     and     #$03
        b6ea: 0a        asl     a
        b6eb: 18        clc
        b6ec: 69 4e     adc     #$4e
        b6ee: a8        tay
        b6ef: be c9 ce  ldx     graphic_table+1,y
        b6f2: b9 c8 ce  lda     graphic_table,y
        b6f5: a4 a9     ldy     $a9
        b6f7: 4c 59 df  jmp     vapp_A_X
        b6fa: 85 29     sta     $29
        b6fc: bd cc 02  lda     $02cc,x
        b6ff: 29 07     and     #$07
        b701: 85 2c     sta     $2c
        b703: 86 2b     stx     $2b
        b705: a2 02     ldx     #$02
        b707: a9 00     lda     #$00
        b709: 46 2c     lsr     $2c
        b70b: 90 03     bcc     $b710
        b70d: 18        clc
        b70e: 65 29     adc     $29
        b710: 0a        asl     a
        b711: 08        php
        b712: 6a        ror     a
        b713: 28        plp
        b714: 6a        ror     a
        b715: ca        dex
        b716: 10 f1     bpl     $b709
        b718: a6 2b     ldx     $2b
        b71a: 60        rts
; Code to draw a pulsar.
        b71b: a9 04     lda     #$04
        b71d: ac 48 01  ldy     pulsing
        b720: 30 02     bmi     $b724
        b722: a9 00     lda     #$00
        b724: 85 9e     sta     curcolor
        b726: ad 48 01  lda     pulsing
        b729: 18        clc
        b72a: 69 40     adc     #$40
        b72c: 4a        lsr     a
        b72d: 4a        lsr     a
        b72e: 4a        lsr     a
        b72f: 4a        lsr     a
        b730: c9 05     cmp     #$05
        b732: 90 02     bcc     $b736
        b734: a9 00     lda     #$00
        b736: a8        tay
        b737: b9 55 b7  lda     $b755,y
        b73a: 85 29     sta     $29
        b73c: bd 83 02  lda     $0283,x
        b73f: 30 0b     bmi     $b74c
        b741: bc b9 02  ldy     enemy_seg,x
        b744: a5 29     lda     $29
        b746: 20 a0 bd  jsr     draw_linegfx
        b749: b8        clv
        b74a: 50 08     bvc     $b754
        b74c: 20 34 b6  jsr     $b634
        b74f: a4 29     ldy     $29
        b751: 20 cb bd  jsr     $bdcb
        b754: 60        rts
        b755: .byte   0d
        b756: .byte   0c
        b757: .byte   0b
        b758: .byte   0a
        b759: .byte   09
        b75a: .byte   09
  draw_shots: a2 0b     ldx     #$0b
        b75d: 86 37     stx     $37
        b75f: a6 37     ldx     $37
        b761: bd d3 02  lda     ply_shotpos,x
        b764: f0 1b     beq     $b781
        b766: 85 57     sta     $57
        b768: 85 2f     sta     $2f
        b76a: e0 08     cpx     #$08 ; player or enemy shot?
        b76c: bc ad 02  ldy     ply_shotseg,x
        b76f: b0 05     bcs     $b776
        b771: a9 08     lda     #$08
        b773: b8        clv
        b774: 50 08     bvc     $b77e
        b776: a5 03     lda     timectr
        b778: 0a        asl     a
        b779: 29 06     and     #$06
        b77b: 18        clc
        b77c: 69 20     adc     #$20
        b77e: 20 fd bc  jsr     graphic_at_mid
        b781: c6 37     dec     $37
        b783: 10 da     bpl     $b75f
        b785: a0 04     ldy     #$04
        b787: ad 35 01  lda     ply_shotcnt
        b78a: c9 06     cmp     #$06
        b78c: 90 08     bcc     $b796
        b78e: a0 0b     ldy     #$0b
        b790: c9 08     cmp     #$08
        b792: 90 02     bcc     $b796
        b794: a0 0c     ldy     #$0c
        b796: 8c 08 08  sty     $0808
        b799: 60        rts
draw_explosions: a0 00     ldy     #$00
        b79c: 84 9e     sty     curcolor
        b79e: a2 07     ldx     #$07
        b7a0: 86 37     stx     $37
        b7a2: a6 37     ldx     $37
        b7a4: bd 0a 03  lda     $030a,x
        b7a7: f0 29     beq     $b7d2
        b7a9: 85 57     sta     $57
        b7ab: bd fa 02  lda     $02fa,x
        b7ae: 85 29     sta     $29
        b7b0: bc 02 03  ldy     $0302,x
        b7b3: c0 01     cpy     #$01
        b7b5: d0 06     bne     $b7bd
        b7b7: 20 eb b7  jsr     vapp_mid_graphic
        b7ba: b8        clv
        b7bb: 50 15     bvc     $b7d2
        b7bd: bd 12 03  lda     $0312,x
        b7c0: 4a        lsr     a
        b7c1: 29 fe     and     #$fe
        b7c3: c0 02     cpy     #$02
        b7c5: 90 02     bcc     $b7c9
        b7c7: a9 00     lda     #$00
        b7c9: 18        clc
        b7ca: 79 e5 b7  adc     $b7e5,y
        b7cd: a4 29     ldy     $29
        b7cf: 20 fd bc  jsr     graphic_at_mid
        b7d2: c6 37     dec     $37
        b7d4: 10 cc     bpl     $b7a2
        b7d6: ad 20 07  lda     $0720
        b7d9: f0 09     beq     $b7e4
        b7db: a5 9f     lda     curlevel
        b7dd: c9 0d     cmp     #$0d
        b7df: 90 03     bcc     $b7e4
        b7e1: 8d ff 01  sta     $01ff
        b7e4: 60        rts
; Table of graphics.  See $b7ca
        b7e5: .byte   00
        b7e6: .byte   00
        b7e7: .byte   5a
        b7e8: .byte   58
        b7e9: 56 1c     lsr     $1c,x
vapp_mid_graphic: a4 29     ldy     $29
        b7ed: b9 35 04  lda     mid_x,y
        b7f0: 85 56     sta     $56
        b7f2: b9 45 04  lda     mid_y,y
        b7f5: 85 58     sta     $58
        b7f7: 20 98 c0  jsr     $c098
        b7fa: a2 61     ldx     #$61
        b7fc: 20 65 c7  jsr     vapp_to(X)
        b7ff: ae 3b 01  ldx     $013b
        b802: ce 3c 01  dec     $013c
        b805: d0 0a     bne     $b811
        b807: e8        inx
        b808: 8e 3b 01  stx     $013b
        b80b: bd 2a b8  lda     $b82a,x
        b80e: 8d 3c 01  sta     $013c
        b811: bc 3d b8  ldy     $b83d,x
        b814: 30 03     bmi     $b819
        b816: 20 4e b8  jsr     $b84e
        b819: ad 3b 01  lda     $013b
        b81c: 0a        asl     a
        b81d: 18        clc
        b81e: 69 28     adc     #$28 ; hit-by-shot explosion
        b820: a8        tay
        b821: be c9 ce  ldx     graphic_table+1,y
        b824: b9 c8 ce  lda     graphic_table,y
        b827: 4c 57 df  jmp     vapp_A_X_Y=0
        b82a: .byte   02
        b82b: .byte   02
        b82c: .byte   02
        b82d: .byte   02
        b82e: .byte   02
        b82f: .byte   04
        b830: .byte   03
        b831: .byte   02
        b832: .byte   01
        b833: .byte   20
        b834: .byte   03
        b835: .byte   03
        b836: .byte   03
        b837: .byte   03
        b838: .byte   03
        b839: .byte   03
        b83a: .byte   03
        b83b: .byte   3b
        b83c: .byte   b8
        b83d: .byte   00
        b83e: .byte   02
        b83f: .byte   02
        b840: .byte   02
        b841: .byte   02
        b842: .byte   02
        b843: .byte   02
        b844: .byte   02
        b845: .byte   04
        b846: .byte   06
        b847: .byte   ff
        b848: .byte   ff
        b849: .byte   ff
        b84a: .byte   ff
        b84b: .byte   ff
        b84c: .byte   ff
        b84d: .byte   ff
        b84e: b9 58 b8  lda     $b858,y
        b851: 48        pha
        b852: b9 57 b8  lda     $b857,y
        b855: 48        pha
        b856: 60        rts
        b857: .word   b85e
        b859: .word   b874
        b85b: .word   b887
        b85d: .word   b895
        b85f: a9 0c     lda     #$0c
        b861: 8d 0b 08  sta     $080b
        b864: 85 24     sta     $24
        b866: a9 04     lda     #$04
        b868: 8d 0a 08  sta     $080a
        b86b: 85 23     sta     $23
        b86d: a9 00     lda     #$00
        b86f: 85 22     sta     $22
        b871: 8d 09 08  sta     $0809
        b874: 60        rts
        b875: a4 22     ldy     $22
        b877: a2 02     ldx     #$02
        b879: b5 22     lda     $22,x
        b87b: 48        pha
        b87c: 94 22     sty     $22,x
        b87e: 98        tya
        b87f: 9d 09 08  sta     $0809,x
        b882: 68        pla
        b883: a8        tay
        b884: ca        dex
        b885: 10 f2     bpl     $b879
        b887: 60        rts
        b888: 20 96 c1  jsr     setcolours
        b88b: a9 7f     lda     #$7f
        b88d: 8d 39 01  sta     $0139
        b890: a9 04     lda     #$04
        b892: 8d 3a 01  sta     $013a
        b895: 60        rts
        b896: ad 39 01  lda     $0139
        b899: 8d fc 2f  sta     $2ffc
        b89c: ad 3a 01  lda     $013a
        b89f: 09 70     ora     #$70
        b8a1: 8d fd 2f  sta     $2ffd
        b8a4: a9 c0     lda     #$c0
        b8a6: 8d ff 2f  sta     $2fff
        b8a9: ad 39 01  lda     $0139
        b8ac: 38        sec
        b8ad: e9 20     sbc     #$20
        b8af: 10 05     bpl     $b8b6
        b8b1: 29 7f     and     #$7f
        b8b3: ce 3a 01  dec     $013a
        b8b6: 8d 39 01  sta     $0139
        b8b9: 60        rts
; $3ff2 = code to move to extreme corners of the screen, not drawing.
        b8ba: a9 3f     lda     #$3f
        b8bc: a2 f2     ldx     #$f2
        b8be: 20 39 df  jsr     vapp_vjsr_AX
        b8c1: a9 00     lda     #$00
        b8c3: 85 6a     sta     $6a
        b8c5: 85 6b     sta     $6b
        b8c7: 85 6c     sta     $6c
        b8c9: 85 6d     sta     $6d
        b8cb: 8d 02 02  sta     player_along
        b8ce: 85 68     sta     $68
        b8d0: 85 69     sta     $69
        b8d2: a9 e0     lda     #$e0
        b8d4: 85 5f     sta     $5f
        b8d6: a9 ff     lda     #$ff
        b8d8: 85 5b     sta     $5b
        b8da: 20 67 b9  jsr     $b967
        b8dd: 85 77     sta     $77
        b8df: 86 76     stx     $76
        b8e1: a2 0f     ldx     #$0f
        b8e3: 86 37     stx     $37
        b8e5: a6 37     ldx     $37
        b8e7: bd 83 02  lda     $0283,x
        b8ea: f0 49     beq     $b935
        b8ec: 85 57     sta     $57
        b8ee: bd 63 02  lda     $0263,x
        b8f1: 85 56     sta     $56
        b8f3: bd a3 02  lda     $02a3,x
        b8f6: 85 58     sta     $58
        b8f8: 20 98 c0  jsr     $c098
        b8fb: a9 00     lda     #$00
        b8fd: 85 73     sta     draw_z
        b8ff: 20 44 b9  jsr     $b944
        b902: 20 ba c3  jsr     $c3ba
        b905: a9 a0     lda     #$a0
        b907: 20 6a b5  jsr     $b56a
        b90a: 20 44 b9  jsr     $b944
        b90d: a2 61     ldx     #$61
        b90f: 20 72 c7  jsr     $c772
        b912: 20 55 b9  jsr     $b955
        b915: 20 6c df  jsr     vapp_scale_A,Y
        b918: a5 37     lda     $37
        b91a: 29 07     and     #$07
        b91c: c9 07     cmp     #$07
        b91e: d0 02     bne     $b922
        b920: a9 00     lda     #$00
        b922: a8        tay
        b923: 84 9e     sty     curcolor
        b925: a9 08     lda     #$08
        b927: 20 4c df  jsr     vapp_sclstat_A_Y
        b92a: a9 00     lda     #$00
        b92c: 20 4a df  jsr     vapp_sclstat_A_73
        b92f: 20 67 b9  jsr     $b967
        b932: 20 39 df  jsr     vapp_vjsr_AX
        b935: c6 37     dec     $37
        b937: 10 ac     bpl     $b8e5
        b939: 20 44 b9  jsr     $b944
        b93c: a9 01     lda     #$01
        b93e: 20 6a df  jsr     vapp_scale_A,0
        b941: 20 09 df  jsr     vapp_rts
        b944: a6 74     ldx     vidptr_l
        b946: a4 75     ldy     vidptr_h
        b948: a5 76     lda     $76
        b94a: 85 74     sta     vidptr_l
        b94c: 86 76     stx     $76
        b94e: a5 77     lda     $77
        b950: 85 75     sta     vidptr_h
        b952: 84 77     sty     $77
        b954: 60        rts
        b955: a5 57     lda     $57
        b957: 4a        lsr     a
        b958: 4a        lsr     a
        b959: 4a        lsr     a
        b95a: 4a        lsr     a
        b95b: a0 00     ldy     #$00
        b95d: c8        iny
        b95e: 4a        lsr     a
        b95f: d0 fc     bne     $b95d
        b961: 18        clc
        b962: 69 02     adc     #$02
        b964: a0 00     ldy     #$00
        b966: 60        rts
        b967: ad 15 04  lda     dblbuf_flg
        b96a: f0 09     beq     $b975
        b96c: ad 6f ce  lda     $ce6f
        b96f: ae 6e ce  ldx     $ce6e
        b972: b8        clv
        b973: 50 06     bvc     $b97b
        b975: ad 87 ce  lda     $ce87
        b978: ae 86 ce  ldx     $ce86
        b97b: 60        rts
; Two-dimensional arrays are indexed by level number first: x[L][P], etc.
; x,y: coordinates of points
; angle: angles of sectors (0-15 represents 0-360 degrees)
; remap: order levels are encountered in
; scale: (reciprocal of) scale of level
; y3d: 3-D Y offset - "camera height"
; y2d,y2db: 2-D Y offset - signed; y2db is high byte, y2d low
; open: $00 if level is closed, $ff if open
; fscale,fscale2: (reciprocal of) flipper scale - fscale2<<7 | fscale
       lev_x: .chunk  256 ; x[16][16]
       lev_y: .chunk  256 ; y[16][16]
   lev_angle: .chunk  256 ; angle[16][16]
   lev_remap: .chunk  16 ; remap[16]
   lev_scale: .chunk  16 ; scale[16]
     lev_y3d: .chunk  16 ; y3d[16]
     lev_y2d: .chunk  16 ; y2d[16]
    lev_y2db: .chunk  16 ; y2db[16]
    lev_open: .chunk  16 ; open[16]
  lev_fscale: .chunk  16 ; fscale[16]
 lev_fscale2: .chunk  16 ; fscale2[16]
        bcfc: .byte   3e
graphic_at_mid: 85 55     sta     $55
        bcff: b9 35 04  lda     mid_x,y
        bd02: 85 56     sta     $56
        bd04: b9 45 04  lda     mid_y,y
        bd07: 85 58     sta     $58
        bd09: 20 98 c0  jsr     $c098
        bd0c: a2 61     ldx     #$61
        bd0e: 20 65 c7  jsr     vapp_to(X)
        bd11: a9 00     lda     #$00
        bd13: 85 a9     sta     $a9
        bd15: 20 3e bd  jsr     $bd3e
        bd18: a5 78     lda     $78
        bd1a: 49 07     eor     #$07
        bd1c: 0a        asl     a
        bd1d: c9 0a     cmp     #$0a
        bd1f: b0 02     bcs     $bd23
        bd21: a9 0a     lda     #$0a
        bd23: 0a        asl     a
        bd24: 0a        asl     a
        bd25: 0a        asl     a
        bd26: 0a        asl     a
        bd27: 91 74     sta     (vidptr_l),y
        bd29: c8        iny
        bd2a: a9 60     lda     #$60
        bd2c: 91 74     sta     (vidptr_l),y
        bd2e: c8        iny
        bd2f: 84 a9     sty     $a9
        bd31: a4 55     ldy     $55
        bd33: be c9 ce  ldx     graphic_table+1,y
        bd36: b9 c8 ce  lda     graphic_table,y
        bd39: a4 a9     ldy     $a9
        bd3b: 4c 59 df  jmp     vapp_A_X
        bd3e: a5 57     lda     $57
        bd40: c9 10     cmp     #$10
        bd42: 90 48     bcc     $bd8c
        bd44: 38        sec
        bd45: e5 5f     sbc     $5f
        bd47: 8d 95 60  sta     mb_w_15
        bd4a: a9 00     lda     #$00
        bd4c: e5 5b     sbc     $5b
        bd4e: 8d 96 60  sta     mb_w_16
        bd51: a9 18     lda     #$18
        bd53: 8d 8c 60  sta     mb_w_0c
        bd56: a5 a0     lda     $a0
        bd58: 8d 8e 60  sta     mb_w_0e
        bd5b: 8d 94 60  sta     mb_w_14
        bd5e: 2c 40 60  bit     eactl_mbst
        bd61: 30 fb     bmi     $bd5e
        bd63: ad 60 60  lda     mb_rd_l
        bd66: 85 79     sta     $79
        bd68: ad 70 60  lda     mb_rd_h
        bd6b: 85 7a     sta     $7a
        bd6d: a2 0f     ldx     #$0f
        bd6f: 8e 8c 60  stx     mb_w_0c
        bd72: 38        sec
        bd73: e9 01     sbc     #$01
        bd75: d0 02     bne     $bd79
        bd77: a9 01     lda     #$01
        bd79: a2 00     ldx     #$00
        bd7b: e8        inx
        bd7c: 06 79     asl     $79
        bd7e: 2a        rol     a
        bd7f: 90 fa     bcc     $bd7b
        bd81: 4a        lsr     a
        bd82: 49 7f     eor     #$7f
        bd84: 18        clc
        bd85: 69 01     adc     #$01
        bd87: a8        tay
        bd88: 8a        txa
        bd89: b8        clv
        bd8a: 50 04     bvc     $bd90
        bd8c: a9 01     lda     #$01
        bd8e: a0 00     ldy     #$00
        bd90: 85 78     sta     $78
        bd92: 48        pha
        bd93: 98        tya
        bd94: a4 a9     ldy     $a9
        bd96: 91 74     sta     (vidptr_l),y
        bd98: c8        iny
        bd99: 68        pla
        bd9a: 09 70     ora     #$70
        bd9c: 91 74     sta     (vidptr_l),y
        bd9e: c8        iny
        bd9f: 60        rts
; Draw a rotatable/scalable graphic.
; Y = segment number
; A = graphic to draw
; $57 = position along tube
draw_linegfx: 85 36     sta     $36
        bda2: b9 ce 03  lda     tube_x,y
        bda5: 85 56     sta     $56
        bda7: b9 de 03  lda     tube_y,y
        bdaa: 85 58     sta     $58
        bdac: a5 57     lda     $57
        bdae: 85 2f     sta     $2f
        bdb0: 98        tya
        bdb1: 18        clc
        bdb2: 69 01     adc     #$01
        bdb4: 29 0f     and     #$0f
        bdb6: aa        tax
        bdb7: bd ce 03  lda     tube_x,x
        bdba: 85 2e     sta     $2e
        bdbc: bd de 03  lda     tube_y,x
        bdbf: 85 30     sta     $30
        bdc1: a9 00     lda     #$00
        bdc3: 85 59     sta     fscale
        bdc5: a9 04     lda     #$04
        bdc7: 85 5a     sta     fscale+1
        bdc9: a4 36     ldy     $36
        bdcb: a5 5b     lda     $5b
        bdcd: 30 07     bmi     $bdd6
        bdcf: a5 57     lda     $57
        bdd1: c5 5f     cmp     $5f
        bdd3: b0 01     bcs     $bdd6
        bdd5: 60        rts
        bdd6: b9 b6 bf  lda     $bfb6,y
        bdd9: 85 99     sta     rgr_pt_inx
        bddb: b9 c4 bf  lda     $bfc4,y
        bdde: 85 38     sta     $38
        bde0: a4 9e     ldy     curcolor
        bde2: a9 08     lda     #$08
        bde4: 20 4c df  jsr     vapp_sclstat_A_Y
        bde7: 20 98 c0  jsr     $c098
        bdea: a2 61     ldx     #$61
        bdec: 20 65 c7  jsr     vapp_to(X)
        bdef: a5 2e     lda     $2e
        bdf1: 85 56     sta     $56
        bdf3: a5 2f     lda     $2f
        bdf5: 85 57     sta     $57
        bdf7: a5 30     lda     $30
        bdf9: 85 58     sta     $58
        bdfb: 20 98 c0  jsr     $c098
        bdfe: a4 59     ldy     fscale
        be00: a5 5a     lda     fscale+1
        be02: 20 6c df  jsr     vapp_scale_A,Y
        be05: a5 61     lda     $61
        be07: 38        sec
        be08: e5 6a     sbc     $6a
        be0a: 85 79     sta     $79
        be0c: a5 62     lda     $62
        be0e: e5 6b     sbc     $6b
        be10: 85 9b     sta     $9b
        be12: 30 09     bmi     $be1d
        be14: f0 04     beq     $be1a
        be16: a9 ff     lda     #$ff
        be18: 85 79     sta     $79
        be1a: b8        clv
        be1b: 50 16     bvc     $be33
        be1d: c9 ff     cmp     #$ff
        be1f: f0 05     beq     $be26
        be21: a9 ff     lda     #$ff
        be23: b8        clv
        be24: 50 0b     bvc     $be31
        be26: a5 79     lda     $79
        be28: 49 ff     eor     #$ff
        be2a: 18        clc
        be2b: 69 01     adc     #$01
        be2d: 90 02     bcc     $be31
        be2f: a9 ff     lda     #$ff
        be31: 85 79     sta     $79
        be33: a5 63     lda     $63
        be35: 38        sec
        be36: e5 6c     sbc     $6c
        be38: 85 89     sta     $89
        be3a: a5 64     lda     $64
        be3c: e5 6d     sbc     $6d
        be3e: 85 9d     sta     $9d
        be40: 30 09     bmi     $be4b
        be42: f0 04     beq     $be48
        be44: a9 ff     lda     #$ff
        be46: 85 89     sta     $89
        be48: b8        clv
        be49: 50 12     bvc     $be5d
        be4b: c9 ff     cmp     #$ff
        be4d: f0 05     beq     $be54
        be4f: a9 ff     lda     #$ff
        be51: b8        clv
        be52: 50 07     bvc     $be5b
        be54: a5 89     lda     $89
        be56: 49 ff     eor     #$ff
        be58: 18        clc
        be59: 69 01     adc     #$01
        be5b: 85 89     sta     $89
        be5d: a9 00     lda     #$00
        be5f: 85 82     sta     $82
        be61: 85 92     sta     $92
        be63: a5 79     lda     $79
        be65: 0a        asl     a
        be66: 26 82     rol     $82
        be68: 85 7a     sta     $7a
        be6a: 0a        asl     a
        be6b: 85 7c     sta     $7c
        be6d: a5 82     lda     $82
        be6f: 2a        rol     a
        be70: 85 84     sta     $84
        be72: a5 7c     lda     $7c
        be74: 65 79     adc     $79
        be76: 85 7d     sta     $7d
        be78: a5 84     lda     $84
        be7a: 69 00     adc     #$00
        be7c: 85 85     sta     $85
        be7e: a5 7a     lda     $7a
        be80: 65 79     adc     $79
        be82: 85 7b     sta     $7b
        be84: a5 82     lda     $82
        be86: 69 00     adc     #$00
        be88: 85 83     sta     $83
        be8a: 85 86     sta     $86
        be8c: a5 7b     lda     $7b
        be8e: 0a        asl     a
        be8f: 85 7e     sta     $7e
        be91: 26 86     rol     $86
        be93: 65 79     adc     $79
        be95: 85 7f     sta     $7f
        be97: a5 86     lda     $86
        be99: 69 00     adc     #$00
        be9b: 85 87     sta     $87
        be9d: a5 89     lda     $89
        be9f: 0a        asl     a
        bea0: 26 92     rol     $92
        bea2: 85 8a     sta     $8a
        bea4: 0a        asl     a
        bea5: 85 8c     sta     $8c
        bea7: a5 92     lda     $92
        bea9: 2a        rol     a
        beaa: 85 94     sta     $94
        beac: a5 8c     lda     $8c
        beae: 65 89     adc     $89
        beb0: 85 8d     sta     $8d
        beb2: a5 94     lda     $94
        beb4: 69 00     adc     #$00
        beb6: 85 95     sta     $95
        beb8: a5 8a     lda     $8a
        beba: 65 89     adc     $89
        bebc: 85 8b     sta     $8b
        bebe: a5 92     lda     $92
        bec0: 69 00     adc     #$00
        bec2: 85 93     sta     $93
        bec4: 85 96     sta     $96
        bec6: a5 8b     lda     $8b
        bec8: 0a        asl     a
        bec9: 85 8e     sta     $8e
        becb: 26 96     rol     $96
        becd: 65 89     adc     $89
        becf: 85 8f     sta     $8f
        bed1: a5 96     lda     $96
        bed3: 69 00     adc     #$00
        bed5: 85 97     sta     $97
        bed7: a0 00     ldy     #$00
        bed9: 84 a9     sty     $a9
; Top of loop for points in claw
        bedb: a4 38     ldy     $38
        bedd: b9 d3 bf  lda     $bfd3,y
        bee0: c9 01     cmp     #$01
        bee2: d0 02     bne     $bee6
        bee4: a9 c0     lda     #$c0
        bee6: 85 73     sta     draw_z
        bee8: b9 d2 bf  lda     $bfd2,y
        beeb: 85 2d     sta     $2d
        beed: c8        iny
        beee: c8        iny
        beef: 84 38     sty     $38
        bef1: aa        tax
        bef2: 29 07     and     #$07
        bef4: a8        tay
        bef5: 8a        txa
        bef6: 0a        asl     a
        bef7: 85 2b     sta     $2b
        bef9: 4a        lsr     a
        befa: 4a        lsr     a
        befb: 4a        lsr     a
        befc: 4a        lsr     a
        befd: 29 07     and     #$07
        beff: aa        tax
        bf00: a5 2b     lda     $2b
        bf02: 45 9b     eor     $9b
        bf04: 30 0b     bmi     $bf11
        bf06: b9 78 00  lda     $0078,y
        bf09: 85 61     sta     $61
        bf0b: b9 80 00  lda     $0080,y
        bf0e: b8        clv
        bf0f: 50 11     bvc     $bf22
        bf11: b9 78 00  lda     $0078,y
        bf14: 49 ff     eor     #$ff
        bf16: 18        clc
        bf17: 69 01     adc     #$01
        bf19: 85 61     sta     $61
        bf1b: b9 80 00  lda     $0080,y
        bf1e: 49 ff     eor     #$ff
        bf20: 69 00     adc     #$00
        bf22: 85 62     sta     $62
        bf24: a5 2d     lda     $2d
        bf26: 45 9d     eor     $9d
        bf28: 10 0e     bpl     $bf38
        bf2a: b5 88     lda     $88,x
        bf2c: 18        clc
        bf2d: 65 61     adc     $61
        bf2f: 85 61     sta     $61
        bf31: b5 90     lda     $90,x
        bf33: 65 62     adc     $62
        bf35: b8        clv
        bf36: 50 0b     bvc     $bf43
        bf38: a5 61     lda     $61
        bf3a: 38        sec
        bf3b: f5 88     sbc     $88,x
        bf3d: 85 61     sta     $61
        bf3f: a5 62     lda     $62
        bf41: f5 90     sbc     $90,x
        bf43: 85 62     sta     $62
        bf45: a5 2b     lda     $2b
        bf47: 45 9d     eor     $9d
        bf49: 30 0b     bmi     $bf56
        bf4b: b9 88 00  lda     $0088,y
        bf4e: 85 63     sta     $63
        bf50: b9 90 00  lda     $0090,y
        bf53: b8        clv
        bf54: 50 11     bvc     $bf67
        bf56: b9 88 00  lda     $0088,y
        bf59: 49 ff     eor     #$ff
        bf5b: 18        clc
        bf5c: 69 01     adc     #$01
        bf5e: 85 63     sta     $63
        bf60: b9 90 00  lda     $0090,y
        bf63: 49 ff     eor     #$ff
        bf65: 69 00     adc     #$00
        bf67: 85 64     sta     $64
        bf69: a5 2d     lda     $2d
        bf6b: 45 9b     eor     $9b
        bf6d: 10 0e     bpl     $bf7d
        bf6f: a5 63     lda     $63
        bf71: 38        sec
        bf72: f5 78     sbc     $78,x
        bf74: 85 63     sta     $63
        bf76: a5 64     lda     $64
        bf78: f5 80     sbc     $80,x
        bf7a: b8        clv
        bf7b: 50 0b     bvc     $bf88
        bf7d: a5 63     lda     $63
        bf7f: 18        clc
        bf80: 75 78     adc     $78,x
        bf82: 85 63     sta     $63
        bf84: a5 64     lda     $64
        bf86: 75 80     adc     $80,x
        bf88: 85 64     sta     $64
        bf8a: a4 a9     ldy     $a9
        bf8c: a5 63     lda     $63
        bf8e: 91 74     sta     (vidptr_l),y
        bf90: c8        iny
        bf91: a5 64     lda     $64
        bf93: 29 1f     and     #$1f
        bf95: 91 74     sta     (vidptr_l),y
        bf97: c8        iny
        bf98: a5 61     lda     $61
        bf9a: 91 74     sta     (vidptr_l),y
        bf9c: c8        iny
        bf9d: a5 62     lda     $62
        bf9f: 29 1f     and     #$1f
        bfa1: 05 73     ora     draw_z
        bfa3: 91 74     sta     (vidptr_l),y
        bfa5: c8        iny
        bfa6: 84 a9     sty     $a9
        bfa8: c6 99     dec     rgr_pt_inx
        bfaa: f0 03     beq     $bfaf
        bfac: 4c db be  jmp     $bedb
        bfaf: a4 a9     ldy     $a9
        bfb1: 88        dey
        bfb2: 4c 5f df  jmp     inc_vidptr
; Rotatable graphics values.
; These are indexed by graphic number:
; 0 = flipper
; 1-8 = claw positions within segment
; 9-d = pulsars of varying jaggedness
; I don't know what this byte is.
        bfb5: .byte   08
; Number of points.  Indexed by graphic number.
        bfb6: .byte   08
        bfb7: .byte   08
        bfb8: .byte   08
        bfb9: .byte   08
        bfba: .byte   08
        bfbb: .byte   08
        bfbc: .byte   08
        bfbd: .byte   08
        bfbe: .byte   09
        bfbf: .byte   06
        bfc0: .byte   07
        bfc1: .byte   07
        bfc2: .byte   04
        bfc3: .byte   02
; Starting offsets into points vector.  Indexed by graphic number.
        bfc4: .byte   00
        bfc5: .byte   10
        bfc6: .byte   20
        bfc7: .byte   30
        bfc8: .byte   40
        bfc9: .byte   50
        bfca: .byte   60
        bfcb: .byte   70
        bfcc: .byte   80
        bfcd: .byte   92
        bfce: .byte   9e
        bfcf: .byte   ac
        bfd0: .byte   ba
        bfd1: .byte   c2
; Points vector.
; Each point occupies two bytes.  The second is just a draw/nodraw flag,
; always 0 (nodraw) or 1 (draw).  The first holds the coordinates.  They
; are encoded thus:
; xx xxx xxx
; || ||| +++---> X coordinate
; || +++-------> Y coordinate
; |+-----------> 1 if X coord should be negated, 0 if not
; +------------> 1 if Y coord should be negated, 0 if not
; For example, the two bytes at $bfd6 are $4a $01.  The $01 indicates that
; a line should be drawn; the $4a is 01 001 010, so we have X=-2 Y=1.
; flipper
; Why flippers use eight lines rather than six I don't know.  Maybe someone
; felt the crossing point in the middle should (FSVO "should") be on a
; point with integral coordinates.  Maybe it's a historical artifact from
; some previous flipper design (the format here means you can't have a
; delta of 8 or higher for a line, so if the upper points were pulled out
; to the ends, you couldn't do a six-line flipper).
        bfd2: .word   010c ; 00 001 100
        bfd4: .word   018c ; 10 001 100
        bfd6: .word   014a ; 01 001 010
        bfd8: .word   0109 ; 00 001 001
        bfda: .word   01cb ; 11 001 011
        bfdc: .word   014b ; 01 001 011
        bfde: .word   0189 ; 10 001 001
        bfe0: .word   01ca ; 11 001 010
; claw position 1
        bfe2: .word   0190 ; 10 010 000
        bfe4: .word   018a ; 10 001 010
        bfe6: .word   0123 ; 00 100 011
        bfe8: .word   01db ; 11 011 011
        bfea: .word   0141 ; 01 000 001
        bfec: .word   0110 ; 00 010 000
        bfee: .word   010a ; 00 001 010
        bff0: .word   01cb ; 11 001 011
; claw position 2
        bff2: .word   0191 ; 10 010 001
        bff4: .word   0117 ; 00 010 111
        bff6: .word   014b ; 01 001 011
        bff8: .word   018a ; 10 001 010
        bffa: .word   01ce ; 11 001 110
        bffc: .word   0108 ; 00 001 000
        bffe: .word   010a ; 00 001 010
        c000: .word   01cb ; 11 001 011
; claw position 3
        c002: .word   0192 ; 10 010 010
        c004: .word   0116 ; 00 010 110
        c006: .word   014b ; 01 001 011
        c008: .word   018a ; 10 001 010
        c00a: .word   01cd ; 11 001 101
        c00c: .word   0149 ; 01 001 001
        c00e: .word   010a ; 00 001 010
        c010: .word   01cb ; 11 001 011
; claw position 4
        c012: .word   0193 ; 10 010 011
        c014: .word   0115 ; 00 010 101
        c016: .word   014b ; 01 001 011
        c018: .word   018a ; 10 001 010
        c01a: .word   01cc ; 11 001 100
        c01c: .word   014a ; 01 001 010
        c01e: .word   010a ; 00 001 010
        c020: .word   01cb ; 11 001 011
; claw position 5
        c022: .word   0195 ; 10 010 101
        c024: .word   0113 ; 00 010 011
        c026: .word   014b ; 01 001 011
        c028: .word   018a ; 10 001 010
        c02a: .word   01ca ; 11 001 010
        c02c: .word   014c ; 01 001 100
        c02e: .word   010a ; 00 001 010
        c030: .word   01cb ; 11 001 011
; claw position 6
        c032: .word   0196 ; 10 010 110
        c034: .word   0112 ; 00 010 010
        c036: .word   014b ; 01 001 011
        c038: .word   018a ; 10 001 010
        c03a: .word   01c9 ; 11 001 001
        c03c: .word   014d ; 01 001 101
        c03e: .word   010a ; 00 001 010
        c040: .word   01cb ; 11 001 011
; claw position 7
        c042: .word   0197 ; 10 010 111
        c044: .word   0111 ; 00 010 001
        c046: .word   014b ; 01 001 011
        c048: .word   018a ; 10 001 010
        c04a: .word   0188 ; 10 001 000
        c04c: .word   014e ; 01 001 110
        c04e: .word   010a ; 00 001 010
        c050: .word   01cb ; 11 001 011
; claw position 8
        c052: .word   000b ; 00 001 011 no-draw
        c054: .word   01a3 ; 10 100 011
        c056: .word   010a ; 00 001 010
        c058: .word   0110 ; 00 010 000
        c05a: .word   014b ; 01 001 011
        c05c: .word   018a ; 10 001 010
        c05e: .word   0190 ; 10 010 000
        c060: .word   0141 ; 01 000 001
        c062: .word   015b ; 01 011 011
; pulsar variant 1
        c064: .word   019a ; 10 011 010
        c066: .word   0131 ; 00 110 001
        c068: .word   01b1 ; 10 110 001
        c06a: .word   0131 ; 00 110 001
        c06c: .word   01b1 ; 10 110 001
        c06e: .word   011a ; 00 011 010
; pulsar variant 2
        c070: .word   0001 ; 00 000 001 no-draw
        c072: .word   0191 ; 10 010 001
        c074: .word   0121 ; 00 100 001
        c076: .word   01a1 ; 10 100 001
        c078: .word   0121 ; 00 100 001
        c07a: .word   01a1 ; 10 100 001
        c07c: .word   0111 ; 00 010 001
; pulsar variant 3
        c07e: .word   0001 ; 00 000 001 no-draw
        c080: .word   0189 ; 10 001 001
        c082: .word   0111 ; 00 010 001
        c084: .word   0191 ; 10 010 001
        c086: .word   0111 ; 00 010 001
        c088: .word   0191 ; 10 010 001
        c08a: .word   0109 ; 00 001 001
; pulsar variant 4
        c08c: .word   0001 ; 00 000 001 no-draw
        c08e: .word   018a ; 10 001 010
        c090: .word   0112 ; 00 010 010
        c092: .word   018a ; 10 001 010
; pulsar variant 5
        c094: .word   0001 ; 00 000 001 no-draw
        c096: .word   0106 ; 00 000 110
        c098: a5 57     lda     $57
        c09a: 38        sec
        c09b: e5 5f     sbc     $5f
        c09d: 8d 95 60  sta     mb_w_15
        c0a0: a9 00     lda     #$00
        c0a2: e5 5b     sbc     $5b
        c0a4: 8d 96 60  sta     mb_w_16
        c0a7: 10 0a     bpl     $c0b3
        c0a9: a9 00     lda     #$00
        c0ab: 8d 96 60  sta     mb_w_16
        c0ae: a9 01     lda     #$01
        c0b0: 8d 95 60  sta     mb_w_15
        c0b3: a5 58     lda     $58
        c0b5: c5 60     cmp     y3d
        c0b7: 90 07     bcc     $c0c0
        c0b9: e5 60     sbc     y3d
        c0bb: a2 00     ldx     #$00
        c0bd: b8        clv
        c0be: 50 07     bvc     $c0c7
        c0c0: a5 60     lda     y3d
        c0c2: 38        sec
        c0c3: e5 58     sbc     $58
        c0c5: a2 ff     ldx     #$ff
        c0c7: 8d 8e 60  sta     mb_w_0e
        c0ca: 8d 94 60  sta     mb_w_14
        c0cd: 86 33     stx     $33
        c0cf: a5 56     lda     $56
        c0d1: c5 5e     cmp     $5e
        c0d3: 90 07     bcc     $c0dc
        c0d5: e5 5e     sbc     $5e
        c0d7: a2 00     ldx     #$00
        c0d9: b8        clv
        c0da: 50 07     bvc     $c0e3
        c0dc: a5 5e     lda     $5e
        c0de: 38        sec
        c0df: e5 56     sbc     $56
        c0e1: a2 ff     ldx     #$ff
        c0e3: 85 32     sta     $32
        c0e5: 86 34     stx     $34
        c0e7: 2c 40 60  bit     eactl_mbst
        c0ea: 30 fb     bmi     $c0e7
        c0ec: ad 60 60  lda     mb_rd_l
        c0ef: 85 63     sta     $63
        c0f1: ad 70 60  lda     mb_rd_h
        c0f4: 85 64     sta     $64
        c0f6: a5 32     lda     $32
        c0f8: 8d 8e 60  sta     mb_w_0e
        c0fb: 8d 94 60  sta     mb_w_14
        c0fe: a5 33     lda     $33
        c100: 30 18     bmi     $c11a
        c102: a5 63     lda     $63
        c104: 18        clc
        c105: 65 68     adc     $68
        c107: 85 63     sta     $63
        c109: a5 64     lda     $64
        c10b: 65 69     adc     $69
        c10d: 50 06     bvc     $c115
        c10f: a9 ff     lda     #$ff
        c111: 85 63     sta     $63
        c113: a9 7f     lda     #$7f
        c115: 85 64     sta     $64
        c117: b8        clv
        c118: 50 15     bvc     $c12f
        c11a: a5 68     lda     $68
        c11c: 38        sec
        c11d: e5 63     sbc     $63
        c11f: 85 63     sta     $63
        c121: a5 69     lda     $69
        c123: e5 64     sbc     $64
        c125: 50 06     bvc     $c12d
        c127: a9 00     lda     #$00
        c129: 85 63     sta     $63
        c12b: a9 80     lda     #$80
        c12d: 85 64     sta     $64
        c12f: 2c 40 60  bit     eactl_mbst
        c132: 30 fb     bmi     $c12f
        c134: ad 60 60  lda     mb_rd_l
        c137: 85 61     sta     $61
        c139: ad 70 60  lda     mb_rd_h
        c13c: 85 62     sta     $62
        c13e: a6 34     ldx     $34
        c140: 30 16     bmi     $c158
        c142: a5 61     lda     $61
        c144: 18        clc
        c145: 65 66     adc     $66
        c147: 85 61     sta     $61
        c149: a5 62     lda     $62
        c14b: 65 67     adc     $67
        c14d: 50 06     bvc     $c155
        c14f: a9 ff     lda     #$ff
        c151: 85 61     sta     $61
        c153: a9 7f     lda     #$7f
        c155: 85 62     sta     $62
        c157: 60        rts
        c158: a5 66     lda     $66
        c15a: 38        sec
        c15b: e5 61     sbc     $61
        c15d: 85 61     sta     $61
        c15f: a5 67     lda     $67
        c161: e5 62     sbc     $62
        c163: 50 06     bvc     $c16b
        c165: a9 00     lda     #$00
        c167: 85 61     sta     $61
        c169: a9 80     lda     #$80
        c16b: 85 62     sta     $62
        c16d: 60        rts
        c16e: 20 13 aa  jsr     $aa13
        c171: a9 80     lda     #$80
        c173: 85 5e     sta     $5e
        c175: a9 ff     lda     #$ff
        c177: 8d 14 01  sta     $0114
        c17a: 20 35 c2  jsr     $c235
        c17d: ad 33 01  lda     $0133
        c180: d0 03     bne     $c185
        c182: 8d 00 58  sta     vg_reset
        c185: a9 00     lda     #$00
        c187: 8d 33 01  sta     $0133
        c18a: ad c6 ce  lda     $cec6
        c18d: 8d 00 20  sta     vecram
        c190: ad c7 ce  lda     $cec7
        c193: 8d 01 20  sta     vecram+1
; Install the correct colours for curlevel.
  setcolours: a5 9f     lda     curlevel
        c198: 29 70     and     #$70
        c19a: c9 5f     cmp     #$5f
        c19c: 90 02     bcc     $c1a0
        c19e: a9 5f     lda     #$5f
        c1a0: 4a        lsr     a
        c1a1: 09 07     ora     #$07
        c1a3: aa        tax
        c1a4: a0 07     ldy     #$07
        c1a6: bd fd c1  lda     $c1fd,x
        c1a9: 29 0f     and     #$0f
        c1ab: 99 19 00  sta     $0019,y
        c1ae: 99 00 08  sta     col_ram,y
        c1b1: bd fd c1  lda     $c1fd,x
        c1b4: 4a        lsr     a
        c1b5: 4a        lsr     a
        c1b6: 4a        lsr     a
        c1b7: 4a        lsr     a
        c1b8: 99 21 00  sta     $0021,y
        c1bb: 99 08 08  sta     $0808,y
        c1be: ca        dex
        c1bf: 88        dey
        c1c0: 10 e4     bpl     $c1a6
        c1c2: 60        rts
        c1c3: a9 00     lda     #$00
        c1c5: 85 81     sta     $81
        c1c7: 85 91     sta     $91
        c1c9: 85 80     sta     $80
        c1cb: 85 78     sta     $78
        c1cd: 85 90     sta     $90
        c1cf: 85 88     sta     $88
        c1d1: a9 00     lda     #$00
        c1d3: 8d 80 60  sta     mb_w_00
        c1d6: 8d 81 60  sta     mb_w_01
        c1d9: 8d 84 60  sta     mb_w_04
        c1dc: 8d 85 60  sta     mb_w_05
        c1df: 8d 86 60  sta     mb_w_06
        c1e2: 8d 87 60  sta     mb_w_07
        c1e5: 8d 89 60  sta     mb_w_09
        c1e8: 8d 83 60  sta     mb_w_03
        c1eb: 8d 8d 60  sta     mb_w_0d
        c1ee: 8d 8e 60  sta     mb_w_0e
        c1f1: 8d 8f 60  sta     mb_w_0f
        c1f4: 8d 90 60  sta     mb_w_10
        c1f7: a9 0f     lda     #$0f
        c1f9: 8d 8c 60  sta     mb_w_0c
        c1fc: 60        rts
; Used at $c1a6 and $c1b1.
; Appear to be blocks of 8 bytes giving colours for the various
; blocks of 16 levels.
        c1fd: .chunk  8 ; levels 1-16
        c205: .chunk  8 ; levels 17-32
        c20d: .chunk  8 ; levels 33-48
        c215: .chunk  8 ; levels 49-64
        c21d: .chunk  8 ; levels 65-80
        c225: .chunk  8 ; levels 81-99 (see $c198)
        c22d: .byte   06
        c22e: .byte   03
        c22f: .byte   01
        c230: .byte   04
        c231: .byte   00
        c232: .byte   05
        c233: .byte   05
        c234: .byte   05
        c235: a6 3d     ldx     curplayer
        c237: b5 46     lda     p1_level,x
        c239: 20 e8 c2  jsr     get_tube_no
        c23c: 48        pha
        c23d: ac 12 01  ldy     curtube
        c240: b9 8c bc  lda     lev_scale,y
        c243: 49 ff     eor     #$ff
        c245: 18        clc
        c246: 69 01     adc     #$01
        c248: 85 5f     sta     $5f
        c24a: 85 5d     sta     $5d
        c24c: a9 10     lda     #$10
        c24e: 38        sec
        c24f: e5 5f     sbc     $5f
        c251: 85 a0     sta     $a0
        c253: a9 ff     lda     #$ff
        c255: 85 5b     sta     $5b
        c257: b9 9c bc  lda     lev_y3d,y
        c25a: 85 60     sta     y3d
        c25c: b9 cc bc  lda     lev_open,y
        c25f: 8d 11 01  sta     open_level
        c262: a5 02     lda     $02
        c264: c9 1e     cmp     #$1e
        c266: d0 0d     bne     $c275
        c268: b9 ac bc  lda     lev_y2d,y
        c26b: 85 68     sta     $68
        c26d: b9 bc bc  lda     lev_y2db,y
        c270: 85 69     sta     $69
        c272: b8        clv
        c273: 50 18     bvc     $c28d
        c275: b9 ac bc  lda     lev_y2d,y
        c278: 38        sec
        c279: e5 68     sbc     $68
        c27b: 8d 21 01  sta     $0121
        c27e: b9 bc bc  lda     lev_y2db,y
        c281: ed 69 00  sbc     $0069
        c284: a2 03     ldx     #$03
        c286: 4a        lsr     a
        c287: 6e 21 01  ror     $0121
        c28a: ca        dex
        c28b: 10 f9     bpl     $c286
        c28d: a9 00     lda     #$00
        c28f: 85 66     sta     $66
        c291: 85 67     sta     $67
        c293: a9 00     lda     #$00
        c295: 8d 0f 01  sta     $010f
        c298: 8d 10 01  sta     $0110
        c29b: a9 2c     lda     #$2c
        c29d: 8d 13 01  sta     $0113
        c2a0: 68        pla
        c2a1: a8        tay
        c2a2: a2 0f     ldx     #$0f
        c2a4: b9 7c b9  lda     lev_x,y
        c2a7: 9d ce 03  sta     tube_x,x
        c2aa: b9 7c ba  lda     lev_y,y
        c2ad: 9d de 03  sta     tube_y,x
        c2b0: a9 00     lda     #$00
        c2b2: 9d 1a 03  sta     $031a,x
        c2b5: 9d 3a 03  sta     $033a,x
        c2b8: 9d 9a 03  sta     $039a,x
        c2bb: b9 7c bb  lda     lev_angle,y
        c2be: 9d ee 03  sta     tube_angle,x
        c2c1: 88        dey
        c2c2: ca        dex
        c2c3: 10 df     bpl     $c2a4
        c2c5: a0 00     ldy     #$00
        c2c7: a2 0f     ldx     #$0f
        c2c9: b9 ce 03  lda     tube_x,y
        c2cc: 38        sec
        c2cd: 7d ce 03  adc     tube_x,x
        c2d0: 6a        ror     a
        c2d1: 9d 35 04  sta     mid_x,x
        c2d4: b9 de 03  lda     tube_y,y
        c2d7: 38        sec
        c2d8: 7d de 03  adc     tube_y,x
        c2db: 6a        ror     a
        c2dc: 9d 45 04  sta     mid_y,x
        c2df: 88        dey
        c2e0: 10 02     bpl     $c2e4
        c2e2: a0 0f     ldy     #$0f
        c2e4: ca        dex
        c2e5: 10 e2     bpl     $c2c9
        c2e7: 60        rts
; Take the level number in A, do the random thing for level 99, and fetch
; the tube number for the level.  Also return a value with the tube number
; in the high four bits and $f in the low four bits; this is used as an
; index into the [][] tables.
 get_tube_no: a2 00     ldx     #$00
        c2ea: c9 62     cmp     #$62
        c2ec: 90 05     bcc     $c2f3
        c2ee: ad ca 60  lda     pokey1_rand
        c2f1: 29 5f     and     #$5f
; This appears to be "high nibble into X, keep low nibble in A".  I can't
; see why not compute that directly with shifts and masks instead of a
; subtract loop.  Maybe the $10 here was an assembly-time constant rather
; than being a deeply-wired-in value?  That's not very plausible, though,
; as level numbers are shifted by four bits elsewhere.
        c2f3: c9 10     cmp     #$10
        c2f5: 90 04     bcc     $c2fb
        c2f7: e8        inx
        c2f8: 38        sec
        c2f9: e9 10     sbc     #$10
        c2fb: c9 10     cmp     #$10
        c2fd: b0 f6     bcs     $c2f5
        c2ff: a8        tay
        c300: b9 7c bc  lda     lev_remap,y
        c303: 8d 12 01  sta     curtube
        c306: 0a        asl     a
        c307: 0a        asl     a
        c308: 0a        asl     a
        c309: 0a        asl     a
        c30a: 09 0f     ora     #$0f
        c30c: 60        rts
        c30d: ad 10 01  lda     $0110
        c310: d0 27     bne     $c339
        c312: a9 f0     lda     #$f0
        c314: 85 57     sta     $57
        c316: a2 4f     ldx     #$4f
        c318: 20 73 c4  jsr     $c473
        c31b: 8d 10 01  sta     $0110
        c31e: f0 03     beq     $c323
        c320: 8d 0f 01  sta     $010f
        c323: ad 0f 01  lda     $010f
        c326: d0 11     bne     $c339
        c328: a9 10     lda     #$10
        c32a: 85 57     sta     $57
        c32c: 20 53 c4  jsr     $c453
        c32f: a5 57     lda     $57
        c331: a2 0f     ldx     #$0f
        c333: 20 73 c4  jsr     $c473
        c336: 8d 0f 01  sta     $010f
        c339: a9 01     lda     #$01
        c33b: 20 6a df  jsr     vapp_scale_A,0
        c33e: a0 06     ldy     #$06
        c340: 84 9e     sty     curcolor
        c342: ae 10 01  ldx     $0110
        c345: f0 01     beq     $c348
        c347: 60        rts
        c348: ae 13 01  ldx     $0113
        c34b: d0 01     bne     $c34e
        c34d: 60        rts
        c34e: a2 0f     ldx     #$0f
        c350: a9 c0     lda     #$c0
        c352: 20 ee c3  jsr     $c3ee
        c355: ca        dex
        c356: 10 f8     bpl     $c350
        c358: a0 06     ldy     #$06
        c35a: 84 9e     sty     curcolor
        c35c: a9 08     lda     #$08
        c35e: 20 4c df  jsr     vapp_sclstat_A_Y
        c361: a0 4f     ldy     #$4f
        c363: ad 10 01  lda     $0110
        c366: 20 6e c3  jsr     $c36e
        c369: a0 0f     ldy     #$0f
        c36b: ad 0f 01  lda     $010f
        c36e: d0 49     bne     $c3b9
        c370: 84 37     sty     $37
        c372: b9 2a 03  lda     $032a,y
        c375: 85 61     sta     $61
        c377: b9 1a 03  lda     $031a,y
        c37a: 85 62     sta     $62
        c37c: b9 4a 03  lda     $034a,y
        c37f: 85 63     sta     $63
        c381: b9 3a 03  lda     $033a,y
        c384: 85 64     sta     $64
        c386: a2 61     ldx     #$61
        c388: 20 72 c7  jsr     $c772
        c38b: a5 74     lda     vidptr_l
        c38d: 85 b0     sta     $b0
        c38f: a5 75     lda     vidptr_h
        c391: 85 b1     sta     $b1
        c393: a2 0f     ldx     #$0f
        c395: ad 11 01  lda     open_level
        c398: f0 01     beq     $c39b
        c39a: ca        dex
        c39b: a9 c0     lda     #$c0
        c39d: 85 73     sta     draw_z
        c39f: 86 38     stx     $38
        c3a1: c6 37     dec     $37
        c3a3: a5 37     lda     $37
        c3a5: 29 0f     and     #$0f
        c3a7: c9 0f     cmp     #$0f
        c3a9: d0 07     bne     $c3b2
        c3ab: a5 37     lda     $37
        c3ad: 18        clc
        c3ae: 69 10     adc     #$10
        c3b0: 85 37     sta     $37
        c3b2: 20 23 c4  jsr     $c423
        c3b5: c6 38     dec     $38
        c3b7: 10 e8     bpl     $c3a1
        c3b9: 60        rts
        c3ba: a5 61     lda     $61
        c3bc: 38        sec
        c3bd: e5 6a     sbc     $6a
        c3bf: 85 6e     sta     $6e
        c3c1: a5 62     lda     $62
        c3c3: e5 6b     sbc     $6b
        c3c5: 85 6f     sta     $6f
        c3c7: a5 63     lda     $63
        c3c9: 38        sec
        c3ca: e5 6c     sbc     $6c
        c3cc: 85 70     sta     $70
        c3ce: a5 64     lda     $64
        c3d0: e5 6d     sbc     $6d
        c3d2: 85 71     sta     $71
        c3d4: a2 6e     ldx     #$6e
        c3d6: 20 92 df  jsr     $df92
        c3d9: a5 61     lda     $61
        c3db: 85 6a     sta     $6a
        c3dd: a5 62     lda     $62
        c3df: 85 6b     sta     $6b
        c3e1: a5 63     lda     $63
        c3e3: 85 6c     sta     $6c
        c3e5: a5 64     lda     $64
        c3e7: 85 6d     sta     $6d
        c3e9: a9 c0     lda     #$c0
        c3eb: 85 73     sta     draw_z
        c3ed: 60        rts
        c3ee: 86 37     stx     $37
        c3f0: 48        pha
        c3f1: a4 9e     ldy     curcolor
        c3f3: a9 08     lda     #$08
        c3f5: 20 4c df  jsr     vapp_sclstat_A_Y
        c3f8: 20 3c c4  jsr     $c43c
        c3fb: a2 61     ldx     #$61
        c3fd: 20 72 c7  jsr     $c772
        c400: 68        pla
        c401: 85 73     sta     draw_z
        c403: 48        pha
        c404: 20 23 c4  jsr     $c423
        c407: c6 37     dec     $37
        c409: a4 9e     ldy     curcolor
        c40b: a9 00     lda     #$00
        c40d: 85 73     sta     draw_z
        c40f: a9 08     lda     #$08
        c411: 20 4c df  jsr     vapp_sclstat_A_Y
        c414: 20 23 c4  jsr     $c423
        c417: 68        pla
        c418: 85 73     sta     draw_z
        c41a: 20 3c c4  jsr     $c43c
        c41d: 20 ba c3  jsr     $c3ba
        c420: a6 37     ldx     $37
        c422: 60        rts
        c423: a6 37     ldx     $37
        c425: bd 2a 03  lda     $032a,x
        c428: 85 61     sta     $61
        c42a: bd 1a 03  lda     $031a,x
        c42d: 85 62     sta     $62
        c42f: bd 4a 03  lda     $034a,x
        c432: 85 63     sta     $63
        c434: bd 3a 03  lda     $033a,x
        c437: 85 64     sta     $64
        c439: 4c ba c3  jmp     $c3ba
        c43c: a6 37     ldx     $37
        c43e: bd 6a 03  lda     $036a,x
        c441: 85 61     sta     $61
        c443: bd 5a 03  lda     $035a,x
        c446: 85 62     sta     $62
        c448: bd 8a 03  lda     $038a,x
        c44b: 85 63     sta     $63
        c44d: bd 7a 03  lda     $037a,x
        c450: 85 64     sta     $64
        c452: 60        rts
        c453: a5 5b     lda     $5b
        c455: d0 1a     bne     $c471
        c457: a5 57     lda     $57
        c459: 38        sec
        c45a: e5 5f     sbc     $5f
        c45c: 90 02     bcc     $c460
        c45e: c9 0c     cmp     #$0c
        c460: b0 0f     bcs     $c471
        c462: a5 5f     lda     $5f
        c464: 18        clc
        c465: 69 0f     adc     #$0f
        c467: b0 02     bcs     $c46b
        c469: c9 f0     cmp     #$f0
        c46b: 90 02     bcc     $c46f
        c46d: a9 f0     lda     #$f0
        c46f: 85 57     sta     $57
        c471: 60        rts
        c472: .byte   db
        c473: 85 57     sta     $57
        c475: 86 38     stx     $38
        c477: a9 00     lda     #$00
        c479: 85 59     sta     fscale
        c47b: a2 0f     ldx     #$0f
        c47d: 86 37     stx     $37
        c47f: a6 37     ldx     $37
        c481: bd ce 03  lda     tube_x,x
        c484: 85 56     sta     $56
        c486: bd de 03  lda     tube_y,x
        c489: 85 58     sta     $58
        c48b: 20 98 c0  jsr     $c098
        c48e: a6 38     ldx     $38
        c490: a4 61     ldy     $61
        c492: a5 62     lda     $62
        c494: 30 0d     bmi     $c4a3
        c496: c9 04     cmp     #$04
        c498: 90 06     bcc     $c4a0
        c49a: a0 ff     ldy     #$ff
        c49c: a9 03     lda     #$03
        c49e: e6 59     inc     fscale
        c4a0: b8        clv
        c4a1: 50 0a     bvc     $c4ad
        c4a3: c9 fc     cmp     #$fc
        c4a5: b0 06     bcs     $c4ad
        c4a7: a0 01     ldy     #$01
        c4a9: a9 fc     lda     #$fc
        c4ab: e6 59     inc     fscale
        c4ad: 9d 1a 03  sta     $031a,x
        c4b0: 98        tya
        c4b1: 9d 2a 03  sta     $032a,x
        c4b4: a4 63     ldy     $63
        c4b6: a5 64     lda     $64
        c4b8: 30 0d     bmi     $c4c7
        c4ba: c9 04     cmp     #$04
        c4bc: 90 06     bcc     $c4c4
        c4be: a0 ff     ldy     #$ff
        c4c0: a9 03     lda     #$03
        c4c2: e6 59     inc     fscale
        c4c4: b8        clv
        c4c5: 50 0a     bvc     $c4d1
        c4c7: c9 fc     cmp     #$fc
        c4c9: b0 06     bcs     $c4d1
        c4cb: a9 fc     lda     #$fc
        c4cd: a0 01     ldy     #$01
        c4cf: e6 59     inc     fscale
        c4d1: 9d 3a 03  sta     $033a,x
        c4d4: 98        tya
        c4d5: 9d 4a 03  sta     $034a,x
        c4d8: c6 38     dec     $38
        c4da: c6 37     dec     $37
        c4dc: 10 a1     bpl     $c47f
        c4de: a5 59     lda     fscale
        c4e0: 60        rts
        c4e1: 20 e8 c2  jsr     get_tube_no
        c4e4: 85 36     sta     $36
        c4e6: 86 35     stx     $35
        c4e8: a9 00     lda     #$00
        c4ea: 85 73     sta     draw_z
        c4ec: a9 05     lda     #$05
        c4ee: 20 6a df  jsr     vapp_scale_A,0
        c4f1: a5 35     lda     $35
        c4f3: 29 07     and     #$07
        c4f5: aa        tax
        c4f6: bc 2d c2  ldy     $c22d,x
        c4f9: 84 9e     sty     curcolor
        c4fb: a9 08     lda     #$08
        c4fd: 20 4c df  jsr     vapp_sclstat_A_Y
        c500: ae 12 01  ldx     curtube
        c503: a5 36     lda     $36
        c505: bc cc bc  ldy     lev_open,x
        c508: d0 03     bne     $c50d
        c50a: 38        sec
        c50b: e9 0f     sbc     #$0f
        c50d: a8        tay
        c50e: b9 7c ba  lda     lev_y,y
        c511: 85 57     sta     $57
        c513: 49 80     eor     #$80
        c515: aa        tax
        c516: b9 7c b9  lda     lev_x,y
        c519: 85 56     sta     $56
        c51b: 49 80     eor     #$80
        c51d: 20 75 df  jsr     vapp_ldraw_A,X
        c520: a9 c0     lda     #$c0
        c522: 85 73     sta     draw_z
        c524: a2 0f     ldx     #$0f
        c526: 86 38     stx     $38
        c528: a4 36     ldy     $36
        c52a: b9 7c b9  lda     lev_x,y
        c52d: aa        tax
        c52e: 38        sec
        c52f: e5 56     sbc     $56
        c531: 48        pha
        c532: 86 56     stx     $56
        c534: b9 7c ba  lda     lev_y,y
        c537: a8        tay
        c538: 38        sec
        c539: e5 57     sbc     $57
        c53b: aa        tax
        c53c: 84 57     sty     $57
        c53e: 68        pla
        c53f: 20 75 df  jsr     vapp_ldraw_A,X
        c542: c6 36     dec     $36
        c544: c6 38     dec     $38
        c546: 10 e0     bpl     $c528
        c548: a9 01     lda     #$01
        c54a: 4c 6a df  jmp     vapp_scale_A,0
        c54d: ad 15 01  lda     $0115
        c550: f0 5f     beq     $c5b1
        c552: a5 5f     lda     $5f
        c554: 48        pha
        c555: a5 5b     lda     $5b
        c557: 48        pha
        c558: a5 a0     lda     $a0
        c55a: 48        pha
        c55b: a9 e8     lda     #$e8
        c55d: 85 5f     sta     $5f
        c55f: a9 ff     lda     #$ff
        c561: 85 5b     sta     $5b
        c563: a9 28     lda     #$28
        c565: 85 a0     sta     $a0
        c567: a2 07     ldx     #$07
        c569: 86 37     stx     $37
        c56b: a6 37     ldx     $37
        c56d: bd fe 03  lda     $03fe,x
        c570: f0 32     beq     $c5a4
        c572: 85 57     sta     $57
        c574: a9 80     lda     #$80
        c576: 85 56     sta     $56
        c578: a9 80     lda     #$80
        c57a: 85 58     sta     $58
        c57c: a5 9f     lda     curlevel
        c57e: c9 05     cmp     #$05
        c580: b0 05     bcs     $c587
        c582: a9 06     lda     #$06
        c584: b8        clv
        c585: 50 09     bvc     $c590
        c587: 8a        txa
        c588: 29 07     and     #$07
        c58a: c9 07     cmp     #$07
        c58c: d0 02     bne     $c590
        c58e: a9 04     lda     #$04
        c590: 85 9e     sta     curcolor
        c592: a8        tay
        c593: a9 08     lda     #$08
        c595: 20 4c df  jsr     vapp_sclstat_A_Y
        c598: a5 37     lda     $37
        c59a: 29 03     and     #$03
        c59c: 0a        asl     a
        c59d: 69 0a     adc     #$0a
        c59f: 85 55     sta     $55
        c5a1: 20 09 bd  jsr     $bd09
        c5a4: c6 37     dec     $37
        c5a6: 10 c3     bpl     $c56b
        c5a8: 68        pla
        c5a9: 85 a0     sta     $a0
        c5ab: 68        pla
        c5ac: 85 5b     sta     $5b
        c5ae: 68        pla
        c5af: 85 5f     sta     $5f
        c5b1: ad 1f 01  lda     $011f
        c5b4: f0 0b     beq     $c5c1
; If P1 score is in the 150K-160K range, increment one byte of RAM in the
; $0200-$0299 range, depending on the low two digits of the score.  This is
; probably responsible for a few of the game crashes I've seen; there are
; some bytes that if struck by this _will_ cause trouble.
        c5b6: a6 42     ldx     p1_score_h
        c5b8: e0 15     cpx     #$15
        c5ba: 90 05     bcc     $c5c1
        c5bc: a6 40     ldx     p1_score_l
        c5be: fe 00 02  inc     player_seg,x
        c5c1: 60        rts
        c5c2: ad 10 01  lda     $0110
        c5c5: f0 01     beq     $c5c8
        c5c7: 60        rts
        c5c8: a5 5b     lda     $5b
        c5ca: d0 07     bne     $c5d3
        c5cc: a5 5f     lda     $5f
        c5ce: c9 f0     cmp     #$f0
        c5d0: 90 01     bcc     $c5d3
        c5d2: 60        rts
        c5d3: a9 01     lda     #$01
        c5d5: 20 6a df  jsr     vapp_scale_A,0
        c5d8: a5 74     lda     vidptr_l
        c5da: 48        pha
        c5db: a5 75     lda     vidptr_h
        c5dd: 48        pha
        c5de: a9 00     lda     #$00
        c5e0: 85 38     sta     $38
        c5e2: 85 a9     sta     $a9
        c5e4: a2 0f     ldx     #$0f
        c5e6: ad 11 01  lda     open_level
        c5e9: f0 01     beq     $c5ec
        c5eb: ca        dex
        c5ec: 86 37     stx     $37
        c5ee: a2 03     ldx     #$03
        c5f0: a4 a9     ldy     $a9
        c5f2: bd 69 c6  lda     $c669,x
        c5f5: 91 74     sta     (vidptr_l),y
        c5f7: c8        iny
        c5f8: ca        dex
        c5f9: 10 f7     bpl     $c5f2
        c5fb: 84 a9     sty     $a9
        c5fd: ad 14 01  lda     $0114
        c600: d0 4a     bne     $c64c
        c602: a6 38     ldx     $38
        c604: bd 9a 03  lda     $039a,x
        c607: 30 11     bmi     $c61a
        c609: a2 0b     ldx     #$0b
        c60b: a4 a9     ldy     $a9
        c60d: b1 aa     lda     ($aa),y
        c60f: 91 74     sta     (vidptr_l),y
        c611: c8        iny
        c612: ca        dex
        c613: 10 f8     bpl     $c60d
        c615: 84 a9     sty     $a9
        c617: b8        clv
        c618: 50 2f     bvc     $c649
        c61a: a4 a9     ldy     $a9
        c61c: b1 aa     lda     ($aa),y
        c61e: 91 74     sta     (vidptr_l),y
        c620: 85 6c     sta     $6c
        c622: c8        iny
        c623: b1 aa     lda     ($aa),y
        c625: 91 74     sta     (vidptr_l),y
        c627: c9 10     cmp     #$10
        c629: 90 02     bcc     $c62d
        c62b: 09 e0     ora     #$e0
        c62d: 85 6d     sta     $6d
        c62f: c8        iny
        c630: b1 aa     lda     ($aa),y
        c632: 91 74     sta     (vidptr_l),y
        c634: 85 6a     sta     $6a
        c636: c8        iny
        c637: b1 aa     lda     ($aa),y
        c639: 91 74     sta     (vidptr_l),y
        c63b: c9 10     cmp     #$10
        c63d: 90 02     bcc     $c641
        c63f: 09 e0     ora     #$e0
        c641: 85 6b     sta     $6b
        c643: c8        iny
        c644: 84 a9     sty     $a9
        c646: 20 c7 c6  jsr     $c6c7
        c649: b8        clv
        c64a: 50 06     bvc     $c652
        c64c: 20 6d c6  jsr     $c66d
        c64f: 20 c7 c6  jsr     $c6c7
        c652: a6 38     ldx     $38
        c654: 1e 9a 03  asl     $039a,x
        c657: e6 38     inc     $38
        c659: c6 37     dec     $37
        c65b: 10 91     bpl     $c5ee
        c65d: 68        pla
        c65e: 85 ab     sta     $ab
        c660: 68        pla
        c661: 85 aa     sta     $aa
        c663: a4 a9     ldy     $a9
        c665: 88        dey
        c666: 4c 5f df  jmp     inc_vidptr
; These four bytes are byte-reversed video code.  The code is
; 6805  vstat z=0 c=5 sparkle=1
; 8040  vcentre
; This is used by the loop at c5f2.
        c669: .byte   80
        c66a: .byte   40
        c66b: .byte   68
        c66c: .byte   05
        c66d: a5 38     lda     $38
        c66f: aa        tax
        c670: 18        clc
        c671: 69 01     adc     #$01
        c673: 29 0f     and     #$0f
        c675: a8        tay
        c676: bd 6a 03  lda     $036a,x
        c679: 38        sec
        c67a: 79 6a 03  adc     $036a,y
        c67d: 85 61     sta     $61
        c67f: bd 5a 03  lda     $035a,x
        c682: 79 5a 03  adc     $035a,y
        c685: 85 62     sta     $62
        c687: 0a        asl     a
        c688: 66 62     ror     $62
        c68a: 66 61     ror     $61
        c68c: bd 8a 03  lda     $038a,x
        c68f: 38        sec
        c690: 79 8a 03  adc     $038a,y
        c693: 85 63     sta     $63
        c695: bd 7a 03  lda     $037a,x
        c698: 79 7a 03  adc     $037a,y
        c69b: 85 64     sta     $64
        c69d: 0a        asl     a
        c69e: 66 64     ror     $64
        c6a0: 66 63     ror     $63
        c6a2: a4 a9     ldy     $a9
        c6a4: a5 63     lda     $63
        c6a6: 91 74     sta     (vidptr_l),y
        c6a8: c8        iny
        c6a9: 85 6c     sta     $6c
        c6ab: a5 64     lda     $64
        c6ad: 85 6d     sta     $6d
        c6af: 29 1f     and     #$1f
        c6b1: 91 74     sta     (vidptr_l),y
        c6b3: c8        iny
        c6b4: a5 61     lda     $61
        c6b6: 91 74     sta     (vidptr_l),y
        c6b8: c8        iny
        c6b9: 85 6a     sta     $6a
        c6bb: a5 62     lda     $62
        c6bd: 85 6b     sta     $6b
        c6bf: 29 1f     and     #$1f
        c6c1: 91 74     sta     (vidptr_l),y
        c6c3: c8        iny
        c6c4: 84 a9     sty     $a9
        c6c6: 60        rts
        c6c7: a6 38     ldx     $38
        c6c9: bd ac 03  lda     spike_ht,x
        c6cc: d0 16     bne     $c6e4
        c6ce: a4 a9     ldy     $a9
        c6d0: a2 03     ldx     #$03
        c6d2: a9 00     lda     #$00
        c6d4: 91 74     sta     (vidptr_l),y
        c6d6: c8        iny
        c6d7: a9 71     lda     #$71
        c6d9: 91 74     sta     (vidptr_l),y
        c6db: c8        iny
        c6dc: ca        dex
        c6dd: 10 f3     bpl     $c6d2
        c6df: 84 a9     sty     $a9
        c6e1: b8        clv
        c6e2: 50 57     bvc     $c73b
        c6e4: 85 57     sta     $57
        c6e6: 20 53 c4  jsr     $c453
        c6e9: bd 35 04  lda     mid_x,x
        c6ec: 85 56     sta     $56
        c6ee: bd 45 04  lda     mid_y,x
        c6f1: 85 58     sta     $58
        c6f3: 20 98 c0  jsr     $c098
        c6f6: 20 3c c7  jsr     $c73c
        c6f9: a6 38     ldx     $38
        c6fb: bd 9a 03  lda     $039a,x
        c6fe: 29 40     and     #$40
        c700: f0 1f     beq     $c721
        c702: 20 3e bd  jsr     $bd3e
        c705: ad ca 60  lda     pokey1_rand
        c708: 29 02     and     #$02
        c70a: 18        clc
        c70b: 69 1c     adc     #$1c
        c70d: aa        tax
        c70e: bd c9 ce  lda     graphic_table+1,x
        c711: c8        iny
        c712: 91 74     sta     (vidptr_l),y
        c714: 88        dey
        c715: bd c8 ce  lda     graphic_table,x
        c718: 91 74     sta     (vidptr_l),y
        c71a: c8        iny
        c71b: c8        iny
        c71c: 84 a9     sty     $a9
        c71e: b8        clv
        c71f: 50 1a     bvc     $c73b
        c721: a4 a9     ldy     $a9
        c723: a9 00     lda     #$00
        c725: 91 74     sta     (vidptr_l),y
        c727: c8        iny
        c728: a9 68     lda     #$68
        c72a: 91 74     sta     (vidptr_l),y
        c72c: c8        iny
        c72d: ad b2 3d  lda     $3db2
        c730: 91 74     sta     (vidptr_l),y
        c732: c8        iny
        c733: ad b3 3d  lda     $3db3
        c736: 91 74     sta     (vidptr_l),y
        c738: c8        iny
        c739: 84 a9     sty     $a9
        c73b: 60        rts
        c73c: a4 a9     ldy     $a9
        c73e: a5 63     lda     $63
        c740: 38        sec
        c741: e5 6c     sbc     $6c
        c743: 91 74     sta     (vidptr_l),y
        c745: c8        iny
        c746: a5 64     lda     $64
        c748: e5 6d     sbc     $6d
        c74a: 29 1f     and     #$1f
        c74c: 91 74     sta     (vidptr_l),y
        c74e: c8        iny
        c74f: a5 61     lda     $61
        c751: 38        sec
        c752: e5 6a     sbc     $6a
        c754: 91 74     sta     (vidptr_l),y
        c756: c8        iny
        c757: a5 62     lda     $62
        c759: e5 6b     sbc     $6b
        c75b: 29 1f     and     #$1f
        c75d: 09 a0     ora     #$a0
        c75f: 91 74     sta     (vidptr_l),y
        c761: c8        iny
        c762: 84 a9     sty     $a9
        c764: 60        rts
; On entry, X contains zero-page address of a four-byte block, holding
; AA BB CC DD.  Then, the following are appended to the video list:
;       vscale  b=1 l=0
;       vcentre
;       vldraw  x=X y=Y z=off
; where X=DDCC and Y=BBAA, in each case taken as 13-bit signed numbers
; (ie, the high three bits are dropped).
  vapp_to(X): a0 00     ldy     #$00
        c767: 98        tya
        c768: 91 74     sta     (vidptr_l),y
        c76a: a9 71     lda     #$71
        c76c: c8        iny
        c76d: 91 74     sta     (vidptr_l),y
        c76f: c8        iny
        c770: d0 02     bne     $c774
        c772: a0 00     ldy     #$00
        c774: a9 40     lda     #$40
        c776: 91 74     sta     (vidptr_l),y
        c778: a9 80     lda     #$80
        c77a: c8        iny
        c77b: 91 74     sta     (vidptr_l),y
        c77d: c8        iny
        c77e: b5 02     lda     $02,x
        c780: 85 6c     sta     $6c
        c782: 91 74     sta     (vidptr_l),y
        c784: c8        iny
        c785: b5 03     lda     timectr,x
        c787: 85 6d     sta     $6d
        c789: 29 1f     and     #$1f
        c78b: 91 74     sta     (vidptr_l),y
        c78d: b5 00     lda     gamestate,x
        c78f: 85 6a     sta     $6a
        c791: c8        iny
        c792: 91 74     sta     (vidptr_l),y
        c794: b5 01     lda     $01,x
        c796: 85 6b     sta     $6b
        c798: 29 1f     and     #$1f
        c79a: c8        iny
        c79b: 91 74     sta     (vidptr_l),y
        c79d: 4c 5f df  jmp     inc_vidptr
        c7a0: 20 95 cd  jsr     $cd95
        c7a3: a9 00     lda     #$00
        c7a5: 85 00     sta     gamestate
; This appears to be the game's main loop, from here through $c7bb.
; (This does not apply to reset-in-service-mode selftest, which has its
; own, different, main loop.)
        c7a7: a5 53     lda     $53
        c7a9: c9 09     cmp     #$09
        c7ab: 90 fa     bcc     $c7a7
        c7ad: a9 00     lda     #$00
        c7af: 85 53     sta     $53
        c7b1: 20 bd c7  jsr     $c7bd
        c7b4: 20 91 c8  jsr     $c891
        c7b7: 20 b6 b1  jsr     $b1b6
        c7ba: 18        clc
        c7bb: 90 ea     bcc     $c7a7
        c7bd: ad 00 0d  lda     optsw1
; This and and cmp tests one of the bonus-coins bits and the coinage bits;
; the compare will show equal if "bonus coins" is set to one of
; "1 each 5", "1 each 3", or demo mode (frozen or not) and coinage is
; set to free play.  Why this is a useful thing to test I have no idea;
; perhaps it's documented as a magic combination?
; Another disassembly comments this as checking for demonstration
; freeze mode, which is inconsistent with the layout of the bits in
; $0d00 - for it to be that, it'd have to be and #$e0, cmp #$e0.
        c7c0: 29 83     and     #$83
        c7c2: c9 82     cmp     #$82
        c7c4: f0 13     beq     $c7d9
        c7c6: 20 d2 a7  jsr     $a7d2
        c7c9: a6 00     ldx     gamestate
        c7cb: a5 4e     lda     zap_fire_new
        c7cd: 09 80     ora     #$80
        c7cf: 85 4e     sta     zap_fire_new
        c7d1: bd db c7  lda     $c7db,x
        c7d4: 48        pha
        c7d5: bd da c7  lda     $c7da,x
        c7d8: 48        pha
        c7d9: 60        rts
; Jump table used by the code at c7d1
; Indexed by general game state.
        c7da: .jump   state_00
        c7dc: .jump   state_02
        c7de: .jump   state_04
        c7e0: .jump   state_06
        c7e2: .jump   state_08
        c7e4: .jump   state_0a
        c7e6: .word   0000
        c7e8: .jump   state_0e
        c7ea: .jump   state_10
        c7ec: .jump   state_12
        c7ee: .jump   state_14
        c7f0: .jump   state_16
        c7f2: .jump   state_18
        c7f4: .jump   state_1a
        c7f6: .jump   state_1c
        c7f8: .jump   state_1e
        c7fa: .jump   state_20
        c7fc: .jump   state_22
        c7fe: .jump   state_24
    state_0a: a5 03     lda     timectr
        c802: 2d 6b 01  and     $016b
        c805: d0 11     bne     $c818
        c807: a5 04     lda     $04
        c809: f0 02     beq     $c80d
        c80b: c6 04     dec     $04
        c80d: d0 09     bne     $c818
        c80f: a5 02     lda     $02
        c811: 85 00     sta     gamestate
        c813: a9 00     lda     #$00
        c815: 8d 6b 01  sta     $016b
        c818: 4c 49 97  jmp     move_player
; Check to see if either START button is pressed, to start a new game.
; Called only when credits > 0.
 check_start: a5 06     lda     credits
        c81d: a0 00     ldy     #$00
        c81f: c9 02     cmp     #$02
        c821: a5 4e     lda     zap_fire_new
        c823: 29 60     and     #$60 ; start1 & start2
        c825: 84 4e     sty     zap_fire_new
        c827: f0 48     beq     $c871 ; branch if neither pressed
        c829: b0 05     bcs     $c830
        c82b: 29 20     and     #$20 ; start1
        c82d: b8        clv
        c82e: 50 05     bvc     $c835
        c830: c8        iny
        c831: c6 06     dec     credits
        c833: 29 40     and     #$40 ; start2
        c835: f0 03     beq     $c83a
        c837: c6 06     dec     credits
        c839: c8        iny
; Y now holds 1 if 1p game, 2 if 2p
        c83a: 98        tya
        c83b: 85 3e     sta     twoplayer
        c83d: f0 2f     beq     $c86e ; can this ever branch?
        c83f: a5 05     lda     $05
        c841: 09 c0     ora     #$c0
        c843: 85 05     sta     $05
        c845: a9 00     lda     #$00
        c847: 85 16     sta     coin_string
        c849: 85 18     sta     $18
        c84b: a9 00     lda     #$00
        c84d: 85 00     sta     gamestate
        c84f: c6 3e     dec     twoplayer
        c851: a6 3e     ldx     twoplayer
        c853: f0 02     beq     $c857
        c855: a2 03     ldx     #$03 ; 3 = games_2p_l - games_1p_l
        c857: fe 0c 04  inc     games_1p_l,x
        c85a: d0 03     bne     $c85f
        c85c: fe 0d 04  inc     games_1p_m,x
        c85f: ad 00 01  lda     $0100
        c862: 38        sec
        c863: 65 3e     adc     twoplayer
        c865: c9 63     cmp     #$63 ; 99 decimal
        c867: 90 02     bcc     $c86b
        c869: a9 63     lda     #$63 ; 99 again
        c86b: 8d 00 01  sta     $0100
        c86e: b8        clv
        c86f: 50 1f     bvc     $c890
; Branch here if neither start button pressed
        c871: a5 50     lda     $50
        c873: f0 1b     beq     $c890
        c875: 24 05     bit     $05
        c877: 30 17     bmi     $c890
        c879: a9 10     lda     #$10
        c87b: 85 01     sta     $01
        c87d: a9 20     lda     #$20
        c87f: 85 04     sta     $04
        c881: a9 0a     lda     #$0a
        c883: 85 00     sta     gamestate
        c885: a9 14     lda     #$14
        c887: 85 02     sta     $02
        c889: a9 00     lda     #$00
        c88b: 85 50     sta     $50
        c88d: 8d 23 01  sta     $0123
        c890: 60        rts
        c891: ad 00 0c  lda     cabsw
        c894: 29 10     and     #$10 ; service mode
        c896: d0 07     bne     $c89f
        c898: a9 22     lda     #$22
        c89a: 85 00     sta     gamestate
        c89c: b8        clv
        c89d: 50 44     bvc     $c8e3
        c89f: 24 05     bit     $05
        c8a1: 70 40     bvs     $c8e3
        c8a3: a5 0a     lda     optsw2_shadow
        c8a5: 29 01     and     #$01 ; 2-credit-minimum bit
        c8a7: f0 29     beq     $c8d2
        c8a9: a4 06     ldy     credits
        c8ab: d0 04     bne     $c8b1
        c8ad: a9 80     lda     #$80
        c8af: 85 a2     sta     $a2
        c8b1: 24 a2     bit     $a2
        c8b3: 10 1d     bpl     $c8d2
        c8b5: c0 02     cpy     #$02
        c8b7: b0 11     bcs     $c8ca
        c8b9: 98        tya
        c8ba: f0 08     beq     $c8c4
        c8bc: a9 16     lda     #$16
        c8be: 85 01     sta     $01
        c8c0: a9 0a     lda     #$0a
        c8c2: 85 00     sta     gamestate
        c8c4: 4c d9 c8  jmp     $c8d9
        c8c7: b8        clv
        c8c8: 50 08     bvc     $c8d2
        c8ca: a9 14     lda     #$14
        c8cc: 85 00     sta     gamestate
        c8ce: a9 00     lda     #$00
        c8d0: 85 a2     sta     $a2
        c8d2: a5 06     lda     credits
        c8d4: f0 03     beq     $c8d9
        c8d6: 20 1b c8  jsr     check_start
        c8d9: a5 09     lda     coinage_shadow
        c8db: 29 03     and     #$03
        c8dd: d0 04     bne     $c8e3
        c8df: a9 02     lda     #$02
        c8e1: 85 06     sta     credits
        c8e3: e6 03     inc     timectr
        c8e5: a5 03     lda     timectr
        c8e7: 29 01     and     #$01
        c8e9: f0 03     beq     $c8ee
        c8eb: 20 1b de  jsr     $de1b
        c8ee: a5 0c     lda     $0c
        c8f0: f0 03     beq     $c8f5
        c8f2: 20 fa cc  jsr     $ccfa
; Apparent anti-piracy test; if the copyright-displaying code has been
; twiddled, then gratuitously drop into decimal mode over level 19, thereby
; doing "interesting" things to arithmetic until we next have occasion to
; use decimal mode "legitimately".
; See also $a91c.
        c8f5: ad 6c 01  lda     copyr_disp_cksum1
        c8f8: f0 07     beq     $c901
        c8fa: a9 13     lda     #$13
        c8fc: c5 9f     cmp     curlevel
        c8fe: b0 01     bcs     $c901
        c900: f8        sed
; End apparent anti-piracy test
        c901: a5 4e     lda     zap_fire_new
        c903: 29 80     and     #$80
        c905: f0 04     beq     $c90b
        c907: a9 00     lda     #$00
        c909: 85 4e     sta     zap_fire_new
        c90b: 60        rts
    state_00: 20 a2 ab  jsr     maybe_init_hs
        c90f: 20 6e c1  jsr     $c16e
        c912: a5 05     lda     $05
        c914: 10 03     bpl     $c919
        c916: 20 62 ca  jsr     $ca62
        c919: a9 00     lda     #$00
        c91b: 85 49     sta     p2_lives
        c91d: a6 3e     ldx     twoplayer
        c91f: 86 3d     stx     curplayer
        c921: a6 3d     ldx     curplayer
        c923: ad 58 01  lda     init_lives
        c926: 9d 48 00  sta     p1_lives,x
        c929: a9 ff     lda     #$ff
        c92b: 9d 46 00  sta     p1_level,x
        c92e: c6 3d     dec     curplayer
        c930: 10 ef     bpl     $c921
        c932: a9 00     lda     #$00
        c934: 85 3f     sta     $3f
        c936: 8d 15 01  sta     $0115
        c939: a5 3e     lda     twoplayer
        c93b: 85 3d     sta     curplayer
        c93d: 4c c4 90  jmp     $90c4
    state_02: a9 00     lda     #$00
        c942: 85 01     sta     $01
        c944: a9 1e     lda     #$1e
        c946: 85 00     sta     gamestate
        c948: 85 02     sta     $02
        c94a: a5 3f     lda     $3f
        c94c: c5 3d     cmp     curplayer
        c94e: f0 1c     beq     $c96c
        c950: 85 3d     sta     curplayer
        c952: a5 05     lda     $05
        c954: 10 16     bpl     $c96c
        c956: a9 0e     lda     #$0e
        c958: 85 01     sta     $01
        c95a: a9 0a     lda     #$0a
        c95c: 85 00     sta     gamestate
        c95e: a9 50     lda     #$50
        c960: ac 17 01  ldy     flagbits
        c963: f0 02     beq     $c967
        c965: a9 28     lda     #$28
        c967: 85 04     sta     $04
        c969: 20 b2 92  jsr     $92b2
        c96c: 20 48 ca  jsr     $ca48
        c96f: a6 3d     ldx     curplayer
        c971: b5 46     lda     p1_level,x
        c973: 85 9f     sta     curlevel
        c975: 20 25 90  jsr     $9025
        c978: 4c 95 cd  jmp     $cd95
    state_1e: a9 04     lda     #$04
        c97d: 85 02     sta     $02
        c97f: a9 00     lda     #$00
        c981: 85 01     sta     $01
        c983: a9 0a     lda     #$0a
        c985: 85 00     sta     gamestate
        c987: a9 14     lda     #$14
        c989: 85 04     sta     $04
        c98b: 60        rts
    state_0e: a6 3d     ldx     curplayer
        c98e: b5 46     lda     p1_level,x
        c990: c9 62     cmp     #$62 ; 98 (level 99) is the max
        c992: b0 04     bcs     $c998
        c994: f6 46     inc     p1_level,x
        c996: e6 9f     inc     curlevel
        c998: a9 18     lda     #$18
        c99a: 85 00     sta     gamestate
        c99c: bd 02 01  lda     p1_startchoice,x
        c99f: f0 0b     beq     $c9ac
        c9a1: 20 b5 91  jsr     ld_startbonus
        c9a4: a2 ff     ldx     #$ff
        c9a6: 20 6c ca  jsr     inc_score
        c9a9: 20 b9 cc  jsr     $ccb9
        c9ac: 4c 09 90  jmp     $9009
    state_06: a9 00     lda     #$00
        c9b1: 85 04     sta     $04
        c9b3: a6 3d     ldx     curplayer
        c9b5: d6 48     dec     p1_lives,x
        c9b7: a5 48     lda     p1_lives
        c9b9: 05 49     ora     p2_lives
        c9bb: d0 06     bne     $c9c3
        c9bd: 20 f1 c9  jsr     state_08
        c9c0: b8        clv
        c9c1: 50 2d     bvc     $c9f0
        c9c3: a6 3d     ldx     curplayer
        c9c5: b5 48     lda     p1_lives,x
        c9c7: d0 08     bne     $c9d1
        c9c9: a9 0c     lda     #$0c
        c9cb: 85 01     sta     $01
        c9cd: a9 28     lda     #$28
        c9cf: 85 04     sta     $04
        c9d1: a5 3e     lda     twoplayer
        c9d3: f0 06     beq     $c9db
        c9d5: a5 3f     lda     $3f
        c9d7: 49 01     eor     #$01
        c9d9: 85 3f     sta     $3f
        c9db: a6 3f     ldx     $3f
        c9dd: b5 48     lda     p1_lives,x
        c9df: f0 f0     beq     $c9d1
        c9e1: a9 02     lda     #$02
        c9e3: b4 46     ldy     p1_level,x
        c9e5: c8        iny
        c9e6: d0 02     bne     $c9ea
        c9e8: a9 1c     lda     #$1c
        c9ea: 85 02     sta     $02
        c9ec: a9 0a     lda     #$0a
        c9ee: 85 00     sta     gamestate
        c9f0: 60        rts
    state_08: a9 00     lda     #$00
        c9f3: 8d 26 01  sta     $0126
        c9f6: a6 3e     ldx     twoplayer
        c9f8: b5 46     lda     p1_level,x
        c9fa: cd 26 01  cmp     $0126
        c9fd: 90 03     bcc     $ca02
        c9ff: 8d 26 01  sta     $0126
        ca02: ca        dex
        ca03: 10 f3     bpl     $c9f8
        ca05: ac 26 01  ldy     $0126
        ca08: f0 03     beq     $ca0d
        ca0a: ce 26 01  dec     $0126
        ca0d: a9 14     lda     #$14
        ca0f: 24 05     bit     $05
        ca11: 10 02     bpl     $ca15
        ca13: a9 10     lda     #$10
        ca15: 85 00     sta     gamestate
        ca17: 60        rts
    state_14: a5 05     lda     $05
        ca1a: 29 3f     and     #$3f
        ca1c: 85 05     sta     $05
        ca1e: a9 00     lda     #$00
        ca20: 85 3e     sta     twoplayer
        ca22: a9 1a     lda     #$1a ; new gamestate
        ca24: 85 02     sta     $02
        ca26: a9 0a     lda     #$0a
        ca28: 85 00     sta     gamestate
        ca2a: a9 a0     lda     #$a0 ; time delay
        ca2c: 85 04     sta     $04
        ca2e: a9 01     lda     #$01
        ca30: 8d 6b 01  sta     $016b
        ca33: a9 0a     lda     #$0a
        ca35: 85 01     sta     $01
        ca37: 60        rts
; Used at $98e6, $9910
        ca38: .byte   80
        ca39: .byte   40
        ca3a: .byte   20
        ca3b: .byte   10
        ca3c: .byte   08
        ca3d: .byte   04
        ca3e: .byte   02
        ca3f: .byte   01
        ca40: .byte   01
        ca41: .byte   02
        ca42: .byte   04
        ca43: .byte   08
        ca44: .byte   10
        ca45: .byte   20
        ca46: .byte   40
        ca47: .byte   80
        ca48: a0 10     ldy     #$10
        ca4a: ad 17 01  lda     flagbits
        ca4d: f0 08     beq     $ca57
        ca4f: a5 3d     lda     curplayer
        ca51: f0 04     beq     $ca57
        ca53: a9 04     lda     #$04
        ca55: a0 08     ldy     #$08
        ca57: 45 a1     eor     $a1
        ca59: 29 04     and     #$04
        ca5b: 45 a1     eor     $a1
        ca5d: 85 a1     sta     $a1
        ca5f: 84 b4     sty     $b4
        ca61: 60        rts
        ca62: a9 00     lda     #$00
        ca64: a2 05     ldx     #$05
        ca66: 95 40     sta     p1_score_l,x
        ca68: ca        dex
        ca69: 10 fb     bpl     $ca66
        ca6b: 60        rts
; Adds the number in $29/$2a/$2b (if X >= 8) or $caf1,x/$caf9,x/00 (else)
; to the current player's score.
   inc_score: f8        sed
        ca6d: 24 05     bit     $05
        ca6f: 10 7e     bpl     $caef
        ca71: a4 3d     ldy     curplayer
        ca73: f0 02     beq     $ca77
        ca75: a0 03     ldy     #$03
        ca77: e0 08     cpx     #$08
        ca79: 90 16     bcc     $ca91
        ca7b: a5 29     lda     $29
        ca7d: 18        clc
        ca7e: 79 40 00  adc     p1_score_l,y
        ca81: 99 40 00  sta     p1_score_l,y
        ca84: a5 2a     lda     $2a
        ca86: 79 41 00  adc     p1_score_m,y
        ca89: 99 41 00  sta     p1_score_m,y
        ca8c: a5 2b     lda     $2b
        ca8e: b8        clv
        ca8f: 50 15     bvc     $caa6
        ca91: bd f1 ca  lda     $caf1,x
        ca94: 18        clc
        ca95: 79 40 00  adc     p1_score_l,y
        ca98: 99 40 00  sta     p1_score_l,y
        ca9b: bd f9 ca  lda     $caf9,x
        ca9e: 79 41 00  adc     p1_score_m,y
        caa1: 99 41 00  sta     p1_score_m,y
        caa4: a9 00     lda     #$00
        caa6: 08        php
        caa7: 79 42 00  adc     p1_score_h,y
        caaa: 99 42 00  sta     p1_score_h,y
        caad: 28        plp
        caae: f0 0b     beq     $cabb
        cab0: ae 56 01  ldx     bonus_life_each
        cab3: f0 06     beq     $cabb
        cab5: e4 2b     cpx     $2b
        cab7: f0 23     beq     $cadc
        cab9: 90 21     bcc     $cadc
        cabb: 90 32     bcc     $caef
        cabd: ae 56 01  ldx     bonus_life_each
        cac0: f0 2c     beq     $caee
        cac2: e0 03     cpx     #$03
        cac4: 90 0b     bcc     $cad1
        cac6: 38        sec
        cac7: ed 56 01  sbc     bonus_life_each
        caca: f0 10     beq     $cadc
        cacc: b0 f8     bcs     $cac6
        cace: b8        clv
        cacf: 50 1d     bvc     $caee
        cad1: e0 02     cpx     #$02
        cad3: d0 07     bne     $cadc
        cad5: 29 01     and     #$01
        cad7: f0 03     beq     $cadc
        cad9: b8        clv
        cada: 50 12     bvc     $caee
        cadc: a6 3d     ldx     curplayer
        cade: b5 48     lda     p1_lives,x
        cae0: c9 06     cmp     #$06
        cae2: b0 0a     bcs     $caee
        cae4: f6 48     inc     p1_lives,x
        cae6: 20 b9 cc  jsr     $ccb9
        cae9: a9 20     lda     #$20
        caeb: 8d 24 01  sta     $0124
        caee: 38        sec
        caef: d8        cld
        caf0: 60        rts
; Low BCD nibbles of score values of something (enemies?).
; Used at ca91.
        caf1: .byte   00
        caf2: .byte   50
        caf3: .byte   00
        caf4: .byte   00
        caf5: .byte   50
        caf6: .byte   50
        caf7: .byte   00
        caf8: .byte   50
; High BCD nibbles of whatever's at caf1.
        caf9: .byte   00
        cafa: .byte   01
        cafb: .byte   02
        cafc: .byte   01
        cafd: .byte   00
        cafe: .byte   02
        caff: .byte   05
        cb00: .byte   07
; Not sure what this stuff is.
; It's blocks of 16 bytes, one of which is copied to $c0-$cf by the loop
; beginning around $ccc7.
; Block 0
        cb01: .chunk  16 ; 0 0 0 0 0 0 0 0 35 38 0 0 0 0 0 0
; Block 1
        cb11: .chunk  16 ; 0 0 47 4a 0 0 0 0 0 0 0 0 0 0 0 0
; Block 2
        cb21: .chunk  16 ; 0 0 0 0 0d 10 0 0 0 0 0 0 0 0 0 0
; Block 3
        cb31: .chunk  16 ; 0 0 0 0 0 0 0 0 0 0 65 68 0 0 0 0
; Block 4
        cb41: .chunk  16 ; 0 0 0 0 0 0 21 32 0 0 0 0 0 0 0 0
; Block 5
        cb51: .chunk  16 ; 13 1a 0 0 0 0 0 0 0 0 0 0 0 0 0 0
; Block 6
        cb61: .chunk  16 ; 0 0 0 0 0 0 0 0 0 0 53 56 0 0 0 0
; Block 7
        cb71: .chunk  16 ; 0 0 0 0 0 0 0 0 0 0 59 5c 0 0 0 0
; Block 8
        cb81: .chunk  16 ; 0 0 0 0 0 0 0 0 0 0 0 0 0 0 3b 3e
; Block 9
        cb91: .chunk  16 ; 0 0 0 0 0 0 0 0 0 0 0 0 41 44 0 0
; Block a
        cba1: .chunk  16 ; 4d 50 0 0 0 0 0 0 0 0 0 0 0 0 0 0
; Block b
        cbb1: .chunk  16 ; 5f 62 0 0 0 0 0 0 0 0 0 0 0 0 0 0
; Block c
        cbc1: .chunk  16 ; 0 0 0 0 0 0 0 0 0 0 6d 6d 0 0 0 0
        cbd1: .byte   c0
        cbd2: .byte   08
        cbd3: .byte   04
        cbd4: .byte   10
        cbd5: .byte   00
        cbd6: .byte   00
        cbd7: .byte   a6
        cbd8: .byte   20
        cbd9: .byte   f8
        cbda: .byte   04
        cbdb: .byte   00
        cbdc: .byte   00
        cbdd: .byte   40
        cbde: .byte   08
        cbdf: .byte   04
        cbe0: .byte   10
        cbe1: .byte   00
        cbe2: .byte   00
        cbe3: .byte   a6
        cbe4: .byte   20
        cbe5: .byte   fe
        cbe6: .byte   04
        cbe7: .byte   00
        cbe8: .byte   00
        cbe9: .byte   10
        cbea: .byte   01
        cbeb: .byte   07
        cbec: .byte   20
        cbed: .byte   00
        cbee: .byte   00
        cbef: .byte   a2
        cbf0: .byte   01
        cbf1: .byte   f8
        cbf2: .byte   20
        cbf3: .byte   00
        cbf4: .byte   00
        cbf5: .byte   08
        cbf6: .byte   04
        cbf7: .byte   20
        cbf8: .byte   0a
        cbf9: .byte   08
        cbfa: .byte   04
        cbfb: .byte   01
        cbfc: .byte   09
        cbfd: .byte   10
        cbfe: .byte   0d
        cbff: .byte   04
        cc00: .byte   0c
        cc01: .byte   00
        cc02: .byte   00
        cc03: .byte   08
        cc04: .byte   04
        cc05: .byte   00
        cc06: .byte   0a
        cc07: .byte   68
        cc08: .byte   04
        cc09: .byte   00
        cc0a: .byte   09
        cc0b: .byte   68
        cc0c: .byte   12
        cc0d: .byte   ff
        cc0e: .byte   09
        cc0f: .byte   00
        cc10: .byte   00
        cc11: .byte   40
        cc12: .byte   01
        cc13: .byte   00
        cc14: .byte   01
        cc15: .byte   40
        cc16: .byte   01
        cc17: .byte   ff
        cc18: .byte   40
        cc19: .byte   30
        cc1a: .byte   01
        cc1b: .byte   ff
        cc1c: .byte   30
        cc1d: .byte   20
        cc1e: .byte   01
        cc1f: .byte   ff
        cc20: .byte   20
        cc21: .byte   18
        cc22: .byte   01
        cc23: .byte   ff
        cc24: .byte   18
        cc25: .byte   14
        cc26: .byte   01
        cc27: .byte   ff
        cc28: .byte   14
        cc29: .byte   12
        cc2a: .byte   01
        cc2b: .byte   ff
        cc2c: .byte   12
        cc2d: .byte   10
        cc2e: .byte   01
        cc2f: .byte   ff
        cc30: .byte   10
        cc31: .byte   00
        cc32: .byte   00
        cc33: .byte   a8
        cc34: .byte   93
        cc35: .byte   00
        cc36: .byte   02
        cc37: .byte   00
        cc38: .byte   00
        cc39: .byte   0f
        cc3a: .byte   04
        cc3b: .byte   00
        cc3c: .byte   01
        cc3d: .byte   00
        cc3e: .byte   00
        cc3f: .byte   a2
        cc40: .byte   04
        cc41: .byte   40
        cc42: .byte   01
        cc43: .byte   00
        cc44: .byte   00
        cc45: .byte   00
        cc46: .byte   03
        cc47: .byte   02
        cc48: .byte   09
        cc49: .byte   00
        cc4a: .byte   00
        cc4b: .byte   08
        cc4c: .byte   03
        cc4d: .byte   ff
        cc4e: .byte   09
        cc4f: .byte   00
        cc50: .byte   00
        cc51: .byte   80
        cc52: .byte   01
        cc53: .byte   e8
        cc54: .byte   05
        cc55: .byte   00
        cc56: .byte   00
        cc57: .byte   a1
        cc58: .byte   01
        cc59: .byte   01
        cc5a: .byte   05
        cc5b: .byte   00
        cc5c: .byte   00
        cc5d: .byte   01
        cc5e: .byte   08
        cc5f: .byte   02
        cc60: .byte   10
        cc61: .byte   00
        cc62: .byte   00
        cc63: .byte   86
        cc64: .byte   20
        cc65: .byte   00
        cc66: .byte   04
        cc67: .byte   00
        cc68: .byte   00
        cc69: .byte   18
        cc6a: .byte   04
        cc6b: .byte   00
        cc6c: .byte   ff
        cc6d: .byte   00
        cc6e: .byte   00
        cc6f: .byte   af
        cc70: .byte   04
        cc71: .byte   00
        cc72: .byte   ff
        cc73: .byte   00
        cc74: .byte   00
        cc75: .byte   c0
        cc76: .byte   02
        cc77: .byte   ff
        cc78: .byte   ff
        cc79: .byte   00
        cc7a: .byte   00
        cc7b: .byte   28
        cc7c: .byte   02
        cc7d: .byte   00
        cc7e: .byte   f0
        cc7f: .byte   00
        cc80: .byte   00
        cc81: .byte   10
        cc82: .byte   0b
        cc83: .byte   01
        cc84: .byte   40
        cc85: .byte   00
        cc86: .byte   00
        cc87: .byte   86
        cc88: .byte   40
        cc89: .byte   00
        cc8a: .byte   0b
        cc8b: .byte   00
        cc8c: .byte   00
        cc8d: .byte   20
        cc8e: .byte   80
        cc8f: .byte   00
        cc90: .byte   03
        cc91: .byte   00
        cc92: .byte   00
        cc93: .byte   a8
        cc94: .byte   40
        cc95: .byte   f8
        cc96: .byte   06
        cc97: .byte   00
        cc98: .byte   00
        cc99: .byte   b0
        cc9a: .byte   02
        cc9b: .byte   00
        cc9c: .byte   ff
        cc9d: .byte   00
        cc9e: .byte   00
        cc9f: .byte   c8
        cca0: .byte   01
        cca1: .byte   02
        cca2: .byte   ff
        cca3: .byte   c8
        cca4: .byte   01
        cca5: .byte   02
        cca6: .byte   ff
        cca7: .byte   00
        cca8: .byte   00
        cca9: .byte   c0
        ccaa: .byte   01
        ccab: .byte   00
        ccac: .byte   01
        ccad: .byte   00
        ccae: .byte   00
        ccaf: .byte   00
        ccb0: a9 5f     lda     #$5f ; 13 1a 0 0 0 0 0 0 0 0 0 0 0 0 0 0
        ccb2: 4c c3 cc  jmp     $ccc3
        ccb5: a9 0f     lda     #$0f ; 0 0 0 0 0 0 0 0 35 38 0 0 0 0 0 0
        ccb7: d0 0a     bne     $ccc3
        ccb9: a9 4f     lda     #$4f ; 0 0 0 0 0 0 21 32 0 0 0 0 0 0 0 0
        ccbb: d0 06     bne     $ccc3
        ccbd: a9 8f     lda     #$8f ; 0 0 0 0 0 0 0 0 0 0 0 0 0 0 3b 3e
        ccbf: d0 02     bne     $ccc3
        ccc1: a9 1f     lda     #$1f ; 0 0 47 4a 0 0 0 0 0 0 0 0 0 0 0 0
        ccc3: 24 05     bit     $05
        ccc5: 10 22     bpl     $cce9
        ccc7: 86 31     stx     $31
        ccc9: 84 32     sty     $32
        cccb: a8        tay
        cccc: a2 0f     ldx     #$0f
        ccce: b9 01 cb  lda     $cb01,y
        ccd1: f0 0e     beq     $cce1
        ccd3: 86 bf     stx     $bf
        ccd5: 95 c0     sta     $c0,x
        ccd7: a9 01     lda     #$01
        ccd9: 95 e0     sta     $e0,x
        ccdb: 95 f0     sta     $f0,x
        ccdd: a9 ff     lda     #$ff
        ccdf: 85 bf     sta     $bf
        cce1: 88        dey
        cce2: ca        dex
        cce3: 10 e9     bpl     $ccce
        cce5: a6 31     ldx     $31
        cce7: a4 32     ldy     $32
        cce9: 60        rts
        ccea: a9 2f     lda     #$2f ; 0 0 0 0 0d 10 0 0 0 0 0 0 0 0 0 0
        ccec: d0 d5     bne     $ccc3
        ccee: a9 6f     lda     #$6f ; 0 0 0 0 0 0 0 0 0 0 53 56 0 0 0 0
        ccf0: d0 d1     bne     $ccc3
        ccf2: a9 7f     lda     #$7f ; 0 0 0 0 0 0 0 0 0 0 59 5c 0 0 0 0
        ccf4: d0 cd     bne     $ccc3
        ccf6: a9 9f     lda     #$9f ; 0 0 0 0 0 0 0 0 0 0 0 0 41 44 0 0
        ccf8: d0 c9     bne     $ccc3
        ccfa: a9 af     lda     #$af ; 4d 50 0 0 0 0 0 0 0 0 0 0 0 0 0 0
        ccfc: d0 c9     bne     $ccc7
        ccfe: a9 bf     lda     #$bf ; 5f 62 0 0 0 0 0 0 0 0 0 0 0 0 0 0
        cd00: d0 c1     bne     $ccc3
        cd02: a9 3f     lda     #$3f ; 0 0 0 0 0 0 0 0 0 0 65 68 0 0 0 0
        cd04: d0 bd     bne     $ccc3
sound_pulsar: a9 cf     lda     #$cf ; 0 0 0 0 0 0 0 0 0 0 6d 6d 0 0 0 0
        cd08: d0 b9     bne     $ccc3
        cd0a: a2 0f     ldx     #$0f
        cd0c: b5 c0     lda     $c0,x
        cd0e: f0 7e     beq     $cd8e
        cd10: e4 bf     cpx     $bf
        cd12: f0 7a     beq     $cd8e
        cd14: d6 e0     dec     $e0,x
        cd16: d0 76     bne     $cd8e
        cd18: d6 f0     dec     $f0,x
        cd1a: d0 38     bne     $cd54
        cd1c: f6 c0     inc     $c0,x
        cd1e: f6 c0     inc     $c0,x
        cd20: b5 c0     lda     $c0,x
        cd22: 0a        asl     a
        cd23: a8        tay
        cd24: b0 10     bcs     $cd36
        cd26: b9 cb cb  lda     $cbcb,y
        cd29: 95 d0     sta     $d0,x
        cd2b: b9 ce cb  lda     $cbce,y
        cd2e: 95 f0     sta     $f0,x
        cd30: b9 cc cb  lda     $cbcc,y
        cd33: b8        clv
        cd34: 50 0d     bvc     $cd43
        cd36: b9 cb cc  lda     $cccb,y
        cd39: 95 d0     sta     $d0,x
        cd3b: b9 ce cc  lda     $ccce,y
        cd3e: 95 f0     sta     $f0,x
        cd40: b9 cc cc  lda     $cccc,y
        cd43: 95 e0     sta     $e0,x
        cd45: d0 0a     bne     $cd51
        cd47: 95 c0     sta     $c0,x
        cd49: b5 d0     lda     $d0,x
        cd4b: f0 04     beq     $cd51
        cd4d: 95 c0     sta     $c0,x
        cd4f: d0 cb     bne     $cd1c
        cd51: b8        clv
        cd52: 50 2b     bvc     $cd7f
        cd54: 0a        asl     a
        cd55: a8        tay
        cd56: b0 0b     bcs     $cd63
        cd58: b9 cc cb  lda     $cbcc,y
        cd5b: 95 e0     sta     $e0,x
        cd5d: b9 cd cb  lda     $cbcd,y
        cd60: b8        clv
        cd61: 50 08     bvc     $cd6b
        cd63: b9 cc cc  lda     $cccc,y
        cd66: 95 e0     sta     $e0,x
        cd68: b9 cd cc  lda     $cccd,y
        cd6b: b4 d0     ldy     $d0,x
        cd6d: 18        clc
        cd6e: 75 d0     adc     $d0,x
        cd70: 95 d0     sta     $d0,x
        cd72: 8a        txa
        cd73: 4a        lsr     a
        cd74: 90 09     bcc     $cd7f
        cd76: 98        tya
        cd77: 55 d0     eor     $d0,x
        cd79: 29 f0     and     #$f0
        cd7b: 55 d0     eor     $d0,x
        cd7d: 95 d0     sta     $d0,x
        cd7f: b5 d0     lda     $d0,x
        cd81: e0 08     cpx     #$08
        cd83: 90 06     bcc     $cd8b
        cd85: 9d c8 60  sta     spinner_cabtyp,x
        cd88: b8        clv
        cd89: 50 03     bvc     $cd8e
        cd8b: 9d c0 60  sta     pokey1,x
        cd8e: ca        dex
        cd8f: 30 03     bmi     $cd94
        cd91: 4c 0c cd  jmp     $cd0c
        cd94: 60        rts
; Commented in another disassembly as pokey initialization
        cd95: a9 00     lda     #$00
        cd97: 8d cf 60  sta     $60cf
        cd9a: 8d df 60  sta     $60df
        cd9d: 8d 20 07  sta     $0720
        cda0: a2 04     ldx     #$04
        cda2: ad ca 60  lda     pokey1_rand
        cda5: ac da 60  ldy     pokey2_rand
        cda8: cd ca 60  cmp     pokey1_rand
        cdab: d0 03     bne     $cdb0
        cdad: cc da 60  cpy     pokey2_rand
        cdb0: f0 05     beq     $cdb7
        cdb2: 8d 20 07  sta     $0720
        cdb5: a2 00     ldx     #$00
        cdb7: ca        dex
        cdb8: 10 ee     bpl     $cda8
        cdba: a9 07     lda     #$07
        cdbc: 8d cf 60  sta     $60cf
        cdbf: 8d df 60  sta     $60df
        cdc2: a2 07     ldx     #$07
        cdc4: a9 00     lda     #$00
        cdc6: 9d c0 60  sta     pokey1,x
        cdc9: 9d d0 60  sta     pokey2,x
        cdcc: 95 c0     sta     $c0,x
        cdce: 95 d0     sta     $d0,x
        cdd0: ca        dex
        cdd1: 10 f3     bpl     $cdc6
        cdd3: a9 00     lda     #$00
        cdd5: 8d c8 60  sta     spinner_cabtyp
        cdd8: a9 00     lda     #$00
        cdda: 8d d8 60  sta     zap_fire_starts
        cddd: 60        rts
; Indexed by player number; see $a991
        cdde: .byte   0b
        cddf: .byte   5d
; Indexed by player number; see $a997
        cde0: .byte   22
        cde1: .byte   74
; Indexed by player number; see $a9cb
        cde2: .byte   0c
        cde3: .byte   5e
        cde4: .byte   34
; Constant used at $a929
; Offset of high-score initials code from $2f60
        cde5: .byte   50
hdr_template: 7100      vscale  b=1 l=0
        cde8: 68c5      vstat   z=12 c=5 sparkle=1
        cdea: 8040      vcentre
        cdec: 016c1e40  vldraw  x=-448 y=+364 z=off
        cdf0: 7100      vscale  b=1 l=0
        cdf2: a8b4      vjsr    $1168 ; P1 score digits
        cdf4: a8b4      vjsr    $1168
        cdf6: a8b4      vjsr    $1168
        cdf8: a8b4      vjsr    $1168
        cdfa: a8b4      vjsr    $1168
        cdfc: a865      vjsr    $10ca
        cdfe: 00001f70  vldraw  x=-144 y=+0 z=off
        ce02: 7100      vscale  b=1 l=0
        ce04: 5800      vsdraw  x=+0 y=-16 z=off
        ce06: 68c1      vstat   z=12 c=1 sparkle=1
        ce08: a93f      vjsr    $127e ; P1 spare lives
        ce0a: a93f      vjsr    $127e
        ce0c: a93f      vjsr    $127e
        ce0e: a93f      vjsr    $127e
        ce10: a93f      vjsr    $127e
        ce12: a93f      vjsr    $127e
        ce14: 00301fd0  vldraw  x=-48 y=+48 z=off
        ce18: 68c5      vstat   z=12 c=5 sparkle=1
        ce1a: a8b4      vjsr    $1168 ; Highest score
        ce1c: a8b4      vjsr    $1168
        ce1e: a8b4      vjsr    $1168
        ce20: a8b4      vjsr    $1168
        ce22: a8b4      vjsr    $1168
        ce24: a8b4      vjsr    $1168
        ce26: 1fdc0000  vldraw  x=+0 y=-36 z=off
        ce2a: 68c7      vstat   z=12 c=7 sparkle=1
        ce2c: a8b4      vjsr    $1168 ; Current level number
        ce2e: a8b4      vjsr    $1168
        ce30: 68c5      vstat   z=12 c=5 sparkle=1
        ce32: 00241fe8  vldraw  x=-24 y=+36 z=off
        ce36: a8b4      vjsr    $1168 ; Highest score's initials
        ce38: a8b4      vjsr    $1168
        ce3a: a8b4      vjsr    $1168
        ce3c: 7100      vscale  b=1 l=0
; Two-player-only stuff from here on
        ce3e: 1fe00028  vldraw  x=+40 y=-32 z=off
        ce42: 7100      vscale  b=1 l=0
        ce44: a8b4      vjsr    $1168 ; P2 score
        ce46: a8b4      vjsr    $1168
        ce48: a8b4      vjsr    $1168
        ce4a: a8b4      vjsr    $1168
        ce4c: a8b4      vjsr    $1168
        ce4e: a865      vjsr    $10ca
        ce50: 00001f70  vldraw  x=-144 y=+0 z=off
        ce54: 7100      vscale  b=1 l=0
        ce56: 5800      vsdraw  x=+0 y=-16 z=off
        ce58: 68c1      vstat   z=12 c=1 sparkle=1
        ce5a: a93f      vjsr    $127e ; P2 spare lives
        ce5c: a93f      vjsr    $127e
        ce5e: a93f      vjsr    $127e
        ce60: a93f      vjsr    $127e
        ce62: a93f      vjsr    $127e
        ce64: a93f      vjsr    $127e
; See $aa2b.  These are the amount of the canned video sequence to copy.
        ce66: .byte   55
        ce67: .byte   7f
; Tables of addresses of areas to assemble various video sequences into.
; 
; These are the 6502-seen addresses; see the vjmp tables below, which hold
; the corresponding vector-generator addresses in the form of a vjmp.
; These tables are used at b2c1 and b2e1; which one is used is based on
; the values in the dblbuf_flg vector.
; Table A
dblbuf_addr_A: .word   2006
        ce6a: .word   2202
        ce6c: .word   240c
        ce6e: .word   2692
        ce70: .word   2900
        ce72: .word   2a56
        ce74: .word   2cd8
        ce76: .word   2dbe
        ce78: .word   2e24
; Table B
dblbuf_addr_B: .word   2104
        ce7c: .word   2306
        ce7e: .word   254e
        ce80: .word   27c8
        ce82: .word   29aa
        ce84: .word   2b96
        ce86: .word   2d4a
        ce88: .word   2df0
        ce8a: .word   2ea6
; This table gives the location to stuff the vjmp from the tables below
; depending on which way the double-buffering flag is set.
dblbuf_vjsr_loc: .word   2004
        ce8e: .word   2200
        ce90: .word   240a
        ce92: .word   2690
        ce94: .word   28fe
        ce96: .word   2a54
        ce98: .word   2cd6
        ce9a: .word   2dbc
        ce9c: .word   2e22
; These tables contain vjmp instructions corresponding to Tables A and B
; above.  The addresses here are the vector-generator-visible addresses
; that refer to the same video RAM as the 6502-visible addresses above.
; Table C
dblbuf_vjmp_C: e003      vjmp    $0006
        cea0: e101      vjmp    $0202
        cea2: e206      vjmp    $040c
        cea4: e349      vjmp    $0692
        cea6: e480      vjmp    $0900
        cea8: e52b      vjmp    $0a56
        ceaa: e66c      vjmp    $0cd8
        ceac: e6df      vjmp    $0dbe
        ceae: e712      vjmp    $0e24
; Table D
dblbuf_vjmp_D: e082      vjmp    $0104
        ceb2: e183      vjmp    $0306
        ceb4: e2a7      vjmp    $054e
        ceb6: e3e4      vjmp    $07c8
        ceb8: e4d5      vjmp    $09aa
        ceba: e5cb      vjmp    $0b96
        cebc: e6a5      vjmp    $0d4a
        cebe: e6f8      vjmp    $0df0
        cec0: e753      vjmp    $0ea6
; I don't know what these three are used for.  The first jumps to a
; routine that calls the double-buffered stuff (in an order different
; from that of the above tables); the second calls just one of them,
; and the third does nothing.  All three halt after doing their things.
; But see $b1bc.
        cec2: eeda      vjmp    $1db4
        cec4: eee4      vjmp    $1dc8
        cec6: eee6      vjmp    $1dcc
graphic_table: aa61      vjsr    $14c2 ; explosion, size 1
        ceca: aa7c      vjsr    $14f8 ; explosion, size 2
        cecc: aa91      vjsr    $1522 ; explosion, size 3
        cece: aaad      vjsr    $155a ; explosion, size 4
        ced0: aaca      vjsr    $1594 ; player shot
        ced2: ab14      vjsr    $1628 ; cloud of dots 1
        ced4: ab6f      vjsr    $16de ; cloud of dots 2
        ced6: abc0      vjsr    $1780 ; cloud of dots 3
        ced8: ac15      vjsr    $182a ; cloud of dots 4
        ceda: ac66      vjsr    $18cc ; spiker 1
        cedc: ac7d      vjsr    $18fa ; spiker 2
        cede: ac94      vjsr    $1928 ; spiker 3
        cee0: acab      vjsr    $1956 ; spiker 4
        cee2: acd8      vjsr    $19b0 ; regular (flipper) tanker
        cee4: acfa      vjsr    $19f4 ; four dots, orthogonal
        cee6: ad0d      vjsr    $1a1a ; four dots, diagonal
        cee8: ad20      vjsr    $1a40 ; enemy shot 1
        ceea: ad39      vjsr    $1a72 ; enemy shot 2
        ceec: ad51      vjsr    $1aa2 ; enemy shot 3
        ceee: ad6a      vjsr    $1ad4 ; enemy shot 4
        cef0: ad8c      vjsr    $1b18 ; hit-by-shot explosion 1
        cef2: ad8a      vjsr    $1b14 ; hit-by-shot explosion 2
        cef4: ad88      vjsr    $1b10 ; hit-by-shot explosion 3
        cef6: ad86      vjsr    $1b0c ; hit-by-shot explosion 4
        cef8: ad84      vjsr    $1b08 ; hit-by-shot explosion 5
        cefa: ad82      vjsr    $1b04 ; hit-by-shot explosion 6
        cefc: ad86      vjsr    $1b0c ; hit-by-shot explosion 7
        cefe: ad8a      vjsr    $1b14 ; hit-by-shot explosion 8
        cf00: ad8c      vjsr    $1b18 ; hit-by-shot explosion 9
        cf02: add7      vjsr    $1bae ; spiked player
        cf04: adc2      vjsr    $1b84 ; fuzzballed player 1
        cf06: adc5      vjsr    $1b8a ; fuzzballed player 2
        cf08: adc8      vjsr    $1b90 ; fuzzballed player 3
        cf0a: adcb      vjsr    $1b96 ; fuzzballed player 4
        cf0c: adce      vjsr    $1b9c ; fuzzballed player 5
        cf0e: add1      vjsr    $1ba2 ; fuzzballed player 6
        cf10: add4      vjsr    $1ba8 ; fuzzballed player 7
        cf12: acc2      vjsr    $1984 ; pulsar-holding tanker
        cf14: accb      vjsr    $1996 ; fuzzball?-holding tanker
        cf16: ae35      vjsr    $1c6a ; fuzzball 1
        cf18: ae59      vjsr    $1cb2 ; fuzzball 2
        cf1a: ae7e      vjsr    $1cfc ; fuzzball 3
        cf1c: aea2      vjsr    $1d44 ; fuzzball 4
        cf1e: aec5      vjsr    $1d8a ; 750
        cf20: aecb      vjsr    $1d96 ; 500
        cf22: aed2      vjsr    $1da4 ; 250
        cf24: a2 02     ldx     #$02
        cf26: ad 08 00  lda     zap_fire_shadow
        cf29: e0 01     cpx     #$01
        cf2b: f0 03     beq     $cf30
        cf2d: b0 02     bcs     $cf31
        cf2f: 4a        lsr     a
        cf30: 4a        lsr     a
        cf31: 4a        lsr     a
        cf32: b5 0d     lda     $0d,x
        cf34: 29 1f     and     #$1f
        cf36: b0 37     bcs     $cf6f
        cf38: f0 10     beq     $cf4a
        cf3a: c9 1b     cmp     #$1b
        cf3c: b0 0a     bcs     $cf48
        cf3e: a8        tay
        cf3f: a5 07     lda     $07
        cf41: 29 07     and     #$07
        cf43: c9 07     cmp     #$07
        cf45: 98        tya
        cf46: 90 02     bcc     $cf4a
        cf48: e9 01     sbc     #$01
        cf4a: 95 0d     sta     $0d,x
        cf4c: ad 08 00  lda     zap_fire_shadow
        cf4f: 29 08     and     #$08
        cf51: d0 04     bne     $cf57
        cf53: a9 f0     lda     #$f0
        cf55: 85 0c     sta     $0c
        cf57: a5 0c     lda     $0c
        cf59: f0 08     beq     $cf63
        cf5b: c6 0c     dec     $0c
        cf5d: a9 00     lda     #$00
        cf5f: 95 0d     sta     $0d,x
        cf61: 95 10     sta     $10,x
        cf63: 18        clc
        cf64: b5 10     lda     $10,x
        cf66: f0 23     beq     $cf8b
        cf68: d6 10     dec     $10,x
        cf6a: d0 1f     bne     $cf8b
        cf6c: 38        sec
        cf6d: b0 1c     bcs     $cf8b
        cf6f: c9 1b     cmp     #$1b
        cf71: b0 09     bcs     $cf7c
        cf73: b5 0d     lda     $0d,x
        cf75: 69 20     adc     #$20
        cf77: 90 d1     bcc     $cf4a
        cf79: f0 01     beq     $cf7c
        cf7b: 18        clc
        cf7c: a9 1f     lda     #$1f
        cf7e: b0 ca     bcs     $cf4a
        cf80: 95 0d     sta     $0d,x
        cf82: b5 10     lda     $10,x
        cf84: f0 01     beq     $cf87
        cf86: 38        sec
        cf87: a9 78     lda     #$78
        cf89: 95 10     sta     $10,x
        cf8b: 90 2a     bcc     $cfb7
        cf8d: a9 00     lda     #$00
        cf8f: e0 01     cpx     #$01
        cf91: 90 16     bcc     $cfa9
        cf93: f0 0c     beq     $cfa1
; coin in right slot
        cf95: a5 09     lda     coinage_shadow
        cf97: 29 0c     and     #$0c ; right slot multiplier
        cf99: 4a        lsr     a
        cf9a: 4a        lsr     a
        cf9b: f0 0c     beq     $cfa9 ; branch if x1
        cf9d: 69 02     adc     #$02
        cf9f: d0 08     bne     $cfa9
; coin in left slot
        cfa1: a5 09     lda     coinage_shadow
        cfa3: 29 10     and     #$10 ; left slot multiplier
        cfa5: f0 02     beq     $cfa9
        cfa7: a9 01     lda     #$01
; At this point, A holds the post-multiplier coin count, minus 1.
        cfa9: 38        sec
        cfaa: 48        pha
        cfab: 65 16     adc     coin_string
        cfad: 85 16     sta     coin_string
        cfaf: 68        pla
        cfb0: 38        sec
        cfb1: 65 17     adc     uncredited
        cfb3: 85 17     sta     uncredited
        cfb5: f6 13     inc     $13,x
        cfb7: ca        dex
        cfb8: 30 03     bmi     $cfbd
        cfba: 4c 26 cf  jmp     $cf26
        cfbd: a5 09     lda     coinage_shadow
        cfbf: 4a        lsr     a ; extract bonus coins bits
        cfc0: 4a        lsr     a
        cfc1: 4a        lsr     a
        cfc2: 4a        lsr     a
        cfc3: 4a        lsr     a
        cfc4: a8        tay
        cfc5: a5 16     lda     coin_string
        cfc7: 38        sec
        cfc8: f9 d9 cf  sbc     $cfd9,y
        cfcb: 30 14     bmi     $cfe1
        cfcd: 85 16     sta     coin_string
        cfcf: e6 18     inc     $18
        cfd1: c0 03     cpy     #$03 ; setting for 2 bonus
        cfd3: d0 0c     bne     $cfe1
        cfd5: e6 18     inc     $18
        cfd7: d0 08     bne     $cfe1
; Bonus coins table (see code just above)
        cfd9: .byte   7f ; no bonus coins
        cfda: .byte   02 ; 1 bonus for each 2
        cfdb: .byte   04 ; 1 bonus for each 4
        cfdc: .byte   04 ; 2 bonus for each 4
        cfdd: .byte   05 ; 1 bonus for each 5
        cfde: .byte   03 ; 1 bonus for each 3
        cfdf: .byte   7f ; no bonus coins
        cfe0: .byte   7f ; no bonus coins
        cfe1: a5 09     lda     coinage_shadow
        cfe3: 29 03     and     #$03 ; coins-to-credits bits (XOR $02)
        cfe5: a8        tay
        cfe6: f0 1a     beq     $d002 ; branch if free play
; A now 1 for 1c/2c, 2 for 1c/1c, 3 for 2c/1c
        cfe8: 4a        lsr     a
        cfe9: 69 00     adc     #$00
        cfeb: 49 ff     eor     #$ff
; A now fe for 1c/*, fd for 2c/1c
        cfed: 38        sec
        cfee: 65 17     adc     uncredited
        cff0: b0 08     bcs     $cffa
        cff2: 65 18     adc     $18
        cff4: 30 0e     bmi     $d004
        cff6: 85 18     sta     $18
        cff8: a9 00     lda     #$00
        cffa: c0 02     cpy     #$02
        cffc: b0 02     bcs     $d000
        cffe: e6 06     inc     credits
        d000: e6 06     inc     credits
        d002: 85 17     sta     uncredited
        d004: a5 07     lda     $07
        d006: 4a        lsr     a
        d007: b0 27     bcs     $d030
        d009: a0 00     ldy     #$00
        d00b: a2 02     ldx     #$02
        d00d: b5 13     lda     $13,x
        d00f: f0 09     beq     $d01a
        d011: c9 10     cmp     #$10
        d013: 90 05     bcc     $d01a
        d015: 69 ef     adc     #$ef
        d017: c8        iny
        d018: 95 13     sta     $13,x
        d01a: ca        dex
        d01b: 10 f0     bpl     $d00d
        d01d: 98        tya
        d01e: d0 10     bne     $d030
        d020: a2 02     ldx     #$02
        d022: b5 13     lda     $13,x
        d024: f0 07     beq     $d02d
        d026: 18        clc
        d027: 69 ef     adc     #$ef
        d029: 95 13     sta     $13,x
        d02b: 30 03     bmi     $d030
        d02d: ca        dex
        d02e: 10 f2     bpl     $d022
        d030: 60        rts
     msgs_en: .word   d15d ; "GAME OVER"
        d033: .word   d18f ; "PLAYER "
        d035: .word   d18f ; "PLAYER "
        d037: .word   d1b1 ; "PRESS START"
        d039: .word   d1eb ; "PLAY"
        d03b: .word   d203 ; "ENTER YOUR INITIALS"
        d03d: .word   d261 ; "SPIN KNOB TO CHANGE"
        d03f: .word   d2cb ; "PRESS FIRE TO SELECT"
        d041: .word   d333 ; "HIGH SCORES"
        d043: .word   d366 ; "RANKING FROM 1 TO "
        d045: .word   d3b0 ; "RATE YOURSELF"
        d047: .word   d3e6 ; "NOVICE"
        d049: .word   d3ff ; "EXPERT"
        d04b: .word   d417 ; "BONUS"
        d04d: .word   d41d ; "TIME"
        d04f: .word   d434 ; "LEVEL"
        d051: .word   d44c ; "HOLE"
        d053: .word   d460 ; "INSERT COINS"
        d055: .word   d4a1 ; "FREE PLAY"
        d057: .word   d4ab ; "1 COIN 2 PLAYS"
        d059: .word   d4ef ; "1 COIN 1 PLAY"
        d05b: .word   d530 ; "2 COINS 1 PLAY"
        d05d: .word   d575 ; "(c) MCMLXXX ATARI"
        d05f: .word   d585 ; "CREDITS "
        d061: .word   d5a1 ; "BONUS "
        d063: .word   d5a8 ; "2 CREDIT MINIMUM"
        d065: .word   d5e9 ; "BONUS EVERY "
        d067: .word   d61c ; "AVOID SPIKES"
        d069: .word   d662 ; "LEVEL"
        d06b: .word   d67a ; "SUPERZAPPER RECHARGE"
     msgs_fr: .word   d167 ; "FIN DE PARTIE"
        d06f: .word   d197 ; "JOUEUR "
        d071: .word   d197 ; "JOUEUR "
        d073: .word   d1bd ; "APPUYEZ SUR START"
        d075: .word   d1f0 ; "JOUEZ"
        d077: .word   d217 ; "SVP ENTREZ VOS INITIALES"
        d079: .word   d275 ; "TOURNEZ LE BOUTON POUR CHANGER"
        d07b: .word   d2e0 ; "POUSSEZ FEU QUAND CORRECTE"
        d07d: .word   d33f ; "MEILLEURS SCORES"
        d07f: .word   d379 ; "PLACEMENT DE 1 A "
        d081: .word   d3be ; "EVALUEZ-VOUS"
        d083: .word   d3e6 ; "NOVICE"
        d085: .word   d3ff ; "EXPERT"
        d087: .word   d417 ; "BONUS"
        d089: .word   d422 ; "DUREE"
        d08b: .word   d43a ; "NIVEAU"
        d08d: .word   d451 ; "TROU"
        d08f: .word   d46d ; "INTRODUIRE LES PIECES"
        d091: .word   d4a1 ; "FREE PLAY"
        d093: .word   d4ba ; "1 PIECE 2 JOUEURS"
        d095: .word   d4fd ; "1 PIECE 1 JOUEUR"
        d097: .word   d53f ; "2 PIECES 1 JOUEUR"
        d099: .word   d575 ; "(c) MCMLXXX ATARI"
        d09b: .word   d585 ; "CREDITS "
        d09d: .word   d5a1 ; "BONUS "
        d09f: .word   d5b9 ; "2 JEUX MINIMUM"
        d0a1: .word   d5f6 ; "BONUS CHAQUE "
        d0a3: .word   d629 ; "ATTENTION AUX LANCES"
        d0a5: .word   d668 ; "NIVEAU"
        d0a7: .word   d67a ; "SUPERZAPPER RECHARGE"
     msgs_de: .word   d175 ; "SPIELENDE"
        d0ab: .word   d19f ; "SPIELER "
        d0ad: .word   d19f ; "SPIELER "
        d0af: .word   d1cf ; "START DRUECKEN"
        d0b1: .word   d1f6 ; "SPIEL"
        d0b3: .word   d230 ; "GEBEN SIE IHRE INITIALEN EIN"
        d0b5: .word   d294 ; "KNOPF DREHEN ZUM WECHSELN"
        d0b7: .word   d2fb ; "FIRE DRUECKEN WENN RICHTIG"
        d0b9: .word   d350 ; "HOECHSTZAHLEN"
        d0bb: .word   d38b ; "RANGLISTE VON 1 ZUM "
        d0bd: .word   d3cb ; "SELBST RECHNEN"
        d0bf: .word   d3f5 ; "ANFAENGER"
        d0c1: .word   d40e ; "ERFAHREN"
        d0c3: .word   d417 ; "BONUS"
        d0c5: .word   d428 ; "ZEIT"
        d0c7: .word   d441 ; "GRAD"
        d0c9: .word   d45b ; "LOCH"
        d0cb: .word   d483 ; "GELD EINWERFEN"
        d0cd: .word   d4a1 ; "FREE PLAY"
        d0cf: .word   d4cc ; "1 MUENZ 2 SPIELE"
        d0d1: .word   d50e ; "1 MUENZE 1 SPIEL"
        d0d3: .word   d551 ; "2 MUENZEN 1 SPIEL"
        d0d5: .word   d575 ; "(c) MCMLXXX ATARI"
        d0d7: .word   d58e ; "KREDITE "
        d0d9: .word   d5a1 ; "BONUS "
        d0db: .word   d5c8 ; "2 SPIELE MINIMUM"
        d0dd: .word   d604 ; "BONUS JEDE "
        d0df: .word   d63e ; "SPITZEN AUSWEICHEN"
        d0e1: .word   d66f ; "GRAD"
        d0e3: .word   d68f ; "NEUER SUPERZAPPER"
     msgs_es: .word   d17f ; "JUEGO TERMINADO"
        d0e7: .word   d1a8 ; "JUGADOR "
        d0e9: .word   d1a8 ; "JUGADOR "
        d0eb: .word   d1de ; "PULSAR START"
        d0ed: .word   d1fc ; "JUEGUE"
        d0ef: .word   d24d ; "ENTRE SUS INICIALES"
        d0f1: .word   d2ae ; "GIRE LA PERILLA PARA CAMBIAR"
        d0f3: .word   d316 ; "OPRIMA FIRE PARA SELECCIONAR"
        d0f5: .word   d35e ; "RECORDS"
        d0f7: .word   d3a0 ; "RANKING DE 1 A "
        d0f9: .word   d3da ; "CALIFIQUESE"
        d0fb: .word   d3ed ; "NOVICIO"
        d0fd: .word   d406 ; "EXPERTO"
        d0ff: .word   d417 ; "BONUS"
        d101: .word   d42d ; "TIEMPO"
        d103: .word   d446 ; "NIVEL"
        d105: .word   d456 ; "HOYO"
        d107: .word   d492 ; "INSERTE FICHAS"
        d109: .word   d4a1 ; "FREE PLAY"
        d10b: .word   d4dd ; "1 MONEDA 2 JUEGOS"
        d10d: .word   d51f ; "1 MONEDA 1 JUEGO"
        d10f: .word   d563 ; "2 MONEDAS 1 JUEGO"
        d111: .word   d575 ; "(c) MCMLXXX ATARI"
        d113: .word   d597 ; "CREDITOS "
        d115: .word   d5a1 ; "BONUS "
        d117: .word   d5d9 ; "2 JUEGOS MINIMO"
        d119: .word   d610 ; "BONUS CADA "
        d11b: .word   d651 ; "EVITE LAS PUNTAS"
        d11d: .word   d674 ; "NIVEL"
        d11f: .word   d6a1 ; "NUEVO SUPERZAPPER"
; Y-coordinates, colours, and sizes of messages.  X-coordinates vary with
; string length and thus with language; they are therefore stored with the
; (language-specific) string contents.
; Each message has two bytes here.  The first contains the message's
; colour in its high nibble, with its size (b value) in its low nibble.
; The second byte is the Y coordinate (signed).
; See the code at ab14 for more.
        d121: .byte   51
        d122: .byte   56 ; 86 (GAME OVER)
        d123: .byte   00
        d124: .byte   1a ; 26 (PLAYER )
        d125: .byte   01
        d126: .byte   20 ; 32 (PLAYER )
        d127: .byte   31
        d128: .byte   56 ; 86 (PRESS START)
        d129: .byte   01
        d12a: .byte   38 ; 56 (PLAY)
        d12b: .byte   31
        d12c: .byte   b0 ; -80 (ENTER YOUR INITIALS)
        d12d: .byte   41
        d12e: .byte   00 ; 0 (SPIN KNOB TO CHANGE)
        d12f: .byte   11
        d130: .byte   f6 ; -10 (PRESS FIRE TO SELECT)
        d131: .byte   30
        d132: .byte   38 ; 56 (HIGH SCORES)
        d133: .byte   31
        d134: .byte   ce ; -50 (RANKING FROM 1 TO )
        d135: .byte   51
        d136: .byte   0a ; 10 (RATE YOURSELF)
        d137: .byte   31
        d138: .byte   e2 ; -30 (NOVICE)
        d139: .byte   31
        d13a: .byte   e2 ; -30 (EXPERT)
        d13b: .byte   51
        d13c: .byte   ba ; -70 (BONUS)
        d13d: .byte   51
        d13e: .byte   98 ; -104 (TIME)
        d13f: .byte   51
        d140: .byte   d8 ; -40 (LEVEL)
        d141: .byte   51
        d142: .byte   c9 ; -55 (HOLE)
        d143: .byte   31
        d144: .byte   56 ; 86 (INSERT COINS)
        d145: .byte   51
        d146: .byte   80 ; -128 (FREE PLAY)
        d147: .byte   51
        d148: .byte   80 ; -128 (1 COIN 2 PLAYS)
        d149: .byte   51
        d14a: .byte   80 ; -128 (1 COIN 1 PLAY)
        d14b: .byte   51
        d14c: .byte   80 ; -128 (2 COINS 1 PLAY)
        d14d: .byte   71
        d14e: .byte   92 ; -110 ((c) MCMLXXX ATARI)
        d14f: .byte   51
        d150: .byte   80 ; -128 (CREDITS)
        d151: .byte   31
        d152: .byte   b0 ; -80 (BONUS )
        d153: .byte   51
        d154: .byte   89 ; -119 (2 CREDIT MINIMUM)
        d155: .byte   41
        d156: .byte   89 ; -119 (BONUS EVERY )
        d157: .byte   00
        d158: .byte   00 ; 0 (AVOID SPIKES)
        d159: .byte   71
        d15a: .byte   5a ; 90 (LEVEL)
        d15b: .byte   71
        d15c: .byte   a0 ; -96 (SUPERZAPPER RECHARGE)
; Each string is preceded by the X-coordinate at which it should be drawn.
; Y-coordinates come from a table at $d122 (they don't vary with string
; length and hence don't have to be language-specific; X coordinates do,
; so they are attached to the language-specific strings).
; See the code at ab14 for more.
        d15d: .byte   e5 ; -27
        d15e: .chunk  9 ; "GAME OVER"
        d167: .byte   d9 ; -39
        d168: .chunk  13 ; "FIN DE PARTIE"
        d175: .byte   e5 ; -27
        d176: .chunk  9 ; "SPIELENDE"
        d17f: .byte   d3 ; -45
        d180: .chunk  15 ; "JUEGO TERMINADO"
        d18f: .byte   cd ; -51
        d190: .chunk  7 ; "PLAYER "
        d197: .byte   c6 ; -58 -- should this be -51 maybe?
        d198: .chunk  7 ; "JOUEUR "
        d19f: .byte   c6 ; -58
        d1a0: .chunk  8 ; "SPIELER "
        d1a8: .byte   c6 ; -58
        d1a9: .chunk  8 ; "JUGADOR "
        d1b1: .byte   df ; -33
        d1b2: .chunk  11 ; "PRESS START"
        d1bd: .byte   cd ; -51
        d1be: .chunk  17 ; "APPUYEZ SUR START"
        d1cf: .byte   d6 ; -47
        d1d0: .chunk  14 ; "START DRUECKEN"
        d1de: .byte   dc ; -36
        d1df: .chunk  12 ; "PULSAR START"
        d1eb: .byte   f4 ; -12
        d1ec: .chunk  4 ; "PLAY"
        d1f0: .byte   f1 ; -15
        d1f1: .chunk  5 ; "JOUEZ"
        d1f6: .byte   f1 ; -15
        d1f7: .chunk  5 ; "SPIEL"
        d1fc: .byte   ee ; -18
        d1fd: .chunk  6 ; "JUEGUE"
        d203: .byte   c7 ; -57
        d204: .chunk  19 ; "ENTER YOUR INITIALS"
        d217: .byte   b8 ; -72
        d218: .chunk  24 ; "SVP ENTREZ VOS INITIALES"
        d230: .byte   ac ; -84
        d231: .chunk  28 ; "GEBEN SIE IHRE INITIALEN EIN"
        d24d: .byte   c7 ; -57
        d24e: .chunk  19 ; "ENTRE SUS INICIALES"
        d261: .byte   c7 ; -57
        d262: .chunk  19 ; "SPIN KNOB TO CHANGE"
        d275: .byte   a6 ; -90
        d276: .chunk  30 ; "TOURNEZ LE BOUTON POUR CHANGER"
        d294: .byte   b5 ; -75
        d295: .chunk  25 ; "KNOPF DREHEN ZUM WECHSELN"
        d2ae: .byte   ac ; -84
        d2af: .chunk  28 ; "GIRE LA PERILLA PARA CAMBIAR"
        d2cb: .byte   c4 ; -60
        d2cc: .chunk  20 ; "PRESS FIRE TO SELECT"
        d2e0: .byte   b2 ; -78
        d2e1: .chunk  26 ; "POUSSEZ FEU QUAND CORRECTE"
        d2fb: .byte   b2 ; -78
        d2fc: .chunk  26 ; "FIRE DRUECKEN WENN RICHTIG"
        d316: .byte   ac ; -84
        d317: .chunk  28 ; "OPRIMA FIRE PARA SELECCIONAR"
        d333: .byte   bc ; -68
        d334: .chunk  11 ; "HIGH SCORES"
        d33f: .byte   9e ; -98
        d340: .chunk  16 ; "MEILLEURS SCORES"
        d350: .byte   b0 ; -80
        d351: .chunk  13 ; "HOECHSTZAHLEN"
        d35e: .byte   d4 ; -44
        d35f: .chunk  7 ; "RECORDS"
        d366: .byte   c2 ; -62
        d367: .chunk  18 ; "RANKING FROM 1 TO "
        d379: .byte   c2 ; -62
        d37a: .chunk  17 ; "PLACEMENT DE 1 A "
        d38b: .byte   bc ; -68
        d38c: .chunk  20 ; "RANGLISTE VON 1 ZUM "
        d3a0: .byte   c8 ; -56
        d3a1: .chunk  15 ; "RANKING DE 1 A "
        d3b0: .byte   d9 ; -39
        d3b1: .chunk  13 ; "RATE YOURSELF"
        d3be: .byte   dc ; -36
        d3bf: .chunk  12 ; "EVALUEZ-VOUS"
        d3cb: .byte   d6 ; -42
        d3cc: .chunk  14 ; "SELBST RECHNEN"
        d3da: .byte   df ; -33
        d3db: .chunk  11 ; "CALIFIQUESE"
        d3e6: .byte   aa ; -86
        d3e7: .chunk  6 ; "NOVICE"
        d3ed: .byte   aa ; -86
        d3ee: .chunk  7 ; "NOVICIO"
        d3f5: .byte   aa ; -86
        d3f6: .chunk  9 ; "ANFAENGER"
        d3ff: .byte   4a ; 74
        d400: .chunk  6 ; "EXPERT"
        d406: .byte   45 ; 69
        d407: .chunk  7 ; "EXPERTO"
        d40e: .byte   40 ; 64
        d40f: .chunk  8 ; "ERFAHREN"
        d417: .byte   8b ; -117
        d418: .chunk  5 ; "BONUS"
        d41d: .byte   e8 ; -24
        d41e: .chunk  4 ; "TIME"
        d422: .byte   e0 ; -32
        d423: .chunk  5 ; "DUREE"
        d428: .byte   e8 ; -24
        d429: .chunk  4 ; "ZEIT"
        d42d: .byte   e4 ; -28
        d42e: .chunk  6 ; "TIEMPO"
        d434: .byte   8b ; -117
        d435: .chunk  5 ; "LEVEL"
        d43a: .byte   8b ; -117
        d43b: .chunk  6 ; "NIVEAU"
        d441: .byte   8b ; -117
        d442: .chunk  4 ; "GRAD"
        d446: .byte   8b ; -117
        d447: .chunk  5 ; "NIVEL"
        d44c: .byte   8b ; -117
        d44d: .chunk  4 ; "HOLE"
        d451: .byte   8b ; -117
        d452: .chunk  4 ; "TROU"
        d456: .byte   8b ; -117
        d457: .chunk  4 ; "HOYO"
        d45b: .byte   8b ; -117
        d45c: .chunk  4 ; "LOCH"
        d460: .byte   dc ; -36
        d461: .chunk  12 ; "INSERT COINS"
        d46d: .byte   c1 ; -63
        d46e: .chunk  21 ; "INTRODUIRE LES PIECES"
        d483: .byte   d6 ; -42
        d484: .chunk  14 ; "GELD EINWERFEN"
        d492: .byte   d6 ; -42
        d493: .chunk  14 ; "INSERTE FICHAS"
        d4a1: .byte   00 ; 0
        d4a2: .chunk  9 ; "FREE PLAY"
        d4ab: .byte   0e ; 14
        d4ac: .chunk  14 ; "1 COIN 2 PLAYS"
        d4ba: .byte   fa ; -6
        d4bb: .chunk  17 ; "1 PIECE 2 JOUEURS"
        d4cc: .byte   00 ; 0
        d4cd: .chunk  16 ; "1 MUENZ 2 SPIELE"
        d4dd: .byte   fa ; -6
        d4de: .chunk  17 ; "1 MONEDA 2 JUEGOS"
        d4ef: .byte   14 ; 20
        d4f0: .chunk  13 ; "1 COIN 1 PLAY"
        d4fd: .byte   00 ; 0
        d4fe: .chunk  16 ; "1 PIECE 1 JOUEUR"
        d50e: .byte   00 ; 0
        d50f: .chunk  16 ; "1 MUENZE 1 SPIEL"
        d51f: .byte   00 ; 0
        d520: .chunk  16 ; "1 MONEDA 1 JUEGO"
        d530: .byte   0e ; 14
        d531: .chunk  14 ; "2 COINS 1 PLAY"
        d53f: .byte   fa ; -6
        d540: .chunk  17 ; "2 PIECES 1 JOUEUR"
        d551: .byte   fa ; -6
        d552: .chunk  17 ; "2 MUENZEN 1 SPIEL"
        d563: .byte   fa ; -6
        d564: .chunk  17 ; "2 MONEDAS 1 JUEGO"
        d575: .byte   d3 ; -45
        d576: .chunk  15 ; "(c) MCMLXXX ATARI"
        d585: .byte   a0 ; -96
        d586: .chunk  8 ; "CREDITS "
        d58e: .byte   a0 ; -96
        d58f: .chunk  8 ; "KREDITE "
        d597: .byte   a0 ; -96
        d598: .chunk  9 ; "CREDITOS "
        d5a1: .byte   da ; -38
        d5a2: .chunk  6 ; "BONUS "
        d5a8: .byte   d0 ; -48
        d5a9: .chunk  16 ; "2 CREDIT MINIMUM"
        d5b9: .byte   d6 ; -42
        d5ba: .chunk  14 ; "2 JEUX MINIMUM"
        d5c8: .byte   d0 ; -48
        d5c9: .chunk  16 ; "2 SPIELE MINIMUM"
        d5d9: .byte   d3 ; -45
        d5da: .chunk  15 ; "2 JUEGOS MINIMO"
        d5e9: .byte   c8 ; -56
        d5ea: .chunk  12 ; "BONUS EVERY "
        d5f6: .byte   ce ; -61
        d5f7: .chunk  13 ; "BONUS CHAQUE "
        d604: .byte   ce ; -61
        d605: .chunk  11 ; "BONUS JEDE "
        d610: .byte   c8 ; -56
        d611: .chunk  11 ; "BONUS CADA "
        d61c: .byte   b8 ; -72
        d61d: .chunk  12 ; "AVOID SPIKES"
        d629: .byte   88 ; -120
        d62a: .chunk  20 ; "ATTENTION AUX LANCES"
        d63e: .byte   96 ; -106
        d63f: .chunk  18 ; "SPITZEN AUSWEICHEN"
        d651: .byte   a0 ; -96
        d652: .chunk  16 ; "EVITE LAS PUNTAS"
        d662: .byte   e0 ; -32
        d663: .chunk  5 ; "LEVEL"
        d668: .byte   da ; -38
        d669: .chunk  6 ; "NIVEAU"
        d66f: .byte   e2 ; -30
        d670: .chunk  4 ; "GRAD"
        d674: .byte   e0 ; -32
        d675: .chunk  5 ; "NIVEL"
        d67a: .byte   c4 ; -60
        d67b: .chunk  20 ; "SUPERZAPPER RECHARGE"
        d68f: .byte   cd ; -51
        d690: .chunk  17 ; "NEUER SUPERZAPPER"
        d6a1: .byte   cd ; -51
        d6a2: .chunk  17 ; "NUEVO SUPERZAPPER"
language_base_tbl: .word   d031 ; msgs_en
        d6b5: .word   d06d ; msgs_fr
        d6b7: .word   d0a9 ; msgs_de
        d6b9: .word   d0e5 ; msgs_es
; Updates optsw2_shadow, coinage_shadow, bonus_life_each, init_lives, and
; diff_bits, from the hardware.
 read_optsws: ad 00 0e  lda     optsw2
        d6be: 85 0a     sta     optsw2_shadow
        d6c0: 29 38     and     #$38 ; bonus life setting
        d6c2: 4a        lsr     a
        d6c3: 4a        lsr     a
        d6c4: 4a        lsr     a
        d6c5: aa        tax
        d6c6: bd f7 d6  lda     bonus_pts_tbl,x
        d6c9: 8d 56 01  sta     bonus_life_each
        d6cc: ad 00 0d  lda     optsw1
        d6cf: 49 02     eor     #$02 ; one of the coinage bits
        d6d1: 85 09     sta     coinage_shadow
        d6d3: a5 0a     lda     optsw2_shadow
        d6d5: 2a        rol     a
        d6d6: 2a        rol     a
        d6d7: 2a        rol     a
        d6d8: 29 03     and     #$03 ; lives
        d6da: aa        tax
        d6db: bd ff d6  lda     init_lives_tbl,x
        d6de: 8d 58 01  sta     init_lives
        d6e1: a5 0a     lda     optsw2_shadow
        d6e3: 29 06     and     #$06 ; language
        d6e5: a8        tay
        d6e6: b9 b3 d6  lda     language_base_tbl,y
        d6e9: 85 ac     sta     strtbl
        d6eb: b9 b4 d6  lda     language_base_tbl+1,y
        d6ee: 85 ad     sta     strtbl+1
        d6f0: 20 e0 db  jsr     get_diff_bits
        d6f3: 8d 6a 01  sta     diff_bits
        d6f6: 60        rts
; Table mapping bonus life setting values to tens of thousands of points
bonus_pts_tbl: .chunk  8 ; 2 1 3 4 5 6 7 0
; Table mapping initial lives setting values to initial lives
init_lives_tbl: .chunk  4 ; 3 4 5 2
        d703: .byte   7c
 nmi_irq_brk: 48        pha
        d705: 8a        txa
        d706: 48        pha
        d707: 98        tya
        d708: 48        pha
        d709: d8        cld
        d70a: ba        tsx
        d70b: e0 d0     cpx     #$d0
        d70d: 90 04     bcc     $d713
        d70f: a5 53     lda     $53
        d711: 10 04     bpl     $d717
        d713: 00        brk
        d714: 4c 3f d9  jmp     reset
        d717: 8d 00 50  sta     watchdog
        d71a: 8d cb 60  sta     $60cb ; pokey 1 potgo
        d71d: ad c8 60  lda     spinner_cabtyp
        d720: 49 0f     eor     #$0f
        d722: a8        tay
        d723: 29 10     and     #$10 ; upright/cocktail bit
        d725: 8d 17 01  sta     flagbits
        d728: 98        tya
        d729: 38        sec
        d72a: e5 52     sbc     $52
        d72c: 29 0f     and     #$0f
        d72e: c9 08     cmp     #$08
        d730: 90 02     bcc     $d734
        d732: 09 f0     ora     #$f0
        d734: 18        clc
        d735: 65 50     adc     $50
        d737: 85 50     sta     $50
        d739: 84 52     sty     $52
        d73b: 8d db 60  sta     $60db ; pokey 2 potgo
        d73e: ac d8 60  ldy     zap_fire_starts
        d741: ad 00 0c  lda     cabsw
        d744: 85 08     sta     zap_fire_shadow
        d746: a5 4c     lda     zap_fire_tmp1
        d748: 84 4c     sty     zap_fire_tmp1
        d74a: a8        tay
        d74b: 25 4c     and     zap_fire_tmp1
        d74d: 05 4d     ora     zap_fire_debounce
        d74f: 85 4d     sta     zap_fire_debounce
        d751: 98        tya
        d752: 05 4c     ora     zap_fire_tmp1
        d754: 25 4d     and     zap_fire_debounce
        d756: 85 4d     sta     zap_fire_debounce
        d758: a8        tay
        d759: 45 4f     eor     zap_fire_tmp2
        d75b: 25 4d     and     zap_fire_debounce
        d75d: 05 4e     ora     zap_fire_new
        d75f: 85 4e     sta     zap_fire_new
        d761: 84 4f     sty     zap_fire_tmp2
        d763: a5 b4     lda     $b4
        d765: a4 13     ldy     $13
        d767: 10 02     bpl     $d76b
        d769: 09 04     ora     #$04
        d76b: a4 14     ldy     $14
        d76d: 10 02     bpl     $d771
        d76f: 09 02     ora     #$02
        d771: a4 15     ldy     $15
        d773: 10 02     bpl     $d777
        d775: 09 01     ora     #$01
        d777: 8d 00 40  sta     vid_coins
        d77a: a6 3e     ldx     twoplayer
        d77c: e8        inx
        d77d: a4 05     ldy     $05
        d77f: d0 10     bne     $d791
        d781: a2 00     ldx     #$00
        d783: a4 07     ldy     $07
        d785: c0 40     cpy     #$40
        d787: 90 08     bcc     $d791
        d789: a6 06     ldx     credits
        d78b: e0 02     cpx     #$02
        d78d: 90 02     bcc     $d791
        d78f: a2 03     ldx     #$03
        d791: bd dd d7  lda     $d7dd,x
        d794: 45 a1     eor     $a1
        d796: 29 03     and     #$03
        d798: 45 a1     eor     $a1
        d79a: 85 a1     sta     $a1
        d79c: 8d e0 60  sta     leds_flip
        d79f: 20 24 cf  jsr     $cf24
        d7a2: 20 0a cd  jsr     $cd0a
        d7a5: e6 53     inc     $53
        d7a7: e6 07     inc     $07
        d7a9: d0 1e     bne     $d7c9
        d7ab: ee 06 04  inc     on_time_l
        d7ae: d0 08     bne     $d7b8
        d7b0: ee 07 04  inc     on_time_m
        d7b3: d0 03     bne     $d7b8
        d7b5: ee 08 04  inc     on_time_h
        d7b8: 24 05     bit     $05
        d7ba: 50 0d     bvc     $d7c9
        d7bc: ee 09 04  inc     play_time_l
        d7bf: d0 08     bne     $d7c9
        d7c1: ee 0a 04  inc     play_time_m
        d7c4: d0 03     bne     $d7c9
        d7c6: ee 0b 04  inc     play_time_h
        d7c9: 2c 00 0c  bit     cabsw
        d7cc: 50 09     bvc     $d7d7
        d7ce: ee 33 01  inc     $0133
        d7d1: 8d 00 58  sta     vg_reset
        d7d4: 8d 00 48  sta     vg_go
        d7d7: 68        pla
        d7d8: a8        tay
        d7d9: 68        pla
        d7da: aa        tax
        d7db: 68        pla
        d7dc: 40        rti
        d7dd: .byte   ff
        d7de: .byte   fd
        d7df: .byte   fe
        d7e0: .byte   fc
; Non-selftest service mode
    state_22: a9 00     lda     #$00
        d7e3: 85 05     sta     $05
        d7e5: a9 02     lda     #$02
        d7e7: 85 01     sta     $01
        d7e9: ad ca 01  lda     earom_op
        d7ec: d0 15     bne     $d803
        d7ee: ad 00 0c  lda     cabsw
        d7f1: 29 10     and     #$10 ; service switch
        d7f3: f0 0e     beq     $d803
        d7f5: a9 00     lda     #$00
        d7f7: 85 00     sta     gamestate
        d7f9: ad c9 01  lda     hs_initflag
        d7fc: 29 03     and     #$03
        d7fe: f0 03     beq     $d803
        d800: 20 ac ab  jsr     init_hs
        d803: 60        rts
        d804: 20 bb d6  jsr     read_optsws
        d807: 20 a8 aa  jsr     show_coin_stuff
        d80a: 20 0d dd  jsr     vapp_test_i3
        d80d: 20 41 dd  jsr     vapp_stats
        d810: ad 58 01  lda     init_lives
        d813: 85 37     sta     $37
        d815: 20 53 df  jsr     vapp_vcentre_2
        d818: a9 e8     lda     #$e8 ; -24
        d81a: a2 c0     ldx     #$c0 ; -64
        d81c: 20 75 df  jsr     vapp_ldraw_A,X
        d81f: a9 32     lda     #$32 ; $326c = player nominal picture
        d821: a2 6c     ldx     #$6c
        d823: 20 39 df  jsr     vapp_vjsr_AX
        d826: c6 37     dec     $37
        d828: d0 f5     bne     $d81f
        d82a: ad 6a 01  lda     diff_bits
        d82d: 29 03     and     #$03 ; difficulty
        d82f: 0a        asl     a
        d830: a8        tay
        d831: b9 1f 3f  lda     diff_str_tbl+1,y
        d834: be 1e 3f  ldx     diff_str_tbl,y
        d837: 20 39 df  jsr     vapp_vjsr_AX
        d83a: ad 00 02  lda     player_seg
        d83d: 20 ce ad  jsr     track_spinner
        d840: 8d 00 02  sta     player_seg
        d843: 29 06     and     #$06
        d845: 48        pha
        d846: a8        tay
        d847: b9 17 3f  lda     test_magic_tbl+1,y
        d84a: be 16 3f  ldx     test_magic_tbl,y
        d84d: 20 39 df  jsr     vapp_vjsr_AX
        d850: 68        pla
        d851: 4a        lsr     a
        d852: aa        tax
        d853: a5 4d     lda     zap_fire_debounce
        d855: 3d b6 d8  and     test_magic_bits,x
        d858: dd b6 d8  cmp     test_magic_bits,x
        d85b: d0 1a     bne     $d877
        d85d: ca        dex
        d85e: ca        dex
        d85f: 10 03     bpl     $d864
; 0, 1: fire&zap -> reset (enter selftest, since test switch is on)
        d861: 4c 3f d9  jmp     reset
        d864: d0 06     bne     $d86c
; 2: fire&start1 -> zero times
        d866: 20 e9 dd  jsr     zero_times
        d869: b8        clv
        d86a: 50 0b     bvc     $d877
; 3: fire&start2 -> zero scores
        d86c: 20 ed dd  jsr     zero_scores
        d86f: ad c9 01  lda     hs_initflag
        d872: 09 03     ora     #$03
        d874: 8d c9 01  sta     hs_initflag
; Common code, after magic button sequence handling done
        d877: ad ca 01  lda     earom_op
        d87a: 2d c6 01  and     earom_clr
        d87d: f0 07     beq     $d886
; 346e = draw ERASING
        d87f: a9 34     lda     #$34
        d881: a2 6e     ldx     #$6e
        d883: 20 39 df  jsr     vapp_vjsr_AX
        d886: 20 53 df  jsr     vapp_vcentre_2
        d889: a5 09     lda     coinage_shadow
        d88b: 29 1c     and     #$1c ; coin-slot multiplier bits
        d88d: 4a        lsr     a
        d88e: 4a        lsr     a
        d88f: aa        tax
        d890: bd ba d8  lda     $d8ba,x
        d893: a0 ee     ldy     #$ee ; -18
        d895: a2 1b     ldx     #$1b ; 27
        d897: 20 a9 d8  jsr     vapp_ldraw_Y,X_2dig_A
        d89a: a5 09     lda     coinage_shadow
        d89c: 4a        lsr     a ; extract bonus-coins bits
        d89d: 4a        lsr     a
        d89e: 4a        lsr     a
        d89f: 4a        lsr     a
        d8a0: 4a        lsr     a
        d8a1: aa        tax
        d8a2: bd c2 d8  lda     $d8c2,x
        d8a5: a0 32     ldy     #$32 ; x offset 50
        d8a7: a2 f8     ldx     #$f8 ; y offset -8
; Append an ldraw per Y,X, then append A as a two-digit hex number.
vapp_ldraw_Y,X_2dig_A: 85 29     sta     $29
        d8ab: 98        tya
        d8ac: 20 75 df  jsr     vapp_ldraw_A,X
        d8af: a9 29     lda     #$29
        d8b1: a0 01     ldy     #$01
        d8b3: 4c b1 df  jmp     vapp_multdig_y@a
; Magic button combinations for when test-mode switch is turned on live.
; 08 = zap, 10 = fire, 20 = start 1, 40 = start 2
test_magic_bits: .byte   18 ; fire&zap: selftest
        d8b7: .byte   18 ; fire&zap: selftest
        d8b8: .byte   30 ; fire&start1: zero times
        d8b9: .byte   50 ; fire&start2: zero scores
; Coin-slot multiplier display values, two-digit BCD.  Indexed by the
; coin-slot multiplier bits in coinage_shadow.  These do not actually
; affect the multipliers used; they are used only for test mode display.
        d8ba: .byte   11
        d8bb: .byte   14
        d8bc: .byte   15
        d8bd: .byte   16
        d8be: .byte   21
        d8bf: .byte   24
        d8c0: .byte   25
        d8c1: .byte   26
; "BONUS ADDER" values - extra credits for multiple coins.  Indexed by
; the bonus-coin bits in coinage_shadow.  These do not actually affect
; bonus coins awarded; they are used only for test mode display.
        d8c2: .byte   00
        d8c3: .byte   12
        d8c4: .byte   14
        d8c5: .byte   24
        d8c6: .byte   15
        d8c7: .byte   13
        d8c8: .byte   00
        d8c9: .byte   00
; Selftest of low RAM failed.
        d8ca: a8        tay
        d8cb: a9 00     lda     #$00
        d8cd: 84 79     sty     $79
        d8cf: 4a        lsr     a
        d8d0: 4a        lsr     a
        d8d1: 0a        asl     a
        d8d2: aa        tax
        d8d3: 98        tya
        d8d4: 29 0f     and     #$0f
        d8d6: d0 01     bne     $d8d9
        d8d8: e8        inx
        d8d9: 9a        txs
        d8da: a9 a2     lda     #$a2
        d8dc: 8d c1 60  sta     $60c1
        d8df: ba        tsx
        d8e0: d0 07     bne     $d8e9
        d8e2: a9 60     lda     #$60
        d8e4: a0 09     ldy     #$09
        d8e6: b8        clv
        d8e7: 50 04     bvc     $d8ed
        d8e9: a9 c0     lda     #$c0
        d8eb: a0 01     ldy     #$01
        d8ed: 8d c0 60  sta     pokey1
        d8f0: a9 03     lda     #$03
        d8f2: 8d e0 60  sta     leds_flip
        d8f5: a2 00     ldx     #$00
        d8f7: 2c 00 0c  bit     cabsw
        d8fa: 30 fb     bmi     $d8f7
        d8fc: 2c 00 0c  bit     cabsw
        d8ff: 10 fb     bpl     $d8fc
        d901: 8d 00 50  sta     watchdog
        d904: ca        dex
        d905: d0 f0     bne     $d8f7
        d907: 88        dey
        d908: d0 ed     bne     $d8f7
        d90a: 8e c1 60  stx     $60c1
        d90d: a9 00     lda     #$00
        d90f: 8d e0 60  sta     leds_flip
        d912: a0 09     ldy     #$09
        d914: 2c 00 0c  bit     cabsw
        d917: 30 fb     bmi     $d914
        d919: 2c 00 0c  bit     cabsw
        d91c: 10 fb     bpl     $d919
        d91e: 8d 00 50  sta     watchdog
        d921: ca        dex
        d922: d0 f0     bne     $d914
        d924: 88        dey
        d925: d0 ed     bne     $d914
        d927: ba        tsx
        d928: ca        dex
        d929: 9a        txs
        d92a: 10 ae     bpl     $d8da
        d92c: 4c 0a da  jmp     $da0a
        d92f: 51 00     eor     (gamestate),y
        d931: a8        tay
        d932: a5 01     lda     $01
        d934: c9 20     cmp     #$20
        d936: 90 02     bcc     $d93a
        d938: e9 18     sbc     #$18
        d93a: 29 1f     and     #$1f
        d93c: 4c cd d8  jmp     $d8cd
       reset: 78        sei
        d940: 8d 00 50  sta     watchdog
        d943: 8d 00 58  sta     vg_reset
; clear all RAM: 0000-07ff (game RAM) and 2000-2fff (vector RAM)
        d946: a2 ff     ldx     #$ff
        d948: 9a        txs
        d949: d8        cld
        d94a: e8        inx
        d94b: 8a        txa
        d94c: a8        tay
        d94d: 84 00     sty     gamestate
        d94f: 86 01     stx     $01
        d951: a0 00     ldy     #$00
        d953: 91 00     sta     (gamestate),y
        d955: c8        iny
        d956: d0 fb     bne     $d953
        d958: e8        inx
        d959: e0 08     cpx     #$08
        d95b: d0 02     bne     $d95f
        d95d: a2 20     ldx     #$20
        d95f: e0 30     cpx     #$30
        d961: 8d 00 50  sta     watchdog
        d964: 90 e7     bcc     $d94d
        d966: 85 01     sta     $01
        d968: 8d e0 60  sta     leds_flip
; init pokeys
        d96b: 8d cf 60  sta     $60cf
        d96e: 8d df 60  sta     $60df
        d971: a2 07     ldx     #$07
        d973: 8e cf 60  stx     $60cf
        d976: 8e df 60  stx     $60df
        d979: e8        inx
        d97a: 9d c0 60  sta     pokey1,x
        d97d: 9d d0 60  sta     pokey2,x
        d980: ca        dex
        d981: 10 f7     bpl     $d97a
        d983: ad 00 0c  lda     cabsw
        d986: 29 10     and     #$10 ; selftest switch
        d988: f0 1f     beq     $d9a9 ; branch if selftest
; reset in non-selftest mode
        d98a: 8d 00 50  sta     watchdog
        d98d: ce 00 01  dec     $0100
        d990: d0 f8     bne     $d98a
        d992: ce 01 01  dec     $0101
        d995: d0 f3     bne     $d98a
        d997: a9 10     lda     #$10
        d999: 85 b4     sta     $b4
        d99b: 20 11 de  jsr     $de11
        d99e: 20 ac ab  jsr     init_hs
        d9a1: 20 6e c1  jsr     $c16e
        d9a4: 58        cli
        d9a5: 4c a0 c7  jmp     $c7a0
        d9a8: .byte   a0
; reset in selftest mode
; Test low RAM: for each byte from $00 to $ff, store $11 in it, then store
; $00 in all other bytes and verify it's there, then check the $11 is
; undisturbed.  Repeat this with $22, $44, and $88 as well.  (There are
; some faults this won't catch, such as a bit getting cloned from its
; corresponding bit in the other nibble, but it's not a bad check.)
; If any of the checks fail, branch to $d8ca.
        d9a9: a2 11     ldx     #$11
        d9ab: 9a        txs
        d9ac: a0 00     ldy     #$00
        d9ae: ba        tsx
        d9af: 96 00     stx     gamestate,y
        d9b1: a2 01     ldx     #$01
        d9b3: c8        iny
        d9b4: b9 00 00  lda     gamestate,y
        d9b7: f0 03     beq     $d9bc
        d9b9: 4c ca d8  jmp     $d8ca
        d9bc: e8        inx
        d9bd: d0 f4     bne     $d9b3
        d9bf: ba        tsx
        d9c0: 8a        txa
        d9c1: 8d 00 50  sta     watchdog
        d9c4: c8        iny
        d9c5: 59 00 00  eor     gamestate,y
        d9c8: d0 ef     bne     $d9b9
        d9ca: 99 00 00  sta     gamestate,y
        d9cd: c8        iny
        d9ce: d0 de     bne     $d9ae
        d9d0: ba        tsx
        d9d1: 8a        txa
        d9d2: 0a        asl     a
        d9d3: aa        tax
        d9d4: 90 d5     bcc     $d9ab
; Low RAM selftest passed.
; Test remaning RAM: $0100-$07ff and $2000-$2fff.  For each byte, check
; that it's zero (which it should be, we cleared it above), then do a
; write-read-compare of $11, $22, $44, and $88 in it.  When done, store a
; $00 back in it.
        d9d6: a0 00     ldy     #$00
        d9d8: a2 01     ldx     #$01
        d9da: 84 00     sty     gamestate
        d9dc: 86 01     stx     $01
        d9de: a0 00     ldy     #$00
        d9e0: b1 00     lda     (gamestate),y
        d9e2: f0 03     beq     $d9e7
        d9e4: 4c 31 d9  jmp     $d931
        d9e7: a9 11     lda     #$11
        d9e9: 91 00     sta     (gamestate),y
        d9eb: d1 00     cmp     (gamestate),y
        d9ed: f0 03     beq     $d9f2
        d9ef: 4c 2f d9  jmp     $d92f
        d9f2: 0a        asl     a
        d9f3: 90 f4     bcc     $d9e9
        d9f5: a9 00     lda     #$00
        d9f7: 91 00     sta     (gamestate),y
        d9f9: c8        iny
        d9fa: d0 e4     bne     $d9e0
        d9fc: 8d 00 50  sta     watchdog
        d9ff: e8        inx
        da00: e0 08     cpx     #$08
        da02: d0 02     bne     $da06
        da04: a2 20     ldx     #$20
        da06: e0 30     cpx     #$30
        da08: 90 d0     bcc     $d9da
; Okay, all RAM passed selftest.
; Checksum ROM.  For each $0800 region, XOR all its bytes together and
; XOR in its region number (0 for the first, 1 for the second, etc), then
; store the result in the space at $7d.  Ranges for each of the 12 bytes:
; $7d - $3000-$37ff
; $7e - $3800-$38ff
; $7f - $9000-$97ff
; $80 - $9800-$9fff
; $81 - $a000-$a7ff
; $82 - $a800-$afff
; $83 - $b000-$b7ff
; $84 - $b800-$bfff
; $85 - $c000-$c7ff
; $86 - $c800-$cfff
; $87 - $d000-$d7ff
; $88 - $d800-$dfff
        da0a: a9 00     lda     #$00
        da0c: a8        tay
        da0d: aa        tax
        da0e: 85 3b     sta     $3b
        da10: a9 30     lda     #$30
        da12: 85 3c     sta     $3c
        da14: a9 08     lda     #$08
        da16: 85 38     sta     $38
        da18: 8a        txa
        da19: 51 3b     eor     ($3b),y
        da1b: c8        iny
        da1c: d0 fb     bne     $da19
        da1e: e6 3c     inc     $3c
        da20: 8d 00 50  sta     watchdog
        da23: c6 38     dec     $38
        da25: d0 f2     bne     $da19
        da27: 95 7d     sta     $7d,x
        da29: e8        inx
        da2a: e0 02     cpx     #$02
        da2c: d0 04     bne     $da32
        da2e: a9 90     lda     #$90
        da30: 85 3c     sta     $3c
        da32: e0 0c     cpx     #$0c
        da34: 90 de     bcc     $da14
; All checksums computed and stored in $7d-$88.
        da36: a5 7d     lda     $7d
        da38: f0 0a     beq     $da44
        da3a: a9 40     lda     #$40
        da3c: a2 a4     ldx     #$a4
        da3e: 8d c4 60  sta     $60c4
        da41: 8e c5 60  stx     $60c5
        da44: a2 05     ldx     #$05
        da46: ad ca 60  lda     pokey1_rand
        da49: cd ca 60  cmp     pokey1_rand
        da4c: d0 05     bne     $da53
        da4e: ca        dex
        da4f: 10 f8     bpl     $da49
        da51: 85 7a     sta     $7a
        da53: a2 05     ldx     #$05
        da55: ad da 60  lda     pokey2_rand
        da58: cd da 60  cmp     pokey2_rand
        da5b: d0 05     bne     $da62
        da5d: ca        dex
        da5e: 10 f8     bpl     $da58
        da60: 85 7b     sta     $7b
; I'm not sure what $de11 does, though I suspect it's an earom read.
; It appears to be loading stuff into the stats stored at $0406-$0411.
        da62: 20 11 de  jsr     $de11
        da65: a0 02     ldy     #$02
        da67: ad c9 01  lda     hs_initflag
        da6a: f0 0a     beq     $da76
        da6c: 85 7c     sta     $7c
        da6e: 20 f1 dd  jsr     $ddf1
        da71: a0 00     ldy     #$00
        da73: 8c c9 01  sty     hs_initflag
        da76: 84 00     sty     gamestate
; Load the colourmap used by the selftest screens.
        da78: a2 07     ldx     #$07
        da7a: bd f9 da  lda     $daf9,x
        da7d: 9d 00 08  sta     col_ram,x
        da80: ca        dex
        da81: 10 f7     bpl     $da7a
        da83: a9 00     lda     #$00
        da85: 8d e0 60  sta     leds_flip
        da88: a9 10     lda     #$10
        da8a: 8d 00 40  sta     vid_coins
; Top of selftest-mode main loop.
; Wait for the vector processor to be done.  Loop up to five times.
        da8d: a0 04     ldy     #$04
; Wait for 21 ($14+1) cycles of the 3KHz signal.
        da8f: a2 14     ldx     #$14
        da91: 2c 00 0c  bit     cabsw
        da94: 10 fb     bpl     $da91
        da96: 2c 00 0c  bit     cabsw
        da99: 30 fb     bmi     $da96
        da9b: ca        dex
        da9c: 10 f3     bpl     $da91
; Have we run out of iterations?  If so, break out.
        da9e: 88        dey
        da9f: 30 08     bmi     $daa9
; Poke the watchdog - don't want to get reset while waiting!
        daa1: 8d 00 50  sta     watchdog
; Is the vector processor done?
        daa4: 2c 00 0c  bit     cabsw
        daa7: 50 e6     bvc     $da8f ; tests vector processor halt bit
; Either the vector processor is done or we got tired of waiting.
        daa9: 8d 00 58  sta     vg_reset
        daac: a9 00     lda     #$00
        daae: 85 74     sta     vidptr_l
        dab0: a9 20     lda     #$20
        dab2: 85 75     sta     vidptr_h
        dab4: 8d cb 60  sta     $60cb
        dab7: ad c8 60  lda     spinner_cabtyp
        daba: 85 52     sta     $52
        dabc: 29 0f     and     #$0f
        dabe: 85 50     sta     $50
        dac0: ad 00 0c  lda     cabsw
        dac3: 49 ff     eor     #$ff
        dac5: 29 2f     and     #$2f ; keep diag step, slam, and coins
        dac7: 85 4e     sta     zap_fire_new
        dac9: 29 28     and     #$28 ; keep diag step and slam
        dacb: f0 0b     beq     $dad8
        dacd: 06 4c     asl     zap_fire_tmp1
        dacf: 90 04     bcc     $dad5
        dad1: e6 00     inc     gamestate
        dad3: e6 00     inc     gamestate
        dad5: b8        clv
        dad6: 50 04     bvc     $dadc
        dad8: a9 20     lda     #$20
        dada: 85 4c     sta     zap_fire_tmp1
        dadc: 20 0f db  jsr     draw_selftest_scr
        dadf: 20 0d df  jsr     vapp_centre_halt
        dae2: 8d 00 48  sta     vg_go
        dae5: e6 03     inc     timectr
        dae7: a5 03     lda     timectr
        dae9: 29 03     and     #$03
        daeb: d0 03     bne     $daf0
        daed: 20 1b de  jsr     $de1b
        daf0: ad 00 0c  lda     cabsw
        daf3: 29 10     and     #$10
        daf5: f0 96     beq     $da8d
; We depend on something else to break us out of this loop.  I suspect
; this "something" is the hardware watchdog.
        daf7: d0 fe     bne     $daf7
; Loaded to colour RAM; see $da7a
        daf9: .byte   00
        dafa: .byte   04
        dafb: .byte   08
        dafc: .byte   0c
        dafd: .byte   03
        dafe: .byte   07
        daff: .byte   0b
        db00: .byte   0b
; Jump table, used just below at $db19.
; These are the various selftest screens.
        db01: .jump   selftest_0
        db03: .jump   selftest_1
        db05: .jump   selftest_2
        db07: .jump   selftest_3
        db09: .jump   selftest_4
        db0b: .jump   selftest_5
        db0d: .jump   selftest_6
draw_selftest_scr: a6 00     ldx     gamestate
        db11: e0 0e     cpx     #$0e
        db13: 90 04     bcc     $db19
        db15: a2 02     ldx     #$02
        db17: 86 00     stx     gamestate
        db19: bd 02 db  lda     $db02,x
        db1c: 48        pha
        db1d: bd 01 db  lda     $db01,x
        db20: 48        pha
        db21: 60        rts
  selftest_6: a9 00     lda     #$00
        db24: 8d e0 60  sta     leds_flip
        db27: 8d 80 60  sta     mb_w_00
        db2a: 8d c0 60  sta     pokey1
        db2d: 8d d0 60  sta     pokey2
        db30: 8d 00 60  sta     earom_write
        db33: 8d 40 60  sta     eactl_mbst
        db36: ad 40 60  lda     eactl_mbst
        db39: ad 60 60  lda     mb_rd_l
        db3c: ad 70 60  lda     mb_rd_h
        db3f: ad 50 60  lda     earom_rd
        db42: a9 08     lda     #$08
        db44: 8d e0 60  sta     leds_flip
        db47: a9 01     lda     #$01
        db49: a2 1f     ldx     #$1f
        db4b: 18        clc
        db4c: 9d 80 60  sta     mb_w_00,x
        db4f: 2a        rol     a
        db50: ca        dex
        db51: 10 f9     bpl     $db4c
; $34a6 = draw box around screen
        db53: a9 34     lda     #$34
        db55: a2 a6     ldx     #$a6
        db57: 4c 39 df  jmp     vapp_vjsr_AX
; It's not clear to me this code is _ever_ executed...
  selftest_0: ad ca 01  lda     earom_op
        db5d: 0d c7 01  ora     $01c7
        db60: d0 0c     bne     $db6e
        db62: 20 11 de  jsr     $de11
        db65: ad c9 01  lda     hs_initflag
        db68: 85 7c     sta     $7c
        db6a: a9 02     lda     #$02
        db6c: 85 00     sta     gamestate
        db6e: 60        rts
  selftest_5: a5 50     lda     $50
        db71: 4a        lsr     a
        db72: a8        tay
        db73: a9 68     lda     #$68
        db75: 20 4c df  jsr     vapp_sclstat_A_Y
; $334e = rectangular grid selftest; this display is mostly ROMed
        db78: a2 4e     ldx     #$4e
        db7a: a9 33     lda     #$33
        db7c: d0 0a     bne     $db88
; $32b6 = coloured-lines selftest; this display is entirely ROMed
  selftest_4: a2 b6     ldx     #$b6
        db80: a9 32     lda     #$32
        db82: d0 04     bne     $db88
; $330a = draw selftest screen 2; this display is entirely ROMed
  selftest_2: a9 33     lda     #$33
        db86: a2 0a     ldx     #$0a
        db88: 20 39 df  jsr     vapp_vjsr_AX
        db8b: a2 06     ldx     #$06
        db8d: a9 00     lda     #$00
        db8f: 9d c1 60  sta     $60c1,x
        db92: 9d d1 60  sta     $60d1,x
        db95: ca        dex
        db96: ca        dex
        db97: 10 f6     bpl     $db8f
        db99: 60        rts
  selftest_3: a5 03     lda     timectr
        db9c: 29 3f     and     #$3f
        db9e: d0 02     bne     $dba2
        dba0: e6 39     inc     $39
        dba2: a5 39     lda     $39
        dba4: 29 07     and     #$07
        dba6: aa        tax
        dba7: bc d5 db  ldy     $dbd5,x
        dbaa: a9 00     lda     #$00
        dbac: 99 c1 60  sta     $60c1,y
        dbaf: bc d6 db  ldy     $dbd6,x
        dbb2: bd dc df  lda     $dfdc,x
        dbb5: 99 c0 60  sta     pokey1,y
        dbb8: a9 a8     lda     #$a8
        dbba: 99 c1 60  sta     $60c1,y
; $3456 = draw full-screen crosshair
        dbbd: a9 34     lda     #$34
        dbbf: a2 56     ldx     #$56
        dbc1: 20 39 df  jsr     vapp_vjsr_AX
        dbc4: a5 03     lda     timectr
        dbc6: 29 7f     and     #$7f
        dbc8: a8        tay
        dbc9: a9 01     lda     #$01
        dbcb: 20 6c df  jsr     vapp_scale_A,Y
; $34aa = draw box
        dbce: a9 34     lda     #$34
        dbd0: a2 aa     ldx     #$aa
        dbd2: 4c 39 df  jmp     vapp_vjsr_AX
; Selftest sound table of some sort - see $dba7
        dbd5: .byte   16
        dbd6: .byte   00
        dbd7: .byte   10
        dbd8: .byte   02
        dbd9: .byte   12
        dbda: .byte   04
        dbdb: .byte   14
        dbdc: .byte   06
        dbdd: .byte   16
        dbde: .byte   00
        dbdf: .byte   ea
; Returns value with difficulty/rating bits in $07, something unknown
; ($20 bit of spinner/cabinet select byte) in $08.
; Uses $37 as temporary storage.
get_diff_bits: 8d db 60  sta     $60db
        dbe3: ad d8 60  lda     zap_fire_starts
        dbe6: 29 07     and     #$07 ; difficulty/rating bits
        dbe8: 85 37     sta     $37
        dbea: 8d cb 60  sta     $60cb
        dbed: ad c8 60  lda     spinner_cabtyp
        dbf0: 29 20     and     #$20 ; Unknown
        dbf2: 4a        lsr     a
        dbf3: 4a        lsr     a
        dbf4: 05 37     ora     $37
        dbf6: 60        rts
; Selftest screen 1 ($00 holds $02)
  selftest_1: a5 2e     lda     $2e
        dbf9: f0 1e     beq     $dc19
        dbfb: 8d 95 60  sta     mb_w_15
        dbfe: 8d 8d 60  sta     mb_w_0d
        dc01: a5 2f     lda     $2f
        dc03: 8d 96 60  sta     mb_w_16
        dc06: a2 00     ldx     #$00
        dc08: 20 e6 dc  jsr     divide
        dc0b: c9 01     cmp     #$01
        dc0d: d0 06     bne     $dc15
        dc0f: 98        tya
        dc10: d0 03     bne     $dc15
        dc12: 8a        txa
        dc13: 10 04     bpl     $dc19
        dc15: a9 ff     lda     #$ff
        dc17: 85 78     sta     $78
        dc19: a2 00     ldx     #$00
        dc1b: 86 73     stx     draw_z
        dc1d: e6 2e     inc     $2e
        dc1f: d0 06     bne     $dc27
        dc21: e6 2f     inc     $2f
        dc23: 10 02     bpl     $dc27
        dc25: 86 2f     stx     $2f
        dc27: 8d db 60  sta     $60db
        dc2a: ad d8 60  lda     zap_fire_starts
        dc2d: 29 78     and     #$78 ; zap, fire, start1, start2
        dc2f: 85 4d     sta     zap_fire_debounce
        dc31: f0 05     beq     $dc38
        dc33: 8d c0 60  sta     pokey1
        dc36: a2 a4     ldx     #$a4
        dc38: 8e c1 60  stx     $60c1
        dc3b: a2 00     ldx     #$00
        dc3d: a5 4e     lda     zap_fire_new
        dc3f: f0 06     beq     $dc47
        dc41: 0a        asl     a
        dc42: 8d c2 60  sta     $60c2
        dc45: a2 a4     ldx     #$a4
        dc47: 8e c3 60  stx     $60c3
        dc4a: 20 0d dd  jsr     vapp_test_i3
        dc4d: a4 4d     ldy     zap_fire_debounce
        dc4f: a9 d0     lda     #$d0 ; -48
        dc51: a2 f0     ldx     #$f0 ; -16
        dc53: 20 2b dd  jsr     vapp_test_ibits
        dc56: a4 4e     ldy     zap_fire_new
        dc58: 20 27 dd  jsr     vapp_test_ibmove
        dc5b: a5 52     lda     $52
        dc5d: 29 10     and     #$10
        dc5f: f0 1d     beq     $dc7e
; $3482 = draw cocktail-bit C
        dc61: a9 34     lda     #$34
        dc63: a2 82     ldx     #$82
        dc65: 20 39 df  jsr     vapp_vjsr_AX
        dc68: a0 10     ldy     #$10
        dc6a: a5 4d     lda     zap_fire_debounce
        dc6c: 29 60     and     #$60 ; start1, start2
        dc6e: f0 0e     beq     $dc7e
        dc70: 49 20     eor     #$20
        dc72: f0 04     beq     $dc78
        dc74: a9 04     lda     #$04
        dc76: a0 08     ldy     #$08
        dc78: 8d e0 60  sta     leds_flip
        dc7b: 8c 00 40  sty     vid_coins
; $3492 = draw box around screen and line across the middle
        dc7e: a9 34     lda     #$34
        dc80: a2 92     ldx     #$92
        dc82: 20 39 df  jsr     vapp_vjsr_AX
; Show any nonzero checksums (stored in the 12 bytes from $7d to $88)
        dc85: a2 0b     ldx     #$0b
        dc87: b5 7d     lda     $7d,x
        dc89: f0 19     beq     $dca4
        dc8b: 85 35     sta     $35
        dc8d: 86 38     stx     $38
        dc8f: 8a        txa
        dc90: 20 1f df  jsr     vapp_digit
        dc93: a0 f4     ldy     #$f4 ; -12
        dc95: a2 f4     ldx     #$f4 ; -12
        dc97: a5 35     lda     $35
        dc99: 20 a9 d8  jsr     vapp_ldraw_Y,X_2dig_A
        dc9c: a9 0c     lda     #$0c ; 12, 12
        dc9e: aa        tax
        dc9f: 20 75 df  jsr     vapp_ldraw_A,X
        dca2: a6 38     ldx     $38
        dca4: ca        dex
        dca5: 10 e0     bpl     $dc87
        dca7: 20 53 df  jsr     vapp_vcentre_2
        dcaa: a9 00     lda     #$00
        dcac: a2 16     ldx     #$16
        dcae: 20 75 df  jsr     vapp_ldraw_A,X
; Show the 5 characters in $78-$7c
        dcb1: a2 04     ldx     #$04
        dcb3: 86 37     stx     $37
        dcb5: a6 37     ldx     $37
        dcb7: a0 00     ldy     #$00
        dcb9: b5 78     lda     $78,x
        dcbb: f0 03     beq     $dcc0
        dcbd: bc e1 dc  ldy     $dce1,x
        dcc0: b9 e4 31  lda     char_jsrtbl,y
        dcc3: be e5 31  ldx     char_jsrtbl+1,y
        dcc6: 20 57 df  jsr     vapp_A_X_Y=0
        dcc9: c6 37     dec     $37
        dccb: 10 e8     bpl     $dcb5
; Draw the spinner line
        dccd: a2 ac     ldx     #$ac
        dccf: a9 30     lda     #$30
        dcd1: 20 75 df  jsr     vapp_ldraw_A,X
        dcd4: a4 50     ldy     $50
        dcd6: b9 e8 df  lda     spinner_sine+4,y
        dcd9: be e4 df  ldx     spinner_sine,y
        dcdc: a0 c0     ldy     #$c0
        dcde: 4c 73 df  jmp     vapp_ldraw_A,X,Y
        dce1: 2e 38 34  rol     $3438
        dce4: 36 1e     rol     $1e,x
; This appears to be doing a divide, but I'm not clear enough on how the
; mathbox works to be certain of the details.
      divide: a0 00     ldy     #$00
        dce8: 84 73     sty     draw_z
        dcea: 8c 14 04  sty     secs_avg_h
        dced: 8d 8e 60  sta     mb_w_0e
        dcf0: 8e 8f 60  stx     mb_w_0f
        dcf3: 8c 90 60  sty     mb_w_10
        dcf6: a2 10     ldx     #$10
        dcf8: 8e 8c 60  stx     mb_w_0c
        dcfb: 8e 94 60  stx     mb_w_14
        dcfe: ca        dex
        dcff: 30 0b     bmi     $dd0c
        dd01: ad 40 60  lda     eactl_mbst
        dd04: 30 f8     bmi     $dcfe
        dd06: ad 60 60  lda     mb_rd_l
        dd09: ac 70 60  ldy     mb_rd_h
        dd0c: 60        rts
; Appends code to display the three lines of bits showing the configuration
; and input button values.
vapp_test_i3: 20 53 df  jsr     vapp_vcentre_2
        dd10: a9 00     lda     #$00
        dd12: 20 6a df  jsr     vapp_scale_A,0
        dd15: a9 e8     lda     #$e8 ; -24
        dd17: ac 00 0d  ldy     optsw1
        dd1a: 20 29 dd  jsr     $dd29
        dd1d: ac 00 0e  ldy     optsw2
        dd20: 20 27 dd  jsr     vapp_test_ibmove
        dd23: 20 e0 db  jsr     get_diff_bits
        dd26: a8        tay
vapp_test_ibmove: a9 d0     lda     #$d0 ; -48
        dd29: a2 f8     ldx     #$f8 ; -8
vapp_test_ibits: 84 35     sty     $35
        dd2d: 20 75 df  jsr     vapp_ldraw_A,X
        dd30: a2 07     ldx     #$07
        dd32: 86 37     stx     $37
        dd34: 06 35     asl     $35
        dd36: a9 00     lda     #$00
        dd38: 2a        rol     a
        dd39: 20 1f df  jsr     vapp_digit
        dd3c: c6 37     dec     $37
        dd3e: 10 f4     bpl     $dd34
        dd40: 60        rts
; Display game statistics.
  vapp_stats: ad 0f 04  lda     games_2p_l
        dd44: 0a        asl     a
        dd45: 85 29     sta     $29
        dd47: ad 10 04  lda     games_2p_m
        dd4a: 2a        rol     a
        dd4b: 85 2a     sta     $2a
        dd4d: ad 0c 04  lda     games_1p_l
        dd50: 18        clc
        dd51: 65 29     adc     $29
        dd53: 8d 95 60  sta     mb_w_15
        dd56: 85 29     sta     $29
        dd58: ad 0d 04  lda     games_1p_m
        dd5b: 65 2a     adc     $2a
        dd5d: 8d 96 60  sta     mb_w_16
        dd60: 05 29     ora     $29
        dd62: d0 05     bne     $dd69
        dd64: a9 01     lda     #$01
        dd66: 8d 95 60  sta     mb_w_15
        dd69: ad 09 04  lda     play_time_l
        dd6c: 8d 8d 60  sta     mb_w_0d
        dd6f: ad 0a 04  lda     play_time_m
        dd72: ae 0b 04  ldx     play_time_h
        dd75: 20 e6 dc  jsr     divide
        dd78: 8d 12 04  sta     secs_avg_l
        dd7b: 8c 13 04  sty     secs_avg_m
; 3dce = draw the "SECONDS ON", "SECONDS PLAYED", etc, labels
        dd7e: a9 3d     lda     #$3d
        dd80: a2 ce     ldx     #$ce
        dd82: 20 39 df  jsr     vapp_vjsr_AX
        dd85: a9 06     lda     #$06
        dd87: 85 3b     sta     $3b
        dd89: a9 04     lda     #$04
        dd8b: 85 3c     sta     $3c
        dd8d: 85 37     sta     $37
        dd8f: a0 00     ldy     #$00
        dd91: 84 31     sty     $31
        dd93: 84 32     sty     $32
        dd95: 84 33     sty     $33
        dd97: 84 34     sty     $34
        dd99: b1 3b     lda     ($3b),y
        dd9b: 85 56     sta     $56
        dd9d: e6 3b     inc     $3b
        dd9f: b1 3b     lda     ($3b),y
        dda1: 85 57     sta     $57
        dda3: e6 3b     inc     $3b
        dda5: b1 3b     lda     ($3b),y
        dda7: 85 58     sta     $58
        dda9: e6 3b     inc     $3b
; From here to the cld at ddc8, code converts a 24-bit number stored in
; $56/$57/$58 into six-nibble BCD, stored in $31/$32/$33.  Only the low
; six digits are retained.
        ddab: f8        sed
        ddac: a0 17     ldy     #$17
        ddae: 84 38     sty     $38
        ddb0: 26 56     rol     $56
        ddb2: 26 57     rol     $57
        ddb4: 26 58     rol     $58
        ddb6: a0 03     ldy     #$03
        ddb8: a2 00     ldx     #$00
        ddba: b5 31     lda     $31,x
        ddbc: 75 31     adc     $31,x
        ddbe: 95 31     sta     $31,x
        ddc0: e8        inx
        ddc1: 88        dey
        ddc2: 10 f6     bpl     $ddba
        ddc4: c6 38     dec     $38
        ddc6: 10 e8     bpl     $ddb0
        ddc8: d8        cld
        ddc9: a9 31     lda     #$31
        ddcb: a0 04     ldy     #$04
        ddcd: 20 b1 df  jsr     vapp_multdig_y@a
        ddd0: a9 d0     lda     #$d0 ; -48
        ddd2: a2 f8     ldx     #$f8 ; -8
        ddd4: 20 75 df  jsr     vapp_ldraw_A,X
        ddd7: c6 37     dec     $37
        ddd9: 10 b4     bpl     $dd8f
        dddb: 60        rts
        dddc: .byte   73
; Starting and ending offsets in EAROM of various pieces.
        dddd: .byte   00 ; Top three initials, start
        ddde: .byte   09 ; Top three initials, end
        dddf: .byte   0a ; Top three scores, start
        dde0: .byte   15 ; Top three scores, end
        dde1: .byte   16 ; Switched-on time, start
        dde2: .byte   22 ; Switched-on time, end
; Pointers to RAM versions of EAROM stuff
        dde3: .ptr    hs_initials_3
        dde5: .ptr    hs_score_3
        dde7: .ptr    on_time_l
  zero_times: a9 04     lda     #$04
        ddeb: d0 06     bne     $ddf3
 zero_scores: a9 03     lda     #$03
        ddef: d0 02     bne     $ddf3
        ddf1: a9 07     lda     #$07
        ddf3: a0 ff     ldy     #$ff
        ddf5: d0 08     bne     $ddff
        ddf7: a9 03     lda     #$03
        ddf9: d0 02     bne     $ddfd
        ddfb: a9 04     lda     #$04
        ddfd: a0 00     ldy     #$00
        ddff: 8c c6 01  sty     earom_clr ; A now 3/4/7; Y now $00/$ff
        de02: 48        pha
        de03: 0d c7 01  ora     $01c7
        de06: 8d c7 01  sta     $01c7
        de09: 68        pla
        de0a: 0d c8 01  ora     $01c8
        de0d: 8d c8 01  sta     $01c8
        de10: 60        rts
        de11: a9 07     lda     #$07
        de13: 8d c7 01  sta     $01c7
        de16: a9 00     lda     #$00
        de18: 8d c8 01  sta     $01c8
        de1b: ad ca 01  lda     earom_op
        de1e: d0 4b     bne     $de6b
        de20: ad c7 01  lda     $01c7
        de23: f0 46     beq     $de6b
        de25: a2 00     ldx     #$00
        de27: 8e cb 01  stx     earom_blkoff
        de2a: 8e cf 01  stx     earom_cksum
        de2d: 8e ce 01  stx     $01ce
; This loop finds the highest bit in A and leaves it in $01ce - the bcc
; tests the C bit set by the asl; the dex doesn't touch C.  It also leaves
; the bit number of this bit in X (0 to 2, since A is $0-$7).
        de30: a2 08     ldx     #$08
        de32: 38        sec
        de33: 6e ce 01  ror     $01ce
        de36: 0a        asl     a
        de37: ca        dex
        de38: 90 f9     bcc     $de33
        de3a: a0 80     ldy     #$80
        de3c: ad ce 01  lda     $01ce
        de3f: 2d c8 01  and     $01c8
        de42: d0 02     bne     $de46
        de44: a0 20     ldy     #$20
        de46: 8c ca 01  sty     earom_op
        de49: ad ce 01  lda     $01ce
        de4c: 4d c7 01  eor     $01c7
        de4f: 8d c7 01  sta     $01c7
        de52: 8a        txa
        de53: 0a        asl     a
        de54: aa        tax
        de55: bd dd dd  lda     $dddd,x
        de58: 8d cc 01  sta     earom_ptr
        de5b: bd de dd  lda     $ddde,x
        de5e: 8d cd 01  sta     earom_blkend
        de61: bd e3 dd  lda     $dde3,x
        de64: 85 bd     sta     earom_memptr
        de66: bd e4 dd  lda     $dde4,x
        de69: 85 be     sta     earom_memptr+1
        de6b: a0 00     ldy     #$00
        de6d: 8c 40 60  sty     eactl_mbst
        de70: ad ca 01  lda     earom_op
        de73: d0 01     bne     $de76
        de75: 60        rts
        de76: ac cb 01  ldy     earom_blkoff
        de79: ae cc 01  ldx     earom_ptr
        de7c: 0a        asl     a
        de7d: 90 0d     bcc     $de8c
; EAROM op $80
        de7f: 9d 00 60  sta     earom_write,x
        de82: a9 40     lda     #$40
        de84: 8d ca 01  sta     earom_op
        de87: a0 0e     ldy     #$0e
        de89: b8        clv
        de8a: 50 73     bvc     $deff
        de8c: 10 25     bpl     $deb3
; EAROM op $40
        de8e: a9 80     lda     #$80
        de90: 8d ca 01  sta     earom_op
        de93: ad c6 01  lda     earom_clr
        de96: f0 04     beq     $de9c
        de98: a9 00     lda     #$00
        de9a: 91 bd     sta     (earom_memptr),y
        de9c: b1 bd     lda     (earom_memptr),y
        de9e: ec cd 01  cpx     earom_blkend
        dea1: 90 08     bcc     $deab
        dea3: a9 00     lda     #$00
        dea5: 8d ca 01  sta     earom_op
        dea8: ad cf 01  lda     earom_cksum
        deab: 9d 00 60  sta     earom_write,x
        deae: a0 0c     ldy     #$0c
        deb0: b8        clv
        deb1: 50 3f     bvc     $def2
; EAROM op $20
        deb3: a9 08     lda     #$08
        deb5: 8d 40 60  sta     eactl_mbst
        deb8: 9d 00 60  sta     earom_write,x
        debb: a9 09     lda     #$09
        debd: 8d 40 60  sta     eactl_mbst
        dec0: ea        nop
        dec1: a9 08     lda     #$08
        dec3: 8d 40 60  sta     eactl_mbst
        dec6: ec cd 01  cpx     earom_blkend
        dec9: ad 50 60  lda     earom_rd
        decc: 90 20     bcc     $deee
        dece: 4d cf 01  eor     earom_cksum
        ded1: f0 13     beq     $dee6
        ded3: a9 00     lda     #$00
        ded5: ac cb 01  ldy     earom_blkoff
        ded8: 91 bd     sta     (earom_memptr),y
        deda: 88        dey
        dedb: 10 fb     bpl     $ded8
        dedd: ad ce 01  lda     $01ce
        dee0: 0d c9 01  ora     hs_initflag
        dee3: 8d c9 01  sta     hs_initflag
        dee6: a9 00     lda     #$00
        dee8: 8d ca 01  sta     earom_op
        deeb: b8        clv
        deec: 50 02     bvc     $def0
        deee: 91 bd     sta     (earom_memptr),y
        def0: a0 00     ldy     #$00
        def2: 18        clc
        def3: 6d cf 01  adc     earom_cksum
        def6: 8d cf 01  sta     earom_cksum
        def9: ee cb 01  inc     earom_blkoff
        defc: ee cc 01  inc     earom_ptr
        deff: 8c 40 60  sty     eactl_mbst
        df02: 98        tya
        df03: d0 03     bne     $df08
        df05: 4c 1b de  jmp     $de1b
        df08: 60        rts
    vapp_rts: a9 c0     lda     #$c0 ; vrts (first byte)
        df0b: d0 05     bne     $df12
vapp_centre_halt: 20 53 df  jsr     vapp_vcentre_2
   vapp_halt: a9 20     lda     #$20 ; vhalt (first byte)
        df12: a0 00     ldy     #$00
        df14: 91 74     sta     (vidptr_l),y
        df16: 4c ac df  jmp     $dfac
; Appends the vjsr for the digit corresponding to the low four bits of A
; on entry.  If C is set, zeros become spaces; C is cleared if the digit
; is nonzero.
vapp_digit_lz: 90 04     bcc     vapp_digit
        df1b: 29 0f     and     #$0f
        df1d: f0 05     beq     $df24
  vapp_digit: 29 0f     and     #$0f
        df21: 18        clc
        df22: 69 01     adc     #$01
        df24: 08        php
        df25: 0a        asl     a
        df26: a0 00     ldy     #$00
        df28: aa        tax
        df29: bd e4 31  lda     char_jsrtbl,x
        df2c: 91 74     sta     (vidptr_l),y
        df2e: bd e5 31  lda     char_jsrtbl+1,x
        df31: c8        iny
        df32: 91 74     sta     (vidptr_l),y
        df34: 20 5f df  jsr     inc_vidptr
        df37: 28        plp
        df38: 60        rts
; Appends a vjsr to the video list.  A holds high byte of address to vjsr
; to; X holds low byte.  Note that the $e0 bits of A are ignored.  (The
; low bit of X is discarded too, but the vjsr format compels this anyway;
; a vjsr to an odd address is not representible.)
vapp_vjsr_AX: 4a        lsr     a
        df3a: 29 0f     and     #$0f
        df3c: 09 a0     ora     #$a0
        df3e: a0 01     ldy     #$01
        df40: 91 74     sta     (vidptr_l),y
        df42: 88        dey
        df43: 8a        txa
        df44: 6a        ror     a
        df45: 91 74     sta     (vidptr_l),y
        df47: c8        iny
        df48: d0 15     bne     inc_vidptr
; Append a vscale or vstat.  The second byte is A|$60, the first byte is
; in $73, or Y, depending on which entry point.  I suppose this could
; generate a vrts or vjmp if entered with A having $80 set.
vapp_sclstat_A_73: a4 73     ldy     draw_z
vapp_sclstat_A_Y: 09 60     ora     #$60
        df4e: aa        tax
        df4f: 98        tya
        df50: 4c 57 df  jmp     vapp_A_X_Y=0
; $40 $80 = vcentre (why $40? who knows.)
vapp_vcentre_2: a9 40     lda     #$40
        df55: a2 80     ldx     #$80
; Append first A, then X, to the video stream.
vapp_A_X_Y=0: a0 00     ldy     #$00
    vapp_A_X: 91 74     sta     (vidptr_l),y
        df5b: c8        iny
        df5c: 8a        txa
        df5d: 91 74     sta     (vidptr_l),y
; increment vidptr_l/vidptr_h by the offset accumulated in y
  inc_vidptr: 98        tya
        df60: 38        sec
        df61: 65 74     adc     vidptr_l
        df63: 85 74     sta     vidptr_l
        df65: 90 02     bcc     $df69
        df67: e6 75     inc     vidptr_h
        df69: 60        rts
; Append a vscale to the video stream, with l=0 and b coming from the
; value in A on entry (we assume it's in the range 0-7).
vapp_scale_A,0: a0 00     ldy     #$00
; ...fall through into...
; Append a vscale to the video stream, getting l from Y and b from A on
; entry (we assume they're in range).
vapp_scale_A,Y: 09 70     ora     #$70
        df6e: aa        tax
        df6f: 98        tya
        df70: 4c 57 df  jmp     vapp_A_X_Y=0
; Appends a long draw to the video list, just like vapp_ldraw_A,X below,
; except that the incoming Y value is stored in $73 first (and thus used
; as the Z value for the draw).
vapp_ldraw_A,X,Y: 84 73     sty     draw_z
; Appends a long draw to the video list.  The X coordinate of the draw
; comes from the A register on entry (sign-extended); the Y coordinate
; from the X register (again, sign-extended).  The Z value for the draw
; is the high three bits of $73.  The Y register and $6e-$71 are trashed.
vapp_ldraw_A,X: a0 00     ldy     #$00
        df77: 0a        asl     a
        df78: 90 01     bcc     $df7b
        df7a: 88        dey
        df7b: 84 6f     sty     $6f
        df7d: 0a        asl     a
        df7e: 26 6f     rol     $6f
        df80: 85 6e     sta     $6e
        df82: 8a        txa
        df83: 0a        asl     a
        df84: a0 00     ldy     #$00
        df86: 90 01     bcc     $df89
        df88: 88        dey
        df89: 84 71     sty     $71
        df8b: 0a        asl     a
        df8c: 26 71     rol     $71
        df8e: 85 70     sta     $70
        df90: a2 6e     ldx     #$6e
        df92: a0 00     ldy     #$00
        df94: b5 02     lda     $02,x
        df96: 91 74     sta     (vidptr_l),y
        df98: b5 03     lda     timectr,x
        df9a: 29 1f     and     #$1f
        df9c: c8        iny
        df9d: 91 74     sta     (vidptr_l),y
        df9f: b5 00     lda     gamestate,x
        dfa1: c8        iny
        dfa2: 91 74     sta     (vidptr_l),y
        dfa4: b5 01     lda     $01,x
        dfa6: 45 73     eor     draw_z
        dfa8: 29 1f     and     #$1f
        dfaa: 45 73     eor     draw_z
        dfac: c8        iny
        dfad: 91 74     sta     (vidptr_l),y
        dfaf: d0 ae     bne     inc_vidptr
; Appends a multidigit number.  A holds the zero page address of the low
; two digits of the number; Y holds the number of two-digit pairs to
; process.
vapp_multdig_y@a: 38        sec
        dfb2: 08        php
        dfb3: 88        dey
        dfb4: 84 ae     sty     $ae
        dfb6: 18        clc
        dfb7: 65 ae     adc     $ae
        dfb9: 28        plp
        dfba: aa        tax
        dfbb: 08        php
        dfbc: 86 af     stx     $af
        dfbe: b5 00     lda     gamestate,x
        dfc0: 4a        lsr     a
        dfc1: 4a        lsr     a
        dfc2: 4a        lsr     a
        dfc3: 4a        lsr     a
        dfc4: 28        plp
        dfc5: 20 19 df  jsr     vapp_digit_lz
        dfc8: a5 ae     lda     $ae
        dfca: d0 01     bne     $dfcd
        dfcc: 18        clc
        dfcd: a6 af     ldx     $af
        dfcf: b5 00     lda     gamestate,x
        dfd1: 20 19 df  jsr     vapp_digit_lz
        dfd4: a6 af     ldx     $af
        dfd6: ca        dex
        dfd7: c6 ae     dec     $ae
        dfd9: 10 e0     bpl     $dfbb
        dfdb: 60        rts
; Used at $dbb2
        dfdc: .byte   10
        dfdd: .byte   10
        dfde: .byte   40
        dfdf: .byte   40
        dfe0: .byte   90
        dfe1: .byte   90
        dfe2: .byte   ff
        dfe3: .byte   ff
; 20-element sine wave, for drawing the spinner line on selftest screen 1
spinner_sine: .chunk  20
        dff8: .byte   00
        dff9: .byte   00
        dffa: .byte   04
        dffb: .byte   d7
        dffc: .byte   3f
        dffd: .byte   d9
        dffe: .byte   04
        dfff: .byte   d7
        e000: .space  6144
        f800: .chunk  2042
     vec_nmi: .ptr    nmi_irq_brk
     vec_res: .ptr    reset
 vec_brk_irq: .ptr    nmi_irq_brk
       10000: 

