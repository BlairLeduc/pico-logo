********************************************************************************
                   * Battlezone (rev 2)                                                           *
                   * Copyright 1980 Atari, Inc.                                                   *
                   *                                                                              *
                   * By Ed Rotberg, Jed Margolin, Harry Jenkins, Roger Hector, Howard Delman,     *
                   * Mike Albaugh, Dan Pliskin, Doug Snyder, Owen Rubin, and Morgan Hoff.         *
                   ********************************************************************************
                   * Disassembly by Andy McFadden, using 6502bench SourceGen v1.7.                *
                   * Originally published 2020/05/23                                              *
                   * Updated with source refs 2022/03/08                                          *
                   *                                                                              *
                   * Huge thanks to:                                                              *
                   *  + Simon Stapleton for his partial Battlezone disassembly.                   *
                   *  + Eric Smith for his work deciphering the vector processor and math box.    *
                   *  + The various MAME contributors.                                            *
                   *  + douglasgb et.al. in the arcade-museum.com forums.                         *
                   *                                                                              *
                   * POKEY register descriptions from https://en.wikipedia.org/wiki/POKEY .       *
                   *                                                                              *
                   * The binary used is a concatenation of the eight ROMs that are addressable by *
                   * the 6502.  The AVG ROMs were placed at the end.                              *
                   *                                                                              *
                   * About a year and a half after I finished this, the original rev 1 source     *
                   * code was posted on GitHub.  I updated the project file with some notes,      *
                   * including references to the source file names.                               *
                   ********************************************************************************
                   * Memory map in a nutshell:                                                    *
                   *                                                                              *
                   *   $0000-03ff: RAM (1KB)                                                      *
                   *   $0800-1fff: various I/O, including math box                                *
                   *   $2000-2fff: vector generator RAM (4KB)                                     *
                   *   $3000-3fff: vector generator ROM (4KB)                                     *
                   *   $5000-7fff: program ROM (12KB)                                             *
                   *                                                                              *
                   * I/O port definitions (primarily from MAME driver):                           *
                   *                                                                              *
                   *   IN0 ($0800): KBDTSLCR                                                      *
                   *     K="tied to a 3KHz clock"                                                 *
                   *     B="VG HALT" bit, indicates VSM is busy                                   *
                   *     D=diagnostic step                                                        *
                   *     T=self-test switch position (1=self-test mode)                           *
                   *     S=slam switch (0=slam detected)                                          *
                   *     LCR=coin mechanism state (left/center/right chutes)                      *
                   *                                                                              *
                   *   DSW0 ($0A00): LLBBMMTT                                                     *
                   *     LL=language (00=English, 01=French, 10=German, 11=Spanish)               *
                   *     BB=bonus tank score (00=none, 01=15K/100K, 10=25K/100K, 11=50K/100K)     *
                   *     MM=missile appears at score (5K, 10K, 20K, 30K)                          *
                   *     TT=number of starting tanks (value + 2)                                  *
                   *                                                                              *
                   *   DSW1 ($0C00): BBBCRRPP                                                     *
                   *     BBB=bonus coins                                                          *
                   *        000=no bonus coin                                                     *
                   *        001=for every 2 coins, +1 coin                                        *
                   *        010=for every 4 coins, +1 coin                                        *
                   *        011=for every 4 coins, +2 coins                                       *
                   *        100=for every 5 coins, +1 coin                                        *
                   *     C=center coin mech x1/x2                                                 *
                   *     RR=right coin mech x1/x4/x5/x6                                           *
                   *     PP=play cost                                                             *
                   *        00=free play                                                          *
                   *        01=1 coin for 2 plays                                                 *
                   *        10=1 coin for 1 play                                                  *
                   *        11=2 coins for 1 play                                                 *
                   *                                                                              *
                   *   IN3/POKEY_ALLPOT: U21BLLRR                                                 *
                   *     U=unused                                                                 *
                   *     2=start 2 (not used)                                                     *
                   *     1=start 1                                                                *
                   *     B=button pressed                                                         *
                   *     LL=left joystick (00=middle, 01=back, 10=fwd)                            *
                   *     RR=right joystick (00=middle, 01=back, 10=fwd)                           *
                   *                                                                              *
                   *   DSOUND_CTRL ($1840):                                                       *
                   *     $80: motor enable - 1=engine sound enabled                               *
                   *     $40: start LED - controls LED on cabinet start button                    *
                   *     $20: sound enable - 0=mute all sound (incl. POKEY)                       *
                   *     $10: engine rev - 0=rev down, 1=rev up                                   *
                   *     $08: cannon fire volume - 0=soft 1=loud                                  *
                   *     $04: cannon fire enable - set to 1 while cannon sound playing            *
                   *     $02: explosion volume - 0=soft 1=loud                                    *
                   *     $01: explosion enable - set to 1 while explosion sound playing           *
                   *                                                                              *
                   ********************************************************************************
                   * On the battlefield, when facing angle 0, +Z is forward and +X is to the      *
                   * left.  Increase the angle to turn left (counter-clockwise rotation).  After  *
                   * the View transform, the distance to an object directly in front of the       *
                   * viewer is determined by the Z coordinate.  This is a little different from   *
                   * typical GL/DX conventions.                                                   *
                   ********************************************************************************
                   vg_intensity    .eq     $01               ;intensity in hi 3 bits: IIIxxxxx
                   vg_cmd_ptr      .eq     $02    {addr/2}   ;points to AVG cmd buf ($2000/$2800)
                   credit_count    .eq     $0e               ;number of credits purchased
                   dsnd_expl_ctr   .eq     $0f               ;nonzero while explosion sound playing
                   unit_state      .eq     $12               ;$00=alive, $20=dying
                   enemy_state     .eq     $14               ;$00=alive, $20=dying
                   dsnd_ctrl_val   .eq     $15               ;bits for DSOUND_CTRL
                   enemy_ang_delta .eq     $16               ;delta btwn player facing and angle to enemy
                   coins_inserted  .eq     $21               ;# of coins inserted, for credit calc
                   projectile_state_0 .eq  $24               ;player ($00=inactive, $01-7f=TTL, $80+=exploding)
                   dist_intensity  .eq     $25               ;intensity reduction for distant obj
                   projectile_state_1 .eq  $26               ;enemy projectile state ($00, $01-7f, $80+)
                   plyr_facing_lo  .eq     $27               ;holds $00 or $80 for 512-step angle
                   vg_write_addr_hi .eq    $28               ;VG RAM write addr hi ($20/28, $00 when done)
                   plyr_facing_hi  .eq     $2a               ;direction player is facing
                   enemy_facing    .eq     $2c               ;direction enemy tank/missile is facing
                   unit_pos_z      .eq     $2d    {addr/2}   ;player position, Z coord
                   enemy_pos_z     .eq     $2f    {addr/2}   ;enemy unit position, Z coord
                   unit_pos_x      .eq     $31    {addr/2}   ;player position, X coord
                   enemy_pos_x     .eq     $33    {addr/2}   ;enemy unit position, X coord
                   cur_beam_xc     .eq     $35    {addr/2}   ;current beam position, X coord
                   cur_beam_yc     .eq     $37    {addr/2}   ;current beam position, Y coord
                   nmi_count_x16   .eq     $39               ;incr every 16 NMIs, reset by main
                   nmi_count       .eq     $3a               ;incr every NMI
                   screen_coords   .eq     $3d    {addr/104} ;4 bytes each (XC then YC) x26; $3d-a4
                   tread_drip_ctr_i .eq    $a5               ;ref this indexed with X=$02 to update $a7
                   temp_a6         .eq     $a6               ;temp storage, used in a couple places
                   tread_drip_ctr  .eq     $a7               ;counter for animating treads / missile drips
                   proj_pos_z      .eq     $a8    {addr/4}   ;projectile Z coords (16-bit, player/enemy)
                   proj_pos_x      .eq     $ac    {addr/4}   ;projectile X coords (16-bit, player/enemy)
                   proj_vel_z      .eq     $b0    {addr/2}   ;projectile velocities, 16-bit Z
                   enemy_proj_vel_z .eq    $b2    {addr/2}
                   proj_vel_x      .eq     $b4    {addr/2}   ;projectile velocities, 16-bit X
                   enemy_proj_vel_x .eq    $b6    {addr/2}
                   score           .eq     $b8    {addr/2}   ;current score, as two-byte BCD
                   enemy_score     .eq     $ba    {addr/2}   ;enemy score (points for killing player)
                   enemy_turn_to   .eq     $bc               ;heading enemy wants to turn to
                   dsnd_cnon_ctr   .eq     $bd               ;counter for cannon fire sound
                   radar_facing    .eq     $be               ;spinning radar antenna on slow tank
                   radar_sweep_ang .eq     $bf               ;current angle for radar sweep
                   saved_obj_pos   .eq     $c0    {addr/4}   ;holds position for undo after collision
                   move_counter    .eq     $c4               ;frames until we pick new enemy heading
                   enemy_rev_flags .eq     $c5               ;$01=reverse, $02=dir, $80=unused?
                   frame_counter   .eq     $c6               ;incremented every game frame (15.625Hz)
                   death_crack_index .eq   $c7               ;tracks cracks and post-death pause
                   recent_coll_flag .eq    $c8               ;bool 00/01: has player recently collided?
                   enemy_known_flag .eq    $c9               ;bool 00/01: have we played "new enemy" alert?
                   missile_hop_flag .eq    $ca               ;bool 00/01: is missile hopping up?
                   missile_flag    .eq     $cb               ;bool 00/ff: set if enemy unit is a missile
                   player_lives    .eq     $cc               ;number of tanks player has left (1-6)
                   game_over_flags .eq     $cd               ;0=alive, >0=dead, $80=enter high score
                   play_flag       .eq     $ce               ;bool 00/ff: is game being played?
                   close_firing_angle? .eq $cf               ;alternates between $02 and $10
                   enemy_ang_delt_abs .eq  $d0               ;abs val of delta between enemy angle and facing
                   rez_protect     .eq     $d1               ;enemy plays nice until counts up to $20/$ff
                   frame_count_256x .eq    $d2               ;inc every 256 game frames (16.384 sec)
                   saucer_ttl      .eq     $d3               ;time until saucer appears / changes direction
                   saucer_dead_intens .eq  $d4               ;brighten/fade saucer after hit; $00 while alive
                   saucer_z        .eq     $d5    {addr/2}   ;saucer position, Z
                   saucer_x        .eq     $d7    {addr/2}   ;saucer position, X
                   saucer_facing   .eq     $d9               ;saucer's current facing
                   saucer_vel_z    .eq     $da    {addr/2}   ;saucer velocity, Z
                   saucer_vel_x    .eq     $dc    {addr/2}   ;saucer velocity, X
                   saucer_state    .eq     $de               ;$00=inactive, $01=silent, $81=noisy
                   horizon_adj     .eq     $df               ;used to shift horizon down when player hit
                   coin_ctr_state1 .eq     $e0    {addr/3}   ;usually 0/0/0, updates when coin drops
                   coin_ctr_state2 .eq     $e4    {addr/3}   ;usually 0/0/0, updates when coin drops
                   coin_ctr_state3 .eq     $e7    {addr/3}   ;usually 1f/1f/1f, updates when coin drops
                   slam_counter    .eq     $ea               ;nonzero if recently slammed
                   latched_dsw1    .eq     $eb               ;DSW1, latched during NMI
                   bonus_coin_count .eq    $ec               ;# of coins inserted (for bonus coin chk)
                   nmi_refresh_ctr .eq     $ed               ;used to start display refresh every 6 NMIs
                   audio_indices   .eq     $ef    {addr/4}   ;index into sfx table for 4 channels
                   audio_values    .eq     $f3    {addr/4}   ;values for AUDF1, AUDC1, AUDF2, AUDC2
                   audio_dur_ctr   .eq     $f7    {addr/4}   ;audio duration counter
                   audio_rep_ctr   .eq     $fb    {addr/4}   ;audio repetition counter
                   STACK           .eq     $0100  {addr/256} ;$00-7f used as stack
                   vis_obj_zpos    .eq     $0200  {addr/56}  ;vis obj list: 16-bit Z coord (x 28)
                   vis_obj_xpos    .eq     $0238  {addr/56}  ;vis obj list: 16-bit X coord (x 28)
                   vis_obj_type    .eq     $0270             ;vis obj list: type, alt bytes (x28); $ff=EOL
                   vis_obj_facing  .eq     $0271  {addr/55}  ;vis obj list: facing angle, alt bytes (x28)
                   chunk_z_pos     .eq     $02a8  {addr/12}  ;exploding chunk positions, 16-bit Z
                   chunk_x_pos     .eq     $02b8  {addr/12}  ;exploding chunk positions, 16-bit X
                   chunk_y_vel     .eq     $02c8  {addr/12}  ;exploding chunk velocities, 16-bit Y
                   ypos_by_type    .eq     $02d8  {addr/16}  ;Y coord for missile, logo, chunks
                   enemy_dist_hi   .eq     $02e8             ;high byte of distance from enemy to player
                   radar_blip_inten .eq    $02e9             ;intensity of dot on radar scope
                   sauc_log_inten  .eq     $02ea             ;intensity for saucer and game logo
                   input_ctrl_state .eq    $02eb             ;data from controller read
                   missile_count   .eq     $02ec             ;number of missiles we've created; initially $ff
                   strange_one_a   .eq     $02ed             ;holds calc result (1)
                   hs_scores       .eq     $0300  {addr/30}  ;high scores (10 entries * 3 bytes)
                   hs_initials     .eq     $031e  {addr/30}  ;high score initials (10 entries * 3 bytes)
                   hs_list_flag    .eq     $033c             ;flag 00/01/80/81; are we showing HS list?
                   hs_initials_entry .eq   $033d  {addr/3}   ;holds initials while player enters them
                   hs_index        .eq     $0340             ;used as index into HS table (0-27 by 3)
                   hs_entry_spd_ctr .eq    $0341             ;slows rate at which we spin initials
                   hs_entry_pos    .eq     $0342             ;which initial we're entering (0-2)
                   hs_btn_prev     .eq     $0343             ;prev button read while entering initials
                   hs_btn_read     .eq     $0344             ;tracks button while entering initials
                   score_100k_flag .eq     $0345             ;bool 00/ff: score just broke 100K
                   logo_vis_zpos   .eq     $0346  {addr/2}   ;logo center Z coordinate
                   logo_vis_ypos   .eq     $0348  {addr/2}   ;logo center Y coordinate
                   missile_rand    .eq     $034a             ;random number used when creating missiles
                   attract_logo_flag .eq   $034b             ;bool 00/ff: in "attract" mode, showing logo
                   volpart_ttl     .eq     $034d  {addr/5}   ;volcano particle time-to-live (0=inactive)
                   volpart_vx      .eq     $0352  {addr/5}   ;volcano particle X velocity
                   volpart_vy      .eq     $0357  {addr/5}   ;volcano particle Y velocity
                   volpart_xc_lo   .eq     $035c  {addr/5}   ;volcano particle X-coord, low
                   volpart_xc_hi   .eq     $0361  {addr/5}   ;volcano particle X-coord, high
                   volpart_yc_lo   .eq     $0366  {addr/5}   ;volcano particle Y-coord, low
                   volpart_yc_hi   .eq     $036b  {addr/5}   ;volcano particle Y-coord, high
                   strange_one_b   .eq     $0370             ;holds calc result (1)
                   IN0             .eq     $0800             ;R (see file comment)
                   DSW0            .eq     $0a00             ;R DIP switch 0 (see file comment)
                   DSW1            .eq     $0c00             ;R DIP switch 1 (see file comment)
                   COIN_COUNTER    .eq     $1000             ;W something with coins
                   VEC_GEN_START   .eq     $1200             ;W vector generator start
                   WATCHDOG_CLEAR  .eq     $1400             ;W resets the watchdog timer
                   VEC_GEN_RESET   .eq     $1600             ;W vector generator reset
                   MB_STATUS       .eq     $1800             ;R bool 00/80(?): busy
                   MB_RESULT_LO    .eq     $1810             ;R low byte of result
                   MB_RESULT_HI    .eq     $1818             ;R high byte of result
                   POKEY_AUDF1     .eq     $1820             ;W audio channel 1 frequency
                   POKEY_AUDC1     .eq     $1821             ;W audio channel 1 control
                   POKEY_AUDF2     .eq     $1822             ;W audio channel 2 frequency
                   POKEY_AUDC2     .eq     $1823             ;W audio channel 2 control
                   POKEY_AUDF3     .eq     $1824             ;W audio channel 3 frequency
                   POKEY_AUDC3     .eq     $1825             ;W audio channel 3 control
                   POKEY_AUDF4     .eq     $1826             ;W audio channel 4 frequency
                   POKEY_AUDC4     .eq     $1827             ;W audio channel 4 control
                   POKEY_ALLPOT    .eq     $1828             ;R read 8 line POT port state
                   POKEY_AUDCTL    .eq     $1828             ;W audio control
                   POKEY_RANDOM    .eq     $182a             ;R random number
                   POKEY_POTGO     .eq     $182b             ;W start POT scan sequence
                   POKEY_SKCTL     .eq     $182f             ;W serial port control
                   DSOUND_CTRL     .eq     $1840             ;W control discrete sound H/W (see file comment)
                   MB_SET_R0L      .eq     $1860             ;W set R0 low; return R0
                   MB_SET_R0H      .eq     $1861             ;W set R0 high; return R0
                   MB_SET_R1L      .eq     $1862             ;W set R1 low; return R1
                   MB_SET_R1H      .eq     $1863             ;W set R1 high; return R1
                   MB_SET_R2L      .eq     $1864             ;W set R2 low; return R2
                   MB_SET_R2H      .eq     $1865             ;W set R2 high; return R2
                   MB_SET_R3L      .eq     $1866             ;W set R3 low; return R3
                   MB_SET_R3H      .eq     $1867             ;W set R3 high; return R3
                   MB_SET_R4L      .eq     $1868             ;W set R4 low; return R4
                   MB_SET_R4H      .eq     $1869             ;W set R4 high; return R4
                   MB_SET_R5L      .eq     $186a             ;W set R5 low; return R5
                   MB_ROT_Z        .eq     $186b             ;W set R5 high; compute Z rot
                   MB_SET_R6       .eq     $186c             ;W set R6; return R6
                   MB_SET_RAL      .eq     $186d             ;W set RA low; return RA
                   MB_SET_RAH      .eq     $186e             ;W set RA high; return RA
                   MB_SET_RBL      .eq     $186f             ;W set RB low; return RB
                   MB_SET_RBH      .eq     $1870             ;W set RB high; return RB
                   MB_SCREEN_X     .eq     $1871             ;W set R5 high; compute screen X
                   MB_ROT_X        .eq     $1872             ;W data ignored; compute X rot
                   MB_DIVIDE_B7    .eq     $1874             ;W data ignored; return RB/R7
                   MB_SET_R7L      .eq     $1875             ;W set R7 low; return R7
                   MB_SET_R7H      .eq     $1876             ;W set R7 high; return R7
                   MB_GET_R7       .eq     $1877             ;W data ignored; return R7
                   MB_GET_R8       .eq     $1879             ;W data ignored; return R8
                   MB_CALC_DIST    .eq     $187d             ;W set R3 high and calc ~distance
                   MB_CALC_HYPOT   .eq     $187e             ;W data ignored; calc ~sqrt(a^2 + b^2)
                   VEC_GEN_RAM     .eq     $2000  {addr/4096} ;vector generator RAM

                   ; 
                   ; Main loop.
                   ; 
                   ; A non-maskable interrupt fires at 250 Hz.  Every 16 interrupts the NMI handler
                   ; increments the counter we're watching, which means we update the game state
                   ; 15.625x per second.
                   ; 
                   ; If the NMI or non-NMI code locks up, the watchdog will eventually reset the
                   ; machine.
                   ; 
                                   .addrs  $5000
                   ]obj_index      .var    $10    {addr/1}
                   ]step_counter   .var    $18    {addr/1}
                   ]ptr            .var    $3b    {addr/2}

5000: 46 39        MainLoop        lsr     nmi_count_x16     ;spin until NMI code sets a bit to tell
5002: 90 fc                        bcc     MainLoop          ; us it's time to wake up
5004: 20 ba 58                     jsr     UpdateVolcano     ;update volcano particles
5007: a6 28        :Spin           ldx     vg_write_addr_hi  ;has NMI routine flipped the command buffer?
5009: f0 fc                        beq     :Spin             ;not yet, spin until it does
500b: a9 02                        lda     #$02              ;init cmd ptr to $2002 or $2802
500d: 85 02                        sta     vg_cmd_ptr
500f: 86 03                        stx     vg_cmd_ptr+1
5011: a9 01                        lda     #$01
5013: 20 81 7a                     jsr     VgScaleL0         ;set scale to 1:1
5016: 20 6a 7a                     jsr     VgCenter          ;return beam to center
5019: a9 81                        lda     #$81              ;$81=-127, *4 = -508
501b: aa                           tax
501c: a0 00                        ldy     #$00              ;beam off
501e: 20 8e 7a                     jsr     VgVec8I           ;move to -508,-508
5021: a9 00                        lda     #$00              ;E=0 L O
5023: 20 63 7a                     jsr     VgStat            ;set lower-left window bound
5026: e6 c6                        inc     frame_counter     ;advance frame counter (used for timing things)
5028: d0 06                        bne     :FcNz
502a: e6 d2                        inc     frame_count_256x  ;inc every 256 frames (16.384 sec)
502c: a5 d2                        lda     frame_count_256x
502e: c9 04                        cmp     #$04              ;set carry if counter has reached 4
5030: 24 cd        :FcNz           bit     game_over_flags   ;are we entering high scores?
5032: 10 06                        bpl     :NoEnterHS        ;no, branch
5034: 20 d9 51                     jsr     FullScreenWin     ;yes, show high-score entry screen
5037: 4c c5 53                     jmp     EnterInitials

503a: 24 ce        :NoEnterHS      bit     play_flag         ;are we playing the game?
503c: 30 2d                        bmi     :PlayingGame      ;yes, go do that
503e: a5 c7                        lda     death_crack_index ;is the player death animation running?
5040: d0 29                        bne     :PlayingGame      ;yes, go do that
5042: 90 1c                        bcc     :ShowScores       ;if timer was low, don't change attract mode yet
5044: 20 e8 69                     jsr     CreateTank        ;create a tank
5047: a2 03                        ldx     #$03
5049: ad 3c 03                     lda     hs_list_flag      ;toggle attract between demo and scores
504c: 49 80                        eor     #$80
504e: 8d 3c 03                     sta     hs_list_flag
5051: 86 d2                        stx     frame_count_256x  ;reset counters
5053: 85 c6                        sta     frame_counter     ;starts at $80 instead of $00 in high-score mode
5055: 30 09                        bmi     :ShowScores
5057: 24 ce                        bit     play_flag         ;are we playing the game? (updated by NMI)
5059: 30 19                        bmi     Playing           ;yes, do that
505b: a9 ff                        lda     #$ff              ;after high scores, show logo
505d: 8d 4b 03                     sta     attract_logo_flag
5060: 2c 3c 03     :ShowScores     bit     hs_list_flag      ;are we showing high scores?
5063: 10 0f                        bpl     Playing           ;no, do attract-mode demo
5065: 20 d9 51                     jsr     FullScreenWin     ;yes, show the high score list
5068: 4c 9b 54                     jmp     HighScoreList

506b: 90 07        :PlayingGame    bcc     Playing           ;branch if counter hasn't expired yet
506d: 24 cb                        bit     missile_flag      ;is the enemy unit a missile?
506f: 30 03                        bmi     Playing           ;yes, keep playing
5071: 20 22 6a                     jsr     CreateMissile     ;no, throw a missile at the slacker
                   ; The game is being played, either by the player or the attract-mode demo.
                   ; 
                   ; Set window to play area (bottom 3/4 of screen).
5074: 20 6a 7a     Playing         jsr     VgCenter          ;return beam to center
5077: a9 7f                        lda     #$7f              ;deltaX=127*4=508
5079: a2 30                        ldx     #$30              ;deltaY=48*4=192
507b: a0 00                        ldy     #$00              ;beam off
507d: 20 8e 7a                     jsr     VgVec8I           ;move
5080: a9 02                        lda     #$02              ;E=0 H O
5082: 20 63 7a                     jsr     VgStat            ;set window
5085: 24 ce                        bit     play_flag         ;are we playing the game?
5087: 30 05                        bmi     :NoNotice         ;yes, skip copyright notice
5089: a2 2e                        ldx     #$2e              ;copyright notice
508b: 20 98 6c                     jsr     DrawString
508e: 20 c8 57     :NoNotice       jsr     DrawBackground    ;draw mountains and volcano
5091: c6 c4                        dec     move_counter      ;decrement enemy move counter
5093: a5 d1                        lda     rez_protect       ;update resurrection/spawn counter
5095: c9 ff                        cmp     #$ff              ;incr to $ff then stop
5097: f0 02                        beq     :NoInc
5099: e6 d1                        inc     rez_protect
509b: a9 0a        :NoInc          lda     #10               ;MB division iterates 10x (not 16)
509d: 8d 6c 18                     sta     MB_SET_R6         ;set register (high byte set to zero)
50a0: a9 00                        lda     #$00              ;init index for draw loop
50a2: 85 10                        sta     ]obj_index
                   ; Generate list of visible objects.
50a4: 20 46 59                     jsr     VLGenerate
50a7: 20 68 52                     jsr     StrangeCalcB      ;adds logo to visible object list when appropriate
                   ; Transform and draw all objects in the visible-objects list.
50aa: a6 10        :ObjLoop        ldx     ]obj_index
50ac: bd 70 02                     lda     vis_obj_type,x    ;get object type
50af: 30 11                        bmi     :EndOfList        ;$ff = end of list
50b1: 20 4a 5d                     jsr     TxfrmObject       ;transform object vertices to screen X/Y
50b4: a6 10                        ldx     ]obj_index
50b6: bd 70 02                     lda     vis_obj_type,x    ;get object type
50b9: 20 5c 5c                     jsr     DrawObject        ;draw it
50bc: e6 10                        inc     ]obj_index        ;advance index
50be: e6 10                        inc     ]obj_index
50c0: d0 e8                        bne     :ObjLoop
50c2: a5 c7        :EndOfList      lda     death_crack_index ;is the windshield cracking/cracked?
50c4: f0 03                        beq     :NotDead
50c6: 4c e9 51                     jmp     CrackWindshield   ;yes, draw that (func JMPs back into us or HS code)

50c9: 20 d9 51     :NotDead        jsr     FullScreenWin     ;set upper-right clip to full screen
50cc: 20 e9 6a                     jsr     DrawRadar
                   ; Set a couple of angles used in enemy movement code.
50cf: 20 10 68                     jsr     CalcAngleToPlayer ;calculate angle from player to enemy
50d2: 49 80                        eor     #$80
50d4: 38                           sec
50d5: e5 2a                        sbc     plyr_facing_hi    ;subtract player's facing angle
50d7: 85 16                        sta     enemy_ang_delta   ;delta between angle to enemy and facing angle
50d9: 10 05                        bpl     :IsPos
50db: 18                           clc                       ;compute absolute value
50dc: 49 ff                        eor     #$ff
50de: 69 01                        adc     #$01
50e0: 85 d0        :IsPos          sta     enemy_ang_delt_abs
                   ; Draw reticle.
50e2: 2c 4b 03                     bit     attract_logo_flag ;in "attract" mode, showing logo?
50e5: 30 1b                        bmi     :SkipDraw         ;yes, don't draw reticle
50e7: c9 02                        cmp     #$02              ;set carry if we're within 2/256ths (about 2.8 deg)
50e9: a5 24                        lda     projectile_state_0 ;player projectile ready?
50eb: f0 06                        beq     :NoFlash          ;yes, reticle is solid, branch
50ed: a9 20                        lda     #$20              ;no, flash reticle: 32 NMIs on then 32 off
50ef: 25 3a                        and     nmi_count
50f1: f0 0f                        beq     :SkipDraw
50f3: 90 06        :NoFlash        bcc     :AtEnemy          ;pointing at or near enemy, use angled reticle
50f5: a2 cc                        ldx     #<vg_reticle1     ;"no target" reticle
50f7: a9 34                        lda     #>vg_reticle1
50f9: d0 04                        bne     :DrawReticle      ;(always)

50fb: a2 00        :AtEnemy        ldx     #<vg_reticle2     ;"target" reticle
50fd: a9 35                        lda     #>vg_reticle2
50ff: 20 5a 7a     :DrawReticle    jsr     VgJsrToAddr
5102: a5 14        :SkipDraw       lda     enemy_state       ;enemy alive?
5104: d0 44                        bne     :SkipEnMsg        ;no, branch
5106: 24 ce                        bit     play_flag         ;are we playing?
5108: 30 1c                        bmi     :SkipCheck        ;yes, branch
                   ; Authenticity check.  Stores the value 1 in $02ed.  Computes a checksum on the
                   ; drawing commands, which (since we're not playing) should start with the
                   ; copyright notice.  Anything but 0 or 1 causes saucer sounds to be suppressed
                   ; after 100K points.  (See also $526c.)
510a: a5 03                        lda     vg_cmd_ptr+1
510c: 29 f8                        and     #$f8
510e: 85 3c                        sta     ]ptr+1
5110: a9 02                        lda     #$02
5112: 85 3b                        sta     ]ptr
5114: a0 35                        ldy     #$35
5116: a9 00                        lda     #$00
5118: 51 3b        :Loop           eor     (]ptr),y
511a: 88                           dey
511b: 10 fb                        bpl     :Loop
511d: 4a                           lsr     A
511e: 8d ed 02                     sta     strange_one_a
                   ; Draw status text messages.
5121: 2c 4b 03                     bit     attract_logo_flag ;are we showing the logo?
5124: 30 33                        bmi     DrawMore          ;yes, skip "enemy to" messages
5126: a9 02        :SkipCheck      lda     #$02              ;flash messages based on game frame
5128: 25 c6                        and     frame_counter
512a: f0 1e                        beq     :SkipEnMsg        ;currently not showing, branch
512c: a5 d0                        lda     enemy_ang_delt_abs ;check angle to enemy
512e: c9 16                        cmp     #$16
5130: 90 18                        bcc     :SkipEnMsg        ;it's in front, bail
5132: a2 00                        ldx     #$00              ;"enemy to "
5134: 20 98 6c                     jsr     DrawString
5137: a2 06                        ldx     #$06              ;"rear"
5139: a9 6b                        lda     #$6b
513b: c5 d0                        cmp     enemy_ang_delt_abs
513d: 90 08                        bcc     :DrawLR
513f: a2 02                        ldx     #$02              ;"left"
5141: 24 16                        bit     enemy_ang_delta
5143: 10 02                        bpl     :DrawLR           ;positive angle, left side
5145: a2 04                        ldx     #$04              ;"right"
5147: 20 98 6c     :DrawLR         jsr     DrawString
514a: a5 c8        :SkipEnMsg      lda     recent_coll_flag
514c: f0 0b                        beq     DrawMore
514e: a5 c6                        lda     frame_counter
5150: 29 04                        and     #$04
5152: d0 05                        bne     DrawMore
5154: a2 12                        ldx     #$12              ;"motion blocked by object"
5156: 20 98 6c                     jsr     DrawString
                   ; 
                   ; Draw score, remaining lives, and attract-mode strings.
                   ; 
                   ; (Other bits of code JMP here when done, e.g. windshield crack and high score
                   ; entry.  Seven locations in total.)
                   ; 
5159: 20 2e 6d     DrawMore        jsr     DrawScoreLives
515c: a5 cd                        lda     game_over_flags   ;player dead?
515e: f0 31                        beq     :Finish           ;still alive, branch
5160: a5 c7                        lda     death_crack_index ;showing death animation?
5162: d0 2d                        bne     :Finish           ;yes, branch
5164: 2c 4b 03                     bit     attract_logo_flag
5167: 30 28                        bmi     :Finish
5169: a2 14                        ldx     #$14              ;"game over"
516b: 20 98 6c                     jsr     DrawString
516e: a5 0e                        lda     credit_count      ;player has paid for at least 1 credit?
5170: d0 16                        bne     :NoShowCoins      ;yes
5172: a5 eb                        lda     latched_dsw1      ;DSW1 latched on last NMI
5174: 29 03                        and     #$03              ;bonus coin setting
5176: f0 10                        beq     :NoShowCoins      ;free play, don't draw cost strings
5178: 0a                           asl     A                 ;double it (2-6)
5179: 69 1e                        adc     #$1e              ;$20/22/24
517b: aa                           tax
517c: 20 98 6c                     jsr     DrawString        ;"1       2     S" (for example)
517f: a2 26                        ldx     #$26              ;"_ coins _ play"
5181: 20 98 6c                     jsr     DrawString
5184: a2 28                        ldx     #$28              ;"insert coin"
5186: d0 02                        bne     :CoinComm         ;(always)

5188: a2 16        :NoShowCoins    ldx     #$16              ;"press start"
518a: 24 3a        :CoinComm       bit     nmi_count         ;bit 6 set?
518c: 50 03                        bvc     :Finish           ;(toggles every 64, ~3.9x/sec)
518e: 20 98 6c                     jsr     DrawString
5191: 20 74 55     :Finish         jsr     FinishFrame       ;done drawing this frame
5194: 2c 45 03                     bit     score_100k_flag   ;just broke 100K points?
5197: 30 29                        bmi     :Play100kBoom     ;yes, try to play post-1812 Overture explosion
                   ; Update projectile positions and check for collisions.  To avoid having fast-
                   ; moving projectiles fly through things, we advance them four steps per frame
                   ; and check for collisions after each step.
5199: a9 04                        lda     #$04              ;repeat 4x
519b: 85 18                        sta     ]step_counter
519d: 20 45 60     :PMoveLoop      jsr     CheckProjColl     ;check for collision
51a0: 20 66 6c                     jsr     UpdateProjectiles ;update position
51a3: c6 18                        dec     ]step_counter
51a5: d0 f6                        bne     :PMoveLoop
                   ; 
51a7: 24 cb                        bit     missile_flag      ;is the enemy a missile?
51a9: 30 05                        bmi     :IsMissile        ;yes, branch
51ab: 20 b5 69                     jsr     GetTankType       ;no, check the tank type
51ae: 90 03                        bcc     :SlowTank
51b0: 20 4e 5f     :IsMissile      jsr     TestProj0Coll     ;extra test because missile moves rapidly (?)
51b3: 20 18 64     :SlowTank       jsr     UpdateEnemyUnit   ;update enemy tank / missile movement
51b6: 20 7c 67                     jsr     UpdateSaucer      ;update saucer movement
51b9: 20 44 5e                     jsr     UpdateHorizAdj    ;bring us back toward horizontal if we lurched
51bc: 20 d0 61                     jsr     UpdatePlayer      ;update player movement
51bf: 4c 00 50     :JumpMainLoop   jmp     MainLoop

51c2: ad ed 02     :Play100kBoom   lda     strange_one_a     ;#$01; if it's not zero or one, we will never clear
51c5: 4a                           lsr     A                 ; the "1812 Overture is playing" flag
51c6: 05 ef                        ora     audio_indices     ;nonzero if playing sound on channel 1
51c8: d0 f5                        bne     :JumpMainLoop     ;1812 Overture still playing, don't do explosion yet
51ca: 8d 45 03                     sta     score_100k_flag   ;A-reg=0
51cd: a5 15                        lda     dsnd_ctrl_val
51cf: 09 02                        ora     #$02              ;d-sound: explosion=loud
51d1: 85 15                        sta     dsnd_ctrl_val
51d3: a9 ff                        lda     #$ff              ;play for ~1 sec
51d5: 85 0f                        sta     dsnd_expl_ctr
51d7: d0 e6                        bne     :JumpMainLoop

                   ; 
                   ; Sets top-right clip to the edge of the screen.
                   ; 
51d9: 20 6a 7a     FullScreenWin   jsr     VgCenter          ;beam to center
51dc: a0 00                        ldy     #$00              ;intensity = 0
51de: a9 7f                        lda     #$7f              ;dx=dy=+508
51e0: aa                           tax
51e1: 20 8e 7a                     jsr     VgVec8I           ;move beam
51e4: a9 02                        lda     #$02              ;E=0 H O
51e6: 4c 63 7a                     jmp     VgStat            ;set window

                   ; 
                   ; Draws windshield cracks of doom.  Also handles re-spawn of player.
                   ; 
                   ; The main loop calls here with a JMP, not a JSR.  We either JMP back into it
                   ; or, if the game is over, into the high score code.
                   ; 
                   • Clear variables
                   ]saved_yreg     .var    $08    {addr/1}

51e9: a0 00        CrackWindshield ldy     #$00
51eb: 8c 27 18                     sty     POKEY_AUDC4
51ee: b9 f8 36     :CrackLoop      lda     vg_hit_cracks,y   ;crack the windshield
51f1: be f9 36                     ldx     vg_hit_cracks+1,y
51f4: 84 08                        sty     ]saved_yreg
51f6: 20 6e 7a                     jsr     VgOutputCmd
51f9: a4 08                        ldy     ]saved_yreg
51fb: c8                           iny
51fc: c8                           iny
51fd: c4 c7                        cpy     death_crack_index ;have we finished current set of cracks?
51ff: b0 61                        bcs     :IncAndBail       ;yes, branch
5201: c0 10                        cpy     #$10              ;have we drawn all possible cracks?
5203: 90 e9                        bcc     :CrackLoop        ;not yet
5205: a9 2c                        lda     #44               ;want to pause for (46-16)=30 frames (~2 sec)
5207: c5 c7                        cmp     death_crack_index ;still waiting?
5209: b0 57                        bcs     :IncAndBail       ;yes, branch
520b: a5 24                        lda     projectile_state_0 ;player projectile still in flight?
520d: d0 50                        bne     :TankDead         ;yes, let it run
                   ; We're done being dead; get busy living or pack it in.
520f: 85 c7                        sta     death_crack_index ;A-reg=0
5211: 85 c5                        sta     enemy_rev_flags
5213: 85 12                        sta     unit_state
5215: 85 d1                        sta     rez_protect       ;make enemy sleepy
5217: a6 cd                        ldx     game_over_flags   ;is game over?
5219: f0 07                        beq     RespawnPlayer     ;not yet, branch
521b: 85 ce                        sta     play_flag         ;clear the play flag
521d: 85 c8                        sta     recent_coll_flag  ;and the recent-collision flag
521f: 4c 44 53                     jmp     CheckHighScore    ;see if we need to input a high score

                   ; Spawn the player in a random location.
5222: a9 30        RespawnPlayer   lda     #$30              ;don't let enemy make decisions for a few seconds
5224: 85 c4                        sta     move_counter
5226: ad 2a 18                     lda     POKEY_RANDOM      ;set player X/Z to random location
5229: 85 bc                        sta     enemy_turn_to     ;send enemy in a random direction
522b: 85 2d                        sta     unit_pos_z
522d: ad 2a 18                     lda     POKEY_RANDOM
5230: 85 31                        sta     unit_pos_x
5232: ad 2a 18     :PlacePlayer    lda     POKEY_RANDOM
5235: 29 3f                        and     #$3f              ;limit range along Z axis (why?)
5237: 85 2e                        sta     unit_pos_z+1
5239: ad 2a 18                     lda     POKEY_RANDOM
523c: 85 32                        sta     unit_pos_x+1
523e: a2 00                        ldx     #$00              ;check for collision with obstacle or enemy
5240: 20 d6 68                     jsr     CheckObstUnitColl
5243: b0 ed                        bcs     :PlacePlayer      ;collided, re-roll high bytes
5245: 86 c8                        stx     recent_coll_flag  ;clear the collision flag
5247: 86 c9                        stx     enemy_known_flag
5249: ad 2a 18                     lda     POKEY_RANDOM      ;randomize player facing
524c: 85 2a                        sta     plyr_facing_hi
524e: 24 cb                        bit     missile_flag      ;was a missile active?
5250: 10 06                        bpl     :DeathByTank      ;no, we got shot
5252: 20 e8 69                     jsr     CreateTank        ;yes, make sure next enemy is tank
5255: 4c 59 51                     jmp     DrawMore          ; so we don't missile-spam the poor player

5258: a5 14        :DeathByTank    lda     enemy_state       ;still alive?
525a: d0 03                        bne     :TankDead         ;no, killed each other
525c: 20 be 69                     jsr     CreateEnemyUnit   ;create a missile or tank
525f: 4c 59 51     :TankDead       jmp     DrawMore

5262: e6 c7        :IncAndBail     inc     death_crack_index ;advance index (counts by 2)
5264: e6 c7                        inc     death_crack_index
5266: d0 f7                        bne     :TankDead         ;(always)

                   ; 
                   ; If not playing the game, perform an authenticity check.  This is only called
                   ; while we're showing the battlefield in "attract" mode, which generally means
                   ; that the copyright notice is being rendered.  (I think it's drawn first, which
                   ; would make this a check on the copyright string.)
                   ; 
                   ; If we're currently showing the logo, this continues to the code that adds the
                   ; logo to the visible object list.
                   ; 
5268: 24 ce        StrangeCalcB    bit     play_flag         ;are we playing the game?
526a: 30 20                        bmi     :Return           ;yes, skip this
                   ]data_ptr       .var    $3b    {addr/2}

526c: a5 03                        lda     vg_cmd_ptr+1      ;$20 or $28
526e: 29 f8                        and     #$f8              ;(redundant?)
5270: 85 3c                        sta     ]data_ptr+1
5272: a9 02                        lda     #$02              ;commands start at $2002 / $2802
5274: 85 3b                        sta     ]data_ptr
                   ; Subtracts the first 54 command bytes from zero, then subtracts $75, yielding
                   ; $01.  This value is then used when deducting credits, and after a few missiles
                   ; have launched, to modify display rendering.  (See also $510a.)
5276: a0 35                        ldy     #$35
5278: a9 00                        lda     #$00
527a: 38                           sec
527b: f1 3b        :Loop           sbc     (]data_ptr),y
527d: 88                           dey
527e: 10 fb                        bpl     :Loop
5280: 38                           sec
5281: ed 93 74                     sbc     strange_75+1      ;#$75
5284: 8d 70 03                     sta     strange_one_b
5287: 2c 4b 03                     bit     attract_logo_flag ;are we showing the logo?
528a: 30 01                        bmi     :CheckLogo        ;yes, update state
528c: 60           :Return         rts

528d: ad 49 03     :CheckLogo      lda     logo_vis_ypos+1
5290: 30 07                        bmi     ShowLogo
5292: c9 02                        cmp     #$02
5294: 90 03                        bcc     ShowLogo
5296: 4c 1f 53                     jmp     InitLogoAndCtrs

                   ; 
                   ; Adds the logo to the set of visible objects.  It's not part of the
                   ; battlefield, just appended to the list.
                   ; 
5299: a2 00        ShowLogo        ldx     #$00              ;find the end of the list
529b: bd 70 02     :FindLoop       lda     vis_obj_type,x
529e: 30 04                        bmi     :FoundEnd
52a0: e8                           inx
52a1: e8                           inx
52a2: d0 f7                        bne     :FindLoop
52a4: a9 f0        :FoundEnd       lda     #$f0              ;max intensity
52a6: 8d ea 02                     sta     sauc_log_inten    ;set intensity override
52a9: a0 00                        ldy     #$00
52ab: b9 f7 3f     :ObjLoop        lda     logo_objects,y    ;copy object type
52ae: 9d 70 02                     sta     vis_obj_type,x
52b1: ad 46 03                     lda     logo_vis_zpos     ;copy current Z position (low byte)
52b4: 9d 00 02                     sta     vis_obj_zpos,x
52b7: a9 00                        lda     #$00              ;directly ahead (low byte)
52b9: 9d 38 02                     sta     vis_obj_xpos,x
52bc: e8                           inx                       ;move to high byte / facing
52bd: c8                           iny                       ;advance halfway to next logo object
52be: a5 2a                        lda     plyr_facing_hi    ;match player facing
52c0: 9d 70 02                     sta     vis_obj_facing-1,x ;"-1" because we already INX
52c3: ad 47 03                     lda     logo_vis_zpos+1   ;copy current Z position (high byte)
52c6: 9d 00 02                     sta     vis_obj_zpos,x
52c9: a9 00                        lda     #$00              ;place directly ahead (high byte)
52cb: 9d 38 02                     sta     vis_obj_xpos,x
52ce: e8                           inx                       ;next entry in vis table
52cf: c8                           iny                       ;advance to next logo object
                   ; Three-part logo starts at $fc00 (center point below ground), with the 3rd
                   ; "Zone" object mostly behind us.  We don't want to start drawing "Zone" until
                   ; it has progressed a little bit.
52d0: ad 49 03                     lda     logo_vis_ypos+1   ;check height
52d3: 10 10                        bpl     :ShowAll          ;> 0, show all
52d5: c9 fd                        cmp     #$fd
52d7: b0 0c                        bcs     :ShowAll          ;>= $fd00, show all
52d9: ad 48 03                     lda     logo_vis_ypos
52dc: c9 b0                        cmp     #$b0              ;>= $fcb0, show all
52de: b0 05                        bcs     :ShowAll
52e0: c0 03                        cpy     #$03              ;< $fcb0, stop after "Ba"/"ttle"
52e2: 4c e7 52                     jmp     :ObjBranch

52e5: c0 05        :ShowAll        cpy     #$05              ;reached last object?
52e7: 90 c2        :ObjBranch      bcc     :ObjLoop
                   ; Set the altitude for the logo objects, using the same mechanism used for
                   ; missiles and exploding chunks.  The transform code sees that the object has
                   ; type $1x and looks up the Y position from (type & 7).
52e9: ad 48 03                     lda     logo_vis_ypos     ;get the current position
52ec: ac 49 03                     ldy     logo_vis_ypos+1
52ef: 8d e4 02                     sta     ypos_by_type+12   ;copy it for objects +6/+7
52f2: 8c e5 02                     sty     ypos_by_type+13
52f5: 8d e6 02                     sta     ypos_by_type+14
52f8: 8c e7 02                     sty     ypos_by_type+15
52fb: 18                           clc                       ;increase height by 8 units
52fc: 69 08                        adc     #$08
52fe: 8d 48 03                     sta     logo_vis_ypos     ;update low byte
5301: 90 01                        bcc     :NoInc
5303: c8                           iny
5304: 8c 49 03     :NoInc          sty     logo_vis_ypos+1   ;update high byte
5307: 18                           clc                       ;increase distance from viewer by 64 units
5308: ac 47 03                     ldy     logo_vis_zpos+1
530b: ad 46 03                     lda     logo_vis_zpos
530e: 69 40                        adc     #$40
5310: 8d 46 03                     sta     logo_vis_zpos     ;update low byte
5313: 90 01                        bcc     :NoInc
5315: c8                           iny
5316: 8c 47 03     :NoInc          sty     logo_vis_zpos+1   ;update high byte
5319: a9 ff                        lda     #$ff              ;mark end of list
531b: 9d 70 02                     sta     vis_obj_type,x
531e: 60                           rts

                   ; 
                   ; Initializes some logo-display parameters and the frame counters.
                   ; 
531f: a9 fc        InitLogoAndCtrs lda     #$fc              ;YC=-1024
5321: 8d 49 03                     sta     logo_vis_ypos+1
5324: a9 00                        lda     #$00
5326: 8d 46 03                     sta     logo_vis_zpos
5329: 8d 48 03                     sta     logo_vis_ypos
532c: 8d 4b 03                     sta     attract_logo_flag ;clear the show-logo flag
532f: a9 04                        lda     #$04              ;XC=+1024
5331: 8d 47 03                     sta     logo_vis_zpos+1
5334: a9 03                        lda     #$03
5336: 85 d2                        sta     frame_count_256x  ;init counter used as timer
5338: a9 00                        lda     #$00
533a: 8d 3c 03                     sta     hs_list_flag      ;not showing high score list
533d: 85 c6                        sta     frame_counter
533f: 60                           rts

5340: 67                           .dd1    $67               ;(checksum adj)
5341: 45 44 52                     .str    ‘EDR’             ;Ed Rotberg's initials?

                   ; 
                   ; Checks to see if the current score has earned a place on the high scores list.
                   ; If it has, play sound effects and prepare for score entry.
                   ; 
5344: a2 00        CheckHighScore  ldx     #$00
5346: 86 d2                        stx     frame_count_256x  ;init timeout
5348: 8e 3c 03                     stx     hs_list_flag
534b: bc 00 03     :Loop           ldy     hs_scores,x       ;compare high scores to current score
534e: c4 b8                        cpy     score
5350: bd 01 03                     lda     hs_scores+1,x
5353: e5 b9                        sbc     score+1
5355: 90 0a                        bcc     NewHighScore      ;this high score was less than current score
5357: e8                           inx
5358: e8                           inx
5359: e8                           inx
535a: e0 1e                        cpx     #30               ;3 bytes * 10 entries
535c: 90 ed                        bcc     :Loop
535e: 4c 59 51                     jmp     DrawMore          ;didn't find a winner

5361: a9 00        NewHighScore    lda     #$00
5363: 85 c6                        sta     frame_counter
5365: 8d 42 03                     sta     hs_entry_pos
5368: 8e 40 03                     stx     hs_index
536b: 85 c8                        sta     recent_coll_flag  ;clear collision flag
536d: 85 d4                        sta     saucer_dead_intens ;clear saucer dying
536f: 85 de                        sta     saucer_state      ;clear saucer
5371: a9 80                        lda     #$80              ;sound: 1812 Overture
5373: 20 85 79                     jsr     StartSoundEffect
5376: a5 cd                        lda     game_over_flags
5378: 09 80                        ora     #$80              ;set flag to trigger high score initials entry
537a: 85 cd                        sta     game_over_flags
                   ; Make space in the high score table.
537c: a2 1b                        ldx     #27
537e: ec 40 03     :ShiftLoop      cpx     hs_index          ;did we reach the designated position?
5381: f0 23                        beq     :AddScore         ;yes, done sliding
5383: bd fd 02                     lda     hs_scores-3,x     ;no, keep moving things down
5386: 9d 00 03                     sta     hs_scores,x
5389: bd fe 02                     lda     hs_scores-2,x
538c: 9d 01 03                     sta     hs_scores+1,x
538f: bd 1b 03                     lda     hs_initials-3,x
5392: 9d 1e 03                     sta     hs_initials,x
5395: bd 1c 03                     lda     hs_initials-2,x
5398: 9d 1f 03                     sta     hs_initials+1,x
539b: bd 1d 03                     lda     hs_initials-1,x
539e: 9d 20 03                     sta     hs_initials+2,x
53a1: ca                           dex
53a2: ca                           dex
53a3: ca                           dex
53a4: 10 d8                        bpl     :ShiftLoop
53a6: a5 b8        :AddScore       lda     score             ;copy score to high score table
53a8: 9d 00 03                     sta     hs_scores,x
53ab: a5 b9                        lda     score+1
53ad: 9d 01 03                     sta     hs_scores+1,x
53b0: a9 04                        lda     #$04
53b2: 8d 41 03                     sta     hs_entry_spd_ctr
53b5: a9 16                        lda     #$16              ;'A'
53b7: 8d 3d 03                     sta     hs_initials_entry
53ba: a9 4c                        lda     #$4c              ;'-'
53bc: 8d 3e 03                     sta     hs_initials_entry+1
53bf: 8d 3f 03                     sta     hs_initials_entry+2 ;default initials are "A--"
53c2: 4c 59 51                     jmp     DrawMore

                   ; 
                   ; Enter initials for high score list.
                   ; 
                   • Clear variables
                   ]saved_x        .var    $08    {addr/1}

53c5: a5 d2        EnterInitials   lda     frame_count_256x  ;check elapsed time since initials entry started
53c7: c9 04                        cmp     #$04              ;>= ~60 sec?
53c9: b0 77                        bcs     SaveInitials      ;yes, save whatever we have
53cb: a5 ef                        lda     audio_indices     ;playing a sound on channel 1?
53cd: d0 0e                        bne     :NoXplode         ;yes, don't play explosion sound yet
53cf: a6 ba                        ldx     enemy_score       ;have we already played explosion sound?
53d1: f0 0a                        beq     :NoXplode         ;yes, don't play it again
53d3: 85 ba                        sta     enemy_score       ;A-reg=0, reset enemy score
53d5: a9 02                        lda     #$02              ;d-sound: explosion=loud
53d7: 85 15                        sta     dsnd_ctrl_val
53d9: a9 ff                        lda     #$ff              ;play for ~1 sec
53db: 85 0f                        sta     dsnd_expl_ctr
53dd: 20 2e 6d     :NoXplode       jsr     DrawScoreLives
53e0: a2 1e                        ldx     #$1e              ;"great score"
53e2: 20 98 6c                     jsr     DrawString
53e5: a2 08                        ldx     #$08              ;"enter your initials"
53e7: 20 98 6c                     jsr     DrawString
53ea: a2 0a                        ldx     #$0a              ;"change letter with right hand controller"
53ec: 20 98 6c                     jsr     DrawString
53ef: a2 0c                        ldx     #$0c              ;"select letter with fire button"
53f1: 20 98 6c                     jsr     DrawString
53f4: 20 65 54                     jsr     UpdateInitialEntry
                   ; Check to see if the fire button is pressed *and* wasn't pressed last time.
53f7: ad 28 18                     lda     POKEY_ALLPOT      ;get current button state (==1 if pressed)
53fa: 8d 44 03                     sta     hs_btn_read       ;save it
53fd: 4d 43 03                     eor     hs_btn_prev       ;XOR with previous state (==1 if changed)
5400: 2d 44 03                     and     hs_btn_read       ;AND with current state (==1 if pressed + changed)
5403: ae 44 03                     ldx     hs_btn_read       ;(could TAX after the LDA)
5406: 8e 43 03                     stx     hs_btn_prev       ;save for next time
5409: 29 10                        and     #$10              ;mask away the non-button bits
540b: f0 10                        beq     :NoButton
540d: ae 42 03                     ldx     hs_entry_pos      ;button pressed, advance to next field
5410: e0 02                        cpx     #$02              ;all 3 initials entered?
5412: b0 2e                        bcs     SaveInitials      ;yes, save them off
5414: e8                           inx
5415: 8e 42 03                     stx     hs_entry_pos      ;save
5418: a9 16                        lda     #$16              ;'A'
541a: 9d 3d 03                     sta     hs_initials_entry,x ;init next entry
541d: 20 6a 7a     :NoButton       jsr     VgCenter
5420: a0 00                        ldy     #$00
5422: 84 00                        sty     $00               ;?
5424: a9 ee                        lda     #$ee
5426: a2 ee                        ldx     #$ee
5428: 20 8e 7a                     jsr     VgVec8I           ;move to -72,-72
                   ; Print the initials entered so far.
542b: a2 00                        ldx     #$00
542d: 86 08        :PrintLoop      stx     ]saved_x
542f: bd 3d 03                     lda     hs_initials_entry,x
5432: 20 6a 55                     jsr     DrawChar
5435: a6 08                        ldx     ]saved_x
5437: e8                           inx
5438: e0 03                        cpx     #$03
543a: 90 f1                        bcc     :PrintLoop
543c: 20 74 55                     jsr     FinishFrame
543f: 4c 00 50                     jmp     MainLoop

                   ; 
                   ; Saves the player-entered initials into the high score list.  Called after the
                   ; 3rd letter is set, or we time out.
                   ; 
5442: ae 40 03     SaveInitials    ldx     hs_index
5445: a0 00                        ldy     #$00              ;copy player-entered initials into table
5447: b9 3d 03     :CopyLoop       lda     hs_initials_entry,y
544a: 9d 1e 03                     sta     hs_initials,x
544d: c0 02                        cpy     #$02
544f: 90 10                        bcc     :IncXY
                   ; Done with initials, update game mode.
5451: a9 03                        lda     #$03              ;set counter so we show scores for a few seconds
5453: 85 d2                        sta     frame_count_256x
5455: 85 cd                        sta     game_over_flags   ;game over, done with scores
5457: a9 81                        lda     #$81
5459: 8d 3c 03                     sta     hs_list_flag
545c: 85 c6                        sta     frame_counter     ;don't inc the 256x counter for ~8 sec
545e: 4c 00 50                     jmp     MainLoop

5461: e8           :IncXY          inx
5462: c8                           iny
5463: d0 e2                        bne     :CopyLoop         ;(always)

                   ; 
                   ; Applies the state of the right joystick to update one letter of the player's
                   ; initials during high score entry.
                   ; 
                   UpdateInitialEntry
5465: 8d 2b 18                     sta     POKEY_POTGO       ;initiate controller scan
5468: ad 28 18                     lda     POKEY_ALLPOT      ;get results
546b: 29 03                        and     #$03              ;right joystick
546d: d0 06                        bne     :StickMoved
546f: a9 04        :ResetAndRet    lda     #$04              ;wait 4 frames before changing the letter again
5471: 8d 41 03                     sta     hs_entry_spd_ctr  ; so it doesn't spin crazy fast
5474: 60           :Return         rts

5475: ce 41 03     :StickMoved     dec     hs_entry_spd_ctr  ;have we waited long enough since the last letter
5478: d0 fa                        bne     :Return           ; was inc/dec?
547a: ae 42 03                     ldx     hs_entry_pos      ;index of initial (0-2)
547d: bc 3d 03                     ldy     hs_initials_entry,x ;get current letter
5480: 4a                           lsr     A                 ;check low bit
5481: 90 0a                        bcc     :StickFwd         ;stick pressed forward, branch
5483: 88                           dey                       ;advance to next char
5484: 88                           dey
5485: c0 16                        cpy     #$16              ;did we pass 'A'?
5487: b0 0c                        bcs     :NoRoll           ;not yet
5489: a0 4a                        ldy     #$4a              ;' '
548b: d0 08                        bne     :NoRoll           ;(always)

548d: c8           :StickFwd       iny                       ;advance to next char
548e: c8                           iny
548f: c0 4c                        cpy     #$4c              ;did we hit '-'?
5491: 90 02                        bcc     :NoRoll           ;not yet
5493: a0 16                        ldy     #$16              ;back to 'A'
5495: 98           :NoRoll         tya
5496: 9d 3d 03                     sta     hs_initials_entry,x ;update character
5499: d0 d4                        bne     :ResetAndRet      ;(always)

                   ; 
                   ; Draws the high score list.
                   ; 
                   ; Each score is a two-byte BCD value, to which "000" is appended.  So the score
                   ; rolls over at 10 million.  Each score has 3 bytes for initials.  To make
                   ; indexing easier, scores are stored in a field 3 bytes wide; the 3rd byte is
                   ; never used.
                   ; 
                   ; The highest score appears first in memory.
                   ; 
                   • Clear variables
                   ]deltaX         .var    $04    {addr/2}
                   ]deltaY         .var    $06    {addr/2}
                   ]this_score     .var    $0a    {addr/2}

549b: 20 2e 6d     HighScoreList   jsr     DrawScoreLives
549e: a2 1a                        ldx     #$1a              ;"high scores"
54a0: 20 98 6c                     jsr     DrawString
54a3: 20 6a 7a                     jsr     VgCenter          ;move to center
54a6: a0 00                        ldy     #$00              ;beam off
                   ; The rev1 ROM is slightly different from $54a8-543c.  The old ROM would draw
                   ; one tank icon for every 100K points, up to 10.  This ROM only draws one.  It
                   ; also staggers each high score line 4 pixels to the left.
54a8: a9 e0                        lda     #$e0
54aa: a2 1a                        ldx     #$1a
54ac: 20 8e 7a                     jsr     VgVec8I           ;move to -128,+104
54af: a2 00                        ldx     #$00              ;start at first slot
54b1: 8e 40 03                     stx     hs_index
54b4: ae 40 03     :HSLoop         ldx     hs_index          ;get slot number
54b7: bd 00 03                     lda     hs_scores,x       ;get score in this slot
54ba: 85 0a                        sta     ]this_score       ;copy into $0a/0b
54bc: 1d 01 03                     ora     hs_scores+1,x     ;was the score zero?
54bf: f0 7c                        beq     DrawBonusTankStr  ;yes, bail
54c1: bd 01 03                     lda     hs_scores+1,x
54c4: 85 0b                        sta     ]this_score+1
54c6: a9 0a                        lda     #$0a
54c8: 20 9e 7b                     jsr     DrawFourDigits    ;draw $0a/0b as 4-digit BCD value
54cb: a2 1c                        ldx     #$1c              ;"000 "
54cd: 20 98 6c                     jsr     DrawString
54d0: ae 40 03                     ldx     hs_index          ;print initials
54d3: bd 1e 03                     lda     hs_initials,x
54d6: 20 6a 55                     jsr     DrawChar
54d9: ae 40 03                     ldx     hs_index
54dc: bd 1f 03                     lda     hs_initials+1,x
54df: 20 6a 55                     jsr     DrawChar
54e2: ae 40 03                     ldx     hs_index
54e5: bd 20 03                     lda     hs_initials+2,x
54e8: 20 6a 55                     jsr     DrawChar
                   ; Add a tank icon for scores >= 100K.
54eb: a0 00                        ldy     #$00              ;cmd index
54ed: ae 40 03                     ldx     hs_index
54f0: bd 01 03                     lda     hs_scores+1,x     ;check second byte
54f3: f0 1a                        beq     :NoTank           ;< 100K, skip this part
54f5: ad f0 33                     lda     vg_glyph_calls    ;draw a ' '
54f8: 91 02                        sta     (vg_cmd_ptr),y
54fa: c8                           iny
54fb: ad f1 33                     lda     vg_glyph_calls+1
54fe: 91 02                        sta     (vg_cmd_ptr),y
5500: c8                           iny
5501: ad 90 35                     lda     vg_life_icon      ;draw a tank icon
5504: 91 02                        sta     (vg_cmd_ptr),y
5506: c8                           iny
5507: ad 91 35                     lda     vg_life_icon+1
550a: 91 02                        sta     (vg_cmd_ptr),y
550c: 20 76 7a                     jsr     VgPtrAddY         ;advance command pointer
550f: ae 40 03     :NoTank         ldx     hs_index
5512: e0 1b                        cpx     #27               ;was this the last entry?
5514: b0 27                        bcs     DrawBonusTankStr  ;yes, bail
5516: e8                           inx                       ;advance to next entry
5517: e8                           inx
5518: e8                           inx
5519: 8e 40 03                     stx     hs_index          ;save index
                   ; Position the beam for the next entry.
551c: a9 d8                        lda     #$d8              ;deltaY=-40 (move toward bottom)
551e: 85 06                        sta     ]deltaY
5520: a9 ff                        lda     #$ff
5522: 85 07                        sta     ]deltaY+1
5524: a9 fe                        lda     #$fe              ;deltaX negative to move back toward left
5526: 85 05                        sta     ]deltaX+1         ;"SSSS000 III" = 11 chars
5528: 98                           tya                       ;Y-reg nonzero if we added a tank icon
5529: f0 04                        beq     :NoTank2
552b: a9 a3                        lda     #$a3              ;deltaX=-349 (268 + 24 for ' ' + 57 for tank icon)
552d: d0 02                        bne     :GotX

552f: a9 f4        :NoTank2        lda     #$f4              ;deltaX=-268 (24*11 + 4), skews to left
5531: 85 04        :GotX           sta     ]deltaX
5533: a9 00                        lda     #$00              ;beam off
5535: 85 01                        sta     vg_intensity
5537: 20 ab 7a                     jsr     VgVector          ;move to position
553a: 4c b4 54                     jmp     :HSLoop

                   ; 
                   ; Draws the string that indicates when bonus tanks are provided.
                   ; 
                   DrawBonusTankStr
553d: ad 00 0a                     lda     DSW0              ;get DIP switch setting
5540: 4a                           lsr     A                 ;shift to get bonus tank setting
5541: 4a                           lsr     A
5542: 4a                           lsr     A
5543: 4a                           lsr     A
5544: 29 03                        and     #$03              ;0-3
5546: f0 1c                        beq     :NoBonus          ;no bonus tank awarded, don't show string
5548: aa                           tax                       ;1-3
5549: bd 86 38                     lda     bonus_tank_score,x ;get score
554c: 18                           clc
554d: f8                           sed
554e: 69 01                        adc     #$01
5550: d8                           cld
5551: 85 13                        sta     $13               ;temp storage?
5553: a2 2a                        ldx     #$2a              ;"bonus tank at "
5555: 20 98 6c                     jsr     DrawString
5558: a9 13                        lda     #$13              ;print value from $13
555a: a0 01                        ldy     #$01
555c: 20 a0 7b                     jsr     DrawNDigits
555f: a2 2c                        ldx     #$2c              ;"000 and 100000"
5561: 20 98 6c                     jsr     DrawString
5564: 20 74 55     :NoBonus        jsr     FinishFrame
5567: 4c 00 50                     jmp     MainLoop

                   ; 
                   ; Adds a single character glyph to the command list.
                   ; 
                   ; On entry:
                   ;   A-reg: character index * 2
                   ; 
556a: a8           DrawChar        tay
556b: be f1 33                     ldx     vg_glyph_calls+1,y
556e: b9 f0 33                     lda     vg_glyph_calls,y
5571: 4c 6e 7a                     jmp     VgOutputCmd

                   ; 
                   ; Puts finishing touches on the current display frame.
                   ; 
5574: 20 1d 7a     FinishFrame     jsr     VgFinishList      ;add CNTR and HALT
5577: a9 00                        lda     #$00
5579: 85 28                        sta     vg_write_addr_hi  ;tell NMI handler that we're done adding vectors
557b: 24 ce                        bit     play_flag         ;showing game play?
557d: 10 0f                        bpl     :Return           ;no (high scores), branch
557f: ad ec 02                     lda     missile_count     ;have we launched a missile yet?
5582: 30 0a                        bmi     :Return           ;no, skip strange
5584: c9 04                        cmp     #$04              ;have we fired more than 4? (prev test redundant?)
5586: 90 06                        bcc     :Return           ;no, skip strange
5588: ad 70 03                     lda     strange_one_b     ;always 1
558b: 8d 00 20                     sta     VEC_GEN_RAM       ;low byte of VJMP instruction should always be 1
558e: 60           :Return         rts

                   ; 
                   ; Handle a non-maskable interrupt (NMI).
                   ; 
                   ; From the MAME sources:
                   ;   m_maincpu->set_periodic_int(... BZONE_CLOCK_3KHZ / 12))
                   ; That means we get interrupted 250x per second.
                   ; 
                   ; The NMI handler manages a number of things, including:
                   ;  - Handling coin drops and the "start" button.
                   ;  - Initializing state for a new game.
                   ;  - Flipping the AVG display/write pointers.
                   ;  - Initiating display updates.
                   ;  - Updating sound effects.
                   ; 
                   ; Three counters are updated every time through:
                   ;  - $3a: counter, incremented every time.  Among other things, this is used to
                   ; flash text, e.g. BVC to toggle state 4x/second, BPL to toggle state 2x/second.
                   ;  - $39: counter, incremented every 16 interrupts.  Used to pace the game
                   ; update code, which runs at 250/16=15.625 fps.
                   ;  - $ed: counter, decremented from 6.  When it hits zero we restart the vector
                   ; state machine, which was hopefully HALTed.  This causes a screen refresh every
                   ; 250/6=41.7 fps.
                   ; 
                   • Clear variables

558f: 48           HandleNMI       pha                       ;preserve A-reg
5590: 8a                           txa
5591: ba                           tsx                       ;check stack pointer
5592: 30 0b                        bmi     :HiStack          ;should always be $00-7f; higher = stack overflow
5594: 48                           pha                       ;preserve X-reg
5595: e6 3a                        inc     nmi_count         ;increment the counter
5597: a5 3a                        lda     nmi_count
5599: 29 0f                        and     #$0f
559b: d0 04                        bne     :Not16
559d: e6 39                        inc     nmi_count_x16     ;every 16 times, increment this counter
                   ; If the N flag is set we've overflowed the stack or the NMI x16 counter.  The
                   ; counter will only overflow if the non-interrupt code is stuck.
559f: 30 fe        :HiStack        bmi     :HiStack          ;spin until watchdog bites
55a1: 98           :Not16          tya
55a2: 48                           pha                       ;preserve Y-reg
55a3: d8                           cld                       ;always a good idea on 6502
55a4: 8d 00 14                     sta     WATCHDOG_CLEAR    ;things look good, feed the dog
                   ; Do some stuff with the coin counter.
55a7: ad 00 0c                     lda     DSW1              ;get DIP switch settings
55aa: 85 eb                        sta     latched_dsw1      ;save for later
55ac: 20 c3 56                     jsr     CoinCounterStuff
55af: a5 e0                        lda     coin_ctr_state1
55b1: 2a                           rol     A
55b2: a5 e1                        lda     coin_ctr_state1+1
55b4: 2a                           rol     A
55b5: 2a                           rol     A
55b6: 48                           pha
55b7: a5 e2                        lda     coin_ctr_state1+2
55b9: 2a                           rol     A
55ba: 68                           pla
55bb: 2a                           rol     A
55bc: 8d 00 10                     sta     COIN_COUNTER
                   ; Update POKEY sounds.
                   ; 
                   ; The saucer noises are looped, so we want to restart them whenever the channel
                   ; goes quiet.
55bf: 20 b9 79                     jsr     UpdateSoundEffect ;update sound effects on POKEY channels 1 & 2
55c2: 2c 45 03                     bit     score_100k_flag   ;are we playing the 1812 Oveture?
55c5: 30 1d                        bmi     :Cont             ;yes, suppress saucer sounds
55c7: a5 ef                        lda     audio_indices     ;is anything playing on channel 1?
55c9: d0 19                        bne     :Cont             ;yes, don't make saucer noise
55cb: a5 d4                        lda     saucer_dead_intens ;saucer dying?
55cd: f0 04                        beq     :NoDeadSaucer     ;no, move on
55cf: a9 20                        lda     #$20              ;sound: saucer killed
55d1: d0 0e                        bne     :PlaySound        ;(always)

55d3: a5 c8        :NoDeadSaucer   lda     recent_coll_flag  ;recent collision?
55d5: f0 04                        beq     :NoRecentColl     ;no, branch
55d7: a9 04                        lda     #$04              ;sound: merp
55d9: d0 06                        bne     :PlaySound        ;(always)

55db: 24 de        :NoRecentColl   bit     saucer_state      ;is saucer in stealth mode?
55dd: 10 05                        bpl     :Cont             ;yes, skip sound
55df: a9 40                        lda     #$40              ;sound: saucer alive
55e1: 20 85 79     :PlaySound      jsr     StartSoundEffect
                   ; Update discrete sounds.  The value in $15 holds the bits for cannon volume,
                   ; explosion volume, and engine rev up/down.  The bits for actually playing
                   ; cannon and explosion sounds are set here, based on countdown timers.
55e4: a5 0f        :Cont           lda     dsnd_expl_ctr     ;are we playing the explosion sound?
55e6: f0 0a                        beq     :NoXplode         ;no, branch
55e8: c6 0f                        dec     dsnd_expl_ctr     ;yes, decrement the play counter
55ea: d0 04                        bne     :IsExploding      ;still going, branch
55ec: a9 00                        lda     #$00              ;start with zero
55ee: f0 02                        beq     :NoXplode         ;(always)

55f0: a9 01        :IsExploding    lda     #$01              ;d-sound: explosion
55f2: a8           :NoXplode       tay                       ;save A-reg
55f3: a5 bd                        lda     dsnd_cnon_ctr     ;are we playing the cannon firing sound?
55f5: f0 08                        beq     :NoCannon         ;no, branch
55f7: c6 bd                        dec     dsnd_cnon_ctr     ;yes, decrement the play counter
55f9: f0 04                        beq     :NoCannon         ;hit zero, no sound
55fb: 98                           tya                       ;restore A-reg
55fc: 09 04                        ora     #$04              ;d-sound: cannon fired
55fe: a8                           tay
55ff: 98           :NoCannon       tya
5600: 05 15                        ora     dsnd_ctrl_val     ;merge with engine rev, volume bits
5602: 24 ce                        bit     play_flag         ;are we playing the game?
5604: 10 05                        bpl     :NotPlaying       ;no, branch
5606: 09 a0                        ora     #$a0              ;d-sound: enable engine sound + un-mute
5608: 4c 94 56                     jmp     :SetDSound

560b: 24 cd        :NotPlaying     bit     game_over_flags   ;is the player entering initials?
560d: 30 7e                        bmi     :EnterInitials    ;yes, branch
                   ; Play a sound if a slam was detected.
560f: a9 00                        lda     #$00              ;assume no sound
5611: a6 ea                        ldx     slam_counter      ;recent slam?
5613: f0 0d                        beq     :NoSlam           ;no, skip this
5615: 09 20                        ora     #$20              ;d-sound: un-mute
5617: a6 ef                        ldx     audio_indices     ;is a sound playing on channel 1?
5619: d0 07                        bne     :NoSlam           ;yes, leave it be
561b: 48                           pha                       ;save A-reg (value=$20)
561c: a9 20                        lda     #$20              ;sound: saucer hit
561e: 20 85 79                     jsr     StartSoundEffect  ;play it (on channel 1)
5621: 68                           pla                       ;restore A-reg
                   ; 
5622: 48           :NoSlam         pha                       ;save d-sound value in A-reg ($00 or $20)
5623: a5 eb                        lda     latched_dsw1      ;get DIP switch setting
5625: 29 03                        and     #%00000011        ;play cost
5627: f0 04                        beq     :FreePlay         ;free play, branch
5629: a5 0e                        lda     credit_count      ;check number of credits purchased
562b: f0 64                        beq     :NoCredits        ;no credits, no play
562d: a5 c6        :FreePlay       lda     frame_counter     ;get the 15.625Hz game frame counter
562f: 4a                           lsr     A                 ;shift bit 1 into carry
5630: 4a                           lsr     A                 ;toggle ~8x / sec
5631: 68                           pla                       ;restore d-sound value
5632: 90 02                        bcc     :NoLed
5634: 09 40                        ora     #$40              ;d-sound: light the "start" LED
5636: 8d 2b 18     :NoLed          sta     POKEY_POTGO       ;start pot scan sequence
5639: 8d 40 18                     sta     DSOUND_CTRL       ;set LED + mute or un-mute audio
                   ; Check to see if the "start" button has been pressed.
563c: ad 28 18                     lda     POKEY_ALLPOT      ;get inputs
563f: 29 20                        and     #%00100000        ;start 1 button (start 2 is not used)
5641: f0 54                        beq     :ChkRefresh       ;not pressed, move on
                   ; Start a new game.
5643: a9 ff                        lda     #$ff              ;reset missile count
5645: 8d ec 02                     sta     missile_count     ;$ff = no missiles launched yet
5648: 85 ce                        sta     play_flag         ;set play-mode flag
564a: a5 0e                        lda     credit_count      ;deduct one play credit
564c: 38                           sec
564d: ed 70 03                     sbc     strange_one_b     ;players will be annoyed if this isn't 1
5650: 85 0e                        sta     credit_count
5652: a9 00                        lda     #$00
5654: 8d 2f 18                     sta     POKEY_SKCTL       ;disable input
5657: 85 ec                        sta     bonus_coin_count  ;reset bonus coin counter
5659: 8d 4b 03                     sta     attract_logo_flag ;not showing the logo
565c: 8d 40 18                     sta     DSOUND_CTRL       ;disable all sound
565f: 85 b8                        sta     score             ;reset score
5661: 85 b9                        sta     score+1
5663: 85 bb                        sta     enemy_score+1     ;reset enemy score
5665: 85 ba                        sta     enemy_score
5667: 85 c4                        sta     move_counter      ;reset enemy movement
5669: 85 26                        sta     projectile_state_1
566b: 85 cd                        sta     game_over_flags   ;game not over
566d: 85 d2                        sta     frame_count_256x  ;reset counter used for timing
566f: 85 d4                        sta     saucer_dead_intens
5671: 85 de                        sta     saucer_state      ;reset saucer
5673: 85 2a                        sta     plyr_facing_hi    ;init player facing to zero (toward moon)
5675: 85 2e                        sta     unit_pos_z+1      ;place player at approximately (0,0)
5677: 85 32                        sta     unit_pos_x+1      ;(we don't zero the low bytes)
5679: a9 07                        lda     #$07              ;KB debounce, KB scan, fast pot scan
567b: 8d 2f 18                     sta     POKEY_SKCTL
567e: 20 be 69                     jsr     CreateEnemyUnit   ;make a bad guy
5681: 18                           clc
5682: ad 00 0a                     lda     DSW0              ;get DIP switch setting
5685: 29 03                        and     #%00000011        ;player life config
5687: 69 02                        adc     #$02              ;add two to raw value
5689: 85 cc                        sta     player_lives      ;set number of player lives
568b: 10 0a                        bpl     :ChkRefresh       ;(always)

568d: 09 60        :EnterInitials  ora     #$60              ;d-sound: un-mute, and light up the "start" button
568f: d0 03                        bne     :SetDSound        ;(always)

5691: 68           :NoCredits      pla
5692: 09 40                        ora     #$40              ;d-sound: light up the "start" button
5694: 8d 40 18     :SetDSound      sta     DSOUND_CTRL
                   ; From the MAME sources:
                   ;   m_screen->set_refresh_hz(BZONE_CLOCK_3KHZ / 12 / 6)
                   ; Redrawing every 6 interrupts gives a 41.7 Hz refresh rate.
5697: c6 ed        :ChkRefresh     dec     nmi_refresh_ctr   ;time to refresh the screen?
5699: d0 22                        bne     :Bail             ;not yet
569b: 8d 00 16                     sta     VEC_GEN_RESET     ;reset the VSM (hopefully HALTed)
569e: a5 28                        lda     vg_write_addr_hi  ;are we ready to flip command list?
56a0: d0 14                        bne     :NoFlip           ;not yet, branch
56a2: ad 01 20                     lda     VEC_GEN_RAM+1     ;yes, flip to the other page
56a5: 49 04                        eor     #$04
56a7: 8d 01 20                     sta     VEC_GEN_RAM+1
56aa: 29 04                        and     #$04              ;which one was it?
56ac: d0 04                        bne     :Write2000        ;now reading $2800, so write $2000
56ae: a9 28                        lda     #<VEC_GEN_RAM+40  ;now reading $2000, so write $2800
56b0: d0 02                        bne     :Cont

56b2: a9 20        :Write2000      lda     #>VEC_GEN_RAM
56b4: 85 28        :Cont           sta     vg_write_addr_hi  ;save the new write-to addr
56b6: 8d 00 12     :NoFlip         sta     VEC_GEN_START     ;start the vector engine
56b9: a9 06                        lda     #$06              ;reset counter
56bb: 85 ed                        sta     nmi_refresh_ctr
56bd: 68           :Bail           pla                       ;restore A/X/Y
56be: a8                           tay
56bf: 68                           pla
56c0: aa                           tax
56c1: 68                           pla
56c2: 40                           rti                       ;return from NMI handler

                   ; 
                   ; Check for coin drops.  We check each of the three bits in IN0 in turn.
                   ; 
                   ; (this executes in the NMI context)
                   ; 
                   CoinCounterStuff
56c3: a2 02                        ldx     #$02              ;loop through 3x
56c5: ad 00 08     :Loop           lda     IN0
56c8: e0 01                        cpx     #$01
56ca: f0 03                        beq     :TwoShift         ;X-reg=1, shift twice
56cc: b0 02                        bcs     :OneShift         ;X-reg=2, shift once
56ce: 4a                           lsr     A                 ;X-reg=0, shift thrice
56cf: 4a           :TwoShift       lsr     A
56d0: 4a           :OneShift       lsr     A                 ;goal is to get bit 0/1/2 into carry
56d1: b5 e7                        lda     coin_ctr_state3,x
56d3: 29 1f                        and     #%00011111
56d5: b0 37                        bcs     L570E
56d7: f0 10                        beq     L56E9
56d9: c9 1b                        cmp     #$1b
56db: b0 0a                        bcs     L56E7
56dd: a8                           tay
56de: a5 3a                        lda     nmi_count
56e0: 29 07                        and     #$07
56e2: c9 07                        cmp     #$07
56e4: 98                           tya
56e5: 90 02                        bcc     L56E9
56e7: e9 01        L56E7           sbc     #$01
56e9: 95 e7        L56E9           sta     coin_ctr_state3,x
                   ; Check the slam switch.
56eb: ad 00 08                     lda     IN0
56ee: 29 08                        and     #%00001000        ;slam switch active?
56f0: d0 04                        bne     :SlamNotSet       ;no, branch
56f2: a9 f0                        lda     #240              ;yes, set counter to 80*3
56f4: 85 ea                        sta     slam_counter
56f6: a5 ea        :SlamNotSet     lda     slam_counter      ;previous slam?
56f8: f0 08                        beq     :NoSlam           ;no, carry on
56fa: c6 ea                        dec     slam_counter      ;decrement counter
56fc: a9 00                        lda     #$00
56fe: 95 e7                        sta     coin_ctr_state3,x ;zero these out
5700: 95 e4                        sta     coin_ctr_state2,x
5702: 18           :NoSlam         clc
                   ; 
5703: b5 e4                        lda     coin_ctr_state2,x
5705: f0 23                        beq     L572A
5707: d6 e4                        dec     coin_ctr_state2,x
5709: d0 1f                        bne     L572A
570b: 38                           sec
570c: b0 1c                        bcs     L572A             ;(always)

570e: c9 1b        L570E           cmp     #$1b
5710: b0 09                        bcs     L571B
5712: b5 e7                        lda     coin_ctr_state3,x
5714: 69 20                        adc     #$20
5716: 90 d1                        bcc     L56E9
5718: f0 01                        beq     L571B
571a: 18                           clc
571b: a9 1f        L571B           lda     #$1f
571d: b0 ca                        bcs     L56E9
571f: 95 e7                        sta     coin_ctr_state3,x
5721: b5 e4                        lda     coin_ctr_state2,x
5723: f0 01                        beq     L5726
5725: 38                           sec
5726: a9 78        L5726           lda     #$78
5728: 95 e4                        sta     coin_ctr_state2,x
572a: 90 2a        L572A           bcc     L5756
572c: a9 00                        lda     #$00
572e: e0 01                        cpx     #$01
5730: 90 16                        bcc     L5748
5732: f0 0c                        beq     L5740
5734: a5 eb                        lda     latched_dsw1      ;get DIP siwtch setting
5736: 29 0c                        and     #$0c              ;right coin counter mechanism
5738: 4a                           lsr     A
5739: 4a                           lsr     A
573a: f0 0c                        beq     L5748
573c: 69 02                        adc     #$02
573e: d0 08                        bne     L5748
5740: a5 eb        L5740           lda     latched_dsw1      ;get DIP switch setting
5742: 29 10                        and     #$10              ;center coin counter mechanism
5744: f0 02                        beq     L5748
5746: a9 01                        lda     #$01
5748: 38           L5748           sec
5749: 48                           pha
574a: 65 ec                        adc     bonus_coin_count
574c: 85 ec                        sta     bonus_coin_count
574e: 68                           pla
574f: 38                           sec
5750: 65 21                        adc     coins_inserted
5752: 85 21                        sta     coins_inserted
5754: f6 e0                        inc     coin_ctr_state1,x
5756: ca           L5756           dex
5757: 30 03                        bmi     :CheckBonus
5759: 4c c5 56                     jmp     :Loop

                   ; Check to see if the number of coins inserted entitles the player to a bonus
                   ; coin (NOT a bonus credit).  The coin count in $ec is only used for this check,
                   ; and is reset to zero when the game starts.
575c: a5 eb        :CheckBonus     lda     latched_dsw1      ;get DIP switch setting
575e: 4a                           lsr     A                 ;right-shift 5x to get top 3 bits (bonus coins)
575f: 4a                           lsr     A
5760: 4a                           lsr     A
5761: 4a                           lsr     A
5762: 4a                           lsr     A
5763: a8                           tay
5764: a5 ec                        lda     bonus_coin_count  ;compute (coins inserted) - (coins needed for bonus)
5766: 38                           sec
5767: f9 78 57                     sbc     bonus_coin_req,y
576a: 30 14                        bmi     :Cont             ;not enough, bail
576c: 85 ec                        sta     bonus_coin_count  ;reduce bonus coin count
576e: e6 21                        inc     coins_inserted    ;increase coin count
5770: c0 03                        cpy     #$03              ;setting 3?
5772: d0 0c                        bne     :Cont
5774: e6 21                        inc     coins_inserted    ;yes, increase coin count again
5776: d0 08                        bne     :Cont             ;(always)

                   ;  001 = for every 2 coins, +1 coin
                   ;  010 = for every 4 coins, +1 coin
                   ;  011 = for every 4 coins, +2 coins
                   ;  100 = for every 5 coins, +1 coin
5778: 7f 02 04 04+ bonus_coin_req  .bulk   $7f,$02,$04,$04,$05,$7f,$7f,$7f

                   ; Compute the number of coins required for 1 credit.
5780: a5 eb        :Cont           lda     latched_dsw1      ;get DIP switch setting
5782: 29 03                        and     #$03              ;get play cost bits
5784: a8                           tay
5785: f0 12                        beq     :UpdateCoins      ;00=free play, branch
5787: 4a                           lsr     A                 ;convert 01/10 to 1, 11 to 2
5788: 69 00                        adc     #$00
578a: 49 ff                        eor     #$ff              ;negate (invert and add one with SEC)
578c: 38                           sec
578d: 65 21                        adc     coins_inserted    ;add -1 or -2 to number of coins inserted
578f: 90 0a                        bcc     :CoinsDone        ;not enough for a play credit
5791: c0 02                        cpy     #$02              ;set to 10 or 11?
5793: b0 02                        bcs     :OnePlay          ;yes, one play per credit; branch
5795: e6 0e                        inc     credit_count      ;no, must be 01 -> 2 plays for 1 coin
5797: e6 0e        :OnePlay        inc     credit_count
5799: 85 21        :UpdateCoins    sta     coins_inserted
                   ; 
579b: a5 3a        :CoinsDone      lda     nmi_count         ;do this every other time (125x/sec)
579d: 4a                           lsr     A
579e: b0 27                        bcs     :Return
                   ; 
                   ; Do things with the coin counter.
                   ; 
57a0: a0 00                        ldy     #$00
57a2: a2 02                        ldx     #$02
57a4: b5 e0        :CCLoop1        lda     coin_ctr_state1,x
57a6: f0 09                        beq     :CCLoop1Next
57a8: c9 10                        cmp     #$10
57aa: 90 05                        bcc     :CCLoop1Next
57ac: 69 ef                        adc     #$ef
57ae: c8                           iny
57af: 95 e0                        sta     coin_ctr_state1,x
57b1: ca           :CCLoop1Next    dex
57b2: 10 f0                        bpl     :CCLoop1
57b4: 98                           tya
57b5: d0 10                        bne     :Return
57b7: a2 02                        ldx     #$02
57b9: b5 e0        :CCLoop2        lda     coin_ctr_state1,x
57bb: f0 07                        beq     :CCLoop2Next
57bd: 18                           clc
57be: 69 ef                        adc     #$ef
57c0: 95 e0                        sta     coin_ctr_state1,x
57c2: 30 03                        bmi     :Return
57c4: ca           :CCLoop2Next    dex
57c5: 10 f2                        bpl     :CCLoop2
57c7: 60           :Return         rts

                   ; 
                   ; Draws the background (mountains/moon, volcano particles, horizon line).
                   ; 
                   ; This is all 2D stuff, using screen coordinates.
                   ; 
                   ; On entry:
                   ;   $01: 0 (intensity)
                   ; 
                   • Clear variables
                   ]deltaX         .var    $04    {addr/2}
                   ]deltaY         .var    $06    {addr/2}
                   ]tmp            .var    $08    {addr/1}

57c8: a2 00        DrawBackground  ldx     #$00
57ca: 86 07                        stx     ]deltaY+1         ;horizon line is at horizontal center of screen
57cc: 86 06                        stx     ]deltaY
57ce: a5 27                        lda     plyr_facing_lo    ;$00/80
57d0: 85 04                        sta     ]deltaX
57d2: a5 2a                        lda     plyr_facing_hi    ;$00-ff
57d4: 4a                           lsr     A                 ;the facing angle is a 9-bit value shifted all the
57d5: 66 04                        ror     ]deltaX           ; way left; right-shift it 4x
57d7: 4a                           lsr     A                 ;HHHHHHHH L0000000 -> 0000HHHH HHHHL000
57d8: 66 04                        ror     ]deltaX           ;(background is 512*8=4096 units wide; there are 512
57da: 4a                           lsr     A                 ; facing angles; so shift 8 pixels per angle)
57db: 66 04                        ror     ]deltaX
57dd: 4a                           lsr     A
57de: 66 04                        ror     ]deltaX
57e0: 48                           pha                       ;save for segment number (want top 3 bits, x2)
57e1: 29 01                        and     #$01              ;reduce high byte to 0-1
57e3: 09 02                        ora     #$02              ;now 2-3 (-> 512 - 1023)
57e5: 85 05                        sta     ]deltaX+1         ;(this is how far right we move initially)
57e7: a5 df                        lda     horizon_adj       ;check horizon adjustment
57e9: 4a                           lsr     A                 ;(shifts screen when player is hit)
57ea: 4a                           lsr     A                 ;divide by 16 to get shift distance
57eb: 4a                           lsr     A
57ec: 4a                           lsr     A
57ed: f0 0d                        beq     :NoAdjust         ;nothing to do
57ef: 85 08                        sta     ]tmp              ;save it
57f1: 38                           sec
57f2: a5 06                        lda     ]deltaY           ;(we know this is zero; could just negate $08)
57f4: e5 08                        sbc     ]tmp
57f6: 85 06                        sta     ]deltaY
57f8: a9 ff                        lda     #$ff              ;sign-extend
57fa: 85 07                        sta     ]deltaY+1
57fc: 20 6a 7a     :NoAdjust       jsr     VgCenter          ;move to center
57ff: 20 ab 7a                     jsr     VgVector          ;move an appropriate distance to the right
5802: a9 30                        lda     #>vg_horizon_line ;copy a VJSR to $3000 instruction; code at $3000
5804: a2 00                        ldx     #<vg_horizon_line ; draws an intensity=6 line to dx=-1536
5806: 20 5a 7a                     jsr     VgJsrToAddr       ; so now we're 512*3 units to the left
5809: 68                           pla                       ;get initial segment number * 2
580a: 49 0f                        eor     #$0f              ;reverse order
580c: 20 a7 58                     jsr     DrawBackSeg       ;draw 3 segments to guarantee coverage
580f: 20 a7 58                     jsr     DrawBackSeg       ; (512 units each)
5812: 20 a7 58                     jsr     DrawBackSeg
5815: a5 27                        lda     plyr_facing_lo    ;init this for volcano code
5817: 85 04                        sta     ]deltaX
                   ; Volcano particles are visible from angles $39 through $87.
5819: a5 2a                        lda     plyr_facing_hi
581b: c9 88                        cmp     #$88
581d: b0 04                        bcs     :Return
581f: c9 39                        cmp     #$39
5821: b0 01                        bcs     DrawVolParts
5823: 60           :Return         rts

                   ; 
                   ; Draws volcano particles.
                   ; 
                   ; The right side of the top of the volcano is at the right edge of segment N. 
                   ; The particle source point is at angle $60.
                   ; 
                   ; On entry:
                   ;   A-reg: ($2a) (player facing, high byte)
                   ;   $04: ($27) player facing, low byte
                   ;   $06/07: adjusted horizon Y coordinate
                   ; 
5824: 4a           DrawVolParts    lsr     A                 ;same shift we did above
5825: 66 04                        ror     ]deltaX           ;HHHHHHHH L0000000 -> 0000HHHH HHHHL000
5827: 4a                           lsr     A
5828: 66 04                        ror     ]deltaX
582a: 4a                           lsr     A
582b: 66 04                        ror     ]deltaX
582d: 4a                           lsr     A
582e: 66 04                        ror     ]deltaX
5830: 85 05                        sta     ]deltaX+1
5832: a5 04                        lda     ]deltaX           ;subtract $605 (would be $6050 before the shift;
5834: 38                           sec                       ; particle spawn point is at facing angle $60)
5835: e9 05                        sbc     #$05
5837: 85 04                        sta     ]deltaX
5839: a5 05                        lda     ]deltaX+1
583b: e9 06                        sbc     #$06
583d: 85 05                        sta     ]deltaX+1
583f: 18                           clc                       ;the background code may have moved the horizon
5840: a9 5e                        lda     #94               ; off of zero, so start with the base value
5842: 65 06                        adc     ]deltaY           ; add 94 to reach the top of the volcano
5844: 85 06                        sta     ]deltaY
5846: 90 02                        bcc     :NoInc
5848: e6 07                        inc     ]deltaY+1
584a: 20 6a 7a     :NoInc          jsr     VgCenter          ;move beam to center
584d: 20 ab 7a                     jsr     VgVector          ;move beam to particle origin
5850: a9 00                        lda     #$00              ;set this as relative origin
5852: 85 35                        sta     cur_beam_xc
5854: 85 36                        sta     cur_beam_xc+1
5856: 85 37                        sta     cur_beam_yc
5858: 85 38                        sta     cur_beam_yc+1
585a: a2 04                        ldx     #$04              ;loop through 5 particles
585c: bd 4d 03     :Loop           lda     volpart_ttl,x     ;active?
585f: f0 42                        beq     :LoopEnd          ;no, skip it
5861: bd 5c 03                     lda     volpart_xc_lo,x   ;get X coordinate
5864: 38                           sec                       ;compute delta from current position
5865: a8                           tay
5866: e5 35                        sbc     cur_beam_xc
5868: 85 04                        sta     ]deltaX           ;set value for vector command
586a: 84 35                        sty     cur_beam_xc       ;update current position
586c: bd 61 03                     lda     volpart_xc_hi,x   ;repeat for high byte
586f: a8                           tay
5870: e5 36                        sbc     cur_beam_xc+1
5872: 85 05                        sta     ]deltaX+1
5874: 84 36                        sty     cur_beam_xc+1
5876: 38                           sec                       ;repeat for Y coordinate low/high
5877: bd 66 03                     lda     volpart_yc_lo,x
587a: a8                           tay
587b: e5 37                        sbc     cur_beam_yc
587d: 85 06                        sta     ]deltaY
587f: 84 37                        sty     cur_beam_yc
5881: bd 6b 03                     lda     volpart_yc_hi,x
5884: a8                           tay
5885: e5 38                        sbc     cur_beam_yc+1
5887: 85 07                        sta     ]deltaY+1
5889: 84 38                        sty     cur_beam_yc+1
588b: a9 00                        lda     #$00              ;beam off
588d: 85 01                        sta     vg_intensity
588f: 86 08                        stx     ]tmp              ;save X-reg
5891: 20 ab 7a                     jsr     VgVector          ;move beam to particle position
5894: a6 08                        ldx     ]tmp
5896: bd 4d 03                     lda     volpart_ttl,x     ;get lifetime counter (0-31)
5899: 0a                           asl     A                 ;use high 3 bits as intensity
589a: 0a                           asl     A
589b: 0a                           asl     A
589c: 29 e0                        and     #%11100000        ;mask intensity bits
589e: 20 8a 7a                     jsr     VgDrawPoint       ;draw a point
58a1: a6 08                        ldx     ]tmp              ;restore X-reg
58a3: ca           :LoopEnd        dex
58a4: 10 b6                        bpl     :Loop
58a6: 60                           rts

                   ; 
                   ; Draws one segment of the landscape background.  Updates state.
                   ; 
                   ; On entry:
                   ;   A-reg: index of segment to draw, * 2
                   ;   Beam positioned at background start
                   ; 
                   ; On exit:
                   ;   A-reg: index of next segment, * 2
                   ;   Beam positioned at next background start
                   ; 
58a7: 29 0e        DrawBackSeg     and     #$0e              ;$00-0e, even values only (8 entries)
58a9: a8                           tay
58aa: b9 06 30                     lda     vg_landscape,y    ;get a VJSR command from ROM table
58ad: c8                           iny
58ae: be 06 30                     ldx     vg_landscape,y
58b1: c8                           iny
58b2: 84 08                        sty     ]tmp              ;save updated index
58b4: 20 6e 7a                     jsr     VgOutputCmd       ;add command to list
58b7: a5 08                        lda     ]tmp              ;restore index
58b9: 60                           rts

                   ; 
                   ; Updates volcano particles.
                   ; 
                   ; (Some ADCs are done without setting or clearing the carry.  FWIW, the carry
                   ; flag is set when the function is first called.)
                   ; 
58ba: a2 04        UpdateVolcano   ldx     #$04              ;max 5 particles (0-4)
58bc: bd 4d 03     :PartLoop       lda     volpart_ttl,x     ;is this particle alive?
58bf: d0 37                        bne     :UpdatePart       ;yes, update it
                   ; Create new particle, maybe.
58c1: ad 2a 18                     lda     POKEY_RANDOM
58c4: 29 07                        and     #$07              ;0-7
58c6: d0 77                        bne     :NextPart         ;12.5% chance of creating a particle
58c8: a9 1f                        lda     #$1f              ;set lifetime to max (5 bits -> $1f)
58ca: 9d 4d 03                     sta     volpart_ttl,x
58cd: ad 2a 18                     lda     POKEY_RANDOM      ;get random number
58d0: 29 03                        and     #$03              ;0-3
58d2: 69 01                        adc     #$01              ;1-4 or 2-5 (note this will clear carry)
58d4: 2c 2a 18                     bit     POKEY_RANDOM      ;get another random number
58d7: 50 02                        bvc     :GoRight          ;50/50 chance of going left or right
58d9: 49 ff                        eor     #$ff              ;negate (didn't add 1, so left-moving particles
58db: 9d 52 03     :GoRight        sta     volpart_vx,x      ; will be slightly faster on average)
58de: ad 2a 18                     lda     POKEY_RANDOM      ;get another random number
58e1: 29 07                        and     #$07              ;0-7
58e3: 69 05                        adc     #$05              ;5-12
58e5: 9d 57 03                     sta     volpart_vy,x
58e8: a9 00        :InitCoords     lda     #$00              ;position at particle source (0,0)
58ea: 9d 5c 03                     sta     volpart_xc_lo,x
58ed: 9d 61 03                     sta     volpart_xc_hi,x
58f0: 9d 66 03                     sta     volpart_yc_lo,x
58f3: 9d 6b 03                     sta     volpart_yc_hi,x
58f6: f0 47                        beq     :NextPart         ;(always)

58f8: de 4d 03     :UpdatePart     dec     volpart_ttl,x     ;decay
58fb: f0 eb                        beq     :InitCoords       ;if we reached zero, clear coords and move on
                   ; Update the particle's Y position.  The Y velocity decrements each time, giving
                   ; particles a gravitational acceleration.
58fd: de 57 03                     dec     volpart_vy,x      ;decrement Y velocity
5900: a0 00                        ldy     #$00              ;assume positive
5902: 18                           clc
5903: bd 57 03                     lda     volpart_vy,x      ;are we moving up or down?
5906: 10 01                        bpl     :IsPos
5908: 88                           dey                       ;sign-extend negative
5909: 7d 66 03     :IsPos          adc     volpart_yc_lo,x   ;update Y coordinate
590c: 9d 66 03                     sta     volpart_yc_lo,x
                   ; Kill particles that hit the ground.  Remember that particles use coordinates
                   ; relative to the origin point at the top of the volcano, which is at Y=+94, so
                   ; the ground is at -94.
590f: bd 6b 03                     lda     volpart_yc_hi,x   ;check high byte
5912: 10 0e                        bpl     :NotGround        ;positive, not at ground level
5914: bd 66 03                     lda     volpart_yc_lo,x   ;check low byte
5917: c9 a2                        cmp     #$a2              ;-94
5919: b0 07                        bcs     :NotGround
591b: a9 00                        lda     #$00              ;kill particle
591d: 9d 4d 03                     sta     volpart_ttl,x
5920: f0 c6                        beq     :InitCoords       ;(always)

5922: 98           :NotGround      tya                       ;high byte of Y velocity
5923: 7d 6b 03                     adc     volpart_yc_hi,x   ;update high byte of Y position
5926: 9d 6b 03                     sta     volpart_yc_hi,x
                   ; Update the X position.  Velocity does not change.
5929: a0 00                        ldy     #$00              ;assume positive
592b: 18                           clc
592c: bd 52 03                     lda     volpart_vx,x
592f: 10 01                        bpl     :IsPos
5931: 88                           dey                       ;sign-extend negative
5932: 7d 5c 03     :IsPos          adc     volpart_xc_lo,x
5935: 9d 5c 03                     sta     volpart_xc_lo,x
5938: 98                           tya
5939: 7d 61 03                     adc     volpart_xc_hi,x
593c: 9d 61 03                     sta     volpart_xc_hi,x
593f: ca           :NextPart       dex                       ;next particle
5940: 30 03                        bmi     :Return           ;done yet?
5942: 4c bc 58                     jmp     :PartLoop         ;no, keep going

5945: 60           :Return         rts

                   ; 
                   ; Generates the visibility list.
                   ; 
                   • Clear variables
                   ]tmp_08         .var    $08    {addr/1}
                   ]temp_angle     .var    $2b    {addr/1}

5946: a0 00        VLGenerate      ldy     #$00              ;Y-reg holds index into visibility list
                   ; Start by setting R0/R1/R2/R3 to the player's position and facing.
5948: a5 2d                        lda     unit_pos_z        ;player position
594a: 8d 64 18                     sta     MB_SET_R2L
594d: a5 2e                        lda     unit_pos_z+1
594f: 8d 65 18                     sta     MB_SET_R2H
5952: a5 31                        lda     unit_pos_x
5954: 8d 66 18                     sta     MB_SET_R3L
5957: a5 32                        lda     unit_pos_x+1
5959: 8d 67 18                     sta     MB_SET_R3H
595c: a5 27                        lda     plyr_facing_lo    ;get low byte ($00/$80)
595e: 18                           clc                       ;roll high bit into low bit (now $00/$01)
595f: 2a                           rol     A
5960: 2a                           rol     A
5961: a6 2a                        ldx     plyr_facing_hi    ;get high byte
5963: 20 17 5f                     jsr     CalcSine512       ;compute sin()
5966: 49 ff                        eor     #$ff              ;negate
5968: 69 01                        adc     #$01
596a: 8d 62 18                     sta     MB_SET_R1L        ;store in R1
596d: 8a                           txa
596e: 49 ff                        eor     #$ff
5970: 69 00                        adc     #$00
5972: 8d 63 18                     sta     MB_SET_R1H        ;high byte too
5975: a5 a6                        lda     temp_a6           ;get low bit again
5977: a6 2a                        ldx     plyr_facing_hi
5979: 20 f9 5e                     jsr     CalcCosine512     ;compute cos()
597c: 8d 60 18                     sta     MB_SET_R0L        ;store that as-is in R0
597f: 8e 61 18                     stx     MB_SET_R0H
5982: 2c 4b 03                     bit     attract_logo_flag ;showing logo?
5985: 10 05                        bpl     :AddEnemyUnit     ;no, show unit
5987: a2 00                        ldx     #$00              ;yes, omit enemy unit
5989: 4c ee 5a                     jmp     :ObstLoop

598c: a5 14        :AddEnemyUnit   lda     enemy_state       ;is the enemy alive?
598e: f0 06                        beq     VLAddEnemy        ;yes, add it to draw list
5990: 20 89 5b                     jsr     VLUpdateChunks    ;no, update exploding chunks
5993: 4c 6c 5a     :jmp_VLProj     jmp     VLAddProjectiles  ;move on to projectiles

                   ; 
                   ; Try to add enemy unit to visibility list.  This must come first (Y-reg=0).
                   ; 
5996: a5 2c        VLAddEnemy      lda     enemy_facing      ;copy facing
5998: 8d 71 02                     sta     vis_obj_facing
599b: a9 16                        lda     #$16              ;type=missile
599d: 24 cb                        bit     missile_flag
599f: 30 0a                        bmi     :GotType
59a1: a2 21                        ldx     #$21              ;type=super tank
59a3: 20 b5 69                     jsr     GetTankType       ;check type
59a6: b0 02                        bcs     :IsSuper          ;super, branch
59a8: a2 02                        ldx     #$02              ;type=slow tank
59aa: 8a           :IsSuper        txa
59ab: 8d 70 02     :GotType        sta     vis_obj_type      ;set object type (Y-reg=0 so no index needed)
59ae: a5 2f                        lda     enemy_pos_z       ;copy enemy unit X/Z position to R4/R5
59b0: 8d 68 18                     sta     MB_SET_R4L
59b3: a5 30                        lda     enemy_pos_z+1
59b5: 8d 69 18                     sta     MB_SET_R4H
59b8: a5 33                        lda     enemy_pos_x
59ba: 8d 6a 18                     sta     MB_SET_R5L
59bd: a5 34                        lda     enemy_pos_x+1
59bf: 8d 6b 18                     sta     MB_ROT_Z          ;invoke function $0b
59c2: 20 1f 5b                     jsr     VLAddIfVis        ;add if visible
59c5: 98                           tya                       ;check index
59c6: f0 cb                        beq     :jmp_VLProj       ;still zero, unit not added; skip accessories
                   ; Copy position and facing for accessory objects (drip/tread) from index 0 to
                   ; index 2.  This is wasted for the super tank, which has no add-ons.
59c8: ad 71 02                     lda     vis_obj_facing
59cb: 99 71 02                     sta     vis_obj_facing,y
59ce: ad 00 02                     lda     vis_obj_zpos
59d1: 99 00 02                     sta     vis_obj_zpos,y
59d4: ad 01 02                     lda     vis_obj_zpos+1
59d7: 99 01 02                     sta     vis_obj_zpos+1,y
59da: ad 38 02                     lda     vis_obj_xpos
59dd: 99 38 02                     sta     vis_obj_xpos,y
59e0: ad 39 02                     lda     vis_obj_xpos+1
59e3: 99 39 02                     sta     vis_obj_xpos+1,y
59e6: 24 cb                        bit     missile_flag      ;is this a missile?
59e8: 10 09                        bpl     :NotMissile       ;no, branch
                   ; Add missile splatter.
59ea: a5 a7                        lda     tread_drip_ctr    ;get current counter
59ec: 29 07                        and     #$07              ;0-7 (there are 8 objects in sequence)
59ee: 18                           clc
59ef: 69 24                        adc     #$24              ;splatter #0 is object $24
59f1: d0 1d                        bne     :SetObjType       ;(always)

59f3: 20 b5 69     :NotMissile     jsr     GetTankType
59f6: b0 74                        bcs     VLAddProjectiles  ;super tank, no treads or radar
                   ; Add treads and spinning radar.
59f8: ad 71 02                     lda     vis_obj_facing    ;get the enemy unit's relative facing
59fb: 38                           sec
59fc: e5 2a                        sbc     plyr_facing_hi
59fe: 18                           clc
59ff: 69 40                        adc     #$40              ;rotate 90 degrees
5a01: 0a                           asl     A                 ;put high bit in carry
5a02: a9 04                        lda     #$04              ;rear tread #0 is object $04
5a04: 90 01                        bcc     :RearTread
5a06: 0a                           asl     A                 ;front tread #0 is object $08
5a07: 85 08        :RearTread      sta     ]tmp_08
5a09: a5 a7                        lda     tread_drip_ctr    ;get tread counter
5a0b: 29 03                        and     #$03              ;0-3 (there are four tread objects in sequence)
5a0d: 18                           clc
5a0e: 65 08                        adc     ]tmp_08           ;now $04-07 or $08-0b
5a10: 99 70 02     :SetObjType     sta     vis_obj_type,y    ;save object type
5a13: c8                           iny                       ;advance object pointer
5a14: c8                           iny
5a15: 24 cb                        bit     missile_flag      ;is this a missile?
5a17: 30 53                        bmi     VLAddProjectiles  ;no, go deal with projectiles
5a19: a9 0d                        lda     #$0d              ;radar antenna
5a1b: 99 70 02                     sta     vis_obj_type,y    ;set type
5a1e: a5 be                        lda     radar_facing      ;current antenna rotation
5a20: 99 71 02                     sta     vis_obj_facing,y  ;set facing
                   ; Antenna is placed off-center on slow tank.  Position it.  If you look at the
                   ; data at $3955, you'll see that the antenna mount is at Z=-512 X=0 Y=80.  Model
                   ; X/Z coordinates are 2x world, so we want to place the object at Z=-256.
5a23: a5 2a                        lda     plyr_facing_hi    ;get tank facing as seen by viewer
5a25: 38                           sec
5a26: e5 2c                        sbc     enemy_facing
5a28: 85 2b                        sta     ]temp_angle
5a2a: 20 4b 5e                     jsr     CalcCosine        ;calc cos(angle), with range [-32767,32767]
5a2d: 8a                           txa                       ;high byte in A-reg (lo byte discarded), effectively
5a2e: a2 ff                        ldx     #$ff              ; computing N/256 ([-127,127]).
5a30: 49 ff                        eor     #$ff              ;negate the value
5a32: 30 01                        bmi     :IsNeg
5a34: e8                           inx                       ;sign-extend positive
5a35: 18           :IsNeg          clc
5a36: 69 01                        adc     #$01
5a38: 90 01                        bcc     :NoInc
5a3a: e8                           inx
5a3b: 0a           :NoInc          asl     A                 ;double it, so now we have [-255,255]
5a3c: 48                           pha
5a3d: 8a                           txa
5a3e: 2a                           rol     A
5a3f: aa                           tax
5a40: 68                           pla
5a41: 18                           clc
5a42: 6d 00 02                     adc     vis_obj_zpos      ;add calculated position to enemy unit Z pos
5a45: 99 00 02                     sta     vis_obj_zpos,y    ;(remember, enemy unit is always in 0th entry in list)
5a48: 8a                           txa
5a49: 6d 01 02                     adc     vis_obj_zpos+1
5a4c: 99 01 02                     sta     vis_obj_zpos+1,y
5a4f: a5 2b                        lda     ]temp_angle       ;repeat for sin(); less complicated because no negate
5a51: 20 4e 5e                     jsr     CalcSine          ;calc sin(angle)
5a54: 8a                           txa                       ;high byte in A-reg (low byte discarded)
5a55: 0a                           asl     A                 ;double it
5a56: a2 00                        ldx     #$00              ;sign-extend
5a58: 90 02                        bcc     :IsPos
5a5a: a2 ff                        ldx     #$ff
5a5c: 18           :IsPos          clc
5a5d: 6d 38 02                     adc     vis_obj_xpos      ;add calculated position to enemy unit X pos
5a60: 99 38 02                     sta     vis_obj_xpos,y
5a63: 8a                           txa
5a64: 6d 39 02                     adc     vis_obj_xpos+1
5a67: 99 39 02                     sta     vis_obj_xpos+1,y
5a6a: c8                           iny                       ;advance object index
5a6b: c8                           iny
                   ; 
                   ; Add projectiles to visible object list.
                   ; 
                   VLAddProjectiles
5a6c: a2 02                        ldx     #$02              ;index to projectile #1
5a6e: b5 24        :ProjLoop       lda     projectile_state_0,x ;is it active?
5a70: f0 40                        beq     :NextProj         ;no, try the next one
5a72: b5 2a                        lda     plyr_facing_hi,x  ;projectile facing matches that of firing unit
5a74: 99 71 02                     sta     vis_obj_facing,y  ; (not correct, but not noticeably wrong)
5a77: a9 03                        lda     #$03              ;type=projectile
5a79: 99 70 02                     sta     vis_obj_type,y    ;set type
5a7c: b5 24                        lda     projectile_state_0,x ;check state
5a7e: 10 1b                        bpl     :InFlight         ;still in flight
5a80: 18                           clc                       ;it's exploding, check how far along
5a81: 69 0f                        adc     #$0f              ;advance state +15 (6 frames starting from $80)
5a83: 30 06                        bmi     :StillExplod      ;still exploding
5a85: a9 00                        lda     #$00              ;done exploding, set state to zero
5a87: 95 24                        sta     projectile_state_0,x
5a89: f0 27                        beq     :NextProj         ;(always)

5a8b: 95 24        :StillExplod    sta     projectile_state_0,x
5a8d: 49 ff                        eor     #$ff              ;invert the value
5a8f: 18                           clc
5a90: 79 01 02                     adc     vis_obj_zpos+1,y  ;add (view coord) Z to scale with distance
5a93: 99 71 02                     sta     vis_obj_facing,y  ;set explosion scale factor in facing field
5a96: a9 0e                        lda     #$0e              ;type=projectile explosion
5a98: 99 70 02                     sta     vis_obj_type,y    ;set type
5a9b: b5 a8        :InFlight       lda     proj_pos_z,x      ;set projectile position in R4/R5
5a9d: 8d 68 18                     sta     MB_SET_R4L
5aa0: b5 a9                        lda     proj_pos_z+1,x
5aa2: 8d 69 18                     sta     MB_SET_R4H
5aa5: b5 ac                        lda     proj_pos_x,x
5aa7: 8d 6a 18                     sta     MB_SET_R5L
5aaa: b5 ad                        lda     proj_pos_x+1,x
5aac: 8d 6b 18                     sta     MB_ROT_Z          ;invoke function $0b
5aaf: 20 1f 5b                     jsr     VLAddIfVis        ;add if visible
5ab2: ca           :NextProj       dex
5ab3: ca                           dex
5ab4: 10 b8                        bpl     :ProjLoop
                   ; 
                   ; Add saucer to visible objects list.
                   ; 
5ab6: a5 de                        lda     saucer_state      ;is there a saucer?
5ab8: f0 32                        beq     VLAddObstacles    ;no, branch
5aba: a9 20                        lda     #$20              ;type=saucer
5abc: 99 70 02                     sta     vis_obj_type,y
5abf: a5 d9                        lda     saucer_facing     ;copy facing
5ac1: 99 71 02                     sta     vis_obj_facing,y
5ac4: a5 d5                        lda     saucer_z          ;set saucer position in R4/R5
5ac6: 8d 68 18                     sta     MB_SET_R4L
5ac9: a5 d6                        lda     saucer_z+1
5acb: 8d 69 18                     sta     MB_SET_R4H
5ace: a5 d7                        lda     saucer_x
5ad0: 8d 6a 18                     sta     MB_SET_R5L
5ad3: a5 d8                        lda     saucer_x+1
5ad5: 8d 6b 18                     sta     MB_ROT_Z          ;invoke function $0b
5ad8: 20 1f 5b                     jsr     VLAddIfVis        ;add if visible
5adb: b9 6e 02                     lda     vis_obj_type-2,y  ;check the type in the now-previous slot to see if
5ade: c9 20                        cmp     #$20              ; the saucer is visible (note "visible" includes
5ae0: d0 06                        bne     :NotVisible       ; a fair distance to the left and right)
5ae2: a9 81                        lda     #$81              ;regular noisy mode
5ae4: 85 de                        sta     saucer_state
5ae6: d0 04                        bne     VLAddObstacles    ;(always)

5ae8: a9 01        :NotVisible     lda     #$01              ;stealth mode
5aea: 85 de                        sta     saucer_state
                   ; Add all visible obstacles.
5aec: a2 00        VLAddObstacles  ldx     #$00
5aee: bd cc 3f     :ObstLoop       lda     obstacle_t_f,x    ;get facing
5af1: 30 28                        bmi     :ListEnd          ;end of list, branch
5af3: 99 70 02                     sta     vis_obj_type,y
5af6: bd cd 3f                     lda     obstacle_t_f+1,x
5af9: 99 71 02                     sta     vis_obj_facing,y
5afc: bd 81 76                     lda     obstacle_z_pos,x  ;set obstacle position in R4/R5
5aff: 8d 68 18                     sta     MB_SET_R4L
5b02: bd 82 76                     lda     obstacle_z_pos+1,x
5b05: 8d 69 18                     sta     MB_SET_R4H
5b08: bd ab 76                     lda     obstacle_x_pos,x
5b0b: 8d 6a 18                     sta     MB_SET_R5L
5b0e: bd ac 76                     lda     obstacle_x_pos+1,x
5b11: 8d 6b 18                     sta     MB_ROT_Z          ;invoke function $0b
5b14: 20 1f 5b                     jsr     VLAddIfVis        ;add if visible
5b17: e8                           inx
5b18: e8                           inx
5b19: d0 d3                        bne     :ObstLoop
5b1b: 99 70 02     :ListEnd        sta     vis_obj_type,y    ;writes $ff to the end of the vis list
5b1e: 60                           rts

                   ; 
                   ; Finish View transform and test visibility against the sides and "far" plane.
                   ; 
                   ; Call after invoking math box function $0b.  Called for 5 different categories
                   ; of things: enemy unit, projectiles, saucer, obstacles, enemy unit chunks.
                   ; 
                   ; On entry:
                   ;   Y-reg: offset of next entry in visible object list
                   ; 
                   ; On exit:
                   ;   Y-reg: incremented by two iff object added to list
                   ; 
                   • Clear variables
                   ]view_pos_z     .var    $1b    {addr/2}
                   ]abs_pos_x      .var    $1d    {addr/2}
                   ]view_pos_x     .var    $1f    {addr/2}

5b1f: 20 80 5b     VLAddIfVis      jsr     MbWaitForResult   ;wait for function $0b to finish
5b22: 0a                           asl     A                 ;double result to compensate for 16-bit shift
5b23: 85 1b                        sta     ]view_pos_z       ;save it
5b25: ad 18 18                     lda     MB_RESULT_HI      ;get the high byte
5b28: 8d 72 18                     sta     MB_ROT_X          ;start function $12 (STA value is ignored)
5b2b: 30 52                        bmi     :Return           ;object is behind us, not visible
5b2d: 2a                           rol     A                 ;double result (high byte)
5b2e: 30 4f                        bmi     :Return           ;if negative, object was too far away
5b30: 85 1c                        sta     ]view_pos_z+1     ;save it
5b32: 4a                           lsr     A                 ;shift right twice to test near plane ($03ff)
5b33: 4a                           lsr     A
5b34: f0 49                        beq     :Return           ;impossibly close (we're partly inside it)
5b36: a5 1c                        lda     ]view_pos_z+1     ;get the high byte
5b38: c9 7b                        cmp     #$7b              ;far plane is $7aff
5b3a: b0 43                        bcs     :Return           ;object too far, not visible
5b3c: 20 80 5b                     jsr     MbWaitForResult   ;wait for function $12 to finish
5b3f: 0a                           asl     A                 ;double to compensate for 16-bit shift
5b40: 85 1f                        sta     ]view_pos_x       ;save it
5b42: ad 18 18                     lda     MB_RESULT_HI      ;get the high byte
5b45: 2a                           rol     A                 ;double that too
5b46: 85 20                        sta     ]view_pos_x+1     ;save it
                   ; We've computed X/Z and determined that the object is between the near and far
                   ; planes.  Now check left/right.  (Remember that +X is to the left.)
5b48: a5 1f                        lda     ]view_pos_x       ;compute absolute value of X position
5b4a: 24 20                        bit     ]view_pos_x+1
5b4c: 10 05                        bpl     :IsPosL
5b4e: 18                           clc                       ;negate
5b4f: 49 ff                        eor     #$ff
5b51: 69 01                        adc     #$01              ;note carry is clear afterward
5b53: 85 1d        :IsPosL         sta     ]abs_pos_x
5b55: a5 20                        lda     ]view_pos_x+1
5b57: 10 04                        bpl     :IsPosH
5b59: 49 ff                        eor     #$ff              ;negate
5b5b: 69 00                        adc     #$00
5b5d: 85 1e        :IsPosH         sta     ]abs_pos_x+1
5b5f: a5 1d                        lda     ]abs_pos_x        ;compare Z to X (90 degree FOV)
5b61: c5 1b                        cmp     ]view_pos_z       ;(actual FOV is 45 degrees, so this is sloppy)
5b63: a5 1e                        lda     ]abs_pos_x+1
5b65: e5 1c                        sbc     ]view_pos_z+1
5b67: b0 16                        bcs     :Return
                   ; Object is visible, add it to the list.
5b69: a5 1b                        lda     ]view_pos_z
5b6b: 99 00 02                     sta     vis_obj_zpos,y    ;set Z coord
5b6e: a5 1c                        lda     ]view_pos_z+1
5b70: 99 01 02                     sta     vis_obj_zpos+1,y
5b73: a5 1f                        lda     ]view_pos_x       ;set X coord
5b75: 99 38 02                     sta     vis_obj_xpos,y
5b78: a5 20                        lda     ]view_pos_x+1
5b7a: 99 39 02                     sta     vis_obj_xpos+1,y
5b7d: c8                           iny                       ;advance to next entry
5b7e: c8                           iny
5b7f: 60           :Return         rts

                   ; 
                   ; Wait for a result from the math box.
                   ; 
                   ; On exit:
                   ;   A-reg: MB result, low byte
                   ;   X-reg/Y-reg preserved
                   ; 
5b80: 2c 00 18     MbWaitForResult bit     MB_STATUS         ;check status
5b83: 30 fb                        bmi     MbWaitForResult   ;branch if busy
5b85: ad 10 18                     lda     MB_RESULT_LO      ;grab low byte in A-reg
5b88: 60                           rts

                   ; 
                   ; Updates the positions of the flying chunks that are created when an enemy unit
                   ; is destroyed.  If the chunks are in the viewport, add them to the list of
                   ; visible objects.
                   ; 
                   • Clear variables
                   ]yvel_x4        .var    $08    {addr/2}

5b89: a2 0a        VLUpdateChunks  ldx     #$0a              ;6 things ($00-0a, even only)
5b8b: bd d9 02     :OuterLoop      lda     ypos_by_type+1,x  ;check the chunk's altitude
5b8e: 18                           clc
5b8f: 10 03                        bpl     :UpdateObj        ;still above ground, update it
5b91: 4c 3d 5c                     jmp     :LoopEnd          ;on to the next chunk

5b94: bd a8 02     :UpdateObj      lda     chunk_z_pos,x     ;update Z position
5b97: 7d ae 3f                     adc     chunk_x_vel,x
5b9a: 9d a8 02                     sta     chunk_z_pos,x
5b9d: 8d 68 18                     sta     MB_SET_R4L        ;set R4=Z
5ba0: bd a9 02                     lda     chunk_z_pos+1,x
5ba3: 7d af 3f                     adc     chunk_x_vel+1,x
5ba6: 9d a9 02                     sta     chunk_z_pos+1,x
5ba9: 8d 69 18                     sta     MB_SET_R4H
5bac: 18                           clc                       ;update X position
5bad: bd b8 02                     lda     chunk_x_pos,x
5bb0: 7d ba 3f                     adc     chunk_z_vel,x
5bb3: 9d b8 02                     sta     chunk_x_pos,x
5bb6: 8d 6a 18                     sta     MB_SET_R5L        ;set R5=X
5bb9: bd b9 02                     lda     chunk_x_pos+1,x
5bbc: 7d bb 3f                     adc     chunk_z_vel+1,x
5bbf: 9d b9 02                     sta     chunk_x_pos+1,x
5bc2: bd c9 02                     lda     chunk_y_vel+1,x   ;get Y velocity (only high 8 bits are used)
5bc5: 85 08                        sta     ]yvel_x4
5bc7: 30 02                        bmi     :IsNeg
5bc9: a9 00                        lda     #$00              ;sign-extend
5bcb: f0 02        :IsNeg          beq     :YComm
5bcd: a9 ff                        lda     #$ff
5bcf: 06 08        :YComm          asl     ]yvel_x4          ;multiply Y velocity by 4
5bd1: 2a                           rol     A
5bd2: 06 08                        asl     ]yvel_x4
5bd4: 2a                           rol     A
5bd5: 85 09                        sta     ]yvel_x4+1
5bd7: bd d8 02                     lda     ypos_by_type,x    ;update Y position
5bda: 18                           clc
5bdb: 65 08                        adc     ]yvel_x4
5bdd: 9d d8 02                     sta     ypos_by_type,x
5be0: a5 09                        lda     ]yvel_x4+1
5be2: 7d d9 02                     adc     ypos_by_type+1,x
5be5: 9d d9 02                     sta     ypos_by_type+1,x
                   ; Simulate gravity by reducing Y velocity.
5be8: bd c9 02                     lda     chunk_y_vel+1,x   ;get the velocity
5beb: 10 04                        bpl     :Upward           ;moving upward, branch
5bed: c9 85                        cmp     #$85              ;hit terminal velocity?
5bef: 90 05                        bcc     :TermVel
5bf1: 69 fc        :Upward         adc     #$fc              ;adding -4 essentially
5bf3: 9d c9 02                     sta     chunk_y_vel+1,x
                   ; Modify the velocity further, and rotate the chunk.
5bf6: 8a           :TermVel        txa                       ;A-reg=chunk index ($00-0a)
5bf7: 4a                           lsr     A                 ;get bit 2 into the carry (set for 4+)
5bf8: 4a                           lsr     A
5bf9: 08                           php                       ;save the carry
5bfa: 8a                           txa                       ;get chunk index again
5bfb: 0a                           asl     A                 ;multiply by 4 ($00-28 by 4)
5bfc: 0a                           asl     A
5bfd: 69 03                        adc     #$03              ;add 3 ($03-2b by 4)
5bff: 28                           plp
5c00: 90 0a                        bcc     :Sub4             ;index < 4, branch
5c02: 85 08                        sta     ]yvel_x4
5c04: bd c8 02                     lda     chunk_y_vel,x
5c07: e5 08                        sbc     ]yvel_x4
5c09: 4c 0f 5c                     jmp     :SetVelAndFacing

5c0c: 7d c8 02     :Sub4           adc     chunk_y_vel,x     ;add value from low 8 bits
                   :SetVelAndFacing
5c0f: 9d c8 02                     sta     chunk_y_vel,x
5c12: 99 71 02                     sta     vis_obj_facing,y  ;update facing to spin the object
                   ; Assign the chunk object type.
                   ]chunk_type     .var    $08    {addr/1}

5c15: 8a                           txa                       ;$00-$0a by 2
5c16: 4a                           lsr     A                 ;$00-05
5c17: 18                           clc
5c18: 69 10                        adc     #$10              ;$10-15
5c1a: 24 cb                        bit     missile_flag      ;was this a missile?
5c1c: 10 04                        bpl     :IsTank           ;no, tank
5c1e: 69 08                        adc     #$08              ;$18-1d for missile
5c20: d0 0f                        bne     :SetType          ;(always)
5c22: c9 13        :IsTank         cmp     #$13              ;is this the radar dish?
5c24: d0 0b                        bne     :SetType          ;no, branch
5c26: 85 08                        sta     ]chunk_type       ;save it off
5c28: 20 b5 69                     jsr     GetTankType       ;check the tank type
5c2b: 90 02                        bcc     :SlowTank         ;slow tank, keep the dish
5c2d: e6 08                        inc     ]chunk_type       ;super tank, add an extra $14
5c2f: a5 08        :SlowTank       lda     ]chunk_type
5c31: 99 70 02     :SetType        sta     vis_obj_type,y    ;set the object type
5c34: bd b9 02                     lda     chunk_x_pos+1,x
5c37: 8d 6b 18                     sta     MB_ROT_Z          ;invoke function $0b
5c3a: 20 1f 5b                     jsr     VLAddIfVis        ;add to list if visible
5c3d: ca           :LoopEnd        dex
5c3e: ca                           dex
5c3f: 30 03                        bmi     :ChunksDone
5c41: 4c 8b 5b                     jmp     :OuterLoop

                   ; Check to see if all chunks have hit the ground.  If so, create a new enemy
                   ; unit.
5c44: a2 0a        :ChunksDone     ldx     #$0a
5c46: bd d9 02     :CheckLoop      lda     ypos_by_type+1,x
5c49: 10 0f                        bpl     :Return
5c4b: ca                           dex
5c4c: ca                           dex
5c4d: 10 f7                        bpl     :CheckLoop
5c4f: a9 00                        lda     #$00              ;mark unit as alive
5c51: 85 14                        sta     enemy_state
5c53: 98                           tya
5c54: 48                           pha
5c55: 20 be 69                     jsr     CreateEnemyUnit   ;make a new friend
5c58: 68                           pla
5c59: a8                           tay
5c5a: 60           :Return         rts

5c5b: 5e                           .dd1    $5e               ;(checksum adj)

                   ; 
                   ; Draws an object.  Call after transforming vertices.
                   ; 
                   ; On entry:
                   ;   A-reg: object type (0-43)
                   ;   $10: object index * 2
                   ; 
                   • Clear variables
                   ]upper5         .var    $08    {addr/1}
                   ]obj_index      .var    $10    {addr/1}
                   ]shape_ptr      .var    $3b    {addr/2}

5c5c: 0a           DrawObject      asl     A
5c5d: a8                           tay
5c5e: b9 72 74                     lda     shape_code_addrs,y
5c61: 85 3b                        sta     ]shape_ptr
5c63: b9 73 74                     lda     shape_code_addrs+1,y
5c66: 85 3c                        sta     ]shape_ptr+1
5c68: a0 00        DrawObjLoop     ldy     #$00
5c6a: b1 3b                        lda     (]shape_ptr),y    ;get code byte
5c6c: c9 ff                        cmp     #$ff              ;end of list?
5c6e: d0 01                        bne     :DoDraw           ;not yet
5c70: 60                           rts

5c71: aa           :DoDraw         tax
5c72: 29 f8                        and     #%11111000        ;upper 5 bits (vertex index or intensity)
5c74: 85 08                        sta     ]upper5
5c76: 8a                           txa
5c77: 29 07                        and     #%00000111        ;lower 3 bits (draw action)
5c79: 0a                           asl     A
5c7a: aa                           tax
5c7b: bd 85 5c                     lda     draw_cmd_addrs+1,x ;get address of code to invoke
5c7e: 48                           pha
5c7f: bd 84 5c                     lda     draw_cmd_addrs,x
5c82: 48                           pha
5c83: 60                           rts

5c84: 91 5c        draw_cmd_addrs  .dd2    DrwPoint-1        ;0
5c86: 9e 5c                        .dd2    DrwSetIntens-1    ;1
5c88: e0 5c                        .dd2    DrwMove-1         ;2
5c8a: d3 5c                        .dd2    DrwCenterMove-1   ;3
5c8c: e4 5c                        .dd2    DrwVec-1          ;4
5c8e: ec 5c                        .dd2    DrwProjExpl-1     ;5
5c90: 0d 5d                        .dd2    DrwNoOp-1         ;6

                   ; Moves to the vertex and draws a point.
5c92: a9 00        DrwPoint        lda     #$00              ;beam off
5c94: 20 11 5d                     jsr     DrwToVertex       ;move to vertex
5c97: a9 20                        lda     #$20              ;intensity=1 (uses STAT value)
5c99: 20 8a 7a                     jsr     VgDrawPoint       ;draw a point
5c9c: 4c 41 5d                     jmp     NextDrawCmd

                   ; Set intensity with vertex index value (overridden for saucer/logo).
5c9f: a6 10        DrwSetIntens    ldx     ]obj_index
5ca1: bd 70 02                     lda     vis_obj_type,x    ;get object type
5ca4: c9 17                        cmp     #$17              ;logo "Ba"
5ca6: f0 10                        beq     :IsLogo
5ca8: c9 1e                        cmp     #$1e              ;logo "ttle"
5caa: f0 0c                        beq     :IsLogo
5cac: c9 1f                        cmp     #$1f              ;logo "Zone"
5cae: f0 08                        beq     :IsLogo
5cb0: c9 20                        cmp     #$20              ;saucer?
5cb2: d0 0a                        bne     :UseVertex
5cb4: a5 d4                        lda     saucer_dead_intens ;is the saucer intensity in flux?
5cb6: f0 06                        beq     :UseVertex        ;no, use vertex value
5cb8: ad ea 02     :IsLogo         lda     sauc_log_inten
5cbb: 4c cb 5c                     jmp     :EmitStat

5cbe: a5 08        :UseVertex      lda     ]upper5           ;use vertex value as intensity
5cc0: 38                           sec
5cc1: e5 25                        sbc     dist_intensity    ;reduce intensity for distant objects
5cc3: 90 04                        bcc     :MinDim           ;branch if we made it invisible
5cc5: c9 30                        cmp     #$30              ;>= $30?
5cc7: b0 02                        bcs     :EmitStat         ;yes, bright enough
5cc9: a9 30        :MinDim         lda     #$30              ;set to $30 (3/16ths bright)
5ccb: a8           :EmitStat       tay
5ccc: a9 04                        lda     #$04              ;E=1 (just set intensity)
5cce: 20 63 7a                     jsr     VgStat            ;output STAT
5cd1: 4c 41 5d                     jmp     NextDrawCmd

                   ; Moves to center of screen, then moves without drawing.
5cd4: a9 00        DrwCenterMove   lda     #$00              ;zero out beam position tracking
5cd6: 85 35                        sta     cur_beam_xc
5cd8: 85 36                        sta     cur_beam_xc+1
5cda: 85 37                        sta     cur_beam_yc
5cdc: 85 38                        sta     cur_beam_yc+1
5cde: 20 6a 7a                     jsr     VgCenter          ;move to center of screen
                   ; Moves without drawing.
5ce1: a9 00        DrwMove         lda     #$00              ;just move
5ce3: f0 02                        beq     :DoDraw           ;(always)

                   ; Draws to vertex with global intensity.
5ce5: a9 20        DrwVec          lda     #$20              ;set intensity=1 to use STAT value
5ce7: 20 11 5d     :DoDraw         jsr     DrwToVertex
5cea: 4c 41 5d                     jmp     NextDrawCmd

                   ; Draws projectile explosion.
5ced: a6 10        DrwProjExpl     ldx     ]obj_index
5cef: bd 71 02                     lda     vis_obj_facing,x  ;scale is stored in "facing" field
5cf2: aa                           tax                       ;save
5cf3: 29 3f                        and     #%00111111        ;grab low 6 bits
5cf5: 0a                           asl     A                 ;double them
5cf6: a8                           tay                       ;use as 'L' value
5cf7: 8a                           txa                       ;restore original
5cf8: 29 c0                        and     #%11000000        ;grab high 2 bits
5cfa: 0a                           asl     A                 ;roll them into the low 2 bits
5cfb: 2a                           rol     A
5cfc: 2a                           rol     A                 ;(0-3), carry is now clear
5cfd: 69 01                        adc     #$01              ;add one (1-4), use as 'B' value
5cff: 20 83 7a                     jsr     VgScale           ;set scale
5d02: a2 7a                        ldx     #<vg_proj_explosion ;get projectile explosion VJSR
5d04: a9 34                        lda     #>vg_proj_explosion
5d06: 20 5a 7a                     jsr     VgJsrToAddr       ;add command
5d09: a9 01                        lda     #$01
5d0b: 20 81 7a                     jsr     VgScaleL0         ;restore 1:1 scale
                   ; No-op.
5d0e: 4c 41 5d     DrwNoOp         jmp     NextDrawCmd

                   ; 
                   ; Draws a vector to a transformed vertex.
                   ; 
                   ; On entry:
                   ;   A-reg: intensity (high 3 bits)
                   ;   $08: index of vertex (* 8)
                   ; 
                   ]deltaX         .var    $04    {addr/2}
                   ]deltaY         .var    $06    {addr/2}

5d11: 85 01        DrwToVertex     sta     vg_intensity
5d13: a5 08                        lda     ]upper5           ;get vertex index << 3
5d15: 4a                           lsr     A                 ;shift right once so we have index * 4
5d16: aa                           tax
5d17: b5 3d                        lda     screen_coords,x   ;X low
5d19: a8                           tay
5d1a: 38                           sec
5d1b: e5 35                        sbc     cur_beam_xc
5d1d: 85 04                        sta     ]deltaX
5d1f: 84 35                        sty     cur_beam_xc
5d21: b5 3e                        lda     screen_coords+1,x ;X high
5d23: a8                           tay
5d24: e5 36                        sbc     cur_beam_xc+1
5d26: 85 05                        sta     ]deltaX+1
5d28: 84 36                        sty     cur_beam_xc+1
5d2a: 38                           sec
5d2b: b5 3f                        lda     screen_coords+2,x ;Y low
5d2d: a8                           tay
5d2e: e5 37                        sbc     cur_beam_yc
5d30: 85 06                        sta     ]deltaY
5d32: 84 37                        sty     cur_beam_yc
5d34: b5 40                        lda     screen_coords+3,x ;Y high
5d36: a8                           tay
5d37: e5 38                        sbc     cur_beam_yc+1
5d39: 85 07                        sta     ]deltaY+1
5d3b: 84 38                        sty     cur_beam_yc+1
5d3d: 20 ab 7a                     jsr     VgVector
5d40: 60                           rts

                   ; Advances the shape code pointer, then jumps to the top of the object draw
                   ; loop.
5d41: e6 3b        NextDrawCmd     inc     ]shape_ptr
5d43: d0 02                        bne     :NoInc
5d45: e6 3c                        inc     ]shape_ptr+1
5d47: 4c 68 5c     :NoInc          jmp     DrawObjLoop

                   ; 
                   ; Transforms vertices of an entry in the visible object list to clip/screen
                   ; coordinates.
                   ; 
                   ; The object center positions in the list have already been view-transformed.
                   ; 
                   ; On entry:
                   ;   X-reg: index of list entry to transform * 2
                   ; 
                   • Clear variables
                   ]obj_index      .var    $13    {addr/1}
                   ]saved_index    .var    $16    {addr/1}
                   ]mesh_offset    .var    $18    {addr/1}
                   ]vertexY        .var    $22    {addr/2}
                   ]temp_angle     .var    $2b    {addr/1}
                   ]obj_ptr        .var    $3b    {addr/2}

5d4a: 86 13        TxfrmObject     stx     ]obj_index
5d4c: a5 2a                        lda     plyr_facing_hi    ;take the player facing
5d4e: 38                           sec
5d4f: fd 71 02                     sbc     vis_obj_facing,x  ;subtract the unit's facing
5d52: 49 80                        eor     #$80              ;flip it (same as CLC / ADC #$80)
5d54: 85 2b                        sta     ]temp_angle       ;rotate the object to this angle
5d56: 20 4e 5e                     jsr     CalcSine          ;calculate sine(theta)
5d59: 8d 62 18                     sta     MB_SET_R1L        ;store in R1
5d5c: 8e 63 18                     stx     MB_SET_R1H
5d5f: a5 2b                        lda     ]temp_angle
5d61: 20 4b 5e                     jsr     CalcCosine        ;calculate cosine(theta)
5d64: 18                           clc                       ;negate it
5d65: 49 ff                        eor     #$ff
5d67: 69 01                        adc     #$01
5d69: 8d 60 18                     sta     MB_SET_R0L        ;store in R0
5d6c: 8a                           txa
5d6d: 49 ff                        eor     #$ff
5d6f: 69 00                        adc     #$00
5d71: 8d 61 18                     sta     MB_SET_R0H
5d74: a6 13                        ldx     ]obj_index
5d76: bd 70 02                     lda     vis_obj_type,x    ;get object type
5d79: 0a                           asl     A                 ;double it for use as index
5d7a: a8                           tay
5d7b: b9 8e 38                     lda     shape_vertex_addrs,y ;get pointer to mesh data
5d7e: 85 3b                        sta     ]obj_ptr
5d80: b9 8f 38                     lda     shape_vertex_addrs+1,y
5d83: 85 3c                        sta     ]obj_ptr+1
5d85: bd 00 02                     lda     vis_obj_zpos,x    ;copy Z posn to R2
5d88: 8d 64 18                     sta     MB_SET_R2L
5d8b: bd 01 02                     lda     vis_obj_zpos+1,x
5d8e: 8d 65 18                     sta     MB_SET_R2H
5d91: 29 f0                        and     #$f0              ;use high byte of Z position to reduce intensity
5d93: 85 25                        sta     dist_intensity    ; of distant objects
5d95: bd 38 02                     lda     vis_obj_xpos,x    ;copy X posn to R3
5d98: 8d 66 18                     sta     MB_SET_R3L
5d9b: bd 39 02                     lda     vis_obj_xpos+1,x
5d9e: 8d 67 18                     sta     MB_SET_R3H
5da1: a2 00                        ldx     #$00              ;zero out RA
5da3: 8e 6d 18                     stx     MB_SET_RAL
5da6: 8e 6e 18                     stx     MB_SET_RAH
5da9: 86 18                        stx     ]mesh_offset
5dab: e6 18                        inc     ]mesh_offset      ;data starts at offset 1 (first byte is count)
5dad: a4 18        :VertexLoop     ldy     ]mesh_offset
5daf: b1 3b                        lda     (]obj_ptr),y      ;get vertex Z coord
5db1: 8d 68 18                     sta     MB_SET_R4L        ;store in R4
5db4: c8                           iny
5db5: b1 3b                        lda     (]obj_ptr),y
5db7: 8d 69 18                     sta     MB_SET_R4H
5dba: c8                           iny
5dbb: b1 3b                        lda     (]obj_ptr),y      ;get vertex X coord
5dbd: 8d 6a 18                     sta     MB_SET_R5L        ;store in R5
5dc0: c8                           iny
5dc1: b1 3b                        lda     (]obj_ptr),y
5dc3: 8d 71 18                     sta     MB_SCREEN_X       ;invoke function $11 (compute screen X)
5dc6: c8                           iny
5dc7: 20 80 5b                     jsr     MbWaitForResult   ;wait for calc to finish
5dca: 18                           clc                       ;negate it (flips screen left/right)
5dcb: 49 ff                        eor     #$ff
5dcd: 69 01                        adc     #$01
5dcf: 95 3d                        sta     screen_coords,x   ;screen X, low
5dd1: ad 18 18                     lda     MB_RESULT_HI
5dd4: 49 ff                        eor     #$ff
5dd6: 69 00                        adc     #$00
5dd8: 95 3e                        sta     screen_coords+1,x ;screen X, high
5dda: b1 3b                        lda     (]obj_ptr),y      ;get vertex Y coord
5ddc: 85 22                        sta     ]vertexY
5dde: c8                           iny
5ddf: b1 3b                        lda     (]obj_ptr),y
5de1: c8                           iny
5de2: 84 18                        sty     ]mesh_offset
5de4: 85 23                        sta     ]vertexY+1
5de6: 86 16                        stx     ]saved_index
5de8: 20 05 5e                     jsr     :CalcScreenY      ;adjust Y coord, then compute screen Y
5deb: 20 80 5b                     jsr     MbWaitForResult   ;wait for calc to finish
5dee: a6 16                        ldx     ]saved_index
5df0: 95 3f                        sta     screen_coords+2,x ;screen Y, low
5df2: ad 18 18                     lda     MB_RESULT_HI
5df5: 95 40                        sta     screen_coords+3,x ;screen Y, high
5df7: 98                           tya
5df8: a0 00                        ldy     #$00
5dfa: d1 3b                        cmp     (]obj_ptr),y      ;compare index to count
5dfc: b0 06                        bcs     :Return           ;done with this object
5dfe: e8                           inx                       ;advance screen coord output ptr
5dff: e8                           inx
5e00: e8                           inx
5e01: e8                           inx
5e02: d0 a9                        bne     :VertexLoop       ;(always)
5e04: 60           :Return         rts

5e05: a6 13        :CalcScreenY    ldx     ]obj_index
5e07: bd 70 02                     lda     vis_obj_type,x    ;get the object type
5e0a: 29 10                        and     #$10              ;is it missile, exploding chunk, or logo (type=$1x)?
5e0c: f0 15                        beq     :NotChunk         ;no, branch
                   ; Set the Y position for objects that aren't at ground level, notably missiles,
                   ; the game logo, and chunks of exploding tanks.  We use 8 16-bit values to hold
                   ; the height.  The height is assigned by object type rather than object index.
                   ; 
                   ; Types $10-15 and $18-1d are the exploding chunk types (6 pieces each),
                   ; missiles are $16, logos use $17/1c/1d.  It would look really weird to show the
                   ; logo and a missile at the same time.
5e0e: bd 70 02                     lda     vis_obj_type,x    ;get the type again
5e11: 29 07                        and     #$07              ;get low 3 bits (0-7)
5e13: 0a                           asl     A                 ;double for index
5e14: aa                           tax
5e15: bd d8 02                     lda     ypos_by_type,x
5e18: 65 22                        adc     ]vertexY
5e1a: 85 22                        sta     ]vertexY
5e1c: bd d9 02                     lda     ypos_by_type+1,x
5e1f: 65 23                        adc     ]vertexY+1
5e21: 85 23                        sta     ]vertexY+1
5e23: a5 df        :NotChunk       lda     horizon_adj       ;get horizon adjustment
5e25: f0 0f                        beq     :NoHAdj           ;no adjustment needed, branch
                   ; Adjust object Y position with the horizon adjustment.  We're shifting the
                   ; object Y positions by the same amount as the horizon, which isn't correct, but
                   ; this is cheaper than rotation about the X axis.  Having a slightly
                   ; discombobulated feel isn't a bad thing for an impact effect.
5e27: 38                           sec
5e28: a5 22                        lda     ]vertexY
5e2a: e5 df                        sbc     horizon_adj
5e2c: 8d 6f 18                     sta     MB_SET_RBL        ;put Y coord in RB
5e2f: a5 23                        lda     ]vertexY+1
5e31: e9 00                        sbc     #$00
5e33: 4c 3d 5e                     jmp     :CalcPart2

5e36: a5 22        :NoHAdj         lda     ]vertexY
5e38: 8d 6f 18                     sta     MB_SET_RBL
5e3b: a5 23                        lda     ]vertexY+1
5e3d: 8d 70 18     :CalcPart2      sta     MB_SET_RBH        ;set high byte of RB
5e40: 8d 74 18                     sta     MB_DIVIDE_B7      ;perspective project Y: RB / R7
5e43: 60                           rts

                   ; 
                   ; If we have a nonzero horizon adjustment (caused by projectile impact or
                   ; driving into something), reduce it toward zero.  Called once every game frame.
                   ; 
5e44: a5 df        UpdateHorizAdj  lda     horizon_adj
5e46: f0 02                        beq     :Return           ;(unnecessary?)
5e48: 46 df                        lsr     horizon_adj       ;cut horizon adjustment in half
5e4a: 60           :Return         rts

                   ; 
                   ; Calculates the value of cos(theta), where theta is an angle in the range
                   ; [$00,$ff], representing [0,359] degrees.
                   ; 
                   ; On entry:
                   ;   A-reg: angle $00-ff
                   ; 
                   ; On exit:
                   ;   A-reg: cosine value, low byte
                   ;   X-reg: cosine value, high byte
                   ;   Y-reg preserved
                   ; 
5e4b: 18           CalcCosine      clc                       ;cosine is 90 degrees out of phase == +$40
5e4c: 69 40                        adc     #$40
                   ; 
                   ; Calculates the value of sin(theta), where theta is an angle in the range
                   ; [$00,$ff], representing [0,359] degrees.  Returns a signed 1.15 value.
                   ; 
                   ; On entry:
                   ;   A-reg: angle $00-ff
                   ;   P flags set for load of angle
                   ; 
                   ; On exit:
                   ;   A-reg: sine value, low byte
                   ;   X-reg: sine value, high byte
                   ;   Y-reg preserved
                   ; 
5e4e: 10 13        CalcSine        bpl     CalcSineHalf      ;range $00-7f, branch to first-half function
5e50: 29 7f                        and     #$7f              ;strip high bit
5e52: 20 63 5e                     jsr     CalcSineHalf      ;call the first-half function
5e55: 49 ff                        eor     #$ff              ;negate result
5e57: 18                           clc                       ;2's complete, so invert and add one
5e58: 69 01                        adc     #$01
5e5a: 48                           pha
5e5b: 8a                           txa
5e5c: 49 ff                        eor     #$ff
5e5e: 69 00                        adc     #$00
5e60: aa                           tax
5e61: 68                           pla
5e62: 60                           rts

                   ; 
                   ; Calculates the value of sin(theta), where theta is an angle in the range
                   ; [$00,$7f], representing [0,179] degrees.
                   ; 
                   ; On entry:
                   ;   A-reg: angle $00-7f
                   ; 
                   ; On exit:
                   ;   A-reg: sine value, low byte
                   ;   X-reg: sine value, high byte
                   ;   Y-reg preserved
                   ; 
5e63: c9 41        CalcSineHalf    cmp     #$41              ;$00-40?
5e65: 90 04                        bcc     :Lower            ;yes, branch
5e67: 49 7f                        eor     #$7f              ;no, $41-7f; reflect it ($3e-00)
5e69: 69 00                        adc     #$00              ;add one ($3f-01)
5e6b: 0a           :Lower          asl     A                 ;double it for 16-bit values (could split table)
5e6c: aa                           tax
5e6d: bd 77 5e                     lda     sine_tab,x
5e70: 48                           pha
5e71: bd 78 5e                     lda     sine_tab+1,x
5e74: aa                           tax
5e75: 68                           pla
5e76: 60                           rts

                   ; 
                   ; Table of sines.  There are 65 entries, representing the first quadrant (angles
                   ; 0-90, inclusive).  Each entry is a 16-bit signed value:
                   ; 
                   ;   sine_tab[n] = 32768 * sin(alpha)
                   ; 
                   ; Because this is only the first quadrant, all values are positive.
                   ; 
5e77: 00 00        sine_tab        .dd2    $0000
5e79: 24 03                        .dd2    $0324
5e7b: 47 06                        .dd2    $0647
5e7d: 6a 09                        .dd2    $096a
5e7f: 8b 0c                        .dd2    $0c8b
5e81: ab 0f                        .dd2    $0fab
5e83: c8 12                        .dd2    $12c8
5e85: e2 15                        .dd2    $15e2
5e87: f8 18                        .dd2    $18f8
5e89: 0b 1c                        .dd2    $1c0b
5e8b: 19 1f                        .dd2    $1f19
5e8d: 23 22                        .dd2    $2223
5e8f: 28 25                        .dd2    $2528
5e91: 26 28                        .dd2    $2826
5e93: 1f 2b                        .dd2    $2b1f
5e95: 11 2e                        .dd2    $2e11
5e97: fb 30                        .dd2    $30fb
5e99: de 33                        .dd2    $33de
5e9b: be 36                        .dd2    $36be
5e9d: 8c 39                        .dd2    $398c
5e9f: 56 3c                        .dd2    $3c56
5ea1: 17 3f                        .dd2    $3f17
5ea3: ce 41                        .dd2    $41ce
5ea5: 7a 44                        .dd2    $447a
5ea7: 1c 47                        .dd2    $471c
5ea9: b4 49                        .dd2    $49b4
5eab: 3f 4c                        .dd2    $4c3f
5ead: bf 4e                        .dd2    $4ebf
5eaf: 33 51                        .dd2    $5133
5eb1: 9b 53                        .dd2    $539b
5eb3: f5 55                        .dd2    $55f5
5eb5: 42 58                        .dd2    $5842
5eb7: 82 5a                        .dd2    $5a82
5eb9: b4 5c                        .dd2    $5cb4
5ebb: d7 5e                        .dd2    $5ed7
5ebd: ec 60                        .dd2    $60ec
5ebf: f2 62                        .dd2    $62f2
5ec1: eb 64                        .dd2    $64eb
5ec3: cf 66                        .dd2    $66cf
5ec5: a6 68                        .dd2    $68a6
5ec7: 6d 6a                        .dd2    $6a6d
5ec9: 24 6c                        .dd2    $6c24
5ecb: c4 6d                        .dd2    $6dc4
5ecd: 5f 6f                        .dd2    $6f5f
5ecf: e2 70                        .dd2    $70e2
5ed1: 55 72                        .dd2    $7255
5ed3: b5 73                        .dd2    $73b5
5ed5: 04 75                        .dd2    $7504
5ed7: 41 76                        .dd2    $7641
5ed9: 6c 77                        .dd2    $776c
5edb: 84 78                        .dd2    $7884
5edd: 8a 79                        .dd2    $798a
5edf: 7d 7a                        .dd2    $7a7d
5ee1: 5d 7b                        .dd2    $7b5d
5ee3: 2a 7c                        .dd2    $7c2a
5ee5: e3 7c                        .dd2    $7ce3
5ee7: 8a 7d                        .dd2    $7d8a
5ee9: 1d 7e                        .dd2    $7e1d
5eeb: 9d 7e                        .dd2    $7e9d
5eed: 09 7f                        .dd2    $7f09
5eef: 62 7f                        .dd2    $7f62
5ef1: a7 7f                        .dd2    $7fa7
5ef3: d8 7f                        .dd2    $7fd8
5ef5: f6 7f                        .dd2    $7ff6
5ef7: ff 7f                        .dd2    $7fff             ;can't represent +32768, but this will do

                   ; 
                   ; Computes cos(theta), where theta is a 9-bit value.
                   ; 
                   ; On entry:
                   ;   X-reg: angle upper 8 bits ($00-ff)
                   ;   A-reg: angle LSB ($00/$01)
                   ; 
                   ; On exit:
                   ;   A-reg: sine value, low byte
                   ;   X-reg: sine value, high byte
                   ;   Y-reg preserved
                   ; 
                   • Clear variables
                   ]result1        .var    $08    {addr/2}
                   ]interp         .var    $0a    {addr/2}
                   ]angle          .var    $0c    {addr/1}

5ef9: 29 01        CalcCosine512   and     #$01              ;mask other bits off (not needed?)
5efb: 85 a6                        sta     temp_a6           ;save for later (see calling function)
5efd: d0 04                        bne     :Interpolate      ;if nonzero, we need to interpolate two angles
5eff: 8a                           txa                       ;LSB is zero, just do plain cos(theta)
5f00: 4c 4b 5e                     jmp     CalcCosine

5f03: 8a           :Interpolate    txa
5f04: 85 0c                        sta     ]angle            ;save angle
5f06: 20 4b 5e                     jsr     CalcCosine        ;do regular cosine calc for (angle - 0.5)
5f09: 85 08                        sta     ]result1          ;save result
5f0b: 86 09                        stx     ]result1+1
5f0d: e6 0c                        inc     ]angle            ;increment to angle + 0.5
5f0f: a5 0c                        lda     ]angle
5f11: 20 4b 5e                     jsr     CalcCosine        ;calculate
5f14: 4c 32 5f                     jmp     InterpAngle       ;interpolate

                   ; 
                   ; Computes sin(theta), where theta is a 9-bit value.
                   ; 
                   ; On entry:
                   ;   X-reg: angle upper 8 bits ($00-ff)
                   ;   A-reg: angle LSB ($00/$01)
                   ; 
                   ; On exit:
                   ;   A-reg: sine value, low byte
                   ;   X-reg: sine value, high byte
                   ;   Y-reg preserved
                   ; 
5f17: 29 01        CalcSine512     and     #$01              ;mask other bits off (not needed?)
5f19: 85 a6                        sta     temp_a6           ;save for later (see calling function)
5f1b: d0 04                        bne     :Interpolate      ;if nonzero, we need to interpolate two angles
5f1d: 8a                           txa                       ;LSB is zero, just do plain sin(theta)
5f1e: 4c 4e 5e                     jmp     CalcSine

5f21: 8a           :Interpolate    txa
5f22: 85 0c                        sta     ]angle            ;save angle
5f24: 20 4e 5e                     jsr     CalcSine          ;do regular sine calc for (angle - 0.5)
5f27: 85 08                        sta     ]result1          ;save result
5f29: 86 09                        stx     ]result1+1
5f2b: e6 0c                        inc     ]angle            ;increment to angle + 0.5
5f2d: a5 0c                        lda     ]angle
5f2f: 20 4e 5e                     jsr     CalcSine          ;calculate
                   ; Interpolate first result with what's in A-reg/X-reg.
5f32: 38           InterpAngle     sec                       ;compute difference
5f33: e5 08                        sbc     ]result1
5f35: 85 0a                        sta     ]interp
5f37: 8a                           txa
5f38: e5 09                        sbc     ]result1+1
5f3a: c9 80                        cmp     #$80              ;divide by 2 with sign extension
5f3c: 6a                           ror     A
5f3d: 66 0a                        ror     ]interp
5f3f: 85 0b                        sta     ]interp+1
5f41: a5 08                        lda     ]result1          ;add half the difference to the first result
5f43: 18                           clc
5f44: 65 0a                        adc     ]interp
5f46: 48                           pha
5f47: a5 09                        lda     ]result1+1
5f49: 65 0b                        adc     ]interp+1
5f4b: aa                           tax                       ;return in A-reg/X-reg
5f4c: 68                           pla
5f4d: 60                           rts

                   ; 
                   ; Checks to see if the player's projectile has hit the enemy unit.
                   ; 
                   ; On exit:
                   ;   Carry set if collision
                   ;   X-reg: $00
                   ; 
                   • Clear variables
                   ]tmp_08         .var    $08    {addr/2}
                   ]tmp_0c         .var    $0c    {addr/1}

5f4e: a5 24        TestProj0Coll   lda     projectile_state_0
5f50: 30 02                        bmi     :Exploding
5f52: d0 03                        bne     :InFlight
5f54: 4c 43 60     :Exploding      jmp     :ClcReturn

5f57: a2 00        :InFlight       ldx     #$00              ;$00 = player
5f59: a0 02                        ldy     #$02              ;$02 = enemy unit
                   ; 
                   ; Tests for a collision between a projectile and the opposing unit (i.e. player
                   ; vs. enemy or enemy vs. player).
                   ; 
                   ; Updates the score if the projectile hit.
                   ; 
                   ; On entry:
                   ;   X-reg: projectile owner: $00 (player) or $02 (enemy)
                   ;   Y-reg: projectile target: $02/$00 (opposite of X-reg)
                   ; 
                   ; On exit:
                   ;   MathBox R0/R1 set to projectile X/Z position
                   ;   Carry set if collision
                   ;   X-reg preserved
                   ; 
5f5b: b5 a8        TestProjCollU   lda     proj_pos_z,x      ;put projectile position in R0/R1
5f5d: 8d 60 18                     sta     MB_SET_R0L
5f60: b5 a9                        lda     proj_pos_z+1,x
5f62: 8d 61 18                     sta     MB_SET_R0H
5f65: b5 ac                        lda     proj_pos_x,x
5f67: 8d 62 18                     sta     MB_SET_R1L
5f6a: b5 ad                        lda     proj_pos_x+1,x
5f6c: 8d 63 18                     sta     MB_SET_R1H
5f6f: b9 12 00                     lda     unit_state,y      ;opposing unit alive?
5f72: d0 2a                        bne     :Bail             ;no, bail
5f74: b9 2d 00                     lda     unit_pos_z,y      ;put unit position in R2/R3
5f77: 8d 64 18                     sta     MB_SET_R2L
5f7a: b9 2e 00                     lda     unit_pos_z+1,y
5f7d: 8d 65 18                     sta     MB_SET_R2H
5f80: b9 31 00                     lda     unit_pos_x,y
5f83: 8d 66 18                     sta     MB_SET_R3L
5f86: b9 32 00                     lda     unit_pos_x+1,y
5f89: 8d 7d 18                     sta     MB_CALC_DIST      ;calculate distance between them
5f8c: c1 00                        cmp     ($00,x)           ;stall (6 cycles)
5f8e: b1 00                        lda     ($00),y           ;stall (5 cycles)
5f90: ad 10 18                     lda     MB_RESULT_LO      ;low byte of distance
5f93: 85 0c                        sta     ]tmp_0c
5f95: ad 18 18                     lda     MB_RESULT_HI      ;high byte of distance
5f98: 30 3f                        bmi     :Missed           ;too big, shot missed
5f9a: 4a                           lsr     A                 ;divide by 4
5f9b: 66 0c                        ror     ]tmp_0c
5f9d: 4a                           lsr     A
5f9e: d0 39        :Bail           bne     :Missed           ;high byte still nonzero, it's a miss
5fa0: 66 0c                        ror     ]tmp_0c
5fa2: 24 cb                        bit     missile_flag      ;shooting a missile?
5fa4: 10 0c                        bpl     :NotMissile       ;no, branch
5fa6: ad e4 02                     lda     ypos_by_type+12   ;missile is type $16, so Y pos is at index 6
5fa9: c9 00                        cmp     #$00
5fab: ad e5 02                     lda     ypos_by_type+13
5fae: e9 02                        sbc     #$02              ;is the missile >= $200 units up?
5fb0: b0 27                        bcs     :Missed           ;yes, it's a miss
                   ; We want to compare the distance between the unit and the projectile against
                   ; the radius of the unit.  Units aren't circular, so facing matters.
5fb2: a5 2a        :NotMissile     lda     plyr_facing_hi    ;subtract enemy facing from player facing
5fb4: 38                           sec
5fb5: e5 2c                        sbc     enemy_facing
5fb7: 0a                           asl     A                 ;double and shift sign bit into carry flag
5fb8: 10 05                        bpl     :IsPos
5fba: 49 ff                        eor     #$ff              ;negate to get absolute value
5fbc: 18                           clc
5fbd: 69 01                        adc     #$01
5fbf: 4a           :IsPos          lsr     A                 ;divide by 4 (so net divide by 2)
5fc0: 4a                           lsr     A
5fc1: 24 cb                        bit     missile_flag      ;shooting a missile?
5fc3: 30 03                        bmi     :IsMissile        ;yes
5fc5: 4a                           lsr     A                 ;not missile, divide by 2
5fc6: 10 03                        bpl     :Comm             ;(always)

5fc8: 18           :IsMissile      clc
5fc9: 69 18                        adc     #24               ;is missile, add 24
5fcb: 85 08        :Comm           sta     ]tmp_08           ;multiply by 1.5
5fcd: 4a                           lsr     A
5fce: 18                           clc
5fcf: 65 08                        adc     ]tmp_08
5fd1: 69 38                        adc     #56               ;add 56
5fd3: 85 08                        sta     ]tmp_08
5fd5: c5 0c                        cmp     ]tmp_0c           ;compare to dist
5fd7: b0 03                        bcs     :ProjHitUnit      ;radius > dist, it's a hit
5fd9: 4c 43 60     :Missed         jmp     :ClcReturn

5fdc: a9 20        :ProjHitUnit    lda     #$20              ;mark unit as exploding
5fde: 99 12 00                     sta     unit_state,y
5fe1: a9 00                        lda     #$00              ;reset timeout (used to throw missiles at
5fe3: 85 d2                        sta     frame_count_256x  ; players who run away)
5fe5: 98                           tya                       ;was target player?
5fe6: d0 0e                        bne     :EnemyProj        ;no, branch
5fe8: a9 02                        lda     #$02              ;yes, kill the player
5fea: 85 c7                        sta     death_crack_index
5fec: a9 ff                        lda     #$ff              ;bump camera
5fee: 85 df                        sta     horizon_adj
5ff0: c6 cc                        dec     player_lives      ;reduce number of lives
5ff2: d0 02                        bne     :EnemyProj        ;if some left, branch
5ff4: e6 cd                        inc     game_over_flags   ;no lives left, set game over flag
5ff6: b5 b8        :EnemyProj      lda     score,x           ;get current score
5ff8: 85 08                        sta     ]tmp_08
5ffa: b5 b9                        lda     score+1,x
5ffc: 85 09                        sta     ]tmp_08+1
                   ; Add score for kill.  Note we reference "score,x" -- the enemy gets points
                   ; every time they kill the player.
5ffe: 8a                           txa                       ;was the projectile owned by player?
5fff: d0 0d                        bne     :EnemyProj2       ;no, branch
6001: a9 02                        lda     #$02              ;2000 points
6003: 24 cb                        bit     missile_flag
6005: 30 09                        bmi     :AddScore
6007: 20 b5 69                     jsr     GetTankType
600a: a9 03                        lda     #$03              ;3000 points
600c: b0 02                        bcs     :AddScore
600e: a9 01        :EnemyProj2     lda     #$01              ;1000 points
6010: 18           :AddScore       clc
6011: f8                           sed                       ;switch to decimal mode for BCD calculation
6012: 75 b8                        adc     score,x           ;add kill score to total
6014: 95 b8                        sta     score,x
6016: b5 b9                        lda     score+1,x
6018: 69 00                        adc     #$00
601a: d8                           cld                       ;back to normal
601b: 95 b9                        sta     score+1,x
601d: 8a                           txa                       ;was the projectile owned by player?
601e: d0 03                        bne     :EnemyProj3       ;no, branch
6020: 20 5f 61                     jsr     CheckAwardLife    ;see if score merits a bonus tank
6023: a9 80        :EnemyProj3     lda     #$80
6025: 95 24                        sta     projectile_state_0,x ;set projectile state to "exploding"
                   ; Play an explosion sound.  It's soft for the owner of the projectile and loud
                   ; for the unit that got hit, so two values are updated.  The enemy unit can't
                   ; actually hear, so the value written to $17 isn't used.
                   ; 
                   ; (This could be written with less code.  Possibly done this way to support a
                   ; head-to-head 2-player mode.)
6027: a9 ff                        lda     #$ff              ;explosion sound for ~1 sec
6029: 99 0f 00                     sta     dsnd_expl_ctr,y
602c: a9 02                        lda     #$02              ;d-sound: explosion=loud
602e: 19 15 00                     ora     dsnd_ctrl_val,y
6031: 99 15 00                     sta     dsnd_ctrl_val,y
6034: b5 15                        lda     dsnd_ctrl_val,x
6036: 29 fd                        and     #%11111101        ;d-sound: explosion=soft
6038: 95 15                        sta     dsnd_ctrl_val,x
603a: 20 99 61                     jsr     InitUnitChunks
603d: a9 70                        lda     #$70              ;explosion sound for ~0.5 sec
603f: 95 0f                        sta     dsnd_expl_ctr,x
6041: 38                           sec
6042: 60                           rts

6043: 18           :ClcReturn      clc
6044: 60                           rts

                   ; 
                   ; Checks to see if one of the projectiles has hit something (enemy unit, saucer,
                   ; obstacle).
                   ; 
                   • Clear variables
                   ]old_score      .var    $08    {addr/2}
                   ]tmp_0c         .var    $0c    {addr/1}

6045: a2 02        CheckProjColl   ldx     #$02              ;start with projectile #1 (enemy)
6047: 8a           :ProjLoop       txa
6048: 49 02                        eor     #$02              ;X-reg is $00/$02, set Y-reg to $02/$00
604a: a8                           tay
604b: b5 24                        lda     projectile_state_0,x
604d: 30 6b                        bmi     :NextProj         ;exploding, move on
604f: f0 69                        beq     :NextProj         ;not active, move on
6051: a5 c7                        lda     death_crack_index ;player death animation in progress?
6053: d0 04                        bne     :DoCheck          ;yes, keep checking (player can still get a kill)
6055: a5 cd                        lda     game_over_flags   ;is the game over?
6057: d0 61                        bne     :NextProj         ;yes, nothing to see here
6059: 20 5b 5f     :DoCheck        jsr     TestProjCollU     ;check vs. unit first; sets MB R0/R1 to projectile
605c: b0 5c                        bcs     :NextProj         ;branch if we hit
605e: 4c c2 60                     jmp     :CheckSaucer      ;check saucer second

6061: a0 00        :CheckObst      ldy     #$00              ;check obstacles third
6063: b9 cc 3f     :ObstLoop       lda     obstacle_t_f,y    ;get the obstacle type
6066: 30 52                        bmi     :NextProj         ;end of list, branch
6068: b9 81 76                     lda     obstacle_z_pos,y  ;copy position to R2/R3
606b: 8d 64 18                     sta     MB_SET_R2L
606e: b9 82 76                     lda     obstacle_z_pos+1,y
6071: 8d 65 18                     sta     MB_SET_R2H
6074: b9 ab 76                     lda     obstacle_x_pos,y
6077: 8d 66 18                     sta     MB_SET_R3L
607a: b9 ac 76                     lda     obstacle_x_pos+1,y
607d: 8d 7d 18                     sta     MB_CALC_DIST      ;calculate distance between projectile and obstacle
6080: c1 00                        cmp     ($00,x)           ;stall (6 cycles)
6082: ad 10 18                     lda     MB_RESULT_LO
6085: 85 0c                        sta     ]tmp_0c
6087: ad 18 18                     lda     MB_RESULT_HI
608a: c9 80                        cmp     #$80              ;divide by 4 with sign-extension
608c: 6a                           ror     A
608d: 66 0c                        ror     ]tmp_0c
608f: c9 80                        cmp     #$80
6091: 6a                           ror     A
6092: 66 0c                        ror     ]tmp_0c
6094: c9 00                        cmp     #$00
6096: d0 1e                        bne     :Miss             ;too far, branch
6098: 86 a6                        stx     temp_a6           ;save X-reg
609a: be cc 3f                     ldx     obstacle_t_f,y    ;get obstacle type
609d: bd 39 61                     lda     obst_proj_diam,x  ;get obstacle vs. projectile diameter
60a0: a6 a6                        ldx     temp_a6           ;restore X-reg
60a2: c5 0c                        cmp     ]tmp_0c           ;are we close?
60a4: 90 10                        bcc     :Miss             ;diameter is less, this was a miss
                   ; Projectile struck obstacle.
60a6: a9 a0                        lda     #$a0              ;set state to exploding
60a8: 95 24                        sta     projectile_state_0,x
60aa: a9 70                        lda     #$70              ;set explosion sound counter
60ac: 85 0f                        sta     dsnd_expl_ctr
60ae: a5 15                        lda     dsnd_ctrl_val
60b0: 29 fd                        and     #%11111101        ;d-sound: explosion=soft
60b2: 85 15                        sta     dsnd_ctrl_val
60b4: b0 04                        bcs     :NextProj         ;(always)

60b6: c8           :Miss           iny
60b7: c8                           iny
60b8: d0 a9                        bne     :ObstLoop
60ba: ca           :NextProj       dex
60bb: ca                           dex
60bc: d0 03                        bne     :Return
60be: 4c 47 60                     jmp     :ProjLoop

60c1: 60           :Return         rts

60c2: a5 de        :CheckSaucer    lda     saucer_state      ;saucer on battlefield?
60c4: f0 33                        beq     :NotSaucer        ;no, branch
60c6: a5 d4                        lda     saucer_dead_intens ;saucer dying?
60c8: d0 2f                        bne     :NotSaucer        ;yes, one per customer
60ca: a5 d5                        lda     saucer_z          ;put saucer coordinates in R2/R3
60cc: 8d 64 18                     sta     MB_SET_R2L
60cf: a5 d6                        lda     saucer_z+1
60d1: 8d 65 18                     sta     MB_SET_R2H
60d4: a5 d7                        lda     saucer_x
60d6: 8d 66 18                     sta     MB_SET_R3L
60d9: a5 d8                        lda     saucer_x+1
60db: 8d 67 18                     sta     MB_SET_R3H
60de: 8d 7d 18                     sta     MB_CALC_DIST      ;calculate distance
60e1: c1 00                        cmp     ($00,x)           ;stall (6 cycles)
60e3: ad 10 18                     lda     MB_RESULT_LO
60e6: 85 0c                        sta     ]tmp_0c
60e8: ad 18 18                     lda     MB_RESULT_HI
60eb: c9 80                        cmp     #$80              ;divide by 4 with sign-extension
60ed: 6a                           ror     A
60ee: 66 0c                        ror     ]tmp_0c
60f0: c9 80                        cmp     #$80
60f2: 6a                           ror     A
60f3: 66 0c                        ror     ]tmp_0c
60f5: c9 00                        cmp     #$00              ;high byte zero?
60f7: f0 03                        beq     :CloseToSaucer    ;yes, keep checking
60f9: 4c 61 60     :NotSaucer      jmp     :CheckObst

60fc: a9 90        :CloseToSaucer  lda     #$90              ;test vs. saucer diameter
60fe: c5 0c                        cmp     ]tmp_0c
6100: 90 f7                        bcc     :NotSaucer
                   ; Projectile struck saucer.
6102: a9 40                        lda     #$40              ;init intensity for brighten/dim
6104: 85 d4                        sta     saucer_dead_intens
6106: a9 20                        lda     #$20              ;sound: saucer hit
6108: 20 85 79                     jsr     StartSoundEffect
610b: a9 a0                        lda     #$a0              ;projectile is exploding
610d: 95 24                        sta     projectile_state_0,x
610f: 95 0f                        sta     dsnd_expl_ctr,x   ;make explosion sound
6111: a9 02                        lda     #$02              ;d-sound: explosion=loud
6113: 19 15 00                     ora     dsnd_ctrl_val,y
6116: 99 15 00                     sta     dsnd_ctrl_val,y
6119: 8a                           txa                       ;was this the player's projectile?
611a: d0 9e                        bne     :NextProj         ;no, so no points for you
                   ; Update score for saucer strike.
611c: a5 b8                        lda     score
611e: 85 08                        sta     ]old_score
6120: a5 b9                        lda     score+1
6122: 85 09                        sta     ]old_score+1
6124: a9 05                        lda     #$05              ;5000 points
6126: 18                           clc
6127: f8                           sed
6128: 65 b8                        adc     score
612a: 85 b8                        sta     score
612c: a5 b9                        lda     score+1
612e: 69 00                        adc     #$00
6130: d8                           cld
6131: 85 b9                        sta     score+1
6133: 20 5f 61                     jsr     CheckAwardLife    ;see if they've earned an extra life
6136: 4c ba 60                     jmp     :NextProj

                   ; 
                   ; Obstacle diameters, for projectile collision detection.
                   ; 
6139: 38           obst_proj_diam  .dd1    $38               ;narrow pyramid
613a: 58                           .dd1    $58               ;tall box
613b: 00                           .dd1    $00
613c: 00                           .dd1    $00
613d: 00                           .dd1    $00
613e: 00                           .dd1    $00
613f: 00                           .dd1    $00
6140: 00                           .dd1    $00
6141: 00                           .dd1    $00
6142: 00                           .dd1    $00
6143: 00                           .dd1    $00
6144: 00                           .dd1    $00
6145: 56                           .dd1    $56               ;wide pyramid
6146: 00                           .dd1    $00
6147: 00                           .dd1    $00
6148: 00                           .dd1    $00               ;short box

                   ; 
                   ; Computes the absolute value of $08/09, negating it if its negative.
                   ; 
                   • Clear variables
                   ]value          .var    $08    {addr/2}

6149: 24 09        CalcAbs08       bit     ]value+1          ;check sign
614b: 10 11                        bpl     :Return           ;positive, do nothing
614d: a5 08                        lda     ]value            ;invert and add one
614f: 18                           clc
6150: 49 ff                        eor     #$ff
6152: 69 01                        adc     #$01
6154: 85 08                        sta     ]value
6156: a5 09                        lda     ]value+1
6158: 49 ff                        eor     #$ff
615a: 69 00                        adc     #$00
615c: 85 09                        sta     ]value+1
615e: 60           :Return         rts

                   ; 
                   ; Awards a bonus tank if newly-awarded points cause the score to cross a
                   ; threshold.
                   ; 
                   ; On entry:
                   ;   $08/09: old score
                   ; 
                   ; On exit:
                   ;   X-reg: $00
                   ;   Y-reg preserved
                   ; 
                   • Clear variables
                   ]old_score      .var    $08    {addr/2}

615f: ad 00 0a     CheckAwardLife  lda     DSW0              ;get DIP switch value
6162: 4a                           lsr     A                 ;shift to get bonus tank setting
6163: 4a                           lsr     A
6164: 4a                           lsr     A
6165: 4a                           lsr     A
6166: 29 03                        and     #%00000011        ;mask off everything else
6168: aa                           tax
6169: bd 86 38                     lda     bonus_tank_score,x ;get score threshold minus 1, in BCD
616c: f0 17                        beq     :NoBonus          ;if zero, no bonus tanks awarded ever
616e: a6 b9                        ldx     score+1           ;check high byte of score
6170: d0 16                        bne     :Score100K        ;nonzero, so >= 100K
6172: c5 08                        cmp     ]old_score        ;high zero; compare low to old score
6174: 90 0f                        bcc     :NoBonus          ;old score was higher, no more bonuses
6176: c5 b8                        cmp     score             ;compare low byte to new score
6178: b0 0b                        bcs     :NoBonus          ;new score <= (bonus-1), no bonus
617a: e6 cc                        inc     player_lives      ;award bonus
617c: a9 00                        lda     #$00              ;if our dying shot killed something and awarded an
617e: 85 cd                        sta     game_over_flags   ; extra life, we need to clear the game-over flag
6180: a9 08                        lda     #$08              ;sound: 4 high-pitched beeps
6182: 20 85 79     :Call           jsr     StartSoundEffect
6185: a2 00        :NoBonus        ldx     #$00
6187: 60                           rts

6188: a5 09        :Score100K      lda     ]old_score+1      ;was the old score >= 100K?
618a: d0 f9                        bne     :NoBonus          ;yes, no bonus
618c: 85 cd                        sta     game_over_flags   ;A-reg=0, clears game-over flag
618e: e6 cc                        inc     player_lives      ;award bonus
6190: a9 ff                        lda     #$ff              ;set flag so we play an explosion after the
6192: 8d 45 03                     sta     score_100k_flag   ; 1812 Overture finishes
6195: a9 80                        lda     #$80              ;sound: 1812 Overture
6197: d0 e9                        bne     :Call             ;(always)

                   ; 
                   ; Initializes flying chunks after an enemy unit is destroyed.  This sets the
                   ; initial positions and Y velocity of the 6 chunks.
                   ; 
                   ; The positions and X/Z velocities are pre-determined.  The high byte of the
                   ; initial Y velocity is fixed but the low byte is random.
                   ; 
                   ; The value in X-reg indicates the projectile's owner, not the exploding unit,
                   ; so if it's not $00 there's nothing for us to do.
                   ; 
                   ; On entry:
                   ;   X-reg: $00 (player) or $02 (enemy)
                   ; 
6199: 8a           InitUnitChunks  txa                       ;projectile fired by player?
619a: d0 33                        bne     :Return           ;no, by enemy; bail
619c: a2 05                        ldx     #$05              ;6 items (0-5)
619e: a0 0a                        ldy     #$0a              ;6 items (0-10 by 2)
61a0: bd c6 3f     :InitLoop       lda     chunk_initial_y_vel,x
61a3: 99 c9 02                     sta     chunk_y_vel+1,y
61a6: a5 2f                        lda     enemy_pos_z
61a8: 99 a8 02                     sta     chunk_z_pos,y
61ab: a5 30                        lda     enemy_pos_z+1
61ad: 99 a9 02                     sta     chunk_z_pos+1,y
61b0: a5 33                        lda     enemy_pos_x
61b2: 99 b8 02                     sta     chunk_x_pos,y
61b5: a5 34                        lda     enemy_pos_x+1
61b7: 99 b9 02                     sta     chunk_x_pos+1,y
61ba: ad 2a 18                     lda     POKEY_RANDOM      ;inject some randomness into the low byte
61bd: 99 c8 02                     sta     chunk_y_vel,y     ; of the Y velocity
61c0: a9 00                        lda     #$00              ;initial height is zero
61c2: 99 d8 02                     sta     ypos_by_type,y
61c5: 99 d9 02                     sta     ypos_by_type+1,y
61c8: 88                           dey
61c9: 88                           dey
61ca: ca                           dex
61cb: 10 d3                        bpl     :InitLoop
61cd: a2 00                        ldx     #$00              ;restore X-reg
61cf: 60           :Return         rts

                   ; 
                   ; Updates the player's position, based on controller reads (if playing) or
                   ; timing (if in attract mode).
                   ; 
61d0: 24 ce        UpdatePlayer    bit     play_flag         ;are we playing?
61d2: 30 24                        bmi     :IsPlaying        ;yes, go read the controllers
61d4: 2c 4b 03                     bit     attract_logo_flag ;are we showing the logo?
61d7: 30 25                        bmi     :Return           ;yes, don't move
61d9: a5 c7                        lda     death_crack_index ;are we showing the player death animation/
61db: d0 21                        bne     :Return           ;yes, don't move
61dd: a2 00                        ldx     #$00              ;$00 = player
61df: 24 c6                        bit     frame_counter     ;alternate fwd/back every $40 game frames
61e1: 70 06                        bvs     :GoBack
61e3: 20 51 63                     jsr     MoveForward
61e6: 4c ec 61                     jmp     :Turn

61e9: 20 6f 63     :GoBack         jsr     MoveBackward
61ec: 24 30        :Turn           bit     enemy_pos_z+1     ;check enemy position
61ee: 10 05                        bpl     :TurnRight        ;use that to decide whether to pivot right or left
61f0: 50 0c                        bvc     :Return
61f2: 4c 8d 63                     jmp     RotateLeft

61f5: 4c 9b 63     :TurnRight      jmp     RotateRight

61f8: a2 00        :IsPlaying      ldx     #$00              ;$00 = player
61fa: b5 12                        lda     unit_state,x      ;is player alive?
61fc: f0 01                        beq     ReadPlayerCtrls   ;yes, read game controllers
61fe: 60           :Return         rts

                   ; 
                   ; Reads player controls (joysticks and fire button).
                   ; 
                   ; On entry:
                   ;   X-reg: $00 (player)
                   ; 
                   • Clear variables
                   ]tmp            .var    $08    {addr/1}
                   ]saved_xreg     .var    $13    {addr/1}

61ff: 20 a9 63     ReadPlayerCtrls jsr     CopyPosToC0       ;copy player position to $c0-c3
6202: 8e 2b 18                     stx     POKEY_POTGO       ;trigger inputs
6205: ad 28 18                     lda     POKEY_ALLPOT      ;get joystick and button state
6208: 8d eb 02                     sta     input_ctrl_state  ;save so we can check fire button later
620b: 49 0f                        eor     #$0f              ;invert joysticks
620d: 29 0f                        and     #$0f              ;reduce to joystick only
                   ; Joysticks are low four bits, LLRR: 00=middle, 10=fwd, 01=back.  After
                   ; inversion, 11=middle, 01=fwd, 10=back.  Because 00 is not a valid value, the
                   ; smallest possible value is 0101 = 5.
620f: c9 05                        cmp     #$05              ;< 5?  (should be impossible)
6211: 90 7f                        bcc     :NoMovement       ;yes, branch
6213: 0a                           asl     A                 ;5-15 -> 10-30
6214: a8                           tay
6215: b9 15 62                     lda     joystick_handlers-9,y
6218: 48                           pha
6219: b9 14 62                     lda     joystick_handlers-10,y
621c: 48                           pha
621d: 60                           rts

                   joystick_handlers
621e: 5d 62                        .dd2    :FwdFwd-1         ;0101: both fwd
6220: 33 62                        .dd2    :FwdBack-1        ;0110: left fwd, right back
6222: 51 62                        .dd2    :FwdMiddle-1      ;0111: left fwd, right middle
6224: 91 62                        .dd2    :NoMovement-1     ;1000: (impossible)
6226: 3c 62                        .dd2    :BackFwd-1        ;1001: left back, right fwd
6228: 68 62                        .dd2    :BackBack-1       ;1010: both back
622a: 4b 62                        .dd2    :BackMiddle-1     ;1011: left back, right middle
622c: 91 62                        .dd2    :NoMovement-1     ;1100: (impossible)
622e: 45 62                        .dd2    :MiddleFwd-1      ;1101: left middle, right fwd
6230: 57 62                        .dd2    :MiddleBack-1     ;1110: left middle, right back
6232: 91 62                        .dd2    :NoMovement-1     ;1111: both middle

6234: 20 9b 63     :FwdBack        jsr     RotateRight       ;rotate right 2 units
6237: 20 9b 63                     jsr     RotateRight
623a: 4c 8a 62                     jmp     :ClearCol

623d: 20 8d 63     :BackFwd        jsr     RotateLeft        ;rotate left 2 units
6240: 20 8d 63                     jsr     RotateLeft
6243: 4c 8a 62                     jmp     :ClearCol

6246: 20 8d 63     :MiddleFwd      jsr     RotateLeft        ;rotate left 1 unit
6249: 4c 61 62                     jmp     :ForwardOnce      ;and move forward

624c: 20 8d 63     :BackMiddle     jsr     RotateLeft        ;rotate left 1 unit
624f: 4c 6c 62                     jmp     :BackOnce         ;and move backward

6252: 20 9b 63     :FwdMiddle      jsr     RotateRight       ;rotate right 1 unit
6255: 4c 61 62                     jmp     :ForwardOnce      ;and move forward

6258: 20 9b 63     :MiddleBack     jsr     RotateRight       ;rotate right 1 unit
625b: 4c 6c 62                     jmp     :BackOnce         ;and move backward

625e: 20 51 63     :FwdFwd         jsr     MoveForward       ;move forward 2 steps
6261: 20 51 63     :ForwardOnce    jsr     MoveForward
6264: f6 a5                        inc     tread_drip_ctr_i,x ;update tread (not used)
6266: 4c 71 62                     jmp     :CheckColl

6269: 20 6f 63     :BackBack       jsr     MoveBackward      ;move backward 2 steps
626c: 20 6f 63     :BackOnce       jsr     MoveBackward
626f: d6 a5                        dec     tread_drip_ctr_i,x ;update tread (not used)
                   ; 
6271: 20 d6 68     :CheckColl      jsr     CheckObstUnitColl ;check to see if we ran into something
6274: 90 14                        bcc     :ClearCol         ;no, good
6276: 20 ba 63                     jsr     CopyPosFromC0     ;yes, restore original position
6279: a5 c8                        lda     recent_coll_flag  ;did we just do this?
627b: d0 11                        bne     :RevUp            ;yes, don't play collision sound
627d: a9 02                        lda     #$02              ;sound: collided with object
627f: 20 85 79                     jsr     StartSoundEffect
6282: a9 3f                        lda     #$3f              ;do horizon shift effect
6284: 85 df                        sta     horizon_adj
6286: e6 c8                        inc     recent_coll_flag  ;set to 1
6288: d0 04                        bne     :RevUp            ;(always)

628a: a9 00        :ClearCol       lda     #$00              ;clear the recent-collision flag
628c: 85 c8                        sta     recent_coll_flag
628e: a9 10        :RevUp          lda     #$10              ;d-sound: engine rev up
6290: d0 02                        bne     :Cont             ;(always)

6292: a9 00        :NoMovement     lda     #$00              ;d-sound: engine rev down
6294: 85 08        :Cont           sta     ]tmp
6296: bd 15 00                     lda:    dsnd_ctrl_val,x
6299: 29 ef                        and     #%11101111        ;mask engine rev bit
629b: 05 08                        ora     ]tmp              ;OR in the new value
629d: 95 15                        sta     dsnd_ctrl_val,x
                   ; Check to see if player wants to fire the cannon.
629f: b5 24                        lda     projectile_state_0,x ;is projectile 0 active?
62a1: d0 6c                        bne     :Return           ;yes, can't fire now
62a3: ad eb 02                     lda     input_ctrl_state  ;get POKEY input values we grabbed earlier
62a6: 29 10                        and     #$10              ;check fire button
62a8: f0 65                        beq     :Return           ;not pressed, bail
62aa: a9 7f                        lda     #$7f              ;init TTL to max (about 2 seconds)
62ac: 95 24                        sta     projectile_state_0,x ;make as alive
62ae: b5 15                        lda     dsnd_ctrl_val,x
62b0: 09 08                        ora     #$08              ;d-sound: cannon=loud
62b2: 95 15                        sta     dsnd_ctrl_val,x
62b4: a9 05                        lda     #$05
62b6: 85 bd                        sta     dsnd_cnon_ctr     ;play briefly
                   ; Configure projectile velocity.  We use the sin/cos value ($0000-ffff) right-
                   ; shifted 7x with sign-extension ($0000-00ff or $ffff-ff00).  Projectiles move
                   ; 4x per frame, so with a time-to-live of $7f, the maximum distance traveled is
                   ; 256*127=32512 ($7f00), which is a bit past the "far" plane (even if moving
                   ; forward?).  The range is slightly shorter when the player is shooting down an
                   ; axis because the sin/cos table doesn't represent 1.0 precisely.
62b8: b5 27                        lda     plyr_facing_lo,x  ;get facing direction, low ($00 or $80)
62ba: 18                           clc                       ;roll it into the low bit ($00 or $01)
62bb: 2a                           rol     A
62bc: 2a                           rol     A
62bd: 48                           pha                       ;spill to stack
62be: b5 2a                        lda     plyr_facing_hi,x  ;get facing direction, high
62c0: 86 13                        stx     ]saved_xreg       ;save the X-reg (which is always $02)
62c2: aa                           tax
62c3: 68                           pla
62c4: 20 f9 5e                     jsr     CalcCosine512     ;compute cos(angle)
62c7: 85 08                        sta     ]tmp              ;save low byte
62c9: 8a                           txa                       ;put high byte in A-reg
62ca: 08                           php                       ;save N-flag for high byte
62cb: a6 13                        ldx     ]saved_xreg       ;restore X-reg ($02 for player)
62cd: 95 b0                        sta     proj_vel_z,x      ;save high byte of projectile velocity
62cf: 28                           plp                       ;restore N-flag
62d0: 30 04                        bmi     :CosNeg           ;was negative, sign-extend with $ff
62d2: a9 00                        lda     #$00              ;positive, sign-extend with $00
62d4: f0 02                        beq     :CosCommon

62d6: a9 ff        :CosNeg         lda     #$ff
62d8: 06 08        :CosCommon      asl     ]tmp              ;the sign-extended high-byte is the value right
62da: 36 b0                        rol     proj_vel_z,x      ; shifted 8x; shift once left to get right-shift by 7
62dc: 2a                           rol     A
62dd: 95 b1                        sta     proj_vel_z+1,x
                   ; Now do the same for sin(value).
62df: b5 2a                        lda     plyr_facing_hi,x
62e1: aa                           tax
62e2: a5 a6                        lda     temp_a6           ;BUG? missing "sta $a6" earlier?
62e4: 20 17 5f                     jsr     CalcSine512       ;(only low bit matters; potentially off by 0.7 deg)
62e7: 85 08                        sta     ]tmp
62e9: 8a                           txa
62ea: 08                           php
62eb: a6 13                        ldx     ]saved_xreg
62ed: 95 b4                        sta     proj_vel_x,x
62ef: 28                           plp
62f0: 30 04                        bmi     :SinNeg
62f2: a9 00                        lda     #$00
62f4: f0 02                        beq     :SinCommon

62f6: a9 ff        :SinNeg         lda     #$ff
62f8: 06 08        :SinCommon      asl     ]tmp
62fa: 36 b4                        rol     proj_vel_x,x
62fc: 2a                           rol     A
62fd: 95 b5                        sta     proj_vel_x+1,x
                   ; Finally, set the projectile's position equal to the player's position.  It
                   ; won't actually be visible until it passes the "near" plane.
62ff: b5 2d                        lda     unit_pos_z,x
6301: 95 a8                        sta     proj_pos_z,x
6303: b5 2e                        lda     unit_pos_z+1,x
6305: 95 a9                        sta     proj_pos_z+1,x
6307: b5 31                        lda     unit_pos_x,x
6309: 95 ac                        sta     proj_pos_x,x
630b: b5 32                        lda     unit_pos_x+1,x
630d: 95 ad                        sta     proj_pos_x+1,x
630f: 60           :Return         rts

                   ; 
                   ; Calculate the X/Z components of forward motion for the player or enemy tank. 
                   ; (Missiles use a different function.)
                   ; 
                   ; This takes the high byte of the cos/sin values and multiplies by 3/4, yielding
                   ; a maximum distance of $60 (96).  The actual values may be slightly off due to
                   ; approximations and rounding.  For example, if you're driving straight toward
                   ; +X, the cosine value will be $7fff which becomes $5e.
                   ; 
                   ; Further, this doesn't handle divide-by-two of signed integers correctly
                   ; (should inc before shift if negative), so off-by-one errors are possible
                   ; depending on facing.
                   ; 
                   ; On entry:
                   ;   X-reg: $00 (player) or $02 (enemy)
                   ; 
                   ; On exit:
                   ;   $19/1a: delta Z (signed 16-bit value)
                   ;   $1d/1e: delta X (signed 16-bit value)
                   ;   X-reg preserved
                   ; 
                   ]saved_xreg     .var    $08    {addr/1}
                   ]move_z         .var    $19    {addr/2}
                   ]move_x         .var    $1d    {addr/2}

6310: b5 2a        CalcMoveDelta   lda     plyr_facing_hi,x  ;get facing angle
6312: 86 08                        stx     ]saved_xreg
6314: 20 4e 5e                     jsr     CalcSine          ;compute sin(theta)
6317: 8a                           txa                       ;put high byte in A-reg
6318: c9 80                        cmp     #$80              ;divide by 2 with sign extension
631a: 6a                           ror     A
631b: 85 1d                        sta     ]move_x           ;save as delta X
631d: c9 80                        cmp     #$80              ;divide again
631f: 6a                           ror     A
6320: 18                           clc
6321: 65 1d                        adc     ]move_x           ;add to previous to get 3/4 value
6323: 85 1d                        sta     ]move_x
6325: 10 04                        bpl     :IsPos
6327: a9 ff                        lda     #$ff
6329: 30 02                        bmi     :IsNeg

632b: a9 00        :IsPos          lda     #$00
632d: 85 1e        :IsNeg          sta     ]move_x+1
632f: a6 08                        ldx     ]saved_xreg
6331: b5 2a                        lda     plyr_facing_hi,x  ;get facing angle
6333: 20 4b 5e                     jsr     CalcCosine        ;compute cos(theta)
6336: 8a                           txa                       ;put high byte in A-reg
6337: c9 80                        cmp     #$80              ;divide by 2 with sign extension
6339: 6a                           ror     A
633a: 85 19                        sta     ]move_z           ;save as delta Z
633c: c9 80                        cmp     #$80              ;divide again
633e: 6a                           ror     A
633f: 18                           clc
6340: 65 19                        adc     ]move_z           ;add to previous to get 3/4 value
6342: 85 19                        sta     ]move_z
6344: 10 04                        bpl     :IsPos
6346: a9 ff                        lda     #$ff
6348: 30 02                        bmi     :IsNeg

634a: a9 00        :IsPos          lda     #$00
634c: 85 1a        :IsNeg          sta     ]move_z+1
634e: a6 08                        ldx     ]saved_xreg
6350: 60                           rts

                   ; 
                   ; Moves the unit forward.
                   ; 
                   ; On entry:
                   ;   X-reg: $00 for player, $02 for enemy unit
                   ; 
6351: 20 10 63     MoveForward     jsr     CalcMoveDelta     ;compute delta X and delta Z
                   ; 
                   ; Moves the unit by a specified amount.  Ignores facing.
                   ; 
                   ; On entry:
                   ;   X-reg: $00 for player, $02 for enemy unit
                   ;   $19/1a: delta Z
                   ;   $1d/1e: delta X
                   ; 
6354: b5 2d        MoveUnit        lda     unit_pos_z,x      ;add movement delta to position
6356: 18                           clc
6357: 65 19                        adc     ]move_z
6359: 95 2d                        sta     unit_pos_z,x
635b: b5 2e                        lda     unit_pos_z+1,x
635d: 65 1a                        adc     ]move_z+1
635f: 95 2e                        sta     unit_pos_z+1,x
6361: b5 31                        lda     unit_pos_x,x
6363: 18                           clc
6364: 65 1d                        adc     ]move_x
6366: 95 31                        sta     unit_pos_x,x
6368: b5 32                        lda     unit_pos_x+1,x
636a: 65 1e                        adc     ]move_x+1
636c: 95 32                        sta     unit_pos_x+1,x
636e: 60                           rts

                   ; 
                   ; Moves the unit backward.
                   ; 
                   ; On entry:
                   ;   X-reg: $00 for player, $02 for enemy unit
                   ; 
636f: 20 10 63     MoveBackward    jsr     CalcMoveDelta     ;calculate forward movement
6372: b5 2d                        lda     unit_pos_z,x      ;subtract movement delta from position
6374: 38                           sec
6375: e5 19                        sbc     ]move_z
6377: 95 2d                        sta     unit_pos_z,x
6379: b5 2e                        lda     unit_pos_z+1,x
637b: e5 1a                        sbc     ]move_z+1
637d: 95 2e                        sta     unit_pos_z+1,x
637f: 38                           sec
6380: b5 31                        lda     unit_pos_x,x
6382: e5 1d                        sbc     ]move_x
6384: 95 31                        sta     unit_pos_x,x
6386: b5 32                        lda     unit_pos_x+1,x
6388: e5 1e                        sbc     ]move_x+1
638a: 95 32                        sta     unit_pos_x+1,x
638c: 60                           rts

                   ; 
                   ; Rotates the unit to the left.
                   ; 
                   ; On entry:
                   ;   X-reg: $00 for player, $02 for enemy unit
                   ; 
638d: a9 80        RotateLeft      lda     #$80              ;add 1 unit to 9-bit facing angle
638f: 18                           clc
6390: 75 27                        adc     plyr_facing_lo,x
6392: 95 27                        sta     plyr_facing_lo,x
6394: a9 00                        lda     #$00
6396: 75 2a                        adc     plyr_facing_hi,x
6398: 95 2a                        sta     plyr_facing_hi,x
639a: 60                           rts

                   ; 
                   ; Rotates the unit to the left.
                   ; 
                   ; On entry:
                   ;   X-reg: $00 for player, $02 for enemy unit
                   ; 
639b: b5 27        RotateRight     lda     plyr_facing_lo,x  ;subtract 1 unit from 9-bit facing angle
639d: 38                           sec
639e: e9 80                        sbc     #$80
63a0: 95 27                        sta     plyr_facing_lo,x
63a2: b5 2a                        lda     plyr_facing_hi,x
63a4: e9 00                        sbc     #$00
63a6: 95 2a                        sta     plyr_facing_hi,x
63a8: 60                           rts

                   ; 
                   ; Copies the position of the player or the enemy unit to $c0-c3.
                   ; 
                   ; On entry:
                   ;   X-reg: $00 (player) or $02 (enemy)
                   ; 
                   ; On exit:
                   ;   X-reg preserved
                   ; 
63a9: b5 2d        CopyPosToC0     lda     unit_pos_z,x
63ab: 85 c0                        sta     saved_obj_pos
63ad: b5 2e                        lda     unit_pos_z+1,x
63af: 85 c1                        sta     saved_obj_pos+1
63b1: b5 31                        lda     unit_pos_x,x
63b3: 85 c2                        sta     saved_obj_pos+2
63b5: b5 32                        lda     unit_pos_x+1,x
63b7: 85 c3                        sta     saved_obj_pos+3
63b9: 60                           rts

                   ; 
                   ; Copies the position of the player or the enemy unit from $c0-c3.  This is used
                   ; to "undo" a move after colliding with an obstacle or opposing unit.
                   ; 
                   ; On entry:
                   ;   X-reg: $00 (player) or $02 (enemy)
                   ; 
                   ; On exit:
                   ;   X-reg preserved
                   ; 
63ba: a5 c0        CopyPosFromC0   lda     saved_obj_pos
63bc: 95 2d                        sta     unit_pos_z,x
63be: a5 c1                        lda     saved_obj_pos+1
63c0: 95 2e                        sta     unit_pos_z+1,x
63c2: a5 c2                        lda     saved_obj_pos+2
63c4: 95 31                        sta     unit_pos_x,x
63c6: a5 c3                        lda     saved_obj_pos+3
63c8: 95 32                        sta     unit_pos_x+1,x
63ca: 60                           rts

                   ; 
                   ; Updates the move counter and "close" firing angle.  This code seems like it
                   ; was intended to do more than it actually does.  Only used for tanks.
                   ; 
                   ; This is only called with $00 when the enemy is out-scoring the player.
                   ; 
                   ; On entry:
                   ;   A-reg: $00 for long movements, $80 for short movements
                   ; 
                   ; On exit:
                   ;   A-reg/X-reg/Y-reg preserved
                   ; 
                   UpdateMoveCtrAndAng
63cb: 48                           pha                       ;preserve A-reg ($00 or $80)
63cc: 24 ce                        bit     play_flag         ;are we playing?
63ce: 30 1d                        bmi     :DoPlay           ;yes
63d0: a9 30                        lda     #$30              ;no, just use basic movement
63d2: 85 c4                        sta     move_counter
63d4: d0 3a                        bne     :Use0a            ;(always)

63d6: a5 b9        unref_63d6?     lda     score+1           ;nothing calls this
63d8: f0 06                        beq     unused_63e0
63da: a9 7f                        lda     #$7f
63dc: 85 cf                        sta     close_firing_angle?
63de: d0 1d                        bne     :Ge5

63e0: a5 b8        unused_63e0     lda     score             ;only referenced by unref'd code above
63e2: 38                           sec
63e3: e5 ba                        sbc     enemy_score
63e5: 85 cf                        sta     close_firing_angle?
63e7: b0 04                        bcs     :DoPlay
63e9: 49 ff                        eor     #$ff
63eb: 69 01                        adc     #$01
                   ; 
63ed: c9 05        :DoPlay         cmp     #$05              ;A-reg $00 or (usually $80)
63ef: b0 0c                        bcs     :Ge5
63f1: 85 c4                        sta     move_counter      ;set counter to zero
63f3: a9 05                        lda     #$05
63f5: e5 c4                        sbc     move_counter      ;note carry is clear, so this yields 4
63f7: 0a                           asl     A
63f8: 0a                           asl     A
63f9: 0a                           asl     A
63fa: 0a                           asl     A                 ;$40
63fb: d0 02                        bne     :Long             ;(always)

63fd: a9 04        :Ge5            lda     #$04              ;move 4 steps then re-evaluate
63ff: 85 c4        :Long           sta     move_counter
                   ; I'm not sure what this code is trying to accomplish.  What it actually does is
                   ; cause $CF to alternate between $02 and $10.
6401: 24 cf                        bit     close_firing_angle? ;high bit set? (never see this)
6403: 30 0b                        bmi     :Use0a
6405: a9 0a                        lda     #$0a              ;value = $0a - value
6407: 38                           sec
6408: e5 cf                        sbc     close_firing_angle?
640a: b0 06                        bcs     :DoShift          ;if value was $02, A-reg holds $08
640c: a9 01                        lda     #$01              ;value was $10
640e: d0 02                        bne     :DoShift          ;(always)

6410: a9 0a        :Use0a          lda     #$0a
6412: 0a           :DoShift        asl     A                 ;multiply by 2 (so $01 -> $02, $08 -> $10)
6413: 85 cf                        sta     close_firing_angle?
6415: 68                           pla                       ;restore A-reg
6416: 60                           rts

6417: d5                           .dd1    $d5               ;(checksum adj)

                   ; 
                   ; Updates the position and movement strategy of the enemy unit.
                   ; 
                   ; On entry:
                   ;   $2c: angle to player
                   ; 
6418: a5 14        UpdateEnemyUnit lda     enemy_state       ;is enemy alive?
641a: f0 01                        beq     :Alive            ;yes, keep going
641c: 60                           rts

641d: 24 cb        :Alive          bit     missile_flag      ;is it a missile?
641f: 10 03                        bpl     UpdateTank        ;no, do tank update
6421: 4c 24 66                     jmp     UpdateMissile     ;yes, do missile update

                   ; 
                   ; Updates the position and movement strategy of an enemy tank.
                   ; 
                   ; The game puts a target angle in $bc, and moves toward it for a few frames.  In
                   ; the early game we pick angles away from the player and chase them for a few
                   ; seconds, but once the game gets going we head directly at the player.
                   ; 
                   ; Because we move toward an angle that isn't updated every frame, distant tanks
                   ; will exhibit a somewhat jerky forward-rotate-forward-rotate movement style.
                   ; 
6424: a2 02        UpdateTank      ldx     #$02              ;set X-reg=$02 for enemy unit
6426: 20 a9 63                     jsr     CopyPosToC0       ;save current position
6429: 18                           clc
642a: a5 be                        lda     radar_facing      ;spin the slow tank's radar
642c: 69 0b                        adc     #$0b
642e: 85 be                        sta     radar_facing
6430: a5 c5                        lda     enemy_rev_flags   ;check the in-reverse flag
6432: 4a                           lsr     A
6433: 90 27                        bcc     :NotRev
                   ; We're backing up.  While doing so we also rotate to the left or right.  When
                   ; done, we drive forward for a few seconds.  The player's position does not
                   ; factor into any of this movement.
                   ; 
                   ; Note no collision detection is performed, which allows tanks that spawn inside
                   ; obstacles to back out of them.  This would also allow tanks to back up onto
                   ; the player, but that's pretty hard to set up.
6435: 4a                           lsr     A                 ;shift once more to get turn direction
6436: 08                           php
6437: 20 6f 63                     jsr     MoveBackward      ;back up one step, regardless of tank type
643a: 28                           plp
643b: 90 06                        bcc     :BackRight
643d: 20 8d 63                     jsr     RotateLeft
6440: 4c 46 64                     jmp     :BackCommon

6443: 20 9b 63     :BackRight      jsr     RotateRight
6446: c6 a7        :BackCommon     dec     tread_drip_ctr    ;move treads backward
6448: a5 c4                        lda     move_counter      ;still going?
644a: f0 01                        beq     :RevDone          ;no, find something new to do
644c: 60                           rts

644d: a5 c5        :RevDone        lda     enemy_rev_flags
644f: 29 fc                        and     #%11111100        ;clear move-backward flag
6451: 85 c5                        sta     enemy_rev_flags
6453: a5 2c                        lda     enemy_facing      ;set desired direction to current direction
6455: 85 bc                        sta     enemy_turn_to
6457: a9 34                        lda     #$34              ;set move counter to 3.5 sec
6459: 85 c4                        sta     move_counter
645b: 60                           rts

645c: a5 c4        :NotRev         lda     move_counter      ;time to update heading?
645e: d0 03                        bne     :ContMove         ;not yet, branch
6460: 4c 34 65                     jmp     SetTankTurnTo

                   ; Rotate toward the facing in $bc, moving forward if we're pointed toward the
                   ; player and not too close.  If the facing is within a few degrees of the
                   ; player's position, fire the cannon.
6463: a5 2c        :ContMove       lda     enemy_facing      ;compute difference between current and desired facing
6465: 38                           sec
6466: e5 bc                        sbc     enemy_turn_to
6468: a8                           tay                       ;copy to Y-reg
6469: 10 05                        bpl     :IsPos            ;get absolute value
646b: 49 ff                        eor     #$ff
646d: 18                           clc
646e: 69 01                        adc     #$01
6470: c5 cf        :IsPos          cmp     close_firing_angle? ;are we close to the correct angle?
6472: 90 3e                        bcc     :SmallAngle       ;yes, turn and move
                   ; Angle is too large, rotate without moving forward.
6474: 98                           tya                       ;get signed angle delta from Y-reg
6475: 10 1e                        bpl     :TurnRightMulti   ;sign says to turn right
6477: 20 8d 63                     jsr     RotateLeft
647a: 20 95 65                     jsr     TryShootPlayer
647d: 20 8d 63                     jsr     RotateLeft
6480: 20 95 65                     jsr     TryShootPlayer
6483: 20 b5 69                     jsr     GetTankType
6486: b0 01                        bcs     :TurnLeftFast
6488: 60           :Return         rts

6489: 20 8d 63     :TurnLeftFast   jsr     RotateLeft        ;super tanks rotate at 2x
648c: 20 95 65                     jsr     TryShootPlayer
648f: 20 8d 63                     jsr     RotateLeft
6492: 4c 95 65                     jmp     TryShootPlayer

6495: 20 9b 63     :TurnRightMulti jsr     RotateRight
6498: 20 95 65                     jsr     TryShootPlayer
649b: 20 9b 63                     jsr     RotateRight
649e: 20 95 65                     jsr     TryShootPlayer
64a1: 20 b5 69                     jsr     GetTankType
64a4: 90 e2                        bcc     :Return
64a6: 20 9b 63                     jsr     RotateRight       ;super tanks rotate at 2x
64a9: 20 95 65                     jsr     TryShootPlayer
64ac: 20 9b 63                     jsr     RotateRight
64af: 4c 95 65                     jmp     TryShootPlayer

                   ; The angle is nearly correct.  Turn slowly (so we don't overshoot) and move
                   ; forward if we're not right in the player's face.
64b2: c9 00        :SmallAngle     cmp     #$00              ;are we lined up on the player?
64b4: f0 0c                        beq     :SmallAngleCom    ;yes, try to shoot
64b6: 98                           tya                       ;no, rotate one step then try to shoot
64b7: 10 06                        bpl     :GoRight
64b9: 20 8d 63                     jsr     RotateLeft
64bc: 4c c2 64                     jmp     :SmallAngleCom

                   ]abs_delta_z    .var    $08    {addr/2}
                   ]abs_delta_x    .var    $0a    {addr/2}

64bf: 20 9b 63     :GoRight        jsr     RotateRight
64c2: 20 95 65     :SmallAngleCom  jsr     TryShootPlayer    ;sets $08-0b if we're close
64c5: a5 08                        lda     ]abs_delta_z      ;(could this end up using stale data?)
64c7: 8d 64 18                     sta     MB_SET_R2L
64ca: a5 09                        lda     ]abs_delta_z+1
64cc: 8d 65 18                     sta     MB_SET_R2H
64cf: a5 0a                        lda     ]abs_delta_x
64d1: 8d 66 18                     sta     MB_SET_R3L
64d4: a5 0b                        lda     ]abs_delta_x+1
64d6: 8d 67 18                     sta     MB_SET_R3H
64d9: 8d 7e 18                     sta     MB_CALC_HYPOT     ;invoke function $1e to get distance
64dc: 20 b5 69                     jsr     GetTankType       ;while we're waiting, check the tank type
64df: ad 18 18                     lda     MB_RESULT_HI      ;get high byte of distance
64e2: 90 05                        bcc     :SlowTank
64e4: c9 08                        cmp     #$08              ;is distance >= $800
64e6: b0 06                        bcs     :GoForward        ;yes, move
64e8: 60                           rts

64e9: c9 05        :SlowTank       cmp     #$05              ;is distance >= $500?
64eb: b0 01                        bcs     :GoForward        ;yes, move
64ed: 60                           rts

                   ; Move toward the player.
64ee: a2 02        :GoForward      ldx     #$02              ;enemy unit = $02
64f0: 20 10 63                     jsr     CalcMoveDelta     ;compute movement distance
64f3: 20 b5 69                     jsr     GetTankType       ;get tank type
64f6: 90 08                        bcc     :MoveSlow         ;slow tank, move at base rate
64f8: 06 1d                        asl     ]move_x           ;super tank, double the movement rate
64fa: 26 1e                        rol     ]move_x+1
64fc: 06 19                        asl     ]move_z
64fe: 26 1a                        rol     ]move_z+1
6500: 20 54 63     :MoveSlow       jsr     MoveUnit          ;update the unit's position
6503: 20 d6 68                     jsr     CheckObstUnitColl ;did we drive into something?
6506: b0 12                        bcs     :HitSomething     ;yes, handle it
6508: e6 a7                        inc     tread_drip_ctr    ;move treads forward
650a: a5 2c                        lda     enemy_facing      ;are we moving without turning?
650c: c5 bc                        cmp     enemy_turn_to
650e: f0 01                        beq     :MoveAgain        ;yes, both treads fwd, move again
6510: 60                           rts

6511: 20 54 63     :MoveAgain      jsr     MoveUnit          ;update unit position
6514: 20 d6 68                     jsr     CheckObstUnitColl ;did we drive into something?
6517: b0 01                        bcs     :HitSomething     ;yes, handle it
6519: 60                           rts

651a: 24 ce        :HitSomething   bit     play_flag         ;are we playing now?
651c: 10 0b                        bpl     :NotPlaying       ;no, branch with A-reg=0 (don't reverse)
651e: 09 00                        ora     #$00              ;set flags for A-reg ($00=obstacle, $ff=player)
6520: 30 0f                        bmi     :HitPlayer
6522: ad 2a 18                     lda     POKEY_RANDOM      ;get random value
6525: 29 02                        and     #$02              ;0 or 2 (determines direction turned while reversing)
6527: 09 01                        ora     #$01              ;1 or 3 ($01=reversing)
                   ; 
6529: 05 c5        :NotPlaying     ora     enemy_rev_flags
652b: 85 c5                        sta     enemy_rev_flags   ;set flags
652d: a9 30                        lda     #$30              ;repeat for 48 frames (3 sec)
652f: 85 c4                        sta     move_counter
                   ; Tanks don't back up when they hit a player.  The tactic that involves running
                   ; at a tank and shooting it while it turns to face doesn't work because of the
                   ; collision, it works because the player is suddenly at an angle that causes the
                   ; enemy tank to switch from "move forward" to "rotate".
6531: 4c ba 63     :HitPlayer      jmp     CopyPosFromC0     ;restore original position

                   ; 
                   ; The move counter expired.  Pick a new direction to move toward.  If we're
                   ; being nice we'll drive a bit randomly, but once the game gets going we just
                   ; use the angle to the player.
                   ; 
6534: 24 ce        SetTankTurnTo   bit     play_flag         ;are we playing the game?
6536: 10 39                        bpl     :GoMild           ;no, branch
6538: a5 d1                        lda     rez_protect       ;has summoning sickness worn off?
653a: c9 ff                        cmp     #$ff
653c: f0 4b                        beq     :GoHard?          ;yes, don't be nice
653e: ad 2a 18                     lda     POKEY_RANDOM      ;get a random number
6541: 4a                           lsr     A                 ;shift low bit into carry flag
6542: 90 45                        bcc     :GoHard?          ;50/50 chance
6544: a5 b9                        lda     score+1           ;score >= 100K?
6546: d0 41                        bne     :GoHard?          ;yes, branch
6548: a5 b8                        lda     score             ;compare player score to enemy "score"
654a: 38                           sec                       ; to see if the enemy is thrashing the player
654b: e5 ba                        sbc     enemy_score
654d: f0 04                        beq     :GoMedium
654f: 90 20                        bcc     :GoMild
6551: b0 36                        bcs     :GoHard?

                   ; After a recent rez the player is winning, but just barely.  Pick an off-target
                   ; angle.
6553: a5 3a        :GoMedium       lda     nmi_count         ;get the NMI count (*not* the game frame count)
6555: 29 07                        and     #$07              ;0-7
6557: d0 08                        bne     :SevenOf8         ;7 out of 8 times, branch
6559: a9 01                        lda     #$01              ;set the move-backward flag
655b: 05 c5                        ora     enemy_rev_flags
655d: 85 c5                        sta     enemy_rev_flags
655f: d0 0b                        bne     :SetCounter       ;(always)

6561: a9 00        :SevenOf8       lda     #$00
6563: 85 c5                        sta     enemy_rev_flags   ;clear move-backward flag
6565: 20 10 68                     jsr     CalcAngleToPlayer ;get the angle to the player
6568: 49 40                        eor     #$40              ;pick a direction 90 degrees off
656a: 85 bc                        sta     enemy_turn_to     ;head that way
656c: a9 40        :SetCounter     lda     #$40              ;move this way for ~4 sec
656e: 85 c4                        sta     move_counter
6570: 60                           rts

                   ; The enemy is out-scoring the player (e.g. the very first tank killed the
                   ; player), so we're going to be very kind.
6571: ad 2a 18     :GoMild         lda     POKEY_RANDOM      ;get random value for angle offset
6574: 29 1f                        and     #$1f              ;reduce to 45 degrees
6576: 24 3a                        bit     nmi_count         ;50/50 chance, essentially
6578: 30 04                        bmi     :IsNeg
657a: e5 bc                        sbc     enemy_turn_to     ;offset previous turn-to
657c: d0 02                        bne     :TurnComm
657e: 65 bc        :IsNeg          adc     enemy_turn_to
6580: 85 bc        :TurnComm       sta     enemy_turn_to     ;set new direction
6582: a9 00                        lda     #$00
6584: 85 c5                        sta     enemy_rev_flags   ;clear reverse flag
6586: 4c cb 63                     jmp     UpdateMoveCtrAndAng

                   ; Head directly at player.
6589: 20 10 68     :GoHard?        jsr     CalcAngleToPlayer ;get angle to player
658c: 85 bc                        sta     enemy_turn_to     ;use that
658e: a9 80                        lda     #$80
6590: 85 c5                        sta     enemy_rev_flags   ;clear reverse flag
6592: 4c cb 63                     jmp     UpdateMoveCtrAndAng

                   ; 
                   ; Attempts to shoot the player.  Does not fire if the the player or enemy is
                   ; newly-spawned, if the angle is too large, or the relative scores indicate that
                   ; we should be mellow.
                   ; 
                   ; On exit if we thought about taking a shot:
                   ;   $08/09: abs(deltaZ)
                   ;   $0a/0b: abs(deltaX)
                   ; 
                   ]tmp_08         .var    $08    {addr/1}

6595: a5 d1        TryShootPlayer  lda     rez_protect       ;check frames since player or enemy spawned
6597: c9 20                        cmp     #$20              ;> 2 seconds?
6599: b0 01                        bcs     :NotNewRez        ;yes, continue
659b: 60           :Return         rts                       ;no, don't be unfair

659c: c9 ff        :NotNewRez      cmp     #$ff              ;has it been >= 255 frames (17 seconds)?
659e: f0 16                        beq     :ShootOkay        ;yes, branch
65a0: a5 b9                        lda     score+1           ;have they scored >= 100K points?
65a2: d0 12                        bne     :ShootOkay        ;yes, branch
65a4: a5 b8                        lda     score
65a6: 4a                           lsr     A                 ;have they scored >= 2000 points?
65a7: d0 0d                        bne     :ShootOkay        ;yes, branch
65a9: a5 d0                        lda     enemy_ang_delt_abs ;check angle to player
65ab: c9 20                        cmp     #$20              ;within 45 degrees?
65ad: b0 ec                        bcs     :Return           ;no, don't shoot
65af: ad e8 02                     lda     enemy_dist_hi     ;are they somewhat close?
65b2: c9 24                        cmp     #$24
65b4: b0 6d                        bcs     :Return           ;no, don't shoot
65b6: 20 10 68     :ShootOkay      jsr     CalcAngleToPlayer ;get angle to player
65b9: a2 02                        ldx     #$02              ;?
65bb: 38                           sec                       ;compute diff of enemy facing and angle to player
65bc: e5 2c                        sbc     enemy_facing
65be: 10 05                        bpl     :IsPos
65c0: 18                           clc                       ;negative value, invert
65c1: 49 ff                        eor     #$ff
65c3: 69 02                        adc     #$02              ;add 2 instead of 1
65c5: c9 02        :IsPos          cmp     #$02              ;off by >= 2 angle units?
65c7: b0 5a                        bcs     :Return           ;yes, don't fire
65c9: a5 26                        lda     projectile_state_1 ;are we ready to fire?
65cb: d0 56                        bne     :Return           ;no, our projectile is active
65cd: a5 12                        lda     unit_state        ;player alive?
65cf: d0 52                        bne     :Return           ;no, bail
                   ; Shoot at player.
65d1: a9 7f                        lda     #$7f              ;init time-to-live
65d3: 85 26                        sta     projectile_state_1
65d5: a9 05                        lda     #$05              ;set sound counter
65d7: 85 bd                        sta     dsnd_cnon_ctr
65d9: a5 15                        lda     dsnd_ctrl_val
65db: 29 f7                        and     #%11110111        ;d-sound: cannon=soft
65dd: 85 15                        sta     dsnd_ctrl_val
                   ; Configure projectile velocity.  We use the sin/cos value ($0000-ffff) right-
                   ; shifted 7x with sign-extension (+/-255).  Projectiles move 4x per frame.
65df: a5 2c                        lda     enemy_facing      ;get enemy tank's facing
65e1: 20 4b 5e                     jsr     CalcCosine        ;compute cosine
65e4: 85 08                        sta     ]tmp_08           ;save low byte
65e6: 8a                           txa
65e7: 85 b2                        sta     enemy_proj_vel_z  ;save high byte in low byte
65e9: 30 04                        bmi     :CosNeg           ;sign-extend to 16 bits
65eb: a9 00                        lda     #$00
65ed: f0 02                        beq     :CosCommon

65ef: a9 ff        :CosNeg         lda     #$ff
65f1: 06 08        :CosCommon      asl     ]tmp_08           ;multiply by 2 to get >> 7 overall
65f3: 26 b2                        rol     enemy_proj_vel_z
65f5: 2a                           rol     A
65f6: 85 b3                        sta     enemy_proj_vel_z+1
                   ; Repeat for sin(angle).
65f8: a5 2c                        lda     enemy_facing
65fa: 20 4e 5e                     jsr     CalcSine
65fd: 85 08                        sta     ]tmp_08
65ff: 8a                           txa
6600: 85 b6                        sta     enemy_proj_vel_x
6602: 30 04                        bmi     :SinNeg
6604: a9 00                        lda     #$00
6606: f0 02                        beq     :SinCommon

6608: a9 ff        :SinNeg         lda     #$ff
660a: 06 08        :SinCommon      asl     ]tmp_08
660c: 26 b6                        rol     enemy_proj_vel_x
660e: 2a                           rol     A
660f: 85 b7                        sta     enemy_proj_vel_x+1
                   ; Copy enemy unit position to projectile.
6611: a5 2f                        lda     enemy_pos_z
6613: 85 aa                        sta     proj_pos_z+2
6615: a5 30                        lda     enemy_pos_z+1
6617: 85 ab                        sta     proj_pos_z+3
6619: a5 33                        lda     enemy_pos_x
661b: 85 ae                        sta     proj_pos_x+2
661d: a5 34                        lda     enemy_pos_x+1
661f: 85 af                        sta     proj_pos_x+3
6621: a2 02                        ldx     #$02              ;restore X-reg ($02=enemy)
6623: 60           :Return         rts

                   ; 
                   ; Updates the position and movement pattern of a missile.
                   ; 
                   ]tmp_0a         .var    $0a    {addr/1}

6624: e6 a7        UpdateMissile   inc     tread_drip_ctr    ;increment missile drip counter
6626: a9 00                        lda     #$00              ;is the altitude < $200?
6628: cd e4 02                     cmp     ypos_by_type+12
662b: a9 02                        lda     #$02
662d: ed e5 02                     sbc     ypos_by_type+13   ;carry is set if near ground
6630: a9 00                        lda     #$00
6632: 2a                           rol     A
6633: 85 a6                        sta     temp_a6           ;set to $01 for low altitude
6635: 25 ca                        and     missile_hop_flag  ;clear it if we're hopping
6637: f0 03                        beq     :NoHop            ;not hopping, do regular stuff
6639: 4c 45 67                     jmp     :FlyUp            ;low altitude, hopping, fly up

663c: a5 2a        :NoHop          lda     plyr_facing_hi    ;compute the difference between the direction the
663e: 38                           sec                       ; player is facing and the direction we're currently
663f: e5 bc                        sbc     enemy_turn_to     ; heading (should be close to $80)
6641: 85 0a                        sta     ]tmp_0a           ;save it
6643: 10 05                        bpl     :IsPos
6645: 18                           clc                       ;negate to get absolute value
6646: 49 ff                        eor     #$ff
6648: 69 01                        adc     #$01
664a: c9 40        :IsPos          cmp     #$40              ;>= 90 degrees?
664c: b0 08                        bcs     :BigAngle         ;yes, branch
664e: a2 02                        ldx     #$02
6650: 24 0a                        bit     ]tmp_0a           ;check sign
6652: 10 21                        bpl     :TurnRightTwo     ;turn right or left
6654: 30 14                        bmi     :TurnLeftTwo      ;(always)

6656: 20 10 68     :BigAngle       jsr     CalcAngleToPlayer ;get angle from missile to player
6659: a2 02                        ldx     #$02
665b: 85 0a                        sta     ]tmp_0a
665d: a5 bc                        lda     enemy_turn_to     ;get direction we're currently heading
665f: 38                           sec
6660: e5 0a                        sbc     ]tmp_0a           ;subtract angle
6662: f0 15                        beq     :TurnCommon       ;headed straight at player, branch
6664: 10 0b                        bpl     :BigPos
6666: c9 fd                        cmp     #$fd              ;turn left once or twice depending on angle delta
6668: 90 02                        bcc     :TurnLeftOne
666a: e6 bc        :TurnLeftTwo    inc     enemy_turn_to
666c: e6 bc        :TurnLeftOne    inc     enemy_turn_to
666e: 4c 79 66                     jmp     :TurnCommon

6671: c9 03        :BigPos         cmp     #$03              ;turn right once or twice depending on angle delta
6673: 90 02                        bcc     TurnRightOne
6675: c6 bc        :TurnRightTwo   dec     enemy_turn_to
6677: c6 bc        TurnRightOne    dec     enemy_turn_to
6679: ad ec 02     :TurnCommon     lda     missile_count     ;first missile ever (or 256th missile)?
667c: f0 24                        beq     :FinalTurn        ;yes, be nice (fly straight in)
                   ; Check the score to see if missiles should be nastier, which happens at 25K
                   ; points beyond the initial missile threshold.  In this context, "nasty"
                   ; determines how far the missile is from the player when it stops swerving.
667e: a5 b9                        lda     score+1           ;over 100K points?
6680: d0 19                        bne     :Harsh
6682: ad 00 0a                     lda     DSW0              ;get DIP switch setting
6685: 4a                           lsr     A
6686: 4a                           lsr     A
6687: 29 03                        and     #%00000011        ;get the "missiles first appear at" value
6689: aa                           tax                       ; (5 / 10 / 20 / 30)
668a: bd 8a 38                     lda     missile_score,x   ;look up score (BCD)
668d: 18                           clc
668e: f8                           sed
668f: 69 25                        adc     #$25              ;add BCD 25 (30 / 35 / 45 / 55)
6691: d8                           cld
6692: 38                           sec
6693: e5 b8                        sbc     score             ;subtract score (BUG? not in decimal mode)
6695: 30 04                        bmi     :Harsh            ;score is higher than second threshold, branch
6697: c9 08                        cmp     #$08              ;is it getting close?
6699: b0 02                        bcs     :SubHarsh         ;no, use score diff as distance threshold
669b: a9 08        :Harsh          lda     #$08              ;use distance threshold of $0800 (very close)
669d: cd e8 02     :SubHarsh       cmp     enemy_dist_hi     ;are we within threshold?
66a0: 90 05                        bcc     :NotPointBlank    ;no, still far away
66a2: a5 bc        :FinalTurn      lda     enemy_turn_to     ;head at player
66a4: 4c be 66                     jmp     :MoveMissile

                   ; Make the missile zig-zag.
                   ; 
                   ; The big swerves aren't done by changing the missile's desired direction -- the
                   ; small rotations performed earlier update the desired facing to match changes
                   ; in the position of the player and the missile.  The big swerves are done by
                   ; setting the missile's facing to an angle offset from the desired angle.
                   ; 
                   ; The amount of swerve is determined by the low 5 bits of the game frame
                   ; counter, which cycle in the span of 2 seconds.
66a7: a5 c6        :NotPointBlank  lda     frame_counter     ;get game frame number
66a9: 4a                           lsr     A                 ;shift bit 3 ($08) into the carry flag
66aa: 4a                           lsr     A                 ;will be set for 0.5 sec, clear for 0.5 sec
66ab: 4a                           lsr     A                 ;this determines whether we swerve left or right
66ac: 4a                           lsr     A
66ad: a5 c6                        lda     frame_counter     ;get counter again
66af: 29 1f                        and     #%00011111        ;keep the low 5 bits (0-31)
66b1: 85 08                        sta     ]tmp_08
66b3: a5 bc                        lda     enemy_turn_to     ;get desired facing
66b5: b0 05                        bcs     :DoSub
66b7: 65 08                        adc     ]tmp_08           ;add or subtract the bits
66b9: 4c be 66                     jmp     :MoveMissile

66bc: e5 08        :DoSub          sbc     ]tmp_08
66be: 85 2c        :MoveMissile    sta     enemy_facing      ;set the facing direction
                   ; Move the missile forward in the direction it's currently facing.  We take the
                   ; sin/cos values ($0000-ffff) and right-shift them 6x with sign extension (+/-
                   ; 512), which is easiest to do by shifting a byte right and then 2 bits left.
66c0: 20 4e 5e                     jsr     CalcSine          ;compute sin(angle)
66c3: 86 1d                        stx     ]move_x           ;save the high byte
66c5: 48                           pha                       ;spill the low byte
66c6: a9 00                        lda     #$00              ;sign-extend
66c8: 24 1d                        bit     ]move_x
66ca: 10 02                        bpl     :SinPos
66cc: a9 ff                        lda     #$ff
66ce: 85 1e        :SinPos         sta     ]move_x+1
66d0: 68                           pla                       ;restore low byte
66d1: 0a                           asl     A                 ;shift left
66d2: 26 1d                        rol     ]move_x           ;roll the bit in
66d4: 26 1e                        rol     ]move_x+1
66d6: 0a                           asl     A                 ;do it twice
66d7: 26 1d                        rol     ]move_x
66d9: 26 1e                        rol     ]move_x+1
                   ; Repeat for cos(angle).
66db: a5 2c                        lda     enemy_facing
66dd: 20 4b 5e                     jsr     CalcCosine        ;compute cos(angle)
66e0: 86 19                        stx     ]move_z           ;save the high byte
66e2: 48                           pha                       ;spill the low byte
66e3: a9 00                        lda     #$00              ;sign-extend
66e5: 24 19                        bit     ]move_z
66e7: 10 02                        bpl     :CosPos
66e9: a9 ff                        lda     #$ff
66eb: 85 1a        :CosPos         sta     ]move_z+1
66ed: 68                           pla
66ee: 0a                           asl     A
66ef: 26 19                        rol     ]move_z
66f1: 26 1a                        rol     ]move_z+1
66f3: 0a                           asl     A
66f4: 26 19                        rol     ]move_z
66f6: 26 1a                        rol     ]move_z+1
                   ; Copy the current position to $c0-c3 (same as CopyPosToC0).
66f8: a5 2f                        lda     enemy_pos_z
66fa: 85 c0                        sta     saved_obj_pos
66fc: a5 30                        lda     enemy_pos_z+1
66fe: 85 c1                        sta     saved_obj_pos+1
6700: a5 33                        lda     enemy_pos_x
6702: 85 c2                        sta     saved_obj_pos+2
6704: a5 34                        lda     enemy_pos_x+1
6706: 85 c3                        sta     saved_obj_pos+3
6708: a2 02                        ldx     #$02              ;X-reg=$02 for enemy unit
670a: 20 54 63                     jsr     MoveUnit          ;uses $19/1a $1d/1e computed above
670d: 20 4e 5f                     jsr     TestProj0Coll     ;did the player shoot us?
6710: b0 1b                        bcs     :Return           ;yes, blow up soon
6712: a2 02                        ldx     #$02
6714: 20 d6 68                     jsr     CheckObstUnitColl ;see if we collided with an obstacle or player
6717: 90 05                        bcc     :NoColl           ;nope
6719: a6 a6                        ldx     temp_a6           ;at low altitude?
671b: d0 11                        bne     :HandleColl       ;yes, we didn't fly over, handle it
671d: 60                           rts                       ;no, we flew over it

671e: a9 00        :NoColl         lda     #$00
6720: 85 ca                        sta     missile_hop_flag  ;no longer colliding, clear the "hop" flag
6722: ad e4 02                     lda     ypos_by_type+12   ;check the current altitude
6725: 0d e5 02                     ora     ypos_by_type+13   ; (OR low + high bytes)
6728: f0 03                        beq     :Return           ;we're at ground level, bail
672a: ce e5 02                     dec     ypos_by_type+13   ;decrement high byte
672d: 60           :Return         rts

672e: 09 00        :HandleColl     ora     #$00              ;set flags for A-reg ($00=obstacle, $ff=player)
6730: 30 17                        bmi     :PlayerDead
                   ; Restore position from $c0-c3 (same as CopyPosFromC0).
6732: a5 c0                        lda     saved_obj_pos
6734: 85 2f                        sta     enemy_pos_z
6736: a5 c1                        lda     saved_obj_pos+1
6738: 85 30                        sta     enemy_pos_z+1
673a: a5 c2                        lda     saved_obj_pos+2
673c: 85 33                        sta     enemy_pos_x
673e: a5 c3                        lda     saved_obj_pos+3
6740: 85 34                        sta     enemy_pos_x+1
6742: e6 ca                        inc     missile_hop_flag  ;set "hop" flag to 1
6744: 60                           rts

6745: ee e5 02     :FlyUp          inc     ypos_by_type+13   ;inc high byte of #6 (missile=$16)
6748: 60                           rts

                   ; Missile and player collided.
6749: a9 20        :PlayerDead     lda     #$20              ;player and missile both die
674b: 85 12                        sta     unit_state
674d: 85 14                        sta     enemy_state
674f: a9 02                        lda     #$02              ;start the death animation
6751: 85 c7                        sta     death_crack_index
6753: a9 ff                        lda     #$ff              ;shake the camera
6755: 85 df                        sta     horizon_adj
6757: a9 01                        lda     #$01              ;score 1 for the bad guy
6759: 18                           clc
675a: f8                           sed
675b: 65 ba                        adc     enemy_score
675d: d8                           cld
675e: 85 ba                        sta     enemy_score
6760: a2 00                        ldx     #$00              ;stop missile sound
6762: 8e 25 18                     stx     POKEY_AUDC3
6765: 8e 27 18                     stx     POKEY_AUDC4
6768: 20 99 61                     jsr     InitUnitChunks    ;create exploded missile chunks
676b: a9 ff                        lda     #$ff
676d: 85 0f                        sta     dsnd_expl_ctr     ;explosion length ~1 sec
676f: a5 15                        lda     dsnd_ctrl_val
6771: 29 fd                        and     #%11111101        ;d-sound: explosion=soft
6773: 85 15                        sta     dsnd_ctrl_val
6775: c6 cc                        dec     player_lives      ;decrement lives
6777: d0 02                        bne     :StillAlive       ;branch if more lives to live
6779: e6 cd                        inc     game_over_flags   ;set flag to 1
677b: 60           :StillAlive     rts

                   ; 
                   ; Updates the saucer position and velocity.
                   ; 
677c: a5 d4        UpdateSaucer    lda     saucer_dead_intens ;saucer dying?
677e: f0 23                        beq     :NotDying         ;no, branch
6780: c9 30                        cmp     #$30              ;< $30?
6782: 90 05                        bcc     :BrtComm          ;yes, use it (dim)
6784: a9 60                        lda     #$60              ;no, use ($60 - counter) instead (brighten)
6786: 38                           sec
6787: e5 d4                        sbc     saucer_dead_intens
6789: 18           :BrtComm        clc
678a: 69 08                        adc     #$08              ;add 8
678c: 0a                           asl     A                 ;shift left
678d: 0a                           asl     A
678e: 29 f0                        and     #%11110000        ;mask off intensity bits
6790: 8d ea 02                     sta     sauc_log_inten    ;save it where the drawing code can find it
6793: c6 d4                        dec     saucer_dead_intens ;decrement counter
6795: c6 d4                        dec     saucer_dead_intens
6797: d0 09                        bne     :Return           ;still fading out, bail
6799: ad 2a 18                     lda     POKEY_RANDOM      ;random delay until new saucer appears
679c: 85 d3                        sta     saucer_ttl        ;(approx 0-17 sec)
679e: a9 00                        lda     #$00              ;set to inactive
67a0: 85 de                        sta     saucer_state
67a2: 60           :Return         rts

67a3: a5 de        :NotDying       lda     saucer_state      ;saucer active?
67a5: f0 51                        beq     :Inactive         ;no, see if we want to make one
67a7: a5 d9                        lda     saucer_facing     ;update facing so saucer spins
67a9: 18                           clc
67aa: 69 08                        adc     #$08
67ac: 85 d9                        sta     saucer_facing
67ae: a5 d3                        lda     saucer_ttl        ;time to change direction?
67b0: f0 1d                        beq     :Randomize        ;yes, branch
67b2: 18                           clc                       ;add X/Z velocity to current position
67b3: a5 da                        lda     saucer_vel_z
67b5: 65 d5                        adc     saucer_z
67b7: 85 d5                        sta     saucer_z
67b9: a5 d6                        lda     saucer_z+1
67bb: 65 db                        adc     saucer_vel_z+1
67bd: 85 d6                        sta     saucer_z+1
67bf: a5 d7                        lda     saucer_x
67c1: 18                           clc
67c2: 65 dc                        adc     saucer_vel_x
67c4: 85 d7                        sta     saucer_x
67c6: a5 d8                        lda     saucer_x+1
67c8: 65 dd                        adc     saucer_vel_x+1
67ca: 85 d8                        sta     saucer_x+1
67cc: c6 d3        :DecAndReturn   dec     saucer_ttl        ;decrement time-to-live (or time-until-vel-change)
67ce: 60                           rts

67cf: ad 2a 18     :Randomize      lda     POKEY_RANDOM      ;set low byte of Z velocity to a random value
67d2: 85 da                        sta     saucer_vel_z
67d4: 09 00                        ora     #$00              ;?
67d6: 30 04                        bmi     :IsNeg            ;sign-extend into high byte
67d8: a9 00                        lda     #$00
67da: f0 02                        beq     :IsPos            ;(always)

67dc: a9 ff        :IsNeg          lda     #$ff
67de: 85 db        :IsPos          sta     saucer_vel_z+1
67e0: ad 2a 18                     lda     POKEY_RANDOM      ;set low byte of X velocity to random value
67e3: 85 dc                        sta     saucer_vel_x
67e5: 09 00                        ora     #$00              ;?
67e7: 30 04                        bmi     :IsNeg            ;sign-extend into high byte
67e9: a9 00                        lda     #$00
67eb: f0 02                        beq     :IsPos            ;(always)

67ed: a9 ff        :IsNeg          lda     #$ff
67ef: 85 dd        :IsPos          sta     saucer_vel_x+1
67f1: ad 2a 18                     lda     POKEY_RANDOM      ;random time until direction change
67f4: 4a                           lsr     A                 ;0-127
67f5: 85 d3                        sta     saucer_ttl
67f7: 60                           rts

67f8: a5 b8        :Inactive       lda     score             ;get current score
67fa: 4a                           lsr     A                 ;divide by 2
67fb: d0 01                        bne     :ScoreGt1         ;if score >= 2000, see if we want to add a saucer
67fd: 60                           rts

67fe: a5 d3        :ScoreGt1       lda     saucer_ttl        ;is it time?
6800: d0 ca                        bne     :DecAndReturn     ;not yet, branch
6802: ad 2a 18                     lda     POKEY_RANDOM      ;put saucer in random location
6805: 85 d6                        sta     saucer_z+1
6807: ad 2a 18                     lda     POKEY_RANDOM
680a: 85 d8                        sta     saucer_x+1
680c: e6 de                        inc     saucer_state      ;set to 1
680e: d0 bf                        bne     :Randomize        ;(always)

                   ; 
                   ; Computes the angle from the enemy unit (tank/missile) to the player.  Sets
                   ; deltaZ/deltaX values as a side-effect.
                   ; 
                   ; (i.e. a tank must rotate to this angle to fire at the player.)
                   ; 
                   ; On exit:
                   ;   A-reg: angle (0-255)
                   ;   $08/09: abs(deltaZ)
                   ;   $0a/0b: abs(deltaX)
                   ; 
                   • Clear variables
                   ]temp           .var    $08    {addr/2}
                   ]zdiff          .var    $0a    {addr/2}
                   ]result_lo      .var    $0c    {addr/1}

                   CalcAngleToPlayer
6810: a0 00                        ldy     #$00              ;initialize sign tracker
                   ; Compute abs(player_x - enemy_x).
6812: 38                           sec
6813: a5 2d                        lda     unit_pos_z
6815: e5 2f                        sbc     enemy_pos_z
6817: 85 08                        sta     ]temp
6819: a5 2e                        lda     unit_pos_z+1
681b: e5 30                        sbc     enemy_pos_z+1
681d: 85 09                        sta     ]temp+1
681f: 10 04                        bpl     :IsPos            ;already positive, branch
6821: 20 49 61                     jsr     CalcAbs08         ;value negative, negate it
6824: c8                           iny                       ;remember it was negative
6825: a5 08        :IsPos          lda     ]temp             ;save zdiff
6827: 85 0a                        sta     ]zdiff
6829: a5 09                        lda     ]temp+1
682b: 85 0b                        sta     ]zdiff+1
682d: a5 09                        lda     ]temp+1           ;?
                   ; Compute abs(player_z - enemy_z).
                   ]xdiff          .var    $08    {addr/2}

682f: 38                           sec
6830: a5 31                        lda     unit_pos_x
6832: e5 33                        sbc     enemy_pos_x
6834: 85 08                        sta     ]xdiff
6836: a5 32                        lda     unit_pos_x+1
6838: e5 34                        sbc     enemy_pos_x+1
683a: 85 09                        sta     ]xdiff+1
683c: 10 0b                        bpl     :IsPos2           ;positive, branch
683e: 20 49 61                     jsr     CalcAbs08         ;negative, negate it
6841: 98                           tya                       ;was zdiff negative?
6842: f0 03                        beq     :ZPosXNeg         ;no, branch
6844: c8                           iny                       ;both negative, Y-reg=2
6845: d0 02                        bne     :IsPos2           ;(always)

6847: a0 03        :ZPosXNeg       ldy     #$03              ;z+, x-, Y-reg=3
                   ; We have the absolute value of zdiff and xdiff, with the sign bits encoded in
                   ; the Y-reg.  Check to see if we're at a 45-degree angle, which would yield
                   ; X/Z=1 (which doesn't fit in the low 8 bits of an 8.8 fixed-point value).  Note
                   ; we're effectively using a 2D coordinate system where +Z is right and +X is up,
                   ; so Z is the "adjacent" side.
                   ; 
                   ; We don't have to worry about division by zero unless two objects have the same
                   ; position.
6849: a5 08        :IsPos2         lda     ]xdiff
684b: c5 0a                        cmp     ]zdiff            ;xdiff low == zdiff low?
684d: d0 0d                        bne     :Cont             ;no, branch (with carry set appropriately)
684f: a5 09                        lda     ]xdiff+1          ;check the high byte
6851: c5 0b                        cmp     ]zdiff+1
6853: f0 03                        beq     :FortyFive        ;equal, skip calc
6855: 38                           sec                       ;make sure carry flag is set
6856: b0 04                        bcs     :Cont             ;go compare high bytes

6858: a9 20        :FortyFive      lda     #$20              ;45-degree angle
685a: d0 54                        bne     :DiffComm         ;(always)

685c: a5 09        :Cont           lda     ]xdiff+1          ;continue (or repeat) comparison into high byte
685e: e5 0b                        sbc     ]zdiff+1
6860: 08                           php                       ;save result
6861: 90 1a                        bcc     :XdiffSmaller     ;xdiff < zdiff, branch
                   ; 
6863: a5 08                        lda     ]xdiff            ;R7 = xdiff
6865: 8d 75 18                     sta     MB_SET_R7L
6868: a5 09                        lda     ]xdiff+1
686a: 8d 76 18                     sta     MB_SET_R7H
686d: a5 0a                        lda     ]zdiff
686f: 8d 6f 18                     sta     MB_SET_RBL        ;RB = zdiff
6872: a5 0b                        lda     ]zdiff+1
6874: 8d 70 18                     sta     MB_SET_RBH
6877: 8d 74 18                     sta     MB_DIVIDE_B7      ;compute zdiff/xdiff (adjacent / opposite)
687a: 4c 94 68                     jmp     :WaitResult

687d: a5 0a        :XdiffSmaller   lda     ]zdiff            ;R7 = zdiff
687f: 8d 75 18                     sta     MB_SET_R7L
6882: a5 0b                        lda     ]zdiff+1
6884: 8d 76 18                     sta     MB_SET_R7H
6887: a5 08                        lda     ]xdiff            ;RB = xdiff
6889: 8d 6f 18                     sta     MB_SET_RBL
688c: a5 09                        lda     ]xdiff+1
688e: 8d 70 18                     sta     MB_SET_RBH
6891: 8d 74 18                     sta     MB_DIVIDE_B7      ;compute xdiff/zdiff (opposite / adjacent)
                   ; The result is an 8.8 fixed-point value, which we know is in the range (1,0],
                   ; so we just want the fraction.  The division routine uses R6=$0a, which causes
                   ; it to do 10 iterations instead of 16, effectively right-shifting the 16-bit
                   ; result 6x.  If we do two more right shifts we will have the fraction as an 8-
                   ; bit value.
6894: 20 80 5b     :WaitResult     jsr     MbWaitForResult
6897: 85 0c                        sta     ]result_lo
6899: ad 18 18                     lda     MB_RESULT_HI      ;divide result by 4
689c: 4a                           lsr     A
689d: 66 0c                        ror     ]result_lo
689f: 4a                           lsr     A
68a0: 66 0c                        ror     ]result_lo
68a2: a6 0c                        ldx     ]result_lo
                   ; Look up the angle in the table.  This is limited to one octant; we use the
                   ; signs to determine the quadrant and whether we did X/Z or Z/X to determine the
                   ; octant.
68a4: bd 85 37                     lda     arctan_table,x    ;get octant angle $00-20
68a7: 28                           plp                       ;did we do X/Z or Z/X?
68a8: 90 06                        bcc     :DiffComm         ;did the standard way, branch
68aa: 85 0c                        sta     ]result_lo        ;did opposite way, flip to other octant in 1st quad
68ac: a9 40                        lda     #$40              ;64 - result
68ae: e5 0c                        sbc     ]result_lo
                   ; Take the collected sign bits and use them to choose the correct quadrant.
68b0: aa           :DiffComm       tax                       ;result in X-reg
68b1: 98                           tya                       ;sign flags in A-reg
68b2: 0a                           asl     A                 ;double it to use as table index
68b3: a8                           tay                       ;back to Y-reg
68b4: b9 bf 68                     lda     sign_fix_tab+1,y  ;push sign-fix function addr
68b7: 48                           pha
68b8: b9 be 68                     lda     sign_fix_tab,y
68bb: 48                           pha
68bc: 8a                           txa                       ;result in A-reg
68bd: 60                           rts                       ;jump to sign-fix function

68be: d4 68        sign_fix_tab    .dd2    :Return-1         ;0 (zdiff and xdiff positive)
68c0: c5 68                        .dd2    :NegateFlip-1     ;1 (zdiff negative)
68c2: ca 68                        .dd2    :Flip-1           ;2 (xdiff and zdiff negative)
68c4: cf 68                        .dd2    :Negate-1         ;3 (xdiff negative)

68c6: 18           :NegateFlip     clc                       ;negate + flip
68c7: 49 ff                        eor     #$ff
68c9: 69 01                        adc     #$01
68cb: 49 80        :Flip           eor     #$80              ;flip
68cd: 4c d5 68                     jmp     :Return           ;(why not RTS?)

68d0: 18           :Negate         clc                       ;negate (invert and add one)
68d1: 49 ff                        eor     #$ff
68d3: 69 01                        adc     #$01
68d5: 60           :Return         rts

                   ; 
                   ; Checks for collision between unit (player or enemy) and obstacles, then checks
                   ; for a collision with the opposing unit.
                   ; 
                   ; On entry:
                   ;   X-reg: $00 for player, $02 for enemy unit
                   ; 
                   ; On exit:
                   ;   Carry flag set if collision found
                   ;   A-reg: $00 for obstacle, $ff for opposing unit
                   ;   X-reg preserved
                   ; 
                   • Clear variables
                   ]saved_xreg     .var    $08    {addr/1}
                   ]tmp_0b         .var    $0b    {addr/1}
                   ]distance       .var    $0c    {addr/2}

                   CheckObstUnitColl
68d6: 18                           clc
68d7: 86 08                        stx     ]saved_xreg
68d9: b5 2d                        lda     unit_pos_z,x      ;copy player or enemy position into R0/R1
68db: 8d 60 18                     sta     MB_SET_R0L
68de: b5 2e                        lda     unit_pos_z+1,x
68e0: 8d 61 18                     sta     MB_SET_R0H
68e3: b5 31                        lda     unit_pos_x,x
68e5: 8d 62 18                     sta     MB_SET_R1L
68e8: b5 32                        lda     unit_pos_x+1,x
68ea: 8d 63 18                     sta     MB_SET_R1H
68ed: a0 00                        ldy     #$00              ;start with first obstacle (there are 21)
68ef: b9 cc 3f     :ObstLoop       lda     obstacle_t_f,y
68f2: 30 67                        bmi     :ObstDone         ;end of obstacle list found, bail
68f4: b9 81 76                     lda     obstacle_z_pos,y
68f7: 8d 64 18                     sta     MB_SET_R2L
68fa: b9 82 76                     lda     obstacle_z_pos+1,y
68fd: 8d 65 18                     sta     MB_SET_R2H
6900: b9 ab 76                     lda     obstacle_x_pos,y
6903: 8d 66 18                     sta     MB_SET_R3L
6906: b9 ac 76                     lda     obstacle_x_pos+1,y
6909: 8d 7d 18                     sta     MB_CALC_DIST      ;calculate distance
690c: c1 00                        cmp     ($00,x)           ;stall (6 cycles)
690e: ad 10 18                     lda     MB_RESULT_LO      ;save result
6911: 85 0c                        sta     ]distance
6913: ad 18 18                     lda     MB_RESULT_HI
6916: 85 0d                        sta     ]distance+1
6918: 8a                           txa                       ;are we checking the player?
6919: d0 08                        bne     :EnemyUnit        ;no, branch
691b: a9 80                        lda     #$80              ;use $480 as player radius
691d: c5 0c                        cmp     ]distance
691f: a9 04                        lda     #$04
6921: d0 2e                        bne     :SubDist          ;(always)

6923: b9 cc 3f     :EnemyUnit      lda     obstacle_t_f,y    ;get obstacle type
6926: 0a                           asl     A                 ;double it for index
6927: aa                           tax
6928: 24 cb                        bit     missile_flag      ;are we a missile?
692a: 10 1b                        bpl     :NotMissile       ;no, branch
692c: a5 0d                        lda     ]distance+1
692e: c9 80                        cmp     #$80              ;divide by two, with sign extension
6930: 6a                           ror     A
6931: 85 0b                        sta     ]tmp_0b
6933: a5 0c                        lda     ]distance
6935: 6a                           ror     A
6936: 18                           clc                       ;add to itself (to get distance * 1.5)
6937: 65 0c                        adc     ]distance
6939: 85 0c                        sta     ]distance
693b: a5 0b                        lda     ]tmp_0b
693d: 65 0d                        adc     ]distance+1
693f: 85 0d                        sta     ]distance+1
6941: 90 04                        bcc     :NotMissile
6943: a6 08                        ldx     ]saved_xreg       ;carry set, we're too far
6945: b0 10                        bcs     :NextObst         ;(always)

6947: bd 95 69     :NotMissile     lda     obstacle_radius,x ;compare distance to obstacle radius
694a: c5 0c                        cmp     ]distance
694c: bd 96 69                     lda     obstacle_radius+1,x
694f: a6 08                        ldx     ]saved_xreg
6951: e5 0d        :SubDist        sbc     ]distance+1
6953: a9 00                        lda     #$00
6955: b0 3b                        bcs     :Return
6957: c8           :NextObst       iny
6958: c8                           iny
6959: d0 94                        bne     :ObstLoop         ;(always)

                   ; Now check for a collision between the enemy unit and the player.
695b: a5 12        :ObstDone       lda     unit_state
695d: 05 14                        ora     enemy_state
695f: d0 31                        bne     :Return           ;enemy or player is dead, collision not possible
6961: 8a                           txa
6962: 49 02                        eor     #$02              ;X-reg is $00 or $02; flip it to $02/$00
6964: aa                           tax
6965: b5 2d                        lda     unit_pos_z,x      ;R2 = other unit Z
6967: 8d 64 18                     sta     MB_SET_R2L
696a: b5 2e                        lda     unit_pos_z+1,x
696c: 8d 65 18                     sta     MB_SET_R2H
696f: b5 31                        lda     unit_pos_x,x      ;R3 = other unit X
6971: 8d 66 18                     sta     MB_SET_R3L
6974: b5 32                        lda     unit_pos_x+1,x
6976: 8d 7d 18                     sta     MB_CALC_DIST      ;invoke function $1d (distance calc)
6979: c1 00                        cmp     ($00,x)           ;stall (6 cycles)
697b: ad 10 18                     lda     MB_RESULT_LO
697e: 85 0c                        sta     ]distance
6980: ad 18 18                     lda     MB_RESULT_HI
6983: 85 0d                        sta     ]distance+1
6985: a9 05                        lda     #$05              ;missile radius?
6987: 24 cb                        bit     missile_flag
6989: 10 02                        bpl     :NotMissile2
698b: a9 03                        lda     #$03              ;tank radius?
698d: 38           :NotMissile2    sec
698e: e5 0d                        sbc     ]distance+1
6990: a9 ff                        lda     #$ff
6992: a6 08        :Return         ldx     ]saved_xreg
6994: 60                           rts                       ;exit with carry set meaningfully

                   ; 
                   ; Collision radii for obstacle vs. player or enemy unit.
                   ; 
                   ; Compare to the obstacle vs. projectile table at $6139.
                   ; 
6995: 40 03        obstacle_radius .dd2    $0340             ;type=$00 narrow pyramid
6997: 40 03                        .dd2    $0340             ;type=$01 tall box
6999: 00 00                        .dd2    $0000
699b: 00 00                        .dd2    $0000
699d: 00 00                        .dd2    $0000
699f: 00 00                        .dd2    $0000
69a1: 00 00                        .dd2    $0000
69a3: 00 00                        .dd2    $0000
69a5: 00 00                        .dd2    $0000
69a7: 00 00                        .dd2    $0000
69a9: 00 00                        .dd2    $0000
69ab: 00 00                        .dd2    $0000
69ad: 00 04                        .dd2    $0400             ;type=$0c wide pyramid
69af: 00 00                        .dd2    $0000
69b1: 00 00                        .dd2    $0000
69b3: c0 03                        .dd2    $03c0             ;type=$0f short box

                   ; 
                   ; Determines which type of enemy tank is active or should be created.  The
                   ; determination is based on the number of missiles that have been launched at
                   ; the player.
                   ; 
                   ; On exit:
                   ;   Carry clear for slow tank, set for super tank
                   ; 
69b5: 18           GetTankType     clc
69b6: ad ec 02                     lda     missile_count     ;check number of missiles launched
69b9: 30 02                        bmi     :SlowTank         ;$80-ff, want slow tanks
69bb: c9 05                        cmp     #$05              ;super tank if it's $05 <= N < $80
69bd: 60           :SlowTank       rts

                   ; 
                   ; Creates a new enemy unit.
                   ; 
69be: a5 cd        CreateEnemyUnit lda     game_over_flags   ;is the player alive?
69c0: d0 26                        bne     CreateTank        ;no, always create a tank
69c2: a5 b9                        lda     score+1           ;score >= 100K?
69c4: d0 0f                        bne     :MaybeMissile     ;yes, think about a missile
69c6: ad 00 0a                     lda     DSW0              ;check DIP switch
69c9: 4a                           lsr     A                 ;get missile setting
69ca: 4a                           lsr     A
69cb: 29 03                        and     #%00000011        ;mask the other bits off
69cd: aa                           tax
69ce: a5 b8                        lda     score             ;get low byte of score
69d0: dd 8a 38                     cmp     missile_score,x   ;compare to missile threshold
69d3: 90 13                        bcc     CreateTank        ;score too low, create a tank
69d5: ac 2a 18     :MaybeMissile   ldy     POKEY_RANDOM      ;get a random number
69d8: 98                           tya
69d9: 4d 4a 03                     eor     missile_rand      ;XOR with previous random value
69dc: 8c 4a 03                     sty     missile_rand      ;save new random value
69df: 4a                           lsr     A                 ;shift low bit into carry
69e0: 90 06                        bcc     CreateTank        ;50/50 chance unless RNG has specific properties
69e2: a9 02                        lda     #$02              ;init counter used for missed-missile retry timeout
69e4: 85 d2                        sta     frame_count_256x
69e6: d0 3a                        bne     CreateMissile     ;(always)

                   ; 
                   ; Creates a new enemy tank.
                   ; 
69e8: ad 2a 18     CreateTank      lda     POKEY_RANDOM
69eb: 85 bc                        sta     enemy_turn_to     ;random heading
69ed: a9 00                        lda     #$00
69ef: 85 cb                        sta     missile_flag      ;not a missile
69f1: 85 14                        sta     enemy_state       ;alive
69f3: a9 01                        lda     #$01
69f5: 85 c4                        sta     move_counter
69f7: 85 d2                        sta     frame_count_256x  ;reset counter
69f9: 24 ce                        bit     play_flag         ;are we playing the game?
69fb: 10 20                        bpl     :BeNice           ;no, branch
69fd: a5 b9                        lda     score+1           ;over 100K points?
69ff: d0 0d                        bne     :BeMean           ;yes, branch
6a01: 38                           sec                       ;compare player score to enemy "score"
6a02: a5 b8                        lda     score
6a04: e5 ba                        sbc     enemy_score
6a06: 90 15                        bcc     :BeNice
6a08: f0 13                        beq     :BeNice
6a0a: c9 07                        cmp     #$07              ;is player_score - enemy_score < 7000?
6a0c: 90 02                        bcc     :BeMeh            ;yes, be sort of nice
                   ; Get a value from $00-78 that affects the angle at which the enemy unit is
                   ; placed.  Smaller values place the enemy closer to where the player is facing,
                   ; so we use those when we're being nice.
6a0e: a9 07        :BeMean         lda     #$07
6a10: 4a           :BeMeh          lsr     A                 ;1-7 becomes 0-3
6a11: f0 0a                        beq     :BeNice           ;player barely ahead of enemy
6a13: aa                           tax                       ;X-reg is now left-shift counter (1-3)
6a14: a9 0f                        lda     #$0f
6a16: 38           :Loop           sec
6a17: 2a                           rol     A                 ;$0f becomes $1e / $3c / $78
6a18: ca                           dex
6a19: d0 fb                        bne     :Loop
6a1b: f0 28                        beq     CreateCommon      ;(always)

6a1d: a9 0f        :BeNice         lda     #$0f
6a1f: 4c 45 6a                     jmp     CreateCommon

                   ; 
                   ; Creates a missile.
                   ; 
6a22: ee ec 02     CreateMissile   inc     missile_count     ;increment missile counter
6a25: a9 18                        lda     #$18              ;set altitude = $1800
6a27: 8d e5 02                     sta     ypos_by_type+13
6a2a: a9 00                        lda     #$00
6a2c: 8d e4 02                     sta     ypos_by_type+12   ;((missile type = $16) & 0x07) * 2 = 12
6a2f: a9 80                        lda     #$80
6a31: 85 c5                        sta     enemy_rev_flags   ;(not sure what $80 does... nothing tests for it?)
6a33: a2 ff                        ldx     #$ff              ;init audio channel 3/4 frequencies
6a35: 8e 24 18                     stx     POKEY_AUDF3
6a38: ca                           dex
6a39: 8e 26 18                     stx     POKEY_AUDF4
6a3c: a9 ff                        lda     #$ff              ;set flag to indicate a missile is active
6a3e: 85 cb                        sta     missile_flag
6a40: ad 2a 18                     lda     POKEY_RANDOM      ;get a random value for angle adjustment
6a43: 29 0f                        and     #$0f              ;reduce it to 0-15
                   ; 
                   ; Common code for creating a new tank or missile.
                   ; 
                   ; The caller passes an angle adjustment in.  The value is added to the player's
                   ; facing to determine the angle at which the new unit will appear.  Small values
                   ; will put the enemy unit to the front, while large values will put it to the
                   ; sides or rear.
                   ; 
                   ; We want the enemy unit to be within vision / radar range, so we need to ensure
                   ; it's between the near and far planes ($03ff-7aff).
                   ; 
                   ; On entry:
                   ;   A-reg: enemy angle adjustment
                   ; 
                   • Clear variables
                   ]angle          .var    $08    {addr/1}
                   ]zoff           .var    $0a    {addr/2}
                   ]cosine         .var    $19    {addr/2}
                   ]sine           .var    $1d    {addr/2}

6a45: 85 08        CreateCommon    sta     ]angle
6a47: ad 2a 18                     lda     POKEY_RANDOM
6a4a: 25 08                        and     ]angle            ;reduce angle value by zeroing random bits
6a4c: 24 3a                        bit     nmi_count         ;check our 250Hz counter
6a4e: 70 02                        bvs     :NoInvert         ;essentially a 50/50 chance
6a50: 49 ff                        eor     #$ff              ;flip the bits
6a52: 18           :NoInvert       clc
6a53: 65 2a                        adc     plyr_facing_hi    ;add to player facing
                   ; Compute the Z coordinate using cos(angle).
6a55: 85 08                        sta     ]angle
6a57: 20 4b 5e                     jsr     CalcCosine        ;compute cos(angle)
6a5a: 85 0a                        sta     ]zoff             ;this is a value from $0000-ffff
6a5c: 85 19                        sta     ]cosine
6a5e: 86 1a                        stx     ]cosine+1
6a60: 8a                           txa
6a61: c9 80                        cmp     #$80              ;divide by 4 with sign extension
6a63: 6a                           ror     A                 ;yields essentially +/- 8191
6a64: 66 0a                        ror     ]zoff
6a66: c9 80                        cmp     #$80
6a68: 6a                           ror     A
6a69: 66 0a                        ror     ]zoff
6a6b: 85 0b                        sta     ]zoff+1
6a6d: a5 19                        lda     ]cosine           ;compute zoff = cos(angle) - zoff
6a6f: 38                           sec                       ;which leaves us with 3/4 cos(angle)
6a70: e5 0a                        sbc     ]zoff             ;about $6000 units away
6a72: 85 0a                        sta     ]zoff
6a74: a5 1a                        lda     ]cosine+1
6a76: e5 0b                        sbc     ]zoff+1
6a78: aa                           tax                       ;spill zoff high to X-reg
6a79: 24 cb                        bit     missile_flag      ;is this a missile?
6a7b: 30 0d                        bmi     :Far1             ;yes, use max range
6a7d: ad 2a 18                     lda     POKEY_RANDOM
6a80: 4a                           lsr     A                 ;put low bit of random number in carry
6a81: 08                           php                       ;save it
6a82: 8a                           txa                       ;retrieve zoff high
6a83: 90 05                        bcc     :Far1
6a85: c9 80                        cmp     #$80              ;cut the value in half, bringing the enemy closer
6a87: 6a                           ror     A
6a88: 66 0a                        ror     ]zoff
6a8a: 85 0b        :Far1           sta     ]zoff+1
                   ; Now repeat the process for sin(angle) and the X coordinate.
6a8c: a5 08                        lda     ]angle
6a8e: 20 4e 5e                     jsr     CalcSine          ;compute sin(angle)
                   ]xoff           .var    $08    {addr/2}

6a91: 85 08                        sta     ]xoff             ;this is a value from $0000-ffff
6a93: 85 1d                        sta     ]sine
6a95: 86 1e                        stx     ]sine+1
6a97: 8a                           txa
6a98: c9 80                        cmp     #$80              ;divide by 4 with sign extension
6a9a: 6a                           ror     A                 ;yields essentially +/- 8191
6a9b: 66 08                        ror     ]xoff
6a9d: c9 80                        cmp     #$80
6a9f: 6a                           ror     A
6aa0: 66 08                        ror     ]xoff
6aa2: 85 09                        sta     ]xoff+1
6aa4: 38                           sec                       ;compute xoff = sin(angle) - zoff
6aa5: a5 1d                        lda     ]sine             ;which leaves us with 3/4 sin(angle)
6aa7: e5 08                        sbc     ]xoff             ;about $6000 units away
6aa9: 85 08                        sta     ]xoff
6aab: a5 1e                        lda     ]sine+1
6aad: e5 09                        sbc     ]xoff+1
6aaf: 24 cb                        bit     missile_flag      ;are we creating a missile?
6ab1: 30 08                        bmi     :Far2             ;yes, use max range
6ab3: 28                           plp                       ;get the random carry bit from earlier
6ab4: 90 05                        bcc     :Far2
6ab6: c9 80                        cmp     #$80              ;divide by two with sign extension
6ab8: 6a                           ror     A                 ; to bring the enemy closer
6ab9: 66 08                        ror     ]xoff
6abb: 85 09        :Far2           sta     ]xoff+1
                   ; Use the X/Z values to set the enemy unit's position as an offset from the
                   ; player's current position.  There's no test here for collision with an
                   ; obstacle.  (Are the obstacles placed in a way that prevents this?)
6abd: 18                           clc                       ;add xoff to player X pos
6abe: a5 2d                        lda     unit_pos_z
6ac0: 65 0a                        adc     ]zoff
6ac2: 85 2f                        sta     enemy_pos_z       ;save as enemy X pos
6ac4: a5 2e                        lda     unit_pos_z+1
6ac6: 65 0b                        adc     ]zoff+1
6ac8: 85 30                        sta     enemy_pos_z+1
6aca: a5 31                        lda     unit_pos_x        ;same for zoff/zpos
6acc: 18                           clc
6acd: 65 08                        adc     ]xoff
6acf: 85 33                        sta     enemy_pos_x
6ad1: a5 32                        lda     unit_pos_x+1
6ad3: 65 09                        adc     ]xoff+1
6ad5: 85 34                        sta     enemy_pos_x+1
6ad7: 24 cb                        bit     missile_flag      ;is this a missile?
6ad9: 10 07                        bpl     :NotMissile
6adb: 20 10 68                     jsr     CalcAngleToPlayer ;point the missile at the player
6ade: 85 2c                        sta     enemy_facing      ;set facing
6ae0: 85 bc                        sta     enemy_turn_to     ;set desired heading
6ae2: a9 00        :NotMissile     lda     #$00
6ae4: 85 c9                        sta     enemy_known_flag  ;new enemy, clear flag
6ae6: 85 d1                        sta     rez_protect       ;suppress aggression briefly
6ae8: 60                           rts

                   ; 
                   ; Draws the radar at the top of the screen.
                   ; 
                   ; Also, if a missile misses the player and gets too far away, this spawns a new
                   ; one.
                   ; 
                   • Clear variables
                   ]delta_x        .var    $04    {addr/2}
                   ]delta_y        .var    $06    {addr/2}
                   ]tmp_0c         .var    $0c    {addr/1}

6ae9: a2 3c        DrawRadar       ldx     #<vg_radar        ;draw the N/S/E/W marks and the vision cone lines
6aeb: a9 35                        lda     #>vg_radar
6aed: 20 5a 7a                     jsr     VgJsrToAddr       ;leaves beam positioned in center of radar
6af0: a9 00                        lda     #$00
6af2: 85 05                        sta     ]delta_x+1
6af4: 85 07                        sta     ]delta_y+1
6af6: 85 36                        sta     cur_beam_xc+1
6af8: a9 0b                        lda     #$0b              ;rotate about 15.5 degrees per frame
6afa: 18                           clc
6afb: 65 bf                        adc     radar_sweep_ang
6afd: 85 bf                        sta     radar_sweep_ang
                   ; Compute the screen coordinates of the end of the beam.  In a Cartesian plane
                   ; angle 0 is toward the right (+X), but we want to have angle 0 at the top (+Y),
                   ; so everything needs to be rotated 90 degrees.  We also want it to rotate
                   ; clockwise rather than CCW.  We can do this trivially by flipping the equation,
                   ; using X=sin(theta) and Y=cos(theta).
6aff: 20 4e 5e                     jsr     CalcSine          ;calc sin(theta)
6b02: 8a                           txa                       ;high byte to A-reg
6b03: c9 80                        cmp     #$80              ;divide by two with sign extension
6b05: 6a                           ror     A
6b06: 85 04                        sta     ]delta_x
6b08: 85 35                        sta     cur_beam_xc
6b0a: 10 06                        bpl     :IsPosX
6b0c: a9 ff                        lda     #$ff              ;sign-extension (could just DEC $05?)
6b0e: 85 05                        sta     ]delta_x+1
6b10: 85 36                        sta     cur_beam_xc+1
6b12: a5 bf        :IsPosX         lda     radar_sweep_ang   ;get sweep angle
6b14: 20 4b 5e                     jsr     CalcCosine        ;calc cos(theta)
6b17: 8a                           txa
6b18: c9 80                        cmp     #$80
6b1a: 6a                           ror     A
6b1b: 85 06                        sta     ]delta_y
6b1d: 10 04                        bpl     :IsPosY
6b1f: a9 ff                        lda     #$ff              ;sign-extension (could just DEC $07?)
6b21: 85 07                        sta     ]delta_y+1
6b23: a9 a0        :IsPosY         lda     #%10100000        ;intensity=9
6b25: 85 01                        sta     vg_intensity      ;set intensity for radar line
6b27: a9 3c                        lda     #$3c              ;radar is at Y=+316 ($13c), so update the
6b29: 18                           clc                       ; current-beam-position tracker
6b2a: 65 06                        adc     ]delta_y
6b2c: 85 37                        sta     cur_beam_yc
6b2e: a9 01                        lda     #$01
6b30: 65 07                        adc     ]delta_y+1
6b32: 85 38                        sta     cur_beam_yc+1
6b34: 20 ab 7a                     jsr     VgVector          ;draw radar line using $04-07
6b37: a5 14                        lda     enemy_state       ;is the enemy unit alive?
6b39: f0 09                        beq     :CheckEnemy       ;yes, figure out the blip
6b3b: a9 00                        lda     #$00              ;no enemy, don't draw a blip
6b3d: 8d 25 18                     sta     POKEY_AUDC3       ;silence the missile buzz
6b40: 8d 27 18                     sta     POKEY_AUDC4       ;(why here?)
6b43: 60                           rts

                   ]ping_x         .var    $04    {addr/2}
                   ]ping_y         .var    $06    {addr/2}
                   ]delta_z        .var    $08    {addr/2}
                   ]delta_x        .var    $0a    {addr/2}

6b44: 20 10 68     :CheckEnemy     jsr     CalcAngleToPlayer ;angle in A-reg, delta_x/delta_z set
6b47: 49 80                        eor     #$80              ;flip to get angle from player (+= 180 deg)
6b49: 38                           sec
6b4a: 85 a6                        sta     temp_a6
6b4c: a5 2a                        lda     plyr_facing_hi    ;make it relative to player facing
6b4e: e5 a6                        sbc     temp_a6
6b50: 85 a6                        sta     temp_a6           ;angle from viewer to enemy
                   ; See if radar scan will sweep over enemy this frame.
6b52: a5 bf                        lda     radar_sweep_ang   ;radar scan direction
6b54: 38                           sec
6b55: e5 a6                        sbc     temp_a6           ;compare to enemy direction
6b57: 30 09                        bmi     :NotIntersect     ;not there yet
6b59: c9 0c                        cmp     #$0c              ;we rotate by $0b; see if it's in the gap
6b5b: b0 05                        bcs     :NotIntersect     ;not this frame
6b5d: a9 f0                        lda     #$f0              ;reset radar blip intensity to max
6b5f: 8d e9 02                     sta     radar_blip_inten
6b62: a5 08        :NotIntersect   lda     ]delta_z          ;R2 = delta Z
6b64: 8d 64 18                     sta     MB_SET_R2L
6b67: a5 09                        lda     ]delta_z+1
6b69: 8d 65 18                     sta     MB_SET_R2H
6b6c: a5 0a                        lda     ]delta_x          ;R3 = delta X
6b6e: 8d 66 18                     sta     MB_SET_R3L
6b71: a5 0b                        lda     ]delta_x+1
6b73: 8d 67 18                     sta     MB_SET_R3H
6b76: 8d 7e 18                     sta     MB_CALC_HYPOT     ;compute distance
6b79: c1 00                        cmp     ($00,x)           ;stall (6 cycles)
6b7b: ad 10 18                     lda     MB_RESULT_LO
6b7e: 85 0c                        sta     ]tmp_0c
6b80: ad 18 18                     lda     MB_RESULT_HI
6b83: 8d e8 02                     sta     enemy_dist_hi     ;save high byte of distance for other code
6b86: aa                           tax                       ;preserve in X-reg
6b87: 30 0b                        bmi     :Silence          ;too far for missile sound
6b89: 4a                           lsr     A                 ;divide by 8
6b8a: 4a                           lsr     A
6b8b: 4a                           lsr     A
6b8c: 29 0f                        and     #$0f              ;volume 0-16
6b8e: 49 af                        eor     #$af              ;add POKEY AUDCn bits, invert so nearer=louder
6b90: 24 cb                        bit     missile_flag      ;is this a missile?
6b92: 30 02                        bmi     :MissileNoise     ;yes, set volume
6b94: a9 00        :Silence        lda     #$00              ;no, silence missile sound
6b96: 8d 25 18     :MissileNoise   sta     POKEY_AUDC3       ;set missile buzz volume
6b99: 8d 27 18                     sta     POKEY_AUDC4       ;(frequency set elsewhere)
6b9c: 8a                           txa                       ;get high byte of distance
6b9d: 8d 68 18                     sta     MB_SET_R4L        ;put that in R4 low
6ba0: c9 80                        cmp     #$80              ;is enemy nearby?
6ba2: 90 1a                        bcc     :InRange          ;yes, branch
                   ; Player out of range of enemy.  If it's a missile, we kill it and make a new
                   ; unit.  If it's a tank we keep going.
6ba4: a9 00                        lda     #$00
6ba6: 85 c9                        sta     enemy_known_flag  ;clear flag so we issue alert sound
6ba8: 24 cb                        bit     missile_flag      ;was it a missile?
6baa: 10 0f                        bpl     :NotMissile       ;no, keep going
6bac: a5 d2                        lda     frame_count_256x  ;check the timer
6bae: c9 04                        cmp     #$04
6bb0: 90 06                        bcc     :MakeMissile      ;not tired of missiles yet, make another
6bb2: 20 e8 69                     jsr     CreateTank        ;let's go back to tanks
6bb5: 4c 5a 6c                     jmp     :SkipBlip

6bb8: 20 22 6a     :MakeMissile    jsr     CreateMissile
6bbb: 4c 5a 6c     :NotMissile     jmp     :SkipBlip

6bbe: ad e9 02     :InRange        lda     radar_blip_inten  ;get radar blip intensity
6bc1: d0 01                        bne     :DrawBlip         ;nonzero, so draw it
6bc3: 60                           rts

6bc4: c9 f0        :DrawBlip       cmp     #$f0              ;are we at max intensity?
6bc6: 90 05                        bcc     :NotMax           ;no, branch
6bc8: a9 01                        lda     #$01              ;sound: radar ping
6bca: 20 85 79                     jsr     StartSoundEffect
                   ; Compute the screen coordinates at which we want to draw the blip.  We're
                   ; effectively using polar coordinates: R4L (the object Z coordinate) was set to
                   ; the distance, R5 (the object X coordinate) is set to zero, and the angle to
                   ; the enemy is in $a6.
                   ; 
                   ; (Could we use the position from the visible object list instead of calculating
                   ; it here?)
6bcd: a5 a6        :NotMax         lda     temp_a6           ;angle from viewer to enemy
6bcf: 20 4e 5e                     jsr     CalcSine
6bd2: 8d 60 18                     sta     MB_SET_R0L        ;R0=sin(theta)
6bd5: 8e 61 18                     stx     MB_SET_R0H
6bd8: a5 a6                        lda     temp_a6
6bda: 20 4b 5e                     jsr     CalcCosine
6bdd: 8d 62 18                     sta     MB_SET_R1L        ;R1=cos(theta)
6be0: 8e 63 18                     stx     MB_SET_R1H
6be3: a9 00                        lda     #$00              ;R2=0
6be5: 8d 64 18                     sta     MB_SET_R2L
6be8: a9 00                        lda     #$00
6bea: 8d 65 18                     sta     MB_SET_R2H
6bed: a9 3c                        lda     #$3c              ;R3=$013c
6bef: 8d 66 18                     sta     MB_SET_R3L        ;(screen Y offset of center of radar, which was set
6bf2: a9 01                        lda     #$01              ; in cur_beam_yc earlier)
6bf4: 8d 67 18                     sta     MB_SET_R3H
6bf7: a9 00                        lda     #$00              ;set R4H to zero; R4L is high byte of distance
6bf9: 8d 69 18                     sta     MB_SET_R4H
6bfc: 85 01                        sta     vg_intensity      ;set intensity = 0
6bfe: 8d 6a 18                     sta     MB_SET_R5L        ;R5=0
6c01: 8d 71 18                     sta     MB_SCREEN_X       ;model transform, result is R8 / R7
6c04: 20 80 5b                     jsr     MbWaitForResult   ;we don't want result; we want R7 and R8 individually
6c07: 8d 77 18                     sta     MB_GET_R7         ;get R7 (model txform Z coord)
6c0a: ad 10 18                     lda     MB_RESULT_LO      ;offset by beam position
6c0d: 38                           sec
6c0e: e5 35                        sbc     cur_beam_xc
6c10: 85 04                        sta     ]ping_x
6c12: ad 18 18                     lda     MB_RESULT_HI
6c15: 8d 79 18                     sta     MB_GET_R8         ;initiate require for R8
6c18: e5 36                        sbc     cur_beam_xc+1
6c1a: 85 05                        sta     ]ping_x+1
6c1c: ad 10 18                     lda     MB_RESULT_LO      ;get R8 (model txform X coord)
6c1f: 38                           sec                       ;offset by beam position
6c20: e5 37                        sbc     cur_beam_yc
6c22: 85 06                        sta     ]ping_y
6c24: ad 18 18                     lda     MB_RESULT_HI
6c27: e5 38                        sbc     cur_beam_yc+1
6c29: 85 07                        sta     ]ping_y+1
6c2b: 20 ab 7a                     jsr     VgVector          ;move to ping_x,ping_y
6c2e: ad e9 02                     lda     radar_blip_inten  ;get current intensity
6c31: 29 e0                        and     #%11100000        ;mask other bits
6c33: 20 8a 7a                     jsr     VgDrawPoint       ;draw point; updates vg_intensity
6c36: a5 01                        lda     vg_intensity      ;get intensity we just used
6c38: 20 8a 7a                     jsr     VgDrawPoint       ;draw it again
6c3b: 24 ce                        bit     play_flag         ;are we playing?
6c3d: 30 05                        bmi     :Playing          ;yes, draw strings
6c3f: 2c 4b 03                     bit     attract_logo_flag ;showing the logo?
6c42: 30 0b                        bmi     :NoInRange        ;yes, don't draw strings
6c44: a9 02        :Playing        lda     #$02
6c46: 25 c6                        and     frame_counter     ;flash it every 2 frames
6c48: f0 05                        beq     :NoInRange
6c4a: a2 10                        ldx     #$10              ;"enemy in range"
6c4c: 20 98 6c                     jsr     DrawString
6c4f: a5 c9        :NoInRange      lda     enemy_known_flag  ;have we played alert for this enemy?
6c51: d0 07                        bne     :SkipBlip         ;yes
6c53: e6 c9                        inc     enemy_known_flag  ;set to 1
6c55: a9 10                        lda     #$10              ;sound: new enemy alert
6c57: 20 85 79                     jsr     StartSoundEffect
6c5a: ad e9 02     :SkipBlip       lda     radar_blip_inten  ;get intensity of radar ping
6c5d: f0 06                        beq     :Return           ;zero, leave it be
6c5f: 38                           sec                       ;reduce by 8
6c60: e9 08                        sbc     #$08
6c62: 8d e9 02                     sta     radar_blip_inten
6c65: 60           :Return         rts

                   ; 
                   ; Updates position of both projectiles.
                   ; 
                   UpdateProjectiles
6c66: a2 02                        ldx     #$02              ;16-bit index, so this is projectile #1
6c68: b5 24        :Loop           lda     projectile_state_0,x
6c6a: f0 1e                        beq     :NextProj         ;inactive?
6c6c: 30 1c                        bmi     :NextProj         ;exploding?
6c6e: d6 24                        dec     projectile_state_0,x ;reduce time-to-live
                   ; Move a projectile.
6c70: 18                           clc
6c71: b5 a8                        lda     proj_pos_z,x
6c73: 75 b0                        adc     proj_vel_z,x
6c75: 95 a8                        sta     proj_pos_z,x
6c77: b5 a9                        lda     proj_pos_z+1,x
6c79: 75 b1                        adc     proj_vel_z+1,x
6c7b: 95 a9                        sta     proj_pos_z+1,x
6c7d: b5 ac                        lda     proj_pos_x,x
6c7f: 18                           clc
6c80: 75 b4                        adc     proj_vel_x,x
6c82: 95 ac                        sta     proj_pos_x,x
6c84: b5 ad                        lda     proj_pos_x+1,x
6c86: 75 b5                        adc     proj_vel_x+1,x
6c88: 95 ad                        sta     proj_pos_x+1,x
6c8a: ca           :NextProj       dex
6c8b: ca                           dex
6c8c: f0 da                        beq     :Loop
6c8e: 60                           rts

6c8f: 6b                           .dd1    $6b               ;(checksum adj)
                   ; 
                   ; Addresses of language-specific functions that do an address lookup.
                   ; 
                   ; (Could just store addresses of string index tables?)
                   ; 
6c90: cb 6c        language_tab    .dd2    StrEnglish-1
6c92: c0 6c                        .dd2    StrGerman-1
6c94: b5 6c                        .dd2    StrFrench-1
6c96: aa 6c                        .dd2    StrSpanish-1

                   ; 
                   ; Draws a pre-formed string in the language specified by the DIP switch
                   ; configuration.
                   ; 
                   ; On entry:
                   ;   X-reg: string index
                   ; 
6c98: ad 00 0a     DrawString      lda     DSW0              ;get DIP switch setting
6c9b: 2a                           rol     A                 ;roll high 3 bits into low 3 bits
6c9c: 2a                           rol     A                 ;(language setting is high two bits)
6c9d: 2a                           rol     A
6c9e: 2a                           rol     A
6c9f: 29 06                        and     #%00000110        ;0/2/4/6
6ca1: a8                           tay
6ca2: b9 91 6c                     lda     language_tab+1,y  ;get address of function that gets an address
6ca5: 48                           pha
6ca6: b9 90 6c                     lda     language_tab,y
6ca9: 48                           pha
6caa: 60                           rts                       ;jump to it

                   • Clear variables
                   ]data_ptr       .var    $3b    {addr/2}

6cab: bd f6 70     StrSpanish      lda     str_addr_es,x
6cae: 85 3b                        sta     ]data_ptr
6cb0: bd f7 70                     lda     str_addr_es+1,x
6cb3: 4c d4 6c                     jmp     DrawStringPtr

6cb6: bd 3a 6f     StrFrench       lda     str_addr_fr,x
6cb9: 85 3b                        sta     ]data_ptr
6cbb: bd 3b 6f                     lda     str_addr_fr+1,x
6cbe: 4c d4 6c                     jmp     DrawStringPtr

6cc1: bd 88 72     StrGerman       lda     str_addr_de,x
6cc4: 85 3b                        sta     ]data_ptr
6cc6: bd 89 72                     lda     str_addr_de+1,x
6cc9: 4c d4 6c                     jmp     DrawStringPtr

6ccc: bd 93 6d     StrEnglish      lda     str_addr_en,x
6ccf: 85 3b                        sta     ]data_ptr
6cd1: bd 94 6d                     lda     str_addr_en+1,x
                   ; 
                   ; Draws a string from a table.  There are 24 pre-defined strings.
                   ; 
                   ; Each table entry is a two-byte header followed by a DCI string.
                   ;  +00: absolute X position / 4
                   ;  +01: absolute Y position / 4
                   ;  +02: character data, with high bit set on last byte
                   ; 
                   ; If the X and Y position are both zero, the string is drawn at the current beam
                   ; position.
                   ; 
                   ; The character set is:
                   ;   ' ', 0-9, A-Z, ' ', '-', '(C)', '(P)'
                   ; 
                   ; Each character value is 2x the character index, which allows us to use the
                   ; value directly as an index into a table of 16-bit VJSR instructions.
                   ; 
                   ; Strings with indices $00-12 are displayed at half size, strings $14-2e at full
                   ; size.
                   ; 
                   ; On entry:
                   ;   $3b: string data ptr, low
                   ;   A-reg: string ptr, high
                   ;   X-reg: string index * 2
                   ; 
                   ]avg_index      .var    $08    {addr/1}
                   ]raw_char       .var    $0a    {addr/1}
                   ]saved_y        .var    $0c    {addr/1}
                   ]b_scale        .var    $0d    {addr/1}

6cd4: 85 3c        DrawStringPtr   sta     ]data_ptr+1
6cd6: a0 00                        ldy     #$00
6cd8: 8a                           txa                       ;(could CPX?)
6cd9: c9 14                        cmp     #$14              ;is it $00-12?
6cdb: a9 02                        lda     #$02
6cdd: 90 02                        bcc     :Scale2           ;yes, use scale=2 (half size)
6cdf: a9 01                        lda     #$01              ;no, use scale=1 (normal size)
6ce1: 85 0d        :Scale2         sta     ]b_scale          ;save for later
6ce3: b1 3b                        lda     (]data_ptr),y     ;Y pos
6ce5: c8                           iny
6ce6: 11 3b                        ora     (]data_ptr),y     ;X pos
6ce8: f0 0e                        beq     :NoCenter         ;both zero, don't start from center
6cea: 20 6a 7a                     jsr     VgCenter          ;center beam
6ced: a0 01                        ldy     #$01
6cef: b1 3b                        lda     (]data_ptr),y     ;Y pos
6cf1: aa                           tax                       ;in X-reg
6cf2: 88                           dey
6cf3: b1 3b                        lda     (]data_ptr),y     ;X-pos
6cf5: 20 8e 7a                     jsr     VgVec8I           ;move to desired position; Y-reg=0 so no draw
6cf8: a5 0d        :NoCenter       lda     ]b_scale          ;set the scale factor
6cfa: 20 81 7a                     jsr     VgScaleL0
6cfd: a0 02                        ldy     #$02              ;start of character data
6cff: a9 00                        lda     #$00              ;index into AVG data we're generating
6d01: 85 08                        sta     ]avg_index
6d03: b1 3b        :Loop           lda     (]data_ptr),y     ;get byte of char data
6d05: 85 0a                        sta     ]raw_char
6d07: 29 7f                        and     #$7f              ;strip high bit
6d09: c8                           iny                       ;advance char index
6d0a: 84 0c                        sty     ]saved_y          ;save Y-reg
6d0c: aa                           tax                       ;put char value in X-reg; already x2
6d0d: bd f0 33                     lda     vg_glyph_calls,x  ;get VJSR to glyph
6d10: a4 08                        ldy     ]avg_index
6d12: 91 02                        sta     (vg_cmd_ptr),y    ;add to command buffer
6d14: c8                           iny
6d15: bd f1 33                     lda     vg_glyph_calls+1,x
6d18: 91 02                        sta     (vg_cmd_ptr),y
6d1a: c8                           iny
6d1b: 84 08                        sty     ]avg_index        ;advance command index (max 127 chars per string)
6d1d: a4 0c                        ldy     ]saved_y
6d1f: 24 0a                        bit     ]raw_char         ;was this the last?
6d21: 10 e0                        bpl     :Loop             ;not yet
6d23: a4 08                        ldy     ]avg_index
6d25: 88                           dey
6d26: 20 76 7a                     jsr     VgPtrAddY         ;update the cmd ptr
6d29: a9 01                        lda     #$01              ;reset scale to 1:1
6d2b: 4c 81 7a                     jmp     VgScaleL0

                   ; 
                   ; Draws the current score, high score, and displays the number of lives left.
                   ; 
                   • Clear variables
                   ]life_count     .var    $08    {addr/1}
                   ]score_arg      .var    $0a    {addr/2}

6d2e: 20 6a 7a     DrawScoreLives  jsr     VgCenter          ;center beam
6d31: a0 00                        ldy     #$00              ;intensity=0
6d33: 84 00                        sty     $00               ;?
6d35: a2 5a                        ldx     #$5a              ;deltaY=+360
6d37: a9 20                        lda     #$20              ;deltaX=+128
6d39: 20 8e 7a                     jsr     VgVec8I           ;move beam
6d3c: a5 cc                        lda     player_lives      ;get player lives remaining
6d3e: f0 19                        beq     :NoLives
6d40: 85 08                        sta     ]life_count
6d42: a0 00                        ldy     #$00
6d44: ae 91 35                     ldx     vg_life_icon+1    ;copy the tank icon VJSR
6d47: ad 90 35     :LifeLoop       lda     vg_life_icon
6d4a: 91 02                        sta     (vg_cmd_ptr),y    ;add to cmd list
6d4c: 8a                           txa
6d4d: c8                           iny
6d4e: 91 02                        sta     (vg_cmd_ptr),y
6d50: c8                           iny
6d51: c6 08                        dec     ]life_count
6d53: d0 f2                        bne     :LifeLoop
6d55: 88                           dey                       ;function adds Y-reg + 1, so dec Y-reg
6d56: 20 76 7a                     jsr     VgPtrAddY         ;update cmd list pointer
6d59: a2 18        :NoLives        ldx     #$18              ;"score     000"
6d5b: 20 98 6c                     jsr     DrawString        ;draws string at fixed screen position
6d5e: a0 00                        ldy     #$00              ;intensity=0
6d60: a2 00                        ldx     #$00              ;deltaY=0
6d62: a9 d6                        lda     #$d6              ;deltaX=-168 (back up to hole in string)
6d64: 20 8e 7a                     jsr     VgVec8I
6d67: a9 b8                        lda     #$b8              ;draw $b8/b9 (current score)
6d69: 20 9e 7b                     jsr     DrawFourDigits
6d6c: a2 0e                        ldx     #$0e              ;"high score      000"
6d6e: 20 98 6c                     jsr     DrawString        ;draw string (at half size)
6d71: a9 02                        lda     #$02              ;set scale to half size
6d73: 20 81 7a                     jsr     VgScaleL0
6d76: a0 00                        ldy     #$00              ;intensity=0
6d78: a2 00                        ldx     #$00              ;deltaY=0
6d7a: a9 d6                        lda     #$d6              ;deltaX=-168 (back up to hole in string)
6d7c: 20 8e 7a                     jsr     VgVec8I
6d7f: ad 00 03                     lda     hs_scores         ;get top high score
6d82: 85 0a                        sta     ]score_arg
6d84: ad 01 03                     lda     hs_scores+1
6d87: 85 0b                        sta     ]score_arg+1
6d89: a9 0a                        lda     #$0a              ;get values from $0a/0b
6d8b: 20 9e 7b                     jsr     DrawFourDigits    ;draw 4-digit number
6d8e: a9 01                        lda     #$01              ;reset scale
6d90: 4c 81 7a                     jmp     VgScaleL0

                   ; 
                   ; English strings.
                   ; 
6d93: c3 6d        str_addr_en     .dd2    L6DC3             ;-440,+296 'ENEMY TO '
6d95: ce 6d                        .dd2    L6DCE             ;(rel) 'LEFT'
6d97: d4 6d                        .dd2    L6DD4             ;(rel) 'RIGHT'
6d99: db 6d                        .dd2    L6DDB             ;(rel) 'REAR'
6d9b: e1 6d                        .dd2    L6DE1             ;+28,+104 'ENTER YOUR INITIALS'
6d9d: f6 6d                        .dd2    L6DF6             ;-240,+64 'CHANGE LETTER WITH RIGHT HAND CONTROLLER'
6d9f: 20 6e                        .dd2    L6E20             ;-180,+32 'SELECT LETTER WITH FIRE BUTTON'
6da1: 40 6e                        .dd2    L6E40             ;+128,+280 'HIGH SCORE      000'
6da3: 55 6e                        .dd2    L6E55             ;-440,+360 'ENEMY IN RANGE'
6da5: 65 6e                        .dd2    L6E65             ;-440,+328 'MOTION BLOCKED BY OBJECT'
6da7: 7f 6e                        .dd2    L6E7F             ;-112,+96 'GAME OVER'
6da9: 8a 6e                        .dd2    L6E8A             ;-136,+0 'PRESS START'
6dab: 97 6e                        .dd2    L6E97             ;+128,+320 'SCORE     000'
6dad: a6 6e                        .dd2    L6EA6             ;-112,+160 'HIGH SCORES'
6daf: b3 6e                        .dd2    L6EB3             ;(rel) '000 '
6db1: b9 6e                        .dd2    L6EB9             ;-256,+96 'GREAT SCORE'
6db3: c6 6e                        .dd2    L6EC6             ;-200,+0 '1       2     S'
6db5: d7 6e                        .dd2    L6ED7             ;-200,+0 '1       1'
6db7: e2 6e                        .dd2    L6EE2             ;-200,+0 '2     S 1'
6db9: ed 6e                        .dd2    L6EED             ;-200,+0 '  COIN    PLAY'
6dbb: fd 6e                        .dd2    L6EFD             ;-144,-88 'INSERT COIN'
6dbd: 0a 6f                        .dd2    L6F0A             ;-348,-280 'BONUS TANK AT '
6dbf: 1a 6f                        .dd2    L6F1A             ;(rel) '000 AND 100000'
6dc1: 2a 6f                        .dd2    L6F2A             ;-168,-240 '(C)(P)  ATARI 1980'
6dc3: 92 4a 1e 30+ L6DC3           .bulk   $92,$4a,$1e,$30,$1e,$2e,$46,$00,$3c,$32,$80
6dce: 00 00 2c 1e+ L6DCE           .bulk   $00,$00,$2c,$1e,$20,$bc
6dd4: 00 00 38 26+ L6DD4           .bulk   $00,$00,$38,$26,$22,$24,$bc
6ddb: 00 00 38 1e+ L6DDB           .bulk   $00,$00,$38,$1e,$16,$b8
6de1: 07 1a 1e 30+ L6DE1           .bulk   $07,$1a,$1e,$30,$3c,$1e,$38,$00,$46,$32,$3e,$38,$00,$26,$30,$26
                                    +      $3c,$26,$16,$2c,$ba
6df6: c4 10 1a 24+ L6DF6           .bulk   $c4,$10,$1a,$24,$16,$30,$22,$1e,$00,$2c,$1e,$3c,$3c,$1e,$38,$00
                                    +      $42,$26,$3c,$24,$00,$38,$26,$22,$24,$3c,$00,$24,$16,$30,$1c,$00
                                    +      $1a,$32,$30,$3c,$38,$32,$2c,$2c,$1e,$b8
6e20: d3 08 3a 1e+ L6E20           .bulk   $d3,$08,$3a,$1e,$2c,$1e,$1a,$3c,$00,$2c,$1e,$3c,$3c,$1e,$38,$00
                                    +      $42,$26,$3c,$24,$00,$20,$26,$38,$1e,$00,$18,$3e,$3c,$3c,$32,$b0
6e40: 20 46 24 26+ L6E40           .bulk   $20,$46,$24,$26,$22,$24,$00,$3a,$1a,$32,$38,$1e,$00,$00,$00,$00
                                    +      $00,$00,$02,$02,$82
6e55: 92 5a 1e 30+ L6E55           .bulk   $92,$5a,$1e,$30,$1e,$2e,$46,$00,$26,$30,$00,$38,$16,$30,$22,$9e
6e65: 92 52 2e 32+ L6E65           .bulk   $92,$52,$2e,$32,$3c,$26,$32,$30,$00,$18,$2c,$32,$1a,$2a,$1e,$1c
                                    +      $00,$18,$46,$00,$32,$18,$28,$1e,$1a,$bc
6e7f: e4 18 22 16+ L6E7F           .bulk   $e4,$18,$22,$16,$2e,$1e,$00,$32,$40,$1e,$b8
6e8a: de 00 34 38+ L6E8A           .bulk   $de,$00,$34,$38,$1e,$3a,$3a,$00,$3a,$3c,$16,$38,$bc
6e97: 20 50 3a 1a+ L6E97           .bulk   $20,$50,$3a,$1a,$32,$38,$1e,$00,$00,$00,$00,$00,$02,$02,$82
6ea6: e4 28 24 26+ L6EA6           .bulk   $e4,$28,$24,$26,$22,$24,$00,$3a,$1a,$32,$38,$1e,$ba
6eb3: 00 00 02 02+ L6EB3           .bulk   $00,$00,$02,$02,$02,$80
6eb9: c0 18 22 38+ L6EB9           .bulk   $c0,$18,$22,$38,$1e,$16,$3c,$00,$3a,$1a,$32,$38,$9e
6ec6: ce 00 04 00+ L6EC6           .bulk   $ce,$00,$04,$00,$00,$00,$00,$00,$00,$00,$06,$00,$00,$00,$00,$00
                                    +      $ba
6ed7: ce 00 04 00+ L6ED7           .bulk   $ce,$00,$04,$00,$00,$00,$00,$00,$00,$00,$84
6ee2: ce 00 06 00+ L6EE2           .bulk   $ce,$00,$06,$00,$00,$00,$00,$00,$3a,$00,$84
6eed: ce 00 00 00+ L6EED           .bulk   $ce,$00,$00,$00,$1a,$32,$26,$30,$00,$00,$00,$00,$34,$2c,$16,$c6
6efd: dc ea 26 30+ L6EFD           .bulk   $dc,$ea,$26,$30,$3a,$1e,$38,$3c,$00,$1a,$32,$26,$b0
6f0a: a9 ba 18 32+ L6F0A           .bulk   $a9,$ba,$18,$32,$30,$3e,$3a,$00,$3c,$16,$30,$2a,$00,$16,$3c,$80
6f1a: 00 00 02 02+ L6F1A           .bulk   $00,$00,$02,$02,$02,$00,$16,$30,$1c,$00,$04,$02,$02,$02,$02,$82
6f2a: d6 c4 4e 50+ L6F2A           .bulk   $d6,$c4,$4e,$50,$00,$00,$16,$3c,$16,$38,$26,$00,$04,$14,$12,$82
                   ; 
                   ; French strings.
                   ; 
6f3a: 6a 6f        str_addr_fr     .dd2    L6F6A             ;-480,+296 'ENNEMI '
6f3c: 73 6f                        .dd2    L6F73             ;(rel) 'A GAUCHE'
6f3e: 7d 6f                        .dd2    L6F7D             ;(rel) 'A DROITE'
6f40: 87 6f                        .dd2    L6F87             ;(rel) 'DERRIERE'
6f42: 91 6f                        .dd2    L6F91             ;+28,+104 'COMPOSER VOS INITIALES'
6f44: a9 6f                        .dd2    L6FA9             ;-304,+64 'CHOISISSEZ VOTRE LETTRE AVEC LE CONTROLEUR DE DROITE'
6f46: df 6f                        .dd2    L6FDF             ;-260,+32 'APPUYER SUR FEU POUR SELECTION DE LETTRE'
6f48: 09 70                        .dd2    L7009             ;+128,+280 'MEILLEUR SCORE      000'
6f4a: 22 70                        .dd2    L7022             ;-480,+360 'ENNEMI A PORTEE'
6f4c: 33 70                        .dd2    L7033             ;-480,+328 'ATTENTION OBSTACLE'
6f4e: 47 70                        .dd2    L7047             ;-128,+96 'FIN DE PARTIE'
6f50: 56 70                        .dd2    L7056             ;-200,+0 'APPUYEZ SUR START'
6f52: 97 6e                        .dd2    L6E97             ;+128,+320 'SCORE     000'
6f54: 69 70                        .dd2    L7069             ;-144,+160 'MEILLEURS SCORE'
6f56: b3 6e                        .dd2    L6EB3             ;(rel) '000 '
6f58: 7a 70                        .dd2    L707A             ;-256,+96 'SCORE ELEVE'
6f5a: 87 70                        .dd2    L7087             ;-232,+0 '1        2       S'
6f5c: 9b 70                        .dd2    L709B             ;-232,+0 '1        1'
6f5e: a7 70                        .dd2    L70A7             ;-232,+0 '2      S 1'
6f60: b3 70                        .dd2    L70B3             ;-232,+0 '  PIECE    JOUEUR'
6f62: c6 70                        .dd2    L70C6             ;-216,-88 'INTRODUIRE LES PIECES'
6f64: dd 70                        .dd2    L70DD             ;-264,-280 'BONUS A '
6f66: e7 70                        .dd2    L70E7             ;(rel) '000 ET 100000'
6f68: 2a 6f                        .dd2    L6F2A             ;-168,-240 '(C)(P)  ATARI 1980'
6f6a: 88 4a 1e 30+ L6F6A           .bulk   $88,$4a,$1e,$30,$30,$1e,$2e,$26,$80
6f73: 00 00 16 00+ L6F73           .bulk   $00,$00,$16,$00,$22,$16,$3e,$1a,$24,$9e
6f7d: 00 00 16 00+ L6F7D           .bulk   $00,$00,$16,$00,$1c,$38,$32,$26,$3c,$9e
6f87: 00 00 1c 1e+ L6F87           .bulk   $00,$00,$1c,$1e,$38,$38,$26,$1e,$38,$9e
6f91: 07 1a 1a 32+ L6F91           .bulk   $07,$1a,$1a,$32,$2e,$34,$32,$3a,$1e,$38,$00,$40,$32,$3a,$00,$26
                                    +      $30,$26,$3c,$26,$16,$2c,$1e,$ba
6fa9: b4 10 1a 24+ L6FA9           .bulk   $b4,$10,$1a,$24,$32,$26,$3a,$26,$3a,$3a,$1e,$48,$00,$40,$32,$3c
                                    +      $38,$1e,$00,$2c,$1e,$3c,$3c,$38,$1e,$00,$16,$40,$1e,$1a,$00,$2c
                                    +      $1e,$00,$1a,$32,$30,$3c,$38,$32,$2c,$1e,$3e,$38,$00,$1c,$1e,$00
                                    +      $1c,$38,$32,$26,$3c,$9e
6fdf: bf 08 16 34+ L6FDF           .bulk   $bf,$08,$16,$34,$34,$3e,$46,$1e,$38,$00,$3a,$3e,$38,$00,$20,$1e
                                    +      $3e,$00,$34,$32,$3e,$38,$00,$3a,$1e,$2c,$1e,$1a,$3c,$26,$32,$30
                                    +      $00,$1c,$1e,$00,$2c,$1e,$3c,$3c,$38,$9e
7009: 20 46 2e 1e+ L7009           .bulk   $20,$46,$2e,$1e,$26,$2c,$2c,$1e,$3e,$38,$00,$3a,$1a,$32,$38,$1e
                                    +      $00,$00,$00,$00,$00,$00,$02,$02,$82
7022: 88 5a 1e 30+ L7022           .bulk   $88,$5a,$1e,$30,$30,$1e,$2e,$26,$00,$16,$00,$34,$32,$38,$3c,$1e
                                    +      $9e
7033: 88 52 16 3c+ L7033           .bulk   $88,$52,$16,$3c,$3c,$1e,$30,$3c,$26,$32,$30,$00,$32,$18,$3a,$3c
                                    +      $16,$1a,$2c,$9e
7047: e0 18 20 26+ L7047           .bulk   $e0,$18,$20,$26,$30,$00,$1c,$1e,$00,$34,$16,$38,$3c,$26,$9e
7056: ce 00 16 34+ L7056           .bulk   $ce,$00,$16,$34,$34,$3e,$46,$1e,$48,$00,$3a,$3e,$38,$00,$3a,$3c
                                    +      $16,$38,$bc
7069: dc 28 2e 1e+ L7069           .bulk   $dc,$28,$2e,$1e,$26,$2c,$2c,$1e,$3e,$38,$3a,$00,$3a,$1a,$32,$38
                                    +      $9e
707a: c0 18 3a 1a+ L707A           .bulk   $c0,$18,$3a,$1a,$32,$38,$1e,$00,$1e,$2c,$1e,$40,$9e
7087: c6 00 04 00+ L7087           .bulk   $c6,$00,$04,$00,$00,$00,$00,$00,$00,$00,$00,$06,$00,$00,$00,$00
                                    +      $00,$00,$00,$ba
709b: c6 00 04 00+ L709B           .bulk   $c6,$00,$04,$00,$00,$00,$00,$00,$00,$00,$00,$84
70a7: c6 00 06 00+ L70A7           .bulk   $c6,$00,$06,$00,$00,$00,$00,$00,$00,$3a,$00,$84
70b3: c6 00 00 00+ L70B3           .bulk   $c6,$00,$00,$00,$34,$26,$1e,$1a,$1e,$00,$00,$00,$00,$28,$32,$3e
                                    +      $1e,$3e,$b8
70c6: ca ea 26 30+ L70C6           .bulk   $ca,$ea,$26,$30,$3c,$38,$32,$1c,$3e,$26,$38,$1e,$00,$2c,$1e,$3a
                                    +      $00,$34,$26,$1e,$1a,$1e,$ba
70dd: be ba 18 32+ L70DD           .bulk   $be,$ba,$18,$32,$30,$3e,$3a,$00,$16,$80
70e7: 00 00 02 02+ L70E7           .bulk   $00,$00,$02,$02,$02,$00,$1e,$3c,$00,$04,$02,$02,$02,$02,$82
                   ; 
                   ; Spanish strings.
                   ; 
70f6: 26 71        str_addr_es     .dd2    L7126             ;-492,+296 'ENEMIGO MUY '
70f8: 34 71                        .dd2    L7134             ;(rel) 'A LA IZQUIERDA'
70fa: 44 71                        .dd2    L7144             ;(rel) 'A LA DERECHA'
70fc: 52 71                        .dd2    L7152             ;(rel) 'ATRAS'
70fe: 59 71                        .dd2    L7159             ;+8,+104 'GRABE SUS INICIALES'
7100: 6e 71                        .dd2    L716E             ;-248,+64 'SELECCIONE LETRA CON CONTROL DERECHO'
7102: 94 71                        .dd2    L7194             ;-220,+32 'GRABELA CON EL BOTON DE DISPARO'
7104: b5 71                        .dd2    L71B5             ;+128,+280 'PUNTOS MAYORES     000'
7106: cd 71                        .dd2    L71CD             ;-492,+360 'ENEMIGO EN RANGO'
7108: df 71                        .dd2    L71DF             ;-492,+328 'MOVIMIENTO BLOQUEADO POR OBJETO'
710a: 00 72                        .dd2    L7200             ;-160,+96 'JUEGO TERMINADO'
710c: 11 72                        .dd2    L7211             ;-136,+0 'PULSAR START'
710e: 1f 72                        .dd2    L721F             ;+128,+320 'PUNTOS     000'
7110: 2f 72                        .dd2    L722F             ;-56,+160 'RECORDS'
7112: b3 6e                        .dd2    L6EB3             ;(rel) '000 '
7114: 38 72                        .dd2    L7238             ;-352,+96 'PUNTOS GRANDES'
7116: 87 70                        .dd2    L7087             ;-232,+0 '1        2       S'
7118: 9b 70                        .dd2    L709B             ;-232,+0 '1        1'
711a: a7 70                        .dd2    L70A7             ;-232,+0 '2      S 1'
711c: 48 72                        .dd2    L7248             ;-232,+0 '  FICHA     JEUGO'
711e: 5b 72                        .dd2    L725B             ;-160,-88 'INSERTE FICHAS'
7120: 6b 72                        .dd2    L726B             ;-312,-280 'VIDA EXTRA A '
7122: 7a 72                        .dd2    L727A             ;(rel) '000 Y 100000'
7124: 2a 6f                        .dd2    L6F2A             ;-168,-240 '(C)(P)  ATARI 1980'
7126: 85 4a 1e 30+ L7126           .bulk   $85,$4a,$1e,$30,$1e,$2e,$26,$22,$32,$00,$2e,$3e,$46,$80
7134: 00 00 16 00+ L7134           .bulk   $00,$00,$16,$00,$2c,$16,$00,$26,$48,$36,$3e,$26,$1e,$38,$1c,$96
7144: 00 00 16 00+ L7144           .bulk   $00,$00,$16,$00,$2c,$16,$00,$1c,$1e,$38,$1e,$1a,$24,$96
7152: 00 00 16 3c+ L7152           .bulk   $00,$00,$16,$3c,$38,$16,$ba
7159: 02 1a 22 38+ L7159           .bulk   $02,$1a,$22,$38,$16,$18,$1e,$00,$3a,$3e,$3a,$00,$26,$30,$26,$1a
                                    +      $26,$16,$2c,$1e,$ba
716e: c2 10 3a 1e+ L716E           .bulk   $c2,$10,$3a,$1e,$2c,$1e,$1a,$1a,$26,$32,$30,$1e,$00,$2c,$1e,$3c
                                    +      $38,$16,$00,$1a,$32,$30,$00,$1a,$32,$30,$3c,$38,$32,$2c,$00,$1c
                                    +      $1e,$38,$1e,$1a,$24,$b2
7194: c9 08 22 38+ L7194           .bulk   $c9,$08,$22,$38,$16,$18,$1e,$2c,$16,$00,$1a,$32,$30,$00,$1e,$2c
                                    +      $00,$18,$32,$3c,$32,$30,$00,$1c,$1e,$00,$1c,$26,$3a,$34,$16,$38
                                    +      $b2
71b5: 20 46 34 3e+ L71B5           .bulk   $20,$46,$34,$3e,$30,$3c,$32,$3a,$00,$2e,$16,$46,$32,$38,$1e,$3a
                                    +      $00,$00,$00,$00,$00,$02,$02,$82
71cd: 85 5a 1e 30+ L71CD           .bulk   $85,$5a,$1e,$30,$1e,$2e,$26,$22,$32,$00,$1e,$30,$00,$38,$16,$30
                                    +      $22,$b2
71df: 85 52 2e 32+ L71DF           .bulk   $85,$52,$2e,$32,$40,$26,$2e,$26,$1e,$30,$3c,$32,$00,$18,$2c,$32
                                    +      $36,$3e,$1e,$16,$1c,$32,$00,$34,$32,$38,$00,$32,$18,$28,$1e,$3c
                                    +      $b2
7200: d8 18 28 3e+ L7200           .bulk   $d8,$18,$28,$3e,$1e,$22,$32,$00,$3c,$1e,$38,$2e,$26,$30,$16,$1c
                                    +      $b2
7211: de 00 34 3e+ L7211           .bulk   $de,$00,$34,$3e,$2c,$3a,$16,$38,$00,$3a,$3c,$16,$38,$bc
721f: 20 50 34 3e+ L721F           .bulk   $20,$50,$34,$3e,$30,$3c,$32,$3a,$00,$00,$00,$00,$00,$02,$02,$82
722f: f2 28 38 1e+ L722F           .bulk   $f2,$28,$38,$1e,$1a,$32,$38,$1c,$ba
7238: a8 18 34 3e+ L7238           .bulk   $a8,$18,$34,$3e,$30,$3c,$32,$3a,$00,$22,$38,$16,$30,$1c,$1e,$ba
7248: c6 00 00 00+ L7248           .bulk   $c6,$00,$00,$00,$20,$26,$1a,$24,$16,$00,$00,$00,$00,$00,$28,$1e
                                    +      $3e,$22,$b2
725b: d8 ea 26 30+ L725B           .bulk   $d8,$ea,$26,$30,$3a,$1e,$38,$3c,$1e,$00,$20,$26,$1a,$24,$16,$ba
726b: b2 ba 40 26+ L726B           .bulk   $b2,$ba,$40,$26,$1c,$16,$00,$1e,$44,$3c,$38,$16,$00,$16,$80
727a: 00 00 02 02+ L727A           .bulk   $00,$00,$02,$02,$02,$00,$46,$00,$04,$02,$02,$02,$02,$82
                   ; 
                   ; German strings.
                   ; 
7288: b8 72        str_addr_de     .dd2    L72B8             ;-492,+296 'GEGNER VON '
728a: c5 72                        .dd2    L72C5             ;(rel) 'LINKS'
728c: cc 72                        .dd2    L72CC             ;(rel) 'RECHTS'
728e: d4 72                        .dd2    L72D4             ;(rel) 'HINTEN'
7290: dc 72                        .dd2    L72DC             ;+128,+104 'GEBEN SIE IHRE INITIALEN EIN'
7292: fa 72                        .dd2    L72FA             ;-312,+64 'WAEHLEN SIE DEN BUCHSTABEN MIT DEM RECHTEN STEUERGRIFF'
7294: 32 73                        .dd2    L7332             ;-240,+32 'GEBEN SIE DEN BUCHSTABEN MIT DEM FEUERKNOPF EIN'
7296: 63 73                        .dd2    L7363             ;+128,+280 'BESTE PUNKTZAHL      000'
7298: 7d 73                        .dd2    L737D             ;-492,+360 'GEGNER IM FEUERBEREICH'
729a: 95 73                        .dd2    L7395             ;-492,+328 'BEWEGUNG BLOCKIERT'
729c: a9 73                        .dd2    L73A9             ;-112,+96 'SPIELENDE'
729e: b4 73                        .dd2    L73B4             ;-232,+0 'STARTKNOPF DRUECKEN'
72a0: c9 73                        .dd2    L73C9             ;+128,+320 'PUNKTZAHL    000'
72a2: db 73                        .dd2    L73DB             ;-168,+160 'HOECHSTERGEBNISSE'
72a4: b3 6e                        .dd2    L6EB3             ;(rel) '000 '
72a6: ee 73                        .dd2    L73EE             ;-416,+96 'GROSSARTIGES ERGEBNIS'
72a8: 05 74                        .dd2    L7405             ;-216,+0 '1         2      E'
72aa: 19 74                        .dd2    L7419             ;-216,+0 '1         1'
72ac: 26 74                        .dd2    L7426             ;-216,+0 '2       N 1'
72ae: 33 74                        .dd2    L7433             ;-216,+0 '  MUENZE    SPIEL'
72b0: 46 74                        .dd2    L7446             ;-160,-88 'GELD EINWERFEN'
72b2: 56 74                        .dd2    L7456             ;-292,-280 'BONUS BEI '
72b4: 62 74                        .dd2    L7462             ;(rel) '000 UND 100000'
72b6: 2a 6f                        .dd2    L6F2A             ;-168,-240 '(C)(P)  ATARI 1980'
72b8: 85 4a 22 1e+ L72B8           .bulk   $85,$4a,$22,$1e,$22,$30,$1e,$38,$00,$40,$32,$30,$80
72c5: 00 00 2c 26+ L72C5           .bulk   $00,$00,$2c,$26,$30,$2a,$ba
72cc: 00 00 38 1e+ L72CC           .bulk   $00,$00,$38,$1e,$1a,$24,$3c,$ba
72d4: 00 00 24 26+ L72D4           .bulk   $00,$00,$24,$26,$30,$3c,$1e,$b0
72dc: 20 1a 22 1e+ L72DC           .bulk   $20,$1a,$22,$1e,$18,$1e,$30,$00,$3a,$26,$1e,$00,$26,$24,$38,$1e
                                    +      $00,$26,$30,$26,$3c,$26,$16,$2c,$1e,$30,$00,$1e,$26,$b0
72fa: b2 10 42 16+ L72FA           .bulk   $b2,$10,$42,$16,$1e,$24,$2c,$1e,$30,$00,$3a,$26,$1e,$00,$1c,$1e
                                    +      $30,$00,$18,$3e,$1a,$24,$3a,$3c,$16,$18,$1e,$30,$00,$2e,$26,$3c
                                    +      $00,$1c,$1e,$2e,$00,$38,$1e,$1a,$24,$3c,$1e,$30,$00,$3a,$3c,$1e
                                    +      $3e,$1e,$38,$22,$38,$26,$20,$a0
7332: c4 08 22 1e+ L7332           .bulk   $c4,$08,$22,$1e,$18,$1e,$30,$00,$3a,$26,$1e,$00,$1c,$1e,$30,$00
                                    +      $18,$3e,$1a,$24,$3a,$3c,$16,$18,$1e,$30,$00,$2e,$26,$3c,$00,$1c
                                    +      $1e,$2e,$00,$20,$1e,$3e,$1e,$38,$2a,$30,$32,$34,$20,$00,$1e,$26
                                    +      $b0
7363: 20 46 18 1e+ L7363           .bulk   $20,$46,$18,$1e,$3a,$3c,$1e,$00,$34,$3e,$30,$2a,$3c,$48,$16,$24
                                    +      $2c,$00,$00,$00,$00,$00,$00,$02,$02,$82
737d: 85 5a 22 1e+ L737D           .bulk   $85,$5a,$22,$1e,$22,$30,$1e,$38,$00,$26,$2e,$00,$20,$1e,$3e,$1e
                                    +      $38,$18,$1e,$38,$1e,$26,$1a,$a4
7395: 85 52 18 1e+ L7395           .bulk   $85,$52,$18,$1e,$42,$1e,$22,$3e,$30,$22,$00,$18,$2c,$32,$1a,$2a
                                    +      $26,$1e,$38,$bc
73a9: e4 18 3a 34+ L73A9           .bulk   $e4,$18,$3a,$34,$26,$1e,$2c,$1e,$30,$1c,$9e
73b4: c6 00 3a 3c+ L73B4           .bulk   $c6,$00,$3a,$3c,$16,$38,$3c,$2a,$30,$32,$34,$20,$00,$1c,$38,$3e
                                    +      $1e,$1a,$2a,$1e,$b0
73c9: 20 50 34 3e+ L73C9           .bulk   $20,$50,$34,$3e,$30,$2a,$3c,$48,$16,$24,$2c,$00,$00,$00,$00,$02
                                    +      $02,$82
73db: d6 28 24 32+ L73DB           .bulk   $d6,$28,$24,$32,$1e,$1a,$24,$3a,$3c,$1e,$38,$22,$1e,$18,$30,$26
                                    +      $3a,$3a,$9e
73ee: 98 18 22 38+ L73EE           .bulk   $98,$18,$22,$38,$32,$3a,$3a,$16,$38,$3c,$26,$22,$1e,$3a,$00,$1e
                                    +      $38,$22,$1e,$18,$30,$26,$ba
7405: ca 00 04 00+ L7405           .bulk   $ca,$00,$04,$00,$00,$00,$00,$00,$00,$00,$00,$00,$06,$00,$00,$00
                                    +      $00,$00,$00,$9e
7419: ca 00 04 00+ L7419           .bulk   $ca,$00,$04,$00,$00,$00,$00,$00,$00,$00,$00,$00,$84
7426: ca 00 06 00+ L7426           .bulk   $ca,$00,$06,$00,$00,$00,$00,$00,$00,$00,$30,$00,$84
7433: ca 00 00 00+ L7433           .bulk   $ca,$00,$00,$00,$2e,$3e,$1e,$30,$48,$1e,$00,$00,$00,$00,$3a,$34
                                    +      $26,$1e,$ac
7446: d8 ea 22 1e+ L7446           .bulk   $d8,$ea,$22,$1e,$2c,$1c,$00,$1e,$26,$30,$42,$1e,$38,$20,$1e,$b0
7456: b7 ba 18 32+ L7456           .bulk   $b7,$ba,$18,$32,$30,$3e,$3a,$00,$18,$1e,$26,$80
7462: 00 00 02 02+ L7462           .bulk   $00,$00,$02,$02,$02,$00,$3e,$30,$1c,$00,$04,$02,$02,$02,$02,$82
                   ; 
                   ; Addresses of shape draw commands.
                   ; 
                   ; Draw commands use a byte code where each byte has the form VVVVVCCC.  The top
                   ; 5 bits are the vertex index (0-31), the bottom 3 are the command (0-7).
                   ; 
                   ; The commands are:
                   ; 
                   ;   0 - draw point at vertex
                   ;   1 - set intensity with vertex index value (overridden for saucer/logo)
                   ;   2 - move without drawing
                   ;   3 - move to center of screen, then move without drawing
                   ;   4 - draw to vertex with intensity=1 (h/w uses intensity from STAT)
                   ;   5 - draw scaled AVG commands from $347a (for projectile explosion)
                   ;   6 - no-op
                   ; 
                   ; The list is terminated with $ff.  See the implementation at $5c5c.
                   ; 
                   shape_code_addrs
7472: cb 74                        .dd2    shp_c_pyr         ;$00
7474: d7 74                        .dd2    shp_c_box         ;$01
7476: e9 74                        .dd2    shp_c_tank1       ;$02
7478: 19 75                        .dd2    shp_c_projectile  ;$03
747a: 25 75                        .dd2    shp_c_tread       ;$04
747c: 25 75                        .dd2    shp_c_tread       ;$05
747e: 25 75                        .dd2    shp_c_tread       ;$06
7480: 25 75                        .dd2    shp_c_tread       ;$07
7482: 25 75                        .dd2    shp_c_tread       ;$08
7484: 25 75                        .dd2    shp_c_tread       ;$09
7486: 25 75                        .dd2    shp_c_tread       ;$0a
7488: 25 75                        .dd2    shp_c_tread       ;$0b
748a: cb 74                        .dd2    shp_c_pyr         ;$0c
748c: 2d 75                        .dd2    shp_c_radar       ;$0d
748e: 3b 75                        .dd2    shp_c_prj_explos  ;$0e
7490: d7 74                        .dd2    shp_c_box         ;$0f
7492: 5c 75        strange_75      .dd2    shp_c_chunk0      ;$10
7494: 6a 75                        .dd2    shp_c_chunk1      ;$11
7496: 3e 75                        .dd2    shp_c_chunk2      ;$12
7498: 2d 75                        .dd2    shp_c_radar       ;$13
749a: 6a 75                        .dd2    shp_c_chunk1      ;$14
749c: 5c 75                        .dd2    shp_c_chunk0      ;$15
749e: 7c 75                        .dd2    shp_c_missile     ;$16
74a0: 2f 76                        .dd2    shp_c_ba          ;$17
74a2: 6a 75                        .dd2    shp_c_chunk1      ;$18
74a4: e6 75                        .dd2    shp_c_chunk4      ;$19
74a6: 5c 75                        .dd2    shp_c_chunk0      ;$1a
74a8: dc 75                        .dd2    shp_c_chunk5      ;$1b
74aa: 5c 75                        .dd2    shp_c_chunk0      ;$1c
74ac: e6 75                        .dd2    shp_c_chunk4      ;$1d
74ae: 49 76                        .dd2    shp_c_ttle        ;$1e
74b0: 61 76                        .dd2    shp_c_zone        ;$1f
74b2: b5 75                        .dd2    shp_c_saucer      ;$20
74b4: ff 75                        .dd2    shp_c_tank2       ;$21
74b6: 00 00                        .dd2    $0000             ;$22
74b8: 00 00                        .dd2    $0000             ;$23
74ba: f4 75                        .dd2    shp_c_spatter     ;$24
74bc: f4 75                        .dd2    shp_c_spatter     ;$25
74be: f4 75                        .dd2    shp_c_spatter     ;$26
74c0: f4 75                        .dd2    shp_c_spatter     ;$27
74c2: f4 75                        .dd2    shp_c_spatter     ;$28
74c4: f4 75                        .dd2    shp_c_spatter     ;$29
74c6: f4 75                        .dd2    shp_c_spatter     ;$2a
74c8: f4 75                        .dd2    shp_c_spatter     ;$2b
74ca: dd                           .dd1    $dd               ;(checksum adj)
74cb: 03 a1 24 0c+ shp_c_pyr       .bulk   $03,$a1,$24,$0c,$04,$1c,$24,$14,$1c,$12,$0c,$ff ;used for narrow & wide pyramid
74d7: 03 a1 0c 14+ shp_c_box       .bulk   $03,$a1,$0c,$14,$1c,$04,$24,$2c,$34,$3c,$24,$2a,$0c,$12,$34,$3a ;used for tall & short box
                                    +      $1c,$ff
74e9: bb a1 b4 62+ shp_c_tank1     .bulk   $bb,$a1,$b4,$62,$6c,$72,$a4,$94,$7c,$74,$8c,$84,$9c,$ac,$8c,$7a
                                    +      $84,$9a,$94,$a2,$ac,$1b,$04,$24,$3c,$34,$14,$1c,$3c,$5c,$54,$34
                                    +      $2c,$4c,$54,$6c,$4c,$44,$5c,$64,$44,$24,$2c,$0c,$14,$0a,$04,$ff
                   shp_c_projectile
7519: 03 e1 24 0c+                 .bulk   $03,$e1,$24,$0c,$04,$1c,$24,$14,$1c,$12,$0c,$ff
7525: 03 81 0c 12+ shp_c_tread     .bulk   $03,$81,$0c,$12,$1c,$22,$2c,$ff ;used for all 8 tread objects
752d: 03 a1 0c 14+ shp_c_radar     .bulk   $03,$a1,$0c,$14,$1c,$04,$24,$2c,$34,$3c,$24,$3a,$1c,$ff
                   shp_c_prj_explos
753b: 03 05 ff                     .bulk   $03,$05,$ff
753e: 03 a1 0c 14+ shp_c_chunk2    .bulk   $03,$a1,$0c,$14,$1c,$04,$24,$2c,$0c,$2a,$14,$1a,$24,$32,$64,$54
                                    +      $3c,$34,$4c,$44,$5c,$6c,$4c,$3a,$44,$5a,$54,$62,$6c,$ff
755c: 03 a1 1c 2c+ shp_c_chunk0    .bulk   $03,$a1,$1c,$2c,$14,$04,$0c,$14,$2c,$24,$0c,$22,$1c,$ff
756a: 03 a1 0c 14+ shp_c_chunk1    .bulk   $03,$a1,$0c,$14,$1c,$04,$24,$34,$3c,$2c,$24,$2a,$0c,$3a,$14,$1a
                                    +      $34,$ff
757c: 6b a1 64 34+ shp_c_missile   .bulk   $6b,$a1,$64,$34,$04,$0c,$3c,$44,$4c,$54,$5c,$34,$3c,$64,$44,$14
                                    +      $1c,$4c,$64,$54,$24,$2c,$5c,$64,$c3,$e1,$bc,$b4,$c4,$cc,$bc,$ca
                                    +      $b4,$0a,$a1,$14,$1a,$24,$2a,$04,$93,$9c,$a4,$ac,$94,$74,$7c,$84
                                    +      $8c,$74,$7a,$9c,$a2,$84,$8a,$ac,$ff
75b5: 83 a1 44 4c+ shp_c_saucer    .bulk   $83,$a1,$44,$4c,$84,$54,$5c,$84,$64,$6c,$84,$74,$7c,$84,$03,$3c
                                    +      $7c,$44,$04,$0c,$4c,$54,$14,$1c,$5c,$64,$24,$2c,$6c,$74,$34,$3c
                                    +      $32,$2c,$22,$1c,$12,$0c,$ff
75dc: 03 a1 14 0c+ shp_c_chunk5    .bulk   $03,$a1,$14,$0c,$1c,$04,$0c,$12,$1c,$ff
75e6: 0b a1 14 1c+ shp_c_chunk4    .bulk   $0b,$a1,$14,$1c,$3c,$34,$2c,$24,$04,$0c,$2c,$32,$14,$ff
75f4: 3b a1 00 08+ shp_c_spatter   .bulk   $3b,$a1,$00,$08,$10,$18,$20,$28,$30,$38,$ff ;used for all 8 spatter objects
75ff: 03 a1 0c 24+ shp_c_tank2     .bulk   $03,$a1,$0c,$24,$04,$1c,$14,$2c,$1c,$12,$0c,$22,$2c,$4b,$54,$34
                                    +      $74,$6c,$4c,$44,$3c,$34,$5c,$64,$44,$62,$6c,$72,$5c,$9b,$b4,$ac
                                    +      $a4,$84,$7c,$94,$8c,$84,$7a,$9c,$b2,$94,$8a,$ac,$ba,$e1,$c4,$ff
762f: 03 f1 0c 14+ shp_c_ba        .bulk   $03,$f1,$0c,$14,$1c,$24,$2c,$34,$04,$3a,$44,$4c,$54,$5c,$3c,$62
                                    +      $6c,$74,$7c,$64,$82,$8c,$94,$9c,$84,$ff
7649: 03 0c 14 1c+ shp_c_ttle      .bulk   $03,$0c,$14,$1c,$24,$2c,$34,$3c,$44,$4c,$54,$5c,$64,$6c,$74,$7c
                                    +      $3c,$84,$8c,$94,$9c,$a4,$04,$ff
7661: 0b 04 2c 24+ shp_c_zone      .bulk   $0b,$04,$2c,$24,$1c,$14,$0c,$1c,$3c,$34,$0c,$4a,$44,$5c,$54,$4c
                                    +      $72,$b4,$bc,$c4,$64,$6c,$74,$7c,$84,$8c,$94,$9c,$a4,$ac,$b4,$ff
                   ; 
                   ; Obstacle coordinates.  There are 21 of them.
                   ; 
                   ; See also the type/facing data at $3fcc and the projectile collision diameters
                   ; at $6139.
                   ; 
7681: 00 20        obstacle_z_pos  .dd2    $2000
7683: 00 00                        .dd2    $0000
7685: 00 00                        .dd2    $0000
7687: 00 40                        .dd2    $4000
7689: 00 80                        .dd2    $8000
768b: 00 80                        .dd2    $8000
768d: 00 80                        .dd2    $8000
768f: 00 40                        .dd2    $4000
7691: 00 30                        .dd2    $3000
7693: 00 c0                        .dd2    $c000
7695: 00 f7                        .dd2    $f700
7697: 00 c8                        .dd2    $c800
7699: 00 d8                        .dd2    $d800
769b: 00 94                        .dd2    $9400
769d: 00 98                        .dd2    $9800
769f: 00 e8                        .dd2    $e800
76a1: 00 70                        .dd2    $7000
76a3: 00 78                        .dd2    $7800
76a5: 00 40                        .dd2    $4000
76a7: 00 24                        .dd2    $2400
76a9: 00 2c                        .dd2    $2c00
76ab: 00 20        obstacle_x_pos  .dd2    $2000
76ad: 00 40                        .dd2    $4000
76af: 00 80                        .dd2    $8000
76b1: 00 80                        .dd2    $8000
76b3: 00 80                        .dd2    $8000
76b5: 00 40                        .dd2    $4000
76b7: 00 00                        .dd2    $0000
76b9: 00 00                        .dd2    $0000
76bb: 00 50                        .dd2    $5000
76bd: 00 18                        .dd2    $1800
76bf: 00 44                        .dd2    $4400
76c1: 00 40                        .dd2    $4000
76c3: 00 8c                        .dd2    $8c00
76c5: 00 0c                        .dd2    $0c00
76c7: 00 e8                        .dd2    $e800
76c9: 00 e4                        .dd2    $e400
76cb: 00 9c                        .dd2    $9c00
76cd: 00 cc                        .dd2    $cc00
76cf: 00 b4                        .dd2    $b400
76d1: 00 bc                        .dd2    $bc00
76d3: 00 f4                        .dd2    $f400
                   ; 
                   ; Battlezone logo.  This is split into 3 pieces because the way meshes are
                   ; defined prevents us from having more than 32 vertices per object.  The logo
                   ; has 61, so we could fit it into two, but it's a bit easier for the humans if
                   ; we split it at a natural boundary.
                   ; 
76d5: 79           shp_v_ba        .dd1    $79               ;shape $17
76d6: e0 00 00 14+                 .bulk   $e0,$00,$00,$14,$20,$00
76dc: e0 00 00 0f+                 .bulk   $e0,$00,$00,$0f,$20,$00
76e2: a0 02 80 0c+                 .bulk   $a0,$02,$80,$0c,$58,$00
76e8: 60 04 c0 0d+                 .bulk   $60,$04,$c0,$0d,$90,$00
76ee: 40 06 80 0c+                 .bulk   $40,$06,$80,$0c,$c8,$00
76f4: 00 08 00 0f+                 .bulk   $00,$08,$00,$0f,$00,$01
76fa: 00 08 00 14+                 .bulk   $00,$08,$00,$14,$00,$01
7700: a0 02 80 11+                 .bulk   $a0,$02,$80,$11,$58,$00
7706: a0 02 40 10+                 .bulk   $a0,$02,$40,$10,$58,$00
770c: 60 04 80 11+                 .bulk   $60,$04,$80,$11,$90,$00
7712: 40 06 40 10+                 .bulk   $40,$06,$40,$10,$c8,$00
7718: 40 06 80 11+                 .bulk   $40,$06,$80,$11,$c8,$00
771e: e0 00 80 0c+                 .bulk   $e0,$00,$80,$0c,$20,$00
7724: 20 00 c0 08+                 .bulk   $20,$00,$c0,$08,$58,$00
772a: e0 00 00 05+                 .bulk   $e0,$00,$00,$05,$20,$00
7730: 00 08 c0 08+                 .bulk   $00,$08,$c0,$08,$00,$01
7736: 80 03 00 0a+                 .bulk   $80,$03,$00,$0a,$70,$00
773c: 00 04 c0 08+                 .bulk   $00,$04,$c0,$08,$80,$00
7742: 80 03 80 07+                 .bulk   $80,$03,$80,$07,$70,$00
7748: 40 05 c0 08+                 .bulk   $40,$05,$c0,$08,$a8,$00
774e: 7f           shp_v_ttle      .dd1    $7f               ;shape $1e
774f: e0 00 80 02+                 .bulk   $e0,$00,$80,$02,$20,$00
7755: 40 06 40 01+                 .bulk   $40,$06,$40,$01,$c8,$00
775b: 40 06 80 fd+                 .bulk   $40,$06,$80,$fd,$c8,$00
7761: e0 00 40 fc+                 .bulk   $e0,$00,$40,$fc,$20,$00
7767: 40 06 00 fb+                 .bulk   $40,$06,$00,$fb,$c8,$00
776d: 40 06 40 f7+                 .bulk   $40,$06,$40,$f7,$c8,$00
7773: e0 00 40 f7+                 .bulk   $e0,$00,$40,$f7,$20,$00
7779: e0 00 00 f1+                 .bulk   $e0,$00,$00,$f1,$20,$00
777f: c0 01 c0 ea+                 .bulk   $c0,$01,$c0,$ea,$38,$00
7785: a0 02 80 ee+                 .bulk   $a0,$02,$80,$ee,$58,$00
778b: 80 03 80 ee+                 .bulk   $80,$03,$80,$ee,$70,$00
7791: 60 04 00 ec+                 .bulk   $60,$04,$00,$ec,$90,$00
7797: 40 05 80 ee+                 .bulk   $40,$05,$80,$ee,$a8,$00
779d: 40 06 80 ee+                 .bulk   $40,$06,$80,$ee,$c8,$00
77a3: 20 07 c0 ea+                 .bulk   $20,$07,$c0,$ea,$e0,$00
77a9: 00 08 00 f1+                 .bulk   $00,$08,$00,$f1,$00,$01
77af: a0 02 c0 f4+                 .bulk   $a0,$02,$c0,$f4,$58,$00
77b5: 00 08 c0 f4+                 .bulk   $00,$08,$c0,$f4,$00,$01
77bb: 00 08 80 07+                 .bulk   $00,$08,$80,$07,$00,$01
77c1: 40 06 80 07+                 .bulk   $40,$06,$80,$07,$c8,$00
77c7: 40 06 c0 03+                 .bulk   $40,$06,$c0,$03,$c8,$00
77cd: 97           shp_v_zone      .dd1    $97               ;shape $1f
77ce: 00 f8 c0 12+                 .bulk   $00,$f8,$c0,$12,$00,$ff
77d4: 00 f8 c0 08+                 .bulk   $00,$f8,$c0,$08,$00,$ff
77da: c0 f9 c0 0d+                 .bulk   $c0,$f9,$c0,$0d,$38,$ff
77e0: 20 ff c0 08+                 .bulk   $20,$ff,$c0,$08,$e0,$ff
77e6: 20 ff c0 12+                 .bulk   $20,$ff,$c0,$12,$e0,$ff
77ec: 60 fd c0 0d+                 .bulk   $60,$fd,$c0,$0d,$a8,$ff
77f2: 00 f8 40 01+                 .bulk   $00,$f8,$40,$01,$00,$ff
77f8: 20 ff 40 01+                 .bulk   $20,$ff,$40,$01,$e0,$ff
77fe: c0 f9 40 06+                 .bulk   $c0,$f9,$40,$06,$38,$ff
7804: c0 f9 c0 03+                 .bulk   $c0,$f9,$c0,$03,$38,$ff
780a: 60 fd c0 03+                 .bulk   $60,$fd,$c0,$03,$a8,$ff
7810: 60 fd 40 06+                 .bulk   $60,$fd,$40,$06,$a8,$ff
7816: 00 f8 00 00+                 .bulk   $00,$f8,$00,$00,$00,$ff
781c: a0 fb 80 fd+                 .bulk   $a0,$fb,$80,$fd,$70,$ff
7822: 00 f8 00 f6+                 .bulk   $00,$f8,$00,$f6,$00,$ff
7828: e0 f8 c0 ef+                 .bulk   $e0,$f8,$c0,$ef,$20,$ff
782e: c0 f9 80 f3+                 .bulk   $c0,$f9,$80,$f3,$38,$ff
7834: c0 fa 80 f3+                 .bulk   $c0,$fa,$80,$f3,$58,$ff
783a: a0 fb 00 f1+                 .bulk   $a0,$fb,$00,$f1,$70,$ff
7840: 80 fc 80 f3+                 .bulk   $80,$fc,$80,$f3,$90,$ff
7846: 60 fd 80 f3+                 .bulk   $60,$fd,$80,$f3,$a8,$ff
784c: 40 fe c0 ef+                 .bulk   $40,$fe,$c0,$ef,$c8,$ff
7852: 20 ff 00 f6+                 .bulk   $20,$ff,$00,$f6,$e0,$ff
7858: a0 fb 80 f8+                 .bulk   $a0,$fb,$80,$f8,$70,$ff
785e: 20 ff 00 00+                 .bulk   $20,$ff,$00,$00,$e0,$ff
                   ; 
                   ; For each sound effect, these are the indices into the SFX audio data table for
                   ; the start of the stream that drives AUDF1, AUDC1, AUDF2, and AUDC2.  Zero
                   ; values indicate the register is not used by this effect.
                   ; 
7864: 00 00 15 1b  sfx_indices     .bulk   $00,$00,$15,$1b   ;$01
7868: 2d 57 00 00                  .bulk   $2d,$57,$00,$00   ;$02
786c: 21 27 00 00                  .bulk   $21,$27,$00,$00   ;$04
7870: 00 00 61 67                  .bulk   $00,$00,$61,$67   ;$08
7874: 00 00 01 0f                  .bulk   $00,$00,$01,$0f   ;$10
7878: 85 8f 00 00                  .bulk   $85,$8f,$00,$00   ;$20
787c: 95 a7 00 00                  .bulk   $95,$a7,$00,$00   ;$40
7880: ad d3 d9 d3                  .bulk   $ad,$d3,$d9,$d3   ;$80
7884: 00           sfx_zero_a      .dd1    $00
7885: 00           sfx_zero_b      .dd1    $00
                   ; 
                   ; POKEY audio has four channels, with two 8-bit I/O locations per channel (AUDFn
                   ; and AUDCn).  The sound effects defined by this data are played on channels 1
                   ; and 2.
                   ; 
                   ; The AUDFn setting determines frequency.  Larger value == lower pitch.
                   ; 
                   ; The AUDCn value is NNNFVVVV, where N is a noise / distortion setting, F is
                   ; "forced volume-only output" enable, and V is the volume level.
                   ; 
                   ; In the table below, each chunk has 4 values:
                   ;  +00 initial value
                   ;  +01 duration
                   ;  +03 increment
                   ;  +04 repetition count
                   ; 
                   ; The sound specified by the value is played until the duration reaches zero. 
                   ; If the repetition count is nonzero, the value is increased or decreased by the
                   ; increment, and the duration is reset.  When the repetition count reaches zero,
                   ; the next chunk is loaded.  If the chunk has the value $00, the sequence ends. 
                   ; The counters are updated by the 250Hz NMI.
                   ; 
                   ; Because AUDFn and AUDCn are specified by different chunks, care must be taken
                   ; to ensure the durations run out at the same time.
                   ; 
7886: 00           sfx_audio_data  .dd1    $00
7887: 40 02 ff 18                  .bulk   $40,$02,$ff,$18   ;sound $10 F2 (three-boop new enemy alert)
788b: 40 02 ff 18                  .bulk   $40,$02,$ff,$18   ;($02 * $18) * $03 = $90
788f: 40 02 ff 18                  .bulk   $40,$02,$ff,$18
7893: 00 00                        .bulk   $00,$00
7895: a3 30 00 03                  .bulk   $a3,$30,$00,$03   ;sound $10 C2 ($30 * $03 = $90)
7899: 00 00                        .bulk   $00,$00
789b: 23 10 00 01                  .bulk   $23,$10,$00,$01   ;sound $01 F2 (radar ping)
789f: 00 00                        .bulk   $00,$00
78a1: a3 10 00 01                  .bulk   $a3,$10,$00,$01   ;sound $01 C2
78a5: 00 00                        .bulk   $00,$00
78a7: 10 01 00 20                  .bulk   $10,$01,$00,$20   ;sound $04 F1 (quiet "merp")
78ab: 00 00                        .bulk   $00,$00
78ad: c1 10 ff 02                  .bulk   $c1,$10,$ff,$02   ;sound $04 C1
78b1: 00 00                        .bulk   $00,$00
78b3: c0 01 f6 06                  .bulk   $c0,$01,$f6,$06   ;sound $02 F1 (vehicular collision)
78b7: 84 01 09 0c                  .bulk   $84,$01,$09,$0c
78bb: f0 01 f8 0c                  .bulk   $f0,$01,$f8,$0c
78bf: 90 01 07 0c                  .bulk   $90,$01,$07,$0c
78c3: e4 01 fa 0c                  .bulk   $e4,$01,$fa,$0c
78c7: 9c 01 05 0c                  .bulk   $9c,$01,$05,$0c
78cb: d8 01 fc 0c                  .bulk   $d8,$01,$fc,$0c
78cf: a8 01 03 0c                  .bulk   $a8,$01,$03,$0c
78d3: cc 01 fe 0c                  .bulk   $cc,$01,$fe,$0c
78d7: b4 01 01 0c                  .bulk   $b4,$01,$01,$0c
78db: 00 00                        .bulk   $00,$00
78dd: ab 04 ff 09                  .bulk   $ab,$04,$ff,$09   ;sound $02 C1
78e1: a2 27 ff 02                  .bulk   $a2,$27,$ff,$02
78e5: 00 00                        .bulk   $00,$00
78e7: 10 70 00 02                  .bulk   $10,$70,$00,$02   ;sound $08 F2 (four-beep extra life)
78eb: 00 00                        .bulk   $00,$00
78ed: a2 20 00 01                  .bulk   $a2,$20,$00,$01   ;sound $08 C2
78f1: a0 20 00 01                  .bulk   $a0,$20,$00,$01
78f5: a2 20 00 01                  .bulk   $a2,$20,$00,$01
78f9: a0 20 00 01                  .bulk   $a0,$20,$00,$01
78fd: a2 20 00 01                  .bulk   $a2,$20,$00,$01
7901: a0 20 00 01                  .bulk   $a0,$20,$00,$01
7905: a2 20 00 01                  .bulk   $a2,$20,$00,$01
7909: 00 00                        .bulk   $00,$00
790b: 30 01 fc 0c                  .bulk   $30,$01,$fc,$0c   ;sound $20 F1 (saucer hit)
790f: 30 01 fc 0c                  .bulk   $30,$01,$fc,$0c
7913: 00 00                        .bulk   $00,$00
7915: a3 02 00 0c                  .bulk   $a3,$02,$00,$0c   ;sound $20 C2
7919: 00 00                        .bulk   $00,$00
791b: 40 01 fe 10                  .bulk   $40,$01,$fe,$10   ;sound $40 F1 (saucer alive)
791f: 20 01 02 10                  .bulk   $20,$01,$02,$10
7923: 40 01 fe 10                  .bulk   $40,$01,$fe,$10
7927: 20 01 02 10                  .bulk   $20,$01,$02,$10
792b: 00 00                        .bulk   $00,$00
792d: a1 10 00 04                  .bulk   $a1,$10,$00,$04   ;sound $40 C1
7931: 00 00                        .bulk   $00,$00
7933: d9 30 00 01                  .bulk   $d9,$30,$00,$01   ;sound $80 F1 (1812 Overture)
7937: a2 30 00 01                  .bulk   $a2,$30,$00,$01
793b: 90 30 00 01                  .bulk   $90,$30,$00,$01
793f: 80 30 00 01                  .bulk   $80,$30,$00,$01
7943: 90 30 00 01                  .bulk   $90,$30,$00,$01
7947: a2 30 00 01                  .bulk   $a2,$30,$00,$01
794b: 90 30 00 01                  .bulk   $90,$30,$00,$01
794f: 80 30 00 02                  .bulk   $80,$30,$00,$02
7953: a2 30 00 04                  .bulk   $a2,$30,$00,$04
7957: 00 00                        .bulk   $00,$00
7959: a7 30 00 0d                  .bulk   $a7,$30,$00,$0d   ;sound $80 C1/C2 (13 steps, same as F1/F2)
795d: 00 00                        .bulk   $00,$00
795f: 6c 30 00 01                  .bulk   $6c,$30,$00,$01   ;sound $80 F2
7963: 51 30 00 01                  .bulk   $51,$30,$00,$01
7967: 48 30 00 01                  .bulk   $48,$30,$00,$01
796b: 40 30 00 01                  .bulk   $40,$30,$00,$01
796f: 48 30 00 01                  .bulk   $48,$30,$00,$01
7973: 51 30 00 01                  .bulk   $51,$30,$00,$01
7977: 48 30 00 01                  .bulk   $48,$30,$00,$01
797b: 40 30 00 02                  .bulk   $40,$30,$00,$02
797f: 51 30 00 04                  .bulk   $51,$30,$00,$04
7983: 00 00                        .bulk   $00,$00

                   ; 
                   ; Initiates a sound effect on audio channel 1 and/or 2.
                   ; 
                   ; The effect is chosen by setting a single bit in the A-reg.  Setting A-reg to
                   ; zero causes the value in the stack pointer to be used instead (this does not
                   ; appear to be used by Battlezone).
                   ; 
                   ;   $01: ch1: radar ping
                   ;   $02: ch1: collided with object
                   ;   $04: ch2: quiet "merp"
                   ;   $08: ch2: extra life notification (4 high-pitched beeps)
                   ;   $10: ch2: new enemy alert (three boops)
                   ;   $20: ch1: saucer hit (played in a loop while saucer fades out)
                   ;   $40: ch1: saucer sound (played in a loop while saucer alive)
                   ;   $80: ch1+2: nine notes from 1812 Overture
                   ; 
                   ; On entry:
                   ;   A-reg: effect choice
                   ; 
                   ; On exit:
                   ;   A/X/Y preserved
                   ; 
                   StartSoundEffect
7985: 48                           pha                       ;push A-reg
7986: 8a                           txa
7987: 48                           pha                       ;push X-reg
7988: 98                           tya
7989: 48                           pha                       ;push Y-reg
798a: ba                           tsx                       ;copy stack pointer to X-reg
798b: bd 03 01                     lda     STACK+3,x         ;get A-reg we pushed earlier
798e: f0 06                        beq     :Acc0             ;zero, use stack pointer (not used in BZ?)
                   ; Set X-reg to N, where N is the highest bit set in A-reg.  For example, if you
                   ; pass in $80, X-reg=7.
                   ; 
                   ; (Was this code imported from something else?  Why not just pass value in?)
7990: a2 08                        ldx     #$08              ;start with X-reg=8
7992: 0a           :Loop           asl     A                 ;shift high bit into carry
7993: ca                           dex
7994: 90 fc                        bcc     :Loop             ;loop until we find a set bit
7996: 8a           :Acc0           txa                       ;0-7 (or arbitrary value from SP)
7997: 0a                           asl     A                 ;multiply by 4; clears carry
7998: 0a                           asl     A                 ;0-28 (by 4)
7999: 69 03                        adc     #$03              ;3-31 (by 4) - add 3 to skip entry for $00
799b: a8                           tay
799c: c0 27                        cpy     #39               ;range check
799e: b0 13                        bcs     :Bail
79a0: a2 03                        ldx     #$03              ;four items
79a2: b9 64 78     :CopyLoop       lda     sfx_indices,y     ;get initial index
79a5: f0 08                        beq     :NextItem         ;don't touch entry if table holds zero
79a7: 95 ef                        sta     audio_indices,x   ;copy the index
79a9: a9 01                        lda     #$01
79ab: 95 f7                        sta     audio_dur_ctr,x   ;set counters to 1
79ad: 95 fb                        sta     audio_rep_ctr,x
79af: 88           :NextItem       dey
79b0: ca                           dex
79b1: 10 ef                        bpl     :CopyLoop
79b3: 68           :Bail           pla                       ;restore A/X/Y
79b4: a8                           tay
79b5: 68                           pla
79b6: aa                           tax
79b7: 68                           pla
79b8: 60                           rts

                   ; 
                   ; Updates sound effects on POKEY channels 1/2.  Called every NMI (250Hz).
                   ; 
                   ; There are 4 streams of data, for AUDF1, AUDC1, AUDF2, and AUDC2.  Some things
                   ; use F1/C1, some things use F2/C2, one thing uses both channels.
                   ; 
                   UpdateSoundEffect
79b9: a2 03                        ldx     #$03
79bb: b5 f7        :Loop           lda     audio_dur_ctr,x   ;is this stream alive?
79bd: f0 1d                        beq     :SoundOff         ;no, make sure it's off
79bf: d6 f7                        dec     audio_dur_ctr,x   ;still running, decrement duration
79c1: d0 47                        bne     :SetAudio         ;didn't hit zero, write I/O again
79c3: b4 ef                        ldy     audio_indices,x   ;get index of next thing
79c5: f0 43                        beq     :SetAudio         ;end of list; play sound (next loop will silence it)
79c7: d6 fb                        dec     audio_rep_ctr,x   ;decrement counter
79c9: d0 32                        bne     :RepNonZero       ;more reps to go, branch
                   ; Both counters reached zero.
79cb: b9 86 78                     lda     sfx_audio_data,y  ;get the new value
79ce: 95 f3                        sta     audio_values,x
79d0: b9 87 78                     lda     sfx_audio_data+1,y ;and the new count
79d3: 95 f7                        sta     audio_dur_ctr,x
79d5: d0 17                        bne     :MoreReps
79d7: 95 ef                        sta     audio_indices,x   ;update the index
79d9: 8a                           txa
79da: d0 2e                        bne     :SetAudio         ;branch unless channel index == 0 (bug?)
79dc: ad 85 78     :SoundOff       lda     sfx_zero_b        ;#$00
79df: 95 f3                        sta     audio_values,x
79e1: 9d 20 18                     sta     POKEY_AUDF1,x     ;set frequency or volume to zero
79e4: a9 00                        lda     #$00
79e6: 95 ef                        sta     audio_indices,x   ;zero index
79e8: ca                           dex
79e9: 10 d0                        bpl     :Loop
79eb: 4c 18 7a                     jmp     :Return

79ee: b9 89 78     :MoreReps       lda     sfx_audio_data+3,y ;get step counter from index + 3
79f1: 95 fb                        sta     audio_rep_ctr,x
79f3: b5 ef                        lda     audio_indices,x
79f5: 18                           clc
79f6: 69 04                        adc     #$04
79f8: 95 ef                        sta     audio_indices,x   ;advance index by 4
79fa: 4c 0a 7a                     jmp     :SetAudio

                   ; Duration hit zero, but more reps to go.
79fd: b9 83 78     :RepNonZero     lda     sfx_audio_data-3,y ;reload previous value; need to reference backward
7a00: 95 f7                        sta     audio_dur_ctr,x   ; because we already incremented index
7a02: b5 f3                        lda     audio_values,x
7a04: 18                           clc
7a05: 79 84 78                     adc     sfx_audio_data-2,y
7a08: 95 f3                        sta     audio_values,x
7a0a: b5 f3        :SetAudio       lda     audio_values,x
7a0c: 9d 20 18                     sta     POKEY_AUDF1,x
7a0f: ad 84 78                     lda     sfx_zero_a        ;#$00
7a12: 8d 28 18                     sta     POKEY_AUDCTL      ;configure clocks, individual channels, etc.
7a15: ca                           dex                       ; (why is this done inside the loop?)
7a16: 10 a3                        bpl     :Loop
7a18: 60           :Return         rts

                   ; 
                   ; Appends a VRTS instruction to the command list.  Unused.
                   ; 
7a19: a9 c0        unref_7a19      lda     #$c0              ;$cxxx = VRTS
7a1b: d0 05                        bne     VgFinish1         ;(always)

                   ; 
                   ; Appends CNTR and HALT instructions to the command list.  Call when the command
                   ; list is finished for this frame.
                   ; 
7a1d: 20 6a 7a     VgFinishList    jsr     VgCenter          ;center the beam
7a20: a9 20                        lda     #$20              ;$2xxx = VHALT
7a22: a0 00        VgFinish1       ldy     #$00
7a24: 91 02                        sta     (vg_cmd_ptr),y    ;write the first byte
7a26: 4c c7 7a                     jmp     VgWriteByte       ;write same value in second byte

                   ; 
                   ; Draws a single digit, 0-9.
                   ; 
                   ; If we're drawing a score, we want to show "15000", not "015000".  This means
                   ; we need to drop the leading zero sometimes.  The glyph for zero is $01, and
                   ; the glyph for space is $00, so we can convert the '0' to ' ' simply by not
                   ; adding 1.  If we see a nonzero digit we want to ensure that we print all
                   ; future zeroes.
                   ; 
                   ; On entry:
                   ;   A-reg: number to draw (0-9)
                   ;   Carry flag is set to get "don't draw zero" behavior
                   ; 
                   ; On exit:
                   ;   Carry flag is set if it was set initially and value was zero
                   ; 
7a29: 90 04        DrawDigit       bcc     DrawDigit1        ;carry clear, always draw
7a2b: 29 0f                        and     #$0f              ;strip high bits
7a2d: f0 05                        beq     :IsZero           ;if zero, don't inc, so we get space instead
7a2f: 29 0f        DrawDigit1      and     #$0f              ;strip high bits
7a31: 18                           clc                       ;found a nonzero, so clear the carry
7a32: 69 01                        adc     #$01              ;add 1 to get glyph index
7a34: 08           :IsZero         php                       ;preserve carry
7a35: 0a                           asl     A                 ;two bytes per VJSR
7a36: a0 00                        ldy     #$00
7a38: aa                           tax
7a39: bd f0 33                     lda     vg_glyph_calls,x  ;get the VJSR command
7a3c: 91 02                        sta     (vg_cmd_ptr),y    ;add it to the output
7a3e: bd f1 33                     lda     vg_glyph_calls+1,x
7a41: c8                           iny
7a42: 91 02                        sta     (vg_cmd_ptr),y
7a44: 20 76 7a                     jsr     VgPtrAddY         ;incr the cmd ptr
7a47: 28                           plp                       ;restore carry
7a48: 60                           rts

                   ; 
                   ; Appends a VJMP to an address.  (Unused.)
                   ; 
7a49: 4a           unref_7a49      lsr     A
7a4a: 29 0f                        and     #$0f
7a4c: 09 e0                        ora     #$e0              ;add VJMP opcode
                   ; Common code for VJSR/VJMP output.
7a4e: a0 01        VgAddJsrJmp     ldy     #$01
7a50: 91 02                        sta     (vg_cmd_ptr),y    ;store high byte
7a52: 88                           dey
7a53: 8a                           txa
7a54: 6a                           ror     A                 ;shift low byte; note carry is still set
7a55: 91 02                        sta     (vg_cmd_ptr),y
7a57: c8                           iny
7a58: d0 1c                        bne     VgPtrAddY         ;(always)

                   ; 
                   ; Appends a VJSR instruction, taking an address ($2000-3fff) as argument.
                   ; 
                   ; On entry:
                   ;   X-reg: low byte of address
                   ;   A-reg: high byte of address
                   ; 
7a5a: 4a           VgJsrToAddr     lsr     A                 ;AVG addresses mem as 16-bit units, so divide by 2
7a5b: 29 0f                        and     #$0f              ;mask off upper 4 bits ($2/1xxx -> $0xxx)
7a5d: 09 a0                        ora     #$a0              ;add VJSR upcode
7a5f: d0 ed                        bne     VgAddJsrJmp       ;(always)

7a61: a4                           .dd1    $a4
7a62: 01                           .dd1    $01

                   ; 
                   ; Appends a STAT opcode.  This has the form:
                   ; 
                   ;   0110xEHI IIIICCCC
                   ; 
                   ; This sets the color and intensity.  IIII is a 4-bit intensity value that is
                   ; used whenever the 3-bit intensity in VCTR or SVEC is set to 1.  CCCC is a
                   ; color value not used by Battlezone.
                   ; 
                   ; When E is 0, the clip window is set based on the current beam position and the
                   ; values of the H/I flags.
                   ; 
                   ; (The hand-written documentation says E works the other way around, but that's
                   ; not how the code is using it.  MAME avgdvg.cpp avg_bzone_device::handler_6()
                   ; seems to agree with me.)
                   ; 
                   ; On entry:
                   ;   A-reg: low 3 bits of upper byte (EHI flags)
                   ;   Y-reg: low byte (intensity and color)
                   ; 
7a63: 09 60        VgStat          ora     #$60              ;add STAT opcode
7a65: aa                           tax                       ;X-reg=hi
7a66: 98                           tya                       ;Y-reg=lo
7a67: 4c 6e 7a                     jmp     VgOutputCmd

                   ; 
                   ; Outputs a CNTR opcode.  Moves beam to center of screen.
                   ; 
7a6a: a9 40        VgCenter        lda     #$40              ;$8040 = CNTR
7a6c: a2 80                        ldx     #$80
                   ; 
                   ; Appends a 16-bit value to the AVG command list.
                   ; 
                   ; On entry:
                   ;   A-reg: low byte
                   ;   X-reg: high byte
                   ; 
7a6e: a0 00        VgOutputCmd     ldy     #$00              ;store at ptr+0
7a70: 91 02                        sta     (vg_cmd_ptr),y    ;low byte
7a72: c8                           iny
7a73: 8a                           txa
7a74: 91 02                        sta     (vg_cmd_ptr),y    ;high byte (note we fall through with Y-reg=1)
                   ; 
                   ; Adds (Y-reg + 1) to the AVG cmd pointer.
                   ; 
7a76: 98           VgPtrAddY       tya
7a77: 38                           sec                       ;set carry so ADC is +1
7a78: 65 02                        adc     vg_cmd_ptr
7a7a: 85 02                        sta     vg_cmd_ptr        ;update low byte
7a7c: 90 02                        bcc     :NoInc
7a7e: e6 03                        inc     vg_cmd_ptr+1      ;update high byte
7a80: 60           :NoInc          rts

                   ; 
                   ; Appends a SCAL instruction to the command list.
                   ; 
                   ; The 'B' value sets the scale to 2^(1-B), so B=0 is double size, B=1 is normal
                   ; size, B=2 is half size, etc.  The 'L' value is set to zero.
                   ; 
                   ; On entry:
                   ;   A-reg: 'B' value (0-7)
                   ; 
7a81: a0 00        VgScaleL0       ldy     #$00              ;set L to 0
                   ; 
                   ; Adds a SCAL instruction to the command list.
                   ; 
                   ; On entry:
                   ;   A-reg: 'B' value (0-7)
                   ;   Y-reg: 'L' value (0-255)
                   ; 
7a83: 09 70        VgScale         ora     #$70              ;$7xxx = VSCALE
7a85: aa                           tax
7a86: 98                           tya
7a87: 4c 6e 7a                     jmp     VgOutputCmd

                   ; 
                   ; Draws a point.
                   ; 
                   ; The hardware requires this to be done with VECT rather than SVEC.
                   ; 
                   ; On entry:
                   ;   A-reg: intensity
                   ; 
7a8a: a8           VgDrawPoint     tay                       ;Y-reg = intensity
7a8b: a9 00                        lda     #$00              ;deltaX = deltaY = 0
7a8d: aa                           tax                       ;...fall through...
                   ; 
                   ; Appends a "long" vector draw command (VECT) to the list.
                   ; 
                   ; This takes deltaX/deltaY as signed 8-bit values, which are multiplied by 4.
                   ; 
                   ; On entry:
                   ;   A-reg: deltaX / 4
                   ;   X-reg: deltaY / 4
                   ;   Y-reg: intensity (upper 3 bits)
                   ; 
                   • Clear variables
                   ]xcoord         .var    $04    {addr/2}
                   ]ycoord         .var    $06    {addr/2}

7a8e: 84 01        VgVec8I         sty     vg_intensity      ;set intensity, fall through...
                   ; 
                   ; Like VgVec8I, but we don't overwrite the intensity in $01.  Only used by self-
                   ; test code.
                   ; 
7a90: a0 00        VgVec8          ldy     #$00              ;compute deltaX * 4; assume positive
7a92: 0a                           asl     A
7a93: 90 01                        bcc     :IsPos1
7a95: 88                           dey                       ;was negative, extend sign
7a96: 84 05        :IsPos1         sty     ]xcoord+1
7a98: 0a                           asl     A
7a99: 26 05                        rol     ]xcoord+1
7a9b: 85 04                        sta     ]xcoord
                   ; Multiply Y coordinate by 4.
7a9d: 8a                           txa
7a9e: 0a                           asl     A
7a9f: a0 00                        ldy     #$00
7aa1: 90 01                        bcc     :IsPos2
7aa3: 88                           dey                       ;extend sign
7aa4: 84 07        :IsPos2         sty     ]ycoord+1
7aa6: 0a                           asl     A
7aa7: 26 07                        rol     ]ycoord+1
7aa9: 85 06                        sta     ]ycoord           ;...fall through...
                   ; 
                   ; Appends a "long" vector draw command (VECT) to the list.
                   ; 
                   ; This accesses zero page with an indexed instruction, but I don't see a way to
                   ; execute here with a different value for X-reg.
                   ; 
                   ; On entry:
                   ;   $01: 3-bit intensity
                   ;   $04/05: 13-bit deltaX
                   ;   $06/07: 13-bit deltaY
                   ; 
7aab: a2 04        VgVector        ldx     #$04              ;X-reg=4; doesn't change
7aad: a0 00                        ldy     #$00
7aaf: b5 02                        lda     $02,x             ;$06 - dY low 8
7ab1: 91 02                        sta     (vg_cmd_ptr),y
7ab3: b5 03                        lda     $03,x             ;$07 - dY high 5
7ab5: 29 1f                        and     #%00011111        ;mask it; high 3 bits are opcode (0)
7ab7: c8                           iny
7ab8: 91 02                        sta     (vg_cmd_ptr),y
7aba: b5 00                        lda     $00,x             ;$04 - dX low 8
7abc: c8                           iny
7abd: 91 02                        sta     (vg_cmd_ptr),y
7abf: b5 01                        lda     $01,x             ;$05 - dX high 5
7ac1: 45 01                        eor     vg_intensity      ;merge intensity by XORing all bits,
7ac3: 29 1f                        and     #%00011111        ; then masking off the intensity bits,
7ac5: 45 01                        eor     vg_intensity      ; then XORing again so only the intensity is changed
7ac7: c8           VgWriteByte     iny
7ac8: 91 02                        sta     (vg_cmd_ptr),y
7aca: d0 aa                        bne     VgPtrAddY         ;(always)

                   ; 
                   ; Zeroes out memory and resets hardware.
                   ; 
                   ; Control transfers here when the 6502 is reset.
                   ; 
7acc: a2 7f        ResetSystem     ldx     #$7f              ;init stack to $7f
7ace: 9a                           txs
7acf: a9 00                        lda     #$00
7ad1: 8d 2f 18                     sta     POKEY_SKCTL       ;disable inputs
7ad4: d8                           cld
7ad5: aa                           tax                       ;X-reg=0
7ad6: 95 00        :ZpLoop         sta     $00,x             ;clear zero page
7ad8: e8                           inx
7ad9: d0 fb                        bne     :ZpLoop
7adb: a0 07                        ldy     #$07
7add: 8c 2f 18                     sty     POKEY_SKCTL       ;enable inputs
7ae0: 9d 00 01     :MemLoop        sta     $0100,x           ;zero out $100/$200/$300
7ae3: 9d 00 02                     sta     $0200,x
7ae6: 9d 00 03                     sta     $0300,x
7ae9: 9d 00 20                     sta     VEC_GEN_RAM,x     ;zero out $2000-2fff
7aec: 9d 00 21                     sta     VEC_GEN_RAM+$100,x
7aef: 9d 00 22                     sta     VEC_GEN_RAM+$200,x
7af2: 9d 00 23                     sta     VEC_GEN_RAM+$300,x
7af5: 9d 00 24                     sta     VEC_GEN_RAM+$400,x
7af8: 9d 00 25                     sta     VEC_GEN_RAM+$500,x
7afb: 9d 00 26                     sta     VEC_GEN_RAM+$600,x
7afe: 9d 00 27                     sta     VEC_GEN_RAM+$700,x
7b01: 9d 00 28                     sta     VEC_GEN_RAM+$800,x
7b04: 9d 00 29                     sta     VEC_GEN_RAM+$900,x
7b07: 9d 00 2a                     sta     VEC_GEN_RAM+$a00,x
7b0a: 9d 00 2b                     sta     VEC_GEN_RAM+$b00,x
7b0d: 9d 00 2c                     sta     VEC_GEN_RAM+$c00,x
7b10: 9d 00 2d                     sta     VEC_GEN_RAM+$d00,x
7b13: 9d 00 2e                     sta     VEC_GEN_RAM+$e00,x
7b16: 9d 00 2f                     sta     VEC_GEN_RAM+$f00,x
7b19: 8d 00 14                     sta     WATCHDOG_CLEAR    ;keep feeding the watchdog
7b1c: e8                           inx
7b1d: d0 c1                        bne     :MemLoop
7b1f: 8d 00 16                     sta     VEC_GEN_RESET     ;reset the vector state machine
7b22: ad 00 08                     lda     IN0
7b25: 29 10                        and     #$10              ;self-test switch set?
7b27: d0 08                        bne     :NotSelfTest      ;no, regular start
7b29: a9 20                        lda     #$20
7b2b: 8d 40 18                     sta     DSOUND_CTRL
7b2e: 4c cb 7b                     jmp     SystemTest

7b31: a9 01        :NotSelfTest    lda     #$01              ;write VJMP $2802 to offset zero
7b33: 8d 00 20                     sta     VEC_GEN_RAM
7b36: a9 e4                        lda     #$e4
7b38: 8d 01 20                     sta     VEC_GEN_RAM+1
7b3b: a9 20                        lda     #$20              ;write VHALT instruction at +2
7b3d: 8d 02 20                     sta     VEC_GEN_RAM+2
7b40: 8d 03 20                     sta     VEC_GEN_RAM+3
7b43: 8d 02 28                     sta     VEC_GEN_RAM+$802  ;in both segments
7b46: 8d 03 28                     sta     VEC_GEN_RAM+$803
7b49: a9 01                        lda     #$01
7b4b: 85 c4                        sta     move_counter
7b4d: 85 cd                        sta     game_over_flags   ;player is dead
7b4f: a9 fe                        lda     #$fe
7b51: 8d 24 18                     sta     POKEY_AUDF3       ;init channels 3/4 (missile buzz)
7b54: e8                           inx                       ;?
7b55: 8d 26 18                     sta     POKEY_AUDF4
                   ; Initialize high score table.  The score and the initials are 3 bytes each. 
                   ; The loop goes from 29 to 2, stepping by 3.
7b58: a2 1d                        ldx     #29
7b5a: a0 00                        ldy     #$00
7b5c: bd 7d 7b     :HsLoop         lda     hs_def_initials-2,x
7b5f: 9d 1c 03                     sta     hs_initials-2,x
7b62: a9 05                        lda     #$05              ;5000 points
7b64: 9d fe 02                     sta     hs_scores-2,x     ;(the entire page was zeroed earlier, so no need
7b67: bd 7e 7b                     lda     hs_def_initials-1,x ; to set the low byte)
7b6a: 9d 1d 03                     sta     hs_initials-1,x
7b6d: bd 7f 7b                     lda     hs_def_initials,x
7b70: 9d 1e 03                     sta     hs_initials,x
7b73: c8                           iny
7b74: ca                           dex
7b75: ca                           dex
7b76: ca                           dex
7b77: 10 e3                        bpl     :HsLoop
7b79: 20 1f 53                     jsr     InitLogoAndCtrs
7b7c: 4c 00 50                     jmp     MainLoop

                   ; 
                   ; Power-on high scores list.
                   ; 
7b7f: 1e 1c 38     hs_def_initials .bulk   $1e,$1c,$38       ;EDR (Ed Rotberg?)
7b82: 2e 34 24                     .bulk   $2e,$34,$24       ;MPH (Morgan Hoff?)
7b85: 28 1e 1c                     .bulk   $28,$1e,$1c       ;JED (Jed Margolin?)
7b88: 1c 1e 3a                     .bulk   $1c,$1e,$3a       ;DES (Doug Snyder?)
7b8b: 3c 2a 1e                     .bulk   $3c,$2a,$1e       ;TKE (?)
7b8e: 40 2a 18                     .bulk   $40,$2a,$18       ;VKB (?)
7b91: 1e 2c 00                     .bulk   $1e,$2c,$00       ;EL (?)
7b94: 24 16 1c                     .bulk   $24,$16,$1c       ;HAD (Howard Delman?)
7b97: 32 38 38                     .bulk   $32,$38,$38       ;ORR (Owen Rubin?)
7b9a: 22 28 38                     .bulk   $22,$28,$38       ;GJR (?)
                   ; 
7b9d: e3                           .dd1    $e3               ;(checksum adj)

                   ; 
                   ; Draws a four-digit (two-byte) BCD number.
                   ; 
7b9e: a0 02        DrawFourDigits  ldy     #$02
                   ; 
                   ; Draws an N-digit BCD number.  Values are stored in zero page in little-endian
                   ; order, so when drawing from left to right we want to start with the last byte.
                   ; 
                   ; Leading zeroes are output as spaces.
                   ; 
                   ; On entry:
                   ;   A-reg: zero page location with BCD data
                   ;   Y-reg: number of two-digit values to draw
                   ; 
                   • Clear variables
                   ]index          .var    $08    {addr/1}
                   ]count          .var    $09    {addr/1}

7ba0: 38           DrawNDigits     sec                       ;convert leading zeroes to spaces
7ba1: 08                           php                       ;save the set carry flag?
7ba2: 88                           dey
7ba3: 84 09                        sty     ]count
7ba5: 18                           clc
7ba6: 65 09                        adc     ]count            ;start with the last one
7ba8: 28                           plp                       ;restore the set carry flag?
7ba9: aa                           tax
7baa: 08           :Loop           php                       ;save carry flag
7bab: 86 08                        stx     ]index
7bad: b5 00                        lda     $00,x             ;get BCD value
7baf: 4a                           lsr     A                 ;shift the high nibble in place
7bb0: 4a                           lsr     A
7bb1: 4a                           lsr     A
7bb2: 4a                           lsr     A
7bb3: 28                           plp                       ;restore carry flag
7bb4: 20 29 7a                     jsr     DrawDigit         ;draw the digit (updates carry flag)
7bb7: a5 09                        lda     ]count            ;last byte?
7bb9: d0 01                        bne     :NotLast          ;no, branch
7bbb: 18                           clc                       ;clear carry so last single digit is always drawn
7bbc: a6 08        :NotLast        ldx     ]index
7bbe: b5 00                        lda     $00,x             ;get BCD value
7bc0: 20 29 7a                     jsr     DrawDigit         ;draw the low nibble
7bc3: a6 08                        ldx     ]index
7bc5: ca                           dex                       ;move to next byte
7bc6: c6 09                        dec     ]count            ;done yet?
7bc8: 10 e0                        bpl     :Loop
7bca: 60                           rts

                   ; 
                   ; System test, engaged by resetting the system with the test button enabled.
                   ; 
                   ; Called from the system reset code, which initializes RAM to zero before
                   ; jumping here.
                   ; 
                   • Clear variables

7bcb: a2 11        SystemTest      ldx     #%00010001        ;use stack pointer to hold mem pattern
7bcd: 9a                           txs                       ;(we don't know if we can trust RAM yet)
7bce: 8a                           txa
7bcf: 85 00                        sta     $00               ;set address $00 to a nonzero value
7bd1: a0 00                        ldy     #$00              ;start by skipping address $00
7bd3: a2 01        :OuterLoop      ldx     #$01              ;init counter for 255 iterations
7bd5: c8           :Loop           iny
7bd6: b9 00 00                     lda     $0000,y
7bd9: d0 21                        bne     :ZPRamFail        ;nonzero value found; should not happen
7bdb: e8                           inx
7bdc: d0 f7                        bne     :Loop             ;continue for 255 tests
                   ; The 255 addresses we didn't set are all zero.  Now verify that the address we
                   ; set hasn't changed.
7bde: ba                           tsx                       ;X-reg=pattern
7bdf: 8a                           txa                       ;A-reg=pattern
7be0: 8d 00 14                     sta     WATCHDOG_CLEAR    ;feed the dog
7be3: c8                           iny                       ;Y-reg=index of byte with nonzero value
7be4: 59 00 00                     eor     $0000,y           ;if all is well, this will be zero
7be7: d0 13                        bne     :ZPRamFail        ;whoops
7be9: 8a                           txa                       ;A-reg=pattern
7bea: a2 00                        ldx     #$00
7bec: 96 00                        stx     $00,y             ;store zero in the address we set to non-zero
7bee: c8                           iny                       ;advance to the next address
7bef: d0 05                        bne     :NotDone
7bf1: 0a                           asl     A                 ;shift the pattern one bit left
7bf2: a2 00                        ldx     #$00              ;?
7bf4: b0 4e                        bcs     TestMoreRam       ;tested with 4 patterns, move on
7bf6: aa           :NotDone        tax                       ;pattern to X-reg
7bf7: 9a                           txs                       ;X-reg to stack pointer
7bf8: 96 00                        stx     $00,y             ;make a byte nonzero
7bfa: d0 d7                        bne     :OuterLoop        ;(always)

7bfc: aa           :ZPRamFail      tax                       ;failed value to X-reg
7bfd: 8a                           txa                       ;?
7bfe: a0 82                        ldy     #$82              ;freq1=low
7c00: 29 0f                        and     #$0f              ;failed in low bits?
7c02: f0 02                        beq     :LoZero
7c04: a0 12                        ldy     #$12              ;freq1=high
7c06: 8a           :LoZero         txa                       ;restore failed value
7c07: a2 82                        ldx     #$82              ;freq2=low
7c09: 29 f0                        and     #$f0              ;failed in high bits?
7c0b: f0 02                        beq     :HiZero
7c0d: a2 12                        ldx     #$12              ;freq2=high
7c0f: 98           :HiZero         tya                       ;A-reg=freq1
7c10: 9a                           txs                       ;stack ptr = freq2
7c11: aa                           tax                       ;X-reg=freq1
                   ; Play diagnostic sound.  From the Atari service manual:
                   ; 
                   ; "RAM FAILURE is indicated by a sequence of 1 to 10 tones. You will hear a
                   ; short low tone for each good RAM chip, and a long high tone for a failing RAM
                   ; chip. The test stops with the first failing RAM-chip pair (example: J2 and H2
                   ; are a pair)."
                   ; 
                   ; At this point, the tone for the first zero-page RAM chip is in the X-reg, and
                   ; the tone for the second is in the stack pointer.  The frequency values both
                   ; end in 2 so that it can also be used as a counter.
7c12: 8e 20 18     :PlaySound      stx     POKEY_AUDF1       ;set frequency
7c15: a2 a8                        ldx     #$a8
7c17: 8e 21 18                     stx     POKEY_AUDC1       ;set volume
7c1a: a0 0c                        ldy     #12               ;12 iterations of everything
7c1c: a2 64        :Wait           ldx     #100              ;100 iterations of wait loop
7c1e: 2c 00 08     :WaitLoop1      bit     IN0               ;"bit 7 is tied to a 3kHz clock"
7c21: 30 fb                        bmi     :WaitLoop1
7c23: 2c 00 08     :WaitLoop2      bit     IN0
7c26: 10 fb                        bpl     :WaitLoop2
7c28: 8d 00 14                     sta     WATCHDOG_CLEAR    ;feed the dog
7c2b: ca                           dex
7c2c: d0 f0                        bne     :WaitLoop1
7c2e: c0 05                        cpy     #$05              ;sound is on for iterations 12-6
7c30: d0 03                        bne     :SoundOn          ; and off for 5-1
7c32: 8e 21 18                     stx     POKEY_AUDC1       ;X-reg=0, sound off
7c35: 88           :SoundOn        dey                       ;decrement beep count
7c36: d0 e4                        bne     :Wait
7c38: 4a                           lsr     A                 ;A-reg is $12 or $82, so this runs twice
7c39: b0 03                        bcs     :Forever          ;we're done, branch
7c3b: ba                           tsx                       ;put frequency #2 in X-reg
7c3c: d0 d4                        bne     :PlaySound
7c3e: 8d 00 14     :Forever        sta     WATCHDOG_CLEAR    ;spin until the device is reset
7c41: 4c 3e 7c                     jmp     :Forever

                   ; 
                   ; Test RAM from $100-3ff and $2000-3fff.  Now that we know zero page RAM is
                   ; working we can use pointers.
                   ; 
                   ; There are 5 pairs of RAM chips.  Each pair holds 1K of memory:
                   ;   0/1: $0000-03ff
                   ;   2/3: $2000-23ff
                   ;   4/5: $2400-27ff
                   ;   6/7: $2800-2bff
                   ;   8/9: $2c00-2fff
                   ; 
                   ; One chip in each pair holds the low 4 bits, the other holds the high 4 bits.
                   ; 
                   ]ptr            .var    $00    {addr/2}
                   ]tmp_02         .var    $02    {addr/1}

7c44: a2 7f        TestMoreRam     ldx     #$7f              ;set stack pointer to reasonable value
7c46: 9a                           txs
7c47: a2 00                        ldx     #$00
7c49: 8a                           txa
7c4a: 95 00        :ZLoop          sta     $00,x             ;zero out zero page (why?)
7c4c: e8                           inx
7c4d: d0 fb                        bne     :ZLoop
                   ; 
7c4f: a8                           tay                       ;Y-reg=0
7c50: a9 01                        lda     #$01              ;start at $100
7c52: 85 01        :SetPtr         sta     ]ptr+1            ;set pointer
7c54: a2 11        :PageLoop       ldx     #%00010001        ;X-reg=test pattern
7c56: b1 00                        lda     (]ptr),y          ;load value, should be zero
7c58: d0 27                        bne     :BadRam
7c5a: 8a           :ByteLoop       txa                       ;A-reg=test pattern
7c5b: 91 00                        sta     (]ptr),y          ;write it
7c5d: 51 00                        eor     (]ptr),y          ;XOR it, result should be zero
7c5f: d0 20                        bne     :BadRam
7c61: 8a                           txa                       ;A-reg=test pattern
7c62: 0a                           asl     A                 ;shift one bit left
7c63: aa                           tax                       ;X-reg=updated test pattern
7c64: 90 f4                        bcc     :ByteLoop         ;try all patterns at this address
7c66: c8                           iny                       ;move to the next address
7c67: d0 eb                        bne     :PageLoop         ;continue for all 256 bytes on this page
7c69: 8d 00 14                     sta     WATCHDOG_CLEAR    ;feed the dog
7c6c: e6 01                        inc     ]ptr+1            ;advance to the next page
7c6e: a6 01                        ldx     ]ptr+1
7c70: e0 04                        cpx     #$04              ;did we hit $400?
7c72: 90 e0                        bcc     :PageLoop         ;still below that, keep going
7c74: a9 20                        lda     #$20              ;prep for move to $2000
7c76: e0 20                        cpx     #$20              ;are we jumping there now?
7c78: 90 d8                        bcc     :SetPtr           ;yes, branch to instruction that sets ptr
7c7a: e0 30                        cpx     #$30              ;did we hit $3000?
7c7c: 90 d6                        bcc     :PageLoop         ;not yet, keep going
7c7e: 4c 01 7d                     jmp     TestRomChecksum   ;RAM is good, move on

                   ; Play failed RAM test tone.  As noted earlier, play 1-10 tones, with short/low
                   ; for good and long/high for bad.  We play tones for both chips in a pair,
                   ; stopping at the bad pair.
                   ; 
                   ; At this point, A-reg has failed value, Y-reg has byte offset.
                   ; 
                   ; Bug here: if there's a fault in low RAM ($0000-03ff), the error is mis-
                   ; reported.
7c81: a6 01        :BadRam         ldx     ]ptr+1            ;get memory page number
7c83: e0 20                        cpx     #$20              ;did we hit $2000?
7c85: 85 02                        sta     ]tmp_02           ;save failed value
7c87: 90 03                        bcc     :LowMem           ;BUG: BCC should come after the TXA
7c89: 8a                           txa                       ;$20-$04=$1c; this gives us an index into a
7c8a: e9 1c                        sbc     #$1c              ; contiguous range $00-13
7c8c: 4a           :LowMem         lsr     A                 ;divide by 4 to get chip pair index (0-4)
7c8d: 4a                           lsr     A
7c8e: 29 07                        and     #%00000111        ;mask unnecessary bits (not needed?)
7c90: a8                           tay
7c91: a5 02                        lda     ]tmp_02
                   ]chip_pair_idx  .var    $00    {addr/1}
                   ]fail_val       .var    $01    {addr/1}
                   ]counter        .var    $02    {addr/1}

7c93: 84 00                        sty     ]chip_pair_idx
7c95: 85 01                        sta     ]fail_val
7c97: a9 01        :PairLoop       lda     #$01              ;two chips in a pair
7c99: 85 02                        sta     ]counter
7c9b: a2 a8        :ChipLoop       ldx     #$a8              ;volume
7c9d: a0 82                        ldy     #$82              ;freq=low
7c9f: a5 00                        lda     ]chip_pair_idx    ;was this the failing pair?
7ca1: d0 08                        bne     :SetSound         ;no, branch
7ca3: a5 01                        lda     ]fail_val         ;check the failing value
7ca5: 29 0f                        and     #$0f              ;was it the bits in this chip?
7ca7: f0 02                        beq     :SetSound         ;no, branch
7ca9: a0 12                        ldy     #$12              ;was this chip, freq=hi
7cab: 8e 21 18     :SetSound       stx     POKEY_AUDC1       ;volume
7cae: 8c 20 18                     sty     POKEY_AUDF1       ;frequency
7cb1: a9 09                        lda     #$09              ;long tone
7cb3: c0 12                        cpy     #$12              ;high frequency?
7cb5: f0 02                        beq     :PlayTone         ;yes, play it long
7cb7: a9 01                        lda     #$01              ;low frequency, short duration
                   ; Play the first tone for a bit.
7cb9: a8           :PlayTone       tay
7cba: a2 00                        ldx     #$00
7cbc: 2c 00 08     :Loop1          bit     IN0
7cbf: 30 fb                        bmi     :Loop1
7cc1: 2c 00 08     :Loop2          bit     IN0
7cc4: 10 fb                        bpl     :Loop2
7cc6: 8d 00 14                     sta     WATCHDOG_CLEAR
7cc9: ca                           dex
7cca: d0 f0                        bne     :Loop1
7ccc: 88                           dey
7ccd: d0 ed                        bne     :Loop1
                   ; Pause briefly.
7ccf: 8e 21 18                     stx     POKEY_AUDC1       ;X-reg=0 (sound off)
7cd2: a0 09                        ldy     #$09
7cd4: 2c 00 08     :Loop3          bit     IN0
7cd7: 30 fb                        bmi     :Loop3
7cd9: 2c 00 08     :Loop4          bit     IN0
7cdc: 10 fb                        bpl     :Loop4
7cde: 8d 00 14                     sta     WATCHDOG_CLEAR
7ce1: ca                           dex
7ce2: d0 f0                        bne     :Loop3
7ce4: 88                           dey
7ce5: d0 ed                        bne     :Loop3
7ce7: a5 00                        lda     ]chip_pair_idx    ;have we reached the failing chip yet?
7ce9: d0 08                        bne     :NotFail          ;not yet
7ceb: a5 01                        lda     ]fail_val         ;get the value that failed
7ced: 4a                           lsr     A                 ;shift it to test the high 4 bits
7cee: 4a                           lsr     A
7cef: 4a                           lsr     A
7cf0: 4a                           lsr     A
7cf1: 85 01                        sta     ]fail_val
7cf3: c6 02        :NotFail        dec     ]counter          ;have we done both chips in this pair?
7cf5: f0 a4                        beq     :ChipLoop         ;no, do the other chip
7cf7: c6 00                        dec     ]chip_pair_idx    ;have we done all pairs?
7cf9: 10 9c                        bpl     :PairLoop         ;not yet
7cfb: 8d 00 14     :Forever        sta     WATCHDOG_CLEAR
7cfe: 4c fb 7c                     jmp     :Forever          ;spin forever

                   ; 
                   ; Compute a rolling XOR of $ff with the contents of each 2KB chunk.  Each
                   ; section of ROM has been tweaked so that the final XOR value is zero, so no
                   ; table of correct values is required (which allows you to replace ROMs
                   ; individually without having to update the test code).  Search for "checksum
                   ; adj" comments to see which bytes appear to be used for this.
                   ; 
                   • Clear variables
                   ]ptr2           .var    $08    {addr/2}
                   ]count          .var    $0a    {addr/1}
                   ]checksums      .var    $3d    {addr/8}

7d01: a9 00        TestRomChecksum lda     #$00
7d03: a8                           tay
7d04: aa                           tax
7d05: 85 08                        sta     ]ptr2
7d07: a9 30                        lda     #$30              ;start of AVG ROM ($3000)
7d09: 85 09        :RomLoop        sta     ]ptr2+1
7d0b: a9 08                        lda     #$08              ;8 pages at a time (ROMs are 2KB)
7d0d: 85 0a                        sta     ]count
7d0f: a9 ff                        lda     #$ff
7d11: 51 08        :PgLoop         eor     (]ptr2),y
7d13: c8                           iny
7d14: d0 fb                        bne     :PgLoop
7d16: e6 09                        inc     ]ptr2+1
7d18: 8d 00 14                     sta     WATCHDOG_CLEAR    ;feed the dog
7d1b: c6 0a                        dec     ]count
7d1d: d0 f2                        bne     :PgLoop
7d1f: 95 3d                        sta     ]checksums,x      ;save page sum at $3d-44
7d21: e8                           inx
7d22: a5 09                        lda     ]ptr2+1
7d24: c9 40                        cmp     #$40              ;end of AVG ROM?
7d26: d0 04                        bne     :Cont             ;not yet
7d28: a9 50                        lda     #$50              ;start of game ROM ($5000)
7d2a: 85 09                        sta     ]ptr2+1
7d2c: c9 80        :Cont           cmp     #$80              ;end of game ROM?
7d2e: 90 d9                        bcc     :RomLoop          ;not yet
                   ; 
                   ; Get "diag step" switch state from IN0.  The bit is set when the switch is not
                   ; pressed.  (In MAME, the '9' key is bound to the switch.)
                   ; 
                   ]diag_step      .var    $0c    {addr/1}

7d30: a2 ff                        ldx     #$ff
7d32: ad 00 08                     lda     IN0
7d35: 29 20                        and     #$20              ;"diagnostic step"
7d37: d0 01                        bne     :NoDStep          ;not pressed
7d39: e8                           inx                       ;X-reg=0
7d3a: 86 0c        :NoDStep        stx     ]diag_step
                   ; 
                   ; Math box test.
                   ; 
7d3c: a2 00        TestMathBox     ldx     #$00
7d3e: 18           :Loop           clc
7d3f: bd 87 7d                     lda     math_box_tests,x  ;get first byte
7d42: 30 40                        bmi     :TestDone         ;if high bit is set, we've reached the end
7d44: 29 1f                        and     #$1f              ;lose the high bits
7d46: a8                           tay                       ;this is the MB register we're going to write to
7d47: bd 88 7d                     lda     math_box_tests+1,x ;get the value to write
7d4a: 99 60 18                     sta     MB_SET_R0L,y
7d4d: a0 08                        ldy     #$08              ;try 9x
7d4f: 2c 00 18     :MbLoop         bit     MB_STATUS         ;done yet?
7d52: 10 07                        bpl     :HaveResult       ;yes, branch
7d54: 88                           dey
7d55: 10 f8                        bpl     :MbLoop
7d57: a9 54                        lda     #‘T’              ;error='T' (time out error)
7d59: d0 28                        bne     :ReportErr

7d5b: bd 87 7d     :HaveResult     lda     math_box_tests,x  ;get first byte again
7d5e: 0a                           asl     A                 ;shift it
7d5f: a8                           tay                       ;preserve it
7d60: 10 0d                        bpl     :NoLoResult       ;bit 6 not set, no low result
7d62: ad 10 18                     lda     MB_RESULT_LO
7d65: dd 89 7d                     cmp     math_box_tests+2,x
7d68: f0 04                        beq     :LoGood
7d6a: a9 4c                        lda     #‘L’              ;error='L' (data error, low byte)
7d6c: d0 15                        bne     :ReportErr

7d6e: e8           :LoGood         inx                       ;advance past byte
7d6f: 98           :NoLoResult     tya
7d70: 0a                           asl     A                 ;check if high-result bit is set
7d71: 10 09                        bpl     :NoHiResult
7d73: ad 18 18                     lda     MB_RESULT_HI      ;test the high result against the expected value
7d76: dd 89 7d                     cmp     math_box_tests+2,x
7d79: d0 06                        bne     :HiBad
7d7b: e8                           inx                       ;advance past byte
7d7c: e8           :NoHiResult     inx                       ;advance two more
7d7d: e8                           inx
7d7e: 4c 3e 7d                     jmp     :Loop

7d81: a9 48        :HiBad          lda     #‘H’              ;error='H' (data error, high byte)
7d83: 38           :ReportErr      sec
7d84: 4c f4 7d     :TestDone       jmp     ReportTestResult

                   ; 
                   ; This is a sequence of tests for the math box.  Each test item is 2-4 bytes. 
                   ; The first byte has the form:
                   ; 
                   ;   ELHIIIII
                   ;     E=1 if this is the end of the test sequence
                   ;     L=1 if we want to check the low byte of the result
                   ;     H=1 if we want to check the high byte of the result
                   ;     IIIII=register index to write to (0-31)
                   ; 
                   ; The second byte is the value that gets written to the specified register.  If
                   ; 'L' is set, the next byte is the expected result low byte.  If 'H' is set, the
                   ; next byte is the expected result high byte.
                   ; 
7d87: 40 00 00     math_box_tests  .bulk   $40,$00,$00       ;R0L=$00
7d8a: 61 7f 00 7f                  .bulk   $61,$7f,$00,$7f   ;R0H=$7f
7d8e: 42 00 00                     .bulk   $42,$00,$00       ;R1L=$00
7d91: 63 00 00 00                  .bulk   $63,$00,$00,$00   ;R1H=$00
7d95: 44 00 00                     .bulk   $44,$00,$00       ;R2L=$00
7d98: 65 00 00 00                  .bulk   $65,$00,$00,$00   ;R2H=$00
7d9c: 46 00 00                     .bulk   $46,$00,$00       ;R3L=$00
7d9f: 67 00 00 00                  .bulk   $67,$00,$00,$00   ;R3H=$00
7da3: 48 00 00                     .bulk   $48,$00,$00       ;R4L=$00
7da6: 69 01 00 01                  .bulk   $69,$01,$00,$01   ;R4H=$01
7daa: 4a 80 80                     .bulk   $4a,$80,$80       ;R5L=$80
7dad: 6b 00 7f 00                  .bulk   $6b,$00,$7f,$00   ;R5H=$00; MB_ROT_X
7db1: 6c 10 10 00                  .bulk   $6c,$10,$10,$00   ;R6=$10
7db5: 72 00 3f 00                  .bulk   $72,$00,$3f,$00   ;MB_ROT_Z
7db9: 78 00 00 80                  .bulk   $78,$00,$00,$80   ;result=R9
7dbd: 79 00 3f 00                  .bulk   $79,$00,$3f,$00   ;result=R8
7dc1: 77 00 7f 00                  .bulk   $77,$00,$7f,$00   ;result=R7
7dc5: 73 00 00 80                  .bulk   $73,$00,$00,$80   ;result=R8.R9/R7
7dc9: 40 00 00                     .bulk   $40,$00,$00       ;R0L=$00
7dcc: 61 5a 00 5a                  .bulk   $61,$5a,$00,$5a   ;R0H=$5a
7dd0: 42 00 00                     .bulk   $42,$00,$00       ;R1L=$00
7dd3: 63 a6 00 a6                  .bulk   $63,$a6,$00,$a6   ;R1H=$a6
7dd7: 44 ff ff                     .bulk   $44,$ff,$ff       ;R2L=$ff
7dda: 65 7f ff 7f                  .bulk   $65,$7f,$ff,$7f   ;R2H=$7f
7dde: 46 ff ff                     .bulk   $46,$ff,$ff       ;R3L=$ff
7de1: 67 7f ff 7f                  .bulk   $67,$7f,$ff,$7f   ;R3H=$7f
7de5: 48 00 00                     .bulk   $48,$00,$00       ;R4L=$00
7de8: 69 01 00 01                  .bulk   $69,$01,$00,$01   ;R4H=$01
7dec: 4a 00 00                     .bulk   $4a,$00,$00       ;R5L=$00
7def: 6b 01 b4 a6                  .bulk   $6b,$01,$b4,$a6   ;R5H=$01; MB_ROT_X
7df3: ff                           .dd1    $ff

                   ; 
                   ; Report results.
                   ; 
                   ]unused_00?     .var    $00    {addr/1}
                   ]mb_fail_code   .var    $0d    {addr/1}

                   ReportTestResult
7df4: b0 02                        bcs     :Fail
7df6: a9 00                        lda     #$00              ;indicate math box success
7df8: 85 0d        :Fail           sta     ]mb_fail_code
7dfa: a5 3d                        lda     ]checksums        ;ROM okay?
7dfc: f0 0a                        beq     :RomOkay
                   ; Atari service manual: "If ROM or PROM B/C3 is bad, you will hear a continuous
                   ; low tone, and the program may be unable to display a screen image."
7dfe: a9 f0                        lda     #$f0              ;low freq
7e00: a2 a2                        ldx     #$a2              ;volume
7e02: 8d 20 18                     sta     POKEY_AUDF1
7e05: 8e 21 18                     stx     POKEY_AUDC1
                   ; 
7e08: a9 02        :RomOkay        lda     #$02
7e0a: 85 00                        sta     ]unused_00?
7e0c: a9 00                        lda     #$00
7e0e: 85 01                        sta     vg_intensity
                   ; Pause for a bit.  We can't use the NMI for timing, so this is how we pace the
                   ; display refresh.
7e10: a2 28                        ldx     #$28
7e12: 2c 00 08     :Wait1          bit     IN0
7e15: 10 fb                        bpl     :Wait1
7e17: 2c 00 08     :Wait2          bit     IN0
7e1a: 30 fb                        bmi     :Wait2
7e1c: ca                           dex
7e1d: 10 f3                        bpl     :Wait1
7e1f: 2c 00 08     :Wait3          bit     IN0               ;bit 6 is "busy vector processor"
7e22: 50 fb                        bvc     :Wait3            ;so we're waiting for VSM to halt (?)
7e24: 8d 00 14                     sta     WATCHDOG_CLEAR
                   ; Draw some stuff on the screen, using the game routines.
7e27: a9 00                        lda     #<VEC_GEN_RAM     ;init AVG cmd pointer
7e29: 85 02                        sta     vg_cmd_ptr
7e2b: a9 20                        lda     #>VEC_GEN_RAM
7e2d: 85 03                        sta     vg_cmd_ptr+1
7e2f: 24 0c                        bit     ]diag_step        ;flag set?
7e31: 30 36                        bmi     :SkipDiag         ;skip this
                   ; 
                   ; Draw lines for the VSM test.  Activated by the diag step switch.
                   ; 
7e33: 20 6a 7a                     jsr     VgCenter          ;move to center
7e36: a9 81                        lda     #$81
7e38: aa                           tax
7e39: a0 00                        ldy     #$00
7e3b: 20 8e 7a                     jsr     VgVec8I           ;move to -508,-508
7e3e: a9 00                        lda     #$00              ;E=0 H O
7e40: 20 63 7a                     jsr     VgStat            ;set window
7e43: 20 6a 7a                     jsr     VgCenter          ;back to center
7e46: a9 7f                        lda     #$7f
7e48: aa                           tax
7e49: a0 00                        ldy     #$00
7e4b: 20 8e 7a                     jsr     VgVec8I           ;move to +508,+508
7e4e: a9 02                        lda     #$02              ;E=0 L O
7e50: 20 63 7a                     jsr     VgStat            ;set window
7e53: a6 0c                        ldx     ]diag_step
                   ; Add commands for VSM test #X, where X is 0-3.  The first test is a fat
                   ; diagonal line from the bottom left to the top right, the rest add on to it. 
                   ; At this point the pointer in $02 is $2010.
7e55: bc ca 7f                     ldy     vsm_test_seq_lens,x ;get length of sequence
7e58: a9 20                        lda     #$20              ;HALT
7e5a: 91 02                        sta     (vg_cmd_ptr),y
7e5c: 88                           dey                       ;skip this byte (value doesn't matter)
7e5d: 88                           dey                       ;prev byte
7e5e: b9 ce 7f     :Loop           lda     vsm_test_seq,y    ;copy data from test sequence
7e61: 91 02                        sta     (vg_cmd_ptr),y
7e63: 88                           dey
7e64: 10 f8                        bpl     :Loop
7e66: 4c 2a 7f                     jmp     :StartVSM         ;skip cross-hatch pattern

                   ]dip_bits       .var    $0b    {addr/1}

7e69: a9 37        :SkipDiag       lda     #>vg_selftest_pat ;get crosshatch pattern
7e6b: a2 0a                        ldx     #<vg_selftest_pat
7e6d: 20 5a 7a                     jsr     VgJsrToAddr       ;add to command list
7e70: 20 6a 7a                     jsr     VgCenter          ;move to center
7e73: a9 82                        lda     #$82
7e75: a2 a7                        ldx     #$a7
7e77: 20 90 7a                     jsr     VgVec8            ;move to -504,-356
7e7a: a9 33                        lda     #>vg_glyph_calls  ;get all-glyph VJSR list
7e7c: a2 f0                        ldx     #<vg_glyph_calls
7e7e: 20 5a 7a                     jsr     VgJsrToAddr       ;show them
7e81: 20 6a 7a                     jsr     VgCenter          ;back to center
7e84: a9 d3                        lda     #$d3
7e86: a2 13                        ldx     #$13
7e88: 20 90 7a                     jsr     VgVec8            ;move to -180,+76
7e8b: a2 0f                        ldx     #$0f              ;16 bits in the DIP switches
7e8d: ad 00 0a                     lda     DSW0              ;get DIP switch settings
7e90: 49 ff                        eor     #$ff              ;0/1 on switch labels are reversed
7e92: 85 0b                        sta     ]dip_bits
7e94: ad 00 0c                     lda     DSW1              ;get other DIP switch settings
7e97: 49 ff                        eor     #$ff
7e99: 48           :Loop           pha                       ;loop over all 16 bits
7e9a: 86 0a                        stx     ]count
7e9c: 29 01                        and     #$01              ;0/1
7e9e: 20 2f 7a                     jsr     DrawDigit1
7ea1: a6 0a                        ldx     ]count
7ea3: 46 0b                        lsr     ]dip_bits
7ea5: 68                           pla
7ea6: 6a                           ror     A
7ea7: ca                           dex
7ea8: 10 ef                        bpl     :Loop
                   ; 
                   ]bad_chk_val    .var    $00    {addr/1}
                   ]saved_index    .var    $0a    {addr/1}
                   ]row_offset     .var    $0b    {addr/1}
                   ]prev_inputs    .var    $10    {addr/1}
                   ]prev_diag_step .var    $13    {addr/1}

7eaa: 20 6a 7a                     jsr     VgCenter          ;move to center
7ead: a9 fb                        lda     #$fb
7eaf: a2 1d                        ldx     #$1d
7eb1: 20 90 7a                     jsr     VgVec8            ;move to -20,+116
7eb4: a9 01                        lda     #$01              ;draw a '1', always
7eb6: 20 2f 7a                     jsr     DrawDigit1
7eb9: ad 00 0c                     lda     DSW1              ;get DIP switch 1 again
7ebc: 29 10                        and     #%00010000        ;center coin mech
7ebe: 4a                           lsr     A                 ;shift right 4x
7ebf: 4a                           lsr     A
7ec0: 4a                           lsr     A
7ec1: 4a                           lsr     A
7ec2: 69 01                        adc     #$01              ;carry is 0, so this is 1 or 2
7ec4: 20 2f 7a                     jsr     DrawDigit1        ;draw center/left mech digit
7ec7: ad 00 0c                     lda     DSW1              ;get it again
7eca: 29 0c                        and     #%00001100        ;right coin mech
7ecc: 4a                           lsr     A
7ecd: 4a                           lsr     A
7ece: aa                           tax
7ecf: bd c6 7f                     lda     right_mech_vals,x ;1/4/5/6
7ed2: 20 2f 7a                     jsr     DrawDigit1        ;draw it
                   ; Display any failed ROM checksums.
7ed5: a2 16                        ldx     #$16              ;vertical position
7ed7: 86 0b                        stx     ]row_offset
7ed9: a2 07                        ldx     #$07              ;8 ROM chips
7edb: b5 3d        :CheckLoop      lda     ]checksums,x      ;get the checksum
7edd: f0 29                        beq     :CheckOkay        ;zero, ROM is good
7edf: 85 00                        sta     ]bad_chk_val      ;save the value
7ee1: 86 0a                        stx     ]saved_index      ;preserve this
7ee3: 20 6a 7a                     jsr     VgCenter          ;beam to center
7ee6: a6 0b                        ldx     ]row_offset       ;get Y offset
7ee8: 8a                           txa                       ;move up 8 (x4) rows for next time
7ee9: 38                           sec
7eea: e9 08                        sbc     #$08
7eec: 85 0b                        sta     ]row_offset
7eee: a9 a0                        lda     #$a0
7ef0: 20 90 7a                     jsr     VgVec8            ;move to -384,(row_offset * 4)
7ef3: a5 0a                        lda     ]saved_index
7ef5: 20 2f 7a                     jsr     DrawDigit1        ;draw failing ROM slot (0-7)
7ef8: a9 06                        lda     #$06
7efa: a2 00                        ldx     #$00
7efc: 20 90 7a                     jsr     VgVec8            ;move to +24,+0
7eff: a9 00                        lda     #$00              ;digits from address $00 (the failed value)
7f01: a0 01                        ldy     #$01
7f03: 20 a0 7b                     jsr     DrawNDigits       ;draw it
7f06: a6 0a                        ldx     ]saved_index      ;get index
7f08: ca           :CheckOkay      dex
7f09: 10 d0                        bpl     :CheckLoop        ;more to do, branch
                   ; If the math box failed, show the failure code.
7f0b: 20 6a 7a                     jsr     VgCenter          ;move to center
7f0e: a9 60                        lda     #$60
7f10: a2 16                        ldx     #$16
7f12: 20 90 7a                     jsr     VgVec8            ;move to +384,+58
7f15: a5 0d                        lda     ]mb_fail_code     ;'E', 'H', or 'L'
7f17: f0 0e                        beq     :SkipGlyph2
7f19: 38                           sec
7f1a: e9 36                        sbc     #$36              ;convert ASCII to glyph number
7f1c: 0a                           asl     A                 ;double it for index
7f1d: a8                           tay
7f1e: b9 f0 33                     lda     vg_glyph_calls,y  ;add glyph to output
7f21: be f1 33                     ldx     vg_glyph_calls+1,y
7f24: 20 6e 7a                     jsr     VgOutputCmd
7f27: 20 1d 7a     :SkipGlyph2     jsr     VgFinishList      ;center beam and add HALT
7f2a: 8d 00 12     :StartVSM       sta     VEC_GEN_START     ;tell the VSM to start
7f2d: 24 0c                        bit     ]diag_step        ;are we doing the diag step tests?
7f2f: 10 4d                        bpl     IoTest            ;yes, go take care of that
7f31: 20 b9 7f                     jsr     CheckDSPress      ;did the diag step switch get pressed?
7f34: d0 4d                        bne     IoTest2           ;yes, start the tests
                   ; Make sounds when joysticks are moved, buttons are pressed, or coins are
                   ; inserted.  One tone is made when the button is pressed, a different tone when
                   ; the button is released.  A count of the number of selected inputs is kept so
                   ; we know which sound to make.
7f36: 8d 2b 18                     sta     POKEY_POTGO       ;scan inputs
7f39: a0 00                        ldy     #$00              ;init count to zero
7f3b: ad 28 18                     lda     POKEY_ALLPOT      ;get all inputs
7f3e: 29 7f                        and     #$7f              ;strip unused high bit
7f40: 4a           :InputLoop      lsr     A
7f41: 90 01                        bcc     :NotHit
7f43: c8                           iny                       ;button pressed or joystick moved, count++
7f44: 09 00        :NotHit         ora     #$00              ;set flags for A-reg
7f46: d0 f8                        bne     :InputLoop        ;loop until no more bits are set
                   ; Check coin activity.
7f48: ad 00 08                     lda     IN0
7f4b: 29 0f                        and     #$0f              ;coin activity
7f4d: 49 0f                        eor     #$0f              ;invert
7f4f: 4a           :CoinLoop       lsr     A
7f50: 90 01                        bcc     :NotSet
7f52: c8                           iny                       ;increase count
7f53: 09 00        :NotSet         ora     #$00              ;set flags for A-reg
7f55: d0 f8                        bne     :CoinLoop         ;loop until no more bits are set
7f57: c4 10                        cpy     ]prev_inputs      ;compare to last time
7f59: f0 08                        beq     :Same
7f5b: a9 a7                        lda     #$a7              ;volume
7f5d: 90 08                        bcc     :HiFreq           ;fewer set, use this
7f5f: a2 80                        ldx     #$80              ;freq=mid
7f61: d0 06                        bne     :SetSound         ;(always)

7f63: a9 00        :Same           lda     #$00              ;silence
7f65: f0 02                        beq     :SetSound         ;(always)

7f67: a2 20        :HiFreq         ldx     #$20              ;freq=hi
7f69: 84 10        :SetSound       sty     ]prev_inputs
7f6b: 8d 23 18                     sta     POKEY_AUDC2       ;set sound
7f6e: 8e 22 18                     stx     POKEY_AUDF2
7f71: ad 00 08                     lda     IN0               ;check switch state
7f74: 29 10                        and     #$10              ;self-test switch pressed?
7f76: d0 03                        bne     jmp_ResetSystem   ;no, do a soft reset
7f78: 4c 3c 7d     jmp_TestMathBox jmp     TestMathBox       ;still testing, loop around

7f7b: 4c cc 7a     jmp_ResetSystem jmp     ResetSystem

                   ; 
                   ; Fifth stage of the "diag step" sequence.  Exercises all I/O and math box
                   ; functions.
                   ; 
7f7e: 20 b9 7f     IoTest          jsr     CheckDSPress      ;did the diag step switch get pressed?
7f81: f0 f5                        beq     jmp_TestMathBox   ;no, keep looping around
7f83: e6 0c        IoTest2         inc     ]diag_step        ;switch pressed, advance to next step
7f85: a5 0c                        lda     ]diag_step
7f87: 29 fc                        and     #%11111100        ;see if it's 0-3
7f89: f0 ed                        beq     jmp_TestMathBox   ;yes, go do the line drawing
7f8b: a9 00        :IoLoop         lda     #$00              ;no, so this is test 4: exercise all the I/O
7f8d: 8d 00 14                     sta     WATCHDOG_CLEAR
7f90: 8d 40 18                     sta     DSOUND_CTRL
7f93: 8d 60 18                     sta     MB_SET_R0L
7f96: 8d 20 18                     sta     POKEY_AUDF1
7f99: ad 00 18                     lda     MB_STATUS
7f9c: ad 18 18                     lda     MB_RESULT_HI
7f9f: ad 10 18                     lda     MB_RESULT_LO
7fa2: a9 01                        lda     #$01              ;explosion (but we're muted)
7fa4: 8d 40 18                     sta     DSOUND_CTRL
7fa7: a2 1f                        ldx     #$1f              ;invoke all of the mathbox functions
7fa9: 18                           clc
7faa: 9d 60 18     :Loop           sta     MB_SET_R0L,x
7fad: 2a                           rol     A
7fae: ca                           dex
7faf: 10 f9                        bpl     :Loop
7fb1: 20 b9 7f                     jsr     CheckDSPress      ;check the switch
7fb4: f0 d5                        beq     :IoLoop           ;not pressed, keep going
7fb6: 4c cc 7a                     jmp     ResetSystem       ;start over

                   ; 
                   ; Checks to see if the diag step input has been grounded (switch pressed).
                   ; 
                   ; On exit:
                   ;   A-reg: $20 if input grounded but wasn't before
                   ;   P-flags set according to A-reg
                   ; 
7fb9: ad 00 08     CheckDSPress    lda     IN0               ;get current value
7fbc: a8                           tay                       ;save it
7fbd: 45 13                        eor     ]prev_diag_step   ;XOR with previous value (1 if changed)
7fbf: 25 13                        and     ]prev_diag_step   ;AND with prev (1 if changed and prev not pressed)
7fc1: 84 13                        sty     ]prev_diag_step   ;set prev = current
7fc3: 29 20                        and     #$20              ;mask off the uninteresting bits
7fc5: 60                           rts

                   ; 
                   ; Some data used by self test.
                   ; 
7fc6: 01 04 05 06  right_mech_vals .bulk   $01,$04,$05,$06
                   ; 
                   ; Commands for VSM test sequence.  For each of 4 tests, the code gets the length
                   ; in bytes from the length table, and then copies that many bytes (minus one) to
                   ; $2010.  A HALT instruction is added to the end of the sequence.
                   ; 
                   ; Among other things, the test confirms that a four-deep VSJR works.
                   ; 
                   vsm_test_seq_lens
7fca: 0d 11 17 2d                  .bulk   $0d,$11,$17,$2d   ;length of sequence, +1
7fce: 40 80        vsm_test_seq    .dd2    $8040             ;$2010 CNTR                            [test0]
7fd0: 80 1e                        .dd2    $1e80             ;$2012 VCTR dx=-512 dy=-384 in=0
7fd2: 00 1e                        .dd2    $1e00
7fd4: ff 02                        .dd2    $02ff             ;$2016 VCTR dx=+1023 dy=+767 in=14
7fd6: ff e3                        .dd2    $e3ff
7fd8: 40 80                        .dd2    $8040             ;$201a CNTR
7fda: 0f 51                        .dd2    $510f             ;$201c SVEC dx=+30 dy=+30 in=0         [test1]
7fdc: f1 4f                        .dd2    $4ff1             ;$201e SVEC dx=-30 dy=+30 in=14
7fde: 12 e0                        .dd2    $e012             ;$2020 VJMP a=$0012 ($2024)            [test2]
7fe0: 00 20                        .dd2    $2000             ;$2022 HALT
7fe2: f1 4f                        .dd2    $4ff1             ;$2024 SVEC dx=-30 dy=+30 in=14
7fe4: 16 a0                        .dd2    $a016             ;$2026 VJSR a=$0016 ($202c)            [test3]
7fe6: e0 51                        .dd2    $51e0             ;$2028 SVEC dx=+0 dy=-30 in=14
7fe8: 00 20                        .dd2    $2000             ;$202a HALT
7fea: 18 a0                        .dd2    $a018             ;$202c VJSR a=$0018 ($2030)
7fec: 00 c0                        .dd2    $c000             ;$202e VRTS
7fee: 1a a0                        .dd2    $a01a             ;$2030 VJSR a=$001a ($2034)
7ff0: 00 c0                        .dd2    $c000             ;$2032 VRTS
7ff2: 1c a0                        .dd2    $a01c             ;$2034 VJSR a=$001c ($2038)
7ff4: 00 c0                        .dd2    $c000             ;$2036 VRTS
7ff6: ef 40                        .dd2    $40ef             ;$2038 SVEC dx=+30 dy=+0 in=14
7ff8: 00 c0                        .dd2    $c000             ;$203a VRTS
                   ; 
                   ; Reset and interrupt vectors.  Note memory is mirrored, so these values also
                   ; appear at $fffa-ffff, where the 6502 expects to find them.
                   ; 
7ffa: 8f 55                        .dd2    HandleNMI         ;NMI vector
7ffc: cc 7a                        .dd2    ResetSystem       ;reset vector
7ffe: cc 7a                        .dd2    ResetSystem       ;IRQ vector
                                   .adrend ↑ ~$5000

                   ********************************************************************************
                   * Vector generator ROM.                                                        *
                   *                                                                              *
                   * The current version of SourceGen does not have a facility for decoding AVG   *
                   * commands into meaningful pseudo-opcodes.  See the web site associated with   *
                   * this project for the code used to generate the instruction comments.         *
                   ********************************************************************************
                   vis
                                   .addrs  $3000
3000: 00 00        vg_horizon_line .dd2    $0000             ;VCTR dx=-1536 dy=+0 in=6
3002: 00 7a                        .dd2    $7a00
3004: 00 c0                        .dd2    $c000             ;VRTS
                   ; 
                   ; VJSRs for eight-part landscape + moon.  Each landscape section is 512 units
                   ; wide, covering half of the screen.  At most 3 sections will be needed.
                   ; 
                   ; When the player faces angle 0, the reticle is just to the right of the moon. 
                   ; So at that angle, landscape 0 covers the left half of the screen, landscape 1
                   ; covers the right.  Every $20 units of rotation lines up with the next section.
                   ; 
                   ; Note the list is not terminated with a VRTS, so if you call here you will see
                   ; the first section twice.
                   ; 
3006: 0b a8        vg_landscape    .dd2    $a80b             ;VJSR a=$080b ($3016)
3008: 65 a8                        .dd2    $a865             ;VJSR a=$0865 ($30ca)
300a: 82 a8                        .dd2    $a882             ;VJSR a=$0882 ($3104)
300c: c1 a8                        .dd2    $a8c1             ;VJSR a=$08c1 ($3182)
300e: e2 a8                        .dd2    $a8e2             ;VJSR a=$08e2 ($31c4)
3010: f9 a8                        .dd2    $a8f9             ;VJSR a=$08f9 ($31f2)
3012: 17 a9                        .dd2    $a917             ;VJSR a=$0917 ($322e)
3014: 34 a9                        .dd2    $a934             ;VJSR a=$0934 ($3268)

                   vis
3016: 40 00                        .dd2    $0040             ;VCTR dx=+0 dy=+64 in=0 <<<
3018: 00 00                        .dd2    $0000
301a: e0 1f                        .dd2    $1fe0             ;VCTR dx=+32 dy=-32 in=6
301c: 20 60                        .dd2    $6020
301e: 18 5c                        .dd2    $5c18             ;SVEC dx=-16 dy=-8 in=0
3020: 28 00                        .dd2    $0028             ;VCTR dx=+80 dy=+40 in=6
3022: 50 60                        .dd2    $6050
3024: 00 00                        .dd2    $0000             ;VCTR dx=+32 dy=+0 in=6
3026: 20 60                        .dd2    $6020
3028: e0 1f                        .dd2    $1fe0             ;VCTR dx=+32 dy=-32 in=6
302a: 20 60                        .dd2    $6020
302c: 20 00                        .dd2    $0020             ;VCTR dx=-64 dy=+32 in=6
302e: c0 7f                        .dd2    $7fc0
3030: c0 1f                        .dd2    $1fc0             ;VCTR dx=+0 dy=-64 in=0
3032: 00 00                        .dd2    $0000
3034: 40 00                        .dd2    $0040             ;VCTR dx=+128 dy=+64 in=6
3036: 80 60                        .dd2    $6080
3038: c0 1f                        .dd2    $1fc0             ;VCTR dx=+64 dy=-64 in=6
303a: 40 60                        .dd2    $6040
303c: 00 00                        .dd2    $0000             ;VCTR dx=+32 dy=+0 in=0
303e: 20 00                        .dd2    $0020
3040: 20 00                        .dd2    $0020             ;VCTR dx=-64 dy=+32 in=6
3042: c0 7f                        .dd2    $7fc0
3044: 20 00                        .dd2    $0020             ;VCTR dx=-32 dy=+32 in=0
3046: e0 1f                        .dd2    $1fe0
3048: e0 1f                        .dd2    $1fe0             ;VCTR dx=+64 dy=-32 in=6
304a: 40 60                        .dd2    $6040
304c: f0 1f                        .dd2    $1ff0             ;VCTR dx=+64 dy=-16 in=6
304e: 40 60                        .dd2    $6040
3050: f0 1f                        .dd2    $1ff0             ;VCTR dx=+96 dy=-16 in=6
3052: 60 60                        .dd2    $6060
3054: a0 00                        .dd2    $00a0             ;VCTR dx=+48 dy=+160 in=0
3056: 30 00                        .dd2    $0030
3058: f4 1f                        .dd2    $1ff4             ;VCTR dx=+5 dy=-12 in=14
305a: 05 e0                        .dd2    $e005
305c: e0 5a                        .dd2    $5ae0             ;SVEC dx=+0 dy=-12 in=14
305e: fc 5a                        .dd2    $5afc             ;SVEC dx=-8 dy=-12 in=14
3060: fa 5a                        .dd2    $5afa             ;SVEC dx=-12 dy=-12 in=14
3062: fd 1f                        .dd2    $1ffd             ;VCTR dx=-12 dy=-3 in=14
3064: f4 ff                        .dd2    $fff4
3066: 03 00                        .dd2    $0003             ;VCTR dx=-12 dy=+3 in=14
3068: f4 ff                        .dd2    $fff4
306a: f7 1f                        .dd2    $1ff7             ;VCTR dx=+12 dy=-9 in=14
306c: 0c e0                        .dd2    $e00c
306e: fd 1f                        .dd2    $1ffd             ;VCTR dx=+12 dy=-3 in=14
3070: 0c e0                        .dd2    $e00c
3072: 03 00                        .dd2    $0003             ;VCTR dx=+12 dy=+3 in=14
3074: 0c e0                        .dd2    $e00c
3076: 09 00                        .dd2    $0009             ;VCTR dx=+12 dy=+9 in=14
3078: 0c e0                        .dd2    $e00c
307a: e3 46                        .dd2    $46e3             ;SVEC dx=+6 dy=+12 in=14
307c: e0 46                        .dd2    $46e0             ;SVEC dx=+0 dy=+12 in=14
307e: fe 46                        .dd2    $46fe             ;SVEC dx=-4 dy=+12 in=14
3080: 0c 00                        .dd2    $000c             ;VCTR dx=-11 dy=+12 in=14
3082: f5 ff                        .dd2    $fff5
3084: 03 00                        .dd2    $0003             ;VCTR dx=-13 dy=+3 in=4
3086: f3 5f                        .dd2    $5ff3
3088: fc 1f                        .dd2    $1ffc             ;VCTR dx=-15 dy=-4 in=4
308a: f1 5f                        .dd2    $5ff1
308c: 5b 5c                        .dd2    $5c5b             ;SVEC dx=-10 dy=-8 in=4
308e: f5 1f                        .dd2    $1ff5             ;VCTR dx=-5 dy=-11 in=4
3090: fb 5f                        .dd2    $5ffb
3092: f3 1f                        .dd2    $1ff3             ;VCTR dx=-2 dy=-13 in=4
3094: fe 5f                        .dd2    $5ffe
3096: f1 1f                        .dd2    $1ff1             ;VCTR dx=+6 dy=-15 in=4
3098: 06 40                        .dd2    $4006
309a: 03 00                        .dd2    $0003             ;VCTR dx=+27 dy=+3 in=0
309c: 1b 00                        .dd2    $001b
309e: a0 5e                        .dd2    $5ea0             ;SVEC dx=+0 dy=-4 in=10
30a0: ff 1f                        .dd2    $1fff             ;VCTR dx=+6 dy=-1 in=10
30a2: 06 a0                        .dd2    $a006
30a4: 0b 00                        .dd2    $000b             ;VCTR dx=+6 dy=+11 in=10
30a6: 06 a0                        .dd2    $a006
30a8: ff 1f                        .dd2    $1fff             ;VCTR dx=-4 dy=-1 in=10
30aa: fc bf                        .dd2    $bffc
30ac: 02 00                        .dd2    $0002             ;VCTR dx=-1 dy=+2 in=10
30ae: ff bf                        .dd2    $bfff
30b0: 09 4a                        .dd2    $4a09             ;SVEC dx=+18 dy=+20 in=0
30b2: fd 1f                        .dd2    $1ffd             ;VCTR dx=-3 dy=-3 in=10
30b4: fd bf                        .dd2    $bffd
30b6: 03 00                        .dd2    $0003             ;VCTR dx=+1 dy=+3 in=10
30b8: 01 a0                        .dd2    $a001
30ba: 01 00                        .dd2    $0001             ;VCTR dx=-3 dy=+1 in=10
30bc: fd bf                        .dd2    $bffd
30be: a0 43                        .dd2    $43a0             ;SVEC dx=+0 dy=+6 in=10
30c0: 01 00                        .dd2    $0001             ;VCTR dx=+1 dy=+1 in=4
30c2: 01 40                        .dd2    $4001
30c4: 6a 1f                        .dd2    $1f6a             ;VCTR dx=+7 dy=-150 in=0
30c6: 07 00                        .dd2    $0007
30c8: 00 c0                        .dd2    $c000             ;VRTS

                   vis
30ca: 00 00                        .dd2    $0000             ;VCTR dx=+32 dy=+0 in=0 <<<
30cc: 20 00                        .dd2    $0020
30ce: 30 00                        .dd2    $0030             ;VCTR dx=+64 dy=+48 in=6
30d0: 40 60                        .dd2    $6040
30d2: d0 1f                        .dd2    $1fd0             ;VCTR dx=+32 dy=-48 in=6
30d4: 20 60                        .dd2    $6020
30d6: 30 00                        .dd2    $0030             ;VCTR dx=-32 dy=+48 in=0
30d8: e0 1f                        .dd2    $1fe0
30da: f0 1f                        .dd2    $1ff0             ;VCTR dx=+32 dy=-16 in=6
30dc: 20 60                        .dd2    $6020
30de: 20 00                        .dd2    $0020             ;VCTR dx=+64 dy=+32 in=6
30e0: 40 60                        .dd2    $6040
30e2: c0 1f                        .dd2    $1fc0             ;VCTR dx=+160 dy=-64 in=6
30e4: a0 60                        .dd2    $60a0
30e6: 20 00                        .dd2    $0020             ;VCTR dx=-128 dy=+32 in=6
30e8: 80 7f                        .dd2    $7f80
30ea: 20 00                        .dd2    $0020             ;VCTR dx=-32 dy=+32 in=6
30ec: e0 7f                        .dd2    $7fe0
30ee: e0 1f                        .dd2    $1fe0             ;VCTR dx=-64 dy=-32 in=0
30f0: c0 1f                        .dd2    $1fc0
30f2: e0 1f                        .dd2    $1fe0             ;VCTR dx=+96 dy=-32 in=6
30f4: 60 60                        .dd2    $6060
30f6: 00 00                        .dd2    $0000             ;VCTR dx=+128 dy=+0 in=0
30f8: 80 00                        .dd2    $0080
30fa: 20 00                        .dd2    $0020             ;VCTR dx=+160 dy=+32 in=6
30fc: a0 60                        .dd2    $60a0
30fe: e0 1f                        .dd2    $1fe0             ;VCTR dx=+0 dy=-32 in=0
3100: 00 00                        .dd2    $0000
3102: 00 c0                        .dd2    $c000             ;VRTS

                   vis
3104: 20 00                        .dd2    $0020             ;VCTR dx=+0 dy=+32 in=0 <<<
3106: 00 00                        .dd2    $0000
3108: 00 00                        .dd2    $0000             ;VCTR dx=+64 dy=+0 in=6
310a: 40 60                        .dd2    $6040
310c: e0 1f                        .dd2    $1fe0             ;VCTR dx=+0 dy=-32 in=0
310e: 00 00                        .dd2    $0000
3110: 20 00                        .dd2    $0020             ;VCTR dx=-64 dy=+32 in=6
3112: c0 7f                        .dd2    $7fc0
3114: 00 00                        .dd2    $0000             ;VCTR dx=+64 dy=+0 in=0
3116: 40 00                        .dd2    $0040
3118: 20 00                        .dd2    $0020             ;VCTR dx=+64 dy=+32 in=6
311a: 40 60                        .dd2    $6040
311c: e0 1f                        .dd2    $1fe0             ;VCTR dx=-64 dy=-32 in=0
311e: c0 1f                        .dd2    $1fc0
3120: e0 1f                        .dd2    $1fe0             ;VCTR dx=+96 dy=-32 in=6
3122: 60 60                        .dd2    $6060
3124: 20 00                        .dd2    $0020             ;VCTR dx=+32 dy=+32 in=6
3126: 20 60                        .dd2    $6020
3128: 20 00                        .dd2    $0020             ;VCTR dx=-64 dy=+32 in=6
312a: c0 7f                        .dd2    $7fc0
312c: 00 00                        .dd2    $0000             ;VCTR dx=+32 dy=+0 in=6
312e: 20 60                        .dd2    $6020
3130: e0 1f                        .dd2    $1fe0             ;VCTR dx=+32 dy=-32 in=6
3132: 20 60                        .dd2    $6020
3134: 10 00                        .dd2    $0010             ;VCTR dx=+32 dy=+16 in=6
3136: 20 60                        .dd2    $6020
3138: f0 1f                        .dd2    $1ff0             ;VCTR dx=+32 dy=-16 in=6
313a: 20 60                        .dd2    $6020
313c: e0 1f                        .dd2    $1fe0             ;VCTR dx=-96 dy=-32 in=6
313e: a0 7f                        .dd2    $7fa0
3140: 20 00                        .dd2    $0020             ;VCTR dx=+96 dy=+32 in=0
3142: 60 00                        .dd2    $0060
3144: 20 00                        .dd2    $0020             ;VCTR dx=+64 dy=+32 in=6
3146: 40 60                        .dd2    $6040
3148: e0 1f                        .dd2    $1fe0             ;VCTR dx=+64 dy=-32 in=6
314a: 40 60                        .dd2    $6040
314c: 10 00                        .dd2    $0010             ;VCTR dx=-96 dy=+16 in=0
314e: a0 1f                        .dd2    $1fa0
3150: f0 1f                        .dd2    $1ff0             ;VCTR dx=+32 dy=-16 in=6
3152: 20 60                        .dd2    $6020
3154: 10 00                        .dd2    $0010             ;VCTR dx=+32 dy=+16 in=6
3156: 20 60                        .dd2    $6020
3158: d0 1f                        .dd2    $1fd0             ;VCTR dx=+0 dy=-48 in=0
315a: 00 00                        .dd2    $0000
315c: 08 00                        .dd2    $0008             ;VCTR dx=+96 dy=+8 in=6
315e: 60 60                        .dd2    $6060
3160: 18 00                        .dd2    $0018             ;VCTR dx=-64 dy=+24 in=6
3162: c0 7f                        .dd2    $7fc0
3164: 10 00                        .dd2    $0010             ;VCTR dx=+32 dy=+16 in=6
3166: 20 60                        .dd2    $6020
3168: f0 1f                        .dd2    $1ff0             ;VCTR dx=+64 dy=-16 in=6
316a: 40 60                        .dd2    $6040
316c: e8 1f                        .dd2    $1fe8             ;VCTR dx=-32 dy=-24 in=6
316e: e0 7f                        .dd2    $7fe0
3170: 28 00                        .dd2    $0028             ;VCTR dx=-32 dy=+40 in=6
3172: e0 7f                        .dd2    $7fe0
3174: f0 1f                        .dd2    $1ff0             ;VCTR dx=+64 dy=-16 in=0
3176: 40 00                        .dd2    $0040
3178: 00 00                        .dd2    $0000             ;VCTR dx=+32 dy=+0 in=6
317a: 20 60                        .dd2    $6020
317c: e0 1f                        .dd2    $1fe0             ;VCTR dx=+0 dy=-32 in=0
317e: 00 00                        .dd2    $0000
3180: 00 c0                        .dd2    $c000             ;VRTS

                   vis
3182: 20 00                        .dd2    $0020             ;VCTR dx=+0 dy=+32 in=0 <<<
3184: 00 00                        .dd2    $0000
3186: e0 1f                        .dd2    $1fe0             ;VCTR dx=+64 dy=-32 in=6
3188: 40 60                        .dd2    $6040
318a: 20 00                        .dd2    $0020             ;VCTR dx=-64 dy=+32 in=0
318c: c0 1f                        .dd2    $1fc0
318e: e0 1f                        .dd2    $1fe0             ;VCTR dx=+128 dy=-32 in=6
3190: 80 60                        .dd2    $6080
3192: 00 00                        .dd2    $0000             ;VCTR dx=+160 dy=+0 in=0
3194: a0 00                        .dd2    $00a0
3196: 20 00                        .dd2    $0020             ;VCTR dx=+96 dy=+32 in=6
3198: 60 60                        .dd2    $6060
319a: e0 1f                        .dd2    $1fe0             ;VCTR dx=+32 dy=-32 in=6
319c: 20 60                        .dd2    $6020
319e: 20 00                        .dd2    $0020             ;VCTR dx=-32 dy=+32 in=0
31a0: e0 1f                        .dd2    $1fe0
31a2: 20 00                        .dd2    $0020             ;VCTR dx=+32 dy=+32 in=6
31a4: 20 60                        .dd2    $6020
31a6: c0 1f                        .dd2    $1fc0             ;VCTR dx=+32 dy=-64 in=6
31a8: 20 60                        .dd2    $6020
31aa: 00 00                        .dd2    $0000             ;VCTR dx=+32 dy=+0 in=0
31ac: 20 00                        .dd2    $0020
31ae: 40 00                        .dd2    $0040             ;VCTR dx=-64 dy=+64 in=6
31b0: c0 7f                        .dd2    $7fc0
31b2: e0 1f                        .dd2    $1fe0             ;VCTR dx=+32 dy=-32 in=0
31b4: 20 00                        .dd2    $0020
31b6: 20 00                        .dd2    $0020             ;VCTR dx=+32 dy=+32 in=6
31b8: 20 60                        .dd2    $6020
31ba: e0 1f                        .dd2    $1fe0             ;VCTR dx=+32 dy=-32 in=6
31bc: 20 60                        .dd2    $6020
31be: e0 1f                        .dd2    $1fe0             ;VCTR dx=+0 dy=-32 in=0
31c0: 00 00                        .dd2    $0000
31c2: 00 c0                        .dd2    $c000             ;VRTS

                   vis
31c4: 20 00                        .dd2    $0020             ;VCTR dx=+0 dy=+32 in=0 <<<
31c6: 00 00                        .dd2    $0000
31c8: 20 00                        .dd2    $0020             ;VCTR dx=+64 dy=+32 in=6
31ca: 40 60                        .dd2    $6040
31cc: c0 1f                        .dd2    $1fc0             ;VCTR dx=+32 dy=-64 in=6
31ce: 20 60                        .dd2    $6020
31d0: 00 00                        .dd2    $0000             ;VCTR dx=+32 dy=+0 in=0
31d2: 20 00                        .dd2    $0020
31d4: 40 00                        .dd2    $0040             ;VCTR dx=-64 dy=+64 in=6
31d6: c0 7f                        .dd2    $7fc0
31d8: e0 1f                        .dd2    $1fe0             ;VCTR dx=+32 dy=-32 in=0
31da: 20 00                        .dd2    $0020
31dc: 10 00                        .dd2    $0010             ;VCTR dx=+32 dy=+16 in=6
31de: 20 60                        .dd2    $6020
31e0: d0 1f                        .dd2    $1fd0             ;VCTR dx=+96 dy=-48 in=6
31e2: 60 60                        .dd2    $6060
31e4: 00 00                        .dd2    $0000             ;VCTR dx=+128 dy=+0 in=0
31e6: 80 00                        .dd2    $0080
31e8: 20 00                        .dd2    $0020             ;VCTR dx=+160 dy=+32 in=6
31ea: a0 60                        .dd2    $60a0
31ec: e0 1f                        .dd2    $1fe0             ;VCTR dx=+0 dy=-32 in=0
31ee: 00 00                        .dd2    $0000
31f0: 00 c0                        .dd2    $c000             ;VRTS

                   vis
31f2: 20 00                        .dd2    $0020             ;VCTR dx=+0 dy=+32 in=0 <<<
31f4: 00 00                        .dd2    $0000
31f6: 00 00                        .dd2    $0000             ;VCTR dx=+64 dy=+0 in=6
31f8: 40 60                        .dd2    $6040
31fa: e0 1f                        .dd2    $1fe0             ;VCTR dx=+224 dy=-32 in=6
31fc: e0 60                        .dd2    $60e0
31fe: 30 00                        .dd2    $0030             ;VCTR dx=-32 dy=+48 in=6
3200: e0 7f                        .dd2    $7fe0
3202: e0 1f                        .dd2    $1fe0             ;VCTR dx=-64 dy=-32 in=6
3204: c0 7f                        .dd2    $7fc0
3206: f0 1f                        .dd2    $1ff0             ;VCTR dx=+96 dy=-16 in=0
3208: 60 00                        .dd2    $0060
320a: 40 00                        .dd2    $0040             ;VCTR dx=+96 dy=+64 in=6
320c: 60 60                        .dd2    $6060
320e: d8 1f                        .dd2    $1fd8             ;VCTR dx=+64 dy=-40 in=6
3210: 40 60                        .dd2    $6040
3212: 18 54                        .dd2    $5418             ;SVEC dx=-16 dy=-24 in=0
3214: 60 00                        .dd2    $0060             ;VCTR dx=+64 dy=+96 in=6
3216: 40 60                        .dd2    $6040
3218: f9 1f                        .dd2    $1ff9             ;VCTR dx=+3 dy=-7 in=6
321a: 03 60                        .dd2    $6003
321c: 05 00                        .dd2    $0005             ;VCTR dx=+5 dy=+5 in=6
321e: 05 60                        .dd2    $6005
3220: fa 1f                        .dd2    $1ffa             ;VCTR dx=+3 dy=-6 in=6
3222: 03 60                        .dd2    $6003
3224: 08 00                        .dd2    $0008             ;VCTR dx=+5 dy=+8 in=6
3226: 05 60                        .dd2    $6005
3228: a0 1f                        .dd2    $1fa0             ;VCTR dx=+0 dy=-96 in=0
322a: 00 00                        .dd2    $0000
322c: 00 c0                        .dd2    $c000             ;VRTS

                   vis
322e: 60 00                        .dd2    $0060             ;VCTR dx=+0 dy=+96 in=0 <<<
3230: 00 00                        .dd2    $0000
3232: a0 1f                        .dd2    $1fa0             ;VCTR dx=+64 dy=-96 in=6
3234: 40 60                        .dd2    $6040
3236: 40 00                        .dd2    $0040             ;VCTR dx=+96 dy=+64 in=6
3238: 60 60                        .dd2    $6060
323a: c0 1f                        .dd2    $1fc0             ;VCTR dx=+64 dy=-64 in=6
323c: 40 60                        .dd2    $6040
323e: 00 00                        .dd2    $0000             ;VCTR dx=+64 dy=+0 in=0
3240: 40 00                        .dd2    $0040
3242: 40 00                        .dd2    $0040             ;VCTR dx=-128 dy=+64 in=6
3244: 80 7f                        .dd2    $7f80
3246: e0 1f                        .dd2    $1fe0             ;VCTR dx=+64 dy=-32 in=0
3248: 40 00                        .dd2    $0040
324a: 20 00                        .dd2    $0020             ;VCTR dx=+64 dy=+32 in=6
324c: 40 60                        .dd2    $6040
324e: c0 1f                        .dd2    $1fc0             ;VCTR dx=+128 dy=-64 in=6
3250: 80 60                        .dd2    $6080
3252: 20 00                        .dd2    $0020             ;VCTR dx=-64 dy=+32 in=0
3254: c0 1f                        .dd2    $1fc0
3256: 10 00                        .dd2    $0010             ;VCTR dx=+64 dy=+16 in=6
3258: 40 60                        .dd2    $6040
325a: d0 1f                        .dd2    $1fd0             ;VCTR dx=+32 dy=-48 in=6
325c: 20 60                        .dd2    $6020
325e: 30 00                        .dd2    $0030             ;VCTR dx=-32 dy=+48 in=0
3260: e0 1f                        .dd2    $1fe0
3262: d0 1f                        .dd2    $1fd0             ;VCTR dx=+96 dy=-48 in=6
3264: 60 60                        .dd2    $6060
3266: 00 c0                        .dd2    $c000             ;VRTS

                   vis
3268: 00 00                        .dd2    $0000             ;VCTR dx=+192 dy=+0 in=0 <<<
326a: c0 00                        .dd2    $00c0
326c: 20 00                        .dd2    $0020             ;VCTR dx=+224 dy=+32 in=6
326e: e0 60                        .dd2    $60e0
3270: e0 1f                        .dd2    $1fe0             ;VCTR dx=+64 dy=-32 in=6
3272: 40 60                        .dd2    $6040
3274: 10 00                        .dd2    $0010             ;VCTR dx=-32 dy=+16 in=0
3276: e0 1f                        .dd2    $1fe0
3278: 30 00                        .dd2    $0030             ;VCTR dx=+64 dy=+48 in=6
327a: 40 60                        .dd2    $6040
327c: c0 1f                        .dd2    $1fc0             ;VCTR dx=+0 dy=-64 in=0
327e: 00 00                        .dd2    $0000
3280: 00 c0                        .dd2    $c000             ;VRTS

                   ; 
                   ; Character glyphs.  Each glyph fits in a 16x24 cell whose origin is the bottom-
                   ; left corner.  The beam will be left at the initial Y coordinate, 8 units to
                   ; the right of the character cell.
                   ; 
                   vis
3282: c0 48                        .dd2    $48c0             ;SVEC dx=+0 dy=+16 in=12 <<<
3284: c4 44                        .dd2    $44c4             ;SVEC dx=+8 dy=+8 in=12
3286: c4 5c                        .dd2    $5cc4             ;SVEC dx=+8 dy=-8 in=12
3288: c0 58                        .dd2    $58c0             ;SVEC dx=+0 dy=-16 in=12
328a: 18 44                        .dd2    $4418             ;SVEC dx=-16 dy=+8 in=0
328c: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12
328e: 04 5c                        .dd2    $5c04             ;SVEC dx=+8 dy=-8 in=0
3290: 00 c0                        .dd2    $c000             ;VRTS

                   vis
3292: c0 4c                        .dd2    $4cc0             ;SVEC dx=+0 dy=+24 in=12 <<<
3294: c6 40                        .dd2    $40c6             ;SVEC dx=+12 dy=+0 in=12
3296: c2 5e                        .dd2    $5ec2             ;SVEC dx=+4 dy=-4 in=12
3298: c0 5e                        .dd2    $5ec0             ;SVEC dx=+0 dy=-4 in=12
329a: de 5e                        .dd2    $5ede             ;SVEC dx=-4 dy=-4 in=12
329c: da 40                        .dd2    $40da             ;SVEC dx=-12 dy=+0 in=12
329e: 06 40                        .dd2    $4006             ;SVEC dx=+12 dy=+0 in=0
32a0: c2 5e                        .dd2    $5ec2             ;SVEC dx=+4 dy=-4 in=12
32a2: c0 5e                        .dd2    $5ec0             ;SVEC dx=+0 dy=-4 in=12
32a4: de 5e                        .dd2    $5ede             ;SVEC dx=-4 dy=-4 in=12
32a6: da 40                        .dd2    $40da             ;SVEC dx=-12 dy=+0 in=12
32a8: 93 e9                        .dd2    $e993             ;VJMP a=$0993 ($3326)

                   vis
32aa: c0 4c                        .dd2    $4cc0             ;SVEC dx=+0 dy=+24 in=12 <<<
32ac: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12
32ae: 18 54                        .dd2    $5418             ;SVEC dx=-16 dy=-24 in=0
32b0: d6 e9                        .dd2    $e9d6             ;VJMP a=$09d6 ($33ac)

                   vis
32b2: c0 4c                        .dd2    $4cc0             ;SVEC dx=+0 dy=+24 in=12 <<<
32b4: c4 40                        .dd2    $40c4             ;SVEC dx=+8 dy=+0 in=12
32b6: c4 5c                        .dd2    $5cc4             ;SVEC dx=+8 dy=-8 in=12
32b8: c0 5c                        .dd2    $5cc0             ;SVEC dx=+0 dy=-8 in=12
32ba: dc 5c                        .dd2    $5cdc             ;SVEC dx=-8 dy=-8 in=12
32bc: dc 40                        .dd2    $40dc             ;SVEC dx=-8 dy=+0 in=12
32be: 93 e9                        .dd2    $e993             ;VJMP a=$0993 ($3326)

                   vis
32c0: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12 <<<
32c2: 18 40                        .dd2    $4018             ;SVEC dx=-16 dy=+0 in=0

                   vis
32c4: c0 4c                        .dd2    $4cc0             ;SVEC dx=+0 dy=+24 in=12 <<<
32c6: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12
32c8: 1e 5a                        .dd2    $5a1e             ;SVEC dx=-4 dy=-12 in=0
32ca: da 40                        .dd2    $40da             ;SVEC dx=-12 dy=+0 in=12
32cc: 0c 5a                        .dd2    $5a0c             ;SVEC dx=+24 dy=-12 in=0 <<<
32ce: 00 c0                        .dd2    $c000             ;VRTS

                   vis
32d0: c0 4c                        .dd2    $4cc0             ;SVEC dx=+0 dy=+24 in=12 <<<
32d2: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12
32d4: c0 5c                        .dd2    $5cc0             ;SVEC dx=+0 dy=-8 in=12
32d6: 1c 5c                        .dd2    $5c1c             ;SVEC dx=-8 dy=-8 in=0
32d8: c4 40                        .dd2    $40c4             ;SVEC dx=+8 dy=+0 in=12
32da: c0 5c                        .dd2    $5cc0             ;SVEC dx=+0 dy=-8 in=12
32dc: 92 e9                        .dd2    $e992             ;VJMP a=$0992 ($3324)

                   vis
32de: c0 4c                        .dd2    $4cc0             ;SVEC dx=+0 dy=+24 in=12 <<<
32e0: 00 5a                        .dd2    $5a00             ;SVEC dx=+0 dy=-12 in=0
32e2: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12
32e4: 00 46                        .dd2    $4600             ;SVEC dx=+0 dy=+12 in=0
32e6: ef e9                        .dd2    $e9ef             ;VJMP a=$09ef ($33de)

                   vis
32e8: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12 <<<
32ea: 18 4c                        .dd2    $4c18             ;SVEC dx=-16 dy=+24 in=0
32ec: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12 <<<
32ee: 1c 40                        .dd2    $401c             ;SVEC dx=-8 dy=+0 in=0
32f0: c0 54                        .dd2    $54c0             ;SVEC dx=+0 dy=-24 in=12 <<<
32f2: 08 40                        .dd2    $4008             ;SVEC dx=+16 dy=+0 in=0
32f4: 00 c0                        .dd2    $c000             ;VRTS

                   vis
32f6: 00 44                        .dd2    $4400             ;SVEC dx=+0 dy=+8 in=0 <<<
32f8: c4 5c                        .dd2    $5cc4             ;SVEC dx=+8 dy=-8 in=12
32fa: c4 40                        .dd2    $40c4             ;SVEC dx=+8 dy=+0 in=12
32fc: b4 e9                        .dd2    $e9b4             ;VJMP a=$09b4 ($3368)

                   vis
32fe: c0 4c                        .dd2    $4cc0             ;SVEC dx=+0 dy=+24 in=12 <<<
3300: 06 40                        .dd2    $4006             ;SVEC dx=+12 dy=+0 in=0
3302: da 5a                        .dd2    $5ada             ;SVEC dx=-12 dy=-12 in=12
3304: c6 5a                        .dd2    $5ac6             ;SVEC dx=+12 dy=-12 in=12
3306: 06 40                        .dd2    $4006             ;SVEC dx=+12 dy=+0 in=0
3308: 00 c0                        .dd2    $c000             ;VRTS

                   vis
330a: 00 4c                        .dd2    $4c00             ;SVEC dx=+0 dy=+24 in=0 <<<
330c: c0 54                        .dd2    $54c0             ;SVEC dx=+0 dy=-24 in=12
330e: d6 e9                        .dd2    $e9d6             ;VJMP a=$09d6 ($33ac)

                   vis
3310: c0 4c                        .dd2    $4cc0             ;SVEC dx=+0 dy=+24 in=12 <<<
3312: c4 5c                        .dd2    $5cc4             ;SVEC dx=+8 dy=-8 in=12
3314: c4 44                        .dd2    $44c4             ;SVEC dx=+8 dy=+8 in=12
3316: ef e9                        .dd2    $e9ef             ;VJMP a=$09ef ($33de)

                   vis
3318: c0 4c                        .dd2    $4cc0             ;SVEC dx=+0 dy=+24 in=12 <<<
331a: c8 54                        .dd2    $54c8             ;SVEC dx=+16 dy=-24 in=12
331c: b4 e9                        .dd2    $e9b4             ;VJMP a=$09b4 ($3368)

                   ; 'O' and '0'.
                   vis
331e: c0 4c                        .dd2    $4cc0             ;SVEC dx=+0 dy=+24 in=12 <<<
3320: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12
3322: c0 54                        .dd2    $54c0             ;SVEC dx=+0 dy=-24 in=12
3324: d8 40                        .dd2    $40d8             ;SVEC dx=-16 dy=+0 in=12 <<<

                   ; Space character.
                   vis
3326: 0c 40                        .dd2    $400c             ;SVEC dx=+24 dy=+0 in=0 <<<
3328: 00 c0                        .dd2    $c000             ;VRTS

                   vis
332a: c0 4c                        .dd2    $4cc0             ;SVEC dx=+0 dy=+24 in=12 <<<
332c: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12
332e: c0 5a                        .dd2    $5ac0             ;SVEC dx=+0 dy=-12 in=12
3330: d8 40                        .dd2    $40d8             ;SVEC dx=-16 dy=+0 in=12
3332: 66 e9                        .dd2    $e966             ;VJMP a=$0966 ($32cc)

                   vis
3334: c0 4c                        .dd2    $4cc0             ;SVEC dx=+0 dy=+24 in=12 <<<
3336: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12
3338: c0 58                        .dd2    $58c0             ;SVEC dx=+0 dy=-16 in=12
333a: dc 5c                        .dd2    $5cdc             ;SVEC dx=-8 dy=-8 in=12
333c: dc 40                        .dd2    $40dc             ;SVEC dx=-8 dy=+0 in=12
333e: 04 44                        .dd2    $4404             ;SVEC dx=+8 dy=+8 in=0
3340: c4 5c                        .dd2    $5cc4             ;SVEC dx=+8 dy=-8 in=12
3342: d7 e9                        .dd2    $e9d7             ;VJMP a=$09d7 ($33ae)

                   vis
3344: c0 4c                        .dd2    $4cc0             ;SVEC dx=+0 dy=+24 in=12 <<<
3346: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12
3348: c0 5a                        .dd2    $5ac0             ;SVEC dx=+0 dy=-12 in=12
334a: d8 40                        .dd2    $40d8             ;SVEC dx=-16 dy=+0 in=12
334c: 02 40                        .dd2    $4002             ;SVEC dx=+4 dy=+0 in=0
334e: c6 5a                        .dd2    $5ac6             ;SVEC dx=+12 dy=-12 in=12
3350: d7 e9                        .dd2    $e9d7             ;VJMP a=$09d7 ($33ae)

                   ; 'S' and '5'.
                   vis
3352: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12 <<<
3354: c0 46                        .dd2    $46c0             ;SVEC dx=+0 dy=+12 in=12
3356: d8 40                        .dd2    $40d8             ;SVEC dx=-16 dy=+0 in=12
3358: c0 46                        .dd2    $46c0             ;SVEC dx=+0 dy=+12 in=12
335a: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12
335c: b5 e9                        .dd2    $e9b5             ;VJMP a=$09b5 ($336a)

                   vis
335e: 00 4c                        .dd2    $4c00             ;SVEC dx=+0 dy=+24 in=0 <<<
3360: 76 e9                        .dd2    $e976             ;VJMP a=$0976 ($32ec)

                   vis
3362: 00 4c                        .dd2    $4c00             ;SVEC dx=+0 dy=+24 in=0 <<<
3364: c0 54                        .dd2    $54c0             ;SVEC dx=+0 dy=-24 in=12
3366: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12
3368: c0 4c                        .dd2    $4cc0             ;SVEC dx=+0 dy=+24 in=12 <<<
336a: 04 54                        .dd2    $5404             ;SVEC dx=+8 dy=-24 in=0 <<<
336c: 00 c0                        .dd2    $c000             ;VRTS

                   vis
336e: 00 4c                        .dd2    $4c00             ;SVEC dx=+0 dy=+24 in=0 <<<
3370: c4 54                        .dd2    $54c4             ;SVEC dx=+8 dy=-24 in=12
3372: c4 4c                        .dd2    $4cc4             ;SVEC dx=+8 dy=+24 in=12
3374: b5 e9                        .dd2    $e9b5             ;VJMP a=$09b5 ($336a)

                   vis
3376: 00 4c                        .dd2    $4c00             ;SVEC dx=+0 dy=+24 in=0 <<<
3378: c0 54                        .dd2    $54c0             ;SVEC dx=+0 dy=-24 in=12
337a: c4 44                        .dd2    $44c4             ;SVEC dx=+8 dy=+8 in=12
337c: c4 5c                        .dd2    $5cc4             ;SVEC dx=+8 dy=-8 in=12
337e: b4 e9                        .dd2    $e9b4             ;VJMP a=$09b4 ($3368)

                   vis
3380: c8 4c                        .dd2    $4cc8             ;SVEC dx=+16 dy=+24 in=12 <<<
3382: 18 40                        .dd2    $4018             ;SVEC dx=-16 dy=+0 in=0
3384: c8 54                        .dd2    $54c8             ;SVEC dx=+16 dy=-24 in=12
3386: f0 e9                        .dd2    $e9f0             ;VJMP a=$09f0 ($33e0)

                   vis
3388: 04 40                        .dd2    $4004             ;SVEC dx=+8 dy=+0 in=0 <<<
338a: c0 48                        .dd2    $48c0             ;SVEC dx=+0 dy=+16 in=12
338c: dc 44                        .dd2    $44dc             ;SVEC dx=-8 dy=+8 in=12
338e: 08 40                        .dd2    $4008             ;SVEC dx=+16 dy=+0 in=0
3390: dc 5c                        .dd2    $5cdc             ;SVEC dx=-8 dy=-8 in=12
3392: 08 58                        .dd2    $5808             ;SVEC dx=+16 dy=-16 in=0
3394: 00 c0                        .dd2    $c000             ;VRTS

                   vis
3396: 00 4c                        .dd2    $4c00             ;SVEC dx=+0 dy=+24 in=0 <<<
3398: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12
339a: d8 54                        .dd2    $54d8             ;SVEC dx=-16 dy=-24 in=12
339c: d6 e9                        .dd2    $e9d6             ;VJMP a=$09d6 ($33ac)

                   vis
339e: 04 4c                        .dd2    $4c04             ;SVEC dx=+8 dy=+24 in=0 <<<
33a0: 78 e9                        .dd2    $e978             ;VJMP a=$0978 ($32f0)

                   vis
33a2: 00 4c                        .dd2    $4c00             ;SVEC dx=+0 dy=+24 in=0 <<<
33a4: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12
33a6: c0 5a                        .dd2    $5ac0             ;SVEC dx=+0 dy=-12 in=12
33a8: d8 40                        .dd2    $40d8             ;SVEC dx=-16 dy=+0 in=12
33aa: c0 5a                        .dd2    $5ac0             ;SVEC dx=+0 dy=-12 in=12

                   vis
33ac: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12 <<<
33ae: 04 40                        .dd2    $4004             ;SVEC dx=+8 dy=+0 in=0 <<<
33b0: 00 c0                        .dd2    $c000             ;VRTS

                   vis
33b2: 00 4c                        .dd2    $4c00             ;SVEC dx=+0 dy=+24 in=0 <<<
33b4: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12 <<<
33b6: c0 54                        .dd2    $54c0             ;SVEC dx=+0 dy=-24 in=12
33b8: d8 40                        .dd2    $40d8             ;SVEC dx=-16 dy=+0 in=12
33ba: 00 46                        .dd2    $4600             ;SVEC dx=+0 dy=+12 in=0
33bc: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12
33be: 04 5a                        .dd2    $5a04             ;SVEC dx=+8 dy=-12 in=0
33c0: 00 c0                        .dd2    $c000             ;VRTS

                   vis
33c2: 00 4c                        .dd2    $4c00             ;SVEC dx=+0 dy=+24 in=0 <<<
33c4: c0 5a                        .dd2    $5ac0             ;SVEC dx=+0 dy=-12 in=12
33c6: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12
33c8: 00 46                        .dd2    $4600             ;SVEC dx=+0 dy=+12 in=0
33ca: ef e9                        .dd2    $e9ef             ;VJMP a=$09ef ($33de)

                   vis
33cc: 00 46                        .dd2    $4600             ;SVEC dx=+0 dy=+12 in=0 <<<
33ce: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12
33d0: c0 5a                        .dd2    $5ac0             ;SVEC dx=+0 dy=-12 in=12
33d2: d8 40                        .dd2    $40d8             ;SVEC dx=-16 dy=+0 in=12
33d4: c0 4c                        .dd2    $4cc0             ;SVEC dx=+0 dy=+24 in=12
33d6: 0c 54                        .dd2    $540c             ;SVEC dx=+24 dy=-24 in=0
33d8: 00 c0                        .dd2    $c000             ;VRTS

                   vis
33da: 00 4c                        .dd2    $4c00             ;SVEC dx=+0 dy=+24 in=0 <<<
33dc: c8 40                        .dd2    $40c8             ;SVEC dx=+16 dy=+0 in=12 <<<
33de: c0 54                        .dd2    $54c0             ;SVEC dx=+0 dy=-24 in=12 <<<
33e0: 04 40                        .dd2    $4004             ;SVEC dx=+8 dy=+0 in=0 <<<
33e2: 00 c0                        .dd2    $c000             ;VRTS

                   vis
33e4: c0 4c                        .dd2    $4cc0             ;SVEC dx=+0 dy=+24 in=12 <<<
33e6: da e9                        .dd2    $e9da             ;VJMP a=$09da ($33b4)

                   vis
33e8: 08 46                        .dd2    $4608             ;SVEC dx=+16 dy=+12 in=0 <<<
33ea: d8 40                        .dd2    $40d8             ;SVEC dx=-16 dy=+0 in=12
33ec: c0 46                        .dd2    $46c0             ;SVEC dx=+0 dy=+12 in=12
33ee: ee e9                        .dd2    $e9ee             ;VJMP a=$09ee ($33dc)
                   ; VJSRs to the instructions that draw glyphs.  Because these are all VJSRs, and
                   ; the list is terminated with a VRTS, you can exercise the full set by calling
                   ; the first entry.  (The self-test code does this.)
33f0: 93 a9        vg_glyph_calls  .dd2    $a993             ;VJSR a=$0993 ($3326) ' '
33f2: 8f a9                        .dd2    $a98f             ;VJSR a=$098f ($331e) '0'
33f4: cf a9                        .dd2    $a9cf             ;VJSR a=$09cf ($339e) '1'
33f6: d1 a9                        .dd2    $a9d1             ;VJSR a=$09d1 ($33a2) '2'
33f8: d9 a9                        .dd2    $a9d9             ;VJSR a=$09d9 ($33b2) '3'
33fa: e1 a9                        .dd2    $a9e1             ;VJSR a=$09e1 ($33c2) '4'
33fc: a9 a9                        .dd2    $a9a9             ;VJSR a=$09a9 ($3352) '5'
33fe: e6 a9                        .dd2    $a9e6             ;VJSR a=$09e6 ($33cc) '6'
3400: ed a9                        .dd2    $a9ed             ;VJSR a=$09ed ($33da) '7'
3402: f2 a9                        .dd2    $a9f2             ;VJSR a=$09f2 ($33e4) '8'
3404: f4 a9                        .dd2    $a9f4             ;VJSR a=$09f4 ($33e8) '9'
3406: 41 a9                        .dd2    $a941             ;VJSR a=$0941 ($3282) 'A'
3408: 49 a9                        .dd2    $a949             ;VJSR a=$0949 ($3292) 'B'
340a: 55 a9                        .dd2    $a955             ;VJSR a=$0955 ($32aa) 'C'
340c: 59 a9                        .dd2    $a959             ;VJSR a=$0959 ($32b2) 'D'
340e: 60 a9                        .dd2    $a960             ;VJSR a=$0960 ($32c0) 'E'
3410: 62 a9                        .dd2    $a962             ;VJSR a=$0962 ($32c4) 'F'
3412: 68 a9                        .dd2    $a968             ;VJSR a=$0968 ($32d0) 'G'
3414: 6f a9                        .dd2    $a96f             ;VJSR a=$096f ($32de) 'H'
3416: 74 a9                        .dd2    $a974             ;VJSR a=$0974 ($32e8) 'I'
3418: 7b a9                        .dd2    $a97b             ;VJSR a=$097b ($32f6) 'J'
341a: 7f a9                        .dd2    $a97f             ;VJSR a=$097f ($32fe) 'K'
341c: 85 a9                        .dd2    $a985             ;VJSR a=$0985 ($330a) 'L'
341e: 88 a9                        .dd2    $a988             ;VJSR a=$0988 ($3310) 'M'
3420: 8c a9                        .dd2    $a98c             ;VJSR a=$098c ($3318) 'N'
3422: 8f a9                        .dd2    $a98f             ;VJSR a=$098f ($331e) 'O'
3424: 95 a9                        .dd2    $a995             ;VJSR a=$0995 ($332a) 'P'
3426: 9a a9                        .dd2    $a99a             ;VJSR a=$099a ($3334) 'Q'
3428: a2 a9                        .dd2    $a9a2             ;VJSR a=$09a2 ($3344) 'R'
342a: a9 a9                        .dd2    $a9a9             ;VJSR a=$09a9 ($3352) 'S'
342c: af a9                        .dd2    $a9af             ;VJSR a=$09af ($335e) 'T'
342e: b1 a9                        .dd2    $a9b1             ;VJSR a=$09b1 ($3362) 'U'
3430: b7 a9                        .dd2    $a9b7             ;VJSR a=$09b7 ($336e) 'V'
3432: bb a9                        .dd2    $a9bb             ;VJSR a=$09bb ($3376) 'W'
3434: c0 a9                        .dd2    $a9c0             ;VJSR a=$09c0 ($3380) 'X'
3436: c4 a9                        .dd2    $a9c4             ;VJSR a=$09c4 ($3388) 'Y'
3438: cb a9                        .dd2    $a9cb             ;VJSR a=$09cb ($3396) 'Z'
343a: 93 a9                        .dd2    $a993             ;VJSR a=$0993 ($3326) ' '
343c: d6 a9                        .dd2    $a9d6             ;VJSR a=$09d6 ($33ac) '-'
343e: 2a aa                        .dd2    $aa2a             ;VJSR a=$0a2a ($3454) (C)
3440: 33 aa                        .dd2    $aa33             ;VJSR a=$0a33 ($3466) (P)
3442: 00 c0                        .dd2    $c000             ;VRTS

                   ; Hexagonal "circle" for (C) and (P).
                   vis
3444: 00 42                        .dd2    $4200             ;SVEC dx=+0 dy=+4 in=0 <<<
3446: c0 48                        .dd2    $48c0             ;SVEC dx=+0 dy=+16 in=12
3448: c4 42                        .dd2    $42c4             ;SVEC dx=+8 dy=+4 in=12
344a: c4 5e                        .dd2    $5ec4             ;SVEC dx=+8 dy=-4 in=12
344c: c0 58                        .dd2    $58c0             ;SVEC dx=+0 dy=-16 in=12
344e: dc 5e                        .dd2    $5edc             ;SVEC dx=-8 dy=-4 in=12
3450: dc 42                        .dd2    $42dc             ;SVEC dx=-8 dy=+4 in=12
3452: 00 c0                        .dd2    $c000             ;VRTS

                   vis
3454: 22 aa                        .dd2    $aa22             ;VJSR a=$0a22 ($3444) <<<
3456: 01 00                        .dd2    $0001             ;VCTR dx=+11 dy=+1 in=0
3458: 0b 00                        .dd2    $000b
345a: dd 40                        .dd2    $40dd             ;SVEC dx=-6 dy=+0 in=12
345c: c0 47                        .dd2    $47c0             ;SVEC dx=+0 dy=+14 in=12
345e: c3 40                        .dd2    $40c3             ;SVEC dx=+6 dy=+0 in=12
3460: ed 1f                        .dd2    $1fed             ;VCTR dx=+13 dy=-19 in=0
3462: 0d 00                        .dd2    $000d
3464: 00 c0                        .dd2    $c000             ;VRTS

                   vis
3466: 22 aa                        .dd2    $aa22             ;VJSR a=$0a22 ($3444) <<<
3468: 01 00                        .dd2    $0001             ;VCTR dx=+5 dy=+1 in=0
346a: 05 00                        .dd2    $0005
346c: c0 47                        .dd2    $47c0             ;SVEC dx=+0 dy=+14 in=12
346e: c3 40                        .dd2    $40c3             ;SVEC dx=+6 dy=+0 in=12
3470: c0 5d                        .dd2    $5dc0             ;SVEC dx=+0 dy=-6 in=12
3472: dd 40                        .dd2    $40dd             ;SVEC dx=-6 dy=+0 in=12
3474: f3 1f                        .dd2    $1ff3             ;VCTR dx=+19 dy=-13 in=0
3476: 13 00                        .dd2    $0013
3478: 00 c0                        .dd2    $c000             ;VRTS

                   ; Projectile explosion, displayed when a projectile from the player or an enemy
                   ; tank strikes something.  The pattern is scaled up over the course of a few
                   ; frames.
                   vis
                   vg_proj_explosion
347a: 00 00                        .dd2    $0000             ;VCTR dx=-64 dy=+0 in=0
347c: c0 1f                        .dd2    $1fc0
347e: 00 00                        .dd2    $0000             ;VCTR dx=+0 dy=+0 in=14
3480: 00 e0                        .dd2    $e000
3482: c0 1f                        .dd2    $1fc0             ;VCTR dx=-64 dy=-64 in=0
3484: c0 1f                        .dd2    $1fc0
3486: 00 00                        .dd2    $0000             ;VCTR dx=+0 dy=+0 in=14
3488: 00 e0                        .dd2    $e000
348a: c0 1f                        .dd2    $1fc0             ;VCTR dx=+64 dy=-64 in=0
348c: 40 00                        .dd2    $0040
348e: 00 00                        .dd2    $0000             ;VCTR dx=+0 dy=+0 in=14
3490: 00 e0                        .dd2    $e000
3492: 20 00                        .dd2    $0020             ;VCTR dx=+96 dy=+32 in=0
3494: 60 00                        .dd2    $0060
3496: 00 00                        .dd2    $0000             ;VCTR dx=+0 dy=+0 in=14
3498: 00 e0                        .dd2    $e000
349a: e0 1f                        .dd2    $1fe0             ;VCTR dx=+64 dy=-32 in=0
349c: 40 00                        .dd2    $0040
349e: 00 00                        .dd2    $0000             ;VCTR dx=+0 dy=+0 in=14
34a0: 00 e0                        .dd2    $e000
34a2: 40 00                        .dd2    $0040             ;VCTR dx=+0 dy=+64 in=0
34a4: 00 00                        .dd2    $0000
34a6: 00 00                        .dd2    $0000             ;VCTR dx=+0 dy=+0 in=14
34a8: 00 e0                        .dd2    $e000
34aa: 60 00                        .dd2    $0060             ;VCTR dx=+32 dy=+96 in=0
34ac: 20 00                        .dd2    $0020
34ae: 00 00                        .dd2    $0000             ;VCTR dx=+0 dy=+0 in=14
34b0: 00 e0                        .dd2    $e000
34b2: 60 00                        .dd2    $0060             ;VCTR dx=-32 dy=+96 in=0
34b4: e0 1f                        .dd2    $1fe0
34b6: 00 00                        .dd2    $0000             ;VCTR dx=+0 dy=+0 in=14
34b8: 00 e0                        .dd2    $e000
34ba: e0 1f                        .dd2    $1fe0             ;VCTR dx=-128 dy=-32 in=0
34bc: 80 1f                        .dd2    $1f80
34be: 00 00                        .dd2    $0000             ;VCTR dx=+0 dy=+0 in=14
34c0: 00 e0                        .dd2    $e000
34c2: 20 00                        .dd2    $0020             ;VCTR dx=-96 dy=+32 in=0
34c4: a0 1f                        .dd2    $1fa0
34c6: 00 00                        .dd2    $0000             ;VCTR dx=+0 dy=+0 in=14
34c8: 00 e0                        .dd2    $e000
34ca: 00 c0                        .dd2    $c000             ;VRTS

                   vis
34cc: 40 80        vg_reticle1     .dd2    $8040             ;CNTR
34ce: 51 1f                        .dd2    $1f51             ;VCTR dx=+0 dy=-175 in=0
34d0: 00 00                        .dd2    $0000
34d2: 64 00                        .dd2    $0064             ;VCTR dx=+0 dy=+100 in=6
34d4: 00 60                        .dd2    $6000
34d6: 19 00                        .dd2    $0019             ;VCTR dx=-75 dy=+25 in=0
34d8: b5 1f                        .dd2    $1fb5
34da: e7 1f                        .dd2    $1fe7             ;VCTR dx=+0 dy=-25 in=6
34dc: 00 60                        .dd2    $6000
34de: 00 00                        .dd2    $0000             ;VCTR dx=+150 dy=+0 in=6
34e0: 96 60                        .dd2    $6096
34e2: 19 00                        .dd2    $0019             ;VCTR dx=+0 dy=+25 in=6
34e4: 00 60                        .dd2    $6000
34e6: 64 00                        .dd2    $0064             ;VCTR dx=+0 dy=+100 in=0
34e8: 00 00                        .dd2    $0000
34ea: 19 00                        .dd2    $0019             ;VCTR dx=+0 dy=+25 in=6
34ec: 00 60                        .dd2    $6000
34ee: 00 00                        .dd2    $0000             ;VCTR dx=-150 dy=+0 in=6
34f0: 6a 7f                        .dd2    $7f6a
34f2: e7 1f                        .dd2    $1fe7             ;VCTR dx=+0 dy=-25 in=6
34f4: 00 60                        .dd2    $6000
34f6: 19 00                        .dd2    $0019             ;VCTR dx=+75 dy=+25 in=0
34f8: 4b 00                        .dd2    $004b
34fa: 64 00                        .dd2    $0064             ;VCTR dx=+0 dy=+100 in=6
34fc: 00 60                        .dd2    $6000
34fe: 00 c0                        .dd2    $c000             ;VRTS

                   vis
3500: 40 80        vg_reticle2     .dd2    $8040             ;CNTR
3502: 51 1f                        .dd2    $1f51             ;VCTR dx=+0 dy=-175 in=0
3504: 00 00                        .dd2    $0000
3506: 64 00                        .dd2    $0064             ;VCTR dx=+0 dy=+100 in=14
3508: 00 e0                        .dd2    $e000
350a: 28 00                        .dd2    $0028             ;VCTR dx=+0 dy=+40 in=6
350c: 00 60                        .dd2    $6000
350e: 00 00                        .dd2    $0000             ;VCTR dx=-35 dy=+0 in=0
3510: dd 1f                        .dd2    $1fdd
3512: d8 1f                        .dd2    $1fd8             ;VCTR dx=-40 dy=-40 in=14
3514: d8 ff                        .dd2    $ffd8
3516: 00 00                        .dd2    $0000             ;VCTR dx=+150 dy=+0 in=14
3518: 96 e0                        .dd2    $e096
351a: 28 00                        .dd2    $0028             ;VCTR dx=-40 dy=+40 in=14
351c: d8 ff                        .dd2    $ffd8
351e: 46 00                        .dd2    $0046             ;VCTR dx=+0 dy=+70 in=0
3520: 00 00                        .dd2    $0000
3522: 28 00                        .dd2    $0028             ;VCTR dx=+40 dy=+40 in=14
3524: 28 e0                        .dd2    $e028
3526: 00 00                        .dd2    $0000             ;VCTR dx=-150 dy=+0 in=14
3528: 6a ff                        .dd2    $ff6a
352a: d8 1f                        .dd2    $1fd8             ;VCTR dx=+40 dy=-40 in=14
352c: 28 e0                        .dd2    $e028
352e: 00 00                        .dd2    $0000             ;VCTR dx=+35 dy=+0 in=0
3530: 23 00                        .dd2    $0023
3532: 28 00                        .dd2    $0028             ;VCTR dx=+0 dy=+40 in=6
3534: 00 60                        .dd2    $6000
3536: 64 00                        .dd2    $0064             ;VCTR dx=+0 dy=+100 in=14
3538: 00 e0                        .dd2    $e000
353a: 00 c0                        .dd2    $c000             ;VRTS

                   ; Radar frame and vision code.  Leaves beam at center of radar.
                   vis
353c: 40 80        vg_radar        .dd2    $8040             ;CNTR
353e: 3c 01                        .dd2    $013c             ;VCTR dx=+68 dy=+316 in=0
3540: 44 00                        .dd2    $0044
3542: fc 40                        .dd2    $40fc             ;SVEC dx=-8 dy=+0 in=14
3544: c4 1f                        .dd2    $1fc4             ;VCTR dx=-60 dy=-60 in=0
3546: c4 1f                        .dd2    $1fc4
3548: e0 5c                        .dd2    $5ce0             ;SVEC dx=+0 dy=-8 in=14
354a: 44 00                        .dd2    $0044             ;VCTR dx=-60 dy=+68 in=0
354c: c4 1f                        .dd2    $1fc4
354e: fc 40                        .dd2    $40fc             ;SVEC dx=-8 dy=+0 in=14
3550: 00 00                        .dd2    $0000             ;VCTR dx=+68 dy=+0 in=0
3552: 44 00                        .dd2    $0044
3554: 34 00                        .dd2    $0034             ;VCTR dx=-36 dy=+52 in=10
3556: dc bf                        .dd2    $bfdc
3558: 08 00                        .dd2    $0008             ;VCTR dx=+36 dy=+8 in=0
355a: 24 00                        .dd2    $0024
355c: e0 44                        .dd2    $44e0             ;SVEC dx=+0 dy=+8 in=14
355e: f0 1f                        .dd2    $1ff0             ;VCTR dx=+36 dy=-16 in=0
3560: 24 00                        .dd2    $0024
3562: cc 1f                        .dd2    $1fcc             ;VCTR dx=-36 dy=-52 in=10
3564: dc bf                        .dd2    $bfdc
3566: 00 c0                        .dd2    $c000             ;VRTS

                   ; Tank icon, indicates number of lives left.  Also used in high score list.
                   vis
3568: dd 43                        .dd2    $43dd             ;SVEC dx=-6 dy=+6 in=12 <<<
356a: 03 00                        .dd2    $0003             ;VCTR dx=+9 dy=+3 in=12
356c: 09 c0                        .dd2    $c009
356e: 06 00                        .dd2    $0006             ;VCTR dx=+3 dy=+6 in=12
3570: 03 c0                        .dd2    $c003
3572: f7 1f                        .dd2    $1ff7             ;VCTR dx=+36 dy=-9 in=12
3574: 24 c0                        .dd2    $c024
3576: dd 5d                        .dd2    $5ddd             ;SVEC dx=-6 dy=-6 in=12
3578: 00 00                        .dd2    $0000             ;VCTR dx=-36 dy=+0 in=12
357a: dc df                        .dd2    $dfdc
357c: 09 46                        .dd2    $4609             ;SVEC dx=+18 dy=+12 in=0
357e: 00 00                        .dd2    $0000             ;VCTR dx=+21 dy=+0 in=12
3580: 15 c0                        .dd2    $c015
3582: fd 1f                        .dd2    $1ffd             ;VCTR dx=+0 dy=-3 in=12
3584: 00 c0                        .dd2    $c000
3586: 00 00                        .dd2    $0000             ;VCTR dx=-9 dy=+0 in=12
3588: f7 df                        .dd2    $dff7
358a: f7 1f                        .dd2    $1ff7             ;VCTR dx=+27 dy=-9 in=0
358c: 1b 00                        .dd2    $001b
358e: 00 c0                        .dd2    $c000             ;VRTS
3590: b4 aa        vg_life_icon    .dd2    $aab4             ;VJSR a=$0ab4 ($3568)

                   ; Shattered windshield effects, in 8 parts.
                   vis
3592: 40 80                        .dd2    $8040             ;CNTR <<<
3594: 32 00                        .dd2    $0032             ;VCTR dx=-100 dy=+50 in=0
3596: 9c 1f                        .dd2    $1f9c
3598: 00 00                        .dd2    $0000             ;VCTR dx=-75 dy=+0 in=12
359a: b5 df                        .dd2    $dfb5
359c: 9c 1f                        .dd2    $1f9c             ;VCTR dx=+35 dy=-100 in=0
359e: 23 00                        .dd2    $0023
35a0: 64 00                        .dd2    $0064             ;VCTR dx=+40 dy=+100 in=12
35a2: 28 c0                        .dd2    $c028
35a4: 64 00                        .dd2    $0064             ;VCTR dx=-100 dy=+100 in=12
35a6: 9c df                        .dd2    $df9c
35a8: 19 00                        .dd2    $0019             ;VCTR dx=+250 dy=+25 in=0
35aa: fa 00                        .dd2    $00fa
35ac: 83 1f                        .dd2    $1f83             ;VCTR dx=-150 dy=-125 in=12
35ae: 6a df                        .dd2    $df6a
35b0: ce 1f                        .dd2    $1fce             ;VCTR dx=+100 dy=-50 in=12
35b2: 64 c0                        .dd2    $c064
35b4: 00 c0                        .dd2    $c000             ;VRTS

                   vis
35b6: 00 00                        .dd2    $0000             ;VCTR dx=+80 dy=+0 in=12 <<<
35b8: 50 c0                        .dd2    $c050
35ba: ce 1f                        .dd2    $1fce             ;VCTR dx=-220 dy=-50 in=0
35bc: 24 1f                        .dd2    $1f24
35be: ce 1f                        .dd2    $1fce             ;VCTR dx=+65 dy=-50 in=12
35c0: 41 c0                        .dd2    $c041
35c2: 96 00                        .dd2    $0096             ;VCTR dx=-100 dy=+150 in=0
35c4: 9c 1f                        .dd2    $1f9c
35c6: d3 1f                        .dd2    $1fd3             ;VCTR dx=+0 dy=-45 in=12
35c8: 00 c0                        .dd2    $c000
35ca: 91 00                        .dd2    $0091             ;VCTR dx=-25 dy=+145 in=0
35cc: e7 1f                        .dd2    $1fe7
35ce: 2d 00                        .dd2    $002d             ;VCTR dx=-150 dy=+45 in=12
35d0: 6a df                        .dd2    $df6a
35d2: 1e 00                        .dd2    $001e             ;VCTR dx=+225 dy=+30 in=0
35d4: e1 00                        .dd2    $00e1
35d6: b5 1f                        .dd2    $1fb5             ;VCTR dx=-75 dy=-75 in=12
35d8: b5 df                        .dd2    $dfb5
35da: 19 00                        .dd2    $0019             ;VCTR dx=+250 dy=+25 in=0
35dc: fa 00                        .dd2    $00fa
35de: 5f 00                        .dd2    $005f             ;VCTR dx=+10 dy=+95 in=12
35e0: 0a c0                        .dd2    $c00a
35e2: a1 1f                        .dd2    $1fa1             ;VCTR dx=-10 dy=-95 in=0
35e4: f6 1f                        .dd2    $1ff6
35e6: 3c 00                        .dd2    $003c             ;VCTR dx=+100 dy=+60 in=12
35e8: 64 c0                        .dd2    $c064
35ea: 00 c0                        .dd2    $c000             ;VRTS

                   vis
35ec: 3c 00                        .dd2    $003c             ;VCTR dx=+100 dy=+60 in=12 <<<
35ee: 64 c0                        .dd2    $c064
35f0: 2d 00                        .dd2    $002d             ;VCTR dx=-100 dy=+45 in=0
35f2: 9c 1f                        .dd2    $1f9c
35f4: 97 1f                        .dd2    $1f97             ;VCTR dx=+0 dy=-105 in=12
35f6: 00 c0                        .dd2    $c000
35f8: f6 1f                        .dd2    $1ff6             ;VCTR dx=-275 dy=-10 in=0
35fa: ed 1e                        .dd2    $1eed
35fc: 5f 00                        .dd2    $005f             ;VCTR dx=-105 dy=+95 in=12
35fe: 97 df                        .dd2    $df97
3600: 83 1f                        .dd2    $1f83             ;VCTR dx=-120 dy=-125 in=0
3602: 88 1f                        .dd2    $1f88
3604: 0a 00                        .dd2    $000a             ;VCTR dx=-50 dy=+10 in=12
3606: ce df                        .dd2    $dfce
3608: f6 1f                        .dd2    $1ff6             ;VCTR dx=+50 dy=-10 in=0
360a: 32 00                        .dd2    $0032
360c: 88 1f                        .dd2    $1f88             ;VCTR dx=-130 dy=-120 in=12
360e: 7e df                        .dd2    $df7e
3610: ba 1f                        .dd2    $1fba             ;VCTR dx=+305 dy=-70 in=0
3612: 31 01                        .dd2    $0131
3614: fb 1f                        .dd2    $1ffb             ;VCTR dx=-90 dy=-5 in=12
3616: a6 df                        .dd2    $dfa6
3618: 9c 1f                        .dd2    $1f9c             ;VCTR dx=+190 dy=-100 in=0
361a: be 00                        .dd2    $00be
361c: 74 1f                        .dd2    $1f74             ;VCTR dx=+45 dy=-140 in=12
361e: 2d c0                        .dd2    $c02d
3620: f0 00                        .dd2    $00f0             ;VCTR dx=+110 dy=+240 in=0
3622: 6e 00                        .dd2    $006e
3624: d8 1f                        .dd2    $1fd8             ;VCTR dx=+0 dy=-40 in=12
3626: 00 c0                        .dd2    $c000
3628: 00 c0                        .dd2    $c000             ;VRTS

                   vis
362a: 00 00                        .dd2    $0000             ;VCTR dx=+105 dy=+0 in=12 <<<
362c: 69 c0                        .dd2    $c069
362e: 38 1f                        .dd2    $1f38             ;VCTR dx=-215 dy=-200 in=0
3630: 29 1f                        .dd2    $1f29
3632: 6a 1f                        .dd2    $1f6a             ;VCTR dx=-80 dy=-150 in=12
3634: b0 df                        .dd2    $dfb0
3636: 86 01                        .dd2    $0186             ;VCTR dx=-155 dy=+390 in=0
3638: 65 1f                        .dd2    $1f65
363a: ce 1f                        .dd2    $1fce             ;VCTR dx=+0 dy=-50 in=12
363c: 00 c0                        .dd2    $c000
363e: 32 00                        .dd2    $0032             ;VCTR dx=+0 dy=+50 in=0
3640: 00 00                        .dd2    $0000
3642: 19 00                        .dd2    $0019             ;VCTR dx=-85 dy=+25 in=12
3644: ab df                        .dd2    $dfab
3646: 27 01                        .dd2    $0127             ;VCTR dx=+120 dy=+295 in=0
3648: 78 00                        .dd2    $0078
364a: 73 00                        .dd2    $0073             ;VCTR dx=-120 dy=+115 in=12
364c: 88 df                        .dd2    $df88
364e: 0f 00                        .dd2    $000f             ;VCTR dx=+175 dy=+15 in=0
3650: af 00                        .dd2    $00af
3652: 7e 1f                        .dd2    $1f7e             ;VCTR dx=-55 dy=-130 in=12
3654: c9 df                        .dd2    $dfc9
3656: e7 1f                        .dd2    $1fe7             ;VCTR dx=+480 dy=-25 in=0
3658: e0 01                        .dd2    $01e0
365a: 00 00                        .dd2    $0000             ;VCTR dx=+70 dy=+0 in=12
365c: 46 c0                        .dd2    $c046
365e: 69 00                        .dd2    $0069             ;VCTR dx=-195 dy=+105 in=0
3660: 3d 1f                        .dd2    $1f3d
3662: c4 1f                        .dd2    $1fc4             ;VCTR dx=+25 dy=-60 in=12
3664: 19 c0                        .dd2    $c019
3666: 55 00                        .dd2    $0055             ;VCTR dx=+75 dy=+85 in=12
3668: 4b c0                        .dd2    $c04b
366a: 00 c0                        .dd2    $c000             ;VRTS

                   vis
366c: 41 00                        .dd2    $0041             ;VCTR dx=-15 dy=+65 in=12 <<<
366e: f1 df                        .dd2    $dff1
3670: d8 1f                        .dd2    $1fd8             ;VCTR dx=+50 dy=-40 in=0
3672: 32 00                        .dd2    $0032
3674: e7 1f                        .dd2    $1fe7             ;VCTR dx=-35 dy=-25 in=12
3676: dd df                        .dd2    $dfdd
3678: 2f 1e                        .dd2    $1e2f             ;VCTR dx=-40 dy=-465 in=0
367a: d8 1f                        .dd2    $1fd8
367c: 64 00                        .dd2    $0064             ;VCTR dx=+105 dy=+100 in=12
367e: 69 c0                        .dd2    $c069
3680: 60 1f                        .dd2    $1f60             ;VCTR dx=-80 dy=-160 in=0
3682: b0 1f                        .dd2    $1fb0
3684: 3c 00                        .dd2    $003c             ;VCTR dx=-25 dy=+60 in=12
3686: e7 df                        .dd2    $dfe7
3688: c4 1f                        .dd2    $1fc4             ;VCTR dx=-10 dy=-60 in=12
368a: f6 df                        .dd2    $dff6
368c: a2 1e                        .dd2    $1ea2             ;VCTR dx=-250 dy=-350 in=0
368e: 06 1f                        .dd2    $1f06
3690: 3c 00                        .dd2    $003c             ;VCTR dx=-35 dy=+60 in=12
3692: dd df                        .dd2    $dfdd
3694: c4 1f                        .dd2    $1fc4             ;VCTR dx=-40 dy=-60 in=12
3696: d8 df                        .dd2    $dfd8
3698: 90 01                        .dd2    $0190             ;VCTR dx=-115 dy=+400 in=0
369a: 8d 1f                        .dd2    $1f8d
369c: e2 1f                        .dd2    $1fe2             ;VCTR dx=-35 dy=-30 in=12
369e: dd df                        .dd2    $dfdd
36a0: 00 c0                        .dd2    $c000             ;VRTS

                   vis
36a2: ba 1f                        .dd2    $1fba             ;VCTR dx=+50 dy=-70 in=12 <<<
36a4: 32 c0                        .dd2    $c032
36a6: 1e 00                        .dd2    $001e             ;VCTR dx=-100 dy=+30 in=0
36a8: 9c 1f                        .dd2    $1f9c
36aa: 28 00                        .dd2    $0028             ;VCTR dx=+50 dy=+40 in=12
36ac: 32 c0                        .dd2    $c032
36ae: ec 1f                        .dd2    $1fec             ;VCTR dx=+475 dy=-20 in=0
36b0: db 01                        .dd2    $01db
36b2: ce 1f                        .dd2    $1fce             ;VCTR dx=-50 dy=-50 in=12
36b4: ce df                        .dd2    $dfce
36b6: 58 02                        .dd2    $0258             ;VCTR dx=+135 dy=+600 in=0
36b8: 87 00                        .dd2    $0087
36ba: ce 1f                        .dd2    $1fce             ;VCTR dx=+15 dy=-50 in=12
36bc: 0f c0                        .dd2    $c00f
36be: ac 1e                        .dd2    $1eac             ;VCTR dx=+15 dy=-340 in=0
36c0: 0f 00                        .dd2    $000f
36c2: 92 1f                        .dd2    $1f92             ;VCTR dx=+95 dy=-110 in=12
36c4: 5f c0                        .dd2    $c05f
36c6: 00 c0                        .dd2    $c000             ;VRTS

                   vis
36c8: 4b 00                        .dd2    $004b             ;VCTR dx=+115 dy=+75 in=12 <<<
36ca: 73 c0                        .dd2    $c073
36cc: 8d 1f                        .dd2    $1f8d             ;VCTR dx=-90 dy=-115 in=0
36ce: a6 1f                        .dd2    $1fa6
36d0: 28 00                        .dd2    $0028             ;VCTR dx=-25 dy=+40 in=12
36d2: e7 df                        .dd2    $dfe7
36d4: ba 1f                        .dd2    $1fba             ;VCTR dx=-735 dy=-70 in=0
36d6: 21 1d                        .dd2    $1d21
36d8: f1 1f                        .dd2    $1ff1             ;VCTR dx=-65 dy=-15 in=12
36da: bf df                        .dd2    $dfbf
36dc: 00 c0                        .dd2    $c000             ;VRTS

                   vis
36de: 2d 00                        .dd2    $002d             ;VCTR dx=-85 dy=+45 in=12 <<<
36e0: ab df                        .dd2    $dfab
36e2: a1 1f                        .dd2    $1fa1             ;VCTR dx=+50 dy=-95 in=0
36e4: 32 00                        .dd2    $0032
36e6: 32 00                        .dd2    $0032             ;VCTR dx=+35 dy=+50 in=12
36e8: 23 c0                        .dd2    $c023
36ea: f1 1f                        .dd2    $1ff1             ;VCTR dx=+855 dy=-15 in=0
36ec: 57 03                        .dd2    $0357
36ee: 3c 00                        .dd2    $003c             ;VCTR dx=-30 dy=+60 in=12
36f0: e2 df                        .dd2    $dfe2
36f2: 1e 00                        .dd2    $001e             ;VCTR dx=+80 dy=+30 in=12
36f4: 50 c0                        .dd2    $c050
36f6: 00 c0                        .dd2    $c000             ;VRTS

                   ; 
                   ; Shattered windshield effect, all 8 parts.
                   ; 
                   ; The shape visualized here will look slightly wrong to experienced players,
                   ; because it was drawn to cover the entire screen.  During the game, the top
                   ; part of it is cut off by the radar / score display area.  (The radar area
                   ; isn't drawn when the player is dead, but the window clip isn't changed.)
                   ; 
                   ; (The code copies the first N VJSRs out.  It would have been easier to list the
                   ; VJSRs in reverse order, and then just output a VJSR to the first element.)
                   ; 
                   vis
36f8: c9 aa        vg_hit_cracks   .dd2    $aac9             ;VJSR a=$0ac9 ($3592)
36fa: db aa                        .dd2    $aadb             ;VJSR a=$0adb ($35b6)
36fc: f6 aa                        .dd2    $aaf6             ;VJSR a=$0af6 ($35ec)
36fe: 15 ab                        .dd2    $ab15             ;VJSR a=$0b15 ($362a)
3700: 36 ab                        .dd2    $ab36             ;VJSR a=$0b36 ($366c)
3702: 51 ab                        .dd2    $ab51             ;VJSR a=$0b51 ($36a2)
3704: 64 ab                        .dd2    $ab64             ;VJSR a=$0b64 ($36c8)
3706: 6f ab                        .dd2    $ab6f             ;VJSR a=$0b6f ($36de)
3708: 00 c0                        .dd2    $c000             ;VRTS

                   ; 
                   ; Self-test pattern.  Starts by setting the clip window to (+520,+520), (-520,-
                   ; 520).  Draws a rectangle around the 1024x768 screen, then diagonal cross-
                   ; hatches.  Finally draws a series of horizontal lines near the center.
                   ; 
                   vis
370a: 40 80        vg_selftest_pat .dd2    $8040             ;CNTR
370c: 00 71                        .dd2    $7100             ;SCAL b=1 l=0 (* 1.000)
370e: 08 02                        .dd2    $0208             ;VCTR dx=+520 dy=+520 in=0
3710: 08 02                        .dd2    $0208
3712: 10 62                        .dd2    $6210             ;STAT in=1 cl=0 flags=N/H/O
3714: 40 80                        .dd2    $8040             ;CNTR
3716: f8 1d                        .dd2    $1df8             ;VCTR dx=-520 dy=-520 in=0
3718: f8 1d                        .dd2    $1df8
371a: 20 60                        .dd2    $6020             ;STAT in=2 cl=0 flags=N/L/O
371c: 88 00                        .dd2    $0088             ;VCTR dx=+8 dy=+136 in=0
371e: 08 00                        .dd2    $0008
3720: 00 00                        .dd2    $0000             ;VCTR dx=+1023 dy=+0 in=14
3722: ff e3                        .dd2    $e3ff
3724: ff 02                        .dd2    $02ff             ;VCTR dx=+0 dy=+767 in=14
3726: 00 e0                        .dd2    $e000
3728: 00 00                        .dd2    $0000             ;VCTR dx=-1023 dy=+0 in=14
372a: 01 fc                        .dd2    $fc01
372c: 01 1d                        .dd2    $1d01             ;VCTR dx=+0 dy=-767 in=14
372e: 00 e0                        .dd2    $e000
3730: ff 02                        .dd2    $02ff             ;VCTR dx=+767 dy=+767 in=14
3732: ff e2                        .dd2    $e2ff
3734: 00 1f                        .dd2    $1f00             ;VCTR dx=+256 dy=-256 in=14
3736: 00 e1                        .dd2    $e100
3738: 01 1e                        .dd2    $1e01             ;VCTR dx=-511 dy=-511 in=14
373a: 01 fe                        .dd2    $fe01
373c: 00 02                        .dd2    $0200             ;VCTR dx=-512 dy=+512 in=14
373e: 00 fe                        .dd2    $fe00
3740: ff 00                        .dd2    $00ff             ;VCTR dx=+256 dy=+255 in=14
3742: 00 e1                        .dd2    $e100
3744: 01 1d                        .dd2    $1d01             ;VCTR dx=+767 dy=-767 in=14
3746: ff e2                        .dd2    $e2ff
3748: ff 02                        .dd2    $02ff             ;VCTR dx=+0 dy=+767 in=0
374a: 00 00                        .dd2    $0000
374c: 01 1d                        .dd2    $1d01             ;VCTR dx=-767 dy=-767 in=14
374e: 01 fd                        .dd2    $fd01
3750: ff 00                        .dd2    $00ff             ;VCTR dx=-256 dy=+255 in=14
3752: 00 ff                        .dd2    $ff00
3754: 00 02                        .dd2    $0200             ;VCTR dx=+512 dy=+512 in=14
3756: 00 e2                        .dd2    $e200
3758: 01 1e                        .dd2    $1e01             ;VCTR dx=+511 dy=-511 in=14
375a: ff e1                        .dd2    $e1ff
375c: 00 1f                        .dd2    $1f00             ;VCTR dx=-256 dy=-256 in=14
375e: 00 ff                        .dd2    $ff00
3760: ff 02                        .dd2    $02ff             ;VCTR dx=-767 dy=+767 in=14
3762: 01 fd                        .dd2    $fd01
3764: 40 80                        .dd2    $8040             ;CNTR

                   ; Another self-test pattern.
                   vis
3766: 18 5c                        .dd2    $5c18             ;SVEC dx=-16 dy=-8 in=0
3768: ef 40                        .dd2    $40ef             ;SVEC dx=+30 dy=+0 in=14
376a: 1f 42                        .dd2    $421f             ;SVEC dx=-2 dy=+4 in=0
376c: d3 40                        .dd2    $40d3             ;SVEC dx=-26 dy=+0 in=12
376e: 01 42                        .dd2    $4201             ;SVEC dx=+2 dy=+4 in=0
3770: ab 40                        .dd2    $40ab             ;SVEC dx=+22 dy=+0 in=10
3772: 1f 42                        .dd2    $421f             ;SVEC dx=-2 dy=+4 in=0
3774: 97 40                        .dd2    $4097             ;SVEC dx=-18 dy=+0 in=8
3776: 01 42                        .dd2    $4201             ;SVEC dx=+2 dy=+4 in=0
3778: 67 40                        .dd2    $4067             ;SVEC dx=+14 dy=+0 in=6
377a: 1f 42                        .dd2    $421f             ;SVEC dx=-2 dy=+4 in=0
377c: 5b 40                        .dd2    $405b             ;SVEC dx=-10 dy=+0 in=4
377e: 01 42                        .dd2    $4201             ;SVEC dx=+2 dy=+4 in=0
3780: 23 40                        .dd2    $4023             ;SVEC dx=+6 dy=+0 in=1
3782: 00 c0                        .dd2    $c000             ;VRTS
                   ; 
3784: 92                           .dd1    $92               ;(checksum adj)
                   ; 
                   ; Table for computation of arctan(T), where T is an 8-bit fraction holding the
                   ; tangent or 1/tangent.  The values are angles in the first octant.
                   ; 
                   ; To find the angle in a right triangle, you first compute
                   ; tan(x)=opposite/adjacent.  Battlezone computes that with the absolute values
                   ; of position deltas or, if opposite is bigger, adjacent/opposite.  The goal is
                   ; to get a 0.8 fractional value.  That value is then used as an index into this
                   ; table to get an angle from $00 to $20 (0 to 45 degrees).  The signs of the
                   ; lengths and the choice of numerator (i.e. which side was longer) determine
                   ; which octant the result is in.
                   ; 
3785: 00 00 00 00+ arctan_table    .bulk   $00,$00,$00,$00,$01,$01,$01,$01,$01,$01,$02,$02,$02,$02,$02,$02
                                    +      $03,$03,$03,$03,$03,$03,$03,$04,$04,$04,$04,$04,$04,$05,$05,$05
                                    +      $05,$05,$05,$06,$06,$06,$06,$06,$06,$06,$07,$07,$07,$07,$07,$07
                                    +      $08,$08,$08,$08,$08,$08,$08,$09,$09,$09,$09,$09,$09,$0a,$0a,$0a
                                    +      $0a,$0a,$0a,$0a,$0b,$0b,$0b,$0b,$0b,$0b,$0b,$0c,$0c,$0c,$0c,$0c
                                    +      $0c,$0c,$0d,$0d,$0d,$0d,$0d,$0d,$0d,$0e,$0e,$0e,$0e,$0e,$0e,$0e
                                    +      $0f,$0f,$0f,$0f,$0f,$0f,$0f,$10,$10,$10,$10,$10,$10,$10,$11,$11
                                    +      $11,$11,$11,$11,$11,$11,$12,$12,$12,$12,$12,$12,$12,$13,$13,$13
                                    +      $13,$13,$13,$13,$13,$14,$14,$14,$14,$14,$14,$14,$14,$15,$15,$15
                                    +      $15,$15,$15,$15,$15,$15,$16,$16,$16,$16,$16,$16,$16,$16,$17,$17
                                    +      $17,$17,$17,$17,$17,$17,$17,$18,$18,$18,$18,$18,$18,$18,$18,$18
                                    +      $19,$19,$19,$19,$19,$19,$19,$19,$19,$19,$1a,$1a,$1a,$1a,$1a,$1a
                                    +      $1a,$1a,$1a,$1b,$1b,$1b,$1b,$1b,$1b,$1b,$1b,$1b,$1b,$1c,$1c,$1c
                                    +      $1c,$1c,$1c,$1c,$1c,$1c,$1c,$1c,$1d,$1d,$1d,$1d,$1d,$1d,$1d,$1d
                                    +      $1d,$1d,$1d,$1e,$1e,$1e,$1e,$1e,$1e,$1e,$1e,$1e,$1e,$1e,$1f,$1f
                                    +      $1f,$1f,$1f,$1f,$1f,$1f,$1f,$1f,$1f,$1f,$20,$20,$20,$20,$20,$20
                   ; 
3885: d4                           .dd1    $d4               ;(checksum adj)
                   ; 
                   ; Determines if and when bonus tanks are awarded, based on DSW0 setting.
                   ; 
                   ; This is the score minus one in BCD, so 15K/25K/50K is stored as $14/$24/$49. 
                   ; A second bonus tank is awarded at 100K points, unless the value is $00, in
                   ; which case no bonus tanks are awarded at all.
                   ; 
                   bonus_tank_score
3886: 00 14 24 49                  .bulk   $00,$14,$24,$49
                   ; 
                   ; Determines when missiles first appear, based on DSW0 setting: after 5000,
                   ; 10000, 20000, or 30000 points.
                   ; 
388a: 05 10 20 30  missile_score   .bulk   $05,$10,$20,$30

                   ; 
                   ; Shape vertex data.
                   ; 
                   ; The first byte in each shape is the length of the data, in bytes, plus one. 
                   ; This is followed by the list of vertices, which are signed 16-bit coordinates.
                   ; 
                   ; Jed Margolin's "Unit Vector Math for 3D Graphics" document defines a left-
                   ; handed coordinate system with +X into the monitor, +Y to the right, and +Z up.
                   ; Using this system, the coordinates are stored in X, Y, Z order.  The SourceGen
                   ; visualizers expect a left-handed system with +X right, +Y up, +Z into the
                   ; screen, so a simple translation is required.
                   ; 
                   ; The shapes themselves are flipped about the X coordinate.  If you look at the
                   ; logo shapes, all of the coordinates in "Ba" are positive, even though it's
                   ; supposed to be left of center.  The visualizer negates the X coordinate to
                   ; compensate.
                   ; 
                   ; Because the math box effectively halves the X/Z coordinates during the model
                   ; transformation, the shapes here are all twice as wide as they should be.  This
                   ; can be compensated for by halving X/Z or doubling Y.
                   ; 
                   ; The table holds 44 addresses, two of which are unused.  The largest shape has
                   ; 26 vertices.
                   ; 
                   vis
                   shape_vertex_addrs
388e: e6 38                        .dd2    shp_v_naropyr     ;$00

                   vis
3890: 05 39                        .dd2    shp_v_tallbox     ;$01

                   vis
3892: 55 39                        .dd2    shp_v_tank1       ;$02

                   vis
3894: 36 39                        .dd2    shp_v_projectile  ;$03

                   vis vis
3896: 55 3a                        .dd2    shp_v_rtread0     ;$04

                   vis
3898: 30 3a                        .dd2    shp_v_rtread1     ;$05

                   vis
389a: 0b 3a                        .dd2    shp_v_rtread2     ;$06

                   vis
389c: e6 39                        .dd2    shp_v_rtread3     ;$07

                   vis vis
389e: 7a 3a                        .dd2    shp_v_ftread0     ;$08

                   vis
38a0: 9f 3a                        .dd2    shp_v_ftread1     ;$09

                   vis
38a2: c4 3a                        .dd2    shp_v_ftread2     ;$0a

                   vis
38a4: e9 3a                        .dd2    shp_v_ftread3     ;$0b

                   vis
38a6: 3b 3c                        .dd2    shp_v_widepyr     ;$0c

                   vis
38a8: 0e 3b                        .dd2    shp_v_radar       ;$0d

                   ; Shape $0e is the projectile explosion pattern.  It uses a special drawing mode
                   ; that scales the AVG code at $347a.
                   vis
38aa: 3f 3b                        .dd2    shp_v_prj_explos  ;$0e

                   vis
38ac: 5a 3c                        .dd2    shp_v_shortbox    ;$0f

                   vis
38ae: 9b 3b                        .dd2    shp_v_chunk0      ;$10

                   vis
38b0: c0 3b                        .dd2    shp_v_chunk1      ;$11

                   vis
38b2: 46 3b                        .dd2    shp_v_chunk2      ;$12

                   vis
38b4: 0e 3b                        .dd2    shp_v_radar       ;$13

                   vis
38b6: c0 3b                        .dd2    shp_v_chunk1      ;$14

                   vis
38b8: 9b 3b                        .dd2    shp_v_chunk0      ;$15

                   vis
38ba: 8b 3c                        .dd2    shp_v_missile     ;$16

                   vis
38bc: d5 76                        .dd2    shp_v_ba          ;$17

                   vis
38be: c0 3b                        .dd2    shp_v_chunk1      ;$18

                   vis
38c0: 0a 3c                        .dd2    shp_v_chunk4      ;$19

                   vis
38c2: 9b 3b                        .dd2    shp_v_chunk0      ;$1a

                   vis
38c4: f1 3b                        .dd2    shp_v_chunk5      ;$1b

                   vis
38c6: 9b 3b                        .dd2    shp_v_chunk0      ;$1c

                   vis
38c8: 0a 3c                        .dd2    shp_v_chunk4      ;$1d

                   vis
38ca: 4e 77                        .dd2    shp_v_ttle        ;$1e

                   vis
38cc: cd 77                        .dd2    shp_v_zone        ;$1f

                   vis
38ce: b0 3e                        .dd2    shp_v_saucer      ;$20

                   vis
38d0: 17 3f                        .dd2    shp_v_tank2       ;$21
38d2: 00 00                        .dd2    $0000             ;$22
38d4: 00 00                        .dd2    $0000             ;$23

                   vis vis
38d6: 28 3d                        .dd2    shp_v_spatter0    ;$24

                   vis
38d8: 59 3d                        .dd2    shp_v_spatter1    ;$25

                   vis
38da: 8a 3d                        .dd2    shp_v_spatter2    ;$26

                   vis
38dc: bb 3d                        .dd2    shp_v_spatter3    ;$27

                   vis
38de: ec 3d                        .dd2    shp_v_spatter4    ;$28

                   vis
38e0: 1d 3e                        .dd2    shp_v_spatter5    ;$29

                   vis
38e2: 4e 3e                        .dd2    shp_v_spatter6    ;$2a

                   vis
38e4: 7f 3e                        .dd2    shp_v_spatter7    ;$2b
38e6: 1f           shp_v_naropyr   .dd1    $1f               ;shape $00
38e7: 00 fe 00 fe+                 .bulk   $00,$fe,$00,$fe,$c0,$fe ;-512,-512,-320
38ed: 00 fe 00 02+                 .bulk   $00,$fe,$00,$02,$c0,$fe
38f3: 00 02 00 02+                 .bulk   $00,$02,$00,$02,$c0,$fe
38f9: 00 02 00 fe+                 .bulk   $00,$02,$00,$fe,$c0,$fe
38ff: 00 00 00 00+                 .bulk   $00,$00,$00,$00,$40,$01 ;0,0,320
3905: 31           shp_v_tallbox   .dd1    $31               ;shape $01
3906: 00 fe 00 fe+                 .bulk   $00,$fe,$00,$fe,$c0,$fe
390c: 00 fe 00 02+                 .bulk   $00,$fe,$00,$02,$c0,$fe
3912: 00 02 00 02+                 .bulk   $00,$02,$00,$02,$c0,$fe
3918: 00 02 00 fe+                 .bulk   $00,$02,$00,$fe,$c0,$fe
391e: 00 fe 00 fe+                 .bulk   $00,$fe,$00,$fe,$40,$01
3924: 00 fe 00 02+                 .bulk   $00,$fe,$00,$02,$40,$01
392a: 00 02 00 02+                 .bulk   $00,$02,$00,$02,$40,$01
3930: 00 02 00 fe+                 .bulk   $00,$02,$00,$fe,$40,$01
                   shp_v_projectile
3936: 1f                           .dd1    $1f               ;shape $03
3937: d8 ff d8 ff+                 .bulk   $d8,$ff,$d8,$ff,$d0,$ff
393d: d8 ff d8 ff+                 .bulk   $d8,$ff,$d8,$ff,$f8,$ff
3943: d8 ff 28 00+                 .bulk   $d8,$ff,$28,$00,$f8,$ff
3949: d8 ff 28 00+                 .bulk   $d8,$ff,$28,$00,$d0,$ff
394f: 50 00 00 00+                 .bulk   $50,$00,$00,$00,$e4,$ff
3955: 91           shp_v_tank1     .dd1    $91               ;shape $02
3956: 20 fd 00 fe+                 .bulk   $20,$fd,$00,$fe,$c0,$fe
395c: 20 fd 00 02+                 .bulk   $20,$fd,$00,$02,$c0,$fe
3962: c8 03 00 02+                 .bulk   $c8,$03,$00,$02,$c0,$fe
3968: c8 03 00 fe+                 .bulk   $c8,$03,$00,$fe,$c0,$fe
396e: 00 fc c8 fd+                 .bulk   $00,$fc,$c8,$fd,$30,$ff
3974: 00 fc 38 02+                 .bulk   $00,$fc,$38,$02,$30,$ff
397a: e0 04 38 02+                 .bulk   $e0,$04,$38,$02,$30,$ff
3980: e0 04 c8 fd+                 .bulk   $e0,$04,$c8,$fd,$30,$ff
3986: 58 fd a8 fe+                 .bulk   $58,$fd,$a8,$fe,$88,$ff
398c: 58 fd 58 01+                 .bulk   $58,$fd,$58,$01,$88,$ff
3992: a8 02 58 01+                 .bulk   $a8,$02,$58,$01,$88,$ff
3998: a8 02 a8 fe+                 .bulk   $a8,$02,$a8,$fe,$88,$ff
399e: 00 fe 58 ff+                 .bulk   $00,$fe,$58,$ff,$30,$00
39a4: 00 fe a8 00+                 .bulk   $00,$fe,$a8,$00,$30,$00
39aa: 80 ff d8 ff+                 .bulk   $80,$ff,$d8,$ff,$f8,$ff
39b0: 80 ff 28 00+                 .bulk   $80,$ff,$28,$00,$f8,$ff
39b6: 80 00 28 00+                 .bulk   $80,$00,$28,$00,$d0,$ff
39bc: 80 00 d8 ff+                 .bulk   $80,$00,$d8,$ff,$d0,$ff
39c2: 60 04 28 00+                 .bulk   $60,$04,$28,$00,$f8,$ff
39c8: 60 04 28 00+                 .bulk   $60,$04,$28,$00,$d0,$ff
39ce: 60 04 d8 ff+                 .bulk   $60,$04,$d8,$ff,$f8,$ff
39d4: 60 04 d8 ff+                 .bulk   $60,$04,$d8,$ff,$d0,$ff
39da: 00 fe 00 00+                 .bulk   $00,$fe,$00,$00,$30,$00
39e0: 00 fe 00 00+                 .bulk   $00,$fe,$00,$00,$50,$00 ;antenna at Z=-512 X=0 Y=80
39e6: 25           shp_v_rtread3   .dd1    $25               ;shape $07
39e7: 00 fc c8 fd+                 .bulk   $00,$fc,$c8,$fd,$30,$ff
39ed: 00 fc 38 02+                 .bulk   $00,$fc,$38,$02,$30,$ff
39f3: 68 fc dc fd+                 .bulk   $68,$fc,$dc,$fd,$08,$ff
39f9: 68 fc 24 02+                 .bulk   $68,$fc,$24,$02,$08,$ff
39ff: d0 fc ec fd+                 .bulk   $d0,$fc,$ec,$fd,$e0,$fe
3a05: d0 fc 14 02+                 .bulk   $d0,$fc,$14,$02,$e0,$fe
3a0b: 25           shp_v_rtread2   .dd1    $25               ;shape $06
3a0c: 18 fc cc fd+                 .bulk   $18,$fc,$cc,$fd,$28,$ff
3a12: 18 fc 34 02+                 .bulk   $18,$fc,$34,$02,$28,$ff
3a18: 80 fc e0 fd+                 .bulk   $80,$fc,$e0,$fd,$00,$ff
3a1e: 80 fc 20 02+                 .bulk   $80,$fc,$20,$02,$00,$ff
3a24: e8 fc f0 fd+                 .bulk   $e8,$fc,$f0,$fd,$d8,$fe
3a2a: e8 fc 10 02+                 .bulk   $e8,$fc,$10,$02,$d8,$fe
3a30: 25           shp_v_rtread1   .dd1    $25               ;shape $05
3a31: 34 fc d4 fd+                 .bulk   $34,$fc,$d4,$fd,$1c,$ff
3a37: 34 fc 2c 02+                 .bulk   $34,$fc,$2c,$02,$1c,$ff
3a3d: 9c fc e4 fd+                 .bulk   $9c,$fc,$e4,$fd,$f4,$fe
3a43: 9c fc 1c 02+                 .bulk   $9c,$fc,$1c,$02,$f4,$fe
3a49: 04 fd f8 fd+                 .bulk   $04,$fd,$f8,$fd,$cc,$fe
3a4f: 04 fd 08 02+                 .bulk   $04,$fd,$08,$02,$cc,$fe
3a55: 25           shp_v_rtread0   .dd1    $25               ;shape $04
3a56: 4c fc d8 fd+                 .bulk   $4c,$fc,$d8,$fd,$14,$ff
3a5c: 4c fc 28 02+                 .bulk   $4c,$fc,$28,$02,$14,$ff
3a62: b4 fc e8 fd+                 .bulk   $b4,$fc,$e8,$fd,$ec,$fe
3a68: b4 fc 18 02+                 .bulk   $b4,$fc,$18,$02,$ec,$fe
3a6e: 20 fd fc fd+                 .bulk   $20,$fd,$fc,$fd,$c4,$fe
3a74: 20 fd 04 02+                 .bulk   $20,$fd,$04,$02,$c4,$fe
3a7a: 25           shp_v_ftread0   .dd1    $25               ;shape $08
3a7b: e0 04 c8 fd+                 .bulk   $e0,$04,$c8,$fd,$30,$ff
3a81: e0 04 38 02+                 .bulk   $e0,$04,$38,$02,$30,$ff
3a87: 80 04 dc fd+                 .bulk   $80,$04,$dc,$fd,$08,$ff
3a8d: 80 04 24 02+                 .bulk   $80,$04,$24,$02,$08,$ff
3a93: 20 04 ec fd+                 .bulk   $20,$04,$ec,$fd,$e0,$fe
3a99: 20 04 14 02+                 .bulk   $20,$04,$14,$02,$e0,$fe
3a9f: 25           shp_v_ftread1   .dd1    $25               ;shape $09
3aa0: c8 04 cc fd+                 .bulk   $c8,$04,$cc,$fd,$28,$ff
3aa6: c8 04 34 02+                 .bulk   $c8,$04,$34,$02,$28,$ff
3aac: 68 04 e0 fd+                 .bulk   $68,$04,$e0,$fd,$00,$ff
3ab2: 68 04 20 02+                 .bulk   $68,$04,$20,$02,$00,$ff
3ab8: 08 04 f0 fd+                 .bulk   $08,$04,$f0,$fd,$d8,$fe
3abe: 08 04 10 02+                 .bulk   $08,$04,$10,$02,$d8,$fe
3ac4: 25           shp_v_ftread2   .dd1    $25               ;shape $0a
3ac5: b0 04 d4 fd+                 .bulk   $b0,$04,$d4,$fd,$1c,$ff
3acb: b0 04 2c 02+                 .bulk   $b0,$04,$2c,$02,$1c,$ff
3ad1: 50 04 e4 fd+                 .bulk   $50,$04,$e4,$fd,$f4,$fe
3ad7: 50 04 1c 02+                 .bulk   $50,$04,$1c,$02,$f4,$fe
3add: f0 03 f8 fd+                 .bulk   $f0,$03,$f8,$fd,$cc,$fe
3ae3: f0 03 08 02+                 .bulk   $f0,$03,$08,$02,$cc,$fe
3ae9: 25           shp_v_ftread3   .dd1    $25               ;shape $0b
3aea: 98 04 d8 fd+                 .bulk   $98,$04,$d8,$fd,$14,$ff
3af0: 98 04 28 02+                 .bulk   $98,$04,$28,$02,$14,$ff
3af6: 38 04 e8 fd+                 .bulk   $38,$04,$e8,$fd,$ec,$fe
3afc: 38 04 18 02+                 .bulk   $38,$04,$18,$02,$ec,$fe
3b02: d8 03 fc fd+                 .bulk   $d8,$03,$fc,$fd,$c4,$fe
3b08: d8 03 04 02+                 .bulk   $d8,$03,$04,$02,$c4,$fe
3b0e: 31           shp_v_radar     .dd1    $31               ;shape $0d,$13
3b0f: 00 00 b0 ff+                 .bulk   $00,$00,$b0,$ff,$50,$00 ;bottom #1 at 0,-80 height=80
3b15: 50 00 60 ff+                 .bulk   $50,$00,$60,$ff,$64,$00
3b1b: 50 00 60 ff+                 .bulk   $50,$00,$60,$ff,$78,$00
3b21: 00 00 b0 ff+                 .bulk   $00,$00,$b0,$ff,$8c,$00
3b27: 00 00 50 00+                 .bulk   $00,$00,$50,$00,$50,$00 ;bottom #2 at 0,80 height=80
3b2d: 50 00 a0 00+                 .bulk   $50,$00,$a0,$00,$64,$00
3b33: 50 00 a0 00+                 .bulk   $50,$00,$a0,$00,$78,$00
3b39: 00 00 50 00+                 .bulk   $00,$00,$50,$00,$8c,$00
                   shp_v_prj_explos
3b3f: 07                           .dd1    $07               ;shape $0e
3b40: 00 00 00 00+                 .bulk   $00,$00,$00,$00,$00,$00
3b46: 55           shp_v_chunk2    .dd1    $55               ;shape $12
3b47: b4 fd a8 fe+                 .bulk   $b4,$fd,$a8,$fe,$6c,$ff
3b4d: b4 fd 58 01+                 .bulk   $b4,$fd,$58,$01,$6c,$ff
3b53: 4c 02 58 01+                 .bulk   $4c,$02,$58,$01,$18,$fe
3b59: 4c 02 a8 fe+                 .bulk   $4c,$02,$a8,$fe,$18,$fe
3b5f: f0 fe 58 ff+                 .bulk   $f0,$fe,$58,$ff,$d0,$ff
3b65: f0 fe a8 00+                 .bulk   $f0,$fe,$a8,$00,$d0,$ff
3b6b: 00 00 d8 ff+                 .bulk   $00,$00,$d8,$ff,$44,$ff
3b71: 00 00 28 00+                 .bulk   $00,$00,$28,$00,$44,$ff
3b77: b4 00 28 00+                 .bulk   $b4,$00,$28,$00,$e0,$fe
3b7d: b4 00 d8 ff+                 .bulk   $b4,$00,$d8,$ff,$e0,$fe
3b83: 38 04 28 00+                 .bulk   $38,$04,$28,$00,$0c,$fe
3b89: 10 04 28 00+                 .bulk   $10,$04,$28,$00,$e8,$fd
3b8f: 38 04 d8 ff+                 .bulk   $38,$04,$d8,$ff,$0c,$fe
3b95: 10 04 d8 ff+                 .bulk   $10,$04,$d8,$ff,$e8,$fd
3b9b: 25           shp_v_chunk0    .dd1    $25               ;shape $10,$15,$1a,$1c
3b9c: dc 00 00 00+                 .bulk   $dc,$00,$00,$00,$f0,$fe
3ba2: c0 fe b0 ff+                 .bulk   $c0,$fe,$b0,$ff,$44,$ff
3ba8: 54 01 50 00+                 .bulk   $54,$01,$50,$00,$a0,$ff
3bae: 48 ff 00 00+                 .bulk   $48,$ff,$00,$00,$9c,$fe
3bb4: 84 ff b0 ff+                 .bulk   $84,$ff,$b0,$ff,$00,$ff
3bba: 8c ff 50 00+                 .bulk   $8c,$ff,$50,$00,$30,$ff
3bc0: 31           shp_v_chunk1    .dd1    $31               ;shape $11,$14,$18
3bc1: 10 ff 88 ff+                 .bulk   $10,$ff,$88,$ff,$c0,$fe
3bc7: 88 fe 40 00+                 .bulk   $88,$fe,$40,$00,$e8,$fe
3bcd: d0 02 a0 00+                 .bulk   $d0,$02,$a0,$00,$80,$fe
3bd3: 80 02 88 ff+                 .bulk   $80,$02,$88,$ff,$c0,$fe
3bd9: d8 ff c0 ff+                 .bulk   $d8,$ff,$c0,$ff,$b0,$ff
3bdf: 00 00 20 00+                 .bulk   $00,$00,$20,$00,$c4,$ff
3be5: 38 00 60 ff+                 .bulk   $38,$00,$60,$ff,$38,$ff
3beb: 78 00 c8 00+                 .bulk   $78,$00,$c8,$00,$10,$ff
3bf1: 19           shp_v_chunk5    .dd1    $19               ;shape $1b
3bf2: b0 ff f4 ff+                 .bulk   $b0,$ff,$f4,$ff,$e0,$fe
3bf8: d8 01 70 00+                 .bulk   $d8,$01,$70,$00,$50,$fe
3bfe: 20 03 d4 ff+                 .bulk   $20,$03,$d4,$ff,$0c,$00
3c04: 58 00 f0 ff+                 .bulk   $58,$00,$f0,$ff,$f4,$fe
3c0a: 31           shp_v_chunk4    .dd1    $31               ;shape $19,$1d
3c0b: d4 fe b8 ff+                 .bulk   $d4,$fe,$b8,$ff,$48,$ff
3c11: 18 ff 58 ff+                 .bulk   $18,$ff,$58,$ff,$48,$ff
3c17: 18 ff f0 fe+                 .bulk   $18,$ff,$f0,$fe,$14,$ff
3c1d: d4 fe f0 fe+                 .bulk   $d4,$fe,$f0,$fe,$e4,$fe
3c23: a0 ff a8 00+                 .bulk   $a0,$ff,$a8,$00,$34,$ff
3c29: 28 00 0c 00+                 .bulk   $28,$00,$0c,$00,$40,$ff
3c2f: 28 00 fc fe+                 .bulk   $28,$00,$fc,$fe,$bc,$fe
3c35: a0 ff 18 ff+                 .bulk   $a0,$ff,$18,$ff,$6c,$fe
3c3b: 1f           shp_v_widepyr   .dd1    $1f               ;shape $0c
3c3c: e0 fc e0 fc+                 .bulk   $e0,$fc,$e0,$fc,$c0,$fe
3c42: e0 fc 20 03+                 .bulk   $e0,$fc,$20,$03,$c0,$fe
3c48: 20 03 20 03+                 .bulk   $20,$03,$20,$03,$c0,$fe
3c4e: 20 03 e0 fc+                 .bulk   $20,$03,$e0,$fc,$c0,$fe
3c54: 00 00 00 00+                 .bulk   $00,$00,$00,$00,$90,$01
3c5a: 31           shp_v_shortbox  .dd1    $31               ;shape $0f
3c5b: 80 fd 80 fd+                 .bulk   $80,$fd,$80,$fd,$c0,$fe
3c61: 80 fd 80 02+                 .bulk   $80,$fd,$80,$02,$c0,$fe
3c67: 80 02 80 02+                 .bulk   $80,$02,$80,$02,$c0,$fe
3c6d: 80 02 80 fd+                 .bulk   $80,$02,$80,$fd,$c0,$fe
3c73: 80 fd 80 fd+                 .bulk   $80,$fd,$80,$fd,$d8,$ff
3c79: 80 fd 80 02+                 .bulk   $80,$fd,$80,$02,$d8,$ff
3c7f: 80 02 80 02+                 .bulk   $80,$02,$80,$02,$d8,$ff
3c85: 80 02 80 fd+                 .bulk   $80,$02,$80,$fd,$d8,$ff
3c8b: 9d           shp_v_missile   .dd1    $9d               ;shape $16
3c8c: 80 fe 90 00+                 .bulk   $80,$fe,$90,$00,$00,$00
3c92: 80 fe 48 00+                 .bulk   $80,$fe,$48,$00,$30,$00
3c98: 80 fe b8 ff+                 .bulk   $80,$fe,$b8,$ff,$30,$00
3c9e: 80 fe 70 ff+                 .bulk   $80,$fe,$70,$ff,$00,$00
3ca4: 80 fe b8 ff+                 .bulk   $80,$fe,$b8,$ff,$d0,$ff
3caa: 80 fe 48 00+                 .bulk   $80,$fe,$48,$00,$d0,$ff
3cb0: a0 ff 20 01+                 .bulk   $a0,$ff,$20,$01,$00,$00
3cb6: a0 ff c0 00+                 .bulk   $a0,$ff,$c0,$00,$60,$00
3cbc: a0 ff 40 ff+                 .bulk   $a0,$ff,$40,$ff,$60,$00
3cc2: a0 ff e0 fe+                 .bulk   $a0,$ff,$e0,$fe,$00,$00
3cc8: a0 ff 40 ff+                 .bulk   $a0,$ff,$40,$ff,$a0,$ff
3cce: a0 ff c0 00+                 .bulk   $a0,$ff,$c0,$00,$a0,$ff
3cd4: 80 04 00 00+                 .bulk   $80,$04,$00,$00,$00,$00
3cda: 70 05 00 00+                 .bulk   $70,$05,$00,$00,$00,$00
3ce0: 70 ff 70 ff+                 .bulk   $70,$ff,$70,$ff,$58,$ff
3ce6: 70 ff 90 00+                 .bulk   $70,$ff,$90,$00,$58,$ff
3cec: 90 00 90 00+                 .bulk   $90,$00,$90,$00,$58,$ff
3cf2: 90 00 70 ff+                 .bulk   $90,$00,$70,$ff,$58,$ff
3cf8: d0 ff d0 ff+                 .bulk   $d0,$ff,$d0,$ff,$a4,$ff
3cfe: d0 ff 30 00+                 .bulk   $d0,$ff,$30,$00,$a4,$ff
3d04: 30 00 30 00+                 .bulk   $30,$00,$30,$00,$ac,$ff
3d0a: 30 00 d0 ff+                 .bulk   $30,$00,$d0,$ff,$ac,$ff
3d10: a0 ff 00 00+                 .bulk   $a0,$ff,$00,$00,$60,$00
3d16: 10 02 48 00+                 .bulk   $10,$02,$48,$00,$30,$00
3d1c: 10 02 b8 ff+                 .bulk   $10,$02,$b8,$ff,$30,$00
3d22: 30 00 00 00+                 .bulk   $30,$00,$00,$00,$90,$00
3d28: 31           shp_v_spatter0  .dd1    $31               ;shape $24
3d29: 00 00 34 00+                 .bulk   $00,$00,$34,$00,$4c,$ff
3d2f: 24 00 24 00+                 .bulk   $24,$00,$24,$00,$4c,$ff
3d35: 34 00 00 00+                 .bulk   $34,$00,$00,$00,$4c,$ff
3d3b: 24 00 dc ff+                 .bulk   $24,$00,$dc,$ff,$4c,$ff
3d41: 00 00 cc ff+                 .bulk   $00,$00,$cc,$ff,$4c,$ff
3d47: dc ff dc ff+                 .bulk   $dc,$ff,$dc,$ff,$4c,$ff
3d4d: cc ff 00 00+                 .bulk   $cc,$ff,$00,$00,$4c,$ff
3d53: dc ff 24 00+                 .bulk   $dc,$ff,$24,$00,$4c,$ff
3d59: 31           shp_v_spatter1  .dd1    $31               ;shape $25
3d5a: 00 00 64 00+                 .bulk   $00,$00,$64,$00,$38,$ff
3d60: 48 00 48 00+                 .bulk   $48,$00,$48,$00,$38,$ff
3d66: 64 00 00 00+                 .bulk   $64,$00,$00,$00,$38,$ff
3d6c: 48 00 b8 ff+                 .bulk   $48,$00,$b8,$ff,$38,$ff
3d72: 00 00 9c ff+                 .bulk   $00,$00,$9c,$ff,$38,$ff
3d78: b8 ff b8 ff+                 .bulk   $b8,$ff,$b8,$ff,$38,$ff
3d7e: 9c ff 00 00+                 .bulk   $9c,$ff,$00,$00,$38,$ff
3d84: b8 ff 48 00+                 .bulk   $b8,$ff,$48,$00,$38,$ff
3d8a: 31           shp_v_spatter2  .dd1    $31               ;shape $26
3d8b: 00 00 98 00+                 .bulk   $00,$00,$98,$00,$24,$ff
3d91: 6c 00 6c 00+                 .bulk   $6c,$00,$6c,$00,$24,$ff
3d97: 98 00 00 00+                 .bulk   $98,$00,$00,$00,$24,$ff
3d9d: 6c 00 94 ff+                 .bulk   $6c,$00,$94,$ff,$24,$ff
3da3: 00 00 68 ff+                 .bulk   $00,$00,$68,$ff,$24,$ff
3da9: 94 ff 94 ff+                 .bulk   $94,$ff,$94,$ff,$24,$ff
3daf: 68 ff 00 00+                 .bulk   $68,$ff,$00,$00,$24,$ff
3db5: 94 ff 6c 00+                 .bulk   $94,$ff,$6c,$00,$24,$ff
3dbb: 31           shp_v_spatter3  .dd1    $31               ;shape $27
3dbc: 00 00 c8 00+                 .bulk   $00,$00,$c8,$00,$10,$ff
3dc2: 90 00 90 00+                 .bulk   $90,$00,$90,$00,$10,$ff
3dc8: c8 00 00 00+                 .bulk   $c8,$00,$00,$00,$10,$ff
3dce: 90 00 70 ff+                 .bulk   $90,$00,$70,$ff,$10,$ff
3dd4: 00 00 38 ff+                 .bulk   $00,$00,$38,$ff,$10,$ff
3dda: 70 ff 70 ff+                 .bulk   $70,$ff,$70,$ff,$10,$ff
3de0: 38 ff 00 00+                 .bulk   $38,$ff,$00,$00,$10,$ff
3de6: 70 ff 90 00+                 .bulk   $70,$ff,$90,$00,$10,$ff
3dec: 31           shp_v_spatter4  .dd1    $31               ;shape $28
3ded: 00 00 fc 00+                 .bulk   $00,$00,$fc,$00,$fc,$fe
3df3: b0 00 b0 00+                 .bulk   $b0,$00,$b0,$00,$fc,$fe
3df9: fc 00 00 00+                 .bulk   $fc,$00,$00,$00,$fc,$fe
3dff: b0 00 50 ff+                 .bulk   $b0,$00,$50,$ff,$fc,$fe
3e05: 00 00 04 ff+                 .bulk   $00,$00,$04,$ff,$fc,$fe
3e0b: 50 ff 50 ff+                 .bulk   $50,$ff,$50,$ff,$fc,$fe
3e11: 04 ff 00 00+                 .bulk   $04,$ff,$00,$00,$fc,$fe
3e17: 50 ff b0 00+                 .bulk   $50,$ff,$b0,$00,$fc,$fe
3e1d: 31           shp_v_spatter5  .dd1    $31               ;shape $29
3e1e: 00 00 2c 01+                 .bulk   $00,$00,$2c,$01,$e8,$fe
3e24: d4 00 d4 00+                 .bulk   $d4,$00,$d4,$00,$e8,$fe
3e2a: 2c 01 00 00+                 .bulk   $2c,$01,$00,$00,$e8,$fe
3e30: d4 00 2c ff+                 .bulk   $d4,$00,$2c,$ff,$e8,$fe
3e36: 00 00 d4 fe+                 .bulk   $00,$00,$d4,$fe,$e8,$fe
3e3c: 2c ff 2c ff+                 .bulk   $2c,$ff,$2c,$ff,$e8,$fe
3e42: d4 fe 00 00+                 .bulk   $d4,$fe,$00,$00,$e8,$fe
3e48: 2c ff d4 00+                 .bulk   $2c,$ff,$d4,$00,$e8,$fe
3e4e: 31           shp_v_spatter6  .dd1    $31               ;shape $2a
3e4f: 00 00 60 01+                 .bulk   $00,$00,$60,$01,$d4,$fe
3e55: 08 01 08 01+                 .bulk   $08,$01,$08,$01,$d4,$fe
3e5b: 60 01 00 00+                 .bulk   $60,$01,$00,$00,$d4,$fe
3e61: 08 01 f8 fe+                 .bulk   $08,$01,$f8,$fe,$d4,$fe
3e67: 00 00 a0 fe+                 .bulk   $00,$00,$a0,$fe,$d4,$fe
3e6d: f8 fe f8 fe+                 .bulk   $f8,$fe,$f8,$fe,$d4,$fe
3e73: a0 fe 00 00+                 .bulk   $a0,$fe,$00,$00,$d4,$fe
3e79: f8 fe 08 01+                 .bulk   $f8,$fe,$08,$01,$d4,$fe
3e7f: 31           shp_v_spatter7  .dd1    $31               ;shape $2b
3e80: 00 00 90 01+                 .bulk   $00,$00,$90,$01,$c0,$fe
3e86: 1c 01 1c 01+                 .bulk   $1c,$01,$1c,$01,$c0,$fe
3e8c: 90 01 00 00+                 .bulk   $90,$01,$00,$00,$c0,$fe
3e92: 1c 01 e4 fe+                 .bulk   $1c,$01,$e4,$fe,$c0,$fe
3e98: 00 00 70 fe+                 .bulk   $00,$00,$70,$fe,$c0,$fe
3e9e: e4 fe e4 fe+                 .bulk   $e4,$fe,$e4,$fe,$c0,$fe
3ea4: 70 fe 00 00+                 .bulk   $70,$fe,$00,$00,$c0,$fe
3eaa: e4 fe 1c 01+                 .bulk   $e4,$fe,$1c,$01,$c0,$fe
3eb0: 67           shp_v_saucer    .dd1    $67               ;shape $20
3eb1: 10 ff 00 00+                 .bulk   $10,$ff,$00,$00,$d8,$ff
3eb7: 60 ff a0 00+                 .bulk   $60,$ff,$a0,$00,$d8,$ff
3ebd: 00 00 f0 00+                 .bulk   $00,$00,$f0,$00,$d8,$ff
3ec3: a0 00 a0 00+                 .bulk   $a0,$00,$a0,$00,$d8,$ff
3ec9: f0 00 00 00+                 .bulk   $f0,$00,$00,$00,$d8,$ff
3ecf: a0 00 60 ff+                 .bulk   $a0,$00,$60,$ff,$d8,$ff
3ed5: 00 00 10 ff+                 .bulk   $00,$00,$10,$ff,$d8,$ff
3edb: 60 ff 60 ff+                 .bulk   $60,$ff,$60,$ff,$d8,$ff
3ee1: 40 fc 00 00+                 .bulk   $40,$fc,$00,$00,$50,$00
3ee7: 58 fd a8 02+                 .bulk   $58,$fd,$a8,$02,$50,$00
3eed: 00 00 c0 03+                 .bulk   $00,$00,$c0,$03,$50,$00
3ef3: a8 02 a8 02+                 .bulk   $a8,$02,$a8,$02,$50,$00
3ef9: c0 03 00 00+                 .bulk   $c0,$03,$00,$00,$50,$00
3eff: a8 02 58 fd+                 .bulk   $a8,$02,$58,$fd,$50,$00
3f05: 00 00 40 fc+                 .bulk   $00,$00,$40,$fc,$50,$00
3f0b: 58 fd 58 fd+                 .bulk   $58,$fd,$58,$fd,$50,$00
3f11: 00 00 00 00+                 .bulk   $00,$00,$00,$00,$18,$01
3f17: 97           shp_v_tank2     .dd1    $97               ;shape $21
3f18: b0 05 70 01+                 .bulk   $b0,$05,$70,$01,$c0,$fe
3f1e: 38 fe 28 02+                 .bulk   $38,$fe,$28,$02,$c0,$fe
3f24: 38 fe d8 fd+                 .bulk   $38,$fe,$d8,$fd,$c0,$fe
3f2a: b0 05 90 fe+                 .bulk   $b0,$05,$90,$fe,$c0,$fe
3f30: 38 fe c8 01+                 .bulk   $38,$fe,$c8,$01,$a4,$ff
3f36: 38 fe 38 fe+                 .bulk   $38,$fe,$38,$fe,$a4,$ff
3f3c: 48 04 00 00+                 .bulk   $48,$04,$00,$00,$ec,$fe
3f42: f0 fe 10 01+                 .bulk   $f0,$fe,$10,$01,$8c,$ff
3f48: 38 fe 10 01+                 .bulk   $38,$fe,$10,$01,$a4,$ff
3f4e: 38 fe f0 fe+                 .bulk   $38,$fe,$f0,$fe,$a4,$ff
3f54: f0 fe f0 fe+                 .bulk   $f0,$fe,$f0,$fe,$8c,$ff
3f5a: f0 fe b8 00+                 .bulk   $f0,$fe,$b8,$00,$2c,$00
3f60: 38 fe b8 00+                 .bulk   $38,$fe,$b8,$00,$2c,$00
3f66: 38 fe 48 ff+                 .bulk   $38,$fe,$48,$ff,$2c,$00
3f6c: f0 fe 48 ff+                 .bulk   $f0,$fe,$48,$ff,$2c,$00
3f72: 00 05 58 00+                 .bulk   $00,$05,$58,$00,$d4,$ff
3f78: 58 00 58 00+                 .bulk   $58,$00,$58,$00,$d4,$ff
3f7e: 58 00 a8 ff+                 .bulk   $58,$00,$a8,$ff,$d4,$ff
3f84: 00 05 a8 ff+                 .bulk   $00,$05,$a8,$ff,$d4,$ff
3f8a: 00 05 58 00+                 .bulk   $00,$05,$58,$00,$00,$00
3f90: a8 ff 58 00+                 .bulk   $a8,$ff,$58,$00,$00,$00
3f96: a8 ff a8 ff+                 .bulk   $a8,$ff,$a8,$ff,$00,$00
3f9c: 00 05 a8 ff+                 .bulk   $00,$05,$a8,$ff,$00,$00
3fa2: 38 fe 00 00+                 .bulk   $38,$fe,$00,$00,$2c,$00
3fa8: 38 fe 00 00+                 .bulk   $38,$fe,$00,$00,$14,$01
                   ; 
                   ; Velocities for the 6 chunks that fly when an enemy unit is destroyed.
                   ; 
3fae: 88 ff        chunk_x_vel     .dd2    $ff88
3fb0: 88 ff                        .dd2    $ff88
3fb2: 14 00                        .dd2    $0014
3fb4: c8 00                        .dd2    $00c8
3fb6: 00 00                        .dd2    $0000
3fb8: 60 ff                        .dd2    $ff60
3fba: 78 00        chunk_z_vel     .dd2    $0078
3fbc: 00 00                        .dd2    $0000
3fbe: ec ff                        .dd2    $ffec
3fc0: c8 00                        .dd2    $00c8
3fc2: 60 ff                        .dd2    $ff60
3fc4: 60 ff                        .dd2    $ff60
                   chunk_initial_y_vel
3fc6: 37 28 46 58+                 .bulk   $37,$28,$46,$58,$28,$42

                   ; 
                   ; Obstacle type and facing data.
                   ; 
                   ; Each entry is two bytes.  The first is the object type, the second is the
                   ; facing angle.  (All obstacles are symmetric, so every 90-degree turn
                   ; effectively returns to zero.)
                   ; 
                   ; See also the map position data at $7681 and the projectile collision diameters
                   ; at $6139.
                   ; 
                   vis
3fcc: 0c 00        obstacle_t_f    .bulk   $0c,$00           ;wide pyramind
3fce: 0f 10                        .bulk   $0f,$10           ;short box
3fd0: 0c 20                        .bulk   $0c,$20           ;wide pyramid
3fd2: 0f 40                        .bulk   $0f,$40           ;short box
3fd4: 0c 18                        .bulk   $0c,$18           ;wide pyramid
3fd6: 00 28                        .bulk   $00,$28           ;narrow pyramid
3fd8: 01 30                        .bulk   $01,$30           ;tall box
3fda: 00 38                        .bulk   $00,$38           ;narrow pyramid
3fdc: 01 40                        .bulk   $01,$40           ;tall box
3fde: 0f 48                        .bulk   $0f,$48           ;short box
3fe0: 0c 50                        .bulk   $0c,$50           ;wide pyramid
3fe2: 00 58                        .bulk   $00,$58           ;narrow pyramid
3fe4: 01 60                        .bulk   $01,$60           ;tall box
3fe6: 0f 68                        .bulk   $0f,$68           ;short box
3fe8: 0c 70                        .bulk   $0c,$70           ;wide pyramid
3fea: 00 78                        .bulk   $00,$78           ;narrow pyramid
3fec: 01 80                        .bulk   $01,$80           ;tall box
3fee: 0f 88                        .bulk   $0f,$88           ;short box
3ff0: 0c 90                        .bulk   $0c,$90           ;wide pyramid
3ff2: 00 98                        .bulk   $00,$98           ;narrow pyramid
3ff4: 01 a0                        .bulk   $01,$a0           ;tall box
3ff6: ff                           .dd1    $ff               ;end of list
                   ; 
                   ; Object types for logo display.
                   ; 
3ff7: 17 80        logo_objects    .bulk   $17,$80           ;"Ba"
3ff9: 1e 80                        .bulk   $1e,$80           ;"ttle"
3ffb: 1f 80                        .bulk   $1f,$80           ;"Zone"
                   ; 
                   ; Unused bytes.
                   ; 
3ffd: 00 00 00                     .align  $1000 (3 bytes)
                                   .adrend ↑ ~$3000
Symbol Table

Label	Value
HandleNMI	$558f
MainLoop	$5000
ResetSystem	$7acc
