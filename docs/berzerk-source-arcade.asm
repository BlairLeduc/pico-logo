;
; Berzerk (C) 1980 Stern Electronics.
;
; Reverse engineering work by Scott Tunstall, Paisley, Scotland. 
; Tools used: MAME debugger, Visual Studio Code text editor, Deepseek V4 AI.
; Date: 15 June 2026 - keep checking for updates. 
; 
; Please send any questions, corrections and updates to scott.tunstall@ntlworld.com
;
; Be sure to check out my reverse engineering work for Robotron 2084, Galaxian and Scramble too, 
; at http://seanriddle.com/robomame.asm, http://seanriddle.com/galaxian.asm and http://seanriddle.com/scramble.asm respectively.
;
; You might be interested to know that I converted Frenzy's (Berzerk's sequel) source code from 8085 to Z80 so that I could 
; identify shared logic and use Frenzy's comments to determine what each routine does. 
;
; It appears there's a lot of shared code!  
; You can find my conversion here: https://github.com/ScottTunstall/FrenzyZ80Conv
;
;
; Finally:
; If you'd like to show appreciation for this work by buying me a coffee, feel free: https://ko-fi.com/scotttunstall
; I'd be equally happy if you donated to Parkinsons UK or Chest Heart And Stroke (CHAS) Scotland.
; Thanks. 

/*
Conventions: 

NUMBERS
=======

The term "@ $" means "at memory address in hexadecimal". 
e.g. @ $1234 means "refer to memory address 1234" or "program code @ memory location 1234" 

The term "#$" means "immediate value in hexadecimal". It's a convention I have kept from 6502 days.
e.g. #$60 means "immediate value of 60 hex" (96 decimal)

If I don't prefix a number with $ or #$ in my comments, treat the value as a decimal number.


ARRAYS, LISTS, TABLES
=====================

The terms "entry", "slot", "item", "record" when used in an array, list or table context all mean the same thing.
I try to be consistent with my terminology but I might not always succeed.

"Length" when used in terms of an array refers to how many elements it contains. 

SizeOf refers to the size in bytes of a structure (as it does in C).

Unless I specify otherwise, I all indexes into arrays/lists/tables are zero-based, 
meaning element [0] is the first element, [1] the second, [2] the third and so on.


FLAGS
=====

The terms "Clear", "Reset", "Unset" in a flag context all mean the flag is set to zero.
A non-zero value in a flag means the flag is "set" - unless I specify otherwise.
                                                                               

COORDINATES
===========

X,Y refer to the X and Y axis in a Cartesian 2D coordinate system, where X is horizontal and Y is vertical.


TODOs:
======
TODOs are for me to look at later, when a piece of code isn't immediately obvious.
Anyone who knows me, knows I like leaving TODOs in code :)


UNCOMMENTED CODE:
I never really bother with reverse engineering code specifically for cocktail cabinets. I just don't see any useful return on the time invested.

*/

/*

MEMORY MAP 

Taken from https://github.com/mamedev/mame/blob/master/src/mame/drivers/berzerk.cpp


0000 - 07ff	2k	0-7	Program PROM (1C)
0800 - 09ff	1/2 k	4-7	CMOS RAM BBKUP
0800 - 0bff	1k	0-3	scratch pad RAM
0a00 - 0bff	1/2 k	4-7	Optional CMOS RAM

1000 - 17ff	2k	0-7	Program PROM (1D)
1800 - 1fff	2k	0-7	Program PROM (3D)
2000 - 27ff	2k	0-7	Program PROM (4D)
2800 - 2fff	2k	0-7	Program PROM (6D)
3000 - 37ff	2k	0-7	Program PROM (4C)
3800 - 3fff	2k	0-7	Program PROM (3C)

4000 - 43ff	1k	0-7	scratch pad RAM

Video RAM - 32 bytes per scan line. 1 bit per pixel. 
4400 - 5fff	7k	0-7	screen image RAM
				(256 * 223)

6000 - 63ff	1k	0-7	Magic scratchpad RAM

6400 - 7fff	7k	0-7	Magic image RAM

8000 - 87ff	2k	0-7	Color look-up
				(RGBI,RGBI)
				0-3ff top half of screen
				(first 128 lines)
				400-fff bottom half of screen
				Maps 4x4 color boxes into one
                                of 16 colors



void berzerk_state::berzerk_io_map(address_map &map)
{
	map.global_mask(0xff)                       ;
	map(0x00, 0x3f).noprw()                     ;
	map(0x40, 0x47).rw(FUNC(berzerk_state::audio_r), FUNC(berzerk_state::audio_w)) ;
	map(0x48, 0x48).portr("P1").nopw()          ;
	map(0x49, 0x49).portr("SYSTEM").nopw()      ;
	map(0x4a, 0x4a).portr("P2").nopw()          ;
	map(0x4b, 0x4b).nopr().w(FUNC(berzerk_state::magicram_control_w)) ;
	map(0x4c, 0x4c).rw(FUNC(berzerk_state::nmi_enable_r), FUNC(berzerk_state::nmi_enable_w)) ;
	map(0x4d, 0x4d).rw(FUNC(berzerk_state::nmi_disable_r), FUNC(berzerk_state::nmi_disable_w)) ;
	map(0x4e, 0x4e).r(FUNC(berzerk_state::intercept_v256_r)).nopw() ; // note reading from here should clear pending frame interrupts, see zfb-1.tiff 74ls74 at 3D pin 13 /CLR
	map(0x4f, 0x4f).nopr().w(FUNC(berzerk_state::irq_enable_w)) ;
	map(0x50, 0x57).noprw()                     ; /* second sound board, initialized but not used */
	map(0x58, 0x5f).noprw()                     ;
	map(0x60, 0x60).mirror(0x18).portr("F3").nopw() ;
	map(0x61, 0x61).mirror(0x18).portr("F2").nopw() ;
	map(0x62, 0x62).mirror(0x18).portr("F6").nopw() ;
	map(0x63, 0x63).mirror(0x18).portr("F5").nopw() ;
	map(0x64, 0x64).mirror(0x18).portr("F4").nopw() ;
	map(0x65, 0x65).mirror(0x18).portr("SW2").nopw() ;
	map(0x66, 0x66).mirror(0x18).rw(FUNC(berzerk_state::led_off_r), FUNC(berzerk_state::led_off_w)) ;
	map(0x67, 0x67).mirror(0x18).rw(FUNC(berzerk_state::led_on_r), FUNC(berzerk_state::led_on_w)) ;
	map(0x80, 0xff).noprw()                     ;
}


// MAGIC RAM control register:
// -- These are early investigation results and subject to change --
//Bit 3 set:           Flip horizontal 
//Bit 4 and Bit 7 set: XOR mode 


/*************************************
 *
 *  Port definitions
 *
 *************************************/

#define BERZERK_COINAGE(CHUTE, DIPBANK) \
	PORT_DIPNAME( 0x0f, 0x00, "Coin "#CHUTE )  PORT_DIPLOCATION(#DIPBANK":1,2,3,4") \
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x0d, DEF_STR( 4C_3C ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0x0e, DEF_STR( 4C_5C ) ) \
	PORT_DIPSETTING(    0x0a, DEF_STR( 2C_3C ) ) \
	PORT_DIPSETTING(    0x0f, DEF_STR( 4C_7C ) ) \
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) ) \
	PORT_DIPSETTING(    0x0b, DEF_STR( 2C_5C ) ) \
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_3C ) ) \
	PORT_DIPSETTING(    0x0c, DEF_STR( 2C_7C ) ) \
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_4C ) ) \
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_5C ) ) \
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_6C ) ) \
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_7C ) ) \
	PORT_DIPSETTING(    0x07, "1 Coin/10 Credits" ) \
	PORT_DIPSETTING(    0x08, "1 Coin/14 Credits" )


static INPUT_PORTS_START( joystick ) // used on all games except moonwarp
	PORT_START("P1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_8WAY
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_8WAY
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_8WAY
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_8WAY
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("P2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_8WAY PORT_COCKTAIL
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_8WAY PORT_COCKTAIL
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_8WAY PORT_COCKTAIL
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_8WAY PORT_COCKTAIL
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_COCKTAIL
	PORT_BIT( 0x60, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
INPUT_PORTS_END

static INPUT_PORTS_START( common ) // used on all games
	PORT_START("SYSTEM")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x1c, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	/* fake port for monitor type */
	PORT_START("MONITOR_TYPE")
	PORT_CONFNAME( 0x01, 0x00, "Monitor Type" )
	PORT_CONFSETTING(    0x00, "Wells-Gardner" )
	PORT_CONFSETTING(    0x01, "Electrohome" )
	PORT_BIT( 0xfe, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("SW2")
	/* port for the 'bookkeeping reset' and 'bookkeeping' buttons ;
	 * The 'bookkeeping reset' button is an actual button on the zpu-1000 and
	 * zpu-1001 pcbs, labeled 'S2' or 'SW2'. It is wired to bit 0.
	 * * pressing it while high scores are displayed will give a free game
	 *   without adding any coin info to the bookkeeping info in nvram.
	 * The 'bookkeeping' button is wired to the control panel, usually hidden
	 * underneath or only accessible through the coin door. Wired to bit 7.
	 * * It displays various bookkeeping statistics when pressed sequentially.
	 *   Pressing P1 fire (according to the manual) when stats are displayed
	 *   will clear the stat shown on screen.
	 */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_NAME("Free Game (not logged in bookkeeping)")
	PORT_BIT( 0x7e, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_SERVICE2 ) PORT_NAME("Bookkeeping") PORT_CODE(KEYCODE_F1)
INPUT_PORTS_END

static INPUT_PORTS_START( berzerk )
	PORT_INCLUDE( joystick )
	PORT_INCLUDE( common )

	PORT_START("F2")
	PORT_DIPNAME( 0x03, 0x00, "Color Test" ) PORT_CODE(KEYCODE_F5) PORT_TOGGLE PORT_DIPLOCATION("F2:1,2")
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x03, DEF_STR( On ) )
	PORT_BIT( 0x3c, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Bonus_Life ) ) PORT_DIPLOCATION("F2:7,8")
	PORT_DIPSETTING(    0xc0, "5000 and 10000" )
	PORT_DIPSETTING(    0x40, "5000" )
	PORT_DIPSETTING(    0x80, "10000" )
	PORT_DIPSETTING(    0x00, DEF_STR( None ) )

	PORT_START("F3")
	PORT_DIPNAME( 0x01, 0x00, "Input Test Mode" ) PORT_CODE(KEYCODE_F2) PORT_TOGGLE PORT_DIPLOCATION("F3:1")
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, "Crosshair Pattern" ) PORT_CODE(KEYCODE_F4) PORT_TOGGLE PORT_DIPLOCATION("F3:2")
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x02, DEF_STR( On ) )
	PORT_BIT( 0x3c, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Language ) ) PORT_DIPLOCATION("F3:7,8")
	PORT_DIPSETTING(    0x00, DEF_STR( English ) )
	PORT_DIPSETTING(    0x40, DEF_STR( German ) )
	PORT_DIPSETTING(    0x80, DEF_STR( French ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( Spanish ) )

	PORT_START("F4")
	BERZERK_COINAGE(1, F4)
	PORT_BIT( 0xf0, IP_ACTIVE_LOW,  IPT_UNUSED )

	PORT_START("F5")
	BERZERK_COINAGE(2, F5)
	PORT_BIT( 0xf0, IP_ACTIVE_LOW,  IPT_UNUSED )

	PORT_START("F6")
	BERZERK_COINAGE(3, F6)
	PORT_BIT( 0xf0, IP_ACTIVE_LOW,  IPT_UNUSED )
INPUT_PORTS_END

*/



/*
Direction bit EQUs, - referred to as DURL bits in this document and the Frenzy source code.

Note: DURL = Down Up Right Left :)
*/

LEFT                        EQU $01
RIGHT                       EQU $02
UP                          EQU $04
DOWN                        EQU $08


/* Speech synthesis EQUs

Berzerk has a vocabulary of 30 words. They are listed below.

See also:
- NMI handler @ $1748 which instructs the voice hardware to emit speech;
- talk.asm within Frenzy's source code.
*/

KILL                        EQU $01
ATTACK                      EQU $02
CHARGE                      EQU $03
GOT                         EQU $04
SHOOT                       EQU $05
GET                         EQU $06
IS                          EQU $07
ALERT                       EQU $08
DETECTED                    EQU $09
THE                         EQU $0A
IN                          EQU $0B
IT                          EQU $0C
THERE                       EQU $0D
WHERE                       EQU $0E
HUMANOID                    EQU $0F
COINS                       EQU $10
POCKET                      EQU $11
INTRUDER                    EQU $12 
NO                          EQU $13
ESCAPE                      EQU $14
DESTROY                     EQU $15
MUST                        EQU $16
NOT                         EQU $17
CHICKEN                     EQU $18 
FIGHT                       EQU $19 
LIKE                        EQU $1A
A                           EQU $1B
ROBOT                       EQU $1C
???                         EQU $1D


// Memory addresses

// Audio ports
CR1_PORT                    EQU $40
CR2_PORT                    EQU $41
VOICE_PORT                  EQU $44                 ; Voice port number


UNKNOWN_0800                EQU $0800
NMI_STACK_PTR               EQU $085E               ; used by NMI handler to store SP register while it does its thing 


; pointer to a list of robot VECTOR pointers. See $15AB.
V.PTR                       EQU $0870               


;
; Cooperative multitasking job system.
;
; Berzerk uses a coroutine-based scheduler where each game entity (player,
; robots, Evil Otto) runs as a "job." Jobs allocate checkpoint nodes on the
; Z80 stack and voluntarily yield to the runner, which round-robins through
; the linked list and resumes the next active job.
;
; Each job follows this pattern:
;   1. CREATE_JOB — allocate a node on the stack
;   2. ACTIVATE_HEAD_JOB — mark it runnable with a delay
;   3. Main loop: do work, then call STOP_JOB to checkpoint and yield
;   4. The runner eventually circles back and resumes the job
;
; Key addresses:
;   TIMER_LIST_PTR ($0872) — head of the linked list of job nodes
;   CREATE_JOB  ($1E22)    — allocate a new node, insert as head
;   ACTIVATE_HEAD_JOB ($1E6D) — mark head node runnable
;   STOP_JOB    ($1E78)    — save SP, yield to runner
;   Runner      ($1E8B)    — walk list, find active job, resume it
;

struct TIMER_ITEM
{
    BYTE Flags
    BYTE Delay
    BYTE NextItemLo
    BYTE NextItemHi
}
TIMER_LIST_PTR              EQU $0872               ; pointer to head TIMER_ITEM

STACK_PTR                   EQU $0874               ; used to preserve value of SP register in interrupt handler. See $26AC.
MAN_PTR                     EQU $0876               ; pointer to current player's VECTOR structure

; Sound chip phantom registers (see also: EQUS.ASM within Frenzy's source code)
TCR1                        EQU $0878
TCR2	                    EQU $0879
TCR3	                    EQU $087A
TMR1	                    EQU $087B
TMR2	                    EQU $087D
TMR3	                    EQU $087F
NOISE	                    EQU $0881
VOL1	                    EQU $0882
VOL2	                    EQU $0883
VOL3	                    EQU $0884

PC0                         EQU $0885
PC1                         EQU $0889                       


IS_CHICKEN                  EQU $089A               ; A flag that determines if the robots call you a chicken or not. 0 = Player not a chicken. $FF = Player is a chicken. See $2BC6 and $2C18.
TALK_TIMER                  EQU $089B               ; Countdown before a robot speaks. When this hits 0, a robot will say something. See $2B9D.  
SPEECH_BUFFER               EQU $0918               ; holds dynamically generated sequence of bytes to output to voice. See $2b6B. 
PLAYER_COLOUR_ADDR          EQU $0940               ; pointer to attribute RAM where player's man is. Maps to CAddr within equs.asm in Frenzy's source code.
PLAYER_COLOUR_SAVE          EQU $0942               ; buffer 10 bytes in size, used to hold attributes to be restored.


; Bookkeeping totals held in CMOS
; Upper nibble (bits 4-7) hold 1 digit of value
CMOS_CREDITS                EQU $08A4  
CMOS_CHUTE1                 EQU $08A6
CMOS_CHUTE2                 EQU $08AE
CMOS_CHUTE3                 EQU $08B6
CMOS_NUM_PLAYS              EQU $08BE               
CMOS_TOTAL_SCORE            EQU $08C4
CMOS_TOTAL_SECS_OF_PLAY     EQU $08D0
CMOS_HIGH_SCORES            EQU $08DC


// Scratch pad RAM EQUs



???                         EQU $4066


; VECTOR.Status bit definitions. Names from Frenzy's equs.asm.
; Berzerk differs from Frenzy: HIT is bit 7 here (Frenzy uses bit 6
; with InUse at bit 7). Bit 6 is unused in Berzerk.
;
ERASE   EQU 0   ; pattern needs XOR-erase from old position
WRITE   EQU 1   ; pattern needs to be drawn at new position
MOVE    EQU 2   ; object is moving (velocity applied each tick)
BLANK   EQU 3   ; player colour was drawn here, needs restoring
COLOR   EQU 4   ; player colour should be drawn here
INEPT   EQU 5   ; non-bolt hit / death animation flag
HIT     EQU 7   ; object was struck by a bolt (Frenzy: bit 6)


;
; Structure that encapsulates state required for an animated sprite, such as the player and robots.
; Or, in Plain English, a data structure that holds information about a sprite on screen.
;
; Note:
; [Laser] Bolts do not use VECTORS, they use the BOLT structure instead.
;

struct VECTOR
{
0    BYTE Status                             ; status bits that determine how this object is rendered. See "status bits" above for info.
1    BYTE Magic                              ; last magic RAM value used for this object
2    BYTE O.A.L                              ; old screen address low byte
3    BYTE O.A.H                              ; old screen address high byte
4    BYTE O.P.L                              ; old pattern (sprite pixel data) address low byte
5    BYTE O.P.H                              ; old pattern address high byte
6    BYTE V.X                                ; X velocity. A signed delta to add to P.X  
7    BYTE P.X                                ; Current X position on screen
8    BYTE V.Y                                ; Y velocity. A signed delta to add to P.Y 
9    BYTE P.Y                                ; Current Y position on screen
10   BYTE D.P.L                              ; low byte of a pointer to an entry in a pattern table (used for animation)
11   BYTE D.P.H                              ; high byte of a pointer to an entry in a pattern table (used for animation)
12   BYTE TIME                               ; Counter used to limit how fast this object moves. Lower = faster  
13   BYTE TPRIME                             ; Used to reload TIME when TIME counts to 0.
} sizeof (VECTOR) = 14 bytes




                            EQU $415E


                            EQU $4300

; High score table. 
;
; A high score requires exactly 6 bytes per entry. There is a maximum of 10 high score entries. 
; 
; Format of each high score entry:
; First 3 bytes: score as BCD. e.g. a score of 123,456 would be stored like so: 12 34 56
; Last 3 bytes: name entered as (mostly) ASCII. 

HI_SCORES                   EQU $4302               ; Start of high score table
HI_SCORE_1                  EQU $4302               ; Top score
HI_SCORE_2                  EQU $4308
HI_SCORE_3                  EQU $430E
HI_SCORE_4                  EQU $4314
HI_SCORE_5                  EQU $431A
HI_SCORE_6                  EQU $4320
HI_SCORE_7                  EQU $4326
HI_SCORE_8                  EQU $432C
HI_SCORE_9                  EQU $4332
HI_SCORE_10                 EQU $4338               ; lowest Hi Score


P1_SCORE                    EQU $433E               ; Player 1's score. Stored as BCD.  
                                             ; $433E = First two digits of score (hundred thousands & ten thousands) 
                                             ; $433F = Third and fourth digits of score (thousands & hundreds)
                                             ; $4340 = Last two digits of score (tens)   
                                             ; e.g. a score of 101120 would be stored as 10 11 20
P2_SCORE                    EQU $4341               ; Player 2's score. Stored in same format as player 1's.

; Player state.
;
; Remarks: 
; The corresponding fields for Frenzy are stored in EQUS.ASM if you want to take a look.
;
CURRENT_PLAYER              EQU $4344               ; 1 = player 1 playing , 2 = player 2 playing. 
ROOM_X                      EQU $4345               ; X coordinate of room within *maze* - decrements when player exits room left, increments when player exits right 
ROOM_Y                      EQU $4346               ; Y coordinate of room within *maze* - decrements when player exits room top, increments when player exits bottom 
MAN_X                       EQU $4347               ; Player's X coordinate on screen. 
MAN_Y                       EQU $4348               ; Player's Y coordinate on screen
DEATHS                      EQU $4349               ; Lives left. 
                            EQU $434A
RBOLTS                      EQU $434B               ; Maximum number of Robot laser bolts allowed on screen at once. 0 = Robots never shoot. See $288E.
ROBOT_SPEED                 EQU $434C               ; Value from 1..255. 1= Fastest, 255 = Slowest
RWAIT                       EQU $434D               ; Initial robot firing holdoff timer. 
OTTO_TIME                   EQU $434E               ; Countdown timer before Evil Otto appears. 
XTRAMEN                     EQU $434F               ; Flag. Set to 1 when a bonus life has been awarded to player. Prevents more bonus lives.   

RNG_SEED                    EQU $435C               ; Seed for the random number generator @ $2678
MAZE_ZONES                  EQU $435E               ; 15-byte maze zone attribute grid (5 cols x 3 rows).
                                             ; Initialised from the template at $268C by an LDIR @ $2567.
                                             ; Bit 0 ($01) = wall on LEFT edge  | Bit 1 ($02) = wall on RIGHT edge
                                             ; Bit 2 ($04) = wall on TOP edge   | Bit 3 ($08) = wall on BOTTOM edge
                                             ; Confirmed by $25EB (CREATE_ROOM) and $1C6E (IQ).

UPDATE                      EQU $436D               ; Flag. Set to $FF when score needs to be updated on screen. See $218D
IS_DEMO_MODE                EQU $436E               ; Flag. When set to a nonzero value, the game is showing "demo mode".   
DEMO_PTR                    EQU $436F
RCOUNT                      EQU $4371               ; Number of robots still alive in room, not counting Evil Otto. Is decremented when a robot is killed.
RSAVED                      EQU $4372               ; Number of robots in room *when room starts*. Used to calculate bonus for clearing room. See $2491.
NUMBER_OF_PLAYERS           EQU $4376               ; 1 = One player game, 2 = 2 player game
PLAYER_COLOUR               EQU $4378               ; Colour of player. Used to set screen attributes.
FLIP                        EQU $4379               ; 0 = Playing upright, 8 = playing on cocktail cabinet. 


;
; Data structure that holds information about a laser bolt on screen, such as its position.
;
; Remarks:
; Bolts are programatically plotted pixel-by-pixel on screen, starting from the "head" at X,Y 
; and ending at the "tail" at TailX, TailY. 
; They do not have pattern data in ROM.
;
; See also: bolts.asm in Frenzy's source code.

struct BOLT
{
0   BYTE Direction                           ; DURL (Direction) bits. bit 0 set: left, bit 1 set: right, bit 2 set: up, bit 3 set: down
1   BYTE Length                              ; current length of bolt.
2   BYTE X                                   ; X coordinate of the head of the bolt
3   BYTE Y                                   ; Y coordinate of the head of the bolt
4   BYTE LastDirection      
5   BYTE MaxLength                           ; max length of bolt (pixels) - see $1f7C 
6   BYTE TailX                               ; X coordinate of the tail of the bolt
7   BYTE LastY                               ; Y coordinate of the tail of the bolt  
} sizeof(BOLT) is 8 bytes

PLAYER_BOLTS                EQU $437B
    PLAYER_BOLT_1           EQU $437B        
    PLAYER_BOLT_2           EQU $4383               

ROBOT_BOLTS                 EQU $438F





0000: 00          nop
0001: F3          di
0002: AF          xor  a
0003: D3 4D       out  ($4D),a               ; write to nmi_disable_r
0005: ED 47       ld   i,a
0007: DB 61       in   a,($61)               ; read from F2
0009: CB 47       bit  0,a                   ; check if "Colour Test Mode" bit is set
000B: C2 00 02    jp   nz,$0200              ; if bit is set, goto COLOUR_TEST_MODE
000E: DB 60       in   a,($60)               ; read F3
0010: CB 47       bit  0,a                   ; check if "Input Test Mode" bit is set
0012: C2 31 05    jp   nz,$0531              ; if bit is set, goto INPUT_TEST_MODE
0015: DD 21 41 01 ld   ix,$0141
0019: 18 68       jr   $0083


001B: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
001E: 
    90
    20 08                                    ; X/Y coordinates

0021:  43 6F 6E 67 72 61 74 75 6C 61 74 69 6F 6E 73 20  Congratulations 
0031:  50 6C 61 79 65 72 20 00                          Player . 
0039: C9          ret

003A: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
003D: 
    90          
    10 08                                    ; X/Y coordinates  

0040:  46 65 6C 69 63 69 74 61 74 69 6F 6E 73 20 61 75  Felicitations au
0050:  20 6A 6F 75 65 75 72 20 00                       joueur .
0059: C9          ret

005A: FF          rst  $38
005B: FF          rst  $38
005C: FF          rst  $38
005D: FF          rst  $38
005E: FF          rst  $38
005F: FF          rst  $38
0060: FF          rst  $38
0061: FF          rst  $38
0062: FF          rst  $38
0063: FF          rst  $38
0064: FF          rst  $38
0065: FF          rst  $38

// NMI handler
0066: D3 4D       out  ($4D),a               ; write to nmi_disable_r
0068: F5          push af
0069: 3A 00 40    ld   a,($4000)
006C: B7          or   a
006D: C2 1D 05    jp   nz,$051D
0070: F1          pop  af
0071: C3 21 17    jp   $1721                 ; jump to main NMI handler code

0074: 05          dec  b

; Referenced by $0199
0075: 00 08          

; Referenced by $019C
0077: A3 00

0079: 01 00 00    ld   bc,$0000
007C: 0D          dec  c
007D: 20 FD       jr   nz,$007C
007F: 10 FB       djnz $007C
0081: DB 66       in   a,($66)               ; read led_off_r

0083: 3E 01       ld   a,$01
0085: 01 41 01    ld   bc,$0141
0088: 11 47 82    ld   de,$8247
008B: ED 41       out  (c),b
008D: 0D          dec  c
008E: ED 51       out  (c),d
0090: 0C          inc  c
0091: 0C          inc  c
0092: ED 41       out  (c),b
0094: 0C          inc  c
0095: ED 41       out  (c),b
0097: 0C          inc  c
0098: 0C          inc  c
0099: 0C          inc  c
009A: ED 59       out  (c),e
009C: 0E 51       ld   c,$51
009E: 3D          dec  a
009F: 28 EA       jr   z,$008B
00A1: 01 00 00    ld   bc,$0000
00A4: 0D          dec  c
00A5: 20 FD       jr   nz,$00A4
00A7: 10 FB       djnz $00A4
00A9: DB 67       in   a,($67)               ; read led_on_r
00AB: AF          xor  a
00AC: D3 40       out  ($40),a               ; write to audio_w
00AE: D3 50       out  ($50),a               ; second sound board, unused
00B0: DD E9       jp   (ix)

00B2: FF          rst  $38
00B3: FF          rst  $38
00B4: FF          rst  $38
00B5: FF          rst  $38
00B6: FF          rst  $38
00B7: FF          rst  $38
00B8: FF          rst  $38
00B9: FF          rst  $38
00BA: FF          rst  $38
00BB: FF          rst  $38
00BC: FF          rst  $38
00BD: FF          rst  $38
00BE: FF          rst  $38
00BF: FF          rst  $38
00C0: FF          rst  $38
00C1: FF          rst  $38
00C2: FF          rst  $38
00C3: FF          rst  $38
00C4: FF          rst  $38
00C5: FF          rst  $38
00C6: FF          rst  $38
00C7: FF          rst  $38
00C8: FF          rst  $38
00C9: FF          rst  $38
00CA: FF          rst  $38
00CB: FF          rst  $38
00CC: FF          rst  $38
00CD: FF          rst  $38
00CE: FF          rst  $38
00CF: FF          rst  $38
00D0: FF          rst  $38
00D1: FF          rst  $38
00D2: FF          rst  $38
00D3: FF          rst  $38
00D4: FF          rst  $38
00D5: FF          rst  $38
00D6: FF          rst  $38
00D7: FF          rst  $38
00D8: FF          rst  $38
00D9: FF          rst  $38
00DA: FF          rst  $38
00DB: FF          rst  $38
00DC: FF          rst  $38
00DD: FF          rst  $38
00DE: FF          rst  $38
00DF: FF          rst  $38
00E0: FF          rst  $38
00E1: FF          rst  $38
00E2: FF          rst  $38
00E3: FF          rst  $38
00E4: FF          rst  $38
00E5: FF          rst  $38
00E6: FF          rst  $38
00E7: FF          rst  $38
00E8: FF          rst  $38
00E9: FF          rst  $38
00EA: FF          rst  $38
00EB: FF          rst  $38
00EC: FF          rst  $38
00ED: FF          rst  $38
00EE: FF          rst  $38
00EF: FF          rst  $38
00F0: FF          rst  $38
00F1: FF          rst  $38
00F2: FF          rst  $38
00F3: FF          rst  $38
00F4: FF          rst  $38
00F5: FF          rst  $38
00F6: FF          rst  $38
00F7: FF          rst  $38
00F8: FF          rst  $38
00F9: FF          rst  $38
00FA: FF          rst  $38
00FB: FF          rst  $38
00FC: FF          rst  $38
00FD: FF          rst  $38
00FE: FF          rst  $38
00FF: FF          rst  $38

0100: 18 3B       jr   $013D
0102: 01 C0 08    ld   bc,$08C0
0105: 21 00 00    ld   hl,$0000
0108: 16 08       ld   d,$08
010A: 7E          ld   a,(hl)
010B: 09          add  hl,bc
010C: AF          xor  a
010D: ED 4F       ld   r,a
010F: 32 00 10    ld   ($1000),a
0112: 15          dec  d
0113: 20 F5       jr   nz,$010A
0115: 0E 7F       ld   c,$7F
0117: 16 20       ld   d,$20
0119: 7B          ld   a,e
011A: E6 20       and  $20
011C: 47          ld   b,a
011D: ED 78       in   a,(c)
011F: CB 0B       rrc  e
0121: 0D          dec  c
0122: 15          dec  d
0123: 20 F4       jr   nz,$0119
0125: 3E 80       ld   a,$80
0127: 01 57 80    ld   bc,$8057
012A: ED 79       out  (c),a
012C: 05          dec  b
012D: 0D          dec  c
012E: CB 0F       rrc  a
0130: 30 F8       jr   nc,$012A
0132: 0E 47       ld   c,$47
0134: ED 79       out  (c),a
0136: 0D          dec  c
0137: CB 0F       rrc  a
0139: 30 F9       jr   nc,$0134
013B: 18 C5       jr   $0102
013D: 3E 01       ld   a,$01
013F: ED 47       ld   i,a
0141: 1E 06       ld   e,$06
0143: DD 21 00 10 ld   ix,$1000
0147: 01 00 08    ld   bc,$0800
014A: 26 00       ld   h,$00
014C: 2E FF       ld   l,$FF
014E: DD 7E 00    ld   a,(ix+$00)
0151: 57          ld   d,a
0152: A5          and  l
0153: 6F          ld   l,a
0154: 7A          ld   a,d
0155: 84          add  a,h
0156: 67          ld   h,a
0157: DD 23       inc  ix
0159: 0D          dec  c
015A: 20 F2       jr   nz,$014E
015C: 10 F0       djnz $014E
015E: 3A 74 00    ld   a,($0074)
0161: FE FF       cp   $FF
0163: 28 25       jr   z,$018A
0165: 83          add  a,e
0166: D6 07       sub  $07
0168: F2 6F 01    jp   p,$016F
016B: 7D          ld   a,l
016C: 3C          inc  a
016D: 28 05       jr   z,$0174
016F: 3E FF       ld   a,$FF
0171: BC          cp   h
0172: 20 16       jr   nz,$018A
0174: 7B          ld   a,e
0175: CB 07       rlc  a
0177: DA 92 01    jp   c,$0192
017A: 1D          dec  e
017B: 20 CA       jr   nz,$0147
017D: ED 57       ld   a,i
017F: A7          and  a
0180: 20 17       jr   nz,$0199
0182: DD 21 00 00 ld   ix,$0000
0186: 1E 80       ld   e,$80
0188: 18 BD       jr   $0147

018A: ED 57       ld   a,i
018C: A7          and  a
018D: C2 02 01    jp   nz,$0102
0190: 18 FE       jr   $0190

0192: DD 21 99 01 ld   ix,$0199
0196: C3 79 00    jp   $0079

0199: 2A 75 00    ld   hl,($0075)            ; load HL with 40800  
019C: ED 4B 77 00 ld   bc,($0077)            ; load BC with #$00A3 (163 decimal)
01A0: 36 55       ld   (hl),$55              ; write to UNKNOWN_0800
01A2: 2B          dec  hl
01A3: ED A1       cpi
01A5: E2 AB 01    jp   po,$01AB
01A8: 23          inc  hl
01A9: 18 F5       jr   $01A0

01AB: 16 AA       ld   d,$AA
01AD: 31 FF FF    ld   sp,$FFFF
01B0: ED 4B 77 00 ld   bc,($0077)
01B4: 7A          ld   a,d
01B5: 2F          cpl
01B6: AE          xor  (hl)
01B7: 20 16       jr   nz,$01CF
01B9: 72          ld   (hl),d
01BA: 2B          dec  hl
01BB: ED A1       cpi
01BD: EA CC 01    jp   pe,$01CC
01C0: 7A          ld   a,d
01C1: FE 55       cp   $55
01C3: 28 25       jr   z,$01EA
01C5: 31 01 00    ld   sp,$0001
01C8: 16 55       ld   d,$55
01CA: 18 E4       jr   $01B0
01CC: 39          add  hl,sp
01CD: 18 E5       jr   $01B4
01CF: 57          ld   d,a
01D0: ED 57       ld   a,i
01D2: CB 0F       rrc  a
01D4: 38 01       jr   c,$01D7
01D6: 76          halt
01D7: 1E 12       ld   e,$12
01D9: 7A          ld   a,d
01DA: E6 0F       and  $0F
01DC: CA 02 01    jp   z,$0102
01DF: 1D          dec  e
01E0: 7A          ld   a,d
01E1: E6 F0       and  $F0
01E3: CA 02 01    jp   z,$0102
01E6: 1D          dec  e
01E7: C3 02 01    jp   $0102

01EA: ED 57       ld   a,i
01EC: CB 0F       rrc  a
01EE: 1E 20       ld   e,$20
01F0: D2 B1 02    jp   nc,$02B1
01F3: C3 02 01    jp   $0102

01F6: FF          rst  $38
01F7: FF          rst  $38
01F8: FF          rst  $38
01F9: FF          rst  $38
01FA: FF          rst  $38
01FB: FF          rst  $38
01FC: FF          rst  $38
01FD: FF          rst  $38
01FE: FF          rst  $38
01FF: FF          rst  $38



;
; Called from $000B
;

COLOUR_TEST_MODE:
0200: 01 48 10    ld   bc,$1048
0203: ED 78       in   a,(c)
0205: 0C          inc  c
0206: ED 78       in   a,(c)
0208: 0C          inc  c
0209: ED 78       in   a,(c)
020B: 0C          inc  c
020C: 0C          inc  c
020D: ED 78       in   a,(c)
020F: 0C          inc  c
0210: ED 78       in   a,(c)
0212: 0C          inc  c
0213: ED 78       in   a,(c)
0215: 0C          inc  c
0216: 3E 01       ld   a,$01
0218: ED 79       out  (c),a
021A: 01 48 00    ld   bc,$0048
021D: ED 78       in   a,(c)
021F: 0C          inc  c
0220: ED 78       in   a,(c)
0222: 0C          inc  c
0223: ED 78       in   a,(c)
0225: 21 00 50    ld   hl,$5000
0228: 11 00 70    ld   de,$7000
022B: 06 10       ld   b,$10
022D: 78          ld   a,b
022E: 3D          dec  a
022F: D3 4B       out  ($4B),a               ; write to magicram_control_w
0231: 3E 80       ld   a,$80
0233: 77          ld   (hl),a
0234: 12          ld   (de),a
0235: 4E          ld   c,(hl)
0236: CB 0F       rrc  a
0238: 30 F9       jr   nc,$0233
023A: AF          xor  a
023B: DB 4E       in   a,($4E)               ; read intercept_v256_r
023D: 3E 08       ld   a,$08
023F: 32 00 50    ld   ($5000),a
0242: 32 00 70    ld   ($7000),a
0245: AF          xor  a
0246: DB 4E       in   a,($4E)               ; read intercept_v256_r
0248: 23          inc  hl
0249: 13          inc  de
024A: 10 E1       djnz $022D
024C: 06 0D       ld   b,$0D
024E: 11 00 A0    ld   de,$A000
0251: 21 FE 5F    ld   hl,$5FFE
0254: 3E 80       ld   a,$80
0256: 25          dec  h
0257: 77          ld   (hl),a
0258: 4E          ld   c,(hl)
0259: 24          inc  h
025A: 37          scf
025B: CB 15       rl   l
025D: CB 14       rl   h
025F: 19          add  hl,de
0260: CB 0F       rrc  a
0262: 10 F2       djnz $0256
0264: 21 11 11    ld   hl,$1111
0267: 11 11 11    ld   de,$1111
026A: 31 00 88    ld   sp,$8800
026D: 0E 10       ld   c,$10
026F: 06 10       ld   b,$10
0271: E5          push hl
0272: E5          push hl
0273: E5          push hl
0274: E5          push hl
0275: 10 FA       djnz $0271
0277: 19          add  hl,de
0278: F1          pop  af
0279: 3B          dec  sp
027A: 3B          dec  sp
027B: 0D          dec  c
027C: 20 F1       jr   nz,$026F
027E: DB 4C       in   a,($4C)
0280: AF          xor  a
0281: DB 4E       in   a,($4E)               ; read intercept_v256_r
0283: AF          xor  a
0284: DB 61       in   a,($61)               ; read from F2
0286: E6 02       and  $02
0288: 28 FE       jr   z,$0288
028A: 21 00 50    ld   hl,$5000
028D: 11 00 70    ld   de,$7000
0290: 3E F0       ld   a,$F0
0292: ED 47       ld   i,a
0294: 01 4B 00    ld   bc,$004B
0297: ED 57       ld   a,i
0299: ED 79       out  (c),a
029B: 0E 00       ld   c,$00
029D: 79          ld   a,c
029E: 70          ld   (hl),b
029F: 12          ld   (de),a
02A0: 48          ld   c,b
02A1: 2F          cpl
02A2: 46          ld   b,(hl)
02A3: 47          ld   b,a
02A4: B1          or   c
02A5: 20 F6       jr   nz,$029D
02A7: ED 57       ld   a,i
02A9: D6 10       sub  $10
02AB: ED 47       ld   i,a
02AD: 20 E5       jr   nz,$0294
02AF: 18 FE       jr   $02AF                 ; enter infinite loop!

02B1: DD 21 B8 02 ld   ix,$02B8
02B5: C3 79 00    jp   $0079

02B8: 21 FF 5F    ld   hl,$5FFF
02BB: 11 00 00    ld   de,$0000
02BE: DD 21 C5 02 ld   ix,$02C5
02C2: C3 95 03    jp   $0395

02C5: 01 00 00    ld   bc,$0000
02C8: 21 00 40    ld   hl,$4000
02CB: DD 21 D2 02 ld   ix,$02D2
02CF: C3 76 03    jp   $0376

02D2: 21 00 40    ld   hl,$4000
02D5: 11 55 00    ld   de,$0055
02D8: DD 21 DF 02 ld   ix,$02DF
02DC: C3 95 03    jp   $0395

02DF: 21 FF 5F    ld   hl,$5FFF
02E2: 11 AA 55    ld   de,$55AA
02E5: DD 21 EC 02 ld   ix,$02EC
02E9: C3 95 03    jp   $0395

02EC: 21 00 40    ld   hl,$4000
02EF: 11 FF AA    ld   de,$AAFF
02F2: DD 21 F9 02 ld   ix,$02F9
02F6: C3 95 03    jp   $0395

02F9: 21 FF 5F    ld   hl,$5FFF
02FC: 11 00 FF    ld   de,$FF00
02FF: DD 21 06 03 ld   ix,$0306
0303: C3 95 03    jp   $0395

0306: 79          ld   a,c
0307: B0          or   b
0308: C2 C1 03    jp   nz,$03C1
030B: DD 21 12 03 ld   ix,$0312
030F: C3 79 00    jp   $0079

0312: 21 FF 87    ld   hl,$87FF
0315: 11 00 00    ld   de,$0000
0318: DD 21 1F 03 ld   ix,$031F
031C: C3 95 03    jp   $0395

031F: 01 00 00    ld   bc,$0000
0322: 21 00 80    ld   hl,$8000
0325: DD 21 2C 03 ld   ix,$032C
0329: C3 76 03    jp   $0376

032C: 21 00 84    ld   hl,$8400
032F: DD 21 36 03 ld   ix,$0336
0333: C3 76 03    jp   $0376

0336: 21 00 80    ld   hl,$8000
0339: 11 55 00    ld   de,$0055
033C: DD 21 43 03 ld   ix,$0343
0340: C3 8F 03    jp   $038F

0343: 21 FF 87    ld   hl,$87FF
0346: 11 AA 55    ld   de,$55AA
0349: DD 21 50 03 ld   ix,$0350
034D: C3 8F 03    jp   $038F

0350: 21 00 80    ld   hl,$8000
0353: 11 FF AA    ld   de,$AAFF
0356: DD 21 5D 03 ld   ix,$035D
035A: C3 8F 03    jp   $038F

035D: 21 FF 87    ld   hl,$87FF
0360: 11 00 FF    ld   de,$FF00
0363: DD 21 6A 03 ld   ix,$036A
0367: C3 8F 03    jp   $038F
036A: 79          ld   a,c
036B: B0          or   b
036C: C2 6C 03    jp   nz,$036C
036F: DD 21 42 04 ld   ix,$0442
0373: C3 79 00    jp   $0079

0376: 16 00       ld   d,$00
0378: 72          ld   (hl),d
0379: 7E          ld   a,(hl)
037A: AA          xor  d
037B: B0          or   b
037C: 47          ld   b,a
037D: 23          inc  hl
037E: 72          ld   (hl),d
037F: 7E          ld   a,(hl)
0380: AA          xor  d
0381: B1          or   c
0382: 4F          ld   c,a
0383: 2B          dec  hl
0384: 15          dec  d
0385: C2 78 03    jp   nz,$0378
0388: 36 00       ld   (hl),$00
038A: 23          inc  hl
038B: 36 00       ld   (hl),$00
038D: DD E9       jp   (ix)
038F: D9          exx
0390: 01 00 08    ld   bc,$0800
0393: 18 04       jr   $0399

0395: D9          exx
0396: 01 00 20    ld   bc,$2000
0399: D9          exx
039A: 7E          ld   a,(hl)
039B: AA          xor  d
039C: B0          or   b
039D: 47          ld   b,a
039E: 73          ld   (hl),e
039F: 7E          ld   a,(hl)
03A0: AB          xor  e
03A1: B0          or   b
03A2: 47          ld   b,a
03A3: CB 43       bit  0,e
03A5: C2 AA 03    jp   nz,$03AA
03A8: 2B          dec  hl
03A9: 3E 23       ld   a,$23
03AB: 78          ld   a,b
03AC: 41          ld   b,c
03AD: 4F          ld   c,a
03AE: D9          exx
03AF: 0D          dec  c
03B0: C2 B7 03    jp   nz,$03B7
03B3: 05          dec  b
03B4: CA BB 03    jp   z,$03BB
03B7: D9          exx
03B8: C3 9A 03    jp   $039A
03BB: D9          exx
03BC: 78          ld   a,b
03BD: 41          ld   b,c
03BE: 4F          ld   c,a
03BF: DD E9       jp   (ix)
03C1: 21 22 04    ld   hl,$0422
03C4: 11 01 00    ld   de,$0001
03C7: 78          ld   a,b
03C8: A2          and  d
03C9: C2 E8 03    jp   nz,$03E8
03CC: 79          ld   a,c
03CD: A3          and  e
03CE: C3 E8 03    jp   $03E8
03D1: EB          ex   de,hl
03D2: 29          add  hl,hl
03D3: EB          ex   de,hl
03D4: D2 C7 03    jp   nc,$03C7
03D7: 11 00 40    ld   de,$4000
03DA: 1D          dec  e
03DB: FD 7E 00    ld   a,(iy+$00)
03DE: C2 DA 03    jp   nz,$03DA
03E1: 15          dec  d
03E2: C2 DA 03    jp   nz,$03DA
03E5: C3 B8 02    jp   $02B8
03E8: 08          ex   af,af'
03E9: 7E          ld   a,(hl)
03EA: 23          inc  hl
03EB: D9          exx
03EC: 6F          ld   l,a
03ED: D9          exx
03EE: 7E          ld   a,(hl)
03EF: 23          inc  hl
03F0: D9          exx
03F1: 67          ld   h,a
03F2: 11 1F 00    ld   de,$001F
03F5: 06 03       ld   b,$03
03F7: 08          ex   af,af'
03F8: B7          or   a
03F9: 28 07       jr   z,$0402
03FB: 36 FC       ld   (hl),$FC
03FD: 23          inc  hl
03FE: 36 3F       ld   (hl),$3F
0400: 18 05       jr   $0407
0402: 36 84       ld   (hl),$84
0404: 23          inc  hl
0405: 36 21       ld   (hl),$21
0407: 19          add  hl,de
0408: 10 EE       djnz $03F8
040A: 06 24       ld   b,$24
040C: B7          or   a
040D: 28 07       jr   z,$0416
040F: 36 FF       ld   (hl),$FF
0411: 23          inc  hl
0412: 36 FF       ld   (hl),$FF
0414: 18 05       jr   $041B
0416: 36 80       ld   (hl),$80
0418: 23          inc  hl
0419: 36 01       ld   (hl),$01
041B: 19          add  hl,de
041C: 10 EE       djnz $040C
041E: D9          exx
041F: C3 D1 03    jp   $03D1
0422: 8D          adc  a,l
0423: 50          ld   d,b
0424: 4D          ld   c,l
0425: 4A          ld   c,d
0426: 0D          dec  c
0427: 44          ld   b,h
0428: CD 56 15    call $1556
042B: 44          ld   b,h
042C: 55          ld   d,l
042D: 4A          ld   c,d
042E: 95          sub  l
042F: 50          ld   d,b
0430: D5          push de
0431: 56          ld   d,(hl)
0432: 89          adc  a,c
0433: 50          ld   d,b
0434: 49          ld   c,c
0435: 4A          ld   c,d
0436: 09          add  hl,bc
0437: 44          ld   b,h
0438: C9          ret

0439: 56          ld   d,(hl)
043A: 11 44 51    ld   de,$5144
043D: 4A          ld   c,d
043E: 91          sub  c
043F: 50          ld   d,b
0440: D1          pop  de
0441: 56          ld   d,(hl)
0442: 21 00 60    ld   hl,$6000
0445: 16 01       ld   d,$01
0447: 42          ld   b,d
0448: AF          xor  a
0449: 4F          ld   c,a
044A: 5F          ld   e,a
044B: ED 47       ld   i,a
044D: ED 57       ld   a,i
044F: D3 4B       out  ($4B),a               ; write to magicram_control_w
0451: 36 FF       ld   (hl),$FF
0453: 72          ld   (hl),d
0454: 36 00       ld   (hl),$00
0456: 7E          ld   a,(hl)
0457: BB          cp   e
0458: 20 FE       jr   nz,$0458
045A: ED 57       ld   a,i
045C: 3C          inc  a
045D: ED 47       ld   i,a
045F: FE 10       cp   $10
0461: 20 0B       jr   nz,$046E
0463: CB 12       rl   d
0465: 30 E0       jr   nc,$0447
0467: DD 21 87 04 ld   ix,$0487
046B: C3 79 00    jp   $0079

046E: 79          ld   a,c
046F: CB 1F       rr   a
0471: CB 18       rr   b
0473: CB 19       rr   c
0475: 59          ld   e,c
0476: ED 57       ld   a,i
0478: FE 08       cp   $08
047A: 38 D1       jr   c,$044D
047C: 3E 08       ld   a,$08
047E: CB 08       rrc  b
0480: CB 13       rl   e
0482: 3D          dec  a
0483: 20 F9       jr   nz,$047E
0485: 18 C6       jr   $044D
0487: 1E 00       ld   e,$00
0489: DD 21 CA 04 ld   ix,$04CA
048D: 21 00 60    ld   hl,$6000
0490: 01 01 01    ld   bc,$0101
0493: 7B          ld   a,e
0494: D3 4B       out  ($4B),a               ; write to magicram_control_w
0496: 78          ld   a,b
0497: 32 00 40    ld   ($4000),a
049A: 71          ld   (hl),c
049B: 79          ld   a,c
049C: DD E9       jp   (ix)
049E: AE          xor  (hl)
049F: 20 FE       jr   nz,$049F
04A1: 77          ld   (hl),a
04A2: 78          ld   a,b
04A3: A1          and  c
04A4: 28 02       jr   z,$04A8
04A6: 3E 80       ld   a,$80
04A8: 57          ld   d,a
04A9: DB 4E       in   a,($4E)               ; read intercept_v256_r
04AB: AA          xor  d
04AC: 17          rla                        ; move intercept bit into carry
04AD: 38 FE       jr   c,$04AD
04AF: CB 00       rlc  b
04B1: 30 E0       jr   nc,$0493
04B3: CB 01       rlc  c
04B5: 30 DC       jr   nc,$0493
04B7: DD 23       inc  ix
04B9: DD 23       inc  ix
04BB: DD 23       inc  ix
04BD: 3E 10       ld   a,$10
04BF: 83          add  a,e
04C0: 5F          ld   e,a
04C1: 30 D0       jr   nc,$0493
04C3: DD 21 FA 04 ld   ix,$04FA
04C7: C3 79 00    jp   $0079

04CA: 00          nop
04CB: 18 D1       jr   $049E
04CD: B0          or   b
04CE: 18 CE       jr   $049E
04D0: 2F          cpl
04D1: 18 18       jr   $04EB
04D3: AF          xor  a
04D4: 18 21       jr   $04F7
04D6: A0          and  b
04D7: 18 C5       jr   $049E
04D9: 78          ld   a,b
04DA: 18 C2       jr   $049E
04DC: A8          xor  b
04DD: 18 18       jr   $04F7
04DF: 2F          cpl
04E0: 18 EB       jr   $04CD
04E2: 2F          cpl
04E3: 18 0F       jr   $04F4
04E5: A8          xor  b
04E6: 18 B6       jr   $049E
04E8: 78          ld   a,b
04E9: 18 0C       jr   $04F7
04EB: A0          and  b
04EC: 18 09       jr   $04F7
04EE: AF          xor  a
04EF: 18 AD       jr   $049E
04F1: 2F          cpl
04F2: 18 E2       jr   $04D6
04F4: B0          or   b
04F5: 18 00       jr   $04F7
04F7: 2F          cpl
04F8: 18 A4       jr   $049E
04FA: ED 5E       im   2
04FC: 3E 07       ld   a,$07
04FE: ED 47       ld   i,a
0500: DD 21 02 16 ld   ix,$1602
0504: 3E FF       ld   a,$FF
0506: D3 4F       out  ($4F),a
0508: 47          ld   b,a
0509: 31 FF 43    ld   sp,$43FF
050C: DB 4E       in   a,($4E)               ; read intercept_v256_r
050E: 1F          rra
050F: CB 10       rl   b
0511: 78          ld   a,b
0512: EE 55       xor  $55
0514: 28 03       jr   z,$0519
0516: FB          ei
0517: 18 FE       jr   $0517
0519: D3 4F       out  ($4F),a               ; enable interrupts?
051B: 06 FF       ld   b,$FF


;
; Called from NMI handler
;

051D: 31 FF 43    ld   sp,$43FF
0520: DB 4D       in   a,($4D)               ; disable NMI (according to MAME driver source)        
0522: DB 4E       in   a,($4E)               ; read intercept_v256_r
0524: 1F          rra
0525: CB 10       rl   b
0527: 78          ld   a,b
0528: EE 20       xor  $20
052A: CA 79 00    jp   z,$0079
052D: DB 4C       in   a,($4C)               ; enable NMI (according to MAME driver source)
052F: 18 FE       jr   $052F                 ; enter infinite loop 

;
; INPUT TEST MODE diagnostics - test controls & switches
; Can be accessed in MAME by setting INPUT TEST MODE dip switch in MAME to ON, then restarting game.
; 

; Called from $0012
INPUT_TEST_MODE:
0531: 31 00 44    ld   sp,$4400
0534: CD 4E 1A    call $1A4E                 ; call CLEAR_SCREEN
0537: CD F8 35    call $35F8                 ; call COLOUR_FILL_WHITE 

; print titles
053A: 21 B9 05    ld   hl,$05B9              ; address of "ZPU DIP SWITCHES" text
053D: 11 20 00    ld   de,$0020              ; D = Y coordinate, E = X coordinate
0540: CD 91 05    call $0591                 ; print "ZPU DIP SWITCHES"
0543: 11 20 80    ld   de,$8020              ; D = Y coordinate, E = X coordinate
0546: CD 91 05    call $0591                 ; print "VFB SWITCHES"
0549: 11 08 10    ld   de,$1008              ; D = Y coordinate, E = X coordinate
054C: CD 91 05    call $0591                 ; print "1 2 3 4 5 6 7 8"
054F: 11 10 D0    ld   de,$D010              ; D = Y coordinate, E = X coordinate
0552: CD 91 05    call $0591                 ; print "0=OFF <hatch>=ON"

; create 2 horizontal rules (like <HR> tag in HTML)
0555: 21 A0 47    ld   hl,$47A0
0558: CD 45 1A    call $1A45                 ; call WRITE_FF_64_TIMES_HL                  
055B: 21 A0 55    ld   hl,$55A0
055E: CD 45 1A    call $1A45                 ; call WRITE_FF_64_TIMES_HL

; read ZPU DIP switches
0561: 11 08 20    ld   de,$2008              ; D = Y coordinate, E = X coordinate
0564: DB 61       in   a,($61)               ; read from F2 DIP switch
0566: CD 97 05    call $0597                 ; call PRINT_SET_BITS
0569: DB 60       in   a,($60)               ; read from F3 DIP switch 
056B: CD 97 05    call $0597                 ; call PRINT_SET_BITS
056E: DB 62       in   a,($62)               ; read from F6 DIP switch
0570: CD 97 05    call $0597                 ; call PRINT_SET_BITS
0573: DB 63       in   a,($63)               ; read from F5 DIP switch
0575: CD 97 05    call $0597                 ; call PRINT_SET_BITS
0578: DB 64       in   a,($64)               ; read from F4 DIP switch
057A: CD 97 05    call $0597                 ; call PRINT_SET_BITS

; now print VFB switches
057D: 11 08 90    ld   de,$9008              ; D = Y coordinate, E = X coordinate
0580: DB 48       in   a,($48)               ; read P1 controls  
0582: CD 96 05    call $0596                 ; call FLIP_THEN_PRINT_SET_BITS
0585: DB 49       in   a,($49)               ; read SYSTEM         
0587: CD 96 05    call $0596                 ; call FLIP_THEN_PRINT_SET_BITS
058A: DB 4A       in   a,($4A)               ; read player 2 controls               
058C: CD 96 05    call $0596                 ; call FLIP_THEN_PRINT_SET_BITS
058F: 18 D0       jr   $0561                 ; go back and read DIP switches until machine switched off!

; TODO: Tentative function name
; D = Y coordinate
; E = X coordinate
JP_PRINT_STRING_06B8:
0591: 06 00       ld   b,$00                 ; Magic RAM control bits 
0593: C3 B8 06    jp   $06B8                 ; jump to PRINT_STRING_06B8



FLIP_THEN_PRINT_SET_BITS:
0596: 2F          cpl                        ; flip bits

;
; Used by INPUT TEST MODE screen.  
;
; Displays a graphic representation of bits set/Unset in a byte
; Set bit will be rendered as a crosshatch pattern
; Unset bit will be rendered as "0" (no quotes)
;
; A = bits
; D = Y coordinate
; E = X coordinate

PRINT_SET_BITS:
0597: 0E 08       ld   c,$08                 ; testing 8 bits (the full byte)
0599: 21 01 06    ld   hl,$0601              ; address of hatch pattern string

; test bit
059C: 1F          rra                        ; move bit into carry flag
059D: 38 03       jr   c,$05A2               ; if bit is set, goto 05A2 to print hatch pattern

; bit is unset, print 0
059F: 21 03 06    ld   hl,$0603              ; address of "0" string

; print hatch character or "0"
05A2: F5          push af
05A3: D5          push de
05A4: C5          push bc
05A5: CD 91 05    call $0591                 ; call JP_PRINT_STRING_06B8
05A8: C1          pop  bc
05A9: D1          pop  de
05AA: F1          pop  af
05AB: 21 20 00    ld   hl,$0020              ; horizontal pixel spacing between characters
05AE: 19          add  hl,de                 ; increment X coordinate by #$20 (32 decimal) pixels   
05AF: EB          ex   de,hl                 ; now make DE = X/Y coordinates 
05B0: 0D          dec  c                     ; decrement count of bits left to test
05B1: 20 E6       jr   nz,$0599              ; repeat until all bits done

; All bits done
05B3: 21 00 0F    ld   hl,$0F00              ; increment Y coordinate by #$0F (15 decimal) pixels             
05B6: 19          add  hl,de
05B7: EB          ex   de,hl
05B8: C9          ret

; Text used in the ZPU DIP SWITCHES test screen
05B9:  5A 50 55 20 44 49 50 20 53 57 49 54 43 48 45 53  ZPU DIP SWITCHES
05C9:  00 

05CA:  56 46 42 20 53 57 49 54 43 48 45 53 00           VFB SWITCHES.  

05D7:  31 20 20 20 32 20 20 20 33 20 20 20 34 20 20 20  1   2   3   4   
05E7:  35 20 20 20 36 20 20 20 37 20 20 20 38 00        5   6   7   8.

05F5:  30 3D 4F 46 46 20 20 7F 3D 4F 4E 00              0=OFF  .=ON.

0601:  7F 00                                            (Hatch pattern)

0603:  30 00                                            0.


;
;
; Display bookkeeping totals 
;
;

BOOKKEEPING:
0605: AF          xor  a
0606: D3 4F       out  ($4F),a               ; disable interrupts
0608: DB 4E       in   a,($4E)               ; clear pending frame interrupts
060A: 31 00 43    ld   sp,$4300
060D: DB 65       in   a,($65)               ; read SW2
060F: CB 7F       bit  7,a                   ; test BOOKKEEPING bit
0611: 20 FA       jr   nz,$060D              ; wait until user takes finger off BOOKKEEPING button

0613: CD 4E 1A    call $1A4E                 ; call CLEAR_SCREEN
0616: CD 3D 36    call $363D                 ; set colour attributes such that any pixels on all but last 2 lines of screen will be blue
0619: F3          di


061A: 21 CC 06    ld   hl,$06CC              ; pointer to the first text string, "Credits" 

; main loop
061D: 7E          ld   a,(hl)                ; read byte
061E: B7          or   a                     ; is it 0? - which is the list terminator 
061F: CA 02 16    jp   z,$1602               ; yes, goto $1602

0622: CD 9A 06    call $069A                 ; call BK_SCROLL_UP
0625: 06 00       ld   b,$00
0627: 11 00 CF    ld   de,$CF00              ; D = Y coordinate, E = X coordinate

; print title of bookkeeping item then scroll screen up to make room for associated value
062A: CD B8 06    call $06B8                 ; print title of bookkeeping item
062D: CD 9A 06    call $069A                 ; call BK_SCROLL_UP            
0630: E5          push hl

; After printing text string, HL is now a pointer to 3 parameter bytes. 
; Bytes 0 & 1: pointer to CMOS RAM entry
; Byte 2: max length, in digits, of entry
0631: 5E          ld   e,(hl)                ; read LSB of CMOS RAM address
0632: 23          inc  hl
0633: 56          ld   d,(hl)                ; read MSB of CMOS RAM address
0634: 23          inc  hl
0635: 46          ld   b,(hl)                ; read size in bytes
0636: 23          inc  hl
0637: E5          push hl

; DE = pointer to CMOS RAM entry to print
; B = size of CMOS RAM entry in bytes (or, put another way, number of digits to print)
0638: 21 00 00    ld   hl,$0000
063B: E5          push hl                    ; allocate 6 bytes of temp storage..
063C: E5          push hl
063D: E5          push hl                    ; ..on the stack
063E: 39          add  hl,sp                 ; HL = SP
063F: CD 73 06    call $0673                 ; call UNPACK_BYTES_TO_HL

; print digits
0642: 11 CF 00    ld   de,$00CF              ; Set Y coordinate to print digits  
0645: CD 8D 06    call $068D                 ; call PRINT_DIGITS_068D
0648: E1          pop  hl                    ; discard temp data on stack
0649: E1          pop  hl
064A: E1          pop  hl
064B: E1          pop  hl
064C: E3          ex   (sp),hl

064D: DB 65       in   a,($65)               ; read SW2
064F: CB 7F       bit  7,a                   ; test BOOKKEEPING bit
0651: 20 16       jr   nz,$0669              ; if bit is set, goto $0669

; Test if fire has been pressed - if so, then clear (zero) the bookkeeping item.
0653: DB 48       in   a,($48)               ; read P1 controls
0655: CB 67       bit  4,a                   ; test FIRE bit
0657: 20 F4       jr   nz,$064D              ; if FIRE is NOT pressed (remember, its active LOW) goto $064D

; P1 fire has been pressed  - clear the bookkeeping item
0659: E5          push hl
065A: 5E          ld   e,(hl)                ; read LSB of CMOS RAM address
065B: 23          inc  hl
065C: 56          ld   d,(hl)                ; read MSB of CMOS RAM address
065D: 23          inc  hl
065E: 46          ld   b,(hl)                ; read size in bytes
065F: AF          xor  a
; write B zero bytes to CMOS RAM @ DE 
0660: 12          ld   (de),a                ; write 0 byte
0661: 13          inc  de                    ; bump DE to next byte to be zeroes
0662: 10 FC       djnz $0660                 ; repeat until all bytes written
0664: E1          pop  hl
0665: D1          pop  de
0666: C3 30 06    jp   $0630                 ; update screen to show value zeroed  

; wait for user to take finger off BOOKKEEPING button 
0669: E1          pop  hl
066A: DB 65       in   a,($65)               ; read SW2
066C: CB 7F       bit  7,a                   ; test BOOKKEEPING bit
066E: 20 FA       jr   nz,$066A              ; repeat until BOOKKEEPING button not pressed
0670: C3 1D 06    jp   $061D                 ; print next bookkeeping item


;
; This routine is used to read values from CMOS.
;
; Expects:
; B = sizeof() in bytes
; DE = pointer to byte pairs where only upper nibble of each byte is relevant
;   e.g. a value of 96 decimal would be stored as 90 60 in CMOS
; HL = pointer to RAM where result can be stored 
;
; Returns: A = value read from CMOS
;

UNPACK_BYTES_TO_HL:
0673: E5          push hl
0674: D5          push de
0675: C5          push bc

; read first byte in pair
0676: 1A          ld   a,(de)         
0677: 13          inc  de
0678: E6 F0       and  $F0                   ; mask in upper nibble
067A: 4F          ld   c,a                   ; store upper nibble in C

; read second byte in pair
067B: 1A          ld   a,(de)
067C: 13          inc  de

067D: 07          rlca                       ; move bits 4..7
067E: 07          rlca
067F: 07          rlca
0680: 07          rlca                       ; into bits 0..3
0681: E6 0F       and  $0F                   ; preserve lower nibble

; C= upper nibble (source: byte 0). A = lower nibble (source: byte 1). 
; Combine nibbles to form 2 digit BCD number then write result to (HL)
0683: B1          or   c                     ; combine A with C
0684: 77          ld   (hl),a                ; write result

0685: 23          inc  hl                    ; bump HL to point to first byte in next byte pair
0686: 05          dec  b
0687: 10 ED       djnz $0676                 ; repeat until all byte pairs done
0689: C1          pop  bc
068A: D1          pop  de
068B: E1          pop  hl
068C: C9          ret


;
; Used by Bookkeeping screen to display totals  
;
; Expects: E = Y coordinate
;

PRINT_DIGITS_068D:
068D: C5          push bc
068E: D5          push de
068F: E5          push hl
0690: 53          ld   d,e                   ; set Y coordinate
0691: 1E 00       ld   e,$00                 ; set X coordinate to 0
0693: CD 40 2A    call $2A40                 ; call PRINT_DIGITS
0696: E1          pop  hl
0697: D1          pop  de
0698: C1          pop  bc
0699: C9          ret

;
; Scroll screen up for bookkeeping
;
;

BK_SCROLL_UP:
069A: C5          push bc
069B: D5          push de
069C: E5          push hl

; scroll up
069D: 01 C0 19    ld   bc,$19C0
06A0: 11 00 44    ld   de,$4400
06A3: 21 00 46    ld   hl,$4600
06A6: ED B0       ldir  

; clear remainder of screen  
06A8: 01 00 02    ld   bc,$0200
06AB: AF          xor  a
06AC: 12          ld   (de),a                ; 
06AD: 13          inc  de
06AE: 0D          dec  c
06AF: C2 AC 06    jp   nz,$06AC
06B2: 10 F8       djnz $06AC
06B4: E1          pop  hl
06B5: D1          pop  de
06B6: C1          pop  bc
06B7: C9          ret

;
; HL = pointer to null(zero byte)-terminated text string
; B = Magic image RAM control bits
; D = Y coordinate
; E = X coordinate
;

PRINT_STRING_06B8:
06B8: EB          ex   de,hl
06B9: CD A3 29    call $29A3                 ; call CALCULATE_MAGIC_IMAGE_RAM_ADDRESS
06BC: EB          ex   de,hl
06BD: 4E          ld   c,(hl)                ; read char from string
06BE: CD DB 29    call $29DB                 ; call PRINT_CHAR
06C1: 13          inc  de
06C2: 23          inc  hl
06C3: 47          ld   b,a
06C4: 7E          ld   a,(hl)
06C5: B7          or   a
06C6: 78          ld   a,b
06C7: C2 BD 06    jp   nz,$06BD
06CA: 23          inc  hl
06CB: C9          ret



;
; BOOKKEEPING CMOS RAM descriptor table
;
; Each entry in the table is a null (zero) terminated text string followed by 3 bytes:
; Bytes 0 & 1: pointer to CMOS RAM entry
; Byte 2: max length, in digits, of entry
;
; For example, after "Credits" is printed below, the system will print 2 digits read from CMOS RAM location $08A4
;
; See $061A for the code that reads and prints these entries

06CC:  43 72 65 64 69 74 73 00                          Credits.
06D4:
    A4 08                                    ; pointer to CMOS RAM where credits is held 
    02                                       ; max number of digits  

/* 08A4: 
06D7:  43 68 75 74 65 20 31 00                          Chute 1.
A6 08                                        ; pointer to CMOS RAM where chute 1 is held  
08                                           ; max number of digits  

06E2:  43 68 75 74 65 20 32 00                          Chute 2.
AE 08                                        ; pointer to CMOS RAM where chute 2 is held 
08                                           ; max number of digits

06ED:  43 68 75 74 65 20 33 00                          Chute 3.
B6 08                                        ; pointer to CMOS RAM where chute 3 is held   
08                                           ; max number of digits

06F8:  50 6C 61 79 73 00                                Plays.
BE 08                                        ; pointer to CMOS RAM where plays is held  
06                                           ; max number of digits

0701:  54 6F 74 61 6C 20 53 63 6F 72 65 00              Total Score.
C4 08                                        ; pointer to CMOS RAM where total score is held  
0C                                           ; max number of digits

0710:  54 6F 74 61 6C 20 53 65 63 6F 6E 64 73           Total Seconds
071D:  20 6F 66 20 50 6C 61 79 00                        of Play.
D0 08                                        ; pointer to CMOS RAM where total seconds of play is held 
0C                                           ; max number of digits

0729:  48 69 67 68 20 53 63 6F 72 65 73 00              High Scores.
DC 08                                        ; pointer to CMOS RAM where high scores is held    
06                                           ; max number of digits

; This terminates the list of entries (see $061E)
00


0739: F3          di


CROSSHAIR_PATTERN_TEST_MODE:
073A: 21 00 44    ld   hl,$4400              ; load HL with address of screen image RAM              ; HL = start of screen image RAM
073D: 54          ld   d,h
073E: 5D          ld   e,l                   ; DE = HL

; create vertical bar
073F: 36 01       ld   (hl),$01              ; write 1 pixel           
0741: 23          inc  hl
0742: 73          ld   (hl),e                 
0743: 23          inc  hl
0744: 01 FD 1B    ld   bc,$1BFD
0747: EB          ex   de,hl
0748: ED B0       ldir

; create horizontal bars
074A: 21 00 45    ld   hl,$4500
074D: 54          ld   d,h
074E: 5D          ld   e,l
074F: 06 20       ld   b,$20
0751: 36 FF       ld   (hl),$FF              ; write 8 pixels
0753: 23          inc  hl
0754: 10 FB       djnz $0751

; We now have a template of sorts to use to create a grid. Copy this template to fill screen. 
0756: 01 80 02    ld   bc,$0280
0759: 09          add  hl,bc
075A: EB          ex   de,hl
075B: 01 7F 18    ld   bc,$187F
075E: ED B0       ldir                       ; create grid pattern

; grid is drawn, now colour it
0760: CD F8 35    call $35F8                 ; call COLOUR_FILL_WHITE

; wait for FIRE to be pressed
0763: CD 86 07    call $0786                 ; call WAIT_FOR_1P_FIRE_BUTTON                 

; Fill entire screen with pixels
0766: 21 00 44    ld   hl,$4400              ; load HL with address of screen image RAM 
0769: 11 01 44    ld   de,$4401
076C: 01 FF 1B    ld   bc,$1BFF
076F: 36 FF       ld   (hl),$FF
0771: ED B0       ldir                       ; fill screen with pixels

; Set pixel colour attributes to RED - now entire screen will be red colour
0773: 21 00 81    ld   hl,$8100
0776: 11 01 81    ld   de,$8101
0779: 01 FF 06    ld   bc,$06FF
077C: 36 11       ld   (hl),$11
077E: ED B0       ldir                       ; fill attribute RAM with $11 (Red)

; wait for fire to be pressed
0780: CD 86 07    call $0786                 ; call WAIT_FOR_1P_FIRE_BUTTON  

; it appears once you enter the crosshair test you can't exit it without switching machine off,
; changing DIP switches to disable test, then restarting machine 
0783: C3 3A 07    jp   $073A                 ; go and draw the crosshair again


WAIT_FOR_1P_FIRE_BUTTON:
; wait for fire button to be pressed
0786: DB 48       in   a,($48)               ; read P1 controls               
0788: CB 67       bit  4,a                   ; test if FIRE pressed
078A: 20 FA       jr   nz,$0786              ; jump if FIRE not pressed (remember, button is ACTIVE_LOW so is deemed pressed when bit NOT set)

; Wait for user to take finger off fire button
078C: DB 48       in   a,($48)               ; read player 1 controls
078E: CB 67       bit  4,a                   ; test if FIRE pressed
0790: 28 FA       jr   z,$078C               l jump if FIRE pressed to $078C
0792: C9          ret

0793: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
0796: 
    90
    08 20                                    ; X/Y coordinates    

0799:  59 6F 75 20 68 61 76 65 20 6A 6F 69 6E 65 64 20  You have joined 
07A9:  74 68 65 20 69 6D 6D 6F 72 74 61 6C 73 00        the immortals..

07B7: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
07BA: 
    90          
    0C 30                                    ; X/Y coordinates  
07BD:  69 6E 20 74 68 65 20 42 45 52 5A 45 52 4B 20 68  in the BERZERK h
07CD:  61 6C 6C 20 6F 66 20 66 61 6D 65 00              all of fame.

07D9: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
07DC: 
    90          
    18 50                                    ; X/Y coordinates 

07DF:  45 6E 74 65 72 20 79 6F 75 72 20 69 6E 69 74 69  Enter your initi
07EF:  61 6C 73 3A 00                                   als:.

07F4: C9          ret

07F5: FF          rst  $38
07F6: FF          rst  $38
07F7: FF          rst  $38
07F8: FF          rst  $38
07F9: FF          rst  $38
07FA: FF          rst  $38
07FB: FF          rst  $38
07FC: 09          add  hl,bc
07FD: 05          dec  b
07FE: 17          rla
07FF: 05          dec  b


;
; 0800 - 0FFF is RAM, not ROM and contains no code.
;

;
; Pattern tables define the animations for each game entity.
; They are essentially a list of pointers to pattern (sprite) data. 
; 


; pattern table for robot standing still
1000: 
    10 D1                                    ; pointer to pixel data for animation frame #1 (see $10D1)                
    10 DE                                    ; and subsequent...    
    10 EB       
    10 F8       
    10 F8       
    10 F8       
    11 05 
    11 12                                    ; .. animation frames       
    00                                       ; null terminator marker byte - indicates animation sequence has ended 
    00 10                                    ; pointer back to $1000, making this animation start over from frame #1 again

; pattern table for robot moving right/up right/down right
1013: 
    11 2C          
    11 1F 
    11 1F          
    00          
    13 10 

; pattern table for robot moving down 
101C: 
    10 D1          
    11 39 
    10 D1          
    11 47 
    00    
    1C 10 

; pattern table for robot moving left/up left/down left    
1027: 
    11 55          
    11 62 
    11 62          
    00          
    27 10 

; pattern table for robot moving up   
1030: 
    11 6F
    11 7C 
    11 6F
    11 8A 
    00    
    30 10       

; pattern table for robot exploding.
103B: 
    11 98 
    11 BC          
    11 E0 
    12 08
    00          
    41 10 


; pattern table for player standing still. (1 frame)
; See also PLAYER_ANIMATION_TABLE entry 0 at $2053.
1046: 
    10 BF                                    ; sprite pattern at $10BF
    00          
    46 10                                    ; loop back to $1046

; pattern table for player moving up/up-right/right/down-right/down
; See also PLAYER_ANIMATION_TABLE entries 1-4,8 at $2053.
104B: 
    10 AD                                    ; sprite pattern at $10AD
    10 9B                                    ; sprite pattern at $109B
    10 89                                    ; sprite pattern at $1089
    10 9B                                    ; sprite pattern at $109B (cycle)
    00          
    4B 10                                    ; loop back to $104B

; pattern table for player moving down-left/left/up-left
; See also PLAYER_ANIMATION_TABLE entries 5-7 at $2053.
1056: 
    13 B5                                    ; sprite pattern at $13B5
    13 A3                                    ; sprite pattern at $13A3
    13 91                                    ; sprite pattern at $1391
    13 A3                                    ; sprite pattern at $13A3 (cycle)
    00          
    56 10                                    ; loop back to $1056

; Player shooting up, right pattern table (1 frame)
1061: 
    13 09          
    00          
    61 10    

; Player shooting right pattern table (1 frame)
1066: 
    13 1A          
    00          
    66 10 

; Player shooting down, right pattern table (1 frame)
106B: 
    13 2B          
    00
    6B 10 

; Player shooting down pattern table (1 frame)
1070: 
    13 3C          
    00          
    70 10 

; Player shooting down, left pattern table (1 frame)
1075: 
    13 4D          
    00          
    75 10 

; Player shooting left pattern table (1 frame)
107A: 
    13 5E          
    00          
    7A 10 

; Player shooting up, left pattern table (1 frame)
107F: 
    13 6F          
    00          
    7F 10 

; Player shooting up pattern table (1 frame)
1084: 
    13 80          
    00          
    84 10 



;
; Player sprite patterns (pixel data)
;
; Format is as follows:
; Byte 0: Width in bytes. 
;         Sprites are 1 bit per pixel, so a width of 1 byte yields a sprite 8 pixels wide,
;         a width of 2 bytes is 16 pixels wide. 
;         No game sprites are wider than 2 bytes. 
;
; Byte 1: Height in pixels.
; Byte 2 onwards: pixel data 

1089: 
    01                                       ; width in bytes       
    10                                       ; height in bytes
    18 18 00 3C 5A 99 58 18 18 24 22 41 41 81 81 00          

109B: 
    01                                       ; width in bytes
    10                                       ; height in bytes
    00 18 18 00 3C 5C 5C 3E 18 18 14 12 F2 82 02 03          

10AD: 
    01                                       ; width in bytes 
    10                                       ; height in bytes
    18 18 00 3C 5C 5C 5A 18 18 18 18 18 18 18 1C 10

10BF: 
    01                                       ; width in bytes 
    10                                       ; height in bytes
    18 18 00 3C 5A 5A 5A 18 18 18 18 18 18 18 1C 10 

10D1: 
    01                                       ; width in bytes
    0B                                       ; height in bytes
    3C 66 FF BD BD BD 3C 24 24 24 66          

10DE: 
    01                                       ; width in bytes
    0B                                       ; height in bytes
    3C 4E FF BD BD BD 3C 24 24 24 66          

10EB: 
    01                                       ; width in bytes
    0B                                       ; height in bytes
    3C 1E FF BD BD BD 3C 24 24 24 66          

10F8: 
    01                                       ; width in bytes
    0B                                       ; height in bytes
    3C 7E FF BD BD BD 3C 24 24 24 66

1105: 
    01                                       ; width in bytes
    0B                                       ; height in bytes
    3C 78 FF BD BD BD 3C 24 24 24 66

1112: 
    01                                       ; width in bytes
    0B                                       ; height in bytes
    3C 71 FF BD BD BD 3C 24 24 24 66

111F: 
    01                                       ; width in bytes
    0B                                       ; height in bytes
    3C 78 FF BD BD BD 3C 18 18 18 1C       

112C: 
    01                                       ; width in bytes
    0B                                       ; height in bytes
    3C 78 FF BD BD BD 3C 24 24 24 36 

1139: 
    01                                       ; width in bytes  
    0C                                       ; height in bytes        
    3C 66 FF BD BD BD 3C 24 24 26 20 60          

1147: 
    01                                       ; width in bytes  
    0C                                       ; height in bytes   
    3C 66 FF BD BD BD 3C 24 24 64 04 06

1155: 
    01                                       ; width in bytes  
    0B                                       ; height in bytes 
    3C 1E FF BD BD BD 3C 24 24 24 6C    

1162: 
    01                                       ; width in bytes  
    0B                                       ; height in bytes
    3C 1E FF BD BD BD 3C 18 18 18 38       

116F: 
    01                                       ; width in bytes  
    0B                                       ; height in bytes
    3C 7E FF BD BD BD 3C 24 24 24 66

117C: 
    01                                       ; width in bytes  
    0C                                       ; height in bytes 
    3C 7E FF BD BD BD 3C 24 24 26 20 60

118A: 
    01                                       ; width in bytes  
    0C                                       ; height in bytes 
    3C 7E FF BD BD BD 3C 24 24 64 04 06 
    
; Robot explosion sprite patterns 

; Robot explosion frame #1
1198: 
    02                                       ; width in bytes
    11                                       ; height in bytes 
    00 00 00 00 00 00 00 00 00 00 00 00 03 C0 07 E0 0F


11AB: F0          ret  p
11AC: 13          inc  de
11AD: C8          ret  z
11AE: 22 C4 03    ld   ($03C4),hl
11B1: 40          ld   b,b
11B2: 03          inc  bc
11B3: C0          ret  nz
11B4: 02          ld   (bc),a
11B5: 40          ld   b,b
11B6: 02          ld   (bc),a
11B7: 40          ld   b,b
11B8: 02          ld   (bc),a
11B9: 40          ld   b,b
11BA: 06 60       ld   b,$60

; Robot explosion Frame #2
11BC: 
    02                                       ; width in bytes          
    11                                       ; height in bytes 
    00 00 00 00 00 00 00 00 03 C0 06 60 12 48 08 10 10

11CF: 08          ex   af,af'
11D0: 22 44 44    ld   ($4444),hl
11D3: 42          ld   b,d
11D4: 02          ld   (bc),a
11D5: 10 03       djnz $11DA
11D7: C0          ret  nz
11D8: 02          ld   (bc),a
11D9: 40          ld   b,b
11DA: 04          inc  b
11DB: 20 02       jr   nz,$11DF
11DD: 40          ld   b,b
11DE: 06 60       ld   b,$60

;
; Robot explosion frame #3
;
11E0: 02          ld   (bc),a
11E1: 13          inc  de
11E2: 00          nop
11E3: 00          nop
11E4: 02          ld   (bc),a
11E5: 00          nop
11E6: 00          nop
11E7: 40          ld   b,b
11E8: 00          nop
11E9: 08          ex   af,af'
11EA: 10 00       djnz $11EC
11EC: 03          inc  bc
11ED: 00          nop
11EE: 00          nop
11EF: 12          ld   (de),a
11F0: 08          ex   af,af'
11F1: 01 80 00    ld   bc,$0080
11F4: 08          ex   af,af'
11F5: 02          ld   (bc),a
11F6: 40          ld   b,b
11F7: 02          ld   (bc),a
11F8: 04          inc  b
11F9: 10 23       djnz $121E
11FB: C0          ret  nz
11FC: 40          ld   b,b
11FD: 04          inc  b
11FE: 20 02       jr   nz,$1202
1200: 00          nop
1201: 00          nop
1202: 80          add  a,b
1203: 00          nop
1204: 10 08       djnz $120E
1206: 18 0C       jr   $1214
1208: 01 01 00    ld   bc,$0001


; Evil otto pattern table

120B: 12          ld   (de),a
120C: 2E 12       ld   l,$12
120E: 34          inc  (hl)
120F: 12          ld   (de),a
1210: 3B          dec  sp
1211: 12          ld   (de),a
1212: 43          ld   b,e
1213: 12          ld   (de),a
1214: 4C          ld   c,h
1215: 12          ld   (de),a
1216: 56          ld   d,(hl)
1217: 12          ld   (de),a
1218: A7          and  a
1219: 12          ld   (de),a
121A: 6D          ld   l,l
121B: 12          ld   (de),a
121C: 79          ld   a,c
121D: 12          ld   (de),a
121E: 85          add  a,l
121F: 12          ld   (de),a
1220: 91          sub  c
1221: 12          ld   (de),a
1222: 9D          sbc  a,l
1223: 12          ld   (de),a
1224: 91          sub  c
1225: 12          ld   (de),a
1226: 85          add  a,l
1227: 12          ld   (de),a
1228: 79          ld   a,c
1229: 12          ld   (de),a
122A: 6D          ld   l,l
122B: 00          nop
122C: 17          rla
122D: 12          ld   (de),a
122E: 84          add  a,h
122F: 00          nop
1230: 01 02 18    ld   bc,$1802
1233: 18 84       jr   $11B9
1235: 00          nop
1236: 01 03 10    ld   bc,$1003
1239: 38 10       jr   c,$124B
123B: 84          add  a,h
123C: 00          nop
123D: 01 04 18    ld   bc,$1804
1240: 3C          inc  a
1241: 3C          inc  a
1242: 18 84       jr   $11C8
1244: 00          nop
1245: 01 05 38    ld   bc,$3805
1248: 7C          ld   a,h
1249: 7C          ld   a,h
124A: 7C          ld   a,h
124B: 38 84       jr   c,$11D1
124D: 00          nop
124E: 01 06 3C    ld   bc,$3C06
1251: 7E          ld   a,(hl)
1252: 7E          ld   a,(hl)
1253: 7E          ld   a,(hl)
1254: 7E          ld   a,(hl)
1255: 3C          inc  a
1256: 84          add  a,h
1257: 00          nop
1258: 01 07 38    ld   bc,$3807
125B: 7C          ld   a,h
125C: FE FE       cp   $FE
125E: FE 7C       cp   $7C
1260: 38 84       jr   c,$11E6
1262: 00          nop
1263: 01 08 3C    ld   bc,$3C08
1266: 7E          ld   a,(hl)
1267: FF          rst  $38
1268: FF          rst  $38
1269: FF          rst  $38
126A: FF          rst  $38
126B: 7E          ld   a,(hl)
126C: 3C          inc  a
126D: 82          add  a,d
126E: 00          nop
126F: 01 08 3C    ld   bc,$3C08
1272: 7E          ld   a,(hl)
1273: DB FF       in   a,($FF)
1275: FF          rst  $38
1276: BD          cp   l
1277: 42          ld   b,d
1278: 3C          inc  a
1279: 81          add  a,c
127A: 00          nop
127B: 01 08 3C    ld   bc,$3C08
127E: 7E          ld   a,(hl)
127F: DB FF       in   a,($FF)
1281: FF          rst  $38
1282: BD          cp   l
1283: 42          ld   b,d
1284: 3C          inc  a
1285: 80          add  a,b
1286: 80          add  a,b
1287: 01 08 3C    ld   bc,$3C08
128A: 7E          ld   a,(hl)
128B: DB FF       in   a,($FF)
128D: FF          rst  $38
128E: BD          cp   l
128F: 42          ld   b,d
1290: 3C          inc  a
1291: 80          add  a,b
1292: 40          ld   b,b
1293: 01 08 3C    ld   bc,$3C08
1296: 7E          ld   a,(hl)
1297: DB FF       in   a,($FF)
1299: FF          rst  $38
129A: BD          cp   l
129B: 42          ld   b,d
129C: 3C          inc  a
129D: 01 08 3C    ld   bc,$3C08
12A0: 7E          ld   a,(hl)
12A1: DB FF       in   a,($FF)
12A3: FF          rst  $38
12A4: BD          cp   l
12A5: 42          ld   b,d
12A6: 3C          inc  a
12A7: 84          add  a,h
12A8: 00          nop
12A9: 01 08 00    ld   bc,$0008
12AC: 00          nop
12AD: 00          nop
12AE: 3C          inc  a
12AF: 7E          ld   a,(hl)
12B0: DB FF       in   a,($FF)
12B2: 7E          ld   a,(hl)
;
; Player death/electrocution animation pattern table
; 4 frames: $12D0 → $12BE → $12E3 → $12F6, then loops.
; See also PLAYER_ANIMATION_TABLE entry 9 at $2053.
;
12B3: 
    D0 12                                    ; sprite pattern at $12D0 (normal standing)
    BE 12                                    ; sprite pattern at $12BE (smaller/shrinking)
    E3 12                                    ; sprite pattern at $12E3 (disintegrating)
    F6 12                                    ; sprite pattern at $12F6 (nearly gone)
    00
    B3 12                                    ; loop back to $12B3


12BE: 
    01                                       ; width in bytes
    10                                       ; height in bytes
    00 18 18 00 3C 5A 5A 5A 18 18 18 18 18 18 18 3C       


;
; Player sprite patterns
;

12D0: 
    01                                       ; width in bytes 
    11                                       ; height in bytes  
    18 24 24 42 81 81 81 81 81 42 24 24 24 24 24 42 3C          

12E3: 
    01                                       ; width in bytes  
    11                                       ; height in bytes 
    3C 24 24 7E C3 A5 A5 A5 E7 66 24 24 24 24 66 42 7E          

12F6: 
    01                                       ; width in bytes  
    11                                       ; height in bytes 
    3C 3C 3C 7E FF FF FF FF FF 7E 3C 3C 3C 3C 7E 7E 7E          

1309: 
    01                                       ; width in bytes  
    0F                                       ; height in bytes   
    18 19 04 1C 18 18 18 18 18 18 18 18 18 18 1C          

131A: 
    01                                       ; width in bytes 
    0F                                       ; height in bytes 
    18 18 00 1F 18 18 18 18 18 18 18 18 18 18 1C          

132B: 
    01                                       ; width in bytes 
    0F                                       ; height in bytes  
    18 18 00 18 18 1C 1A 18 18 18 18 18 18 18 1C       


133C: 
    01                                       ; width in bytes 
    0F                                       ; height in bytes  
    18 18 00 3C 3C 3A 3A 3A 18 18 18 18 18 18 1C          

134D: 
    01                                       ; width in bytes 
    0F                                       ; height in bytes  
    18 18 00 3C 3C 5C 9C 1C 18 18 18 18 18 18 38 

135E:
    01                                       ; width in bytes 
    0F                                       ; height in bytes 
    18 18 00 F8 18 18 18 18 18 18 18 18 18 18 38 

136F:
    01                                       ; width in bytes 
    0F                                       ; height in bytes           
    98 58 20 18 18 18 18 18 18 18 18 18 18 18 38 

1380:
    01                                       ; width in bytes 
    0F                                       ; height in bytes 
    18 18 00 1D 1B 19 18 18 18 18 18 18 18 18 38 



1391: 01          ???
1392: 10 18       djnz $13AC
1394: 18 00       jr   $1396
1396: 3C          inc  a
1397: 5A          ld   e,d
1398: 99          sbc  a,c
1399: 9A          sbc  a,d
139A: 18 18       jr   $13B4
139C: 24          inc  h
139D: 44          ld   b,h
139E: 42          ld   b,d
139F: 42          ld   b,d
13A0: 41          ld   b,c
13A1: 41          ld   b,c
13A2: 80          add  a,b
13A3: 01 10 00    ld   bc,$0010
13A6: 18 18       jr   $13C0
13A8: 00          nop
13A9: 3C          inc  a
13AA: 3A 3A 7C    ld   a,($7C3A)
13AD: 18 18       jr   $13C7
13AF: 28 48       jr   z,$13F9
13B1: 4F          ld   c,a
13B2: 41          ld   b,c
13B3: 40          ld   b,b
13B4: 80          add  a,b
13B5: 01 10 18    ld   bc,$1810
13B8: 18 00       jr   $13BA
13BA: 3C          inc  a
13BB: 3A 3A 7A    ld   a,($7A3A)
13BE: 18 18       jr   $13D8
13C0: 18 18       jr   $13DA
13C2: 18 18       jr   $13DC
13C4: 18 18       jr   $13DE
13C6: 38 72       jr   c,$143A
13C8: 20 62       jr   nz,$142C
13CA: 6F          ld   l,a
13CB: 75          ld   (hl),l
13CC: 74          ld   (hl),h
13CD: 6F          ld   l,a
13CE: 6E          ld   l,(hl)
13CF: 20 73       jr   nz,$1444
13D1: 74          ld   (hl),h
13D2: 61          ld   h,c
13D3: 72          ld   (hl),d
13D4: 74          ld   (hl),h
13D5: 20 31       jr   nz,$1408
13D7: 20 6F       jr   nz,$1448
13D9: 75          ld   (hl),l
13DA: 20 32       jr   nz,$140E
13DC: 00          nop
13DD: C9          ret

13DE: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
13E1: 
    90          
    20 BE                                    ; X/Y coordinates      
13E4:  53 74 61 72 74 6B 6E 6F 65 70 66 65 20 64 72 75  Startknoepfe dru
13F4:  65 63 6B 65 6E 00                                ecken.
13FA: C9          ret

13FB: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
13FE: 
    90          
    44 BE                                    ; X/Y coordinates  
1401:  50 75 6C 73 61 72 20 53 74 61 72 74 00           Pulsar Start.
140E: C9          ret

140F: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
1412: 
    90          
    58 BE                                    ; X/Y coordinates    
1415:  49 6E 73 65 72 74 20 43 6F 69 6E 00              Insert Coin.
1421: C9          ret

1422: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
1425: 
    90          
    30 BE                                    ; X/Y coordinates  
1428:  49 6E 74 72 6F 64 75 69 72 65 20 6C 61 20 6D 6F  Introduire la mo
1438:  6E 6E 61 69 65 00                                nnaie.
143E: C9          ret

143F: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
1442: 
    90          
    48 BE                                    ; X/Y coordinates 
1445:  4D 75 6E 7A 65 20 65 69 6E 77 65 72 66 65 6E 00  Munze einwerfen.
1455: C9          ret

1456: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
1459: 
    90          
    48 BE                                    ; X/Y coordinates 

145C:  50 6F 6E 67 61 20 6C 61 20 6D 6F 6E 65 64 61 00  Ponga la moneda.
146C: C9          ret

146D: 00          nop


; NOT CALLED BY ANY ROUTINE (leftover dev code?)
146E: C5          push bc
146F: F5          push af
1470: FD 2A 76 08 ld   iy,($0876)            ; load IY with contents of MAN_PTR
1474: FD 66 07    ld   h,(iy+$07)            ; read VECTOR.P.X (X pos)
1477: FD 6E 09    ld   l,(iy+$09)            ; read VECTOR.P.Y (Y pos)
147A: CD E7 1C    call $1CE7                 ; call WALLINDEX. On return A=attr, E=index
147D: D5          push de
147E: DD 66 07    ld   h,(ix+$07)
1481: DD 6E 09    ld   l,(ix+$09)
1484: 2D          dec  l
1485: 2D          dec  l
1486: 2D          dec  l
1487: 2D          dec  l
1488: 25          dec  h
1489: 25          dec  h
148A: 25          dec  h
148B: 25          dec  h
148C: CD E7 1C    call $1CE7                 ; call WALLINDEX. On return A=attr, E=index
148F: D9          exx
1490: 47          ld   b,a
1491: D9          exx
1492: C1          pop  bc
1493: 79          ld   a,c
1494: BB          cp   e
1495: 20 03       jr   nz,$149A
1497: F1          pop  af
1498: C1          pop  bc
1499: C9          ret

149A: 7C          ld   a,h
149B: E5          push hl
149C: C6 10       add  a,$10
149E: 67          ld   h,a
149F: CD E7 1C    call $1CE7                 ; call WALLINDEX. On return A=attr, E=index
14A2: D9          exx
14A3: 4F          ld   c,a
14A4: D9          exx
14A5: 7D          ld   a,l
14A6: C6 13       add  a,$13
14A8: 6F          ld   l,a
14A9: CD E7 1C    call $1CE7                 ; call WALLINDEX. On return A=attr, E=index
14AC: D9          exx
14AD: 57          ld   d,a
14AE: D9          exx
14AF: 7D          ld   a,l
14B0: E1          pop  hl
14B1: 6F          ld   l,a
14B2: CD E7 1C    call $1CE7                 ; call WALLINDEX. On return A=attr, E=index
14B5: D9          exx
14B6: 5F          ld   e,a
14B7: F1          pop  af
14B8: 67          ld   h,a
14B9: 2E 00       ld   l,$00
14BB: CB 5C       bit  3,h
14BD: 28 06       jr   z,$14C5
14BF: 78          ld   a,b
14C0: B1          or   c
14C1: E6 08       and  $08
14C3: B5          or   l
14C4: 6F          ld   l,a
14C5: CB 54       bit  2,h
14C7: 28 06       jr   z,$14CF
14C9: 7A          ld   a,d
14CA: B3          or   e
14CB: E6 04       and  $04
14CD: B5          or   l
14CE: 6F          ld   l,a
14CF: CB 4C       bit  1,h
14D1: 28 06       jr   z,$14D9
14D3: 78          ld   a,b
14D4: B3          or   e
14D5: E6 02       and  $02
14D7: B5          or   l
14D8: 6F          ld   l,a
14D9: CB 44       bit  0,h
14DB: 28 06       jr   z,$14E3
14DD: 79          ld   a,c
14DE: B2          or   d
14DF: E6 01       and  $01
14E1: B5          or   l
14E2: 6F          ld   l,a
14E3: 2F          cpl
14E4: A4          and  h
14E5: C1          pop  bc
14E6: C9          ret

14E7: 7D          ld   a,l
14E8: 1E 00       ld   e,$00
14EA: FE 46       cp   $46
14EC: 38 08       jr   c,$14F6
14EE: 1E 05       ld   e,$05
14F0: FE 8A       cp   $8A
14F2: EA 


;
; TODO: Player bolt related
;
;

HANDLE_PLAYER_BOLTS:
14F3: 06 02       ld   b,$02
14F5: CD 05 15    call $1505
14F8: 3A 7A 43    ld   a,($437A)
14FB: C6 02       add  a,$02
14FD: E6 07       and  $07
14FF: 47          ld   b,a
1500: CD 05 15    call $1505
1503: 06 07       ld   b,$07

;
; Process player BOLTs.
;
; Expects:
; B = number of BOLT slots to iterate through
;
; Calls PROCESS_BOLT @ $151A for each slot, then advances IY by sizeof(BOLT).
;
; See also: BOLT in bolts.asm within Frenzy's source code.

1505: FD 21 7B 43 ld   iy,$437B              ; load IY with address of PLAYER_BOLTS   
1509: C5          push bc
150A: FD E5       push iy
150C: CD 1A 15    call $151A
150F: FD E1       pop  iy
1511: C1          pop  bc
1512: 11 08 00    ld   de,$0008              ; sizeof (BOLT)   
1515: FD 19       add  iy,de
1517: 10 F0       djnz $1509
1519: C9          ret

;
;
; Expects:
; IY = pointer to BOLT structure (DURL bits in Direction field at offset 0)
;
; Remarks:
; See also: 

PROCESS_BOLT:
151A: FD 7E 00    ld   a,(iy+$00)            ; read BOLT.Direction flags (DURL bits)
151D: B7          or   a                     ; test if bolt is moving in any direction (0 = No)
151E: 28 0C       jr   z,$152C               ; if bolt has no direction, then its inactive, goto $152C

; bolt is active
1520: FD 34 01    inc  (iy+$01)              ; increment BOLT.Length to count how big the laser bolt is  
1523: CD 53 15    call $1553                 ; call MOVE_AND_DRAW_BOLT
1526: DC A0 15    call c,$15A0               ; if bolt has collided with something, call HANDLE_BOLT_COLLISION 
1529: CD 7E 15    call $157E                 ; call CHECK_IF_BOLT_OFFSCREEN
152C: 01 04 00    ld   bc,$0004
152F: FD 09       add  iy,bc
1531: FD 35 01    dec  (iy+$01)
1534: C0          ret  nz
1535: FD 34 01    inc  (iy+$01)
1538: AF          xor  a                      
1539: FD B6 FD    or   (iy-$03)              ; test if BOLT.Length is zero
153C: C8          ret  z                     ; exit if so
153D: AF          xor  a
153E: FD B6 00    or   (iy+$00)
1541: CA 4A 15    jp   z,$154A

; erase the bolt
1544: CD 53 15    call $1553                 ; call MOVE_AND_DRAW_BOLT
1547: CD 7E 15    call $157E                 ; call CHECK_IF_BOLT_OFFSCREEN

154A: FD 35 FD    dec  (iy-$03)              ; decrement BOLT.Length
154D: C0          ret  nz

154E: FD 36 00 00 ld   (iy+$00),$00
1552: C9          ret


;
; Test the DURL (direction) bits to determine the direction of this BOLT.
; Adjust the X,Y coordinates of the BOLT as required.
;
; Expects:
; A = DURL bits
; IY = pointer to BOLT structure
;
; Returns:
; Carry set if the bolt has hit something such as a wall or enemy robot

MOVE_AND_DRAW_BOLT: 
1553: 0F          rrca                       ; move DURL bit for LEFT into carry
1554: D2 5A 15    jp   nc,$155A              ; if this bit isn't set, the bolt isn't moving left, goto $155A

; player bolt is moving left
1557: FD 35 02    dec  (iy+$02)              ; decrement BOLT.PX

155A: 0F          rrca                       ; move DURL bit for RIGHT into carry   
155B: D2 61 15    jp   nc,$1561              ; if this bit isn't set, the bolt isn't moving right, goto $1561

; player bolt is moving right
155E: FD 34 02    inc  (iy+$02)              ; increment BOLT.PX

1561: 0F          rrca                       ; move DURL bit for UP into carry              
1562: D2 68 15    jp   nc,$1568              ; if this bit isn't set, the bolt isn't moving right, goto $1568

; player bolt is moving up
1565: FD 35 03    dec  (iy+$03)              ; decrement BOLT.Y

1568: 0F          rrca                       ; move DURL bit for DOWN into carry
1569: D2 6F 15    jp   nc,$156F              ; if this bit isn't set, the bolt isn't moving down, goto $156F

; player bolt is moving down
156C: FD 34 03    inc  (iy+$03)              ; increment BOLT.Y

156F: FD 66 03    ld   h,(iy+$03)            ; load H with BOLT.Y
1572: FD 6E 02    ld   l,(iy+$02)            ; load L with BOLT.PX
1575: CD A1 29    call $29A1                 ; call RTOAX to get Magic RAM address and set XOR pixel mode
1578: 36 80       ld   (hl),$80              ; write a single pixel for the bolt              
157A: DB 4E       in   a,($4E)               ; read intercept_v256_r. Bit 7 determines if a pixel was plotted onto another pixel 
157C: 07          rlca                       ; if carry is set, the bolt has hit something.
157D: C9          ret


; Check if a bolt has left the screen.
;
; Expects:
; IY = pointer to BOLT structure
; B = DURL Direction bits (read from BOLT.Direction at offset 0)
;

CHECK_IF_BOLT_OFFSCREEN:
157E: FD 7E 02    ld   a,(iy+$02)            ; A = BOLT.X   
1581: FD 46 00    ld   b,(iy+$00)            ; B = BOLT.Direction 
1584: 11 FF 03    ld   de,$03FF
1587: CD 97 15    call $1597                 ; call CHECK_IF_ZERO_OR_E
158A: FD 7E 03    ld   a,(iy+$03)            ; A = BOLT.Y
158D: 11 D0 0C    ld   de,$0CD0
1590: CD 97 15    call $1597                 ; call CHECK_IF_ZERO_OR_E
1593: FD 70 00    ld   (iy+$00),b            ; set BOLT.Direction
1596: C9          ret


; Check if a supplied value in A is equal to 0 or the value in E.
;
; Expects:
; A = value
; E = value to compare against
; B = DURL Direction bits
; 
; Returns:
; if A is 0 or A == E, then B is set to 0 (all Direction bits clear) 
; Otherwise, B is same as on entry to routine (ie: left untouched).
;

CHECK_IF_ZERO_OR_E:
1597: FE 00       cp   $00                   ; is value == 0?
1599: 28 02       jr   z,$159D               ; yes, set B to 0
159B: BB          cp   e                     ; is value == E?
159C: C0          ret  nz                    ; exit if not
159D: 06 00       ld   b,$00                 ; set B to 0
159F: C9          ret                        ; and we're out


;
; Check if a bolt has hit the player or a robot.
;
; Expects:
; IY = pointer to BOLT structure 

HANDLE_BOLT_COLLISION:
; Deactivate the bolt. It's hit something - maybe just a wall.
15A0: FD 36 00 00 ld   (iy+$00),$00          ; Set BOLT.Direction to 0, meaning "Inactive"

; check if the bolt has hit the player
15A4: DD 2A 76 08 ld   ix,($0876)            ; load IX with contents of MAN_PTR
15A8: CD CB 15    call $15CB                 ; call COLLISION_DETECTION

;
; Iterate over the circular linked list of robot VECTORs at V.PTR ($0870).
; Each VECTOR stores a backward link to the previous VECTOR at offsets
; ix-$02 (lo) and ix-$01 (hi) — the same linked-list scheme as CREATE_JOB.
;
; Loop: check collision against current robot, then follow the backward
; link to the next robot. Stop when the link points back to the head.
;
15AB: ED 4B 70 08 ld   bc,($0870)            ; BC = V.PTR (head of robot list, sentinel)
15AF: 78          ld   a,b
15B0: B1          or   c                     ; check if list is empty
15B1: 28 17       jr   z,$15CA               ; if NULL, no robots → done

15B3: C5          push bc                    ; push head (cannot do ld ix,bc directly)
15B4: DD E1       pop  ix                    ; IX = robot VECTOR
15B6: CD CB 15    call $15CB                 ; call COLLISION_DETECTION against this robot
15B9: DD 66 FF    ld   h,(ix-$01)            ; read backward link (same scheme as job nodes)
15BC: DD 6E FE    ld   l,(ix-$02)
15BF: E5          push hl
15C0: DD E1       pop  ix                    ; IX = next (older) robot
15C2: 7D          ld   a,l                   ; compare link to head sentinel
15C3: B9          cp   c
15C4: 20 F0       jr   nz,$15B6              ; not back to start → check next
15C6: 7C          ld   a,h
15C7: B8          cp   b
15C8: 20 EC       jr   nz,$15B6              ; not back to start → check next
15CA: C9          ret                        ; walked full circle → done


;
; Check if a VECTOR has collided with [the head of] a BOLT.
;
; Expects:
; IX = pointer to VECTOR structure 
; IY = pointer to BOLT structure 
;
; Remarks:
; See also: HITCHK in bolts.asm within Frenzy's source code.

COLLISION_DETECTION:
15CB: DD CB 00 56 bit  2,(ix+$00)            ; test MOVE bit
15CF: C8          ret  z                     ; exit if not moving
15D0: DD 66 0B    ld   h,(ix+$0b)            
15D3: DD 6E 0A    ld   l,(ix+$0a)            ; HL = pointer to sprite pattern table
15D6: 56          ld   d,(hl)                ;                 
15D7: 23          inc  hl
15D8: 5E          ld   e,(hl)                ; DE = pointer to sprite pattern
15D9: EB          ex   de,hl
15DA: 56          ld   d,(hl)                ; read width of pattern (in bytes. 1 byte = 8 pixels)
15DB: 23          inc  hl
15DC: 5E          ld   e,(hl)                ; read height of pattern in pixels
15DD: 1C          inc  e
; Determine how far away the BOLT is from the VECTOR.
; This is a "is a single pixel [the head of the bolt] within the [VECTOR's] rectangle" routine.
; D = width of VECTOR pattern, bytes
; E = height of VECTOR pattern, pixels
; 
15DE: FD 7E 03    ld   a,(iy+$03)            ; read BOLT.Y
15E1: DD 96 09    sub  (ix+$09)              ; subtract VECTOR.Y
15E4: 3C          inc  a
15E5: F8          ret  m                     ; if BOLT.Y < VECTOR.Y then subtraction result will be negative, and M flag set
15E6: BB          cp   e                     ;  
15E7: D0          ret  nc                    ; if ((BOLT.Y - VECTOR.Y)+1 > e then exit
15E8: FD 7E 02    ld   a,(iy+$02)            ; read BOLT.X
15EB: DD 96 07    sub  (ix+$07)              ; subtract VECTOR.X
15EE: F8          ret  m                     ; if BOLT.X < VECTOR.X then subtraction result will be negative, and M flag set 
15EF: CB 22       sla  d                     ; multiply width in bytes...
15F1: CB 22       sla  d
15F3: CB 22       sla  d                     ; ... by 8 to give pattern width in pixels
15F5: 14          inc  d                     ; and add 1 
15F6: BA          cp   d                     ;  
15F7: D0          ret  nc                    ; if (BOLT.X - VECTOR.X)+1 > d then exit
15F8: DD CB 00 FE set  7,(ix+$00)            ; set HIT bit
15FC: DD CB FA C6 set  0,(ix-$06)            ; TODO: ????
1600: E1          pop  hl
1601: C9          ret

;
;
;

1602: F3          di
1603: 31 00 43    ld   sp,$4300
1606: DB 60       in   a,($60)               ; read F3
1608: CB 4F       bit  1,a                   ; CROSSHAIR PATTERN switch set?
160A: C2 3A 07    jp   nz,$073A              ; yes, jump to CROSSHAIR_PATTERN_TEST_MODE

; clear from $0870 - $08A3
160D: 21 70 08    ld   hl,$0870              
1610: 06 34       ld   b,$34
1612: 36 00       ld   (hl),$00
1614: 23          inc  hl
1615: 10 FB       djnz $1612

; clear scratch pad RAM from $4000 - $43ff
1617: 21 00 40    ld   hl,$4000
161A: 01 00 04    ld   bc,$0400
161D: AF          xor  a
161E: 77          ld   (hl),a
161F: 23          inc  hl
1620: 0D          dec  c
1621: 20 FB       jr   nz,$161E
1623: 10 F9       djnz $161E

1625: CD 21 17    call $1721

; Copy high score entries from CMOS RAM to RAM. 
1628: 21 DC 08    ld   hl,$08DC              ; load HL with address of CMOS_HIGH_SCORES
162B: 11 02 43    ld   de,$4302              ; load DE with address of HI_SCORES 
162E: 06 1E       ld   b,$1E                 ; the high score table requires $1E (30 decimal) *pairs* of bytes in CMOS
                                             ; as there are 10 high score entries requiring 6 bytes each.                 
1630: 7E          ld   a,(hl)                ; read BCD digit of score from CMOS
1631: 23          inc  hl                    ; bump HL to point to next BCD digit
1632: E6 F0       and  $F0                   ; mask in upper nibble (plain English: retain the tens part of the BCD number)
1634: 4F          ld   c,a                   ; preserve in C
1635: 7E          ld   a,(hl)                ; read BCD digit of score from CMOS
1636: 23          inc  hl                    ; bump HL to point to next BCD digit
1637: 0F          rrca                       ; shift number held in upper nibble..
1638: 0F          rrca
1639: 0F          rrca
163A: 0F          rrca                       ; .. to lower nibble
163B: E6 0F       and  $0F                   ; mask in lower nibble (discard any rubbish brought in by rrca)
163D: B1          or   c                     ; combine result with value preserved @ $1634
163E: 12          ld   (de),a                ; write to HI_SCORES in memory  
163F: 13          inc  de
1640: 10 EE       djnz $1630


1642: DB 49       in   a,($49)               ; read SYSTEM
1644: 2F          cpl
1645: 21 9F 08    ld   hl,$089F
1648: 77          ld   (hl),a
1649: 23          inc  hl
164A: 77          ld   (hl),a
164B: CD 66 16    call $1666
164E: CD AC 19    call $19AC
1651: CD 8B 18    call $188B
1654: CD 98 1A    call $1A98
1657: CD 8B 18    call $188B
165A: CD 85 16    call $1685
165D: CA 4B 16    jp   z,$164B
1660: CD 66 16    call $1666
1663: CD B2 18    call $18B2

;
; Initialise/reset the game state and linked job list.
; Disables interrupts, preserves return address on stack,
; sets stack pointer to $4300 (top of scratch RAM),
; then creates the initial job and processes it.
;
; See also: INT in init.asm within Frenzy's source code.

1666: F3          di
1667: E1          pop  hl
1668: 22 00 44    ld   ($4400),hl
166B: 32 02 44    ld   ($4402),a
166E: 31 00 43    ld   sp,$4300
1671: CD 22 1E    call $1E22                 ; call CREATE_JOB
1674: FD 2A 72 08 ld   iy,($0872)
1678: CD F1 22    call $22F1
167B: CD AB 26    call $26AB
167E: 2A 00 44    ld   hl,($4400)
1681: 3A 02 44    ld   a,($4402)
1684: E9          jp   (hl)

;
; Initialise demo mode. Saves the player's current score,
; sets IS_DEMO_MODE flag, copies default state, then
; calls $209D to start a demo game.
;
; Returns: Zero flag set according to player state.

1685: 2A 3E 43    ld   hl,($433E)
1688: 22 73 43    ld   ($4373),hl
168B: 3A 40 43    ld   a,($4340)
168E: 32 75 43    ld   ($4375),a
1691: 21 00 00    ld   hl,$0000
1694: 22 3E 43    ld   ($433E),hl
1697: 22 3F 43    ld   ($433F),hl
169A: 2A 5C 43    ld   hl,($435C)
169D: E5          push hl
169E: 21 D9 16    ld   hl,$16D9
16A1: 22 6F 43    ld   ($436F),hl
16A4: 3E FF       ld   a,$FF
16A6: 32 6E 43    ld   ($436E),a             ; set IS_DEMO_MODE
16A9: 01 0C 00    ld   bc,$000C
16AC: 11 44 43    ld   de,$4344
16AF: 21 CD 16    ld   hl,$16CD
16B2: ED B0       ldir
16B4: CD 9D 20    call $209D
16B7: E1          pop  hl
16B8: F5          push af
16B9: 22 5C 43    ld   ($435C),hl
16BC: CD 78 26    call $2678                 ; call RANDOM
16BF: 2A 73 43    ld   hl,($4373)
16C2: 22 3E 43    ld   ($433E),hl
16C5: 2A 74 43    ld   hl,($4374)
16C8: 22 3F 43    ld   ($433F),hl
16CB: F1          pop  af
16CC: C9          ret

16CD: 01 02 02    ld   bc,$0202
16D0: 1E 64       ld   e,$64
16D2: 01 70 00    ld   bc,$0070
16D5: 01 1E 02    ld   bc,$021E
16D8: 00          nop
16D9: 0A          ld   a,(bc)
16DA: 8F          adc  a,a
16DB: 12          ld   (de),a
16DC: 8F          adc  a,a
16DD: 14          inc  d
16DE: 8F          adc  a,a
16DF: 19          add  hl,de
16E0: 8F          adc  a,a
16E1: 02          ld   (bc),a
16E2: 9F          sbc  a,a
16E3: 19          add  hl,de
16E4: 8F          adc  a,a
16E5: 06 8F       ld   b,$8F
16E7: 18 8F       jr   $1678
16E9: 02          ld   (bc),a
16EA: FF          rst  $38
16EB: 00          nop
16EC: FF          rst  $38
16ED: 02          ld   (bc),a
16EE: FF          rst  $38
16EF: BF          cp   a
16F0: 08          ex   af,af'
16F1: AF          xor  a
16F2: 09          add  hl,bc
16F3: BF          cp   a
16F4: 01 BF 09    ld   bc,$09BF
16F7: FF          rst  $38
16F8: 09          add  hl,bc
16F9: C1          pop  bc
16FA: 12          ld   (de),a
16FB: 8F          adc  a,a
16FC: 00          nop
16FD: FF          rst  $38
16FE: 01 D4 08    ld   bc,$08D4
1701: BF          cp   a
1702: 18 BF       jr   $16C3
1704: 18 8F       jr   $1695
1706: 08          ex   af,af'
1707: FF          rst  $38
1708: 08          ex   af,af'
1709: B6          or   (hl)
170A: 02          ld   (bc),a
170B: C0          ret  nz
170C: 06 9C       ld   b,$9C
170E: 12          ld   (de),a
170F: 8F          adc  a,a
1710: 08          ex   af,af'
1711: BF          cp   a
1712: 00          nop
1713: FF          rst  $38
1714: 14          inc  d
1715: 8F          adc  a,a
1716: 14          inc  d
1717: 8F          adc  a,a
1718: 14          inc  d
1719: 8F          adc  a,a
171A: 14          inc  d
171B: 8F          adc  a,a
171C: 14          inc  d
171D: 8F          adc  a,a
171E: 00          nop
171F: FF          rst  $38
1720: FF          rst  $38


;
; NMI handler
;
; See also: NMI.ASM in Frenzy's source code

NMI_HANDLER:
1721: ED 73 5E 08 ld   ($085E),sp            ; preserve stack pointer in NMI_STACK_PTR
1725: 31 5E 08    ld   sp,$085E
1728: F5          push af
1729: C5          push bc
172A: D5          push de
172B: E5          push hl
172C: DD E5       push ix
172E: DB 65       in   a,($65)               ; read SW2
1730: CB 7F       bit  7,a                   ; test BOOKKEEPING button
1732: C2 05 06    jp   nz,$0605              ; if bit is set, goto BOOKKEEPING

1735: 3A 6E 43    ld   a,($436E)             ; read IS_DEMO_MODE
1738: B7          or   a
1739: CC 12 1D    call z,$1D12
173C: CD 76 17    call $1776                 ; call C.LOAD

; Do voice if not demo - This section of code is replicated in Frenzy. 
173F: 3A 6E 43    ld   a,($436E)             ; read IS_DEMO_MODE
1742: B7          or   a                     ; test flag
1743: 20 1D       jr   nz,$1762              ; if flag is non zero then in demo mode, goto STOP_TALKING.

1745: 2A 98 08    ld   hl,($0898)            ; Get VOICE_PC      

; speech loop
1748: 7C          ld   a,h
1749: B5          or   l
174A: 28 19       jr   z,$1765               ; if HL = 0, exit routine
174C: DB 44       in   a,($44)               ; read VOICE_PORT
174E: E6 C0       and  $C0              
1750: FE 40       cp   $40
1752: 20 11       jr   nz,$1765              ; if busy, exit routine

; speak
1754: 7E          ld   a,(hl)                ; read byte to send to voice hardware
1755: CB 7F       bit  7,a                   ; test if byte is a terminator byte 
1757: 20 09       jr   nz,$1762              ; if bit 7 is set, it is a terminator byte, goto STOP_TALKINGto shut up!
1759: 23          inc  hl
175A: D3 44       out  ($44),a               ; output to VOICE_PORT
175C: CB 77       bit  6,a                   ; test if a word
175E: 28 05       jr   z,$1765               ; exit if not a word
1760: 18 E6       jr   $1748                 ; do another byte for speech

; stop talking 
STOP_TALKING:
1762: 21 00 00    ld   hl,$0000              ; NULL pointer 

1765: 22 98 08    ld   ($0898),hl            ; update VOICE_PC

; Clean up
1768: DD E1       pop  ix
176A: E1          pop  hl
176B: D1          pop  de
176C: C1          pop  bc
176D: F1          pop  af
176E: ED 7B 5E 08 ld   sp,($085E)            ; restore stack pointer
1772: D3 4C       out  ($4C),a               ; write to nmi_enable_r
1774: ED 45       retn


;
; Frenzy's source code states the following about this routine:
;
; OUTPUT DATA TO ALL REGISTERS FROM RAM
; DO CONTROL REGISTERS
;
; All I know about this routine is that it writes to the audio control registers.
; If anyone has any more information, please email me. Credit will be given, of course.
;
; See also: 
; C.LOAD within NMI.ASM in Frenzy's source code.

C.LOAD:
1776: 21 78 08    ld   hl,$0878              ; load HL with address of TCR1
1779: 46          ld   b,(hl)
177A: 23          inc  hl                    ; bump HL to point to TCR2
177B: 56          ld   d,(hl)
177C: 23          inc  hl                    ; bump HL to point to TCR3
177D: 5E          ld   e,(hl)
177E: 23          inc  hl
177F: 0E 41       ld   c,$41                 ; load C with CR2_PORT
1781: CB 80       res  0,b
1783: CB C2       set  0,d
1785: ED 51       out  (c),d
1787: 0D          dec  c
1788: ED 41       out  (c),b
178A: 0C          inc  c
178B: CB 82       res  0,d
178D: ED 51       out  (c),d
178F: 0D          dec  c
1790: ED 59       out  (c),e

; DO TIMERS
T.LOAD:
1792: 0C          inc  c
1793: 0C          inc  c
1794: 06 03       ld   b,$03
1796: 79          ld   a,c
1797: 0C          inc  c
1798: 51          ld   d,c

T.LOOP:
1799: 5E          ld   e,(hl)
179A: 23          inc  hl
179B: 4F          ld   c,a
179C: 7E          ld   a,(hl)
179D: 23          inc  hl
179E: ED 79       out  (c),a
17A0: 79          ld   a,c
17A1: 4A          ld   c,d
17A2: ED 59       out  (c),e
17A4: 14          inc  d
17A5: 14          inc  d
17A6: 10 F1       djnz $1799
; DO NOISE AND VOLUMES
17A8: 0D          dec  c
17A9: 3E 00       ld   a,$00
17AB: 06 04       ld   b,$04

V.LOOP:
17AD: B6          or   (hl)
17AE: 23          inc  hl
17AF: ED 79       out  (c),a
17B1: E6 C0       and  $C0
17B3: C6 40       add  a,$40
17B5: 10 F6       djnz $17AD
17B7: C9          ret


;
;
;
;
;

17B8: 06 06       ld   b,$06
17BA: 21 BE 08    ld   hl,$08BE              ; load HL with address of CMOS_NUM_PLAYS
17BD: CD B3 2D    call $2DB3                 ; call INCREMENT_BY_1
17C0: 3A 76 43    ld   a,($4376)             ; read NUMBER_OF_PLAYERS
17C3: FE 02       cp   $02
17C5: CC B3 2D    call z,$2DB3               ; call INCREMENT_BY_1
17C8: CD 51 18    call $1851
17CB: 01 0C 00    ld   bc,$000C
17CE: 11 44 43    ld   de,$4344
17D1: 21 7F 18    ld   hl,$187F              ; load HL with address of DEFAULT_PLAYER_STATE
17D4: ED B0       ldir
17D6: CD 78 26    call $2678                 ; call RANDOM
17D9: 2A 5C 43    ld   hl,($435C)
17DC: 22 45 43    ld   ($4345),hl
17DF: AF          xor  a
17E0: 32 6E 43    ld   ($436E),a             ; clear IS_DEMO_MODE flag
17E3: 32 9A 08    ld   ($089A),a             ; clear IS_CHICKEN flag             
17E6: 3C          inc  a
17E7: 32 9B 08    ld   ($089B),a             ; set TALK_TIMER
17EA: CD A7 33    call $33A7
17ED: 21 44 43    ld   hl,$4344
17F0: 11 50 43    ld   de,$4350
17F3: 01 0C 00    ld   bc,$000C
17F6: ED B0       ldir
17F8: 3E 02       ld   a,$02
17FA: 77          ld   (hl),a
17FB: 3A 76 43    ld   a,($4376)             ; read NUMBER_OF_PLAYERS
17FE: FE 02       cp   $02
1800: 28 04       jr   z,$1806
1802: AF          xor  a
1803: 32 55 43    ld   ($4355),a
1806: FB          ei
1807: 21 1E 64    ld   hl,$641E
180A: 22 47 43    ld   ($4347),hl            ; set MAN_X and MAN_Y
180D: 3A 44 43    ld   a,($4344)             ; read CURRENT_PLAYER
1810: 3D          dec  a
1811: 28 0B       jr   z,$181E
1813: DB 4A       in   a,($4A)               ; read player 2 controls
1815: CB 7F       bit  7,a
1817: 28 05       jr   z,$181E
1819: 3E E0       ld   a,$E0
181B: 32 47 43    ld   ($4347),a             ; set MAN_X
181E: CD 9D 20    call $209D
1821: CD 78 26    call $2678                 ; call RANDOM
1824: 2A 5C 43    ld   hl,($435C)
1827: 22 45 43    ld   ($4345),hl
182A: 3E 5A       ld   a,$5A
182C: CD 6D 1E    call $1E6D                 ; call ACTIVATE_HEAD_JOB
182F: CD F1 22    call $22F1
1832: 21 49 43    ld   hl,$4349              ; load HL with address of DEATHS
1835: 35          dec  (hl)                  ; reduce number of lives 
1836: 08          ex   af,af'
1837: CD 6A 18    call $186A
183A: 7E          ld   a,(hl)
183B: B7          or   a
183C: C2 06 18    jp   nz,$1806
183F: 08          ex   af,af'
1840: 20 F5       jr   nz,$1837
1842: CD 6A 18    call $186A
1845: CD 51 2C    call $2C51
1848: CD 6A 18    call $186A
184B: CD 51 2C    call $2C51
184E: C3 4B 16    jp   $164B

;
; Clear player scores and reset voice pointer.
; Zeros P1_SCORE, P2_SCORE and sets VOICE_PC to "no speech".
;

1851: 21 00 00    ld   hl,$0000
1854: 22 3E 43    ld   ($433E),hl
1857: 22 40 43    ld   ($4340),hl
185A: 22 42 43    ld   ($4342),hl
185D: 22 A1 08    ld   ($08A1),hl
1860: 22 A2 08    ld   ($08A2),hl
1863: 21 DB 1A    ld   hl,$1ADB
1866: 22 98 08    ld   ($0898),hl            ; set VOICE_PC
1869: C9          ret

;
; Swap current player state with saved player state buffer.
; Exchanges 12 bytes at $4344 and $4350.
;
; See also: SWAP in main.asm within Frenzy's source code.

186A: E5          push hl
186B: 21 44 43    ld   hl,$4344
186E: 11 50 43    ld   de,$4350
1871: 06 0C       ld   b,$0C
1873: 1A          ld   a,(de)
1874: 4E          ld   c,(hl)
1875: EB          ex   de,hl
1876: 12          ld   (de),a
1877: 71          ld   (hl),c
1878: EB          ex   de,hl
1879: 23          inc  hl
187A: 13          inc  de
187B: 10 F6       djnz $1873
187D: E1          pop  hl
187E: C9          ret

;
; Default player state
;

DEFAULT_PLAYER_STATE:
187F: 01 00 00    ld   bc,$0000
1882: 1E 64       ld   e,$64
1884: 03          inc  bc
1885: 60          ld   h,b
1886: 00          nop
1887: 05          dec  b
1888: 5A          ld   e,d
1889: 00          nop
188A: 00          nop

;
; Wait for credits and/or start button press.
; Loops through the timer list counting down delays.
; If the free game (SW2 bit 0) button is pressed,
; awards a free credit immediately.
; Otherwise checks for coin insert or start button.
;
; See also: COIN1 in coins.asm within Frenzy's source code.
;

188B: 06 03       ld   b,$03
188D: 2A 72 08    ld   hl,($0872)
1890: 23          inc  hl
1891: 36 3C       ld   (hl),$3C
1893: 2B          dec  hl
1894: CB CE       set  1,(hl)
1896: DB 65       in   a,($65)               ; read SW2
1898: CB 47       bit  0,a
189A: 28 04       jr   z,$18A0
189C: 3E 01       ld   a,$01
189E: 18 21       jr   $18C1
18A0: CD 7B 19    call $197B
18A3: CD 97 19    call $1997
18A6: 20 0A       jr   nz,$18B2
18A8: 2A 72 08    ld   hl,($0872)
18AB: CB 4E       bit  1,(hl)
18AD: 20 E7       jr   nz,$1896
18AF: 10 DC       djnz $188D
18B1: C9          ret

;
; Start button handler. Deducts credits and determines
; number of players (1 or 2).
;
; Expects:
; A = bitmask of which start buttons were pressed
;
; Returns:
; ($4376) = 1 for 1-player, 2 for 2-player game

18B2: 6F          ld   l,a
18B3: CD F1 18    call $18F1                 ; call DECREMENT_CREDITS
18B6: CB 4D       bit  1,l
18B8: 3E 01       ld   a,$01
18BA: 28 05       jr   z,$18C1
18BC: CD F1 18    call $18F1                 ; call DECREMENT_CREDITS
18BF: 3E 02       ld   a,$02
18C1: 32 76 43    ld   ($4376),a
18C4: 2A 72 08    ld   hl,($0872)
18C7: 36 01       ld   (hl),$01
18C9: E1          pop  hl
18CA: C3 B8 17    jp   $17B8


; Display credits
;
; See also: CREDS in coins.asm within Frenzy's source code.
;

PRINT_CREDITS:
18CD: E5          push hl                    ; create space for a word on the stack
18CE: 21 00 00    ld   hl,$0000
18D1: 39          add  hl,sp                 ; HL = SP
18D2: CD E0 18    call $18E0                 ; call GET_CREDITS_AS_BCD
18D5: 77          ld   (hl),a                ; write number of credits to our temp space on stack
18D6: 06 02       ld   b,$02                 ; You can have up to 99 credits, so 2 digits long.
18D8: 11 78 D5    ld   de,$D578              ; D = Y coordinate, E = X coordinate
18DB: CD 40 2A    call $2A40                 ; call PRINT_DIGITS
18DE: E1          pop  hl                    ; discard temp space on stack 
18DF: C9          ret

;
; Read number of credits from CMOS 
;
; Returns:
; A = Number of credits, BCD encoded
;
; See also: Getc in coins.asm within Frenzy's source code.

GET_CREDITS_AS_BCD:
18E0: 3A A5 08    ld   a,($08A5)             ; load HL with address of CMOS_CREDITS+1
18E3: 0F          rrca                       ; move upper nibble..
18E4: 0F          rrca
18E5: 0F          rrca
18E6: 0F          rrca                       ; to lower nibble. 
18E7: E6 0F       and  $0F                   ; Now A is a value from 0..9.
18E9: 4F          ld   c,a                   ; preserve A in C
18EA: 3A A4 08    ld   a,($08A4)             ; load HL with address of CMOS_CREDITS
18ED: E6 F0       and  $F0                   ; mask in upper nibble and discard lower
18EF: B1          or   c                     ; combine with value we preserved in C
18F0: C9          ret


;
; Decrement credits by 1
;
; See also: DECCRD in coins.asm within Frenzy's source code.
; 

DECREMENT_CREDITS:
18F1: CD E0 18    call $18E0                 ; call GET_CREDITS_AS_BCD
18F4: C6 99       add  a,$99                 ; add -1 in 9's complement arithmetic
18F6: 27          daa
18F7: 4F          ld   c,a                   ; Preserve A in C
18F8: E6 F0       and  $F0                   ; mask in upper nibble. We can store that "as-is" into CMOS
18FA: 32 A4 08    ld   ($08A4),a             ; set first digit of CMOS_CREDITS
18FD: 79          ld   a,c                   ; restore A from C 
18FE: 07          rlca                       ; move lower nibble..
18FF: 07          rlca
1900: 07          rlca
1901: 07          rlca                       ; .. to upper nibble.
1902: E6 F0       and  $F0                   ; Discard lower nibble - it'll be garbage
1904: 32 A5 08    ld   ($08A5),a             ; set second digit of CMOS_CREDITS
1907: C9          ret


;
; Process coin insertion for a chute.
;
; Expects:
; HL = pointer to byte at $089C + offset
; B = loop counter (chute processing index)
; C = I/O port to read from
;
; Increments the appropriate CMOS coin counter
; and adds credits based on DIP switch settings.
;
; See also: ClickToCredit in coins.asm within Frenzy's source code.

1908: 7E          ld   a,(hl)
1909: B7          or   a
190A: C8          ret  z
190B: C5          push bc
190C: E5          push hl
190D: 35          dec  (hl)
190E: 78          ld   a,b
190F: 3D          dec  a
1910: 87          add  a,a
1911: 87          add  a,a
1912: 87          add  a,a
1913: 5F          ld   e,a
1914: 16 00       ld   d,$00
1916: 21 A6 08    ld   hl,$08A6
1919: 19          add  hl,de
191A: 06 08       ld   b,$08
191C: CD B3 2D    call $2DB3                 ; call INCREMENT_BY_1
191F: E1          pop  hl
1920: E5          push hl
1921: 11 05 00    ld   de,$0005
1924: 19          add  hl,de
1925: 7E          ld   a,(hl)
1926: 57          ld   d,a
1927: 3C          inc  a
1928: E6 03       and  $03
192A: 77          ld   (hl),a
192B: ED 78       in   a,(c)
192D: E6 0F       and  $0F
192F: CB 1A       rr   d
1931: 17          rla
1932: 4F          ld   c,a
1933: 06 00       ld   b,$00
1935: 21 5B 19    ld   hl,$195B
1938: 09          add  hl,bc
1939: 7E          ld   a,(hl)
193A: CB 42       bit  0,d
193C: 28 04       jr   z,$1942
193E: 07          rlca
193F: 07          rlca
1940: 07          rlca
1941: 07          rlca
1942: E6 0F       and  $0F
1944: C6 00       add  a,$00
1946: 27          daa
1947: 57          ld   d,a
1948: CD E0 18    call $18E0                 ; call GET_CREDITS_AS_BCD
194B: FE 99       cp   $99
194D: 28 09       jr   z,$1958
194F: 82          add  a,d
1950: 27          daa
1951: 30 02       jr   nc,$1955
1953: 3E 99       ld   a,$99
1955: CD F7 18    call $18F7
1958: E1          pop  hl
1959: C1          pop  bc
195A: C9          ret

195B: 11 11 22    ld   de,$2211
195E: 22 33 33    ld   ($3333),hl
1961: 44          ld   b,h
1962: 44          ld   b,h
1963: 55          ld   d,l
1964: 55          ld   d,l
1965: 66          ld   h,(hl)
1966: 66          ld   h,(hl)
1967: 77          ld   (hl),a
1968: 77          ld   (hl),a
1969: AA          xor  d
196A: AA          xor  d
196B: EE EE       xor  $EE
196D: 00          nop
196E: 11 11 22    ld   de,$2211
1971: 00          nop
1972: 55          ld   d,l
1973: 00          nop
1974: 77          ld   (hl),a
1975: 00          nop

; TODO: Data table for coin processing
1976: 21 11 21    ld   hl,$2111
1979: 11 32 C5    ld   de,$C532

;
; Main coin processing routine. Scans all 3 coin chutes
; and checks for coin insertions.
;
; See also: COINCK in coins.asm within Frenzy's source code.
;

197C: 21 9C 08    ld   hl,$089C
197F: CD E0 18    call $18E0                 ; call GET_CREDITS_AS_BCD
1982: F5          push af
1983: 01 62 03    ld   bc,$0362
1986: CD 08 19    call $1908
1989: 23          inc  hl
198A: 0C          inc  c
198B: 10 F9       djnz $1986
198D: CD E0 18    call $18E0                 ; call GET_CREDITS_AS_BCD
1990: C1          pop  bc
1991: B8          cp   b
1992: C4 CD 18    call nz,$18CD
1995: C1          pop  bc
1996: C9          ret

;
; Check if a start button has been pressed.
;
; Returns:
; L = 0 if no credits, 1 if 1 credit, 3 if multiple credits
; A = SYSTEM port bits (inverted)
; Zero flag set if no start button pressed

1997: CD E0 18    call $18E0                 ; call GET_CREDITS_AS_BCD
199A: 2E 00       ld   l,$00
199C: B7          or   a
199D: 28 08       jr   z,$19A7
199F: FE 01       cp   $01
19A1: 2E 01       ld   l,$01
19A3: 28 02       jr   z,$19A7
19A5: 2E 03       ld   l,$03
19A7: DB 49       in   a,($49)               ; read SYSTEM
19A9: 2F          cpl
19AA: A5          and  l
19AB: C9          ret


;
; Display the title / copyright screen.
; Clears screen, prints "1980 STERN Electronics, Inc.",
; shows credits, high scores, and inserts horizontal rules.
;

19AC: CD 4E 1A    call $1A4E                 ; call CLEAR_SCREEN
19AF: CD AF 35    call $35AF                 ; call SET_COLOUR_ATTRS_35AF
19B2: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
19B5: 
    90 
    0C BE                                    ; X/Y coordinates

19B8:  1F 31 39 38 30 20 53 54 45 52 4E 20 45 6C 65 63  .1980 STERN Elec
19C8:  74 72 6F 6E 69 63 73 2C 20 49 6E 63 2E 00        tronics, Inc.

19D6: CD CD 18    call $18CD
19D9: CD 14 23    call $2314                 ; call SHOW_SCORE
19DC: CD ED 1A    call $1AED                 ; call LTABLE
19DF: 04          inc  b
19E0: 1B          dec  de
19E1: 2D          dec  l
19E2: 1B          dec  de
19E3: 17          rla
19E4: 1B          dec  de
19E5: 45          ld   b,l
19E6: 1B          dec  de
19E7: 21 02 43    ld   hl,$4302
19EA: 3E 01       ld   a,$01
19EC: 32 00 43    ld   ($4300),a
19EF: 11 38 18    ld   de,$1838
19F2: D5          push de
19F3: E5          push hl
19F4: 7E          ld   a,(hl)
19F5: 23          inc  hl
19F6: B6          or   (hl)
19F7: 23          inc  hl
19F8: B6          or   (hl)
19F9: E1          pop  hl
19FA: E5          push hl
19FB: 20 04       jr   nz,$1A01
19FD: E1          pop  hl
19FE: D1          pop  de
19FF: 18 35       jr   $1A36

1A01: 21 00 43    ld   hl,$4300
1A04: 06 02       ld   b,$02
1A06: CD 40 2A    call $2A40                 ; call PRINT_DIGITS
1A09: 13          inc  de
1A0A: E1          pop  hl
1A0B: 06 06       ld   b,$06
1A0D: CD 4A 2A    call $2A4A
1A10: 13          inc  de
1A11: AF          xor  a
1A12: 4E          ld   c,(hl)
1A13: CD DB 29    call $29DB                 ; call PRINT_CHAR
1A16: 13          inc  de
1A17: 23          inc  hl
1A18: 4E          ld   c,(hl)
1A19: CD DB 29    call $29DB                 ; call PRINT_CHAR
1A1C: 13          inc  de
1A1D: 23          inc  hl
1A1E: 4E          ld   c,(hl)
1A1F: CD DB 29    call $29DB                 ; call PRINT_CHAR
1A22: 23          inc  hl
1A23: D1          pop  de
1A24: 7A          ld   a,d
1A25: C6 10       add  a,$10
1A27: 57          ld   d,a
1A28: 3A 00 43    ld   a,($4300)
1A2B: C6 01       add  a,$01
1A2D: 27          daa
1A2E: 32 00 43    ld   ($4300),a
1A31: FE 11       cp   $11
1A33: C2 F2 19    jp   nz,$19F2
1A36: 21 00 46    ld   hl,$4600
1A39: CD 45 1A    call $1A45                 ; call WRITE_FF_64_TIMES_HL
1A3C: 21 00 5B    ld   hl,$5B00
1A3F: CD 45 1A    call $1A45                 ; call WRITE_FF_64_TIMES_HL
1A42: 21 80 5D    ld   hl,$5D80



;
; Fill 64 memory addresses with #$FF (255 decimal) 
; Expects:
; HL = start memory address

WRITE_FF_64_TIMES_HL:
1A45: 3E FF       ld   a,$FF
1A47: 06 40       ld   b,$40
1A49: 77          ld   (hl),a
1A4A: 23          inc  hl
1A4B: 10 FC       djnz $1A49
1A4D: C9          ret


;
;
;

CLEAR_SCREEN:

; fill colour RAM with 0
1A4E: 21 00 81    ld   hl,$8100
1A51: 01 00 07    ld   bc,$0700
1A54: AF          xor  a
1A55: 77          ld   (hl),a
1A56: 23          inc  hl
1A57: 0D          dec  c
1A58: 20 FB       jr   nz,$1A55
1A5A: 10 F9       djnz $1A55
1A5C: F3          di
1A5D: ED 73 00 43 ld   ($4300),sp

; clear screen image RAM by using stack commands
; Set stack pointer to top of screen image RAM  
1A61: 31 00 60    ld   sp,$6000
1A64: 06 E0       ld   b,$E0
1A66: 11 00 00    ld   de,$0000
1A69: D5          push de                    ; write 0...   
1A6A: D5          push de
1A6B: D5          push de
1A6C: D5          push de
1A6D: D5          push de
1A6E: D5          push de
1A6F: D5          push de
1A70: D5          push de
1A71: D5          push de
1A72: D5          push de
1A73: D5          push de
1A74: D5          push de
1A75: D5          push de
1A76: D5          push de
1A77: D5          push de
1A78: D5          push de                    ; .. to screen image RAM
1A79: 10 EE       djnz $1A69

1A7B: ED 7B 00 43 ld   sp,($4300)
1A7F: FB          ei
1A80: DB 4A       in   a,($4A)               ; read player 2 controls
1A82: CB 7F       bit  7,a                   ; test DIP switch to check if upright                      
1A84: 20 0D       jr   nz,$1A93
1A86: 3A 44 43    ld   a,($4344)             ; read CURRENT_PLAYER
1A89: FE 02       cp   $02                   ; player 2 playing?
1A8B: 20 06       jr   nz,$1A93

; cocktail cabinet
1A8D: 3E 08       ld   a,$08
1A8F: 32 79 43    ld   ($4379),a             ; set FLIP flag
1A92: C9          ret

; upright cabinet
1A93: AF          xor  a
1A94: 32 79 43    ld   ($4379),a             ; set FLIP flag	
1A97: C9          ret






;
; Display "Insert Coin" or "Push Start Button" message based on credit count.
;
; See also: INSERT in gameover.asm within Frenzy's source code.
;
1A98: CD DD 1A    call $1ADD                 ; call CLEAR_CHYRON
1A9B: CD E0 18    call $18E0                 ; call GET_CREDITS_AS_BCD
1A9E: 28 21       jr   z,$1AC1
1AA0: 3D          dec  a
1AA1: 28 0F       jr   z,$1AB2
1AA3: CD 13 36    call $3613
1AA6: CD ED 1A    call $1AED                 ; call LTABLE
1AA9: 
    94 1B                                    ; $1B94: Push 1 or 2 player start button
    DE 1B                                    ; $1BDE: Startknoepfe druecken.  
    BB 1B                                    ; $1BBB: Pousser bouton start 1 ou 2.
    FB 1B                                    ; $1BFB: Pulsar Start. 
1AB1: C9          ret
1AB2: CD 0A 36    call $360A
1AB5: CD ED 1A    call $1AED                 ; call LTABLE
1AB8: 
    54 1B                                    ; $1B54: Push 1 Player Start Button. 
    DE 1B                                    ; $1BDE: Startknoepfe druecken.
    76 1B                                    ; $1B76: Pousser bouton start 1. 
    FB 1B                                    ; $1BFB: Pulsar Start. 
1AC0: C9          ret
1AC1: CD 01 36    call $3601                 
1AC4: CD ED 1A    call $1AED                 ; call LTABLE 
1AC7: 
    0F 1C                                    ; $1C0F: Insert Coin.      
    3F 1C                                    ; $1C3F: Munze einwerfen. 
    22 1C                                    ; $1C22: Introduire la monnaie.          
    56 1C                                    ; $1C56: Ponga la moneda.  

; Say "coins detected in pocket"
1ACF: 21 D6 1A    ld   hl,$1AD6              ; load HL with address of COINS_DETECTED_IN_POCKET speech   
1AD2: 22 98 08    ld   ($0898),hl            ; set VOICE_PC
1AD5: C9          ret


COINS_DETECTED_IN_POCKET:
1AD6: 
    65            
    10                                       ; COINS
    09                                       ; DETECTED
    0B                                       ; IN
    11                                       ; POCKET
    45 
    FF    


;
; Clears the message area which holds the copyright message and instructions to players 
; such as Push 1 or 2 player start
; 
; Remarks: 
; named LERASE (erase line for messages) in gameover.asm within Frenzy's source code.
;
CLEAR_CHYRON:
1ADD: 21 C0 5B    ld   hl,$5BC0
1AE0: 01 C0 02    ld   bc,$02C0
1AE3: AF          xor  a
1AE4: 77          ld   (hl),a
1AE5: 23          inc  hl
1AE6: 0D          dec  c
1AE7: C2 E4 1A    jp   nz,$1AE4
1AEA: 10 F8       djnz $1AE4
1AEC: C9          ret


;
;
;
; Remarks: 
; Named LTABLE in gameover.asm within Frenzy's source code.
;

LTABLE:
1AED: E1          pop  hl                    ; get table address
1AEE: 54          ld   d,h
1AEF: 5D          ld   e,l                   ; save table address in DE
1AF0: 01 08 00    ld   bc,$0008              ; offset to end of table
1AF3: 09          add  hl,bc
1AF4: E5          push hl
1AF5: EB          ex   de,hl
1AF6: DB 60       in   a,($60)               ; read F3                
1AF8: E6 C0       and  $C0                   ; mask in bits set when German, French, Spanish
1AFA: 07          rlca
1AFB: 07          rlca
1AFC: 07          rlca
1AFD: 4F          ld   c,a
1AFE: 09          add  hl,bc
1AFF: 7E          ld   a,(hl)
1B00: 23          inc  hl
1B01: 66          ld   h,(hl)
1B02: 6F          ld   l,a
1B03: E9          jp   (hl)

1B04: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
1B07: 
    90          
    58 00                                    ; X/Y coordinates          
1B0A:  48 69 67 68 20 53 63 6F 72 65 73 00              High Scores.

1B16: C9          ret

1B17: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
1B1A: 
    90          
    48 00                                    ; X/Y coordinates             
1B1D:  4D 65 69 6C 6C 65 75 72 20 53 63 6F 72 65 00     Meilleur Score..

1B2C: C9          ret

1B2D: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
1B30: 
    90          
    40 00                                    ; X/Y coordinates           
1B33:  48 6F 65 63 68 73 74 65 72 20 47 65 62 6E 69 73  Hoechster Gebnis
1B43:  00

1B44: C9          ret

1B45: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
1B48: 
    90          
    5C 00                                    ; X/Y coordinates  
1B4B:  52 65 63 6F 72 64 73 00                          Records.

1B53: C9          ret

1B54: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
1B57: 
    90          
    14 BE                                    ; X/Y coordinates  
1B5A:  50 75 73 68 20 31 20 50 6C 61 79 65 72 20 53 74  Push 1 Player St
1B6A:  61 72 74 20 42 75 74 74 6F 6E 00                 art Button.
1B75: C9          ret

1B76: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
1B79: 
    90
    24 BE                                    ; X/Y coordinates 
1B7C:  50 6F 75 73 73 65 72 20 62 6F 75 74 6F 6E 20 73  Pousser bouton s
1B8C:  74 61 72 74 20 31 00                             tart 1.
1B93: C9          ret

1B94: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
1B97: 
    90          
    04 BE                                    ; X/Y coordinates 
1B9A:  50 75 73 68 20 31 20 6F 72 20 32 20 50 6C 61 79  Push 1 or 2 Play
1BAA:  65 72 20 53 74 61 72 74 20 42 75 74 74 6F 6E 00  er Start Button.
1BBA: C9          ret

1BBB: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
1BBE: 
    90          
    10 BE                                    ; X/Y coordinates 
1BC1:  50 6F 75 73 73 65 72 20 62 6F 75 74 6F 6E 20 73  Pousser bouton s
1BD1:  74 61 72 74 20 31 20 6F 75 20 32 00              tart 1 ou 2.
1BDD: C9          ret

1BDE: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
1BE1: 
    90          
    20 BE                                    ; X/Y coordinates
1BE4:  53 74 61 72 74 6B 6E 6F 65 70 66 65 20 64 72 75  Startknoepfe dru
1BF4:  65 63 6B 65 6E 00 C9 CD 7B 29 90 44 BE 50 75 6C  ecken.
1BFA: C9          ret

1BFB: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
1BFE: 
    90          
    44 BE                                    ; X/Y coordinates 
1C01:  50 75 6C 73 61 72 20 53 74 61 72 74 00           Pulsar Start.
1C0E: C9          ret

1C0F: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
1C12: 
    90          
    58 BE                                    ; X/Y coordinates 
1C15:  49 6E 73 65 72 74 20 43 6F 69 6E 00              Insert Coin.
1C21: C9          ret

1C22: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
1C25: 
    90          
    30 BE                                    ; X/Y coordinates 
1C28:  49 6E 74 72 6F 64 75 69 72 65 20 6C 61 20 6D 6F  Introduire la mo
1C38:  6E 6E 61 69 65 00                                nnaie.          
1C3E: C9          ret

1C3F: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
1C42: 
    90
    48 BE                                    ; X/Y coordinates 
1C45:  4D 75 6E 7A 65 20 65 69 6E 77 65 72 66 65 6E 00  Munze einwerfen.
1C55: C9          ret

1C56: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
1C59: 
    90
    48 BE                                    ; X/Y coordinates 
1C5C:  50 6F 6E 67 61 20 6C 61 20 6D 6F 6E 65 64 61 00  Ponga la moneda.
1C6C: C9          ret

1C6D: A3          and  e                     ; stray byte (orphaned from data preceding)

;
; Obstacle check for robot pathfinding. Called from SETPAT when a robot
; is about to move in a new direction. Probes the maze layout grid at 4
; points around the robot and tests which directions are blocked by walls.
;
; See also: IQ in iq.asm within Frenzy's source code.
;
; Expects:
; A = desired direction (DURL bits)
; IX = pointer to robot's VECTOR struct
;
; Returns:
; A = allowed direction bits (blocked directions cleared)
;

IQ:
1C6E: C5          push bc                    
1C6F: F5          push af                    ; save desired direction (DURL bits)

; Probe 1: get maze attribute at player's position
1C70: FD 2A 76 08 ld   iy,($0876)            ; IY = MAN_PTR (player's VECTOR)
1C74: FD 66 07    ld   h,(iy+$07)            ; H = player X
1C77: FD 6E 09    ld   l,(iy+$09)            ; L = player Y
1C7A: CD E7 1C    call $1CE7                 ; call WALLINDEX. On return A=attr, E=index
1C7D: D5          push de                    ; save D=player attr, E=player index

; Probe 2: get maze attribute at robot's position (offset -4,-4)
1C7E: DD 66 07    ld   h,(ix+$07)            ; H = robot X
1C81: DD 6E 09    ld   l,(ix+$09)            ; L = robot Y
1C84: 2D          dec  l                     ; offset robot position
1C85: 2D          dec  l                     ;  4 pixels up
1C86: 2D          dec  l
1C87: 2D          dec  l
1C88: 25          dec  h                     ;  4 pixels left
1C89: 25          dec  h
1C8A: 25          dec  h
1C8B: 25          dec  h
1C8C: CD E7 1C    call $1CE7                 ; call WALLINDEX → A=attr (stored as B' top-left probe)
1C8F: D9          exx                        ; switch to alternate registers
1C90: 47          ld   b,a                   ; B' = top-left probe attribute
1C91: D9          exx                        ; back to main registers

; Compare player and robot grid indices — if same, no obstacle on path
1C92: C1          pop  bc                    ; B = player attr, C = player grid index
1C93: 79          ld   a,c                   ; A = player's grid index
1C94: BB          cp   e                     ; compare with robot's grid index
1C95: 20 03       jr   nz,$1C9A              ; if different, probe more positions
1C97: F1          pop  af                    ; same: restore registers
1C98: C1          pop  bc                    ;  and return without modifying
1C99: C9          ret                        ;  the desired direction

; Probe 3: attribute at (robot.X+16-4, robot.Y-4) = top-right probe
1C9A: 7C          ld   a,h                   ; A = robot.X-4 (HL still holds robot pos from $1C88)
1C9B: E5          push hl                    ; save (robot.X-4, robot.Y-4) coordinates
1C9C: C6 10       add  a,$10                 ; add 16 to X
1C9E: 67          ld   h,a                   ; H = robot.X-4+16 = robot.X+12
1C9F: CD E7 1C    call $1CE7                 ; call WALLINDEX → A=attr (stored as C' top-right probe)
1CA2: D9          exx
1CA3: 4F          ld   c,a                   ; C' = top-right probe attribute
1CA4: D9          exx

; Probe 4: attribute at (robot.X+12, robot.Y-4+19) = bottom-right probe
1CA5: 7D          ld   a,l                   ; A = robot.Y-4
1CA6: C6 13       add  a,$13                 ; add 19 to Y
1CA8: 6F          ld   l,a                   ; L = robot.Y-4+19 = robot.Y+15
1CA9: CD E7 1C    call $1CE7                 ; call WALLINDEX → A=attr (stored as D' bottom-right probe)
1CAC: D9          exx
1CAD: 57          ld   d,a                   ; D' = bottom-right probe attribute
1CAE: D9          exx

; Probe 5: attribute at (original.X-4, robot.Y+15) = bottom-left probe
1CAF: 7D          ld   a,l                   ; A = robot.Y+15
1CB0: E1          pop  hl                    ; H = (robot.X-4), L = (robot.Y-4) from push @ $1C9B
1CB1: 6F          ld   l,a                   ; L = robot.Y+15, H = robot.X-4
1CB2: CD E7 1C    call $1CE7                 ; call WALLINDEX → A=attr (stored as E' bottom-left probe)
1CB5: D9          exx
1CB6: 5F          ld   e,a                   ; E' = bottom-left probe attribute
1CB7: F1          pop  af                    ; A = original desired direction (DURL bits)
1CB8: 67          ld   h,a                   ; H = direction bits
1CB9: 2E 00       ld   l,$00                 ; L = 0 (accumulator for blocked direction mask)
; The 4 probe points form a 2x2 grid centred on the robot:
;   B' = top-left     (robot.X-4, robot.Y-4)
;   C' = top-right    (robot.X+12, robot.Y-4)
;   E' = bottom-left  (robot.X-4, robot.Y+15)
;   D' = bottom-right (robot.X+12, robot.Y+15)
;
; For each desired direction bit, OR two adjacent probe attributes
; and test the corresponding bit. If set, that path is blocked.

; Test DOWN (bit 3): check top-edge probes (B' | C') & $08
1CBB: CB 5C       bit  3,h                   ; DOWN direction desired?
1CBD: 28 06       jr   z,$1CC5               ; if not, skip
1CBF: 78          ld   a,b                   ; A = top-left probe attribute
1CC0: B1          or   c                     ; OR with top-right probe
1CC1: E6 08       and  $08                   ; test bit 3 (DOWN block flag)
1CC3: B5          or   l                     ; accumulate in L
1CC4: 6F          ld   l,a

; Test UP (bit 2): check bottom-edge probes (D' | E') & $04
1CC5: CB 54       bit  2,h                   ; UP direction desired?
1CC7: 28 06       jr   z,$1CCF               ; if not, skip
1CC9: 7A          ld   a,d                   ; A = bottom-right probe attribute
1CCA: B3          or   e                     ; OR with bottom-left probe
1CCB: E6 04       and  $04                   ; test bit 2 (UP block flag)
1CCD: B5          or   l                     ; accumulate in L
1CCE: 6F          ld   l,a

; Test RIGHT (bit 1): check left-edge probes (B' | E') & $02
1CCF: CB 4C       bit  1,h                   ; RIGHT direction desired?
1CD1: 28 06       jr   z,$1CD9               ; if not, skip
1CD3: 78          ld   a,b                   ; A = top-left probe attribute
1CD4: B3          or   e                     ; OR with bottom-left probe
1CD5: E6 02       and  $02                   ; test bit 1 (RIGHT block flag)
1CD7: B5          or   l                     ; accumulate in L
1CD8: 6F          ld   l,a

; Test LEFT (bit 0): check right-edge probes (C' | D') & $01
1CD9: CB 44       bit  0,h                   ; LEFT direction desired?
1CDB: 28 06       jr   z,$1CE3               ; if not, skip
1CDD: 79          ld   a,c                   ; A = top-right probe attribute
1CDE: B2          or   d                     ; OR with bottom-right probe
1CDF: E6 01       and  $01                   ; test bit 0 (LEFT block flag)
1CE1: B5          or   l                     ; accumulate in L
1CE2: 6F          ld   l,a

; L now has bits set for blocked directions. Invert to get clear paths.
1CE3: 2F          cpl                        ; invert: L = allowed direction mask
1CE4: A4          and  h                     ; AND with original desired direction
                                             ; result: only desired + unblocked bits survive
1CE5: C1          pop  bc                    ; restore BC
1CE6: C9          ret                        ; return A = allowed direction bits

;
; Map an (X,Y) coordinate pair to an attribute in the 15-zone maze layout grid.
; The screen is divided into 3 rows (Y-based) x 5 cols (X-based), producing
; 15 zones. Each zone has an attribute byte stored at MAZE_ZONES+index.
;
; See also: WallIndex in iq.asm within Frenzy's source code.
;
; Expects:
; H = X coordinate
; L = Y coordinate
;
; Returns:
; A = attribute byte from MAZE_ZONES[index]
; E = grid index (0-14)
;
; Grid layout (5 cols x 3 rows):
;    Row 0 (Y<70):     indices 0-4
;    Row 1 (70≤Y<138): indices 5-9
;    Row 2 (Y≥138):    indices 10-14
;    Within each row, col 0 = X<58, …, col 4 = X≥202.

WALLINDEX:
; First find out what row we are in.
1CE7: 7D          ld   a,l                   ; A = Y (used to select row from MAZE_ZONES)
1CE8: 1E 00       ld   e,$00                 ; default E = 0 (row 0)
1CEA: FE 46       cp   $46                   ; Y < 70? (row 0)
1CEC: 38 08       jr   c,$1CF6               ; yes, skip with E=0
1CEE: 1E 05       ld   e,$05                 ; no: E = 5 (row 1 offset)
1CF0: FE 8A       cp   $8A                   ; Y < 138? (row 1)
1CF2: 38 02       jr   c,$1CF6               ; yes, skip with E=5
1CF4: 1E 0A       ld   e,$0A                 ; no: E = 10 (row 2 offset)

; E now = zero-based row*5. Next: determine which of the 5 X columns we are in.
1CF6: 7C          ld   a,h                   ; A = X (used to select column from MAZE_ZONES)
1CF7: 06 05       ld   b,$05                 ; B = 5 columns per row
1CF9: 0E 3A       ld   c,$3A                 ; C = 58 (first column X boundary)
1CFB: 16 30       ld   d,$30                 ; D = 48 (step between boundaries)

; Loop: increment E for each X boundary passed
1CFD: B9          cp   c                     ; X < current boundary?
1CFE: 38 08       jr   c,$1D08               ; yes, E = index, goto $1D08
1D00: 1C          inc  e                     ; no → advance to next column (E++)
1D01: 08          ex   af,af'                ; save X
1D02: 79          ld   a,c                   ; A = current boundary
1D03: 82          add  a,d                   ; add step (48) for next boundary
1D04: 4F          ld   c,a                   ; update boundary
1D05: 08          ex   af,af'                ; restore X
1D06: 10 F5       djnz $1CFD                 ; loop for up to 5 columns

; E = final grid index.
1D08: EB          ex   de,hl                 ; HL = index
1D09: 01 5E 43    ld   bc,$435E              ; BC = MAZE_ZONES base address
1D0C: 26 00       ld   h,$00                 ; HL = unsigned 8-bit index
1D0E: 09          add  hl,bc                 ; HL = MAZE_ZONES + index
1D0F: 7E          ld   a,(hl)                ; A = zone attribute byte
1D10: EB          ex   de,hl                 ; restore original XY to DE
1D11: C9          ret

;
; Execute audio control bytecode sequence.
; Reads bytes from the audio program counter ($0885)
; and dispatches them via the jump table at $1D31.
; Called from NMI handler when not in demo mode.
;

1D12: DD 21 22 1D ld   ix,$1D22
1D16: ED 4B 85 08 ld   bc,($0885)
1D1A: CD 22 1D    call $1D22
1D1D: ED 43 85 08 ld   ($0885),bc
1D21: C9          ret

;
; Audio bytecode interpreter. Reads opcode byte from (BC),
; doubles it for table lookup, then dispatches to the
; appropriate handler routine via jump table @ $1D31.
;
; BC is the bytecode stream pointer on entry and exit.
;
; Opcodes handled:
; 0  = reset audio state (clear $0887 and $0889)
; 1  = nop (no operation)
; 2  = REL: add signed byte to BC (relative branch in bytecode)
; 3  = LOOP: decrement (HL), if non-zero do relative branch back
; 4  = move byte: (ptr1) → (ptr2), both pointers read from stream
; 5  = move word: (ptr1) → (ptr2), both pointers read from stream
; 6  = store immediate byte from stream to (HL)
; 7  = store immediate word from stream to (HL)
; 8  = add byte from (ptr1) to (ptr2), both pointers from stream
; 9  = add word from (ptr1) to (ptr2), both pointers from stream
; 10 = add immediate byte from stream to (HL)
; 11 = add immediate word from stream to (HL)
; 12 = arithmetic shift right (HL) by immediate E bits
; 13 = 16-bit arithmetic shift right (HL) by immediate E bits
; 14 = fill (HL) with E bytes read from stream
; 15 = same as 14 but E *= 2 first
;

1D22: 0A          ld   a,(bc)
1D23: 03          inc  bc
1D24: 26 00       ld   h,$00
1D26: 87          add  a,a
1D27: 6F          ld   l,a
1D28: 11 31 1D    ld   de,$1D31
1D2B: 19          add  hl,de
1D2C: 7E          ld   a,(hl)
1D2D: 23          inc  hl
1D2E: 66          ld   h,(hl)
1D2F: 6F          ld   l,a
1D30: E9          jp   (hl)
1D31: 51          ld   d,c
1D32: 1D          dec  e
1D33: 5C          ld   e,h
1D34: 1D          dec  e
1D35: 5D          ld   e,l
1D36: 1D          dec  e
1D37: 68          ld   l,b
1D38: 1D          dec  e
1D39: 74          ld   (hl),h
1D3A: 1D          dec  e
1D3B: 7E          ld   a,(hl)
1D3C: 1D          dec  e
1D3D: 8A          adc  a,d
1D3E: 1D          dec  e
1D3F: 96          sub  (hl)
1D40: 1D          dec  e
1D41: A7          and  a
1D42: 1D          dec  e
1D43: B1          or   c
1D44: 1D          dec  e
1D45: BD          cp   l
1D46: 1D          dec  e
1D47: CB 1D       rr   l
1D49: E4 1D F4    call po,$F41D
1D4C: 1D          dec  e
1D4D: 09          add  hl,bc
1D4E: 1E 1C       ld   e,$1C
1D50: 1E 0B       ld   e,$0B
1D52: 21 00 00    ld   hl,$0000
1D55: 22 87 08    ld   ($0887),hl
1D58: 22 89 08    ld   ($0889),hl
1D5B: C9          ret
1D5C: C9          ret
1D5D: 0A          ld   a,(bc)
1D5E: 03          inc  bc
1D5F: 6F          ld   l,a
1D60: 87          add  a,a
1D61: 9F          sbc  a,a
1D62: 67          ld   h,a
1D63: 09          add  hl,bc
1D64: 44          ld   b,h
1D65: 4D          ld   c,l
1D66: DD E9       jp   (ix)
1D68: 0A          ld   a,(bc)
1D69: 03          inc  bc
1D6A: 6F          ld   l,a
1D6B: 0A          ld   a,(bc)
1D6C: 03          inc  bc
1D6D: 67          ld   h,a
1D6E: 35          dec  (hl)
1D6F: 20 EC       jr   nz,$1D5D
1D71: 03          inc  bc
1D72: DD E9       jp   (ix)
1D74: 0A          ld   a,(bc)
1D75: 03          inc  bc
1D76: 6F          ld   l,a
1D77: 0A          ld   a,(bc)
1D78: 03          inc  bc
1D79: 67          ld   h,a
1D7A: 5E          ld   e,(hl)
1D7B: C3 8D 1D    jp   $1D8D
1D7E: 0A          ld   a,(bc)
1D7F: 03          inc  bc
1D80: 6F          ld   l,a
1D81: 0A          ld   a,(bc)
1D82: 03          inc  bc
1D83: 67          ld   h,a
1D84: 5E          ld   e,(hl)
1D85: 23          inc  hl
1D86: 56          ld   d,(hl)
1D87: C3 9C 1D    jp   $1D9C
1D8A: 0A          ld   a,(bc)
1D8B: 03          inc  bc
1D8C: 5F          ld   e,a
1D8D: 0A          ld   a,(bc)
1D8E: 03          inc  bc
1D8F: 6F          ld   l,a
1D90: 0A          ld   a,(bc)
1D91: 03          inc  bc
1D92: 67          ld   h,a
1D93: 73          ld   (hl),e
1D94: DD E9       jp   (ix)
1D96: 0A          ld   a,(bc)
1D97: 03          inc  bc
1D98: 5F          ld   e,a
1D99: 0A          ld   a,(bc)
1D9A: 03          inc  bc
1D9B: 57          ld   d,a
1D9C: 0A          ld   a,(bc)
1D9D: 03          inc  bc
1D9E: 6F          ld   l,a
1D9F: 0A          ld   a,(bc)
1DA0: 03          inc  bc
1DA1: 67          ld   h,a
1DA2: 73          ld   (hl),e
1DA3: 23          inc  hl
1DA4: 72          ld   (hl),d
1DA5: DD E9       jp   (ix)
1DA7: 0A          ld   a,(bc)
1DA8: 03          inc  bc
1DA9: 6F          ld   l,a
1DAA: 0A          ld   a,(bc)
1DAB: 03          inc  bc
1DAC: 67          ld   h,a
1DAD: 5E          ld   e,(hl)
1DAE: C3 C0 1D    jp   $1DC0
1DB1: 0A          ld   a,(bc)
1DB2: 03          inc  bc
1DB3: 6F          ld   l,a
1DB4: 0A          ld   a,(bc)
1DB5: 03          inc  bc
1DB6: 67          ld   h,a
1DB7: 5E          ld   e,(hl)
1DB8: 23          inc  hl
1DB9: 56          ld   d,(hl)
1DBA: C3 D1 1D    jp   $1DD1
1DBD: 0A          ld   a,(bc)
1DBE: 03          inc  bc
1DBF: 5F          ld   e,a
1DC0: 0A          ld   a,(bc)
1DC1: 03          inc  bc
1DC2: 6F          ld   l,a
1DC3: 0A          ld   a,(bc)
1DC4: 03          inc  bc
1DC5: 67          ld   h,a
1DC6: 7E          ld   a,(hl)
1DC7: 83          add  a,e
1DC8: 77          ld   (hl),a
1DC9: DD E9       jp   (ix)
1DCB: 0A          ld   a,(bc)
1DCC: 03          inc  bc
1DCD: 5F          ld   e,a
1DCE: 0A          ld   a,(bc)
1DCF: 03          inc  bc
1DD0: 57          ld   d,a
1DD1: 0A          ld   a,(bc)
1DD2: 03          inc  bc
1DD3: 6F          ld   l,a
1DD4: 0A          ld   a,(bc)
1DD5: 03          inc  bc
1DD6: 67          ld   h,a
1DD7: E5          push hl
1DD8: 7E          ld   a,(hl)
1DD9: 23          inc  hl
1DDA: 66          ld   h,(hl)
1DDB: 6F          ld   l,a
1DDC: 19          add  hl,de
1DDD: EB          ex   de,hl
1DDE: E1          pop  hl
1DDF: 73          ld   (hl),e
1DE0: 23          inc  hl
1DE1: 72          ld   (hl),d
1DE2: DD E9       jp   (ix)
1DE4: 0A          ld   a,(bc)
1DE5: 03          inc  bc
1DE6: 5F          ld   e,a
1DE7: 0A          ld   a,(bc)
1DE8: 03          inc  bc
1DE9: 6F          ld   l,a
1DEA: 0A          ld   a,(bc)
1DEB: 03          inc  bc
1DEC: 67          ld   h,a
1DED: CB 2E       sra  (hl)
1DEF: 1D          dec  e
1DF0: 20 FB       jr   nz,$1DED
1DF2: DD E9       jp   (ix)
1DF4: 0A          ld   a,(bc)
1DF5: 03          inc  bc
1DF6: 5F          ld   e,a
1DF7: 0A          ld   a,(bc)
1DF8: 03          inc  bc
1DF9: 6F          ld   l,a
1DFA: 0A          ld   a,(bc)
1DFB: 03          inc  bc
1DFC: 67          ld   h,a
1DFD: 23          inc  hl
1DFE: CB 2E       sra  (hl)
1E00: 2B          dec  hl
1E01: CB 1E       rr   (hl)
1E03: 23          inc  hl
1E04: 1D          dec  e
1E05: 20 F7       jr   nz,$1DFE
1E07: DD E9       jp   (ix)
1E09: 0A          ld   a,(bc)
1E0A: 03          inc  bc
1E0B: 5F          ld   e,a
1E0C: 0A          ld   a,(bc)
1E0D: 03          inc  bc
1E0E: 6F          ld   l,a
1E0F: 0A          ld   a,(bc)
1E10: 03          inc  bc
1E11: 67          ld   h,a
1E12: 0A          ld   a,(bc)
1E13: 77          ld   (hl),a
1E14: 23          inc  hl
1E15: 03          inc  bc
1E16: 1D          dec  e
1E17: C2 12 1E    jp   nz,$1E12
1E1A: DD E9       jp   (ix)
1E1C: 0A          ld   a,(bc)
1E1D: 03          inc  bc
1E1E: 87          add  a,a
1E1F: C3 0B 1E    jp   $1E0B

;
; CREATE_JOB — Push a 4-byte node onto the stack and make it the new
; head of TIMER_LIST_PTR ($0872).
;
; Pseudocode:
;   pop return_addr
;   push $0001; push $0001          // 4 bytes for the node
;   node = SP                        // node address
;   if (TIMER_LIST_PTR == 0):
;       TIMER_LIST_PTR = node       // first job ever
;   else:
;       old_head = TIMER_LIST_PTR
;       *(old_head - 2) = node      // link old head back to new node
;       TIMER_LIST_PTR = node       // node becomes new head
;   return to return_addr
;
; Nodes live entirely on the stack. The link between a node and the
; next-older node is stored in the 2 bytes immediately BEFORE the
; older node (at old_head - 2). The runner ($1E8F) walks this reverse
; chain, following these 2-byte links backward through time.

CREATE_JOB:
1E22: E1          pop  hl                    ; pop return address → jump here when job runs
1E23: D9          exx                        ; use alternate registers
1E24: 21 01 00    ld   hl,$0001
1E27: E5          push hl                    ; allocate node (4 bytes, init $0001)
1E28: E5          push hl
1E29: 21 00 00    ld   hl,$0000
1E2C: 39          add  hl,sp                 ; HL = node address
1E2D: 3A 73 08    ld   a,($0873)             ; TIMER_LIST_PTR MSB
1E30: B7          or   a                     ; any previous jobs?
1E31: 20 06       jr   nz,$1E39              ; yes → link them up
1E33: E5          push hl
1E34: 22 72 08    ld   ($0872),hl            ; first job: set head
1E37: 18 1E       jr   $1E57                 ; done
1E39: FD E5       push iy                    ; preserve IY
1E3B: C1          pop  bc
1E3C: FD 2A 72 08 ld   iy,($0872)            ; IY = old head
1E40: EB          ex   de,hl                 ; DE = new node
1E41: FD 66 FF    ld   h,(iy-$01)            ; read old head's backward link
1E44: FD 6E FE    ld   l,(iy-$02)
1E47: E5          push hl                    ; save it
1E48: F3          di
1E49: FD 72 FF    ld   (iy-$01),d            ; overwrite with new node address
1E4C: FD 73 FE    ld   (iy-$02),e            ; → old head now links back to new
1E4F: FB          ei
1E50: ED 53 72 08 ld   ($0872),de            ; new node is now the head
1E54: C5          push bc                    ; restore IY
1E55: FD E1       pop  iy
1E57: D9          exx                        ; back to normal registers
1E58: E9          jp   (hl)                  ; return to caller


;
; Job bootstrap helper — allocate 24 bytes of stack workspace for the
; current job, then store the old SP (before allocation) into the head
; node at +2. Finally jumps to (IY), which is typically the runner.
;
; This is called once per job, right after CREATE_JOB and ACTIVATE_HEAD_JOB.
; The saved SP becomes the job's "entry point" that the runner will
; restore to when first activating the job.
;
; Expects: IY = address to jump to after setup (usually runner at $1E8B)

JOB_BOOTSTRAP:
1E59: 21 00 00    ld   hl,$0000
1E5C: 39          add  hl,sp                 ; HL = SP (before allocation)
1E5D: EB          ex   de,hl                 ; DE = old SP
1E5E: 21 E8 FF    ld   hl, -24               ; allocate 24 bytes
1E61: 39          add  hl,sp
1E62: F9          ld   sp,hl
1E63: 2A 72 08    ld   hl,($0872)            ; HL = head node
1E66: 23          inc  hl                    ; offset to +2
1E67: 23          inc  hl
1E68: 73          ld   (hl),e                ; store old SP (checkpoint)
1E69: 23          inc  hl
1E6A: 72          ld   (hl),d
1E6B: FD E9       jp   (iy)                  ; jump to runner


;
; ACTIVATE_HEAD_JOB — Mark the head job as runnable with a delay countdown.
;
; Sets the head node's delay field and writes $82 to the flags field.
; After this, the runner will see this job as active (once the delay
; expires) and resume it.

ACTIVATE_HEAD_JOB:
1E6D: FD 2A 72 08 ld   iy,($0872)            ; IY = head node
1E71: FD 77 01    ld   (iy+$01),a            ; store delay count
1E74: FD 36 00 82 ld   (iy+$00),$82          ; set flags = $82 (active + ?)


;
; STOP_JOB — Checkpoint the current job and yield to the runner.
;
; Saves SP into the head node at +2 (the job's resume point), resets
; SP to $0870 (above all job data), then falls into the runner.
; The job will be resumed later when the runner restores its saved SP
; and returns.

STOP_JOB:
1E78: FD 2A 72 08 ld   iy,($0872)            ; IY = head node
1E7C: 21 00 00    ld   hl,$0000
1E7F: 39          add  hl,sp                 ; HL = current SP (checkpoint)
1E80: 31 70 08    ld   sp,$0870              ; reset SP above job data
1E83: FD 75 02    ld   (iy+$02),l            ; save checkpoint SP at +2,+3
1E86: FD 74 03    ld   (iy+$03),h
                                             ; fall through to runner
1E89: 18 04       jr   $1E8F

;
; JOB RUNNER — Walk the backward-linked chain of job nodes looking
; for one whose 2-byte link has bit 0 set ("active"). When found,
; restore that job's saved SP and return → the job resumes.
;
; Pseudocode:
;   node = TIMER_LIST_PTR
;   loop:
;       link = *(node - 2)          // 2-byte link stored before node
;       node = link                  // follow it backward
;       if (link & 1): break        // bit 0 set → active
;   SP = *(node + 2)                // restore job's saved stack
;   TIMER_LIST_PTR = node           // becomes new head
;   return                          // job resumes
;
; Entry at $1E8B: loads node from TIMER_LIST_PTR (fresh start).
; Entry at $1E8F: node already in IY (from STOP_JOB fall-through).
;
; The link stored at node-2 was set by the NEXT call to CREATE_JOB
; (the job created after this one). For the oldest job in the chain
; (the first one created), node-2 contains whatever was on the stack
; at the time — typically a return address whose bit 0 depends on
; the caller's ROM address.

1E8B: FD 2A 72 08 ld   iy,($0872)            ; node = head

1E8F: FD 66 FF    ld   h,(iy-$01)            ; read link at node-2
1E92: FD 6E FE    ld   l,(iy-$02)
1E95: E5          push hl
1E96: FD E1       pop  iy                    ; node = link (walk backward)
1E98: CB 46       bit  0,(hl)                ; link & 1 ?
1E9A: CA 8F 1E    jp   z,$1E8F               ; no → keep walking

1E9D: FD 6E 02    ld   l,(iy+$02)            ; load job's saved SP
1EA0: FD 66 03    ld   h,(iy+$03)
1EA3: FD 22 72 08 ld   ($0872),iy            ; this node is new head
1EA7: F9          ld   sp,hl                 ; switch to job's stack
1EA8: C9          ret                        ; resume the job

;
;
; See also: MAN in man.asm within Frenzy's source code.

MAN:
1EA9: CD D4 1F    call $1FD4                 ; call MAN_INIT
1EAC: DD 36 00 16 ld   (ix+$00),$16          ; set WRITE | MOVE | COLOR ($16 = $02|$04|$10)
1EB0: CD 22 1E    call $1E22                 ; call CREATE_JOB
1EB3: C5          push bc
1EB4: FD 21 8B 1E ld   iy,$1E8B              
1EB8: CD 59 1E    call $1E59                 ; call JOB_BOOTSTRAP → enters runner
1EBB: C1          pop  bc
1EBC: DD 2A 76 08 ld   ix,($0876)            ; Now IX = MAN_PTR, a pointer to the player's VECTOR structure. 
1EC0: DD CB 00 7E bit  7,(ix+$00)            ; test HIT bit — has player been killed?
1EC4: C2 A7 1F    jp   nz,$1FA7              ; player's dead, goto PLAYER_DEAD

1EC7: 3A 6E 43    ld   a,($436E)             ; read IS_DEMO_MODE
1ECA: B7          or   a                     ; check if zero. if zero, its a real game
1ECB: 28 14       jr   z,$1EE1               ; its a real game, goto MOVE_PLAYER

; automatic demo mode
; The player's movements during the demo are encoded as an array of DURL Direction bits in ROM.
1ECD: 2A 6F 43    ld   hl,($436F)            ; Load HL with contents of DEMO_PTR
1ED0: 7E          ld   a,(hl)                ; Read DURL Direction bits from HL 
1ED1: 23          inc  hl               
1ED2: 22 6F 43    ld   ($436F),hl            ; Update DEMO_PTR
1ED5: CB 7F       bit  7,a                   ; test bit 7.    
1ED7: 28 16       jr   z,$1EEF               ; if bit 7 not set, A is a DURL bit, move player sprite accordingly      
1ED9: CB BF       res  7,a
1EDB: C5          push bc
1EDC: CD 6D 1E    call $1E6D                 ; call ACTIVATE_HEAD_JOB
1EDF: 18 DA       jr   $1EBB


;
; Expects:
; A = DURL bits read from player controls
; C = Last DURL direction player moved in
;
; Remarks:
; See also S.STICK in control.asm within Frenzy's source code.

MOVE_PLAYER:
1EE1: 3A 79 43    ld   a,($4379)             ; read FLIP
1EE4: B7          or   a                     ; is upright cabinet?
1EE5: 28 04       jr   z,$1EEB               ; yes, goto $1EEB
1EE7: DB 4A       in   a,($4A)               ; read player 2 controls
1EE9: 18 02       jr   $1EED

1EEB: DB 48       in   a,($48)               ; read player 1 controls

; A = bits read from player control port. 
; C = bits previously read from player control port. Used to determine if player changing direction.
1EED: EE 1F       xor  $1F                   
1EEF: CB 67       bit  4,a                   ; test fire button
1EF1: 57          ld   d,a                   ; save direction bits in D
1EF2: 20 0D       jr   nz,$1F01              ; fire button pressed, goto TRY_FIRE
1EF4: E6 0F       and  $0F                   ; mask in left,right,up,down bits and discard rest
1EF6: B9          cp   c                     ; compare to last direction player moved.
1EF7: C4 91 1F    call nz,$1F91              ; if the player has changed direction, call CHANGE_PLAYER_DIRECTION
1EFA: C5          push bc
1EFB: CD 78 1E    call $1E78                 ; call STOP_JOB → yield to runner
1EFE: C3 BB 1E    jp   $1EBB


;
; Fire button has been pressed. Can the player shoot?
;
; Expects:
; D = DURL bits read from P1 or P2 control
; IY = ???
; 
; See also: TRY.F in Frenzy's MAN.ASM source.

TRY_FIRE:
1F01: FD CB 00 4E bit  1,(iy+$00)
1F05: 20 F3       jr   nz,$1EFA
1F07: AF          xor  a

1F08: FD 21 7B 43 ld   iy,$437B              ; load IY with address of PLAYER_BOLT_1
1F0C: FD B6 04    or   (iy+$04)              ; is this bolt in use?
1F0F: 28 0A       jr   z,$1F1B               ; no, we can use it to shoot, goto FIRE
1F11: FD 21 83 43 ld   iy,$4383              ; load IY with address of PLAYER_BOLT_2
1F15: AF          xor  a
1F16: FD B6 04    or   (iy+$04)              ; is this bolt in use?
1F19: 20 DF       jr   nz,$1EFA              ; yes, so goto $1EFA.


; Attempt to fire a bolt.
;
; Expects: 
; D = DURL bits read from P1 or P2 controls
; IX = pointer to player's VECTOR structure
; IY = pointer to BOLT structure to use for player's bolt
;
; See also: FIRE contained within MAN.ASM in Frenzy's source

FIRE:
1F1B: 7A          ld   a,d                   ; get joystick + fire button bits from D
1F1C: E6 0F       and  $0F                   ; preserve DURL direction bits, discard everything else
1F1E: CA FA 1E    jp   z,$1EFA               ; if joystick in "neutral" position, don't shoot

; Fire a bolt
1F21: CD BD 33    call $33BD                 ; play bolt shooting sound
1F24: 4F          ld   c,a                   ; preserve DURL bits in A
1F25: 21 42 20    ld   hl,$2042              ; load HL with address of DIRECTION_OFFSET_TABLE
1F28: 06 00       ld   b,$00
1F2A: 50          ld   d,b
1F2B: 09          add  hl,bc                 ; now HL points to offset held in DIRECTION_OFFSET_TABLE
1F2C: 5E          ld   e,(hl)                ; read offset from DIRECTION_OFFSET_TABLE
1F2D: 21 67 20    ld   hl,$2067              ; load HL with address of SR.TAB
1F30: 19          add  hl,de
1F31: 19          add  hl,de
1F32: 19          add  hl,de                 ; HL = HL + (offset in DE * 3)

; HL now is pointer to an entry in the SR.TAB table.
; First, make the player stand still to shoot (set his velocity in both axes to 0), 
; then select the correct animation for direction player is shooting in
1F33: DD 36 06 00 ld   (ix+$06),$00          ; reset VECTOR.V.X and..
1F37: DD 36 08 00 ld   (ix+$08),$00          ; ... reset VECTOR.V.Y - player must stand still while shooting
1F3B: 7E          ld   a,(hl)
1F3C: 23          inc  hl

1F3D: F3          di
1F3E: DD 77 0A    ld   (ix+$0a),a            ; set VECTOR.D.P.L 
1F41: 7E          ld   a,(hl)
1F42: 23          inc  hl
1F43: DD 77 0B    ld   (ix+$0b),a            ; set VECTOR.D.P.H
1F46: FB          ei
1F47: DD 36 0C 01 ld   (ix+$0c),$01          ; set VECTOR.TIME

; wait a bit before spawning bolt
1F4B: 00          nop
1F4C: DD 7E 0C    ld   a,(ix+$0c)            ; read VECTOR.TIME
1F4F: FE 02       cp   $02
1F51: 20 F8       jr   nz,$1F4B

1F53: 0E FF       ld   c,$FF
1F55: C5          push bc
1F56: 46          ld   b,(hl)                ; get X offset for bolt into B
1F57: 23          inc  hl
1F58: 4E          ld   c,(hl)                ; get Y offset for bolt into C
1F59: 23          inc  hl
1F5A: 56          ld   d,(hl)                ; get DURL bits for bolt into D 
1F5B: DD 7E 07    ld   a,(ix+$07)            ; read player's P.X (X pos)
1F5E: 80          add  a,b                   ; add X offset for bolt   
1F5F: 6F          ld   l,a                   ; L now = start X coord for bolt
1F60: DD 7E 09    ld   a,(ix+$09)            ; read player's P.Y (Y pos)
1F63: 81          add  a,c                   ; add Y offset for bolt
1F64: 67          ld   h,a                   ; H now = start Y coord for bolt
1F65: FD 75 02    ld   (iy+$02),l            ; set BOLT.X
1F68: FD 74 03    ld   (iy+$03),h            ; set BOLT.Y
1F6B: FD 75 06    ld   (iy+$06),l            ; set BOLT.TailX
1F6E: FD 74 07    ld   (iy+$07),h            ; set BOLT.TailY

1F71: F3          di
1F72: FD 72 00    ld   (iy+$00),d            ; set BOLT.Direction
1F75: FD 36 01 00 ld   (iy+$01),$00          ; set BOLT.Length
1F79: FD 72 04    ld   (iy+$04),d            ; set BOLT.LastDirection
1F7C: FD 36 05 08 ld   (iy+$05),$08          ; set BOLT.MaxLength
1F80: FB          ei
1F81: 3E 08       ld   a,$08                 ; delay before man is allowed to shoot again.
1F83: CD 6D 1E    call $1E6D                 ; call ACTIVATE_HEAD_JOB
1F86: FD CB 00 CE set  1,(iy+$00)
1F8A: FD 36 01 0C ld   (iy+$01),$0C
1F8E: C3 BB 1E    jp   $1EBB


;
; Change direction of player
;
; Expects:
; A = DURL bits read from P1 or P2 stick 
; IX = pointer to VECTOR structure
;
; Remarks:
; See also: CHANGE within man.asm in Frenzy's source code.
;

CHANGE_PLAYER_DIRECTION:
1F91: 4F          ld   c,a                   ; set C to "track" stick movement
1F92: E6 0F       and  $0F                   ; mask to DURL direction bits

;
; CDIR — Set the player's animation pattern and velocity for a new
; direction.
; 
; Expects:
; A = DURL direction bits
; IX = pointer to VECTOR structure
;
; Updates the following VECTOR fields:
;    VECTOR.V.X, 
;    VECTOR.V.Y, 
;    VECTOR.D.P.H  
;    VECTOR.D.P.L  
;
; Returns:
; DE = offset read from DIRECTION_OFFSET_TABLE in DE.
;
; See also: CHANGE within man.asm in Frenzy's source code.

CDIR:
1F94: CD 3D 2B    call $2B3D                 ; call SET_VELOCITY. Returns DE = offset read from DIRECTION_OFFSET_TABLE.
1F97: 21 53 20    ld   hl,$2053              ; HL = address of PLAYER_ANIMATION_TABLE
1F9A: 19          add  hl,de                 ; HL = PLAYER_ANIMATION_TABLE + offset
1F9B: 7E          ld   a,(hl)                ; read pattern pointer (lo)
1F9C: 23          inc  hl
1F9D: 66          ld   h,(hl)                ; read pattern pointer (hi)
1F9E: F3          di
1F9F: DD 77 0A    ld   (ix+$0a),a            ; set VECTOR.D.P.L
1FA2: DD 74 0B    ld   (ix+$0b),h            ; set VECTOR.D.P.H
1FA5: FB          ei
1FA6: C9          ret


; The player has been killed.
; 
; Remarks:
; See also DEAD in man.asm within Frenzy's source code. 

PLAYER_DEAD:
1FA7: CD 39 34    call $3439                 ; play sound of player being electrocuted 
1FAA: 3E 10       ld   a,$10                 ; set direction bits 
1FAC: CD 94 1F    call $1F94                 ; call CDIR 
1FAF: DD CB 00 EE set  5,(ix+$00)            ; set INEPT bit (death animation)
1FB3: CD 1F 2C    call $2C1F                 ; call SAY_GOT_THE_HUMANOID_GOT_THE_INTRUDER
1FB6: 3E 2D       ld   a,$2D                 ; set timer for how long the frying lasts!
1FB8: CD 6D 1E    call $1E6D                 ; call ACTIVATE_HEAD_JOB

; 
1FBB: FD 7E 01    ld   a,(iy+$01)
1FBE: B7          or   a
1FBF: 28 05       jr   z,$1FC6
1FC1: CD 78 1E    call $1E78                 ; call STOP_JOB → yield to runner
1FC4: 18 F5       jr   $1FBB

1FC6: 2A 76 08    ld   hl,($0876)            ; load HL with contents of MAN_PTR. Now HL = pointer to player's VECTOR
1FC9: 36 09       ld   (hl),$09              ; set ERASE | BLANK ($09 = $01|$08)
1FCB: FD CB 00 86 res  0,(iy+$00)
1FCF: CD 78 1E    call $1E78                 ; call STOP_JOB → yield to runner
1FD2: 18 FB       jr   $1FCF



;
;
;
; See also: M.INIT in man.asm within Frenzy's source code.
;

MAN_INIT:
1FD4: FD E1       pop  iy                    ; pop return address off stack into IY

; Allocate and zero 14 bytes on the stack for player's VECTOR 
1FD6: 21 00 00    ld   hl,$0000
1FD9: 06 07       ld   b,$07
1FDB: E5          push hl
1FDC: 10 FD       djnz $1FDB

1FDE: DD 21 00 00 ld   ix,$0000
1FE2: DD 39       add  ix,sp                 ; IX = SP
1FE4: E5          push hl
1FE5: DD 22 76 08 ld   ($0876),ix            ; set MAN_PTR

; Set player colour
1FE9: 3A 44 43    ld   a,($4344)             ; read CURRENT_PLAYER
1FEC: FE 01       cp   $01                   ; Is it player one's turn?   
1FEE: 3E AA       ld   a,$AA                 ; Colour for player 1
1FF0: 28 02       jr   z,$1FF4               ; It's player one's turn, goto $1FF4

1FF2: 3E DD       ld   a,$DD                 ; Colour for player 2

1FF4: 32 78 43    ld   ($4378),a             ; set PLAYER_COLOUR

1FF7: 2A 47 43    ld   hl,($4347)            ; get MAN_X into L and MAN_Y into H
1FFA: DD 75 07    ld   (ix+$07),l            ; set VECTOR.P.X (X pos)
1FFD: DD 74 09    ld   (ix+$09),h            ; set VECTOR.P.Y (Y pos)
2000: AF          xor  a
2001: CD 91 1F    call $1F91                 ; call CHANGE_PLAYER_DIRECTION
2004: DD 36 0D 02 ld   (ix+$0d),$02          ; set VECTOR.TPRIME
2008: DD 36 0C 01 ld   (ix+$0c),$01          ; set VECTOR.TIME
200C: FD E9       jp   (iy)                  ; jump to return address popped @ $1fd4


;
; Allocate space on the stack for a robot's VECTOR 
;

200E: E1          pop  hl                    ; pop return address off stack
200F: D9          exx

; Allocate 14 bytes for robot's VECTOR 
2010: 06 07       ld   b,$07
2012: 11 00 00    ld   de,$0000
2015: D5          push de
2016: 10 FD       djnz $2015

2018: 21 00 00    ld   hl,$0000
201B: 39          add  hl,sp                 ; HL = SP
201C: EB          ex   de,hl
201D: 3A 71 08    ld   a,($0871)
2020: B7          or   a
2021: 20 07       jr   nz,$202A
2023: D5          push de
2024: ED 53 70 08 ld   ($0870),de
2028: 18 13       jr   $203D


202A: DD 2A 70 08 ld   ix,($0870)
202E: DD 66 FF    ld   h,(ix-$01)
2031: DD 6E FE    ld   l,(ix-$02)
2034: E5          push hl
2035: F3          di
2036: DD 72 FF    ld   (ix-$01),d
2039: DD 73 FE    ld   (ix-$02),e
203C: FB          ei

203D: D5          push de
203E: DD E1       pop  ix                    ; IX = DE
2040: D9          exx
2041: E9          jp   (hl)


;
; Maps Direction (DURL) bits to offsets into other tables. 
;
; The offsets are sometimes multiplied by another number before being used.
;
; For an example, let's look at how this table is used to calculate an offset into the robot shoot table S.TAB .
; Let's say we have DURL bits of 5, which is LEFT (1) + UP (4).
; 
; The 5th entry (with a zero based index) of the DIRECTION_OFFSET_TABLE table is $0E (14 decimal).
; The logic at $28EC multiplies this value by 3 to give 42 decimal. 
; 42 bytes into S.TAB @ $296E you have the entry for shooting UP + LEFT. 
;
; Remarks:
; See also D.TAB within man.asm in Frenzy's source code.

DIRECTION_OFFSET_TABLE:
2042: 
    00                                       ; no move   
    0C                                       ; left
    04                                       ; right
    00        
    10                                       ; up
    0E                                       ; up,left 
    02                                       ; up,right
    10                                       ; up default
    08                                       ; down
    0A                                       ; down,left
    06                                       ; down,right
    08                                       ; down default
    00            
    0C                                       ; left default
    04                                       ; right default
    00        
    12                                       ; explode


;
; PLAYER_ANIMATION_TABLE — maps each direction to the player's animation
; table. There are 10 entries, one per possible offset from
; DIRECTION_OFFSET_TABLE.
;
; How the lookup works:
;
;   1. DURL bits (left/right/up/down) form a direction index from 0 to 16.
;   2. DIRECTION_OFFSET_TABLE[direction index] returns a byte offset.
;   3. That offset is used directly here: read the 2-byte pointer at
;      PLAYER_ANIMATION_TABLE + offset.
;   4. The pointer is the address of an animation table — a list of
;      sprite-pattern pointers ending with a zero byte then a self-loop.
;
; Example — player moving LEFT (direction index = 1):
;
;   DIRECTION_OFFSET_TABLE[1] = $0C (12 decimal)
;   PLAYER_ANIMATION_TABLE + $0C points to two bytes: $56, $10, which form address $1056.
;   $1056 is the "moving left" animation table (a 4-frame cycle).
;
; Note that offsets $02 through $10 (moving up/right/down) all share
; the same animation table at $104B, and offsets $0A through $0E
; (moving left) all share $1056. This is because the player sprite
; only has two directional movement animations — "up" and "left".
;
; Remarks:
; See also P.TAB within man.asm in Frenzy's source code.

PLAYER_ANIMATION_TABLE:
2053: 
    46 10                                    ; $1046 = stand still
    4B 10                                    ; $104B = move up-right
    4B 10                                    ; $104B = move right
    4B 10                                    ; $104B = move down-right
    4B 10                                    ; $104B = move down
    56 10                                    ; $1056 = move down-left
    56 10                                    ; $1056 = move left
    56 10                                    ; $1056 = move up-left
    4B 10                                    ; $104B = move up
    B3 12                                    ; $12B3 = death / electrocution


;
; Player shoot table.
;
; Defines what animation the player's VECTOR should use when shooting, and deltas to add to the VECTOR's X,Y coordinates
; to compute starting X,Y coordinates for a BOLT. 
;
; Each entry in this table requires 6 bytes.
;
; The format of an entry in the table is as follows:
;      WORD PatternPtr - pointer to a pattern table (animation table) for player sprite
;      BYTE XDelta - Y Delta to add to player's X coordinate to compute bolt's initial X coordinate 
;      BYTE YDelta - Y Delta to add to player's Y coordinate to compute bolt's initial Y coordinate
;      BYTE Direction - DURL bits representing the direction the bolt will move in DURL bits
;      BYTE 0 - padding byte with no other use.
;
; Remarks:
; See also: SR.TAB within man.asm in Frenzy's source code.
; 

SR.TAB:
; No direction
2067: 
    46 10                                    ; pointer to pattern table        
    00                                       ; Bolt starting position X Delta (0 as can't shoot if joystick not pointing in a direction)
    00          
    00                                       ; DURL bits (0 = No direction)
    00          

; Player shooting UP + RIGHT
206D: 
    61 10                                    ; pointer to pattern table 
    07                                       ; Bolt X delta 
    01                                       ; Bolt Y delta 
    06                                       ; DURL bits (RIGHT + UP)
    00 

; Player shooting RIGHT
2073: 
    66 10                                    ; pointer to pattern table 
    07                                       ; Bolt X delta 
    03                                       ; Bolt Y delta 
    02                                       ; DURL bits (RIGHT)
    00

; Player shooting DOWN + RIGHT   
2079: 
    6B 10                                    ; pointer to pattern table
    06                                       ; Bolt X delta 
    06                                       ; Bolt Y delta  
    0A                                       ; DURL bits (RIGHT + DOWN)
    00          

; Player shooting DOWN
207F:    
    70 10       
    06          
    07          
    08          
    00

; Player shooting DOWN + LEFT
2085: 
    75 10       
    00          
    06 
    09          
    00

; Player shooting LEFT
208B: 
    7A 10         
    00           
    03          
    01          
    00 
    
; Player shooting UP + LEFT
2091: 
    7F 10        
    00          
    00          
    05          
    00

; Player shooting UP
2097:          
    84 10       
    07          
    02          
    04          
    00          

;
; Initialise a new game room. Clears screen, draws maze walls,
; creates player and robot VECTORs, sets up timers and sound.
;

209D: CD 4E 1A    call $1A4E                 ; call CLEAR_SCREEN
20A0: CD 40 25    call $2540
20A3: 3A 6E 43    ld   a,($436E)             ; read IS_DEMO_MODE
20A6: B7          or   a
20A7: 20 2E       jr   nz,$20D7              ; if demo mode, goto $20D7
20A9: CD D4 1F    call $1FD4
20AC: 01 12 18    ld   bc,$1812
20AF: 3A 76 43    ld   a,($4376)             ; read NUMBER_OF_PLAYERS
20B2: FE 02       cp   $02
20B4: 28 02       jr   z,$20B8
20B6: 06 08       ld   b,$08
20B8: DD 71 00    ld   (ix+$00),c
20BB: C5          push bc
20BC: DD E5       push ix
20BE: 3E 0A       ld   a,$0A
20C0: CD 6D 1E    call $1E6D                 ; call ACTIVATE_HEAD_JOB
20C3: DD E1       pop  ix
20C5: C1          pop  bc
20C6: 3E 1B       ld   a,$1B
20C8: A9          xor  c
20C9: 4F          ld   c,a
20CA: 10 EC       djnz $20B8
20CC: 21 00 00    ld   hl,$0000
20CF: 22 76 08    ld   ($0876),hl            ; set MAN_PTR to NULL       
20D2: 06 08       ld   b,$08
20D4: D1          pop  de
20D5: 10 FD       djnz $20D4
20D7: FB          ei
20D8: 3A 4A 43    ld   a,($434A)
20DB: C6 60       add  a,$60
20DD: 27          daa
20DE: 32 4A 43    ld   ($434A),a
20E1: 3A 4C 43    ld   a,($434C)
20E4: FE 01       cp   $01
20E6: 28 04       jr   z,$20EC
20E8: 3D          dec  a
20E9: 32 4C 43    ld   ($434C),a
20EC: AF          xor  a
20ED: 32 71 43    ld   ($4371),a             ; set RCOUNT 
20F0: FD 2A 72 08 ld   iy,($0872)
20F4: FD 77 01    ld   (iy+$01),a
20F7: 3A 4D 43    ld   a,($434D)             ; read RWAIT   
20FA: FE 14       cp   $14
20FC: 38 05       jr   c,$2103
20FE: D6 0A       sub  $0A
2100: 32 4D 43    ld   ($434D),a             ; set RWAIT
2103: FD 21 17 21 ld   iy,$2117
2107: 21 F0 FF    ld   hl,$FFF0
210A: 39          add  hl,sp
210B: F9          ld   sp,hl
210C: CD 59 1E    call $1E59                 ; call JOB_BOOTSTRAP → enters runner
210F: 21 10 00    ld   hl,$0010
2112: 39          add  hl,sp
2113: F9          ld   sp,hl
2114: C3 57 21    jp   $2157
2117: 3E 16       ld   a,$16
2119: 32 00 43    ld   ($4300),a
211C: 4F          ld   c,a
211D: 3A 4A 43    ld   a,($434A)
2120: 47          ld   b,a
2121: CD 78 26    call $2678                 ; call RANDOM
2124: B8          cp   b
2125: 38 1E       jr   c,$2145
2127: 21 A0 23    ld   hl,$23A0
212A: 06 00       ld   b,$00
212C: 09          add  hl,bc
212D: 46          ld   b,(hl)
212E: 23          inc  hl
212F: 4E          ld   c,(hl)
2130: CD 78 26    call $2678                 ; call RANDOM
2133: E6 1F       and  $1F
2135: 80          add  a,b
2136: 47          ld   b,a
2137: CD 78 26    call $2678                 ; call RANDOM
213A: E6 1F       and  $1F
213C: 81          add  a,c
213D: 4F          ld   c,a
213E: FD 21 45 21 ld   iy,$2145
2142: C3 B8 23    jp   $23B8                 ; jump to ROBOT

2145: 3A 00 43    ld   a,($4300)
2148: 3D          dec  a
2149: 3D          dec  a
214A: 32 00 43    ld   ($4300),a
214D: 4F          ld   c,a
214E: 20 CD       jr   nz,$211D
2150: FD 21 A9 1E ld   iy,$1EA9
2154: C3 8E 2A    jp   $2A8E

2157: DD 2A 76 08 ld   ix,($0876)            ; read MAN_PTR
215B: DD CB 00 56 bit  2,(ix+$00)
215F: C8          ret  z
2160: DD 7E 09    ld   a,(ix+$09)
2163: 32 48 43    ld   ($4348),a
2166: 47          ld   b,a
2167: DD 7E 07    ld   a,(ix+$07)
216A: 32 47 43    ld   ($4347),a             ; set MAN_X
216D: DD CB 00 7E bit  7,(ix+$00)
2171: C2 8D 21    jp   nz,$218D
2174: B7          or   a
2175: F2 82 21    jp   p,$2182
2178: FE FC       cp   $FC
217A: D2 AC 22    jp   nc,$22AC
217D: FE F6       cp   $F6
217F: D2 5D 22    jp   nc,$225D
2182: 78          ld   a,b
2183: FE 02       cp   $02
2185: DA 20 22    jp   c,$2220
2188: FE BE       cp   $BE
218A: D2 CF 21    jp   nc,$21CF
218D: 3A 6D 43    ld   a,($436D)             ; read UPDATE flag
2190: B7          or   a                     ; has score changed?
2191: 28 03       jr   z,$2196               ; if not, goto $2196
2193: CD 14 23    call $2314                 ; call SHOW_SCORE

2196: 3A 6E 43    ld   a,($436E)             ; read IS_DEMO_MODE
2199: B7          or   a
219A: 28 07       jr   z,$21A3
219C: CD 7B 19    call $197B
219F: CD 97 19    call $1997
21A2: C0          ret  nz
21A3: FD 2A 72 08 ld   iy,($0872)            ; Load IY with contents of TIMER_LIST_PTR
21A7: FD CB 00 CE set  1,(iy+$00)
21AB: FD 7E 01    ld   a,(iy+$01)
21AE: B7          or   a
21AF: 20 18       jr   nz,$21C9
21B1: FD 36 01 3B ld   (iy+$01),$3B
21B5: 06 0C       ld   b,$0C
21B7: 21 D0 08    ld   hl,$08D0              ; load HL with address of CMOS_TOTAL_SECS_OF_PLAY
21BA: 3A 6E 43    ld   a,($436E)             ; read IS_DEMO_MODE
21BD: B7          or   a
21BE: 20 06       jr   nz,$21C6
21C0: CD B3 2D    call $2DB3                 ; call INCREMENT_BY_1
21C3: CD 97 2B    call $2B97
21C6: CD 78 26    call $2678                 ; call RANDOM
21C9: CD 78 1E    call $1E78                 ; call STOP_JOB → yield to runner
21CC: C3 57 21    jp   $2157

21CF: 3E 06       ld   a,$06
21D1: 32 48 43    ld   ($4348),a
21D4: 21 46 43    ld   hl,$4346
21D7: 34          inc  (hl)
21D8: CD EB 22    call $22EB
21DB: 28 09       jr   z,$21E6
21DD: 21 AD 5F    ld   hl,$5FAD
21E0: 11 DF 5F    ld   de,$5FDF
21E3: C3 3D 22    jp   $223D


;
; Scroll room up
;
; Remarks:
; See also: function S.U in Frenzy's PLAY.ASM 
;

SCROLL_UP:
21E6: 21 2D 44    ld   hl,$442D
21E9: 11 00 44    ld   de,$4400              ; load HL with address of screen image RAM  

S.U:
21EC: E5          push hl
21ED: 3E 1B       ld   a,$1B
21EF: 21 00 01    ld   hl,$0100
21F2: 19          add  hl,de
21F3: 01 00 19    ld   bc,$1900
21F6: D5          push de
21F7: ED B0       ldir
; remove junk left after scroll
21F9: D1          pop  de
21FA: 01 00 01    ld   bc,$0100
21FD: 2B          dec  hl
21FE: 36 00       ld   (hl),$00
2200: 0D          dec  c
2201: C2 FD 21    jp   nz,$21FD
2204: 10 F7       djnz $21FD
2206: 3D          dec  a
2207: 20 E6       jr   nz,$21EF

; TODO: ID what this is doing
2209: CD 40 25    call $2540
220C: E1          pop  hl

220D: 11 1A 00    ld   de,$001A
2210: 0E 02       ld   c,$02
2212: 06 06       ld   b,$06
2214: 36 FF       ld   (hl),$FF
2216: 23          inc  hl
2217: 10 FB       djnz $2214
2219: 19          add  hl,de
221A: 0D          dec  c
221B: 20 F5       jr   nz,$2212
221D: C3 D7 20    jp   $20D7




2220: 3E B9       ld   a,$B9
2222: 32 48 43    ld   ($4348),a             ; set MAN_Y
2225: 21 46 43    ld   hl,$4346              ; load HL with address of ROOM_Y
2228: 35          dec  (hl)                  ; decrement Y coordinate within maze
2229: CD EB 22    call $22EB
; zero flag set if upright cabinet
222C: 28 09       jr   z,$2237               ; if an upright cabinet, go to SCROLL_DOWN

; cocktail cabinet
222E: 21 2D 46    ld   hl,$462D
2231: 11 00 46    ld   de,$4600
2234: C3 EC 21    jp   $21EC                 ; jump to S.U to scroll up


;
; Scroll room down
;
; Remarks:
; See also: function S.D in Frenzy's PLAY.ASM 
;

SCROLL_DOWN:
2237: 21 AD 5D    ld   hl,$5DAD
223A: 11 FF 5D    ld   de,$5DFF
223D: E5          push hl

S.D:
223E: 3E 1A       ld   a,$1A
2240: 01 00 19    ld   bc,$1900
2243: 21 00 FF    ld   hl,$FF00
2246: 19          add  hl,de
2247: D5          push de
2248: ED B8       lddr                       ; execute scroll
; remove junk left after scroll
224A: D1          pop  de
224B: 01 00 01    ld   bc,$0100
224E: 23          inc  hl
224F: 36 00       ld   (hl),$00
2251: 0D          dec  c
2252: C2 4E 22    jp   nz,$224E
2255: 10 F7       djnz $224E
2257: 3D          dec  a
2258: 20 E6       jr   nz,$2240
225A: C3 09 22    jp   $2209



225D: 3E 08       ld   a,$08
225F: 32 47 43    ld   ($4347),a             ; set MAN_X
2262: 21 45 43    ld   hl,$4345              ; load HL with address of ROOM_X
2265: 34          inc  (hl)                  ; increment X coordinate within maze
2266: CD EB 22    call $22EB
; zero flag set if upright cabinet
2269: 28 09       jr   z,$2274               ; if upright cabinet goto SCROLL_LEFT

; cocktail cabinet
226B: 21 1F 4F    ld   hl,$4F1F
226E: 11 E0 5F    ld   de,$5FE0
2271: C3 C9 22    jp   $22C9                 ; jump to S.R to scroll right


;
; Scroll room left
;
; Remarks:
; See also: S.L in PLAY.ASM within Frenzy's source code. 
;

SCROLL_LEFT:
2274: 21 00 4D    ld   hl,$4D00
2277: 11 00 44    ld   de,$4400              ; load DE with address of screen image RAM   

S.L:
227A: E5          push hl
227B: 3E 20       ld   a,$20
227D: 01 FF 19    ld   bc,$19FF
2280: 21 01 00    ld   hl,$0001
2283: 19          add  hl,de
2284: D5          push de
2285: ED B0       ldir                       ; execute scroll
; remove junk left after scroll
2287: 06 D0       ld   b,$D0
2289: 11 E1 FF    ld   de,$FFE1
228C: 36 00       ld   (hl),$00
228E: 2B          dec  hl
228F: 36 00       ld   (hl),$00
2291: 19          add  hl,de
2292: 10 F8       djnz $228C
2294: D1          pop  de
2295: 3D          dec  a
2296: 20 E5       jr   nz,$227D

2298: 3E 06       ld   a,$06
229A: F5          push af
229B: CD 40 25    call $2540
229E: F1          pop  af
229F: E1          pop  hl
22A0: 11 20 00    ld   de,$0020
22A3: 06 40       ld   b,$40
22A5: 77          ld   (hl),a
22A6: 19          add  hl,de
22A7: 10 FC       djnz $22A5
22A9: C3 D7 20    jp   $20D7


;
; Player has exited left side of room
;
; Remarks:
; See also: OLEFT in PLAY.ASM within Frenzy's source code.
;

22AC: 3E E6       ld   a,$E6
22AE: 32 47 43    ld   ($4347),a             ; set MAN_X
22B1: 21 45 43    ld   hl,$4345              ; load HL with address of ROOM_X
22B4: 35          dec  (hl)                  ; decrement X coordinate within maze
22B5: CD EB 22    call $22EB
; zero flag set if upright cabinet
22B8: 28 09       jr   z,$22C3               ; jump to SCROLL_RIGHT if upright cabinet

; cocktail cabinet - need to scroll screen left 
22BA: 21 00 4F    ld   hl,$4F00
22BD: 11 00 46    ld   de,$4600
22C0: C3 7A 22    jp   $227A                 ; jump to S.L to scroll left

;
; Scroll room right 
;
; Remarks:
; See also: function S.R in PLAY.ASM within Frenzy's source code
;

SCROLL_RIGHT:
22C3: 21 1F 4D    ld   hl,$4D1F
22C6: 11 00 5E    ld   de,$5E00

S.R:
22C9: E5          push hl
22CA: 3E 20       ld   a,$20
22CC: 01 00 1A    ld   bc,$1A00
22CF: 21 FF FF    ld   hl,$FFFF
22D2: 19          add  hl,de
22D3: D5          push de
22D4: ED B8       lddr                       ; execute scroll
; remove any garbage left after scroll
22D6: 11 1F 00    ld   de,$001F
22D9: 36 00       ld   (hl),$00
22DB: 23          inc  hl
22DC: 36 00       ld   (hl),$00
22DE: 19          add  hl,de
22DF: 10 F8       djnz $22D9
22E1: D1          pop  de
22E2: 3D          dec  a
22E3: 20 E7       jr   nz,$22CC

22E5: 3E 60       ld   a,$60
22E7: F5          push af
22E8: C3 9B 22    jp   $229B


;
; TODO: Check if this is equivalent of TREST within Frenzy's PLAY.ASM
;
; Returns:
; Zero flag clear if screen should flip  

22EB: CD E4 2B    call $2BE4                 ; call TRY_SPEAK_ON_PLAYER_LEAVING_ROOM
22EE: CD 4E 36    call $364E

;
; Clear game state between rooms/exits.
; Clears V.PTR, MAN_PTR, player bolts data,
; and checks FLIP status.
;
; Returns: Zero flag set if upright cabinet

22F1: FD E5       push iy
22F3: E1          pop  hl
22F4: FD 74 FF    ld   (iy-$01),h
22F7: FD 75 FE    ld   (iy-$02),l
22FA: 21 00 00    ld   hl,$0000
22FD: F3          di
22FE: 22 70 08    ld   ($0870),hl            ; set V.PTR to NULL   
2301: 22 76 08    ld   ($0876),hl            ; set MAN_PTR to NULL
2304: AF          xor  a
2305: 21 7B 43    ld   hl,$437B              ; load HL with address of PLAYER_BOLTS   
2308: 06 38       ld   b,$38
230A: 77          ld   (hl),a
230B: 23          inc  hl
230C: 10 FC       djnz $230A
230E: 3A 79 43    ld   a,($4379)             ; read FLIP
2311: B7          or   a
2312: FB          ei
2313: C9          ret


;
; Displays player one's score, and player two's if two player game.
;
; Remarks:
; See also: SHOWS in play.asm within Frenzy's source code.
;

SHOW_SCORE:
2314: AF          xor  a
2315: 32 6D 43    ld   ($436D),a             ; clear UPDATE flag
; print player 1's score
2318: 11 00 D5    ld   de,$D500
231B: 21 3E 43    ld   hl,$433E              ; load HL with address of P1_SCORE
231E: 06 06       ld   b,$06                 ; max number of digits in score
2320: CD 40 2A    call $2A40                 ; call PRINT_DIGITS

; is this a 2 player game?
2323: 3A 76 43    ld   a,($4376)             ; read NUMBER_OF_PLAYERS
2326: FE 02       cp   $02                   ; 2 players?
2328: C0          ret  nz                    ; exit if not a 2 player game

; print player 2's score
2329: 11 B0 D5    ld   de,$D5B0
232C: 21 41 43    ld   hl,$4341              ; load HL with address of P2_SCORE              
232F: 06 06       ld   b,$06                 ; max number of digits in score 
2331: C3 40 2A    jp   $2A40                 ; jump to PRINT_DIGITS

;
; Get a pointer to the current player's score
;
; Frenzy also has this routine; it is named "ScorePtr" in PLAY.ASM.
;

GET_PLAYER_SCORE_PTR:
2334: 3A 44 43    ld   a,($4344)             ; read CURRENT_PLAYER
2337: FE 02       cp   $02                   ; is player 2 playing?   
2339: 21 41 43    ld   hl,$4341              ; load HL with address of P2_SCORE
233C: C8          ret  z                     ; exit if player 2 playing
233D: 21 3E 43    ld   hl,$433E              ; otherwise, load HL with address of P1_SCORE
2340: C9          ret


;
; Update player's score.  Pretty much same as ADDS within Play.ASM in Frenzy's source code. 
;
; Expects:
; B = index of digit to update (where 0 = units, 1 = tens, 2=hundreds, 3=thousands and so on.)
; C = value to add to digit
;
; e.g. to add a score of 100 you would pass B = 2, C = 1. To add 200, you'd pass B = 2, C= 2.  
;
; (Note: Frenzy's docs say "ADD C * 10 ^ B TO SCORE", but I think my explanation is more easily understood)
;

UPDATE_SCORE:
2341: 3E FF       ld   a,$FF
2343: 32 6D 43    ld   ($436D),a             ; set UPDATE flag - player score needs to be redrawn ASAP.
2346: 1E 04       ld   e,$04
2348: CD 34 23    call $2334                 ; call GET_PLAYER_SCORE_PTR
234B: 23          inc  hl
234C: 23          inc  hl
234D: 23          inc  hl                    ; bump HL to point to memory address just after last 2 digits of players score
234E: CB 38       srl  b                     ; Divide By 2. If the index in B is an odd number, then carry will be set 
2350: 08          ex   af,af'                ; switch to alternate register pair to preserve flags
2351: 04          inc  b                     ; ensure B is at least 1 
2352: 2B          dec  hl
2353: 1D          dec  e
2354: 10 FC       djnz $2352

2356: 08          ex   af,af'                ; restore flags
2357: 30 08       jr   nc,$2361              ; if carry is not set, then B on entry is even , goto $2361

; B is an odd number. In this situation we need to add to the upper nibble of the score BCD byte, so we need to change our value 
; parameter so that it effects the upper nibble. 
2359: CB 21       sla  c                     ; move lower nibble..
235B: CB 21       sla  c
235D: CB 21       sla  c
235F: CB 21       sla  c                     ; to upper nibble

; Now HL = pointer to BCD byte to update, C= BCD value to add to byte pointed to by HL 
2361: 79          ld   a,c                   ; get value into A
2362: 86          add  a,(hl)                ; Add to BCD digits in existing score
2363: 27          daa                        ; Ensure result is valid BCD, and set carry flag if add caused a carry
2364: 77          ld   (hl),a                ; and update BCD digits in existing score
2365: 30 08       jr   nc,$236F              ; if no carry occurred, we're done 
2367: 2B          dec  hl                    ; otherwise, we need to carry over result to previous digit (going from 10s to hundreds to thousands etc if necessary.) 
2368: 1D          dec  e
2369: 28 04       jr   z,$236F

236B: 0E 01       ld   c,$01
236D: 18 F2       jr   $2361

; Now check if we get a bonus life
236F: CD 34 23    call $2334                 ; call GET_PLAYER_SCORE_PTR
2372: 44          ld   b,h
2373: 4D          ld   c,l                   ; BC = pointer to player score
2374: 21 4F 43    ld   hl,$434F              ; load HL with address of XTRAMEN flag
2377: 11 49 43    ld   de,$4349              ; load DE with address of DEATHS
237A: DB 61       in   a,($61)               ; read from F2
237C: CB 7F       bit  7,a                   ; test for BONUS LIFE AT 10000 DIP settings
237E: 28 11       jr   z,$2391               ; if not set, goto $2391

; handle BONUS LIFE AT 10000 POINTS 
2380: 0A          ld   a,(bc)                ; read first two digits of score (the hundred thousands & ten thousands) 
2381: B7          or   a                     ; test if zero
2382: 28 0D       jr   z,$2391               ; if zero then score isn't at least 10000, goto $2391
2384: CB 4E       bit  1,(hl)                ; test if XTRAMEN flag has been set
2386: C0          ret  nz                    ; exit if bonus life already awarded to player
2387: CB CE       set  1,(hl)                ; set XTRAMEN flag. No more bonus lives for you!
2389: EB          ex   de,hl                 ; Now HL = address of DEATHS
238A: 34          inc  (hl)                  ; award an extra life
238B: CD 38 35    call $3538                 
238E: C3 9A 25    jp   $259A                 ; refresh display to show extra life

2391: DB 61       in   a,($61)               ; read from F2
2393: CB 77       bit  6,a                   ; test for BONUS LIFE AT 5000 DIP setting
2395: C8          ret  z                     ; exit if the player doesn't get a bonus life at 5000 points

; handle BONUS LIFE AT 5000 POINTS
2396: 03          inc  bc                    ; bump BC to point to the thousands and hundreds digits of the score 
2397: 0A          ld   a,(bc)           
2398: FE 50       cp   $50                   ; is the thousands digit "5" ? 
239A: D8          ret  c                     ; exit if lower
239B: CB 46       bit  0,(hl)                ; test if XTRAMEN flag has been set
239D: C0          ret  nz                    ; exit if bonus life already awarded to player
239E: CB C6       set  0,(hl)                ; set XTRAMEN flag   
23A0: 18 E7       jr   $2389                 ; refresh display to show extra life



; TODO: There appears to be no references to this in the code. What is it?
23A2: 0C          inc  c
23A3: 0C          inc  c
23A4: 40          ld   b,b
23A5: 0C          inc  c
23A6: A0          and  b
23A7: 0C          inc  c
23A8: CE 0C       adc  a,$0C
23AA: 40          ld   b,b
23AB: 50          ld   d,b
23AC: 70          ld   (hl),b
23AD: 50          ld   d,b
23AE: 9E          sbc  a,(hl)
23AF: 50          ld   d,b
23B0: 0C          inc  c
23B1: 96          sub  (hl)
23B2: 40          ld   b,b
23B3: 96          sub  (hl)
23B4: A0          and  b
23B5: 96          sub  (hl)
23B6: CE 96       adc  a,$96


;
; Creates a robot.
;
; See also: ROBOT within robot.asm in Frenzy's source code.
;

ROBOT:
23B8: C5          push bc
23B9: D1          pop  de                    ; DE = BC
23BA: 21 71 43    ld   hl,$4371              ; load HL with address of RCOUNT
23BD: 34          inc  (hl)                  ; increment RCOUNT to count number of robots in room  
23BE: 7E          ld   a,(hl)                ; copy RCOUNT ..
23BF: 32 72 43    ld   ($4372),a             ; .. to RSAVED so we remember how many robots room had for room clearance bonus

; Get a VECTOR for the robot, then initialise it
23C2: CD 0E 20    call $200E                 ; Allocate VECTOR for robot and store address in IX
23C5: DD 72 07    ld   (ix+$07),d            ; set VECTOR.P.X (X pos)
23C8: DD 73 09    ld   (ix+$09),e            ; set VECTOR.P.Y (Y pos)
23CB: AF          xor  a
23CC: CD 36 24    call $2436                 ; call SETPAT 
23CF: DD 36 0C 01 ld   (ix+$0c),$01          ; set VECTOR.TIME
23D3: DD 36 00 06 ld   (ix+$00),$06          ; set WRITE | MOVE ($06 = $02|$04)
23D7: CD 22 1E    call $1E22                 ; call CREATE_JOB

; 
23DA: DD E5       push ix
23DC: CD 59 1E    call $1E59                 ; call JOB_BOOTSTRAP → enters runner
23DF: 3A 4D 43    ld   a,($434D)             ; read RWAIT
23E2: FE 1E       cp   $1E                   ; compare to minimum wait time (MinWait in Frenzy's code)
23E4: 30 02       jr   nc,$23E8              ; if A>= RWAIT, goto $23E8
23E6: 3E 1E       ld   a,$1E                 ; clamp A to 30 decimal minimum

; A = now delay before robots can start shooting. Activate the robot job
23E8: CD 6D 1E    call $1E6D                 ; call ACTIVATE_HEAD_JOB
23EB: DD E1       pop  ix

23ED: 0E 00       ld   c,$00

; See also: SEEK within robot.asm in Frenzy's source code.
SEEK:
23EF: FD 2A 76 08 ld   iy,($0876)            ; read contents of MAN_PTR
23F3: C5          push bc

; Delta X = player X coordinate - robot X coordinate
23F4: FD 7E 07    ld   a,(iy+$07)            ; read P.X  of player VECTOR
23F7: DD 96 07    sub  (ix+$07)              ; subtract P.X  of robot VECTOR 
23FA: 57          ld   d,a                   ; D = Delta X    
23FB: 06 00       ld   b,$00

; if delta X == 0, that means the robot's X coordinate and player's X coordinate are the same.
23FD: 28 06       jr   z,$2405               ; if player X == Robot X, goto $2405

; if the subtraction caused a carry that means the player is to the left of the robot.
23FF: 06 01       ld   b,$01                 ; DURL bit for LEFT
2401: 38 02       jr   c,$2405

; player is to the right of the robot.
2403: 06 02       ld   b,$02                 ; DURL bit for RIGHT   

; B now holds the horizontal DURL bits indicating if player is to the left or right.  
; Delta Y = player Y coordinate - robot Y coordinate
2405: FD 7E 09    ld   a,(iy+$09)            ; read P.Y (Y Pos) of player VECTOR
2408: C6 02       add  a,$02                 ; player is 2 pixels taller than robot, so compensate
240A: DD 96 09    sub  (ix+$09)              ; subtract P.Y (Y Pos) of robot VECTOR 
240D: 5F          ld   e,a                   ; E = Delta y

; D and E hold Delta X and Delta Y (the horizontal and vertical distances of the player from the robot).
240E: 0E 00       ld   c,$00

; if delta Y == 0, that means the robot's Y coordinate and player's Y coordinates are the same.
2410: 28 06       jr   z,$2418               ; if Player Y == Robot Y, goto $2418 
2412: 0E 04       ld   c,$04                 ; DURL bit for UP
2414: 38 02       jr   c,$2418
2416: 0E 08       ld   c,$08                 ; DURL bit for DOWN

2418: 78          ld   a,b                   ; get horizontal DURL bits in B 
2419: 81          add  a,c                   ; and combine with vertical DURL bits in C
241A: F5          push af                    ; save DURL bits on stack
241B: 4F          ld   c,a                   ; load C with DURL bits as SHOOT routine needs it

; Robot shoots at player!
241C: CD 7F 28    call $287F                 ; call SHOOT 
241F: F1          pop  af
2420: C1          pop  bc
2421: CD 36 24    call $2436                 ; call SETPAT 
2424: DD E5       push ix
2426: C5          push bc
2427: CD 78 1E    call $1E78                 ; call STOP_JOB → yield to runner
242A: C1          pop  bc
242B: DD E1       pop  ix
242D: DD CB 00 7E bit  7,(ix+$00)            ; test HIT bit
2431: CA EF 23    jp   z,$23EF               ; if bits not set, robot is still functional, goto $23EF
2434: 18 21       jr   $2457                 ; Robot's been hit! Goto BLAM!


; Set the sprite pattern table pointer for a robot's VECTOR.
;
; Expects:
; A = direction (DURL bits) of robot
; C = last direction (DURL bits) of robot
; IX = pointer to VECTOR structure for robot.
;
; Remarks:
; See also: SETPAT in robot.asm within Frenzy's source code.

SETPAT:
2436: E6 0F       and  $0F                   ; read direction of robot
2438: C4 6E 1C    call nz,$1C6E              ; if not standing still, call IQ for the robot to choose a direction. A now = allowed direction of robot

243B: B9          cp   c                     ; has the robot changed direction?                    
243C: C8          ret  z                     ; no, so exit
243D: 4F          ld   c,a
243E: CD 3D 2B    call $2B3D                 ; call SET_VELOCITY. Now DE = offset read from DIRECTION_OFFSET_TABLE.

; Set pointer to pattern table for robot on room start
2441: 21 2D 25    ld   hl,$252D              ; load HL with address of ROBOT_ANIMATION_TABLES
2444: 19          add  hl,de
2445: 7E          ld   a,(hl)                ; A = low byte of pointer to pattern table   
2446: 23          inc  hl
2447: 66          ld   h,(hl)                ; H = high byte of pointer to pattern table.
2448: F3          di
2449: DD 77 0A    ld   (ix+$0a),a            ; set VECTOR.D.P.L   
244C: DD 74 0B    ld   (ix+$0b),h            ; set VECTOR.D.P.H
244F: FB          ei

; Set robot speed. 
2450: 3A 4C 43    ld   a,($434C)             ; read ROBOT_SPEED   
2453: DD 77 0D    ld   (ix+$0d),a            ; set VECTOR.TPRIME
2456: C9          ret


;
; Make a robot explode.
;
; Expects:
; IX = pointer to robot's VECTOR structure
;
; See also: BLAM contained within robot.asm in Frenzy's source code.

BLAM:
2457: E5          push hl
2458: DD E5       push ix
245A: CD 8A 34    call $348A                 ; play sound of robot exploding
245D: E1          pop  hl                    ; HL = IX
245E: 11 06 00    ld   de,$0006
2461: 19          add  hl,de                 ; now HL points to VECTOR.V.X (X velocity)
2462: F3          di
2463: 72          ld   (hl),d                ; set VECTOR.V.X to 0.
2464: 23          inc  hl                    ; bump HL to point to VECTOR.P.X (X pos)
2465: 7E          ld   a,(hl)
2466: D6 04       sub  $04                   ; offset blast pattern
2468: 77          ld   (hl),a
2469: 23          inc  hl                    ; bump HL to point to VECTOR.V.Y (Y velocity)
246A: 72          ld   (hl),d                ; set VECTOR.V.Y to 0. As both V.X and V.Y are now 0, the robot has no velocity and won't move while it explodes.
246B: 23          inc  hl
246C: 7E          ld   a,(hl)                ; bump HL to point to VECTOR.P.Y (Y pos)
246D: D6 06       sub  $06                   ; offset for large blast
246F: 77          ld   (hl),a
2470: 23          inc  hl                    ; bump HL to point to VECTOR.D.P.L               
2471: 01 3B 10    ld   bc,$103B              ; pointer to pattern table describing explosion sequence     
2474: 71          ld   (hl),c                ; set VECTOR.D.P.L 
2475: 23          inc  hl                    ; bump HL to point to VECTOR.D.P.H
2476: 70          ld   (hl),b                ; set VECTOR.D.P.H 
2477: FB          ei
2478: 23          inc  hl                    ; bump HL to point to VECTOR.TIME
2479: 36 01       ld   (hl),$01              ; set VECTOR.TIME to 1 (kick off animation immediately)
247B: 23          inc  hl                    ; bump HL to point to VECTOR.TPRIME
247C: 36 01       ld   (hl),$01              ; set VECTOR.TPRIME
247E: DD E5       push ix
2480: 01 05 01    ld   bc,$0105              ; 50 PTS
2483: CD 41 23    call $2341                 ; call UPDATE_SCORE 
2486: 21 4E 43    ld   hl,$434E              ; load HL with address of OTTO_TIME
2489: 34          inc  (hl)                  ; delay otto's appearance slightly! 
248A: 34          inc  (hl)
248B: 21 71 43    ld   hl,$4371              ; load HL with address of RCOUNT
248E: 35          dec  (hl)                  ; decrement count
248F: 20 3A       jr   nz,$24CB              ; if any robots are still alive, goto $24CB

; All the robots in the room have been destroyed.
; Award <number of robots that were in room> * 10  points   
2491: 3A 72 43    ld   a,($4372)             ; read RSAVED
2494: F5          push af                    ; see SCLOP in Frenzy's source code.
2495: 01 01 01    ld   bc,$0101              ; 10 PTS
2498: CD 41 23    call $2341                 ; call UPDATE_SCORE
249B: F1          pop  af
249C: 3D          dec  a                     ; reduce counter of robots
249D: 20 F5       jr   nz,$2494              ; repeat until 10PTs awarded for each robot.

; And now display message on screen stating the bonus for clearing the room
249F: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
24A2: 
    00
    60 D5          
24A5:  42 4F 4E 55 53 00                                BONUS.



24AB: F5          push af
24AC: 3A 72 43    ld   a,($4372)             ; read RSAVED
24AF: C6 00       add  a,$00
24B1: 27          daa
24B2: 0F          rrca
24B3: 0F          rrca
24B4: 0F          rrca
24B5: 0F          rrca
24B6: 6F          ld   l,a
24B7: E6 F0       and  $F0
24B9: 67          ld   h,a
24BA: 3E 0F       ld   a,$0F
24BC: A5          and  l
24BD: 6F          ld   l,a
24BE: F1          pop  af
24BF: 22 00 43    ld   ($4300),hl
24C2: 08          ex   af,af'

24C3: 21 00 43    ld   hl,$4300
24C6: 06 04       ld   b,$04
24C8: CD 4A 2A    call $2A4A                 ; call SHOWO
24CB: 3E 1E       ld   a,$1E
24CD: CD 6D 1E    call $1E6D                 ; call ACTIVATE_HEAD_JOB
24D0: DD E1       pop  ix
24D2: 21 08 12    ld   hl,$1208
24D5: DD 7E 04    ld   a,(ix+$04)
24D8: BD          cp   l
24D9: 20 06       jr   nz,$24E1
24DB: DD 7E 05    ld   a,(ix+$05)
24DE: BC          cp   h
24DF: 28 09       jr   z,$24EA
24E1: DD E5       push ix
24E3: CD 78 1E    call $1E78                 ; call STOP_JOB → yield to runner
24E6: DD E1       pop  ix
24E8: 18 E8       jr   $24D2



24EA: CD F7 24    call $24F7
24ED: 2A 72 08    ld   hl,($0872)            ; load HL with contents of TIMER_LIST_PTR
24F0: CB 86       res  0,(hl)
24F2: CD 78 1E    call $1E78                 ; call STOP_JOB → yield to runner
24F5: 18 F6       jr   $24ED


;
; SPLICE_VECTOR — Remove a robot VECTOR from the backward-linked list
; at V.PTR ($0870). Called when a robot's death animation finishes.
;
; The linked list works like the job system: each VECTOR stores a
; pointer to the previous VECTOR at offsets ix-$02 (lo) and ix-$01 (hi).
; To remove a robot, this routine walks the chain looking for the node
; that points TO the dying robot, then overwrites that link with the
; dying robot's own backward pointer — effectively "unhooking" it.
;
; Frenzy uses an explicit JobDel routine instead; Berzerk does it
; manually because of its different job/scheduling architecture.
;
; Pseudocode:
;   dying_robot = IX                 // address of VECTOR being removed
;   scanner = V.PTR - 1              // start searching from end of V.PTR
;   loop:
;       someones_backlink = *scanner // read the 2-byte backward link
;       scanner = scanner - 1
;       if (someones_backlink == dying_robot): found_it
;   // scanner now points at the link that references dying_robot
;   *scanner = *(dying_robot - 2)   // replace that link with dying_robot's
;   V.PTR = scanner + 2             //   own backlink, splicing it out
;
; Expects: IX = VECTOR to remove
; Destroys: BC, DE, HL

24F7: DD E5       push ix                    ; BC = dying_robot address
24F9: C1          pop  bc
24FA: 50          ld   d,b
24FB: 59          ld   e,c                   ; DE = dying_robot
24FC: EB          ex   de,hl                 ; HL = dying_robot

; Walk backward through memory looking for who links to dying_robot
24FD: 2B          dec  hl                    ; scanner = dying_robot - 1
24FE: 56          ld   d,(hl)                ; read someones_backlink (hi)
24FF: 2B          dec  hl                    ; scanner = scanner - 1
2500: 5E          ld   e,(hl)                ; read someones_backlink (lo)
2501: 78          ld   a,b                   ; compare backlink (hi)
2502: BA          cp   d                     ;  against dying_robot (hi)
2503: 20 F7       jr   nz,$24FC              ; not a match → step scanner back
2505: 79          ld   a,c                   ; compare backlink (lo)
2506: BB          cp   e                     ;  against dying_robot (lo)
2507: 20 F3       jr   nz,$24FC              ; not a match → step scanner back
                                             ; found: scanner points at link referencing dying_robot
2509: F3          di
250A: DD 7E FE    ld   a,(ix-$02)            ; read dying_robot's own backlink (lo)
250D: 77          ld   (hl),a                ; overwrite the found link → splices it out
250E: 23          inc  hl
250F: DD 7E FF    ld   a,(ix-$01)            ; read dying_robot's own backlink (hi)
2512: 77          ld   (hl),a
2513: 23          inc  hl
2514: 22 70 08    ld   ($0870),hl            ; V.PTR = scanner + 2
2517: FB          ei
2518: C9          ret


; Movement table
;
; This table specifies deltas to be added to X,Y coordinates.
; Each entry in the table is a pair of signed bytes.
;
; The first byte of each pair is the X Delta (XDelta) and the second byte
; is the Y Delta (YDelta)
; 
; Remarks:
; See also: M.TAB within super.asm in Frenzy's source code.
;

VELOCITY_TABLE:
2519: 
    00 00                                    ; No direction. Will not affect X,Y coordinates        
    01 FF                                    ; Up right (XDelta = 1, YDelta =-1)
    01 00                                    ; Right (XDelta = 1, YDelta =0)   
    01 01                                    ; Down Right (XDelta = 1, YDelta = 1)
    00 01                                    ; Down (XDelta = 0, YDelta = 1)
    FF 01                                    ; Down Left (XDelta =-1, YDelta = 1)
    FF 00                                    ; Left (XDelta = -1, YDelta = 0)   
    FF FF                                    ; Up Left (XDelta =-1, YDelta = -1)   
    00 FF                                    ; Up (XDelta =0, YDelta = -1)   
    00 00                                    ; XDelta, YDelta both 0: Will not affect X,Y coordinates


;
; Pointers to pattern tables for the robots. 
; 
; See also: P.TAB within robot.asm in Frenzy's source code.
;

ROBOT_ANIMATION_TABLES:
252D:  
    00 10                                    ; Pointer to robot Standing still pattern table  
    13 10                                    ; Pointer to Robot moving up & right pattern table      
    13 10                                    ; Pointer to Robot moving right pattern table   
    13 10                                    ; Pointer to Robot moving down & right pattern table   
    1C 10                                    ; Pointer to Robot moving down pattern table 
    27 10                                    ; Pointer to Robot moving down & left pattern table  
    27 10                                    ; Pointer to Robot moving left pattern table  
    27 10                                    ; Pointer to Robot moving up & left pattern table
    30 10                                    ; Pointer to Robot moving up pattern table
    
253F: 41          ld   b,c

;
; Draw the maze room walls based on ROOM_X and ROOM_Y.
; Seeds the RNG with room coordinates, then builds the
; maze layout by calling drawing helpers and the
; procedural maze generator at $25EB.
;

2540: 2A 45 43    ld   hl,($4345)
2543: 22 5C 43    ld   ($435C),hl
2546: 3A 79 43    ld   a,($4379)             ; read FLIP
2549: B7          or   a
254A: 20 05       jr   nz,$2551
254C: 21 6A 5E    ld   hl,$5E6A
254F: 18 03       jr   $2554
2551: 21 4A 44    ld   hl,$444A
2554: 11 14 00    ld   de,$0014
2557: AF          xor  a
2558: 0E 0C       ld   c,$0C
255A: 06 0C       ld   b,$0C
255C: 77          ld   (hl),a
255D: 23          inc  hl
255E: 10 FC       djnz $255C
2560: 19          add  hl,de
2561: 0D          dec  c
2562: 20 F6       jr   nz,$255A
2564: 01 0F 00    ld   bc,$000F
2567: 11 5E 43    ld   de,$435E
256A: 21 8C 26    ld   hl,$268C
256D: ED B0       ldir
256F: 21 08 00    ld   hl,$0008
2572: CD D4 25    call $25D4
2575: 21 08 CC    ld   hl,$CC08
2578: CD D4 25    call $25D4
257B: 21 04 00    ld   hl,$0004
257E: CD CA 25    call $25CA
2581: 21 F8 00    ld   hl,$00F8
2584: CD CA 25    call $25CA
2587: DD 21 5E 43 ld   ix,$435E
258B: 21 38 44    ld   hl,$4438
258E: CD EB 25    call $25EB
2591: 21 38 88    ld   hl,$8838
2594: CD EB 25    call $25EB
2597: CD 9F 36    call $369F

; Display number of lives
; First find out who's playing
259A: 3A 44 43    ld   a,($4344)             ; read CURRENT_PLAYER
259D: FE 02       cp   $02                   ; is it player 2 in control?
259F: 21 38 D5    ld   hl,$D538              ; H = Y coordinate, L = X coordinate for player 1 lives
25A2: 20 02       jr   nz,$25A6              ; not player 2 in control, goto $25A6
25A4: 2E E8       ld   l,$E8                 ; L = X coordinate for player 2 lives
25A6: 06 00       ld   b,$00                 ; magic image RAM control bits
25A8: CD A3 29    call $29A3                 ; call CALCULATE_MAGIC_IMAGE_RAM_ADDRESS
25AB: EB          ex   de,hl                 ; now DE = magic image RAM address
25AC: 08          ex   af,af'
25AD: 3A 49 43    ld   a,($4349)             ; read DEATHS
25B0: 47          ld   b,a                   ; load B with count of player lives
25B1: 08          ex   af,af'
25B2: 05          dec  b
25B3: 28 0A       jr   z,$25BF
; Display B number of player life icons starting from magic image RAM address DE
25B5: C5          push bc
25B6: 0E 80       ld   c,$80                 ; ordinal for "life" character (looks like a stick man.)
25B8: CD DB 29    call $29DB                 ; call PRINT_CHAR
25BB: 13          inc  de                    ; bump DE to point to next image ram address 
25BC: C1          pop  bc
25BD: 10 F6       djnz $25B5                 ; repeat until all lives drawn. 
; are we in demo mode? If so, print the number of credits inserted too
25BF: 3A 6E 43    ld   a,($436E)             ; read IS_DEMO_MODE
25C2: B7          or   a                     ; test if in DEMO mode
25C3: C4 CD 18    call nz,$18CD              ; we're in demo mode, so call PRINT_CREDITS
25C6: CD 14 23    call $2314                 ; call SHOW_SCORE
25C9: C9          ret

25CA: CD 62 26    call $2662                 ; CALL DRAW_HORIZONTAL_WALL
25CD: 3E 40       ld   a,$40
25CF: 84          add  a,h
25D0: 67          ld   h,a                   ; X = X + 64
25D1: C3 62 26    jp   $2662                 ; jump to DRAW_HORIZONTAL_WALL
25D4: CD 4C 26    call $264C                 ; CALL DRAW_VERTICAL_WALL
25D7: CD 4C 26    call $264C                 ; CALL DRAW_VERTICAL_WALL
25DA: 3E 30       ld   a,$30
25DC: 85          add  a,l
25DD: 6F          ld   l,a                   ; Y = Y + 48
25DE: CD 4C 26    call $264C                 ; CALL DRAW_VERTICAL_WALL
25E1: C3 4C 26    jp   $264C                 ; jump to DRAW_VERTICAL_WALL

;
; Calculate magic image RAM address for wall drawing.
;
; Expects: H = X, L = Y
; Returns: DE = magic image RAM address
;
; See also: RtoAx in showa.asm within Frenzy's source code.

25E4: 06 10       ld   b,$10                 ; magic RAM control bits (XOR mode)
25E6: CD A3 29    call $29A3                 ; call CALCULATE_MAGIC_IMAGE_RAM_ADDRESS
25E9: EB          ex   de,hl                 ; return address in DE
25EA: C9          ret

;
; Procedural maze room generator. Uses the seeded RNG
; to randomly generate wall configurations for the
; current room. Randomly selects wall segments, doors,
; and open spaces.
;
; Expects:
; HL = magic image RAM base address
; IX = pointer to maze attribute data at $435E

CREATE_ROOM:
25EB: CD 78 26    call $2678                 ; call RANDOM
25EE: E5          push hl
25EF: CD 78 26    call $2678                 ; call RANDOM
25F2: 01 06 26    ld   bc,$2606
25F5: C5          push bc
25F6: E6 03       and  $03
25F8: CA 30 26    jp   z,$2630
25FB: 3D          dec  a
25FC: CA 40 26    jp   z,$2640
25FF: 3D          dec  a
2600: CA 14 26    jp   z,$2614
2603: C3 20 26    jp   $2620
2606: E1          pop  hl
2607: DD 23       inc  ix
2609: 3E 30       ld   a,$30
260B: 85          add  a,l
260C: 6F          ld   l,a
260D: FE DC       cp   $DC
260F: 38 DA       jr   c,$25EB
2611: DD 23       inc  ix
2613: C9          ret
2614: CD 4C 26    call $264C                 ; CALL DRAW_VERTICAL_WALL
2617: DD CB 01 DE set  3,(ix+$01)
261B: DD CB 06 D6 set  2,(ix+$06)
261F: C9          ret

2620: 7D          ld   a,l
2621: D6 30       sub  $30
2623: 6F          ld   l,a
2624: CD 4C 26    call $264C                 ; CALL DRAW_VERTICAL_WALL
2627: DD CB 00 DE set  3,(ix+$00)
262B: DD CB 05 D6 set  2,(ix+$05)
262F: C9          ret

2630: 7C          ld   a,h
2631: D6 44       sub  $44
2633: 67          ld   h,a
2634: CD 62 26    call $2662                 ; CALL DRAW_HORIZONTAL_WALL
2637: DD CB 00 CE set  1,(ix+$00)
263B: DD CB 01 C6 set  0,(ix+$01)
263F: C9          ret

2640: CD 62 26    call $2662                 ; CALL DRAW_HORIZONTAL_WALL
2643: DD CB 05 CE set  1,(ix+$05)
2647: DD CB 06 C6 set  0,(ix+$06)
264B: C9          ret

;
; Draw a vertical wall by drawing the wall sprite 12 times,
; moving downward (Y+4) each step.
;
; Expects:
; H = X coordinate
; L = Y coordinate
;
; Returns:
; H = X (unchanged)
; L = Y + 48 (end of the wall)
;
DRAW_VERTICAL_WALL:
264C: 06 0C       ld   b,$0C                 ; 12 copies (12 * 4 = 48 pixels tall)
264E: C5          push bc
264F: E5          push hl                    ; save (X,Y)
2650: CD E4 25    call $25E4                 ; get magic RAM address to draw wall
2653: 21 9B 26    ld   hl,$269B              ; address of WALL_SPRITE_PATTERN
2656: CD 17 28    call $2817                 ; call DRAW_SPRITE
2659: E1          pop  hl                    ; restore (X,Y)
265A: C1          pop  bc
265B: 3E 04       ld   a,$04
265D: 85          add  a,l                   ; Y += 4 (move down one tile)
265E: 6F          ld   l,a
265F: 10 ED       djnz $264E                 ; loop 12 times
2661: C9          ret

;
; Draw a horizontal wall by drawing the wall sprite 18 times,
; moving rightward (X+4) each step. 
;
; Expects:
; H = X coordinate
; L = Y coordinate
;
; Returns:
; H = X + 72 (end of the wall)
; L = Y (unchanged)
;
DRAW_HORIZONTAL_WALL:
2662: 06 12       ld   b,$12                 ; 18 copies (18 * 4 = 72 pixels wide)
2664: C5          push bc
2665: E5          push hl                    ; save (X,Y)
2666: CD E4 25    call $25E4                 ; get magic RAM address to draw wall
2669: 21 9B 26    ld   hl,$269B              ; address of WALL_SPRITE_PATTERN
266C: CD 17 28    call $2817                 ; call DRAW_SPRITE
266F: E1          pop  hl                    ; restore (X,Y)
2670: C1          pop  bc
2671: 3E 04       ld   a,$04
2673: 84          add  a,h                   ; X += 4 (move right one tile)
2674: 67          ld   h,a
2675: 10 ED       djnz $2664                 ; loop 18 times
2677: C9          ret


; Generate a random number.
;
; Returns: A = random number
; 
; See also: RANDOM contained within demo.asm in Frenzy's source code.
RANDOM:
2678: E5          push hl
2679: 2A 5C 43    ld   hl,($435C)            ; load HL with RNG_SEED         
267C: 54          ld   d,h
267D: 5D          ld   e,l
267E: 29          add  hl,hl
267F: 19          add  hl,de
2680: 29          add  hl,hl
2681: 19          add  hl,de
2682: 11 53 31    ld   de,$3153
2685: 19          add  hl,de
2686: 22 5C 43    ld   ($435C),hl            ; set RNG_SEED 
2689: 7C          ld   a,h
268A: E1          pop  hl
268B: C9          ret


;
;   $268C: 05 04 04 04 06    ; row 0, cols 0-4 (X<70)
;   $2691: 01 00 00 00 02    ; row 1, cols 0-4 (70≤X<138)
;   $2696: 09 08 08 08 0A    ; row 2, cols 0-4 (X≥138)
;
; $1CE7 maps (X,Y) to index = (row * 5) + col, where col 0=Y<58, col 4=Y≥202.
;
; Bit 0 ($01) = wall on LEFT edge  | Bit 1 ($02) = wall on RIGHT edge
; Bit 2 ($04) = wall on TOP edge   | Bit 3 ($08) = wall on BOTTOM edge
;
; Confirmed by: $25EB (CREATE_ROOM) and $1C6E (IQ).

BLOCKED_DIRECTIONS:
268C: 
    05 04 04 04 06 
    01 00 00 00 02
    09 08 08 08 0A

WALL_SPRITE_PATTERN:
269B: 
    01 04 
    F0 F0 F0 F0

26A1: 02          ld   (bc),a
26A2: 04          inc  b
26A3: FF          rst  $38
26A4: F0          ret  p
26A5: FF          rst  $38
26A6: F0          ret  p
26A7: FF          rst  $38
26A8: F0          ret  p
26A9: FF          rst  $38
26AA: F0          ret  p

; Interrupt routine.
; 
; Remarks:
; See also: INT within init.asm in Frenzy's source code.

26AB: F3          di
26AC: ED 73 74 08 ld   ($0874),sp            ; set STACK_PTR
26B0: 31 40 08    ld   sp,$0840
26B3: F5          push af
26B4: DB 4E       in   a,($4E)               ; read middle/bottom screen status
26B6: 1F          rra
26B7: 38 20       jr   c,$26D9
26B9: E5          push hl
26BA: C5          push bc
26BB: 21 9F 08    ld   hl,$089F
26BE: 7E          ld   a,(hl)
26BF: 23          inc  hl
26C0: 46          ld   b,(hl)
26C1: A8          xor  b
26C2: 4F          ld   c,a
26C3: DB 49       in   a,($49)               ; read SYSTEM
26C5: 2F          cpl
26C6: 77          ld   (hl),a
26C7: 2B          dec  hl
26C8: 70          ld   (hl),b
26C9: A1          and  c
26CA: E6 E0       and  $E0
26CC: 2B          dec  hl
26CD: 87          add  a,a
26CE: 30 03       jr   nc,$26D3
26D0: 34          inc  (hl)
26D1: 18 F9       jr   $26CC

26D3: 20 F7       jr   nz,$26CC
26D5: C1          pop  bc
26D6: E1          pop  hl
26D7: 18 43       jr   $271C


;
;
; Remarks:
; See also: BS in init.asm within Frenzy's source code.
;

BOTTOM_OF_SCREEN_INTERRUPT:
26D9: FD E5       push iy
26DB: DD E5       push ix
26DD: E5          push hl
26DE: D5          push de
26DF: C5          push bc
26E0: 08          ex   af,af'
26E1: F5          push af
26E2: 2A 70 08    ld   hl,($0870)            ; read V.PTR
26E5: E5          push hl

; Erase the player's sprite from the screen
26E6: 2A 76 08    ld   hl,($0876)            ; load HL with contents of MAN_PTR 
26E9: CD 2D 27    call $272D                 ; call ERASE_PATTERN
26EC: CD 19 37    call $3719                 ; call UNCOLOUR_MAN
26EF: E1          pop  hl
26F0: E5          push hl
26F1: CD 2D 27    call $272D                 ; call ERASE_PATTERN
26F4: CD F3 14    call $14F3                 ; call HANDLE_PLAYER_BOLTS

; Update players position on screen 
26F7: 2A 76 08    ld   hl,($0876)            ; load HL with contents of MAN_PTR
26FA: CD A9 27    call $27A9                 ; call MOVE_ANIMATE_VECTOR

;
26FD: E1          pop  hl
26FE: CD A9 27    call $27A9                 ; call MOVE_ANIMATE_VECTOR

2701: 2A 70 08    ld   hl,($0870)            ; read V.PTR
2704: 7C          ld   a,h
2705: B5          or   l
2706: 28 08       jr   z,$2710
2708: 2B          dec  hl
2709: 56          ld   d,(hl)
270A: 2B          dec  hl
270B: 5E          ld   e,(hl)
270C: ED 53 70 08 ld   ($0870),de            ; set V.PTR   

2710: CD F5 27    call $27F5
2713: F1          pop  af
2714: 08          ex   af,af'
2715: C1          pop  bc
2716: D1          pop  de
2717: E1          pop  hl
2718: DD E1       pop  ix
271A: FD E1       pop  iy
271C: 3E 01       ld   a,$01
271E: D3 4F       out  ($4F),a               ; turn on interrupts
2720: 3E 37       ld   a,$37
2722: ED 47       ld   i,a                   ; set interrupt flags
2724: ED 5E       im   2
2726: F1          pop  af
2727: ED 7B 74 08 ld   sp,($0874)            ; restore SP from STACK_PTR
272B: FB          ei
272C: C9          ret



; Expects:
; HL = pointer to VECTOR structure
;
; Remarks:
; See also: SECT1 in Frenzy's INIT.ASM source.

ERASE_PATTERN:
272D: 22 70 08    ld   ($0870),hl
2730: E5          push hl
2731: FD E1       pop  iy                    ; IY = HL
2733: CB 46       bit  0,(hl)                ; test ERASE bit in VECTOR.Status
2735: CA 4D 27    jp   z,$274D               ; if we're not erasing, we must be writing, goto WRITE_PATTERN
2738: CB 86       res  0,(hl)                ; clear ERASE bit so its not done twice
273A: 23          inc  hl                    ; bump HL to point to VECTOR.Magic
273B: 7E          ld   a,(hl)                ; read VECTOR.Magic 
273C: D3 4B       out  ($4B),a               ; write to magicram_control_w
273E: 23          inc  hl                    ; 
273F: 5E          ld   e,(hl)                ; read VECTOR.O.A.L
2740: 23          inc  hl
2741: 56          ld   d,(hl)                ; read VECTOR.O.A.H
2742: 23          inc  hl
2743: 7E          ld   a,(hl)                ; read VECTOR.O.P.L
2744: 23          inc  hl
2745: 66          ld   h,(hl)                ; read VECTOR.O.P.H   
2746: 6F          ld   l,a                  
; HL now = pointer to sprite width, height, pattern data
; DE = pointer to magic image RAM
2747: CD 17 28    call $2817                 ; call DRAW_SPRITE

; Restore pointer to VECTOR structure we preserved @ $272D
274A: 2A 70 08    ld   hl,($0870)

; HL = pointer to VECTOR structure
; IY = same as HL
;
; Remarks:
; See also: SECT2 in Frenzy's INIT.ASM source.

WRITE_PATTERN:
274D: CB 4E       bit  1,(hl)                ; test WRITE bit in VECTOR.Status
274F: C8          ret  z
2750: CB 8E       res  1,(hl)                ; clear WRITE bit
2752: 11 07 00    ld   de,$0007              ; offset of P.X field in VECTOR struct
2755: 19          add  hl,de                 ; bump HL to point to VECTOR.P.X (X pos) 
2756: 5E          ld   e,(hl)                ; read VECTOR.P.X 
2757: 23          inc  hl
2758: 23          inc  hl                    ; bump HL to point to VECTOR.P.Y (Y pos) 
2759: 56          ld   d,(hl)                ; read VECTOR.P.Y 
275A: 23          inc  hl
275B: 06 90       ld   b,$90                 ; XOR write magic control byte
275D: EB          ex   de,hl                 ; Now H = VECTOR.Y, L = VECTOR.X
275E: CD A3 29    call $29A3                 ; call CALCULATE_MAGIC_IMAGE_RAM_ADDRESS

; now A = magic RAM control byte, and HL = magic RAM address
2761: FD 77 01    ld   (iy+$01),a            ; write to VECTOR.Magic
2764: EB          ex   de,hl                 

; now DE = magic RAM address and HL = pointer to VECTOR.D.P.L
2765: 7E          ld   a,(hl)                ; read VECTOR.D.P.L
2766: 23          inc  hl                   
2767: 66          ld   h,(hl)                ; read VECTOR.D.P.H
2768: 6F          ld   l,a                   ; HL now = pointer to a pattern table (see $1000 for example)
2769: 7E          ld   a,(hl)                
276A: 23          inc  hl
276B: 6E          ld   l,(hl)
276C: 67          ld   h,a                   ; HL now = pointer to sprite pattern
276D: 7E          ld   a,(hl)
276E: CB 7F       bit  7,a
2770: 28 19       jr   z,$278B
2772: 23          inc  hl
2773: E6 7F       and  $7F
2775: 47          ld   b,a
2776: 7E          ld   a,(hl)
2777: 23          inc  hl
2778: 4F          ld   c,a
2779: EB          ex   de,hl
277A: 3A 79 43    ld   a,($4379)             ; read FLIP
277D: B7          or   a
277E: CA 86 27    jp   z,$2786

; cocktail cabinet
2781: ED 42       sbc  hl,bc
2783: C3 8A 27    jp   $278A

; standard cabinet
2786: 09          add  hl,bc
2787: C3 8A 27    jp   $278A

278A: EB          ex   de,hl
; HL = pointer to sprite pattern data , DE = pointer to magic image RAM
; Note: see "..noo" in init.asm within Frenzy's source code.
278B: FD 75 04    ld   (iy+$04),l            ; set VECTOR.O.P.L
278E: FD 74 05    ld   (iy+$05),h            ; set VECTOR.O.P.H
2791: FD 73 02    ld   (iy+$02),e            ; set VECTOR.O.A.L 
2794: FD 72 03    ld   (iy+$03),d            ; set VECTOR.O.A.H
2797: CD 17 28    call $2817                 ; call DRAW_SPRITE
279A: 2A 70 08    ld   hl,($0870)            ; get pointer to next VECTOR to process from V.PTR

; Has this sprite been drawn over some other pixel data? If so, then register a collision.
279D: DB 4E       in   a,($4E)               ; read intercept bit
279F: CB 7F       bit  7,a
27A1: C8          ret  z
27A2: CB FE       set  7,(hl)                ; set HIT bit
27A4: FD CB FA C6 set  0,(iy-$06)
27A8: C9          ret


;
; Move and animate a VECTOR, such as player or robot (including Otto) 
;
; Expects: 
; HL = pointer to VECTOR structure
;
; See also: SECT3 in init.asm within Frenzy's source code.

MOVE_ANIMATE_VECTOR:
27A9: 22 70 08    ld   ($0870),hl            ; set V.PTR to point to next VECTOR structure
27AC: CB 56       bit  2,(hl)                ; test if MOVE bit is set in VECTOR.Status 
27AE: C8          ret  z                     ; exit if bit not set
27AF: E5          push hl
27B0: FD E1       pop  iy                    ; IY is now a pointer to the VECTOR structure
27B2: 11 0C 00    ld   de,$000C              ; offset of VECTOR.TIME field
27B5: 19          add  hl,de                 ; point HL to VECTOR.TIME field

; VECTOR.TIME is used to control how fast the VECTOR moves on screen.
; A VECTOR can only move when VECTOR.TIME counts down to zero.
27B6: 35          dec  (hl)                  ; decrement VECTOR.TIME timer
27B7: C0          ret  nz                    ; exit if timer is not zero

; When VECTOR.TIME reaches zero, reload it with VECTOR.TPRIME.
27B8: 23          inc  hl                    ; point HL to VECTOR.TPRIME field
27B9: 7E          ld   a,(hl)                ; read value of TPRIME 
27BA: 2B          dec  hl                    ; point HL to VECTOR.TIME field
27BB: 77          ld   (hl),a                ; set TIME field from TPRIME
27BC: 11 FA FF    ld   de, -6

; VECTOR.TIME has counted down to zero and the object can move.
; Add velocity deltas VECTOR.V.X and VECTOR.V.Y to VECTOR.P.X and VECTOR.P.Y coordinates
; This changes the screen position of the VECTOR.   
27BF: 19          add  hl,de                 ; now HL points to VECTOR.V.X
27C0: 7E          ld   a,(hl)                ; read VECTOR.V.X
27C1: 23          inc  hl                    ; bump HL to point to VECTOR.P.X
27C2: 86          add  a,(hl)                ; 
27C3: 77          ld   (hl),a                ; VECTOR.P.X += VECTOR.V.X
27C4: 23          inc  hl                    ; bump HL to point to VECTOR.V.Y
27C5: 7E          ld   a,(hl)                ; read VECTOR.V.Y
27C6: 23          inc  hl                    ; bump HL to point to VECTOR.P.Y
27C7: 86          add  a,(hl)                ; 
27C8: 77          ld   (hl),a                ; VECTOR.P.Y += VECTOR.V.Y

; get pointer to next pattern to draw 
27C9: 23          inc  hl                    ; bump HL to point to VECTOR.D.P.L
27CA: 5E          ld   e,(hl)
27CB: 23          inc  hl                    ; bump HL to point to VECTOR.D.P.H   
27CC: 56          ld   d,(hl)                
27CD: 13          inc  de
27CE: 13          inc  de
27CF: EB          ex   de,hl

; When we encounter a zero byte this is the end of the pattern table. 
27D0: 7E          ld   a,(hl)
27D1: B7          or   a
27D2: C2 DA 27    jp   nz,$27DA

; We've reached the end of the pattern table.  
27D5: 23          inc  hl
27D6: 7E          ld   a,(hl)
27D7: 23          inc  hl
27D8: 66          ld   h,(hl)
27D9: 6F          ld   l,a

27DA: EB          ex   de,hl
27DB: 72          ld   (hl),d
27DC: 2B          dec  hl
27DD: 73          ld   (hl),e

27DE: 3E 1B       ld   a,$1B
27E0: FD B6 00    or   (iy+$00)
27E3: FD 77 00    ld   (iy+$00),a            ; set VECTOR.Status flags.

27E6: FD CB 00 6E bit  5,(iy+$00)
27EA: C8          ret  z
27EB: 3A 78 43    ld   a,($4378)             ; read PLAYER_COLOUR
27EE: 07          rlca
27EF: EE 11       xor  $11
27F1: 32 78 43    ld   ($4378),a             ; set PLAYER_COLOUR
27F4: C9          ret

;
; Process the linked timer list.
; Decrements TIMER_ITEM.Delay for each entry.
; When Delay reaches 0, sets bit 0 (active flag)
; and clears bit 1 (counting flag).
;
; Walks the list via the linked list pointers.

27F5: 2A 72 08    ld   hl,($0872)            ; load HL with contents of TIMER_LIST_PTR
27F8: 7C          ld   a,h
27F9: B5          or   l                            
27FA: C8          ret  z                     ; return if HL is NULL
27FB: 54          ld   d,h
27FC: 5D          ld   e,l
27FD: CB 4E       bit  1,(hl)
27FF: 28 09       jr   z,$280A
2801: 23          inc  hl
2802: 35          dec  (hl)
2803: 2B          dec  hl
2804: 20 04       jr   nz,$280A
2806: CB 8E       res  1,(hl)
2808: CB C6       set  0,(hl)
280A: 2B          dec  hl
280B: 7E          ld   a,(hl)
280C: 2B          dec  hl
280D: 6E          ld   l,(hl)
280E: 67          ld   h,a
280F: BA          cp   d
2810: 20 EB       jr   nz,$27FD
2812: 7D          ld   a,l
2813: BB          cp   e
2814: 20 E7       jr   nz,$27FD
2816: C9          ret


;
; Draw sprite either 1 or 2 bytes wide (1 bit per pixel, so max sprite width = 16 pixels)
;
; Expects:
; HL = pointer to sprite width, height, pattern data  
; DE = pointer to magic image RAM
; First byte:  width in bytes. 1 bit per pixel. 
; Second byte: height in bytes
;
; See also: PLOT in init.asm within Frenzy's source code.

DRAW_SPRITE:
2817: 06 00       ld   b,$00
2819: 7E          ld   a,(hl)                ; read width of sprite
281A: 23          inc  hl                    ; bump HL to point to height 
281B: 3D          dec  a
281C: CA 53 28    jp   z,$2853               ; if width == 1 byte, goto DRAW_SPRITE_1_BYTE_WIDE

; Sprite is now presumed to be 2 bytes wide
281F: 3A 79 43    ld   a,($4379)             ; read FLIP
2822: B7          or   a                     ; set zero flag if upright cabinet

2823: 7E          ld   a,(hl)                ; read height of sprite
2824: 23          inc  hl                    ; bump HL to point to first byte of sprite pattern data
2825: C2 3D 28    jp   nz,$283D              ; if cocktail cabinet, goto $283D

; upright cabinet
2828: 01 1E 00    ld   bc,$001E              ; $1E (30 decimal) = number of bytes to add to get to next row of image RAM after writing 2 bytes 
282B: EB          ex   de,hl                 ; now DE = pointer to pattern data, HL = pointer to magic RAM
282C: 08          ex   af,af'                   
282D: 1A          ld   a,(de)                ; read pattern data   
282E: 13          inc  de                    ; bump DE to next byte of pattern data
282F: 77          ld   (hl),a                ; write pattern data to magic image RAM                  
2830: 23          inc  hl                    ; bump HL to next image RAM address
2831: 1A          ld   a,(de)                ; read pattern data      
2832: 13          inc  de                    ; bump DE to next byte of pattern data
2833: 77          ld   (hl),a                ; write pattern data to magic image RAM
2834: 23          inc  hl                    ; bump HL to next image RAM address
2835: 70          ld   (hl),b                ; write 0 to image RAM (Frenzys docs says "flush shifter")
2836: 08          ex   af,af'                ; now A is number of rows left in sprite left to draw
2837: 09          add  hl,bc                 ; adjust HL so that it points to the next row down in image RAM
2838: 3D          dec  a                     ; decrement count of rows of sprite left to draw
2839: C2 2C 28    jp   nz,$282C              ; repeat until all rows done
283C: C9          ret

; cocktail cabinet
283D: 01 E2 FF    ld   bc,$FFE2
2840: EB          ex   de,hl
2841: 08          ex   af,af'
2842: 1A          ld   a,(de)
2843: 13          inc  de
2844: 77          ld   (hl),a
2845: 2B          dec  hl
2846: 1A          ld   a,(de)
2847: 13          inc  de
2848: 77          ld   (hl),a
2849: 2B          dec  hl
284A: 36 00       ld   (hl),$00
284C: 08          ex   af,af'
284D: 09          add  hl,bc
284E: 3D          dec  a
284F: C2 41 28    jp   nz,$2841
2852: C9          ret



;
; Draw 1 byte wide sprite.
;
; Expects:
; HL = pointer to byte specifying sprite height followed by pattern data 
; DE = pointer to magic image RAM
;
; See also: X1PLOT in init.asm within Frenzy's source code 

DRAW_SPRITE_1_BYTE_WIDE:
2853: 3A 79 43    ld   a,($4379)             ; read FLIP				 
2856: B7          or   a                     ; if upright cabinet, zero flag will be set
2857: 7E          ld   a,(hl)                ; read height
2858: 23          inc  hl
2859: C2 6D 28    jp   nz,$286D              ; if cocktail cabinet, goto $286D

; upright mode
285C: 01 1F 00    ld   bc,$001F              ; $1F (31 decimal) = number of bytes to add to get to next row of image RAM after writing a single byte
285F: EB          ex   de,hl                 ; now DE = pointer to pattern data, HL = pointer to magic RAM
2860: 08          ex   af,af'
2861: 1A          ld   a,(de)                ; read pattern data 
2862: 13          inc  de                    ; bump DE to next byte of pattern data
2863: 77          ld   (hl),a                ; write pattern data to magic image RAM 
2864: 23          inc  hl                    ; bump HL to next image RAM address
2865: 70          ld   (hl),b                ; write 0 to image RAM (Frenzys docs says "flush shifter")
2866: 08          ex   af,af'                ; now A is number of rows left in sprite left to draw
2867: 09          add  hl,bc                 ; adjust HL so that it points to the next row down in image RAM
2868: 3D          dec  a                     ; decrement count of rows of sprite left to draw
2869: C2 60 28    jp   nz,$2860              ; repeat until all rows done
286C: C9          ret

; cocktail mode - see XF1PLT in init.asm within Frenzy's source code
286D: 01 E1 FF    ld   bc,$FFE1
2870: EB          ex   de,hl
2871: 08          ex   af,af'
2872: 1A          ld   a,(de)
2873: 13          inc  de
2874: 77          ld   (hl),a
2875: 2B          dec  hl
2876: 36 00       ld   (hl),$00
2878: 08          ex   af,af'
2879: 09          add  hl,bc
287A: 3D          dec  a
287B: C2 71 28    jp   nz,$2871
287E: C9          ret



;
; Check if a robot can shoot.
;
; Expects:
;
; C = DURL bits representing direction robot should shoot
; D = Delta X (player's X coordinate subtracted from robot's X coordinate)
; E = Delta Y (player's Y coordinate subtracted from robot's X coordinate)
; IX = pointer to robot's VECTOR structure
;
;
; See also: 
; See $23EF for code that calculates deltas.
; SHOOT in shoot.asm within Frenzy's source code.
;

SHOOT:
287F: 2A 72 08    ld   hl,($0872)            ; Load HL with contents of TIMER_LIST_PTR
2882: 23          inc  hl
2883: 7E          ld   a,(hl)
2884: B7          or   a
2885: C0          ret  nz

2886: C5          push bc
2887: D5          push de                    ; save X,Y deltas on stack
2888: 11 08 00    ld   de,$0008              ; sizeof(BOLT)
288B: 21 8F 43    ld   hl,$438F              ; load HL with address of ROBOT_BOLTS
288E: 3A 4B 43    ld   a,($434B)             ; read RBOLTS to check max allowed robot bolts on screen
2891: B7          or   a                     ; check if zero (meaning, robots can't fire)
2892: 28 08       jr   z,$289C               ; if zero, just exit
2894: 47          ld   b,a                   ; load B with RBOLTS (number of bolts to process)

2895: 7E          ld   a,(hl)                ; read BOLT.Direction DURL bits                
2896: B7          or   a                     ; if no bits set, then bolt hasn't been fired
2897: 28 06       jr   z,$289F               
2899: 19          add  hl,de
289A: 10 F9       djnz $2895                 ; repeat until all bolts are done.
289C: D1          pop  de                    ; 
289D: C1          pop  bc
289E: C9          ret

; HL = pointer to a BOLT structure 
289F: D1          pop  de
28A0: C1          pop  bc
28A1: 2B          dec  hl
28A2: 2B          dec  hl
28A3: 2B          dec  hl
28A4: 2B          dec  hl

; Quick Check if the robot should fire horizontally OR vertically (but not diagonally)   

; is the X Delta of player and robot between -2 and 5?
28A5: 7A          ld   a,d                   ; load A with X Delta
28A6: FE FE       cp   -2
28A8: D2 D8 28    jp   nc,$28D8              ; make shot go vertical
28AB: FE 06       cp   $06
28AD: 38 29       jr   c,$28D8               ; make shot go vertical 

; is the Y Delta of player and robot between -4 and 6?
28AF: 7B          ld   a,e                   ; load A with Y Delta
28B0: FE FC       cp   -4
28B2: 30 1E       jr   nc,$28D2              ; make shot go horizontal 
28B4: FE 07       cp   $07
28B6: 38 1A       jr   c,$28D2               ; make shot go horizontal  

;
; OK, now check if the robot should fire diagonally.
;
; First ensure that D (X Delta) and E (Y Delta) are positive integers.
; Skip to $28C8 if you just want to see the calculation.
28B8: 7A          ld   a,d                   ; load A with X Delta

28B9: CB 41       bit  0,c                   ; test LEFT DURL bit
28BB: 28 03       jr   z,$28C0               ; if not set, goto $28C0

; The [shoot] LEFT DURL bit is set in C, meaning the player is to the left of the robot.
; This means the Player X [coordinate] is < Robot X [coordinate].
; Given that delta X is the product of (Player X - Robot X) (see $23F4) then Delta X will be a negative number. 
; We need to convert it to its absolute value. 
28BD: ED 44       neg                        ; Convert X Delta to a positive number                    
28BF: 57          ld   d,a

28C0: 7B          ld   a,e                   ; load A with Y Delta
28C1: CB 51       bit  2,c                   ; test UP DURL bit
28C3: 28 03       jr   z,$28C8               ; if not set, goto $28C0

; The [shoot] UP DURL bit is set in C, meaning the player is standing above the robot.
; This means the Player Y [coordinate] is < Robot Y [coordinate].
; Given that delta Y is the product of (Player Y - Robot Y) (see $2405) then Delta Y will be a negative number. 
; We need to convert it to its absolute value. 

28C5: ED 44       neg                        ; Convert Y Delta to a positive number
28C7: 5F          ld   e,a

; C = DURL Direction bits indicating what direction robot will shoot
; D = absolute value of X Delta
; E = absolute value of Y Delta
; A = absolute value of Y Delta
28C8: 92          sub  d                     ; A = Y Delta - X Delta
28C9: FE F6       cp   $F6
28CB: 30 11       jr   nc,$28DE
28CD: FE 06       cp   $06
28CF: D0          ret  nc
28D0: 18 0C       jr   $28DE

; Make shot go horizontal
28D2: 79          ld   a,c
28D3: E6 03       and  $03                   ; Mask in LEFT, RIGHT DURL bits
28D5: 4F          ld   c,a
28D6: 18 06       jr   $28DE

; Make shot go vertical
28D8: 79          ld   a,c
28D9: E6 0C       and  $0C                   ; Mask in UP, DOWN DURL bits
28DB: 4F          ld   c,a
28DC: 18 00       jr   $28DE

; C = DURL bits for robot's bolt
28DE: 06 00       ld   b,$00                 ; zero B so that BC = index into DIRECTION_OFFSET_TABLE table
28E0: E5          push hl
28E1: CD E7 34    call $34E7                 ; call SRFIRE# to make robot shoot noise
28E4: 21 42 20    ld   hl,$2042              ; load HL with address of DIRECTION_OFFSET_TABLE table
28E7: 09          add  hl,bc                 ; now HL = pointer to offset in DIRECTION_OFFSET_TABLE table
28E8: 4E          ld   c,(hl)                ; now BC = offset read from DIRECTION_OFFSET_TABLE table                
28E9: 21 44 29    ld   hl,$2944              ; load HL with address of S.TAB table
28EC: 09          add  hl,bc                 ; HL = HL + (BC * 3)
28ED: 09          add  hl,bc
28EE: 09          add  hl,bc

; Set the robot's horizontal and vertical velocity to zero, to make the robot stand still before it shoots.
; If you didn't do this, the robot could walk into the bolt it's just fired and blow itself up....
28EF: DD 70 06    ld   (ix+$06),b            ; set VECTOR.V.X velocity for robot to 0
28F2: DD 70 08    ld   (ix+$08),b            ; set VECTOR.V.Y velocity for robot to 0   

; Now make the robot "look" in the direction of the player.
28F5: 7E          ld   a,(hl)                ; get pattern address low byte
28F6: 23          inc  hl                   
28F7: F3          di
28F8: DD 77 0A    ld   (ix+$0a),a            ; set robot's VECTOR.D.P.L 
28FB: 7E          ld   a,(hl)                
28FC: DD 77 0B    ld   (ix+$0b),a            ; set robot's VECTOR.D.P.H
28FF: FB          ei
2900: 23          inc  hl
2901: DD 36 0C 01 ld   (ix+$0c),$01          ; set robot's VECTOR.TIME 
2905: 46          ld   b,(hl)                ; get X delta into B
2906: 23          inc  hl
2907: 4E          ld   c,(hl)                ; get Y delta into C
2908: 23          inc  hl
2909: 56          ld   d,(hl)
290A: DD 7E 07    ld   a,(ix+$07)            ; read robot's VECTOR.X (X coordinate)
290D: 80          add  a,b                   ; add X Delta held in B (see $2905)
290E: 47          ld   b,a                   ; now B is initial X coordinate of bolt
290F: DD 7E 09    ld   a,(ix+$09)            ; read robot's VECTOR.Y (Y coordinate)
2912: 81          add  a,c                   ; add Y Delta held in C (see $2907)
2913: 4F          ld   c,a                   ; now C is initial Y coordinate of bolt

; Restore pointer to BOLT structure
2914: E1          pop  hl

; IX = pointer to robot's VECTOR structure
; HL = pointer to BOLT structure
; B = Bolt's initial X coordinate 
; C = Bolt's initial Y coordinate
; D = DURL bits for bolt's direction
2915: F3          di
2916: 72          ld   (hl),d                ; set BOLT.Direction            
2917: 23          inc  hl
2918: 36 00       ld   (hl),$00              ; set BOLT.Length
291A: 23          inc  hl
291B: 70          ld   (hl),b                ; set BOLT.X
291C: 23          inc  hl
291D: 71          ld   (hl),c                ; set BOLT.Y
291E: 23          inc  hl
291F: 72          ld   (hl),d                ; set BOLT.LastDirection
2920: 23          inc  hl
2921: 36 05       ld   (hl),$05              ; set BOLT.MaxLength
2923: 23          inc  hl
2924: 70          ld   (hl),b                ; Set BOLT.TailX
2925: 23          inc  hl
2926: 71          ld   (hl),c                ; Set BOLT.TailY
2927: FB          ei
2928: DD E5       push ix
292A: 3E 0A       ld   a,$0A                 ; delay
292C: CD 6D 1E    call $1E6D                 ; call ACTIVATE_HEAD_JOB
292F: DD E1       pop  ix


;
; TODO: Unsure what this is doing here.
;

2931: 2A 72 08    ld   hl,($0872)            ; get head of list by reading contents of TIMER_LIST_PTR
2934: 23          inc  hl
2935: 3A 4D 43    ld   a,($434D)             ; read RWAIT
2938: 77          ld   (hl),a
2939: 2B          dec  hl
293A: CB CE       set  1,(hl)
293C: E1          pop  hl
293D: 23          inc  hl
293E: 23          inc  hl
293F: F1          pop  af
2940: C1          pop  bc
2941: 0E 10       ld   c,$10
2943: E9          jp   (hl)


;
; Robot shoot table.
;
; Defines what animation a robot's VECTOR should use when shooting, and deltas to add to VECTOR's X,Y coordinates
; to compute starting X,Y coordinates for a BOLT. 
;
; Each entry in this table requires 6 bytes.
;
; WORD patternPtr       - pointer to pattern table for robot to use when shooting.
; BYTE XDelta           - X Delta to add to robot's X coordinate to compute head of bolt's X coordinate 
; BYTE YDelta           - Y Delta to add to robot's Y coordinate to compute head of bolt's Y coordinate
; BYTE Direction        - DURL bits specifying direction bolt should go
; BYTE 0                - padding byte with no other use.
;
; See also: 
; SR.TAB in this document, and S.TAB within shoot.asm in Frenzy's source code.
;

; No direction
S.TAB:
2944: 
    00 10                                    ; Pointer to pattern table @ $1000
    00                                       ; Bolt X delta  
    00                                       ; Bolt Y delta        
    00                                       ; No direction
    00

; UP + RIGHT
294A: 
    00 10                                    ; Pointer to pattern table @ $1000
    07                                       ; Bolt X delta  
    06                                       ; Bolt Y delta   
    06                                       ; DURL bits (RIGHT + UP)  
    00

; RIGHT
2950: 
    00 10                                    ; Pointer to pattern table @ $1000 
    07                                       ; Bolt X delta 
    06                                       ; Bolt Y delta 
    02                                       ; DURL bits (RIGHT)
    00

; DOWN + RIGHT
2956: 
    00 10                                    ; Pointer to pattern table @ $1000
    07                                       ; Bolt X delta 
    06                                       ; Bolt Y delta
    0A                                       ; DURL bits (RIGHT + DOWN)
    00

; DOWN
295C: 
    00 10                                    ; Pointer to pattern table @ $1000
    07                                       ; Bolt X delta 
    06                                       ; Bolt Y delta
    08                                       ; DURL bits (DOWN) 
    00

; DOWN + LEFT
2962: 
    00 10                                    ; Pointer to pattern table @ $1000
    00                                       ; Bolt X delta 
    06                                       ; Bolt Y delta
    09                                       ; DURL bits (LEFT + DOWN)  
    00

; LEFT
2968: 
    00 10                                    ; Pointer to pattern table @ $1000
    00                                       ; Bolt X delta 
    06                                       ; Bolt Y delta
    01                                       ; DURL bits (LEFT)  
    00          

; UP + LEFT
296E:
    00 10                                    ; Pointer to pattern table @ $1000           
    00                                       ; Bolt X delta 
    06                                       ; Bolt Y delta
    05                                       ; DURL bits (LEFT + UP) 
    00          

; UP
2974:   
    00 10                                    ; Pointer to pattern table @ $1000
    07                                       ; Bolt X delta 
    01                                       ; Bolt Y delta 
    04                                       ; DURL bits (UP) 
    00


297A: CA 


; Print character string
;
; Expects:
; Return address on stack to point to N bytes:
; Byte 0: magic image RAM control bits
; Byte 1: X coordinate 
; Byte 2: Y coordinate 
; Byte 3.. N: null (zero byte) terminated text string
;
; When the print routine reads a zero byte, it will then jump to the instruction following the byte.

PRINT_STRING_297B:
297B: E1          pop  hl                    ; get return address from stack into HL
297C: 46          ld   b,(hl)                ; load B with magic RAM control bits
297D: 23          inc  hl
297E: 5E          ld   e,(hl)                ; load E with X coordinate
297F: 23          inc  hl
2980: 56          ld   d,(hl)                ; load D with Y coordinate
2981: 23          inc  hl
2982: EB          ex   de,hl                 ; now H = Y coordinate, L = X coordinate
2983: CD A3 29    call $29A3                 ; call CALCULATE_MAGIC_IMAGE_RAM_ADDRESS                 
2986: EB          ex   de,hl                 ; now HL = pointer to character to print, DE = magic RAM address
2987: 4E          ld   c,(hl)                ; read character to print
2988: CB B9       res  7,c                   
298A: CD DB 29    call $29DB                 ; call PRINT_CHAR
298D: 47          ld   b,a
298E: 3A 79 43    ld   a,($4379)             ; read FLIP	
2991: B7          or   a
2992: 20 03       jr   nz,$2997              ; if cocktail setup, goto $2997

; playing on upright cabinet
2994: 13          inc  de                    ; otherwise, its upright cabinet
2995: 18 01       jr   $2998

; playing on cocktail cabinet
2997: 1B          dec  de

; 
2998: 23          inc  hl                    ; bump HL to next character to print
2999: 7E          ld   a,(hl)                ; read character
299A: B7          or   a                     ; set zero flag if terminating character 0 found
299B: 78          ld   a,b
299C: C2 87 29    jp   nz,$2987              ; if not string terminator (0) goto $2987
299F: 23          inc  hl
29A0: E9          jp   (hl)                  ; jump to code immediately after string terminator byte


;
; Expects: 
; H = Y coordinate
; L = X coordinate
;
; Returns:
; HL = Magic image RAM address
; 
; Remarks:
; See also: RtoAx (Relative To Absolute) in showa.asm within Frenzy's source code

RTOAX:
29A1: 06 90       ld   b,$90                 ; Magic image RAM control bits - XOR write

; Expects:
; B = bits to write to magicram_control_w
; H = X coordinate
; L = Y coordinate
;
; Returns:
; A = magic image RAM control byte
; HL = magic image RAM address
; if upright cabinet, BC = 0 
; (Cocktail path not investigated, as I deem that unnecessary extra work) 
;
; See also: RtoA (Relative To Absolute) in showa.asm within Frenzy's source code

CALCULATE_MAGIC_IMAGE_RAM_ADDRESS:
29A3: 3A 79 43    ld   a,($4379)             ; read FLIP	
29A6: B7          or   a                     ; set zero flag if playing on upright cabinet  
29A7: 3E 07       ld   a,$07                 ; magic RAM control byte 
29A9: 20 15       jr   nz,$29C0 

; playing on upright cabinet
29AB: A5          and  l
29AC: B0          or   b                     ; combine with magic image RAM control bits  
29AD: D3 4B       out  ($4B),a               ; write to magicram_control_w
29AF: CB 3C       srl  h                     ; divide HL..
29B1: CB 1D       rr   l
29B3: CB 3C       srl  h
29B5: CB 1D       rr   l
29B7: CB 3C       srl  h
29B9: CB 1D       rr   l                     ; .. by 8 
29BB: 01 00 64    ld   bc,$6400              ; start of magic image RAM
29BE: 09          add  hl,bc                 ; now HL = image RAM address to return
29BF: C9          ret

; Playing on cocktail cabinet
29C0: A5          and  l
29C1: B0          or   b
29C2: CB DF       set  3,a                   ; set horizontal flip bit
29C4: D3 4B       out  ($4B),a               ; write to magicram_control_w
29C6: CB 3C       srl  h
29C8: CB 1D       rr   l
29CA: CB 3C       srl  h
29CC: CB 1D       rr   l
29CE: CB 3C       srl  h
29D0: CB 1D       rr   l
29D2: 44          ld   b,h
29D3: 4D          ld   c,l
29D4: 21 FF 7F    ld   hl,$7FFF
29D7: B7          or   a
29D8: ED 42       sbc  hl,bc
29DA: C9          ret



;
; Plot a single character to the screen
;
; Expects:
; A = magic image RAM control byte
; C = ordinal of character to print
; DE = pointer to address in magic image RAM
;
; See also: SHOWC in showa.asm within Frenzy's source code.

PRINT_CHAR:
29DB: E5          push hl
29DC: 21 00 00    ld   hl,$0000
29DF: 06 00       ld   b,$00
29E1: 09          add  hl,bc                 ; HL = C
29E2: 29          add  hl,hl
29E3: 29          add  hl,hl
29E4: 29          add  hl,hl
29E5: 09          add  hl,bc                 ; HL = C * 9 
29E6: 01 1E 2F    ld   bc,$2F1E
29E9: 09          add  hl,bc                 ; now HL = pointer to start of character bitmap (9 bytes)

29EA: D5          push de
29EB: F5          push af
29EC: EB          ex   de,hl                 ; now DE = pointer to character set bitmap data, HL = pointer to magic image RAM
29ED: 3A 79 43    ld   a,($4379)             ; read FLIP	
29F0: B7          or   a                     ; set zero flag if playing on upright cabinet
29F1: 1A          ld   a,(de)                ; read byte from character set bitmap
29F2: 20 26       jr   nz,$2A1A              ; if zero flag is not set, we're on cocktail cabinet, goto $2A1A 

; Upright cabinet character print routine
29F4: B7          or   a                     ; set various flags depending on character set bitmap data
29F5: F2 FC 29    jp   p,$29FC               ; if bit 7 is clear, goto $29FC 

29F8: 01 60 00    ld   bc,$0060
29FB: 09          add  hl,bc                 ; move 3 lines down

29FC: 3E 09       ld   a,$09                 ; each character is 9 pixels high
29FE: 01 1F 00    ld   bc,$001F              ; offset to add to HL to get to next pixel row down
2A01: 08          ex   af,af'
2A02: F1          pop  af                    ; get magic image RAM control byte into A' 
2A03: F5          push af
2A04: F3          di                         ; prevent interrupts while writing to magic RAM
2A05: D3 4B       out  ($4B),a               ; write control byte param to magicram_control_w
2A07: 1A          ld   a,(de)                ; read bitmap data from character set
2A08: E6 7F       and  $7F
2A0A: 13          inc  de
2A0B: 77          ld   (hl),a                ; write 8 pixels to magic RAM
2A0C: 23          inc  hl
2A0D: 36 00       ld   (hl),$00              ; ensure character to right is deleted 
2A0F: FB          ei                         ; re-enable interrupts
2A10: 09          add  hl,bc                 ; bump HL to point to the pixel row below where we will draw next line of our char
2A11: 08          ex   af,af'
2A12: 3D          dec  a                     ; decrement count of pixel rows left to do
2A13: C2 01 2A    jp   nz,$2A01              ; repeat until all rows of character have been drawn
2A16: F1          pop  af
2A17: D1          pop  de
2A18: E1          pop  hl
2A19: C9          ret

; Cocktail character print routine
; A = byte from character set bitmap data
2A1A: B7          or   a
2A1B: F2 22 2A    jp   p,$2A22
2A1E: 01 A0 FF    ld   bc,$FFA0
2A21: 09          add  hl,bc
2A22: 3E 09       ld   a,$09
2A24: 01 E1 FF    ld   bc,$FFE1
2A27: 08          ex   af,af'
2A28: F1          pop  af
2A29: F5          push af
2A2A: F3          di
2A2B: D3 4B       out  ($4B),a               ; write to magicram_control_w
2A2D: 1A          ld   a,(de)
2A2E: E6 7F       and  $7F
2A30: 13          inc  de
2A31: 77          ld   (hl),a
2A32: 2B          dec  hl
2A33: 36 00       ld   (hl),$00
2A35: FB          ei
2A36: 09          add  hl,bc
2A37: 08          ex   af,af'
2A38: 3D          dec  a
2A39: C2 27 2A    jp   nz,$2A27
2A3C: F1          pop  af
2A3D: D1          pop  de
2A3E: E1          pop  hl
2A3F: C9          ret


; Print a BCD-encoded value with specified number of digits at given X, Y coordinates.    
; 
; Expects:
; B = number of digits to print
; HL = pointer to BCD digits to print
; D = Y coordinate
; E = X coordinate
;
; See also: SHOWN (Show Number) in showa.asm within Frenzy's source code

PRINT_DIGITS:
2A40: C5          push bc
2A41: 06 00       ld   b,$00                 ; magic image RAM control bits
2A43: EB          ex   de,hl                 ; now HL = Y/X coordinates
2A44: CD A3 29    call $29A3                 ; call CALCULATE_MAGIC_IMAGE_RAM_ADDRESS
2A47: EB          ex   de,hl                 ; now DE = magic image RAM address
2A48: 08          ex   af,af'                ; Save magic register byte
2A49: C1          pop  bc

SHOWO:
; bit 0 of C determines if numbers will be padded out to B digits length with leading spaces if required.
; if bit 0 is set, then padding is disabled (see $2A65) 
2A4A: CB 81       res  0,c                   ; clear bit 0 of c, number will be padded with leading spaces 

2A4C: 78          ld   a,b                   ; load A with number of digits to print
2A4D: 3D          dec  a                     ; zero flag will now be set if number of digits to print is 1          
2A4E: 20 02       jr   nz,$2A52              ; if number of digits to print != 1, goto $2A52

; we have 1 digit left to print
2A50: CB C1       set  0,c                   ; disable "pad with leading spaces" 
2A52: 7E          ld   a,(hl)                ; read BCD number

2A53: CB 40       bit  0,b                   ; is the index of the digit to print odd?
2A55: 20 09       jr   nz,$2A60              ; yes, goto $2A60

; index of digit is even. This means we need to shift the upper nibble to the lower nibble
2A57: CB 3F       srl  a                     ; move upper nibble..
2A59: CB 3F       srl  a
2A5B: CB 3F       srl  a
2A5D: CB 3F       srl  a                     ; ..to lower nibble
2A5F: 2B          dec  hl                    ; decrement hl to compensate for following inc hl instruction. We want to read
                                             ; the lower nibble from the same byte next time

2A60: 23          inc  hl
2A61: E6 0F       and  $0F                   ; test if end result is 0
2A63: 20 08       jr   nz,$2A6D              ; if not, then we disable the "pad with leading spaces" flag.

2A65: CB 41       bit  0,c                   ; test "disable pad with leading spaces" bit
2A67: 20 06       jr   nz,$2A6F              ; if bit is set, padding is disabled, goto $2A6F  

; print a leading space character to pad the number out
2A69: 3E 20       ld   a,$20                 ; ordinal of SPACE character
2A6B: 18 0A       jr   $2A77

; set the "disable pad with leading spaces" flag. Its time to start printing numbers! 
2A6D: CB C1       set  0,c

; convert numeric value in A to an ASCII digit, e.g.  0 -> '0' (48 decimal in ASCII)
2A6F: C6 30       add  a,$30                 ; convert A to ASCII ordinal  
2A71: FE 3A       cp   $3A
2A73: 38 02       jr   c,$2A77               ; if A< 58 decimal its fine to print as is, goto $2A77 
2A75: C6 07       add  a,$07

; A= ordinal of character to print
2A77: E5          push hl
2A78: C5          push bc
2A79: 4F          ld   c,a                   ; load C with ordinal of character to print
2A7A: 08          ex   af,af'                ; Restore magic register byte
2A7B: CD DB 29    call $29DB                 ; call PRINT_CHAR
2A7E: 08          ex   af,af'
2A7F: C1          pop  bc
2A80: E1          pop  hl
2A81: 3A 79 43    ld   a,($4379)             ; read FLIP	
2A84: B7          or   a                     ; set zero flag if playing on upright cabinet
2A85: 20 03       jr   nz,$2A8A              ; if zero flag is not set, we're on cocktail cabinet, goto $2A8A 

; upright cabinet 
2A87: 13          inc  de                    
2A88: 18 01       jr   $2A8B

; cocktail cabinet setup
2A8A: 1B          dec  de

; called for both upright & cocktail
2A8B: 10 BF       djnz $2A4C
2A8D: C9          ret



;
; Initialise Evil Otto.
;
; TODO: Analysis in progress, comments incomplete.
;
; See also:
; function SUPER in super.asm within Frenzy's source code.

2A8E: C5          push bc
2A8F: D1          pop  de
2A90: CD 0E 20    call $200E
2A93: CD 22 1E    call $1E22                 ; call CREATE_JOB
2A96: DD E5       push ix
2A98: CD 59 1E    call $1E59                 ; call JOB_BOOTSTRAP → enters runner
2A9B: DD E1       pop  ix

; Look at the player's current position in order to calculate Otto's start position.
; We do not want Otto to start too close to the player. 
2A9D: 2A 47 43    ld   hl,($4347)            ; load MAN_X into L and MAN_Y into H
2AA0: 7D          ld   a,l                   ; get MAN_X into A
2AA1: FE 18       cp   $18
2AA3: 30 04       jr   nc,$2AA9              ; if A>= then goto
2AA5: 2E 02       ld   l,$02
2AA7: 18 06       jr   $2AAF

2AA9: FE E6       cp   $E6
2AAB: 38 02       jr   c,$2AAF               ; if A< then goto
2AAD: 2E F8       ld   l,$F8

2AAF: 7C          ld   a,h                   ; get MAN_Y into A
2AB0: FE B4       cp   $B4
2AB2: 38 02       jr   c,$2AB6
2AB4: 26 A0       ld   h,$A0

2AB6: DD 75 07    ld   (ix+$07),l            ; set VECTOR.X
2AB9: DD 74 09    ld   (ix+$09),h            ; set VECTOR.Y

; Set countdown before Evil Otto appears.
; OTTO_TIME = ROBOT_SPEED + RSAVED + RBOLTS 
2ABC: 3A 4C 43    ld   a,($434C)             ; read ROBOT_SPEED
2ABF: 47          ld   b,a
2AC0: 3A 72 43    ld   a,($4372)             ; read RSAVED 
2AC3: 80          add  a,b
2AC4: 47          ld   b,a
2AC5: 3A 4B 43    ld   a,($434B)             ; read RBOLTS
2AC8: 80          add  a,b
2AC9: 32 4E 43    ld   ($434E),a             ; set OTTO_TIME
2ACC: DD E5       push ix
2ACE: 3E 28       ld   a,$28                 ; delay
2AD0: CD 6D 1E    call $1E6D                 ; call ACTIVATE_HEAD_JOB
2AD3: DD E1       pop  ix

; Wait for OTTO_TIME to count down to 0. 
2AD5: 21 4E 43    ld   hl,$434E              ; load HL with address of OTTO_TIME
2AD8: 35          dec  (hl)                 
2AD9: 20 F1       jr   nz,$2ACC

; time for Otto to appear!!!
2ADB: CD DE 2B    call $2BDE                 ; say "INTRUDER ALERT! INTRUDER ALERT!"
2ADE: 21 0B 12    ld   hl,$120B              ; address of Otto's pattern table    
2AE1: DD 75 0A    ld   (ix+$0a),l            ; set VECTOR.D.P.L            
2AE4: DD 74 0B    ld   (ix+$0b),h            ; set VECTOR.D.P.H
2AE7: DD 36 0C 01 ld   (ix+$0c),$01          ; set VECTOR.TIME  
2AEB: DD 36 0D 02 ld   (ix+$0d),$02          ; set VECTOR.TPRIME
2AEF: DD 36 00 06 ld   (ix+$00),$06          ; set VECTOR.Status
2AF3: AF          xor  a
2AF4: CD 39 2B    call $2B39                 ; call SETDIR
2AF7: DD E5       push ix
2AF9: C5          push bc
2AFA: 3E 28       ld   a,$28
2AFC: CD 6D 1E    call $1E6D                 ; call ACTIVATE_HEAD_JOB
2AFF: C1          pop  bc
2B00: DD E1       pop  ix

; Set Otto's initial direction
2B02: FD 2A 76 08 ld   iy,($0876)            ; load IY with contents of MAN_PTR
; IX = pointer to Otto's VECTOR structure
; IY = pointer to player's VECTOR structure
2B06: C5          push bc

; Need Otto move horizontally to reach the player?
2B07: FD 7E 07    ld   a,(iy+$07)            ; read player VECTOR.P.X (X pos)
2B0A: C6 02       add  a,$02
2B0C: DD 96 07    sub  (ix+$07)              ; subtract from Otto's VECTOR.P.X (X pos)
2B0F: 57          ld   d,a                   ; D = horizontal distance in pixels from Otto to player
2B10: 06 00       ld   b,$00                 ; B will hold DURL bits for horizontal movement 
2B12: 28 06       jr   z,$2B1A
2B14: 06 01       ld   b,$01                 ; DURL bit for LEFT                  
2B16: 38 02       jr   c,$2B1A
2B18: 06 02       ld   b,$02                 ; DURL bit for RIGHT

; Need otto move vertically to reach the player?
2B1A: FD 7E 09    ld   a,(iy+$09)            ; read player VECTOR.P.Y (Y pos)
2B1D: DD 96 09    sub  (ix+$09)              ; subtract from Otto's VECTOR.P.Y (Y pos)
2B20: 5F          ld   e,a                   ; E = vertical distance in pixels from Otto to player  
2B21: 0E 00       ld   c,$00                 ; C will hold DURL bits for vertical movement
2B23: 28 06       jr   z,$2B2B               ; If Otto is parallel to player on Y axis, goto $2B2B  
2B25: 0E 04       ld   c,$04                 ; DURL bit for UP
2B27: 38 02       jr   c,$2B2B
2B29: 0E 08       ld   c,$08                 ; DURL bit for DOWN

; B & C now hold DURL bits, which, when combined, indicate what direction Otto needs to move
; to reach the player quickest.
2B2B: 78          ld   a,b
2B2C: 81          add  a,c                   ; Combine DURL bits held in B & C.
2B2D: C1          pop  bc
2B2E: CD 39 2B    call $2B39                 ; call SETDIR
2B31: 3E 04       ld   a,$04
2B33: CD 54 2B    call $2B54
2B36: C3 02 2B    jp   $2B02

;
; Set the direction of a VECTOR.
;
; Expects: 
; A = Direction (DURL bits)   
; C = Previous direction tracker  
; IX = pointer to a VECTOR structure
;
; Returns: C = new direction if changed
;
; If direction has changed, falls through to SET_VELOCITY.
; See also: SETDIR in super.asm within Frenzy's source code.

SETDIR:
2B39: E6 0F       and  $0F                   ; mask in DURL bits
2B3B: B9          cp   c                     ; has direction of VECTOR changed?
2B3C: C8          ret  z                     ; no, we don't need to update vector in SET_VELOCITY below. 


;
; Set X and Y velocities for a VECTOR.
; 
; Expects:
; A = DURL Direction bits (bit 0 set: left, 1: right, 2: up, 3:down)
; IX = pointer to VECTOR structure 
;
; Returns:
; DE = offset to be used to read from a direction-based table, such as VELOCITY_TABLE or PLAYER_ANIMATION_TABLE
;
; Remarks:
; See also: SETVXY in super.asm within Frenzy's source code

SET_VELOCITY:
2B3D: 4F          ld   c,a
2B3E: 06 00       ld   b,$00                 ; Extend A into BC (meaning, BC = A)
2B40: 50          ld   d,b                   ; Set d to 0
2B41: 21 42 20    ld   hl,$2042              ; load HL with address of DIRECTION_OFFSET_TABLE
2B44: 09          add  hl,bc                 ; now HL = pointer to entry in table   
2B45: 5E          ld   e,(hl)                ; read offset byte from DIRECTION_OFFSET_TABLE table. Now DE = offset

; DE is now zero-extended offset read from DIRECTION_OFFSET_TABLE. 
; Now set VECTOR velocity fields V.X and V.Y from VELOCITY_TABLE velocity lookup table  
2B46: 21 19 25    ld   hl,$2519              ; load HL with address of VELOCITY_TABLE            
2B49: 19          add  hl,de                 ; now HL = pointer to entry in VELOCITY_TABLE
2B4A: 7E          ld   a,(hl)                ; read X velocity from table 
2B4B: 23          inc  hl
2B4C: DD 77 06    ld   (ix+$06),a            ; set VECTOR.V.X (X velocity)
2B4F: 7E          ld   a,(hl)                ; read Y velocity from table
2B50: DD 77 08    ld   (ix+$08),a            ; set VECTOR.V.Y (Y velocity)
2B53: C9          ret


;
; Set timer item delay and schedule job, then check
; if the VECTOR at IX has been hit.
;
; Expects:
; A = delay value
; IX = pointer to VECTOR structure
;
; Returns: Zero flag set if STATUS_BIT_HIT not set

2B54: 2A 72 08    ld   hl,($0872)
2B57: 36 82       ld   (hl),$82
2B59: 23          inc  hl
2B5A: 77          ld   (hl),a
2B5B: DD E5       push ix
2B5D: E5          push hl
2B5E: C5          push bc
2B5F: CD 78 1E    call $1E78                 ; call STOP_JOB → yield to runner
2B62: C1          pop  bc
2B63: E1          pop  hl
2B64: DD E1       pop  ix
2B66: DD CB 00 7E bit  7,(ix+$00)
2B6A: C9          ret


;
; Select random sentences from a speech table and insert them into a speech buffer.
; VOICE_PC can then be set to point to this buffer, and The NMI handler @ $1748 will 
; make the robots say the words represented by the speech bytes stored in the buffer.
;
; Expects:
; HL = pointer to a speech table (see ROBOT_TARGET_SPEECH_TABLE @ $2C25 for an example)
; DE = pointer to buffer in RAM that will hold result 
; B = number of entries ("sentences") held in table at HL
;
; The last speech byte for each "sentence" in the table must have bit 7 set.
;
; Algorithm:
; Calculate a random number between 0..15. Let's call it RAND.
; Subtract the value in B from RAND until the result is a negative number. 
; Add B to negative result to make it positive again. Call this value SENTENCENUMBER
; Scan memory from HL for exactly SENTENCENUMBER bytes that have bit 7 set. Or, put another way,
; skip <SENTENCENUMBER> number of sentences in the table.
; 
; HL then points to a sequence of speech bytes. The last speech byte in the sequence will have bit 7 set, as a "marker" bit.
; From HL: 
;   read byte from HL.
;   and byte with $7F to remove bit 7.
;   write byte to speech buffer.  
;   increment HL and DE. 
;   if bit 7 of byte read was not set, continue loop.
;   else, this is the last speech byte in the word sequence, so exit routine.

WRITE_RANDOM_SENTENCE_TO_BUFFER:
2B6B: D5          push de
2B6C: CD 78 26    call $2678                 ; call RANDOM
2B6F: D1          pop  de
; A now  = random number.
; Looks like this is just shaking A up a bit more - adding a wee bit more "randomness" perhaps?. I can't discern any other logic.
2B70: 07          rlca
2B71: 07          rlca
2B72: EE 09       xor  $09
2B74: E6 0F       and  $0F
; A is now a pseudo-random number between 0 and 15 decimal (RAND in docs above)
; Keep subtracting B from A until A is a negative number, then add B to A again to make it positive.
; A is then the sentence number (SENTENCENUMBER) to read from the speech table pointed to by HL
2B76: 90          sub  b
2B77: FA 7C 2B    jp   m,$2B7C               ; if A is negative, goto $2B7C
2B7A: 18 FA       jr   $2B76                 ; otherwise, keep subtracting B from A.

2B7C: 80          add  a,b                   ; Ensure A is a positive number
2B7D: 28 0C       jr   z,$2B8B               ; if A is 0 then we're selecting the first sentence from the speech table. HL already points to it,
                                             ; so no need to go into a loop to find the word.

; We need to loop through the speech table to find the correct sentence.
; A is the sentence number (SENTENCENUMBER) to read from the speech table pointed to by HL
2B7F: 47          ld   b,a                   ; preserve A in B, as A will be used for other purposes

; This code will read from and increment HL until it reads exactly <B> bytes with bit 7 set
; In Plain English - look for <B> sentence terminating bytes.
2B80: 7E          ld   a,(hl)                ; read a byte from the speech template
2B81: CB 7F       bit  7,a                   ; test if bit 7 is set
2B83: 20 03       jr   nz,$2B88              ; if bit 7 is set, decrement B, and if B!=0 goto $2b80  
2B85: 23          inc  hl                    ; otherwise ignore this byte
2B86: 18 F8       jr   $2B80

; Increment HL and decrement B. Loop until B==0.
2B88: 23          inc  hl
2B89: 10 F5       djnz $2B80                 

; HL now points to the sentence we want to copy into the SPEECH_BUFFER 
; DE = pointer to a byte in SPEECH_BUFFER
; The last byte of the word in the sentence has bit 7 set.
2B8B: 7E          ld   a,(hl)                ; read byte from sentence
2B8C: 47          ld   b,a                   ; preserve in B
2B8D: E6 7F       and  $7F                   ; mask out bit 7 and preserve rest. 
2B8F: 12          ld   (de),a                ; write speech byte into SPEECH_BUFFER   
2B90: 13          inc  de                    ; bump DE to next byte of speech buffer
2B91: CB 78       bit  7,b                   ; test bit 7 - is the "end of sentence" marker set?
2B93: 23          inc  hl                    ; bump HL to point to next byte in speech template
2B94: 28 F5       jr   z,$2B8B               ; if bit 7 is not set, goto $2B8B to read another speech byte.
2B96: C9          ret


;
; Generates random speech for robots to say
;
; Speech is processed in NMI_HANDLER @ $1748
;
; Remarks: I could not find a corresponding routine in Frenzy's source code. 
; I guess this makes sense, as Frenzy is much less talkative than Berzerk. 
; Nevertheless, I enjoyed working out the logic here - very satisfying to crack it by oneself.
;

GENERATE_ROBOT_SPEECH:
2B97: 2A 98 08    ld   hl,($0898)            ; read VOICE_PC
2B9A: 7C          ld   a,h
2B9B: B5          or   l                     ; if H is 0 and L is 0 then A will be 0. HL is thus NULL and zero flag will be set.
2B9C: C0          ret  nz                    ; exit if VOICE_PC is not a null pointer

; Time for the robots to speak?
2B9D: 21 9B 08    ld   hl,$089B              ; Load HL with address of TALK_TIMER
2BA0: 35          dec  (hl)                  ; decrement countdown. 
2BA1: C0          ret  nz                    ; if counter hasn't hit zero, it's not time for the robots to talk, exit

; Set countdown before robots next speak 
2BA2: CD 78 26    call $2678                 ; call RANDOM
2BA5: 07          rlca
2BA6: 07          rlca
2BA7: E6 0C       and  $0C                   
2BA9: C6 04       add  a,$04                 ; ensure timer is 4 minimum, otherwise the robots will speak too often                    
2BAB: 77          ld   (hl),a                ; set TALK_TIMER

; OK, its time for the robots to speak. Generate a sentence in SPEECH_BUFFER.
2BAC: 21 18 09    ld   hl,$0918              ; load HL with address of SPEECH_BUFFER
;
; Algorithm:
; Select voice pitch at random.
; Let firstWord = a random word from ROBOT_FIRST_WORD_SPEECH_TABLE.
; Let LastWords = a random sentence chosen from ROBOT_TARGET_SPEECH_TABLE.
; Let sentenceToSpeak = firstWord + LastWords.
; Store sentenceToSpeak in SPEECH_BUFFER
; Say sentenceToSpeak.
;
; Example: if firstWord is "GET" and lastwords are "THE HUMANOID", create "GET THE HUMANOID" in SPEECH_BUFFER then speak it
;

; first byte in buffer is pitch for voice. Generate this at random
2BAF: CD 78 26    call $2678                 ; call RANDOM
2BB2: E6 1F       and  $1F                   ; ensure number is between 0..31 decimal
2BB4: F6 60       or   $60
2BB6: 77          ld   (hl),a                ; first byte = set pitch/ speed
2BB7: 23          inc  hl                    ; bump HL to next byte in SPEECH_BUFFER
2BB8: EB          ex   de,hl                 

; Get a random byte from ROBOT_FIRST_WORD_SPEECH_TABLE speech table and insert it into SPEECH_BUFFER.  
2BB9: 21 2C 2C    ld   hl,$2C2C              ; load HL with address of ROBOT_FIRST_WORD_SPEECH_TABLE
2BBC: 06 06       ld   b,$06                 ; there are 6 words available to choose from in the table
2BBE: CD 6B 2B    call $2B6B                 ; call WRITE_RANDOM_SENTENCE_TO_BUFFER - just writes a single spoken word.

; Write the bytes representing the word(s) comprising the target of the sentence (the player!) to SPEECH_BUFFER. 
2BC1: 21 25 2C    ld   hl,$2C25              ; load HL with address of ROBOT_TARGET_SPEECH_TABLE speech table
2BC4: 06 04       ld   b,$04                 ; There are 4 word sequences in the table to choose from
2BC6: 3A 9A 08    ld   a,($089A)             ; read IS_CHICKEN flag
2BC9: B7          or   a                     ; test if flag is set
2BCA: 20 03       jr   nz,$2BCF              ; if flag is set, the player's a chicken, goto $2BCF

; The player's not a chicken, but HL currently points to a list of word sequences that include "THE CHICKEN"
; We can't call the player a chicken if they aren't, so adjust HL to skip over the words "THE CHICKEN" in 
; ROBOT_TARGET_SPEECH_TABLE table.
2BCC: 23          inc  hl
2BCD: 23          inc  hl                    ; now HL points to "IT" in the ROBOT_TARGET_SPEECH_TABLE speech table.
2BCE: 05          dec  b                     ; decrement B to 3, which means "only 3 word sequences left to choose from"

; Code executes whether chicken or not ;) 
2BCF: CD 6B 2B    call $2B6B                 ; call WRITE_RANDOM_SENTENCE_TO_BUFFER

2BD2: EB          ex   de,hl
2BD3: 36 47       ld   (hl),$47              ; write "end of sentence" byte required by speech synthesiser
2BD5: 23          inc  hl
2BD6: 36 FF       ld   (hl),$FF              ; write terminator byte for NMI routine to read (see $1755 which detects it)
2BD8: 21 18 09    ld   hl,$0918              ; load HL with address of SPEECH_BUFFER
2BDB: C3 1B 2C    jp   $2C1B                 ; jump to TALK


SAY_INTRUDER_ALERT_INTRUDER_ALERT:
2BDE: 21 4A 2C    ld   hl,$2C4A              ; load HL with address of INTRUDER_ALERT_INTRUDER_ALERT speech data
2BE1: C3 1B 2C    jp   $2C1B                 ; jump to TALK



;
; Called when exiting a room after killing all robots (with exception of Otto, of course).
;
;

TRY_SPEAK_ON_PLAYER_LEAVING_ROOM:
; Has the player killed all of the robots?
2BE4: 3A 71 43    ld   a,($4371)             ; read RCOUNT
2BE7: B7          or   a                     ; test if zero.
2BE8: 20 29       jr   nz,$2C13              ; if there's still some robots left in the maze, player's a chicken. Goto SAY_CHICKEN_FIGHT_LIKE_A_ROBOT

; Player's not a chicken. 
; Say "THE HUMANOID MUST NOT ESCAPE" or "THE INTRUDER MUST NOT ESCAPE"
; First, select pitch of robot voice.
2BEA: 21 18 09    ld   hl,$0918              ; load HL with address of SPEECH_BUFFER
2BED: CD 78 26    call $2678                 ; call RANDOM
2BF0: E6 07       and  $07                   ; clamp number to 0..7
2BF2: F6 70       or   $70
2BF4: 77          ld   (hl),a                ; first byte = set pitch/ speed   
2BF5: 23          inc  hl                    ; bump to next byte in SPEECH_BUFFER
2BF6: EB          ex   de,hl

; Add bytes representing "THE HUMANOID" or "THE INTRUDER" and insert them into SPEECH_BUFFER.  
2BF7: 21 28 2C    ld   hl,$2C28              ; point to the second word sequence, "THE HUMANOID", within ROBOT_TARGET_SPEECH_TABLE
2BFA: 06 02       ld   b,$02                 ; 2 remaining word sequences to choose from in the list
2BFC: CD 6B 2B    call $2B6B                 ; call WRITE_RANDOM_SENTENCE_TO_BUFFER

; Append MUST NOT ESCAPE
2BFF: 21 32 2C    ld   hl,$2C32              ; load HL with address of MUST_NOT_ESCAPE_SPEECH_TABLE
2C02: 06 01       ld   b,$01                 ; MUST NOT ESCAPE is the sole word sequence available, so just 1 to choose from.
2C04: CD 6B 2B    call $2B6B                 ; call WRITE_RANDOM_SENTENCE_TO_BUFFER
2C07: EB          ex   de,hl
2C08: 36 44       ld   (hl),$44              ; write "end of sentence" byte required by speech synthesiser
2C0A: 23          inc  hl
2C0B: 36 FF       ld   (hl),$FF              ; write terminator byte for NMI routine to read (see $1755 which detects it)
2C0D: 21 18 09    ld   hl,$0918              ; load HL with address of SPEECH_BUFFER
2C10: AF          xor  a                     ; clear the IS_CHICKEN flag
2C11: 18 05       jr   $2C18                 ; jump to tTALK


;
;  The player's exited the room. Taunt them with "Chicken, fight like a robot" (!)
;
;
SAY_CHICKEN_FIGHT_LIKE_A_ROBOT:
2C13: 21 35 2C    ld   hl,$2C35              ; load HL with address of CHICKEN_FIGHT_LIKE_A_ROBOT_SPEECH_BYTES speech data
2C16: 3E FF       ld   a,$FF                 ; This will make the player a chicken in the robot's eyes!

2C18: 32 9A 08    ld   ($089A),a             ; set IS_CHICKEN flag

; Set VOICE_PC - the NMI handler 
; HL = pointer to speech data
TALK:
2C1B: 22 98 08    ld   ($0898),hl            ; update VOICE_PC
2C1E: C9          ret


SAY_GOT_THE_HUMANOID_GOT_THE_INTRUDER:
2C1F: 21 40 2C    ld   hl,$2C40              ; load HL with address of GOT_THE_HUMANOID_GOT_THE_INTRUDER_SPEECH_BYTES speech data
2C22: AF          xor  a                     ; clear the IS_CHICKEN flag
2C23: 18 F3       jr   $2C18                 



;
; Used in dynamically generating robot speech
;
;
; There are 4 sentences available in this speech table. They are as follows:
;
; THE CHICKEN
; IT
; THE HUMANOID
; THE INTRUDER
;
; Note how bit 7 of the CHICKEN, IT, HUMANOID, INTRUDER speech bytes are set; this bit marks the end of a sentence.
; (See docs for WRITE_RANDOM_SENTENCE_TO_BUFFER @ $2B6B)
;
; See also:
; docs @ $2BAC for more info.
ROBOT_TARGET_SPEECH_TABLE:
2C25: 
    0A                                       ; THE              
    98                                       ; CHICKEN  
    8C                                       ; IT       
    0A                                       ; THE    
    8F                                       ; HUMANOID     
    0A                                       ; THE
    92                                       ; INTRUDER                        ; bit 7 set to terminate the sentence    

;
; ROBOT_FIRST_WORD_SPEECH_TABLE contains a list of words that begin a sentence spoken by the robots.
; Note how bit 7 of each byte is set; this stops WRITE_RANDOM_SENTENCE_TO_BUFFER @ $2B6B from taking more than a single word from this table.
; 
; See also:
; $2BB9 within GENERATE_ROBOT_SPEECH
; 

ROBOT_FIRST_WORD_SPEECH_TABLE:
2C2C: 
    86                                       ; GET          
    83                                       ; CHARGE    
    82                                       ; ATTACK  
    95                                       ; DESTROY    
    85                                       ; SHOOT    
    81                                       ; KILL    

;
; Used to generate the sentence "<pick a sentence from ROBOT_TARGET_SPEECH_TABLE above> MUST NOT ESCAPE"
; Note how bit 7 of ESCAPE (the last word) is set.
MUST_NOT_ESCAPE_SPEECH_TABLE:
2C32: 
    16                                       ; MUST   
    17                                       ; NOT 
    94                                       ; ESCAPE     



CHICKEN_FIGHT_LIKE_A_ROBOT_SPEECH_BYTES:
2C35: 
    73                                       ; Pitch of speech
    18                                       ; CHICKEN 
    47       
    1B                                       ; "A" - TODO: why is this here? Wonder if its silent just to introduce a delay?       
    73          
    19                                       ; FIGHT 
    1A                                       ; LIKE
    1B                                       ; A
    1C                                       ; ROBOT
    47          
    FF                                       ; Terminator byte

GOT_THE_HUMANOID_GOT_THE_INTRUDER_SPEECH_BYTES:
2C40: 
    7B                                       ; Pitch of speech
    04                                       ; GOT         
    0A                                       ; THE
    0F                                       ; HUMANOID
    7D          
    04                                       ; GOT
    0A                                       ; THE
    12                                       ; INTRUDER
    47          
    FF                                       ; Terminator byte

INTRUDER_ALERT_INTRUDER_ALERT:
2C4A: 
    7B                                       ; Pitch of speech   
    12                                       ; INTRUDER                        
    08                                       ; ALERT
    12                                       ; INTRUDER
    08                                       ; ALERT
    47          
    FF                                       ; Terminator byte


;
; Handle game over / high score processing.
; Updates CMOS total score with player's final score,
; then inserts the score into the high score table
; if it qualifies.
;

2C51: CD 34 23    call $2334                 ; call GET_PLAYER_SCORE_PTR
2C54: E5          push hl
2C55: 23          inc  hl
2C56: 23          inc  hl
2C57: EB          ex   de,hl
2C58: 21 C4 08    ld   hl,$08C4              ; load HL with address of CMOS_TOTAL_SCORE
2C5B: 0E 03       ld   c,$03
2C5D: 06 0C       ld   b,$0C
2C5F: 1A          ld   a,(de)
2C60: E6 0F       and  $0F
2C62: 28 06       jr   z,$2C6A
2C64: CD B3 2D    call $2DB3                 ; call INCREMENT_BY_1
2C67: 3D          dec  a
2C68: 20 FA       jr   nz,$2C64
2C6A: 05          dec  b
2C6B: 1A          ld   a,(de)
2C6C: E6 F0       and  $F0
2C6E: 28 07       jr   z,$2C77
2C70: CD B3 2D    call $2DB3                 ; call INCREMENT_BY_1
2C73: D6 10       sub  $10
2C75: 20 F9       jr   nz,$2C70
2C77: 05          dec  b
2C78: 1B          dec  de
2C79: 0D          dec  c
2C7A: 20 E3       jr   nz,$2C5F
2C7C: 0E 0A       ld   c,$0A
2C7E: 11 02 43    ld   de,$4302
2C81: E1          pop  hl
2C82: E5          push hl
2C83: D5          push de
2C84: 06 03       ld   b,$03
2C86: 1A          ld   a,(de)
2C87: BE          cp   (hl)
2C88: 38 11       jr   c,$2C9B
2C8A: 20 04       jr   nz,$2C90
2C8C: 13          inc  de
2C8D: 23          inc  hl
2C8E: 10 F6       djnz $2C86
2C90: D1          pop  de
2C91: 21 06 00    ld   hl,$0006
2C94: 19          add  hl,de
2C95: EB          ex   de,hl
2C96: E1          pop  hl
2C97: 0D          dec  c
2C98: 20 E8       jr   nz,$2C82
2C9A: C9          ret
2C9B: D1          pop  de
2C9C: D5          push de
2C9D: 06 00       ld   b,$00
2C9F: 0D          dec  c
2CA0: 28 14       jr   z,$2CB6
2CA2: 21 00 00    ld   hl,$0000
2CA5: 09          add  hl,bc
2CA6: 29          add  hl,hl
2CA7: 09          add  hl,bc
2CA8: 29          add  hl,hl
2CA9: E5          push hl
2CAA: 19          add  hl,de
2CAB: 2B          dec  hl
2CAC: 54          ld   d,h
2CAD: 5D          ld   e,l
2CAE: 01 06 00    ld   bc,$0006
2CB1: 09          add  hl,bc
2CB2: EB          ex   de,hl
2CB3: C1          pop  bc
2CB4: ED B8       lddr
2CB6: D1          pop  de
2CB7: E1          pop  hl
2CB8: 06 03       ld   b,$03
2CBA: 7E          ld   a,(hl)
2CBB: 23          inc  hl
2CBC: 12          ld   (de),a
2CBD: 13          inc  de
2CBE: 10 FA       djnz $2CBA
2CC0: EB          ex   de,hl
2CC1: E5          push hl
2CC2: CD 4E 1A    call $1A4E                 ; call CLEAR_SCREEN
2CC5: CD 1C 36    call $361C
2CC8: CD ED 1A    call $1AED                 ; call LTABLE
2CCB: 1B          dec  de
2CCC: 00          nop
2CCD: D8          ret  c
2CCE: 2D          dec  l
2CCF: 3A 00 F4    ld   a,($F400)
2CD2: 2D          dec  l
2CD3: 08          ex   af,af'
2CD4: 06 01       ld   b,$01
2CD6: 21 44 43    ld   hl,$4344
2CD9: CD 4A 2A    call $2A4A
2CDC: CD ED 1A    call $1AED                 ; call LTABLE
2CDF: 93          sub  e
2CE0: 07          rlca
2CE1: 71          ld   (hl),c
2CE2: 2E 13       ld   l,$13
2CE4: 2E CB       ld   l,$CB
2CE6: 2E CD       ld   l,$CD
2CE8: 7B          ld   a,e
2CE9: 29          add  hl,hl
2CEA: 90          sub  b
2CEB: 78          ld   a,b
2CEC: 62          ld   h,d
2CED: 5F          ld   e,a
2CEE: 5F          ld   e,a
2CEF: 5F          ld   e,a
2CF0: 00          nop
2CF1: CD ED 1A    call $1AED                 ; call LTABLE
2CF4: 1B          dec  de
2CF5: 2F          cpl
2CF6: 1B          dec  de
2CF7: 2F          cpl
2CF8: 61          ld   h,c
2CF9: 2F          cpl
2CFA: BC          cp   h
2CFB: 2F          cpl
2CFC: 06 00       ld   b,$00                 ; bits to write to magicram_control_w
2CFE: 21 78 60    ld   hl,$6078
2D01: CD A3 29    call $29A3                 ; call CALCULATE_MAGIC_IMAGE_RAM_ADDRESS
2D04: EB          ex   de,hl
2D05: E1          pop  hl
2D06: E5          push hl
2D07: 06 03       ld   b,$03
2D09: 36 20       ld   (hl),$20
2D0B: 23          inc  hl
2D0C: 10 FB       djnz $2D09
2D0E: E1          pop  hl
2D0F: 3E 1E       ld   a,$1E
2D11: 32 49 43    ld   ($4349),a             ; set DEATHS            
2D14: 06 03       ld   b,$03
2D16: 0E 41       ld   c,$41
2D18: C5          push bc
2D19: 3A 79 43    ld   a,($4379)             ; read FLIP	
2D1C: CD DB 29    call $29DB                 ; call PRINT_CHAR
2D1F: D5          push de
2D20: E5          push hl
2D21: 3E 0F       ld   a,$0F
2D23: CD 6D 1E    call $1E6D                 ; call ACTIVATE_HEAD_JOB
2D26: E1          pop  hl
2D27: D1          pop  de
2D28: C1          pop  bc
2D29: FD CB 00 CE set  1,(iy+$00)
2D2D: FD 36 01 3C ld   (iy+$01),$3C
2D31: FD 7E 01    ld   a,(iy+$01)
2D34: B7          or   a
2D35: 20 0B       jr   nz,$2D42
2D37: 3A 49 43    ld   a,($4349)             ; read DEATHS
2D3A: 3D          dec  a
2D3B: 32 49 43    ld   ($4349),a             ; set DEATHS
2D3E: 28 3A       jr   z,$2D7A
2D40: 18 E7       jr   $2D29

2D42: CD CE 2D    call $2DCE
2D45: CB 67       bit  4,a
2D47: 20 46       jr   nz,$2D8F
2D49: 71          ld   (hl),c
2D4A: C5          push bc
2D4B: 3A 79 43    ld   a,($4379)             ; read FLIP	
2D4E: B7          or   a
2D4F: 01 40 00    ld   bc,$0040
2D52: 28 03       jr   z,$2D57
2D54: 01 C0 FF    ld   bc,$FFC0
2D57: D5          push de
2D58: EB          ex   de,hl
2D59: 09          add  hl,bc
2D5A: EB          ex   de,hl
2D5B: 3A 79 43    ld   a,($4379)             ; read FLIP	
2D5E: F6 90       or   $90
2D60: 0E 5F       ld   c,$5F
2D62: CD DB 29    call $29DB                 ; call PRINT_CHAR
2D65: D1          pop  de
2D66: C1          pop  bc
2D67: 23          inc  hl
2D68: 3A 79 43    ld   a,($4379)             ; read FLIP	
2D6B: B7          or   a
2D6C: 13          inc  de
2D6D: 28 02       jr   z,$2D71
2D6F: 1B          dec  de
2D70: 1B          dec  de
2D71: CD CE 2D    call $2DCE
2D74: CB 67       bit  4,a
2D76: 28 F9       jr   z,$2D71
2D78: 10 9E       djnz $2D18
2D7A: 21 02 43    ld   hl,$4302
2D7D: 11 DC 08    ld   de,$08DC
2D80: 06 1E       ld   b,$1E
2D82: 7E          ld   a,(hl)
2D83: 23          inc  hl
2D84: 12          ld   (de),a
2D85: 13          inc  de
2D86: 07          rlca
2D87: 07          rlca
2D88: 07          rlca
2D89: 07          rlca
2D8A: 12          ld   (de),a
2D8B: 13          inc  de
2D8C: 10 F4       djnz $2D82
2D8E: C9          ret

2D8F: CD CE 2D    call $2DCE
2D92: 2F          cpl
2D93: CB 47       bit  0,a
2D95: 28 04       jr   z,$2D9B
2D97: 3E FF       ld   a,$FF
2D99: 18 07       jr   $2DA2

2D9B: CB 4F       bit  1,a
2D9D: CA 31 2D    jp   z,$2D31
2DA0: 3E 01       ld   a,$01
2DA2: 81          add  a,c
2DA3: FE 40       cp   $40
2DA5: 20 02       jr   nz,$2DA9
2DA7: 3E 5A       ld   a,$5A
2DA9: FE 5B       cp   $5B
2DAB: 20 02       jr   nz,$2DAF
2DAD: 3E 41       ld   a,$41
2DAF: 4F          ld   c,a
2DB0: C3 18 2D    jp   $2D18



;
; Increment a BCD-packed number by 1. 
;
; Expects:
; B = length of number in digits
; HL = pointer to bytes containing digits. Upper nibble of each byte contains digits.  
;
; Remarks:
;
; Algorithm:  
; Working from the last digit to the first:
; 1. Read byte
; 2. Mask in (preserve) upper nibble and treat that as a number from 0-9. 
; 3. Add 1 to that number.
; 4. Update byte with result
; 5. If no carry (ie: result of increment is <=9) then exit
; 6. Otherwise repeat step 1 with byte before last 
;
; See also: ItemInc in books.asm within Frenzy's source code

INCREMENT_BY_1:
2DB3: F5          push af
2DB4: C5          push bc
2DB5: D5          push de
2DB6: E5          push hl
2DB7: 16 00       ld   d,$00                 ; Extend B..
2DB9: 58          ld   e,b                   ; into DE. 
2DBA: 1D          dec  e                     ; Effectively DE is now B-1. 
2DBB: 19          add  hl,de                 ; make HL point to very last digit
2DBC: 0E 10       ld   c,$10
2DBE: 7E          ld   a,(hl)                ; read byte
2DBF: E6 F0       and  $F0                   ; mask in upper nibble
2DC1: 81          add  a,c                   ; add 1 to upper nibble
2DC2: 27          daa                        ; ensure valid BCD
2DC3: 77          ld   (hl),a                ; and write it back
2DC4: 30 03       jr   nc,$2DC9              ; if the operation didn't cause a carry, restore regs and exit
2DC6: 2B          dec  hl
2DC7: 10 F5       djnz $2DBE                 ; repeat until no more digits to process
2DC9: E1          pop  hl
2DCA: D1          pop  de
2DCB: C1          pop  bc
2DCC: F1          pop  af
2DCD: C9          ret

;
; Read controls for the current player.
; Reads P1 if upright cabinet, P2 if cocktail.
;
; Returns: A = control port bits

2DCE: 3A 79 43    ld   a,($4379)             ; read FLIP	
2DD1: B7          or   a
2DD2: DB 48       in   a,($48)               ; read player 1 controls
2DD4: C8          ret  z
2DD5: DB 4A       in   a,($4A)               ; read player 2 controls
2DD7: C9          ret

2DD8: CD 7B 29    call $297B                 ; call PRINT_STRING_297B
2DDB: 
    90          
    20 08                                    ; X/Y coordinates


2DDE:  47 72 61 74 75 6C 69 65 72 65 2C 20 53 70 69 65  Gratuliere, Spie
2DEE:  6C 65 72 20 00 C9 CD 7B 29 90 20 08 46 65 6C 69  ler ...{). .Feli
2DFE:  63 69 74 61 63 69 6F 6E 65 73 20 6A 75 67 61 64  citaciones jugad
2E0E:  6F 72 20 00 C9 CD 7B 29 90 08 20 56 6F 75 73 20  or ...{).. Vous 
2E1E:  61 76 65 7A 20 6A 6F 69 6E 74 20 6C 65 73 20 69  avez joint les i
2E2E:  6D 6D 6F 72 74 65 6C 73 00 CD 7B 29 90 08 30 64  mmortels..{)..0d
2E3E:  75 20 70 61 6E 74 68 65 6F 6E 20 42 45 52 5A 45  u pantheon BERZE
2E4E:  52 4B 2E 00 CD 7B 29 90 08 50 49 6E 73 63 72 69  RK...{)..PInscri
2E5E:  72 65 20 76 6F 73 20 69 6E 69 74 69 61 6C 65 73  re vos initiales
2E6E:  3A 00 C9 CD 7B 29 90 08 20 44 61 73 20 57 61 72  :...{).. Das War
2E7E:  20 65 69 6E 20 52 75 68 6D 76 6F 6C 6C 65 72 20   ein Ruhmvoller 
2E8E:  53 69 65 67 21 00 CD 7B 29 90 08 40 54 72 61 67  Sieg!..{)..@Trag
2E9E:  20 44 65 69 6E 65 6E 20 4E 61 6D 65 6E 20 69 6E   Deinen Namen in
2EAE:  20 64 69 65 00 CD 7B 29 90 08 50 48 65 6C 64 65   die..{)..PHelde
2EBE:  6E 6C 69 73 74 65 20 65 69 6E 21 00 C9 CD 7B 29  nliste ein!...{)
2ECE:  90 04 20 53 65 20 70 75 6E 74 61 6A 65 20 65 73  .. Se puntaje es
2EDE:  74 61 20 65 6E 74 72 65 20 6C 6F 73 20 64 69 65  ta entre los die
2EEE:  7A 00 CD 7B 29 90 08 30 6D 65 6A 6F 72 65 73 2E  z..{)..0mejores.
2EFE:  00 CD 7B 29 90 18 50 45 6E 74 72 65 20 73 75 73  ..{)..PEntre sus
2F0E:  20 69 6E 69 63 69 61 6C 65 73 3A 00 C9 CD 7B 29   iniciales:...{)
2F1E:  90 08 80 4D 6F 76 65 20 73 74 69 63 6B 20 74 6F  ...Move stick to
2F2E:  20 63 68 61 6E 67 65 20 6C 65 74 74 65 72 00 CD   change letter..
2F3E:  7B 29 90 08 90 74 68 65 6E 20 70 72 65 73 73 20  {)...then press 
2F4E:  46 49 52 45 20 74 6F 20 73 74 6F 72 65 20 69 74  FIRE to store it
2F5E:  2E 00 C9 CD 7B 29 90 04 80 50 6F 75 73 73 65 7A  ....{)...Poussez
2F6E:  20 62 61 74 6F 6E 6E 65 74 20 70 6F 75 72 20 76   batonnet pour v
2F7E:  6F 73 00 CD 7B 29 90 04 90 69 6E 69 74 69 61 6C  os..{)...initial
2F8E:  65 73 2E 20 50 6F 75 73 73 65 7A 20 46 49 52 45  es. Poussez FIRE
2F9E:  20 71 75 61 6E 64 00 CD 7B 29 90 04 A0 6C 65 74   quand..{)...let
2FAE:  74 72 65 20 63 6F 72 72 65 63 74 65 00 C9 CD 7B  tre correcte...{
2FBE:  29 90 04 80 4D 6F 76 69 65 6E 64 6F 20 6C 61 20  )...Moviendo la 
2FCE:  70 61 6C 61 6E 63 61 20 70 61 72 61 00 CD 7B 29  palanca para..{)
2FDE:  90 04 90 63 61 6D 62 69 61 72 20 6C 61 73 20 6C  ...cambiar las l
2FEE:  65 74 72 61 73 2C 20 6C 75 65 67 6F 00 CD 7B 29  etras, luego..{)
2FFE:  90 04 A0 61 70 6C 61 73 74 65 20 65 6C 20 62 6F  ...aplaste el bo
300E:  74 6F 6E 20 64 65 20 64 69 73 70 61 72 6F 00 CD  ton de disparo..
301E:  7B 29 90 04 B0 70 61 72 61 20 72 65 74 65 6E 65  {)...para retene
302E:  72 6C 61 73 2E 00 C9 00 3E 41 5D 51 5D 41 3E 00  rlas....>A]Q]A>.

303E: 00          nop
303F: 00          nop
3040: 00          nop
3041: 00          nop
3042: 00          nop
3043: 00          nop
3044: 00          nop
3045: 00          nop
3046: 00          nop
3047: 10 10       djnz $3059
3049: 10 10       djnz $305B
304B: 10 10       djnz $305D
304D: 10 00       djnz $304F
304F: 10 14       djnz $3065
3051: 14          inc  d
3052: 14          inc  d
3053: 00          nop
3054: 00          nop
3055: 00          nop
3056: 00          nop
3057: 00          nop
3058: 00          nop
3059: 14          inc  d
305A: 14          inc  d
305B: 14          inc  d
305C: 7F          ld   a,a
305D: 14          inc  d
305E: 7F          ld   a,a
305F: 14          inc  d
3060: 14          inc  d
3061: 14          inc  d
3062: 14          inc  d
3063: 7F          ld   a,a
3064: 54          ld   d,h
3065: 54          ld   d,h
3066: 7F          ld   a,a
3067: 15          dec  d
3068: 15          dec  d
3069: 7F          ld   a,a
306A: 14          inc  d
306B: 20 51       jr   nz,$30BE
306D: 22 04 08    ld   ($0804),hl
3070: 10 22       djnz $3094
3072: 45          ld   b,l
3073: 02          ld   (bc),a
3074: 00          nop
3075: 18 24       jr   $309B
3077: 28 10       jr   z,$3089
3079: 29          add  hl,hl
307A: 46          ld   b,(hl)
307B: 46          ld   b,(hl)
307C: 39          add  hl,sp
307D: 08          ex   af,af'
307E: 08          ex   af,af'
307F: 08          ex   af,af'
3080: 00          nop
3081: 00          nop
3082: 00          nop
3083: 00          nop
3084: 00          nop
3085: 00          nop
3086: 04          inc  b
3087: 08          ex   af,af'
3088: 10 10       djnz $309A
308A: 10 10       djnz $309C
308C: 10 08       djnz $3096
308E: 04          inc  b
308F: 10 08       djnz $3099
3091: 04          inc  b
3092: 04          inc  b
3093: 04          inc  b
3094: 04          inc  b
3095: 04          inc  b
3096: 08          ex   af,af'
3097: 10 00       djnz $3099
3099: 00          nop
309A: 08          ex   af,af'
309B: 2A 1C 1C    ld   hl,($1C1C)
309E: 2A 08 00    ld   hl,($0008)
30A1: 00          nop
30A2: 00          nop
30A3: 08          ex   af,af'
30A4: 08          ex   af,af'
30A5: 3E 08       ld   a,$08
30A7: 08          ex   af,af'
30A8: 00          nop
30A9: 00          nop
30AA: 80          add  a,b
30AB: 00          nop
30AC: 00          nop
30AD: 00          nop
30AE: 18 18       jr   $30C8
30B0: 08          ex   af,af'
30B1: 10 00       djnz $30B3
30B3: 00          nop
30B4: 00          nop
30B5: 00          nop
30B6: 00          nop
30B7: 3E 00       ld   a,$00
30B9: 00          nop
30BA: 00          nop
30BB: 00          nop
30BC: 00          nop
30BD: 00          nop
30BE: 00          nop
30BF: 00          nop
30C0: 00          nop
30C1: 00          nop
30C2: 00          nop
30C3: 18 18       jr   $30DD
30C5: 00          nop
30C6: 00          nop
30C7: 01 02 04    ld   bc,$0402
30CA: 08          ex   af,af'
30CB: 10 20       djnz $30ED
30CD: 40          ld   b,b


;
; Character "0"
;

30CE: 1C 22 41 41 41 41 41 22 1C 

;
; Character "1"
;

30D7: 08 18 08 08 08 08 08 08 1C          

;
; Character "2"
;

30E0: 3E 41 01 01 3E 40 40 40 7F          

;
; Character "3"
;

30E9: 3E 41 01 01 1E 01 01 41 3E 

;
; Character "4"
;

30F2: 02 06 0A 12 22 7F 02 02 02 

;
; Character "5"
;

30FB: 7F 40 40 40 7E 01 01 41 3E 

;
; Character "6"
;

3104: 3E 41 40 40 7E 41 41 41 3E 

;
; Character "7"
;

310D: 7F 01 02 02 04 04 08 08 08 

;
; Character "8"
;

3116: 3E 41 41 41 3E 41 41 41 3E 

;
; Character "9"
;

311F: 3E 41 41 41 3F 01 01 41 3E 

;
; Character ":"
;

3128: 00 00 00 18 18 00 00 18 18 

;
; Character ";"
;

3131: 98 18 00 00 18 18 08 10 00 

;
; Character "<"
;

313A: 02 04 08 10 20 10 08 04 02 

;
; Character "="
;

3143: 00 00 00 3E 00 3E 00 00 00 

;
; Character ">"
;

314C: 20 10 08 04 02 04 08 10 20  

;
; Character "?"
;

3155: 1C 22 02 02 04 08 08 00 08 

;
; Character "@"
;

315E: 3E 41       
3160: 4F          
3161: 49          
3162: 49          
3163: 4F          
3164: 40          
3165: 40          
3166: 3F          
3167: 3E 41       
3169: 41          
316A: 41          
316B: 7F          
316C: 41          
316D: 41          
316E: 41          
316F: 41          
3170: 7E          
3171: 41          
3172: 41          
3173: 41          
3174: 7E          
3175: 41          
3176: 41          
3177: 41          
3178: 7E          
3179: 3E 41       
317B: 40          
317C: 40          
317D: 40          
317E: 40          
317F: 40          
3180: 41          
3181: 3E 7E       
3183: 41          
3184: 41          
3185: 41          
3186: 41          
3187: 41          
3188: 41          
3189: 41          
318A: 7E          
318B: 7F          
318C: 40          
318D: 40          
318E: 40          
318F: 7C          
3190: 40          
3191: 40          
3192: 40          
3193: 7F          
3194: 7F          
3195: 40          
3196: 40          
3197: 40          
3198: 7C          
3199: 40          
319A: 40          
319B: 40          
319C: 40          
319D: 3E 41       
319F: 40          
31A0: 40          
31A1: 47          
31A2: 41          
31A3: 41          
31A4: 41          
31A5: 3F          
31A6: 41          
31A7: 41          
31A8: 41          
31A9: 41          
31AA: 7F          
31AB: 41          
31AC: 41          
31AD: 41          
31AE: 41          
31AF: 1C          
31B0: 08          
31B1: 08          
31B2: 08          
31B3: 08          
31B4: 08          
31B5: 08          
31B6: 08          
31B7: 1C          
31B8: 01 01 01    
31BB: 01 01 01    
31BE: 01 41 3E    
31C1: 41          
31C2: 42          
31C3: 44          
31C4: 48          
31C5: 50          
31C6: 68          
31C7: 44          
31C8: 42          
31C9: 41          
31CA: 40          
31CB: 40          
31CC: 40          
31CD: 40          
31CE: 40          
31CF: 40          
31D0: 40          
31D1: 40          
31D2: 7F          
31D3: 41          
31D4: 63          
31D5: 55          
31D6: 49          
31D7: 41          
31D8: 41          
31D9: 41          
31DA: 41          
31DB: 41          
31DC: 41          
31DD: 61          
31DE: 51          
31DF: 49          
31E0: 45          
31E1: 43          
31E2: 41          
31E3: 41          
31E4: 41          
31E5: 3E 41       
31E7: 41          
31E8: 41          
31E9: 41          
31EA: 41          
31EB: 41          
31EC: 41          
31ED: 3E 7E       
31EF: 41          
31F0: 41          
31F1: 41          
31F2: 7E          
31F3: 40          
31F4: 40          
31F5: 40          
31F6: 40          
31F7: 3E 41       
31F9: 41          
31FA: 41          
31FB: 41          
31FC: 41          
31FD: 45          
31FE: 42          
31FF: 3D          
3200: 7E          
3201: 41          
3202: 41          
3203: 41          
3204: 7E          
3205: 48          
3206: 44          
3207: 42          
3208: 41          
3209: 3E 41       
320B: 40          
320C: 40          
320D: 3E 01       
320F: 01 41 3E    
3212: 7F          
3213: 08          
3214: 08          
3215: 08          
3216: 08          
3217: 08          
3218: 08          
3219: 08          
321A: 08          
321B: 41          
321C: 41          
321D: 41          
321E: 41          
321F: 41          
3220: 41          
3221: 41          
3222: 41          
3223: 3E 41       
3225: 41          
3226: 41          
3227: 22 22 14    
322A: 14          
322B: 08          
322C: 08          
322D: 41          
322E: 41          
322F: 41          
3230: 41          
3231: 41          
3232: 49          
3233: 55          
3234: 63          
3235: 41          
3236: 41          
3237: 41          
3238: 22 14 08    
323B: 14          
323C: 22 41 41    
323F: 41          
3240: 41          
3241: 22 14 08    
3244: 08          
3245: 08          
3246: 08          
3247: 08          
3248: 7F          
3249: 01 02 04    
324C: 08          
324D: 10 20       
324F: 40          
3250: 7F          
3251: 3C          
3252: 20 20       
3254: 20 20       
3256: 20 20       
3258: 20 3C       
325A: 00          
325B: 00          
325C: 40          
325D: 20 10       
325F: 08          
3260: 04          
3261: 02          
3262: 01 3C 04    
3265: 04          
3266: 04          
3267: 04          
3268: 04          
3269: 04          
326A: 04          
326B: 3C          
326C: 08          
326D: 14          
326E: 22 41 00    
3271: 00          
3272: 00          
3273: 00          
3274: 00          
3275: 80          
3276: 00          
3277: 00          
3278: 00          
3279: 00          
327A: 00          
327B: FF          
327C: 00          
327D: 00          
327E: 18 18       
3280: 10 08       
3282: 00          
3283: 00          
3284: 00          
3285: 00          
3286: 00          

;
; Character "a"
;

3287: 00 00 00 3A 46 42 42 46 3A 

;
; Character "b"
;

3290: 40 40 40 5C 62 42 42 62 5C       

;
; Character "c"
;

3299: 00 00 00 3C 42 40 40 42 3C          

;
; Character "d"
;

32A2: 02 02 02 3A 46 42 42 46 3A 

;
; Character "e"
;

32AB: 00 00 00 3C 42 7E 40 40 3C          

;
; Character "f"
;

32B4: 0C 12 10 10 38 10 10 10 10 

;
; Character "g"
;

32BD: BA 46 42 42 46 3A 02 42 3C          


;
; Character "h"
;

32C6: 40 40 40 7C 42 42 42 42 42          

;
; Character "i"
;

32CF: 00 08 00 08 08 08 08 08 08          

;
; Character "j"
;

32D8: 84 04 04 04 04 04 04 44 38 

;
; Character "k"
;

32E1: 40 40 40 44 48 50 70 48 44 

;
; Character "l"
;

32EA: 10 10 10 10 10 10 10 10 10 

;
; Character "m"
;

32F3: 00 00 00 76 49 49 49 49 49          

;
; Character "n"
;

32FC: 00 00 00 7C 42 42 42 42 42          

;
; Character "o"
;

3305: 00 00 00 3C 42 42 42 42 3C          

;
; Character "p"
;

330E: DC 62 42 42 62 5C 40 40 40          

;
; Character "q"
;

3317: BA 46 42 42 46 3A 02 02 02          

;
; Character "r"
;

3320: 00 00 00 5C 62 40 40 40 40          

;
; Character "s"
;

3329: 00 00 00 3C 42 30 0C 42 3C          

;
; Character "t"
;

3332: 00 10 10 7C 10 10 10 10 10 

;
; Character "u"
;

333B: 00 00 00 42 42 42 42 42 3C          

;
; Character "v"
;

3344: 00 00 00 44 44 44 44 28 10       

;
; Character "w"
;

334D: 00 00 00 41 41 41 49 49 36 

;
; Character "x"
;

3356: 00 00 00 42 24 18 18 24 42          

;
; Character "y"
;

335F: C2 42 42 42 46 3A 02 42 3C          

;
; Character "z"
;

3368: 00 00 00 7E 04 08 10 20 7E          

;
; Character "{"
;

3371: 0C 10 10 10 20 10 10 10 0C       

;
; Character "|"
;

337A: 08 08 08 00 00 08 08 08 00          

;
; Character "}"
;

3383: 18 04 04 04 02 04 04 04 18 

;
; Character "~"
;

338C: 30 49 06 00 00 00 00 00 00          



3395: 55          ld   d,l
3396: 2A 55 2A    ld   hl,($2A55)
3399: 55          ld   d,l
339A: 2A 55 2A    ld   hl,($2A55)
339D: 55          ld   d,l
339E: 08          ex   af,af'
339F: 00          nop
33A0: 1C          inc  e
33A1: 2A 08 08    ld   hl,($0808)
33A4: 14          inc  d
33A5: 22 00 21    ld   ($2100),hl
33A8: 89          adc  a,c
33A9: 08          ex   af,af'
33AA: F5          push af
33AB: 3E FF       ld   a,$FF
33AD: BE          cp   (hl)
33AE: 38 09       jr   c,$33B9
33B0: D3 4D       out  ($4D),a               ; write to nmi_disable_r
33B2: 77          ld   (hl),a
33B3: 21 29 34    ld   hl,$3429
33B6: 22 85 08    ld   ($0885),hl
33B9: F1          pop  af
33BA: D3 4C       out  ($4C),a               ; write to nmi_enable_r
33BC: C9          ret

;
; Play Bolt shooting sound.
; 
; See also: START macro contained within NMI.ASM in Frenzy's source code.

SFIRE:
33BD: 21 89 08    ld   hl,$0889              ; load HL with address of PC1
33C0: F5          push af
33C1: 3E 00       ld   a,$00                 ; priority of bolt sound
33C3: BE          cp   (hl)                  ; compare with current sound's priority   
33C4: 38 09       jr   c,$33CF               ; if 0 < current sound priority, don't play sound, goto $33CF
33C6: D3 4D       out  ($4D),a               ; write to nmi_disable_r
33C8: 77          ld   (hl),a                ; set current sound priority 
33C9: 21 D3 33    ld   hl,$33D3
33CC: 22 85 08    ld   ($0885),hl
33CF: F1          pop  af
33D0: D3 4C       out  ($4C),a               ; write to nmi_enable_r
33D2: C9          ret


33D3: 0E 04       ld   c,$04
33D5: 81          add  a,c
33D6: 08          ex   af,af'
33D7: 00          nop
33D8: 05          dec  b
33D9: 06 06       ld   b,$06
33DB: 0E 03       ld   c,$03
33DD: 78          ld   a,b
33DE: 08          ex   af,af'
33DF: 92          sub  d
33E0: 92          sub  d
33E1: 92          sub  d
33E2: 0F          rrca
33E3: 03          inc  bc
33E4: 7B          ld   a,e
33E5: 08          ex   af,af'
33E6: 32 00 32    ld   ($3200),a
33E9: 00          nop
33EA: 32 00 06    ld   ($0600),a
33ED: 04          inc  b
33EE: 95          sub  l
33EF: 08          ex   af,af'
33F0: 06 32       ld   b,$32
33F2: 93          sub  e
33F3: 08          ex   af,af'
33F4: 01 0B 0F    ld   bc,$0F0B
33F7: 00          nop
33F8: 7B          ld   a,e
33F9: 08          ex   af,af'
33FA: 0B          dec  bc
33FB: 11 00 7D    ld   de,$7D00
33FE: 08          ex   af,af'
33FF: 0B          dec  bc
3400: 10 00       djnz $3402
3402: 7F          ld   a,a
3403: 08          ex   af,af'
3404: 03          inc  bc
3405: 93          sub  e
3406: 08          ex   af,af'
3407: EC 07 32    call pe,$3207
340A: 00          nop
340B: 7B          ld   a,e
340C: 08          ex   af,af'
340D: 03          inc  bc
340E: 95          sub  l
340F: 08          ex   af,af'
3410: DF          rst  $18
3411: 06 FA       ld   b,$FA
3413: 93          sub  e
3414: 08          ex   af,af'
3415: 01 0B 0A    ld   bc,$0A0B
3418: 00          nop
3419: 7B          ld   a,e
341A: 08          ex   af,af'
341B: 0B          dec  bc
341C: 0D          dec  c
341D: 00          nop
341E: 7D          ld   a,l
341F: 08          ex   af,af'
3420: 0B          dec  bc
3421: 0F          rrca
3422: 00          nop
3423: 7F          ld   a,a
3424: 08          ex   af,af'
3425: 03          inc  bc
3426: 93          sub  e
3427: 08          ex   af,af'
3428: EC 0E 03    call pe,$030E
342B: 78          ld   a,b
342C: 08          ex   af,af'
342D: 00          nop
342E: 00          nop
342F: 00          nop
3430: 0E 04       ld   c,$04
3432: 82          add  a,d
3433: 08          ex   af,af'
3434: 00          nop
3435: 00          nop
3436: 00          nop
3437: 00          nop
3438: 00          nop


;
; Play the "player being electrocuted" sounds.
;
; See also: START macro in NMI.ASM within Frenzy's source code. 

SFRY:
3439: 21 89 08    ld   hl,$0889              ; load HL with address of PC1 
343C: F5          push af
343D: 3E 03       ld   a,$03                 
343F: BE          cp   (hl)
3440: 38 09       jr   c,$344B
3442: D3 4D       out  ($4D),a               ; write to nmi_disable_r
3444: 77          ld   (hl),a
3445: 21 4F 34    ld   hl,$344F
3448: 22 85 08    ld   ($0885),hl            ; set PC0.
344B: F1          pop  af
344C: D3 4C       out  ($4C),a               ; write to nmi_enable_r
344E: C9          ret

344F: 0E 04       ld   c,$04
3451: 81          add  a,c
3452: 08          ex   af,af'
3453: 00          nop
3454: 06 07       ld   b,$07
3456: 07          rlca
3457: 0E 03       ld   c,$03
3459: 78          ld   a,b
345A: 08          ex   af,af'
345B: 90          sub  b
345C: 90          sub  b
345D: 90          sub  b
345E: 06 10       ld   b,$10
3460: 93          sub  e
3461: 08          ex   af,af'
3462: 07          rlca
3463: AC          xor  h
3464: 00          nop
3465: 7B          ld   a,e
3466: 08          ex   af,af'
3467: 0F          rrca
3468: 02          ld   (bc),a
3469: 7D          ld   a,l
346A: 08          ex   af,af'
346B: 14          inc  d
346C: 00          nop
346D: 0A          ld   a,(bc)
346E: 00          nop
346F: 06 19       ld   b,$19
3471: 95          sub  l
3472: 08          ex   af,af'
3473: 01 0A 05    ld   bc,$050A
3476: 7D          ld   a,l
3477: 08          ex   af,af'
3478: 0A          ld   a,(bc)
3479: 1E 7F       ld   e,$7F
347B: 08          ex   af,af'
347C: 03          inc  bc
347D: 95          sub  l
347E: 08          ex   af,af'
347F: F3          di
3480: 0A          ld   a,(bc)
3481: FC 7B 08    call m,$087B
3484: 03          inc  bc
3485: 93          sub  e
3486: 08          ex   af,af'
3487: DF          rst  $18
3488: 02          ld   (bc),a
3489: 9F          sbc  a,a

;
; Play sound of robot exploding.
;
; See also: START macro contained within NMI.ASM in Frenzy's source code.
;

SBLAM:
348A: 21 89 08    ld   hl,$0889
348D: F5          push af
348E: 3E 01       ld   a,$01
3490: BE          cp   (hl)
3491: 38 09       jr   c,$349C
3493: D3 4D       out  ($4D),a               ; write to nmi_disable_r
3495: 77          ld   (hl),a
3496: 21 A0 34    ld   hl,$34A0
3499: 22 85 08    ld   ($0885),hl
349C: F1          pop  af
349D: D3 4C       out  ($4C),a               ; write to nmi_enable_r
349F: C9          ret

34A0: 0E 03       ld   c,$03
34A2: 78          ld   a,b
34A3: 08          ex   af,af'
34A4: 82          add  a,d
34A5: 80          add  a,b
34A6: 80          add  a,b
34A7: 0E 04       ld   c,$04
34A9: 81          add  a,c
34AA: 08          ex   af,af'
34AB: 03          inc  bc
34AC: 07          rlca
34AD: 07          rlca
34AE: 07          rlca
34AF: 0F          rrca
34B0: 03          inc  bc
34B1: 7B          ld   a,e
34B2: 08          ex   af,af'
34B3: 01 00 01    ld   bc,$0100
34B6: 00          nop
34B7: 05          dec  b
34B8: 00          nop
34B9: 01 0E 03    ld   bc,$030E
34BC: 78          ld   a,b
34BD: 08          ex   af,af'
34BE: 92          sub  d
34BF: 90          sub  b
34C0: 90          sub  b
34C1: 06 37       ld   b,$37
34C3: 95          sub  l
34C4: 08          ex   af,af'
34C5: 06 06       ld   b,$06
34C7: 93          sub  e
34C8: 08          ex   af,af'
34C9: 01 03 93    ld   bc,$9303
34CC: 08          ex   af,af'
34CD: FB          ei
34CE: 0B          dec  bc
34CF: 01 00 7B    ld   bc,$7B00
34D2: 08          ex   af,af'
34D3: 03          inc  bc
34D4: 95          sub  l
34D5: 08          ex   af,af'
34D6: EE 0E       xor  $0E
34D8: 03          inc  bc
34D9: 78          ld   a,b
34DA: 08          ex   af,af'
34DB: 00          nop
34DC: 00          nop
34DD: 00          nop
34DE: 0E 04       ld   c,$04
34E0: 81          add  a,c
34E1: 08          ex   af,af'
34E2: 00          nop
34E3: 00          nop
34E4: 00          nop
34E5: 00          nop
34E6: 00          nop


;
; Robot shoot sound.
;
; See also: SRFIRE# in NMI.ASM within Frenzy's source code.
;

SRFIRE#:
34E7: 21 89 08    ld   hl,$0889
34EA: F5          push af
34EB: 3E 01       ld   a,$01
34ED: BE          cp   (hl)
34EE: 38 09       jr   c,$34F9
34F0: D3 4D       out  ($4D),a               ; write to nmi_disable_r
34F2: 77          ld   (hl),a
34F3: 21 FD 34    ld   hl,$34FD
34F6: 22 85 08    ld   ($0885),hl
34F9: F1          pop  af
34FA: D3 4C       out  ($4C),a               ; write to nmi_enable_r
34FC: C9          ret

34FD: 0E 03       ld   c,$03
34FF: 78          ld   a,b
3500: 08          ex   af,af'
3501: 92          sub  d
3502: 92          sub  d
3503: 92          sub  d
3504: 0E 04       ld   c,$04
3506: 81          add  a,c
3507: 08          ex   af,af'
3508: 00          nop

3509: 06 06       ld   b,$06
350B: 07          rlca
350C: 0F          rrca
350D: 03          inc  bc
350E: 7B          ld   a,e
350F: 08          ex   af,af'
3510: 14          inc  d
3511: 00          nop
3512: 2D          dec  l
3513: 00          nop
3514: 5A          ld   e,d
3515: 00          nop
3516: 06 04       ld   b,$04
3518: 95          sub  l
3519: 08          ex   af,af'
351A: 06 50       ld   b,$50
351C: 93          sub  e
351D: 08          ex   af,af'
351E: 01 0B 08    ld   bc,$080B
3521: 00          nop
3522: 7B          ld   a,e
3523: 08          ex   af,af'
3524: 0B          dec  bc
3525: 11 00 7D    ld   de,$7D00
3528: 08          ex   af,af'
3529: 0B          dec  bc
352A: 2F          cpl
352B: 00          nop
352C: 7F          ld   a,a
352D: 08          ex   af,af'
352E: 03          inc  bc
352F: 93          sub  e
3530: 08          ex   af,af'
3531: EC 03 95    call pe,$9503
3534: 08          ex   af,af'
3535: E4 02 9F    call po,$9F02


;
; Play the "extra life awarded" sound.
; See also: START macro in NMI.ASM within Frenzy's source code.

SXLIFE:
3538: 21 89 08    ld   hl,$0889
353B: F5          push af
353C: 3E 02       ld   a,$02
353E: BE          cp   (hl)
353F: 38 09       jr   c,$354A
3541: D3 4D       out  ($4D),a               ; write to nmi_disable_r
3543: 77          ld   (hl),a
3544: 21 4E 35    ld   hl,$354E
3547: 22 85 08    ld   ($0885),hl
354A: F1          pop  af
354B: D3 4C       out  ($4C),a               ; write to nmi_enable_r
354D: C9          ret


354E: 0E 04       ld   c,$04
3550: 81          add  a,c
3551: 08          ex   af,af'
3552: 00          nop
3553: 07          rlca
3554: 07          rlca
3555: 07          rlca
3556: 0E 03       ld   c,$03
3558: 78          ld   a,b
3559: 08          ex   af,af'
355A: 92          sub  d
355B: 92          sub  d
355C: 92          sub  d
355D: 0F          rrca
355E: 03          inc  bc
355F: 7B          ld   a,e
3560: 08          ex   af,af'
3561: C8          ret  z
3562: 00          nop
3563: 3C          inc  a
3564: 00          nop
3565: 28 00       jr   z,$3567
3567: 06 14       ld   b,$14
3569: 95          sub  l
356A: 08          ex   af,af'
356B: 06 14       ld   b,$14
356D: 93          sub  e
356E: 08          ex   af,af'
356F: 01 0B 14    ld   bc,$140B
3572: 00          nop
3573: 7B          ld   a,e
3574: 08          ex   af,af'
3575: 0B          dec  bc
3576: 06 00       ld   b,$00
3578: 7D          ld   a,l
3579: 08          ex   af,af'
357A: 0B          dec  bc
357B: 04          inc  b
357C: 00          nop
357D: 7F          ld   a,a
357E: 08          ex   af,af'
357F: 03          inc  bc
3580: 93          sub  e
3581: 08          ex   af,af'
3582: EC 06 14    call pe,$1406
3585: 93          sub  e
3586: 08          ex   af,af'
3587: 01 0B EC    ld   bc,$EC0B
358A: FF          rst  $38
358B: 7B          ld   a,e
358C: 08          ex   af,af'
358D: 0B          dec  bc
358E: FA FF 7D    jp   m,$7DFF
3591: 08          ex   af,af'
3592: 0B          dec  bc
3593: FC FF 7F    call m,$7FFF
3596: 08          ex   af,af'
3597: 03          inc  bc
3598: 93          sub  e
3599: 08          ex   af,af'
359A: EC 03 95    call pe,$9503
359D: 08          ex   af,af'
359E: CC 0E 03    call z,$030E
35A1: 78          ld   a,b
35A2: 08          ex   af,af'
35A3: 00          nop
35A4: 00          nop
35A5: 00          nop
35A6: 0E 04       ld   c,$04
35A8: 81          add  a,c
35A9: 08          ex   af,af'
35AA: 00          nop
35AB: 00          nop
35AC: 00          nop
35AD: 00          nop
35AE: 00          nop


;
; Set colour attributes for the screen
;
;

SET_COLOUR_ATTRS_35AF:
35AF: AF          xor  a
35B0: 32 79 43    ld   ($4379),a             ; set FLIP to upright cabinet mode
35B3: 3E 1E       ld   a,$1E
35B5: 00          nop
35B6: 00          nop

35B7: CD 57 36    call $3657                 ; call COLOUR_FILL
35BA: 
    00                                       ; offset LSB
    00                                       ; offset MSB ($0000 = row 0, col 0)
    05                                       ; lines = 5 (20 pixel lines)
    20                                       ; width = 32 bytes
    AA                                       ; colour = $AA
35BF: CD 57 36    call $3657                 ; call COLOUR_FILL
35C2: 
    A0                                       ; offset LSB
    00                                       ; offset MSB ($00A0 = row 5, col 0)
    29                                       ; lines = 41 (164 pixel lines)
    20                                       ; width = 32 bytes
    11                                       ; colour = $11
35C7: CD 57 36    call $3657                 ; call COLOUR_FILL
35CA: 
    A0                                       ; offset LSB
    00                                       ; offset MSB ($00A0 = row 5, col 0)
    29                                       ; lines = 41 (164 pixel lines)
    09                                       ; width = 9 bytes
    99                                       ; colour = $99
35CF: CD 57 36    call $3657                 ; call COLOUR_FILL
35D2: 
    A9                                       ; offset LSB
    00                                       ; offset MSB ($00A9 = row 5, col 9)
    29                                       ; lines = 41 (164 pixel lines)
    08                                       ; width = 8 bytes
    BB                                       ; colour = $BB
35D7: CD 57 36    call $3657                 ; call COLOUR_FILL
35DA: 
    B0                                       ; offset LSB
    00                                       ; offset MSB ($00B0 = row 5, col 16)
    29                                       ; lines = 41 (164 pixel lines)
    10                                       ; width = 16 bytes
    55                                       ; colour = $55
35DF: CD 57 36    call $3657                 ; call COLOUR_FILL
35E2: 
    C0                                       ; offset LSB
    05                                       ; offset MSB ($05C0 = row 46, col 0)
    0A                                       ; lines = 10 (40 pixel lines)
    20                                       ; width = 32 bytes
    77                                       ; colour = $77
35E7: CD 57 36    call $3657                 ; call COLOUR_FILL
35EA: 
    80                                       ; offset LSB
    06                                       ; offset MSB ($0680 = row 52, col 0)
    04                                       ; lines = 4 (16 pixel lines)
    0A                                       ; width = 10 bytes
    AA                                       ; colour = $AA
35EF: CD 57 36    call $3657                 ; call COLOUR_FILL
35F2: 
    96                                       ; offset LSB
    06                                       ; offset MSB ($0696 = row 52, col 22)
    04                                       ; lines = 4 (16 pixel lines)
    0A                                       ; width = 10 bytes
    DD                                       ; colour = $DD
35F7: C9          ret


;
; Fill large area of colour RAM with white (for diagnostic / test screens).
; See also: C.DIPS in color.asm within Frenzy's source code.
;
COLOUR_FILL_WHITE:
35F8: CD 57 36    call $3657                 ; call COLOUR_FILL
35FB: 
    00                                       ; offset LSB
    00                                       ; offset MSB ($0000 = row 0, col 0)
    38                                       ; lines = 56 (224 pixel lines = full screen)
    20                                       ; width = 32 bytes
    FF                                       ; colour = $FF
3600: C9          ret

;
; Set colour attributes for "insert coin" message area
; See also: C.LI in color.asm within Frenzy's source code.
;

SET_INSERT_COIN_COLOUR_ATTRS:
3601: CD 57 36    call $3657                 ; call COLOUR_FILL
3604: 
    E0                                       ; offset LSB
    05                                       ; offset MSB ($05E0 = row 47, col 0)
    04                                       ; lines = 4 (16 pixel lines)
    20                                       ; width = 32 bytes
    33                                       ; colour = $33
3609: C9          ret

;
; Set colour attributes for "press start" message area (1 credit)
; See also: C.L1 / C.L2 in color.asm within Frenzy's source code.
;

SET_PRESS_START_1CRED_COLOUR_ATTRS:
360A: CD 57 36    call $3657                 ; call COLOUR_FILL
360D: 
    E0                                       ; offset LSB
    05                                       ; offset MSB ($05E0 = row 47, col 0)
    04                                       ; lines = 4 (16 pixel lines)
    20                                       ; width = 32 bytes
    99                                       ; colour = $99
3612: C9          ret

;
; Set colour attributes for "press start" message area (2 or more credits)
; See also: C.L1 / C.L2 in color.asm within Frenzy's source code.
;

SET_PRESS_START_MULTICRED_COLOUR_ATTRS:
3613: CD 57 36    call $3657                 ; call COLOUR_FILL
3616: 
    E0                                       ; offset LSB
    05                                       ; offset MSB ($05E0 = row 47, col 0)
    04                                       ; lines = 4 (16 pixel lines)
    20                                       ; width = 32 bytes
    66                                       ; colour = $66
361B: C9          ret

;
; Set colour attributes for high score / congratulations screen
; See also: C.HIGH in color.asm within Frenzy's source code.
;

SET_HI_SCORE_COLOUR_ATTRS:
361C: CD 57 36    call $3657                 ; call COLOUR_FILL
361F: 
    00                                       ; offset LSB
    00                                       ; offset MSB ($0000 = row 0, col 0)
    08                                       ; lines = 8 (32 pixel lines)
    20                                       ; width = 32 bytes
    BB                                       ; colour = $BB
3624: CD 57 36    call $3657                 ; call COLOUR_FILL
3627: 
    00                                       ; offset LSB
    01                                       ; offset MSB ($0100 = row 8, col 0)
    10                                       ; lines = 16 (64 pixel lines)
    20                                       ; width = 32 bytes
    66                                       ; colour = $66
362C: CD 57 36    call $3657                 ; call COLOUR_FILL
362F:
    00                                       ; offset LSB
    03                                       ; offset MSB ($0300 = row 24, col 0)
    04                                       ; lines = 4 (16 pixel lines)
    20                                       ; width = 32 bytes
    FF                                       ; colour = $FF
3634: CD 57 36    call $3657                 ; call COLOUR_FILL
3637:
    80                                       ; offset LSB
    03                                       ; offset MSB ($0380 = row 28, col 0)
    1C                                       ; lines = 28 (112 pixel lines)
    20                                       ; width = 32 bytes
    AA                                       ; colour = $AA
363C: C9          ret

;
; Set colour attributes for bookkeeping screen
; See also: C.BOOKS in color.asm within Frenzy's source code.
;
363D: CD 57 36    call $3657                 ; call COLOUR_FILL
3640:
    00                                       ; offset LSB
    00                                       ; offset MSB ($0000 = row 0, col 0)
    2F                                       ; lines = 47 (188 pixel lines)
    20                                       ; width = 32 bytes
    CC                                       ; colour = $CC
3645: CD 57 36    call $3657                 ; call COLOUR_FILL
3648: 
    E0                                       ; offset LSB
    05                                       ; offset MSB ($05E0 = row 47, col 0)
    09                                       ; lines = 9 (36 pixel lines)
    20                                       ; width = 32 bytes
    AA                                       ; colour = $AA
364D: C9          ret

;
; Set colour attributes for moving between rooms
; See also: C.MOVE in color.asm within Frenzy's source code.
;
364E: CD 57 36    call $3657                 ; call COLOUR_FILL 
3651: 
    00                                       ; offset LSB
    00                                       ; offset MSB ($0000 = row 0, col 0)
    34                                       ; lines = 52 (208 pixel lines)
    20                                       ; width = 32 bytes
    44                                       ; colour = $44
3656: C9          ret


; 
; Sets a rectangular area of colour attribute RAM to a specified colour.
; Or, in plain English - rectangular colour fill!
;
; Expects:
; Return address on stack to point to 5 bytes:
; Byte 0: LSB of *offset into colour RAM
; Byte 1: MSB of offset into colour RAM 
; Byte 2: number of lines
; Byte 3: width in bytes
; Byte 4: colour 
;
; Remarks:
; See also C.BOX in color.asm within Frenzy's source code.

COLOUR_FILL:
3657: E1          pop  hl                    ; get parameter address
3658: 5E          ld   e,(hl)                ; Read 1st parameter (offset LSB)
3659: 23          inc  hl
365A: 56          ld   d,(hl)                ; Read 2nd parameter (offset MSB). DE now offset into colour lookup RAM
365B: 23          inc  hl
365C: E5          push hl                    ; save address of 3rd parameter byte on stack 
365D: 3A 79 43    ld   a,($4379)             ; read FLIP	
3660: B7          or   a
3661: 20 06       jr   nz,$3669              ; if cocktail setup goto $3669

; upright cabinet
3663: 21 00 81    ld   hl,$8100              ; load HL with address in colour lookup RAM   
3666: 19          add  hl,de
3667: 18 05       jr   $366E

; cocktail cabinet
3669: 21 FF 87    ld   hl,$87FF
366C: ED 52       sbc  hl,de

; both cabinet types
366E: EB          ex   de,hl
366F: E1          pop  hl                    ; restore address of 3rd parameter byte from stack
3670: 4E          ld   c,(hl)                ; read 3rd parameter (number of lines) 
3671: 23          inc  hl
3672: 08          ex   af,af'
3673: 7E          ld   a,(hl)                ; read 4th parameter (width in bytes) 
3674: 23          inc  hl
3675: 08          ex   af,af'
3676: B7          or   a                     ; set zero flag if upright cabinet
3677: 7E          ld   a,(hl)                ; read 5th parameter (colour) byte
3678: 23          inc  hl
3679: E5          push hl
367A: EB          ex   de,hl
367B: 20 11       jr   nz,$368E              ; if not upright cabinet, goto $368E

; upright cabinet
; HL = start of colour attribute RAM to fill 
; A = colour to fill cells with
; A' = count of columns to fill
; C = count of rows to fill 
367D: 11 20 00    ld   de,$0020              ; sizeof a row of colour attributes           
3680: 08          ex   af,af'                ; now A = count of columns to fill
3681: 47          ld   b,a                   ; load b with count of attribute cells to fill
3682: 08          ex   af,af'                ; now A = colour to fill cells with
3683: E5          push hl                    
3684: 77          ld   (hl),a                ; write colour to cell
3685: 23          inc  hl
3686: 10 FC       djnz $3684                 ; repeat until all cells for row are coloured
3688: E1          pop  hl
3689: 19          add  hl,de                 ; bump HL to next row
368A: 0D          dec  c                     ; decrement row counter
368B: 20 F3       jr   nz,$3680              ; repeat until all rows done
368D: C9          ret

; cocktail cabinet
; Ignored - I don't care about cocktail cabinets
368E: 11 E0 FF    ld   de,$FFE0
3691: 08          ex   af,af'
3692: 47          ld   b,a
3693: 08          ex   af,af'
3694: E5          push hl
3695: 77          ld   (hl),a
3696: 2B          dec  hl
3697: 10 FC       djnz $3695
3699: E1          pop  hl
369A: 19          add  hl,de
369B: 0D          dec  c
369C: 20 F3       jr   nz,$3691
369E: C9          ret

;
; Colour the walls of the room based on difficulty table
; 
; See also: C.WALLS in color.asm within Frenzy's source code.
;

C.WALLS:
369F: CD 57 36    call $3657                 ; call COLOUR_FILL
36A2: 
    80                                       ; offset LSB
    06                                       ; offset MSB ($0680 = row 52, col 0)
    04                                       ; lines = 4 (16 pixel lines)
    20                                       ; width = 32 bytes
    77                                       ; colour = $77

36A7: CD E7 35    call $35E7                 ; fill bottom info area coloured boxes
36AA: CD 34 23    call $2334                 ; call GET_PLAYER_SCORE_PTR  
36AD: EB          ex   de,hl                 ; DE = pointer to player score (3 bytes BCD)
36AE: 01 05 00    ld   bc,$0005              ; BC = size of each difficulty table entry
36B1: 13          inc  de                    ; skip BCD byte 0 (hundred-thousands & ten-thousands)
36B2: 1A          ld   a,(de)                ; read BCD byte 1 (thousands & hundreds)
36B3: 08          ex   af,af'                ; save byte 1 in alternate register
36B4: 1B          dec  de                    ; point back to BCD byte 0
36B5: 1A          ld   a,(de)                ; read BCD byte 0 (hundred-thousands & ten-thousands)
36B6: B7          or   a                     ; test if score < 10,000 (byte 0 zero)
36B7: 21 94 37    ld   hl,$3794              ; HL = pointer to LOW_DIFFICULTY_TABLE (score < 10,000)
36BA: 28 0F       jr   z,$36CB               ; if < 10,000, use low table (AF' = byte 1 value)

; Score is more than 10,000
36BC: E6 0F       and  $0F                   ; keep ten-thousands digit from byte 0
36BE: 67          ld   h,a                   ; save ten-thousands in H
36BF: 08          ex   af,af'                ; retrieve byte 1 (thousands & hundreds) from AF'
36C0: E6 F0       and  $F0                   ; keep thousands digit from byte 1
36C2: B4          or   h                     ; A = [thousands][ten-thousands]
36C3: 07          rlca                       ; rotate left 4 times to
36C4: 07          rlca                       ;  produce A = [ten-thousands][thousands]
36C5: 07          rlca
36C6: 07          rlca
36C7: 21 BC 37    ld   hl,$37BC              ; HL =pointer to NORMAL_DIFFICULTY_TABLE (score >= 10,000)
36CA: 08          ex   af,af'                ; save difficulty index in AF'

36CB: 08          ex   af,af'                ; retrieve difficulty index (byte 1 if score<10k, else combined)
36CC: BE          cp   (hl)                  ; compare with table entry threshold
36CD: 38 06       jr   c,$36D5               ; if index < threshold, found entry
36CF: 08          ex   af,af'                ; save difficulty index
36D0: 09          add  hl,bc                 ; advance HL by 5 bytes to next entry
36D1: 7E          ld   a,(hl)                ; read threshold of next entry
36D2: B7          or   a                     ; is it zero? (end marker)
36D3: 20 F6       jr   nz,$36CB              ; if not end, keep searching
36D5: 23          inc  hl                    ; skip threshold byte, point to parameter data
36D6: 7E          ld   a,(hl)                ; read byte 1: max robot bolts count
36D7: 23          inc  hl
36D8: 32 4B 43    ld   ($434B),a             ; set RBOLTS   
36DB: 7E          ld   a,(hl)                ; read byte 2: robot bolt slot offset
36DC: 23          inc  hl
36DD: 32 7A 43    ld   ($437A),a             ; set bolt slot processing modifier
36E0: 7E          ld   a,(hl)                ; read byte 3: robot recharge wait
36E1: 23          inc  hl
36E2: 32 4D 43    ld   ($434D),a             ; set RWAIT
36E5: 4E          ld   c,(hl)                ; read byte 4: robot colour (used for wall colouring)

36E6: 3A 79 43    ld   a,($4379)             ; read FLIP	
36E9: B7          or   a                     ; set zero flag if upright cabinet
36EA: DD 21 00 81 ld   ix,$8100              ; IX = colour attribute RAM base (upright)
36EE: 21 00 44    ld   hl,$4400              ; HL = screen image RAM base (upright)
36F1: 28 07       jr   z,$36FA               ; if upright, skip cocktail setup

; cocktail cabinet colour/screen RAM base
36F3: DD 21 80 81 ld   ix,$8180              ; IX = colour RAM (offset for flipped cocktail)
36F7: 21 00 46    ld   hl,$4600              ; HL = screen RAM (offset for flipped cocktail)

; both cocktail and upright
36FA: 3E 34       ld   a,$34                 ; A = 52 rows of colour attributes to process
36FC: 08          ex   af,af'                ; save row counter in AF'
36FD: 06 20       ld   b,$20                 ; B = 32 bytes per row (full screen width)
36FF: 7E          ld   a,(hl)                ; read byte from screen RAM
3700: 23          inc  hl                    ; advance screen RAM pointer
3701: 5F          ld   e,a                   ; save screen byte in E
3702: E6 44       and  $44                   ; test wall edge bits (bit 2 and bit 6)
3704: 57          ld   d,a                   ; D = non-zero if wall pixel detected
3705: 7B          ld   a,e                   ; get original screen byte back
3706: 2F          cpl                        ; invert = open space bits
3707: A1          and  c                     ; AND with robot colour -> coloured pixel if open
3708: B2          or   d                     ; OR in wall detection -> grey tint if wall
3709: DD 77 00    ld   (ix+$00),a            ; write composite colour to attribute RAM
370C: DD 23       inc  ix                    ; advance colour RAM pointer
370E: 10 EF       djnz $36FF                 ; loop for all 32 bytes in this row
3710: 11 60 00    ld   de,$0060              ; DE = 96 bytes (3 screen rows)
3713: 19          add  hl,de                 ; skip 3 screen rows (we colour 4 at a time)
3714: 08          ex   af,af'                ; retrieve row counter
3715: 3D          dec  a                     ; decrement row counter
3716: 20 E4       jr   nz,$36FC              ; loop until all 52 rows processed
3718: C9          ret                        ; end of C.WALLS


;
; Restore original colour attributes of maze at player position   
;
; Expects:
; HL = pointer to VECTOR struct for player
;
; Remarks: See UNCMAN within color.asm in Frenzy's source code.
;

UNCOLOUR_MAN:
3719: CB 5E       bit  3,(hl)                ; test BLANK bit
371B: CA 3C 37    jp   z,$373C               ; if not set, jump to COLOUR_MAN            
371E: CB 9E       res  3,(hl)
3720: E5          push hl

; Restore colour attributes 
3721: 2A 40 09    ld   hl,($0940)            ; read PLAYER_COLOUR_ADDR
3724: 11 42 09    ld   de,$0942              ; load DE with address of PLAYER_COLOUR_SAVE   
3727: 3E 05       ld   a,$05                 ; 5 attribute rows high
3729: 08          ex   af,af'                ; Use alternate register AF' for reading from PLAYER_COLOUR_SAVE buffer  
372A: 06 02       ld   b,$02                 ; 2 attribute columns wide
372C: 1A          ld   a,(de)                ; read from PLAYER_COLOUR_SAVE buffer   
372D: 13          inc  de
372E: 77          ld   (hl),a                ; and write to attributes
372F: 23          inc  hl
3730: 10 FA       djnz $372C                 ; repeat until all columns done
3732: 01 1E 00    ld   bc,$001E              
3735: 09          add  hl,bc                 ; bump HL to point to attribute row below
3736: 08          ex   af,af'                ; Now A = number of rows left to fill with colour
3737: 3D          dec  a                     ; decrement counter
3738: C2 29 37    jp   nz,$3729              ; if not all rows done, goto $3729
373B: E1          pop  hl


;
; Colour the player sprite.
;
; Expects:
; HL = pointer to player's VECTOR structure
;
; See also: COLMAN in color.asm within Frenzy's source code.
;

COLOUR_MAN:
373C: CB 66       bit  4,(hl)                ; test COLOR bit
373E: C8          ret  z                     ; if the flag isn't set to colour the player sprite, exit

373F: CB DE       set  3,(hl)                ; set BLANK bit
3741: E5          push hl                    
3742: 11 07 00    ld   de,$0007  
3745: 19          add  hl,de                 ; bump HL to point to VECTOR P.X 
3746: 5E          ld   e,(hl)                ; E = man's X position on screen
3747: 23          inc  hl
3748: 23          inc  hl                    ; bump HL to point to VECTOR.P.Y 
3749: 3A 79 43    ld   a,($4379)             ; read FLIP	
374C: B7          or   a                     ; is upright cabinet?
374D: 7E          ld   a,(hl)                ; A = man's Y position on screen
374E: 28 0A       jr   z,$375A               ; if upright cabinet goto $375A 

; for cocktail cabinet only. 
3750: ED 44       neg
3752: C6 D0       add  a,$D0
3754: 08          ex   af,af'
3755: 3E F7       ld   a,$F7
3757: 93          sub  e
3758: 5F          ld   e,a
3759: 08          ex   af,af'

; for both upright and cocktail cabinets

; calculate offset into colour attribute RAM. A = Y coordinate of player, E = X coordinate
375A: CB 3F       srl  a
375C: CB 3F       srl  a
375E: 67          ld   h,a
375F: 6B          ld   l,e
3760: CB 3C       srl  h
3762: CB 1D       rr   l
3764: CB 3C       srl  h
3766: CB 1D       rr   l
3768: CB 3C       srl  h
376A: CB 1D       rr   l

376C: 01 00 81    ld   bc,$8100              ; load BC with address of attribute RAM
376F: 09          add  hl,bc

; HL now = address in colour attribute RAM to update
3770: 22 40 09    ld   ($0940),hl            ; set PLAYER_COLOUR_ADDR
3773: 3A 78 43    ld   a,($4378)             ; read PLAYER_COLOUR
3776: 11 1E 00    ld   de,$001E
3779: FD 21 42 09 ld   iy,$0942              ; load IY with address of PLAYER_COLOUR_SAVE 
377D: 0E 05       ld   c,$05                 ; 5 attribute rows high
377F: 06 02       ld   b,$02                 ; 2 attribute columns wide
3781: 08          ex   af,af'
3782: 7E          ld   a,(hl)                ; read from screen attribute RAM
3783: FD 77 00    ld   (iy+$00),a            ; write to PLAYER_COLOUR_SAVE buffer
3786: FD 23       inc  iy
3788: 08          ex   af,af'                ; Now A = PLAYER_COLOUR
3789: 77          ld   (hl),a                ; write colour to screen attribute memory       
378A: 23          inc  hl
378B: 10 F4       djnz $3781                 ; repeat until all columns done
378D: 19          add  hl,de                 ; bump HL to point to attribute row below
378E: 0D          dec  c                     ; decrement row counter
378F: C2 7F 37    jp   nz,$377F              ; if not all rows done, goto $377F to do more
3792: E1          pop  hl                    ; restore HL from stack (was pushed @ $3741)  
3793: C9          ret


LOW_DIFFICULTY_TABLE:
3794: 03          inc  bc
3795: 00          nop
3796: 00          nop
3797: 50          ld   d,b
3798: 33          inc  sp
3799: 15          dec  d
379A: 01 00 50    ld   bc,$5000
379D: 99          sbc  a,c
379E: 30 02       jr   nc,$37A2
37A0: 00          nop
37A1: 14          inc  d
37A2: 66          ld   h,(hl)
37A3: 45          ld   b,l
37A4: 03          inc  bc
37A5: 00          nop
37A6: 0A          ld   a,(bc)
37A7: AA          xor  d
37A8: 60          ld   h,b
37A9: 04          inc  b
37AA: 00          nop
37AB: 0A          ld   a,(bc)
37AC: 55          ld   d,l
37AD: 75          ld   (hl),l
37AE: 05          dec  b
37AF: 00          nop
37B0: 0F          rrca
37B1: BB          cp   e
37B2: 90          sub  b
37B3: 01 01 3C    ld   bc,$3C01
37B6: FF          rst  $38
37B7: 00          nop
37B8: 01 01 32    ld   bc,$3201
37BB: FF          rst  $38

NORMAL_DIFFICULTY_TABLE:
37BC: 10 01       djnz $37BF
37BE: 01 2D FF    ld   bc,$FF2D
37C1: 11 02 02    ld   de,$0202
37C4: 23          inc  hl
37C5: 66          ld   h,(hl)
37C6: 13          inc  de
37C7: 03          inc  bc
37C8: 03          inc  bc
37C9: 19          add  hl,de
37CA: DD          db   $dd
37CB: 15          dec  d
37CC: 04          inc  b
37CD: 04          inc  b
37CE: 14          inc  d
37CF: 77          ld   (hl),a
37D0: 17          rla
37D1: 05          dec  b
37D2: 05          dec  b
37D3: 0F          rrca
37D4: 33          inc  sp
37D5: 19          add  hl,de
37D6: 05          dec  b
37D7: 05          dec  b
37D8: 0A          ld   a,(bc)
37D9: 99          sbc  a,c
37DA: 00          nop
37DB: 05          dec  b
37DC: 05          dec  b
37DD: 05          dec  b
37DE: EE 81       xor  $81
37E0: 94          sub  h
37E1: 0C          inc  c
37E2: 02          ld   (bc),a
37E3: AD          xor  l
37E4: 20 75       jr   nz,$385B
37E6: 7A          ld   a,d
37E7: 0D          dec  c
37E8: 5F          ld   e,a
37E9: 40          ld   b,b
37EA: 93          sub  e
37EB: D5          push de
37EC: 67          ld   h,a
37ED: 8A          adc  a,d
37EE: 49          ld   c,c
37EF: 14          inc  d
37F0: F5          push af
37F1: 74          ld   (hl),h
37F2: CF          rst  $08
37F3: 80          add  a,b
37F4: 21 2B CA    ld   hl,$CA2B
37F7: CC 2B 3C    call z,$3C2B
37FA: 04          inc  b
37FB: FB          ei
37FC: AB          xor  e
37FD: 26 F8       ld   h,$F8
37FF: 00          nop