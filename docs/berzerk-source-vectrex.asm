BERZERK_DEBUG = 0
LIMIT_SUPPORT = 0
DALEK         = 0

; still "beta" version, since no bug was found :-()

; TEST (Thomas, shots go thru - perhaps only up to 6?
; Bug?
; shoot thru wall in some cases?
;
;
; last changes
; Otto timing more dynamic, similar to version 1: 
; https://tcrf.net/Berzerk_(Arcade)?fbclid=IwAR1kqSLuyKg3FcHKwstTbWxg-r_aahaKPmOA8oWYNmvuqN_Aj7bPuPxELHo
;
; note
; wall collision detection always is done to the walls
; north, south west, east
; but never diagonal - this is the original code.
; 
; therefor on "corners" robots tend to slide into walls...

;------------------------------

; Note Malban:
; I tried to keep the original Fred Teft disassembled code in place as well as it was possible
; many of the original RAM addresses are also kept.
; In some places, where I "often" needed the RAM addresses I subsituted a "good" name, instead of the hex address
; but I didn't substitute ALL addresses
; The memory map done by Fred is mostly still intact!
;
; I also kept the BIOS names Fred used.
; Although, where I coded myself using the BIOS - from old habbit - I also defined and used BIOS names
; as supplied by Bruce Tomlin.
; This is a little messy... but actually I don't care.
; 
; * includes "no shoot thru wall"
; * look at maze generation for "prisons"
; * changed shots to be a linked list with max shot = 32
; * when more than about 10 bullets are live -> than < 50Hz
; * Each shot in the air takes anbout 1200-1300 cycles
;------------------------------
; idea, keep current "MapPointToQnMakeRel" in player/robot struct, than we do not have to calculate it x times EVERY turn
; 

                    include  "macro.i"

USE_PB6             =        1 



; Increased OTTO minimal appearance time from 3 seconds to 4
; Decreased max Bullet speed from 6 to 4
; Decreased max ROBOT bullets in the air from 10 to 5
; Added a Threshold Firing range for robots -> if robots are nearer then 20 "pixels" they do not fire
; Added a Threshold for bullet speed, if robot is nearer than a certain range it fires less fast bullets
; Added a Threshold appear range: in the beginning of a level robots can not appear nearler then 40 "pixels"
; Added "Schonzeit" at the beginning of a level, during this, bullet speed is also reduced
; Inital Otto timer (level 1) increased - thus it is incresed for all levels, progress of fast appearance slower
; During HighScore edit - display of version

; V 1.2
; slow player speed increase (up to 2x)
; Add life every 5000

; V 1.3
; Decreased max Bullet speed from 6 to 3
; added two seconds of "no shoot" in the beginning of each level (Schonzeit2)
; corrected Schonzeit1 to actually work
; added ROBOT_MIN_UPDATE and set it to 4 (instead of 0)

; V 1.4
; highscore displays level values > 100 correctly (removing spave between level and player though)
; displayes #of lives greater 4 as "4+"
; corrected Schonzeit2 so it is applied not only on level start, but also after a death

BULLET_BULLET_THRESHOLD  =   1                            ; how far away can a bullet be from another bullet and still "hit" 
OTTO_MIN_TIME       =        200                          ; minimal time for Otto to appear in a given level
MAX_BULLET_SPEED    =        3;4                            ; maximal speed of bullet shot from robots (1-6)
ROBOT_BULLET_RESTRICTION  =  1                            ; if "on" than only max no of bullets can be shot by robots                           
MAX_ROBOT_BULLETS   =        5                            ; number of robot bullets in the air at the same time
FIRE_THRESHOLD      =        20                           ; minimal distance at which a robot is allowed to fire at a player
ROBOT_THRESHOLD     =        30                           ; minimal distance at which a robot can spawn from the player starting position
BULLET_SPEED_THRESHOLD =     32                           ; under this range robots will fire with half bullet speed
INITIAL_OTTO_TIMER  =        10                           ; in seconds, used to be 5
SCHONZEIT           =        255                          ; number of rounds, in the beginning of a level, that shots are slow
SCHONZEIT2          =        100                          ; number of rounds, in the beginning of a level, robots do not shoot
ROBOT_BURST_FIRE_MAX = 6
ROBOT_FIRE_DELAY = 5
ROBOT_MIN_UPDATE = 4; 0 = default

 if BERZERK_DEBUG = 1
START_LEVEL         =        99
NO_OTTO             =        1
INVINCIBLE          =        1                            ; only shots from robots 
INVINCIBLE_ROBOT    =        1                            ; only robots 
 else
START_LEVEL         =        0 
NO_OTTO             =        0 
INVINCIBLE          =        0                            ; only shots from robots 
INVINCIBLE_ROBOT    =        0                            ; only robots 
 endif

;With the final version, from top to bottom in hex, the intensities of the "middle" lines are:
MAGENTA             =        $7d 
WHITE               =        $68 
CYAN                =        $53 
GREEN               =        $3e 
YELLOW              =        $29 
RED                 =        $14 
BLUE                =        $ff 

; $c867 or just 67
; seems to be the main sound marker.
; it seems divided into 2 nibblers
; P R Br Bp # p r br bp
; P = player death
; R = Robot death
; Br = bullet robot
; Bp = bullet player
; P = all channels
; all other "their" respective channels
;
; upper nibble "continueing"
; lower nibble "initiating"
SOUND_PLAYER_DEATH_CONTNUE_BIT  =  $80 
SOUND_ROBOT_DEATH_CONTNUE_BIT  =  $40 
SOUND_ROBOT_BULLET_CONTINUE_BIT  =  $20 
SOUND_PLAYER_BULLET_CONTINUE_BIT  =  $10 
SOUND_PLAYER_DEATH_START_BIT  =  $08 
SOUND_ROBOT_DEATH_START_BIT  =  $04 
SOUND_ROBOT_BULLET_START_BIT  =  $02 
SOUND_PLAYER_BULLET_START_BIT  =  $01 

soundTypes          =        $67 
SOUND_OTTO_CONTINUE_BIT  =   $10 
SOUND_OTTO_START_BIT  =      $01 

;soundType2
;$C868...$C876   ;Unused?

MapVelocity         =        $C868                        ; 4 byte 
currentRobotMapVelocity  =   MapVelocity+4                ; 4 byte 
currentPlayerMapVelocity  =  currentRobotMapVelocity+4    ; 4 byte 
soundType2          =        currentPlayerMapVelocity+4 
soundCounter        =        soundType2+1 
muck_6a             =        soundCounter+1 
drawFunction        =        $CBFB                        ; $C8C9 
isScrolling         =        $C878 
currentMapVelocity  =        $C8C9                        ; $CBFB 
currentCombinedVelocityDirection  =  $C8C9+1 
tmpWord             =        $CBF2 
isColoredMode       =        colorEnabled                 ;$CBF2+2 
currentRobotColor   =        $CBF5 
currentMazeColor    =        $CBF5+1 
currentDoorColor    =        $CBF5+2 
currentPlayerColor  =        $CBF8 
currentShotColor    =        $CBF8+1 
currentStringColor  =        $CBF8+2 
_bulletCounter      =        $c8cc 
_bulletStorage      =        $C974 
;
OBJECT_LIST_COMPARE_ADDRESS  =  $C973 
BULLET_NONE         =        0 
bullet_empty_head   =        $C974                        ; 
bullet_objects_head  =       bullet_empty_head+ 2         ; if greater OBJECT_LIST_COMPARE_ADDRESS, than this is a pointer to a RAM location of an Object 
bulletCount         =        bullet_objects_head+2 

bulletObjects       =        bulletCount +1 
MAX_BULLETS_OBJECTS  =       32 
drawScale           =        $C8CB 
mainJumpIndex       =        $c8C5 
;
                    struct   BulletStruct 
                    ds       B_YPOS, 1                    ; 0 
                    ds       B_XPOS, 1                    ; 1 
; start of bullet vectorlist
                    ds       B_HEIGHT, 1                  ; 2 
                    ds       B_WIDTH, 1                   ; 3 
                    ds       B_YPOS_TAIL, 1               ; 4 
                    ds       B_XPOS_TAIL, 1               ; 5 
                    ds       B_VELOCITY, 1                ; 6 
                    ds       B_DIRECTION, 1               ; 7 
                    ds       B_NEXT_OBJECT, 2 
                    ds       B_ORIGIN, 1                  ; 1 = player bullet, 0 robot bullet 
                    end struct 
;

endOfBullets        =        bulletObjects + BulletStruct*MAX_BULLETS_OBJECTS ; CAF4 
quadrantOfPoint     =        $C89A 
                    bss      
                    org      $cb00 
robotShotCounter    equ      $cb00 
ds2431Present       equ      $cb01                        ; flag whether the eEprom was found or not 
ignoreDs2431        equ      ds2431Present+1              ; flag whether to ignore eEprom altogether (button on startup), VecFlash gets irritated by PB6 access 
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; -> 9*5 +3 = 48 byte
; this is the "old" save structure
; uses verbatim "bytes
; during a "save" this gets converted to "new save structure"
; which is reduced to use only 32 bytes
; if I wasn't so lazy, than I would have gotton rid
; of the old structure, as it is now it adds to confusion :-)
; anyways, in highscore editing and displaying - the "old" structure is used
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
                    struct   HighScoreEntry 
                    ds       NAME,3                       ; 
                    ds       LEVEL,1                      ; 
                    ds       ASCIISCORE,6                 ; 
                    end struct 
; save to eEprom Starts here!
optionsBlock        equ      ignoreDs2431+1 
v4e_saveBlockStart  equ      optionsBlock 
voxEnabled          equ      optionsBlock                 ; 0 = 1 player, 1 = 2 player game 
colorEnabled        equ      voxEnabled+1                 ; 0 = tournament, 1 = training, 2 Hardcore 
musicEnabled        equ      colorEnabled+1               ; 0 = on, 1 = off 
dalekEnabled        equ      musicEnabled+1               ; 0 = no, 1 = yes 
musicOption         equ      dalekEnabled+1 
option6             equ      musicOption+1 
option7             equ      option6+1 
option8             equ      option7+1 
optionsBlockEnd     equ      option8+1                    ; 8 byte at least for a block 

schonzeitTimer = option7

;
highScoreCompetitionBlock  equ  optionsBlockEnd 
highScoreTable      equ      highScoreCompetitionBlock 
highScoreTableEnd   EQU      highScoreTable + HighScoreEntry*5 ; 5 entries 
highScoreCompetitionBlockEnd  equ  highScoreTableEnd 
highScoreHardcoreBlock  equ  highScoreCompetitionBlockEnd 
highScoreHardTable  equ      highScoreHardcoreBlock 
highScoreHardTableEnd  EQU   highScoreHardTable + HighScoreEntry*5 ; 5 entries 
highScoreHardcoreBlockEnd  equ  highScoreHardTableEnd 
v4e_saveBlockEnd    equ      highScoreHardcoreBlockEnd 
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
newEepromRAMStart   equ      highScoreHardcoreBlockEnd    ; new save struct, straight 32 bytes 
newEepromRAMEnd     equ      newEepromRAMStart +32        ; of which only 28 bytes are used! 
tmp1                equ      newEepromRAMEnd +1 
current_eprom_blocksize  equ  tmp1+1 
current_eprom_blockadr  equ  current_eprom_blocksize+1 
eeprom_tmp          equ      current_eprom_blockadr +1    ; 2 byte! 
levelDoneTemp       equ      eeprom_tmp + 2 
tmp2                equ      levelDoneTemp +1             ; 2 bytes 
vox_addr            equ      tmp2 +2 
pitch_addr          equ      vox_addr +2 
sentence_addr       equ      pitch_addr +2 
chickenFlag         equ      sentence_addr +2 
divide_tmp          equ      chickenFlag +1 
conversionBuffer    EQU      divide_tmp+2                 ; internal buffer used by conversion ASCII <-> 16 bit value (in D) vice/versa [values from 0 to 65536*10 + $80] 
highScoreTmpBuffer  EQU      conversionBuffer+7           ; 14 ; one "0" is added to achieve the tenfold length of 16 bit! (which cant be displayed - hahaha!) 
counter16 EQU highScoreTmpBuffer
schonzeitTimer2 = highScoreTmpBuffer+2

 if LIMIT_SUPPORT = 1
limitReached = schonzeitTimer2+1
 endif
                    code     
;
;
; Fred Tafts stuff following
;
;
;
*               0       y position of bullet's tail
*               1       x position of bullet's tail
*               2       y delta between bullet's head and tail
*               3       x delta between bullet's head and tail
*               4       y position of bullet's head
*               5       x position of bullet's head
*               6       velocity factor
*               7       direction of travel               ; anded w/ $F0 when hits something
; can be assembled correctly now
; compatibilty added by Malban
;
; assenmble with comand line:
; .\ass\as09.exe -w200 -h0 -l -mcti berzerk.asm >error
;
; used the 6809 assembler:
; as09 [1.11].
; Copyright 1990-1994, Frank A. Vorstenbosch, Kingswood Software.
; Available at:
; http://www.falstaff.demon.co.uk/cross.html
*
* This work was originally done by Fred Taft (fred@hp-pcd.cv.hp.com).
* Please forward any comments, corrections or additions back to Fred.
*
* Berzerk
*
******************************************************************************
*
* The following are possible bugs discovered in this code:
*
*    DisplayBonusString
*            When checking to see if the bonus strings should be displayed,
                    *        the 'ble' command really should be a 'blt'!!  This explains why
*            when a player clears a maze containing 10 robots, the bonus
*            string was not displayed.  In this case the bonus string is
                    *        "   100", and this fails the above check!
*
*    DisplayLives
*            There is no check to see if the C8D8 buffer is overrun (this
*            is the buffer used to display the number of lives a player has) ;
*            what is the intended size of this buffer?
*
*    FireBullet
*            There seems to be the potential to overwrite the stack when
*            firing bullets.  The array of bullet entries can grow very
*            quickly as a player gets better, since robots can fire more
*            bullets (and fire them more quickly).  The end of the array
*            only gets moved back when the last bullet hits something and
*            becomes inactive.  It would be better to modify FireBullet()
*            to only allow a certain number of bullets (or to do a better
*            job of compacting the array).
*
******************************************************************************
*
* The following is the memory map for Berzerk RAM usage:
*
* C880-C881     Points to player structure for the current player.
*
* C882          Number of players: 0 = 1 player, 1 = 2 players
*
* C883          Controls maximum number of bullets a robot can fire in a
*               burst                                     ; takes a big jump when user's score surpasses 10,000
*               points.
*
* C884          Velocity factor used when firing robot's bullets.
*
* C885          Work memory and loop counter.
*
* C886          Flag                                      ; don't draw player if set < 0
*
* C88F-C890     Player's position
player_y            =        $C88F 
player_x            =        $C890 
*
* C891          Work memory
*
* C892          Work memory
*
* C893-C894     Otto's position (y,x)
*
                    *        C895-C896 Player 's position (starting position); used to seed Otto's
*               initial position.
*
* C897          Counter                                   ; controls bouncing offset for Otto's 'y' position
*
* C898          Otto's movement velocity
*
* C899          After a point is mapped into a quadrant number, this contains
*               the bitmask describing which walls are present in that
*               quadrant:
*
*                      ---------------------------------
*                      | 0 | 0 | 0 | 0 | B | T | R | L |
*                      ---------------------------------
*           Quadrant has Bottom Wall-----^
*           Quadrant has Top Wall------------^
*           Quadrant has Right Wall--------------^
*           Quadrant has Left Wall-------------------^
*
* C89A          Quadrant into which a point falls:
*
*                   -100 -60 -20 +20 +60 +100
*                     +---+---+---+---+---+ +96
*                     | 0 | 1 | 2 | 3 | 4 |
*                     +---o---o---o---o---+ +32
*                     | 5 | 6 | 7 | 8 | 9 |
*                     +---o---o---o---o---+ -32
*                     |10 |11 |12 |13 |14 |
*                     +---+---+---+---+---+ -96
*
* C89B-C89C     After a point is mapped into a quadrant number, this contains
*               the point as a relative offset from the lower left corner
*               of the quadrant.
*
* C89D          Work memory/counter
*
* C89E-C89F     During game, work memory                  ; usually contains a position (y,x).
*               After a game, C89F is a semaphore causing author's initials
*               to be displayed                           ;also used as a counter
*
* C8A1          Work memory and loop counter.
*
* C8A2          Work memory and loop counter.
*
* C8A7          Delay (placed in the robot's structure) between how often
*               a robot is updated (moved, fire, etc)     ; 0 = no delay.
*
* C8A8          Used while populating a level with robots ; if the random
*               number is < C8A8, then a robot is placed in the maze.
*
* C8A9          Used during robot processing.
*
* C8AA          Used during robot processing.
*
* C8AB-C8B2     Array of byte-long offsets, telling which walls are to be
*               drawn from each of the 8 possible starting points within
*               the maze (0=up, 2=down, 4=to right, 6=to left).  The 8
*               possible starting points are shown below:
*
*                   -100 -60 -20 +20 +60 +100
*                     +---+---+---+---+---+ +96
*                     |                   |
*                     +   o   o   o   o   + +32
*                     |                   |
*                     +   o   o   o   o   + -32
*                     |                   |
*                     +---+---+---+---+---+ -96
*
* C8B5-C8C3     Array of masks (1 per quadrant) indicating which walls are
*               present in the quadrant.  The mask values are:
*
*                     $01 = left wall
*                     $02 = right wall
*                     $04 = top wall
*                     $08 = bottom wall
*
*               C8B5 refers to quadrant 0, C8B6 to quadrant 1, etc.
*
* C8C5          Jump table index based on the player's current state:
*
*                     0 = player is alive
*                     2 = player has left the maze
*                     4 = player has died
*
* C8C6          Indicates if one of the maze doors should be closed off:
*
*                       ---------------------------------
*                       | 0 | 0 | 0 | T | B | L | R | 0 |
*                       ---------------------------------
*                     Block Top Door--^
*                     Block Bottom Door---^
*                     Block Left Door---------^
*                     Block Right Door------------^
*
* C8C7          Number of robots initially seeded into the maze
*
* C8C8          Number of robots still alive
*
* C8C9-C8CA     Indirect jump pointer
*
* C8CB          Intensity value used by DrawWithPossibleScrolling()
*
* C8CC          Index of the last bullet in use
*
* C8CD          Cleared in one place, then never again used.
*
* C8CE          Indicates state of player's legs:

PlayerLegState = $c8CE
PlayerArmState = $C8CF

*
*                     0 = player is standing still
*                     2 = player is running right (wide legs)
*                     4 = player is running right (close legs)
*                     6 = player is running left (close legs)
*                     8 = player is running left (wide legs)
*
* C8CF          Indicates the state of the player's arms:
*
*                  When player is running:
*                     $00 = arms moving to the right
*                     $02 = arms moving to the left
*
*                  When player is standing still:
*                     $00 = arms neutral
*                     $02 = arms shooting left
*                     $04 = arms shooting right
*                     $06 = arms shooting up
*                     $08 = arms shooting 45 degrees upper left
*                     $0A = arms shooting 45 degrees upper right
*                     $0C = arms shooting down
*                     $0E = arms shooting 45 degrees lower left
*                     $10 = arms shooting 45 degrees lower right
*
                    *        C8D0 Intensity used when drawing a 'fried' player
*
* C8D1-C8D7     Bonus score string buffer
*
* C8D8-????     Buffer used when displaying number of lives a player has.
*
                    *        C8DE 'y' delta between 2 points, as determined by
*               DetermineDirectionToMove()
*
                    *        C8DF 'x' delta between 2 points, as determined by
*               DetermineDirectionToMove()
*
* C8E0          Used during scrolling                     ; used to calculate 'y' delta
*
* C8E1          Used during scrolling                     ; used to calculate 'x' delta
*
* C8E2-C8E3     Used during scrolling                     ; delta added to 'y' drawing point
*
* C8E4-C8E5     Used during scrolling                     ; delta added to 'x' drawing point
*
* C8E6          Used during scrolling                     ; 0 if scrolling in a + direction (right
*               or up), $80 if scrolling in a - direction (down or left)
*
* C8E7-C8E8     Work memory (used by DetermineDirectionToMove)
*
* C8E9          Loop counter
*
* C8EA          Mask showing joystick position and fire button state:
*
*                       ---------------------------------
*                       | 0 | 0 | B | L | D | U | R | L |
*                       ---------------------------------
*                 Button Pressed--^
*                 Joystick Left-------^---------------^
*                 Joystick Down-----------^
*                 Joystick Up-----------------^
*                 Joystick Right------------------^
*
* C8EB          Direction current object is traveling (work memory):
*
*                       ---------------------------------
*                       | 0 | 0 | 0 | L | D | U | R | L |
*                       ---------------------------------
*                 Joystick Left-------^---------------^
*                 Joystick Down-----------^
*                 Joystick Up-----------------^
*                 Joystick Right------------------^
*
* C900-C90D     Player 1 structure
* C90E-C91B     Player 2 structure
*
*               byte    meaning
*               ----    -------
*               0-6     player's score
*               7       unused
*               8       number of lives
*               9       Flag specifying if 5000 point bonus was given
*               10      player's y position
*               11      player's x position
; 12 current level
*               12-13   unused
*
* C91C-C973     Array of robot structures (11 entries)
*
*               byte    meaning
*               ----    -------
*               0       robot's y position.
*               1       robot's x position.
*               2       flag: $80 = alive, 0 = gone, $0F = just killed
*               3       direction of travel(0-10)         ; indexes into extremities tbl
*               4       Eye box to draw (0, 2, 4, 6)
*               5       Delay before robot starts shooting (0-7)
*               6       # of bullets robot can fire in a burst
*               7       Delay between processing a robot (moving, firing, etc)
*
* C974-????     Array of bullet structures
*
*               byte    meaning
*               ----    -------
*               0       y position of bullet's tail
*               1       x position of bullet's tail
*               2       y delta between bullet's head and tail
*               3       x delta between bullet's head and tail
*               4       y position of bullet's head
*               5       x position of bullet's head
*               6       velocity factor
*               7       direction of travel               ; anded w/ $F0 when hits something
*
******************************************************************************
                    include  "TAFT.I"
                    org      $0000                        ; 
                    noopt    
                    direct   -1 
                    DB       "g GCE 1982",$80             ;
                    DW       berzerk_music_block          ; 
                    DB       $F8                          ; * height 
                    DB       $50                          ; * width 
                    DB       $20                          ; * rel y 
                    DB       $a8                          ; * rel x 
                    DB       "BERZERK ULTIMATE",$80,$00   ;
                    bra      startCartridge1 

                    ORG      $30 
                    fcb      "ThGS"                       ; magic handshake marker
v4ecartversion      fdb      $0001                        ; I always have a version 
                                                          ; in comm. structs 
v4ecartflags        fdb      $28c0                        ; v4e flags: 
                                                          ; $8000 + always set by v4e 
                                                          ; $4000 - hiscore entry supported 
                                                          ; $2000 - enable cart as ram 
                                                          ; $1000 - supply default font 
                                                          ; $0800 - 1: fast menu switch supported 
                                                          ; set to 0 if hiscore entry 
                                                          ; $0700 + v4e timing bits: 
                                                          ; 0 - heuristic 
                                                          ; 1 - zero 
                                                          ; 2 - one 
                                                          ; 3 - two 
                                                          ; 4 - three 
                                                          ; $0080 - populate storage upon start 
                                                          ; $0040 - 1: extension calls used 
                                                          ; $0020 - gpios used (@7ffe) 
                                                          ; $0010 - free for future use 
                                                          ; $0008 - 1:screensaver enabled 
                                                          ; $0003 - font size 
                                                          ; 
                                                          ; first the variables for the v4e font system 
                                                          ; 
v4efontptr          fdb      0                            ; supplied by app: 
                                                          ; if != 0: the cart uses this 
                                                          ; ptr to supply a font and to 
                                                          ; optimize strings; 
v4efontwidth        fcb      0                            ; supplied by app: 
                                                          ; the cart stores a system 
                                                          ; font at v4efontptr+0x20 and 
                                                          ; adds v4efontwidth per line 
                                                          ; ..must be at least $3f 
v4efontlastchar     fcb      0                            ; supplied by cart:($5e or $7e) 
                                                          ; last char supplied by v4e 
                                                          ; (first one is always 0x20) 
v4estringlists      fdb      0                            ; if !=0 a ptr to a list of ptrs 
                                                          ; containing lists of constant 
                                                          ; strings that can be optimized 
                                                          ; for a given font (0 == end 
                                                          ; of lists) 
                                                          ; 
                                                          ; now the variables for the v4e store/load area 
                                                          ; 
v4eStorageArea      fdb      vec4SaveBuffer               ; pointer to the area - 0: unused 
v4eStorageSize      fdb      v4e_saveBlockEnd-v4e_saveBlockStart ; and its size 
v4eStorageLoaded    fdb      0                            ; set by v4e: return size for a load 
                                                          ; e.g. if set to zero in compile an != 0 
                                                          ; at the start shows whether something was 
                                                          ; loaded via 'populate storage upon start' 
v4eStorageID                                              ;        4 bytes storage identifier 
                                                          ; 
                    fcb      "MBZ2"
                                                          ; end of v4e cart header 
                                                          ; 
defaultValuesEeprom: 
                    db       "MAL", 0, "   100"
copyDefaults 
                    ldd      #$0100;#$0101                       ; vox enabled, color enabed 
                    std      optionsBlock 
                    std      optionsBlock+2 
                    std      optionsBlock+4 
                    std      optionsBlock+6 
                    ldx      #highScoreCompetitionBlock 
                    ldy      #highScoreHardcoreBlock 
                    ldb      #5                           ; 5 * 10 
                    stb      tmp1 
copyEvenMore: 
                    ldb      #10                          ; 5 * 10 
                    ldu      #defaultValuesEeprom 
copyMore: 
                    lda      ,u+ 
                    sta      ,x+ 
                    sta      ,y+ 
                    decb     
                    bne      copyMore 
                    dec      tmp1 
                    bne      copyEvenMore 
                    rts      

startCartridge1 
                    jsr      voxInit 
                    ldu      #aks_song 
                    jsr      PLY_INIT 
                    clr      isDemo 
startCartridge 
                    clr      isColoredMode 
                    clr      ignoreDs2431                 ; we assume a eEprom is available 
                    if       USE_PB6 = 1 
                    else     
                    inc      ignoreDs2431                 ; no, than set ignore flag 
                    endif    
                    ldx      v4eStorageLoaded             ; check if v4e did load bytes from storage area 
                    bne      noScoreDefaults              ; if != 0, than yes -> jump 
                    bsr      copyDefaults                 ; initiate eEprom Highscore defaults 
                    lda      v4ecartflags                 ; check if there is any v4e at all? 
                    bpl      nov4e                        ; if not (positive) jump 
                    jsr      COPY_RAM_TO_VECROM           ; otherwise fill the (usual) ROM with default values (v4e this is RAM to!) 
nov4e 
noScoreDefaults 
                    jsr      checkEprom                   ; is there an eprom (ignore flag always checked in eeprom routines) 
                                                          ; eprom saves valus in three stages, to keep times smaller (when only block save is needed) 
                    ldd      #(EEPROM_STORESIZE_OPTIONS*256)+EEPROM_OPTION_BLOCK 
                    std      current_eprom_blocksize 
                    ldx      #optionsBlock 
                    jsr      eeprom_load_option           ; load eprom data (also resets if no data found) 
                    lda      v4ecartflags                 ; check if there is any v4e at all? 
                    bmi      donotSaveagain               ; if yes, than don't save the same stuff 3 times, poor Flash... 
                    ldd      #(EEPROM_STORESIZE_HS*256)+EEPROM_HS1_BLOCK 
                    std      current_eprom_blocksize 
                    ldx      #highScoreCompetitionBlock 
                    jsr      eeprom_load_highscore        ; load eprom data (also resets if no data found) 
                    ldd      #(EEPROM_STORESIZE_HS*256)+EEPROM_HS2_BLOCK 
                    std      current_eprom_blocksize 
                    ldx      #highScoreHardcoreBlock 
                    jsr      eeprom_load_highscore        ; load eprom data (also resets if no data found) 
donotSaveagain 
Vec_High_Score      EQU      $CBEB                        ;High score storage (7 bytes) 
                    ldx      #highScoreTable 
                    leax     4,x                          ; scip name and level 
                    ldd      ,x++ 
                    std      Vec_High_Score 
                    ldd      ,x++ 
                    std      Vec_High_Score+2 
                    ldd      ,x++ 
                    std      Vec_High_Score+2+2 
; done with one times
;
*
* Get the number of players: 1 or 2.
* C882 will be set to 0 for 1 player, and 1 for 2 players.
start: 
                    jsr      initSavedHighscore 
                    jsr      introMalban 
                    jsr      rasterIntro 
                    jsr      voxInit 
;
                    direct   $c8 
                    clr      chickenFlag 
                    ldd      #$0200                       ; 
                    jsr      select_game                  ; get_players_game ; 
                    lda      <$79                         ; 
                    asra                                  ; 
                    sta      <$82                         ; 
                    clr      isScrolling 
                    clr      robotShotCounter 
*
* Disable 2 of the 4 joystick mask registers              ; the other
* 2 are enabled automatically by the ExecRom.
                    clr      <$21                         ; 
                    clr      <$22                         ; 
                    jsr      init_music_buf               ; 
                    clr      <soundTypes                  ; 
*
* Initialize the 2 player's structures.
* Player 1 is C900-C90D, player 2 is C90E-C91B.
* The player's scores will be initialized, their
* number of lives set (3), and their initial positions
* set.
RestartBerzerk: 
                    jsr      dptoC8                       ; 
 clr counter16
 if LIMIT_SUPPORT = 1
 clr limitReached
 endif

                    clr      <$9D                         ; 
                    inc      <$9D                         ; 
                    ldx      #$C900                       ; 
                    stx      <$80                         ; 
P003A:              lda      #$03                         ; 
                    sta      8,x                          ; * Set number of lives 
                    clr      9,x                          ; * Clear 'bonus given' flag 
                    clr      12,x                         ; levels played 
P0040:              jsr      set_dft_score                ; * Set default score 
                    ldx      #$C90E                       ; 
                    dec      <$9D                         ; 
                    bpl      P003A                        ; 
                    ldx      #$C900                       ; 
                    ldd      #$00A5                       ; 
                    std      10,x                         ; * Set starting position 
                    ldx      #$C90E                       ; 
                    ldd      #$005B                       ; 
                    std      10,x                         ; * Set starting position 
* Set initial robot attributes
                    lda      #$A0                         ; 
                    sta      <$A8                         ; * Robot seeding threshold 
                    lda      #$06                         ; 
 

P0060:              sta      <$A7                         ; * Set robot processing delay 
                    lda      #$01                         ; * Start robot bullets with 
                    sta      <$84                         ; * a low velocity factor 



 if BERZERK_DEBUG = 1

 lda #ROBOT_MIN_UPDATE
 sta <$A7

 ldx #$C87D ; Vec_Random_Seed
; lda #16
; sta ,x+
; lda #16
; sta ,x+
; lda #17
; sta ,x+

 lda #16
 sta ,x+
 lda #16
 sta ,x+
 lda #17
 sta ,x+

 ldx      #$C900                       ; 
 lda #10
 sta      8,x                          ; * Set number of lives 
 lda #START_LEVEL
 sta      12,x                         ; levels played 


 lda #$33
 sta $C900 + 1 ; player 0 highscore - the "middle" digits xx !! xx

 ldd #$3830 ; start with score of 8000 - max difficulty = 6
 std $C900 + 2 ; player 0 highscore - the "middle" digits xx !! xx

 ldd #$3030 ; start with score of 8000 - max difficulty = 6
 std $C900 + 4 ; player 0 highscore - the "middle" digits xx !! xx


 if LIMIT_SUPPORT = 1
 clr limitReached
 ldx $c880
 lda 1,x ; 5th digit of player score
 cmpa #$31 ; > 10000 thousand
 blo notLimitReachedYet_
 inc limitReached
notLimitReachedYet_
 endif

 endif


*
* This block takes care of setting up for the next turn,
* after a player has died.  It determines how the maze
* will look, displays the maze, the scores and the number
* of lives, and then does some music mucking.
StartNewTurn: 
 lda #SCHONZEIT
 sta schonzeitTimer
 lda #SCHONZEIT2
 sta schonzeitTimer2

                    ldd      #0 
                    std      RecalCounter 
                    jsr      colorSetup 
                    jsr      initBulletList 
                    clr      currentMapVelocity 
                    clr      currentCombinedVelocityDirection 


                    lda      #$3F                         ; * Set loop counter 
                    sta      <$85                         ; 
                    ldx      #$C8C5                       ; * Initialize game RAM 
                    ldb      #$2d                         ; 
                    jsr      clear_blockxb                ; 
                    jsr      DetermineMazeWalls           ; 
                    ldx      #$C8D1                       ; * Initialize the bonus string 
                    jsr      set_dft_score                ; 
                    ldx      <$80                         ; * Save the player's position 
                    ldd      10,x                         ; 
                    std      <player_y                    ; 
                    lda      8,x                          ; * Display # of lives left 
                    jsr      DisplayLives                 ; 
*
* Display the maze, but not the player or the robots,
* and do some sound mucking                               ; loop here for a while,
* before proceeding.
LoopWhileDisplayingMaze: 
                    clr      <$86                         ; 
                    jsr      playGameSounds               ; 
                    jsr      init_sound                   ; 
                    lda      <$26                         ; 
                    bita     #$10                         ; 
                    bne      P0096                        ; 
                    dec      <$86                         ; 
P0096:              jsr      waitrecal                    ; 
                    jsr      voxSentence 
                    jsr      DrawMaze                     ; 
                    jsr      do_sound                     ; 
                    jsr      dptoC8                       ; 
                    dec      <$85                         ; 
                    bne      LoopWhileDisplayingMaze      ; 
*
* Remove one of the player's lives, to take into account
* the life he is using now.
                    ldx      <$80                         ; * Points to active player struct 
                    dec      8,x                          ; 
                    jsr      init_music_buf               ; 
                    bra      P00B5                        ; 

EnterNextMaze: 
 lda #SCHONZEIT
 sta schonzeitTimer
 lda #SCHONZEIT2
 sta schonzeitTimer2
                    jsr      dptoC8                       ; 
                    jsr      DetermineMazeWalls           ; 
                    clr      isScrolling 
                    clr      robotShotCounter 
P00B5:              lda      #$2A                         ; 
                    sta      <$86                         ; 
                    ldu      <$80                         ; 
                    ldd      10,u                         ; 
                    std      <player_y                    ; * Save the player's position 
                    std      <$95                         ; 
                    ldx      #$C8E7                       ; 
                    ldb      #$05                         ; 
                    jsr      clear_blockxb                ; 
                    clr      <$83                         ; 
                    dec      <$83                         ; 
*
* Based on the player's score, set the level of difficulty.
* If the score >= 10,000, then bump the difficulty by alot.
                    *        After that, compare the 'thousands' portion of the score
* against the cutoff values in the table, and further bump
* for each cutoff value surpassed.
                    lda      1,u                          ; * Check if score >= 10,000 
                    cmpa     #$20                         ; 
                    beq      P00D7                        ; 
                    lda      #ROBOT_BURST_FIRE_MAX                         ; * It is, so increase difficulty 
                    sta      <$83                         ; 
P00D7:              ldd      2,u                          ; * Check the cutoff values 
                    ldx      #SkillLevelCutoffs           ; 
P00DC:              inc      <$83                         ; 
                    tst      ,x                           ; 
                    bmi      P00E7                        ; 
                    cmpd     ,x++                         ; 
                    bge      P00DC                        ; 
*
* Initialize the bonus score string.
                    if       START_LEVEL = 0 
                    else     
                    lda      #START_LEVEL 
                    sta      <$83 
                    endif    
P00E7:              ldx      #$C8D1                       ; 
                    jsr      set_dft_score                ; 
                    jsr      colorSetup 
*
* Do some processing based on the difficulty value calculated
* earlier (set the robot's bullet velocity and the robot
* processing delay).  Lastly, clear out the bullet structure.
;                    lda      <$83                         ; * Max # of robot bullets 
;                    cmpa     #$06                         ; 
;                    blt      P00F5                        ; 
                    inc      <$84                         ; * Robot bullet velocity factor 
                    lda      <$84 ; Robot bullet velocity factor 
                    cmpa     #MAX_BULLET_SPEED 
                    bls      P00F5 
                    lda      #MAX_BULLET_SPEED 
P00F5:              
                    sta      <$84

 dec <$A7
 lda  <$A7 ; processing delay
 cmpa #ROBOT_MIN_UPDATE
 bgt updateRobotTimerOK
 lda #ROBOT_MIN_UPDATE
updateRobotTimerOK

 if LIMIT_SUPPORT = 1
 tst limitReached
 beq noMinUpdateChange
 clra ; no min update when limit reached
noMinUpdateChange
 endif
                    sta      <$A7                         ; * Robot processing delay 
                    if       DALEK = 1 
                    clra     
                    sta      <$A7 
                    endif    
*
* This seeds the robot structure array.  It attempts to
*        'activate' a random number of robots.  If it fails to
* seed any robots, then it tries over again.
* The robot array is an array of structures, located
* at C91C-C973                                            ; there are 11 entries, each 8 bytes long.
* C8C7 keeps track of the number of robots initially
* seeded                                                  ; C8C8 keeps track of the number of robots still
* alive.  A robot is seeded only if the generated random
* number is less than a pre-calculated threshold value.
P0104:              clr      <$C7                         ; 
                    ldb      #$0B                         ; 
                    ldx      #RobotStartingPositionTable  ; 
                    ldu      #$C91C                       ; 
P010E:              jsr      get_random_a                 ; 
                    cmpa     <$A8                         ; * Should we seed this robot? 
                    bhi      P014C                        ; 
                    lda      #$80                         ; 
                    sta      2,u                          ; * Flag the robot as alive 
                    jsr      get_random_a                 ; 
                    anda     #$1F                         ; 
P0120:              sta      <$9D                         ; 
                    suba     #$10                         ; 
                    adda     ,x                           ; 
                    sta      ,u                           ; * Set the y starting position 
                    jsr      get_random_a                 ; 
                    anda     #$0F                         ; 
                    suba     #$08                         ; 
                    adda     1,x                          ; 
                    sta      1,u                          ; * Set the x starting position 

;--
 pshs b
 suba player_x
 bpl _xpos2_
 nega
_xpos2_
 asra

 ldb ,u
 subb player_y
 bpl _ypos2_
 negb
_ypos2_
 asrb
 stb divide_tmp
 puls b 
 adda divide_tmp
 cmpa #ROBOT_THRESHOLD
 bgt robotPosOk
 bra P014C
robotPosOk
;--
                    inc      <$C7                         ; * Bump the robot count 


                    clr      3,u                          ; 
                    clr      4,u                          ; 
                    jsr      get_random_a                 ; 
                    anda     #$07                         ; 
                    sta      5,u                          ; 
                    lda      <$83                         ; 
                    cmpa     #ROBOT_BURST_FIRE_MAX                         ; 
                    blt      P0146                        ; 
                    suba     #ROBOT_BURST_FIRE_MAX                         ; 
P0146:              sta      6,u                          ; 
                    jsr      get_random_a                 ; 
                    anda     #$f 
                    adda     <$A7 

 if LIMIT_SUPPORT = 1
 tst limitReached
 clra
 beq randomReactionTime
randomReactionTime
 endif
                    sta      7,u 
                    leau     8,u                          ; * Change ptr to the next robot 
P014C:              leax     2,x                          ; 
                    decb                                  ; 
                    bne      P010E                        ; 
                    lda      <$C7                         ; 
                    beq      P0104                        ; 
                    sta      <$C8                         ; 




* C91C-C973     Array of robot structures (11 entries)
*
*               byte    meaning
*               ----    -------
*               0       robot's y position.
*               1       robot's x position.
*               2       flag: $80 = alive, 0 = gone, $0F = just killed
*               3       direction of travel(0-10)         ; indexes into extremities tbl
*               4       Eye box to draw (0, 2, 4, 6)
*               5       Delay before robot starts shooting (0-7)
*               6       # of bullets robot can fire in a burst
*               7       Delay between processing a robot (moving, firing, etc)

* Set value used during robot seeding
                    tfr      u,d                          ; 
                    subd     #$C924                       ; 
                    stb      <$AA                         ; 
                    clr      <$A9 
* Initialize Otto information
                    ldd      <$95                         ; 
                    std      <$91                         ; * Set Otto's initial position 
                    std      <$93                         ; * Set Otto's initial position 
                    lda      #$05                         ; 
                    sta      <$30                         ; 
                    lda      #$20                         ; 
                    sta      <$97                         ; * Set Otto's sound loop counter 
                    lda      #$01                         ; 
                    sta      <$98                         ; * Set Otto's initial velocity 
* Set up some counters
; setup otto
                    lda      #INITIAL_OTTO_TIMER          ; five at level 0 
                    ldu      $C880                        ; 
                    adda     <$C7                         ; plus count of robots 

                    suba     12,u                         ; reduced by # levels played 


                    suba     #(OTTO_MIN_TIME/50)
                    bmi      doMinOttoTime 
                    adda     #(OTTO_MIN_TIME/50)
                    ldb      #50                          ; times seconds 
                    mul      
                    bra      nominotto 
doMinOttoTime 
                    ldd      #(OTTO_MIN_TIME)                         ; min 3s 
nominotto 
                    std      <$2E 
; ------ ORG
                    if       ORIGINAL_OTTO_APPEARANCE = 1 
                    lda      <$C7                         ; Number of robots initially seeded into the maze 
                    ldb      #$50                         ; 
                    mul                                   ; 
                    tsta                                  ; 
                    bne      P017B                        ; 
; OTTO TIMING
; $200 till he appears
                    lda      #$02                         ; 
                    clrb                                  ; 
P017B:              std      <$2E                         ; 
                    endif    
; ------ ORG
* Main processing loop
                    clr      <mainJumpIndex               ; * Set jump idx to 'player is alive' 
P017F: 

 lda schonzeitTimer
 beq noSchonZeit
 dec schonzeitTimer
noSchonZeit
 lda schonzeitTimer2
 beq noSchonZeit2
 dec schonzeitTimer2
noSchonZeit2


 if LIMIT_SUPPORT = 1
 clr limitReached
 ldx $c880
 lda 1,x ; 5th digit of player score
 cmpa #$31 ; > 10000 thousand
 blo notLimitReachedYet

; bhi limit20000

 lda #Vec_Loop_Count+1
 anda #1
 beq notLimitReachedYet
limit20000
 inc limitReached
notLimitReachedYet
 endif

                    jsr      waitrecal                    ; old ; 







                    jsr      voxSentence 
                    jsr      read_jstick                  ; old ; 
; old
                    jsr      read_switches2               ; 
; for vecVox disable buttons on 2. pad
                    lda      >$c80F 
                    anda     #$f 
                    sta      >$c80F 
                    jsr      randomVox 
                    jsr      do_sound                     ; 
                    jsr      DrawMaze                     ; 
                    jsr      dptoC8                       ; 
                    ldx      #MainJumpTable               ; 
                    lda      <mainJumpIndex               ; 
                    jmp      [a,x]                        ; * (UNKNOWN JUMP) 

MainJumpTable: 
                    DW       PlayerIsAlive                ; 
                    DW       PlayerHasLeftMaze            ; 
                    DW       PlayerHasDied                ; 
MakeSoundThenGotoMainLoop: 
                    tst      <$56                         ; 
                    beq      P01A4                        ; 
                    clr      <soundTypes                  ; 
P01A4:              jsr      init_sound                   ; 
                    jsr      playGameSounds               ; 
                    ldx      <$2E                         ; 
                    beq      P01B0                        ; 
                    leax     -1,x                         ; 
P01B0:              stx      <$2E                         ; 
                    bra      P017F                        ; 

*
*        This is one of the 'indirect jump' functions.  It is the
* active jump function while the player is alive.  It uses
* the state of the buttons and the joystick to decide how
* to draw the player.  It also attempts to detect whenever
* the player leaves a quadrant, to determine if the player
* has left the maze, or run into a wall.
PlayerIsAlive: 
                    ldx      #player_y                    ; 
                    lda      <$F0                         ; 
                    beq      ProcessJoystickAndButtons    ; 
                    dec      <$F0                         ; 
                    bra      DoNotUpdatePlayerMovement    ; 

*
*        Based on the joystick and button values, the 'b' register
* is set to the following mask:
*
*         ---------------------------------
*         | 0 | 0 | B | L | D | U | R | L |
*         ---------------------------------
*   Button Pressed--^
*   Joystick Left-------^---------------^
*   Joystick Down-----------^
*   Joystick Up-----------------^
*   Joystick Right------------------^
ProcessJoystickAndButtons: 
                    clrb                                  ; 
                    lda      <$1C                         ; * Get joystick up/down setting 
                    bpl      P01C8                        ; * 0=u/d neutral, -=down, +=up 
                    addb     #$08                         ; * Down 
                    bra      P01CC                        ; 

P01C8:              beq      P01CC                        ; 
                    addb     #$04                         ; * Up 
P01CC:              lda      <$1B                         ; * Get joystick left/right setting 
                    bpl      P01D4                        ; * 0=l/r neutral, -=left, +=right) 
                    addb     #$11                         ; * Left 
                    bra      P01D8                        ; 

P01D4:              beq      P01D8                        ; 
                    addb     #$02                         ; * Right 
P01D8:              lda      <$0F                         ; * Load button states 
                    beq      NoBtnsPressed                ; 
                    orb      #$20                         ; * Button pressed, but if joystick is 
                    bitb     #$0F                         ; * neutral, then do nothing. 
                    beq      DoNotUpdatePlayerMovement    ; 
*
* The joystick is not in the neutral position, and a button
* is pressed, so the player is firing.
                    stb      <$F1                         ; * Save the joystick/button mask 
                    lda      #$08                         ; 
                    sta      <$F0                         ; 
                    andb     #$0F                         ; 
                    stb      <$F2                         ; * Save just the joystick mask 
                    lda      #$06                         ; 

                    jsr      CalculateUpdatedPosition     ; 


                    std      <$9E                         ; 
                    ldx      #$C89E                       ; 
                    ldb      #$03                         ; 
                    jsr      FireBullet                   ; 
                    beq      FireError1
                    lda      #1 
                    sta      B_ORIGIN, x                  ; is a player bullet! 
FireError1
                    lda      #SOUND_PLAYER_BULLET_START_BIT ; 
                    ora      <soundTypes                  ; 
                    sta      <soundTypes                  ; 
                    bra      DoNotUpdatePlayerMovement    ; 

NoBtnsPressed: 
                    stb      <$F1                         ; * Save the joystick/button mask 
                    lda      <$26                         ; 
                    bita     #$02                         ; 
                    bne      DoNotUpdatePlayerMovement    ; 

 ldu      $C880 ; current player
 lda      12,u                         ; levels played 1
 sta option8
                    lda      #$01                         ; 
                    jsr      CalculateUpdatedPositionPlayer     ; 
                    std      ,x                           ; 
*
* If none of the buttons are pressed, and if the joystick
* is in neutral, then reset the players arms and legs to
                    *        the 'neutral' position.
DoNotUpdatePlayerMovement: 
                    lda      <$F1                         ; 
                    bne      CheckIfFiring                ; 
                    clr      <PlayerLegState                         ; 
                    clr      <PlayerArmState                         ; 


                    bra      CheckForPlayerLeavingQuadrant ; 

*
* If a button was pressed, then we know the player was
* firing, so we can force his legs to the neutral position,
* and can set his arms to be shooting in the appropriate
* direction.
CheckIfFiring: 
                    bita     #$20                         ; 
                    beq      NotFiringButMoving           ; 
                    clr      <PlayerLegState                         ; * Force player to stand still 

                    ldu      #ArmsShootingIndicesTable    ; 
                    anda     #$0F                         ; 
                    ldb      a,u                          ; 
                    stb      <PlayerArmState                         ; * Set player's arms accordingly 
                    bra      CheckForPlayerLeavingQuadrant ; 

*
* We know that the player is on the move.  We will therefore
* make his arms swing left and right, and we will draw his
* legs so it appears he is running in the desired direction.
* The timing for changing the arms and legs is controlled by
* bit $08 of C826, which is a counter controlled by the
* Exec Rom.
*
* For the arms:
*
*        0 = 'arms to the right'
*        2 = 'arms to the left'
*
* For the legs:
*
*        2 = 'running right with wide legs'
*        4 = 'running right with close legs'
*        6 = 'running left with wide legs'
*        8 = 'running left with close legs'
NotFiringButMoving: 
                    anda     #$10                         ; * a = joystick/button mask 
                    pshs     a                            ; 
                    lda      <$26                         ; 
                    anda     #$08                         ; 
                    tfr      a,b                          ; * Get a value of 0 or 2, to control 
                    asrb                                  ; * which arms get drawn. 
                    asrb                                  ; 
                    stb      <PlayerArmState                         ; 
                    ora      ,s+                          ; * Get a value of 2, 4, 6 or 8, to 
                    asra                                  ; * control which legs get drawn. 
                    asra                                  ; 
                    adda     #$02                         ; 
                    sta      <PlayerLegState                         ; 

*
* Take the player's position, and map it into:
*
*   1) the quadrant the player is in (C89A),
*   2) the relative position within that quadrant (C89B-C89C)
*   3) the bitmask indicating which walls are present for
*      the quadrant (C899).
*
* If it is determined that there was contact with a wall,
* then do some special checks to see if the wall was really
* a door                                                  
; if it was a door, and the door was not blocked off,
* then exit to the next maze.
CheckForPlayerLeavingQuadrant: 
                    ldd      <player_y                    ; 
                    jsr      MapPointToQnMakeRel          ;MapPointToQuadrantAndMakeRelative; 
                    CHECK_FOR_QUADRANT_EDGE_CROSSING  PlayerThresholds 
                    bitb     <$99                         ; 
                    lbeq     NoContactWithWalls           ; 
* Check for passing out the top door
                    lda      <quadrantOfPoint             ; 
                    cmpa     #$02                         ; * If quad 2, check for up door 
                    bne      P0264                        ; 
                    lda      <$C6                         ; * Check if door is blocked 
                    bita     #$10                         ; 
                    bne      PlayerHitWall                ; 
                    andb     #$04                         ; * Top wall (door) passed thru? 
                    beq      PlayerHitWall                ; 
                    bra      ExitMaze                     ; 

* Check for passing out the left door
P0264:              cmpa     #$05                         ; * If quad 5, check for left door 
                    bne      P0274                        ; 
                    lda      <$C6                         ; 
                    bita     #$04                         ; * Check if door is blocked 
                    bne      PlayerHitWall                ; 
                    andb     #$01                         ; * Left wall (door) passed thru ? 
                    beq      PlayerHitWall                ; 
                    bra      ExitMaze                     ; 

* Check for passing out the right door
P0274:              cmpa     #$09                         ; * If quad 9, check for right door 
                    bne      P0284                        ; 
                    lda      <$C6                         ; 
                    bita     #$02                         ; * Check if door is blocked 
                    bne      PlayerHitWall                ; 
                    andb     #$02                         ; * Right wall (door) passed thru ? 
                    beq      PlayerHitWall                ; 
                    bra      ExitMaze                     ; 

* Check for passing out the bottom door
P0284:              cmpa     #$0C                         ; * If quad 12, check for bottom door 
                    bne      PlayerHitWall                ; 
                    lda      <$C6                         ; 
                    bita     #$08                         ; * Check if door is blocked 
                    bne      PlayerHitWall                ; 
                    andb     #$08                         ; * Bottom wall (door) passed thru ? 
                    beq      PlayerHitWall                ; 
*
* The player has successfully exited from the maze        ; change
* the jump index so that the code is called which scrolls
* the maze off the display.
ExitMaze: 
                    stb      <$F1                         ; 
                    ldx      $c880                        ; player struct 
                    inc      12,x                         ; levels played 
                    lda      $C8C8                        ; number of alive robots 
                    beq      noenemiesleft 
                    FORCE_INIT_VOX_SPEECH_DIRECT  chickenSentence, pitchNormal0 
                    lda      #1 
                    sta      chickenFlag 
                    bra      speechMazeExitDone 

noenemiesleft 
                    clr      chickenFlag 
                    jsr      get_random_a 
                    bmi      variant2 
                    FORCE_INIT_VOX_SPEECH_DIRECT  mustNotEscape1, pitchNormal0 
                    bra      speechMazeExitDone 

variant2 
                    FORCE_INIT_VOX_SPEECH_DIRECT  mustNotEscape2, pitchNormal0 
speechMazeExitDone 
                    ldb      #$02                         ; 
                    stb      <mainJumpIndex               ; 
                    inc      isScrolling 
                    jsr      initBulletList               ; remove all bullets 
                    jmp      MakeSoundThenGotoMainLoop    ; 

*
* The player died by walking into a wall                  ; change the
* jump index so that the code which shows the player being
* fried, is called.
PlayerHitWall: 
                    lda      #SOUND_PLAYER_DEATH_START_BIT ; 
                    ora      <soundTypes                  ; 
                    sta      <soundTypes                  ; 
                    ldb      #$04                         ; 
                    stb      <mainJumpIndex               ; 
                    jmp      MakeSoundThenGotoMainLoop    ; 

*
* The player did not make contact with any walls, so all
* is well.  Starting at the end of the bullet array, work
* back until we find an active bullet, then update the
* bullet counter in C8CC.
NoContactWithWalls: 
                    jsr      ProcessAndUpdateBullets      ; 
                    jsr      ProcessAndUpdateRobots       ; 
*
* Display the number lives the player has, and then check
* to see if the player has earned an extra life (awarded
* for each 5000 points)                                   ; if so, then award the extra life,
* and play a little tune.  The bonus check happens by
*        comparing the thousands digit of the score to ascii '5'.
CheckForExtraLifeBonus: 
                    ldx      <$80                         ; 
                    lda      8,x                          ; 
                    bsr      DisplayLives                 ; 

*               0-6     player's score
*               7       unused
*               8       number of lives
*               9       Flag specifying if 5000 point bonus was given


                    tst      9,x                          ; * Semaphore; only do once 
                    bne      CheckForClearedLevelBonus    ; 
                    lda      #$35                         ; * "5" 
                    cmpa     2,x                          ; 
                    beq      doBonusLife
                    lda      #$30                         ; * "5" 
                    cmpa     2,x                          ; 
                    bne      CheckForClearedLevelBonus    ; 
doBonusLife

                    inc      8,x                          ; * Award the extra life 
                    inc      9,x                          ; * Set the semaphore 
                    ldu      #$FF44                       ; * Play a tune 
                    jsr      init_sound2                  ; 
*
* Check to see if the player has cleared out all of
* the robots                                              ; if so, then award a bonus, and display
* the bonus string.  Some fancy manipulation is done
* to convert the integer into a BCD string value.
CheckForClearedLevelBonus: 

; enable bonus points again
                    lda      #$36                         ; * "5" 
                    cmpa     2,x                          ; 
                    bne noSixer
                    clr      9,x                          ; * Set the semaphore 
noSixer
                    lda      #$31                         ; * "5" 
                    cmpa     2,x                          ; 
                    bne noOner
                    clr      9,x                          ; * Set the semaphore 
noOner


                    tst      <$C8                         ; * Any robots left? 
                    bne      P0312                        ; 
                    lda      #$02                         ; * All robots have been cleared out 
                    bita     <$98                         ; 
                    bne      P0312                        ; 
                    inc      <$98                         ; * Increment Otto's velocity 
                    clra                                  ; 
                    ldb      <$C7                         ; * Get the starting count of robots 
                    cmpb     #$0A                         ; 
                    blt      P02FD                        ; 
                    subb     #$0A                         ; * Handle counts >= 10 specially 
                    orb      #$10                         ; 
P02FD:              aslb                                  ; * Convert to BCD string value 
                    rola                                  ; 
                    aslb                                  ; 
P0300:              rola                                  ; 
                    aslb                                  ; 
                    rola                                  ; 
                    aslb                                  ; 
                    rola                                  ; 
                    pshs     a,b                          ; * Add bonus to player's score 
                    jsr      add_d_to_x_in_bcd            ; 
                    puls     a,b                          ; 
                    ldx      #$C8D1                       ; * Place value into the bonus string 
                    jsr      add_d_to_x_in_bcd            ; 
P0312:              jmp      MakeSoundThenGotoMainLoop    ; 

*
* DisplayLives()
*
* Entry:
*   a = number of lives
*
*        Displays a string of 'little men', one for each live
* the user has.  The buffer into which the string is
* placed starts at C8D8.
*
*  BUG ALERT: there is no check to see if the C8D8 buffer
*             is overrun                                  ; what is the intended size of this
*             buffer?
DisplayLives: 
    cmpa #4
    bgt manyLives

                    clrb                                  ; 
                    ldu      #$C8D8                       ; * String buffer 
                    sta      <$9D                         ; 
                    beq      P0326                        ; 
                    lda      #$69                         ; * 'little man' icon 
P031F:              sta      b,u                          ; 
                    incb                                  ; 
                    cmpb     <$9D                         ; 
                    blt      P031F                        ; 
P0326:              lda      #$20                         ; 
                    sta      b,u                          ; * Terminate string with 2 spaces 
                    incb                                  ; * and $80. 
                    sta      b,u                          ; 
                    incb                                  ; 
                    lda      #$80                         ; 
                    sta      b,u                          ; 
                    rts                                   ; 

; display + at the end of 4 icons
manyLives
                    clrb                                  ; 
                    ldu      #$C8D8                       ; * String buffer 
                    lda      #$69                         ; * 'little man' icon 
                    sta      ,u+                          ; 
                    sta      ,u+                          ; 
                    sta      ,u+                          ; 
                    sta      ,u+                          ; 
 lda #'+'
                    sta      ,u+                          ; 
 lda      #$20                         ; 
                    sta      ,u+                          ; 
    lda      #$80                         ; 
    sta      ,u+                          ; 
                    rts                                   ; 


*
* FireBullet()
*
* This function attempts to fire a bullet from the indicated
* position, using the specified velocity.  However, if the
* bullet would immediately hit a wall, then it is not fired.
*
* Entry:
*    b = bullet's velocity factor (a value indicating how
*        often the bullet's position is updated for each
*        pass).
*    x = Bullet's starting position.
*    C8EB = direction of travel for the bullet.
* exit success Z = 1, error Z = 0
* X = bullet structure
noShot
 clra ; error
 rts

FireRobotBullet
 tst schonzeitTimer2 ; no shot in schonzeit 2
 bne noShot

; test whether bullet is to close to player
player_y            =        $C88F 
player_x            =        $C890 
 pshs b 
 ldd ,x
 ; a = y
 ; b = x

 suba player_y
 bpl _ypos_
 nega
_ypos_
 asra

 subb player_x
 bpl _xpos_
 negb
_xpos_
 asrb
 stb divide_tmp
 adda divide_tmp
 puls b

 cmpa #FIRE_THRESHOLD
 blt noShot

 ; if lower, then a speed threshold, reduce bullet speed
 cmpa #BULLET_SPEED_THRESHOLD
 bhi FireBullet
 incb
 lsrb


; while in "schonzeit" 
; bullet speed is again halved! 
 tst schonzeitTimer
 beq FireBullet

 incb
 lsrb




FireBullet: 
                    stb      <$9D                         ; 
                    pshs     x,u                          ; 
                    ldd      ,x                           ; 
                    jsr      MapPointToQnMakeRel          ;MapPointToQuadrantAndMakeRelative; 
                    ldx      #BulletThresholds            ; 
                    jsr      CheckForQuadrantEdgeCrossing ; 
                    puls     x                            ; * Don't fire if bullet will 
                    bitb     <$99                         ; * immediately hit a wall. 
                    bne      P0369_2                      ; 
                    jsr      newBulletObject              ;#isfunction 
                    cmpu     #OBJECT_LIST_COMPARE_ADDRESS 
                    bls      P0369_2                        ; no bullet available 
                    clr      B_ORIGIN, u                  ; is a per default no player bullet! 
                    lda      <$F2                         ; 
                    sta      B_DIRECTION,u                ; * Store bullet's direction 
                    lda      <$9D                         ; 
                    sta      B_VELOCITY,u                 ; * Store velocity factor 
                    ldd      ,x                           ; 
                    std      B_YPOS,u                     ; * Set bullet's tail position 
                    ldb      <$F2                         ; 
                    lda      #$02                         ; 
                    jsr      CalculateUpdatedPosition     ; 
                    std      B_YPOS_TAIL,u                ; * Set bullet's head position 
                    suba     B_YPOS,u                     ; 
                    subb     B_XPOS,u                     ; 
                    std      B_HEIGHT,u                   ; 
                    leax     ,u                           ; return to caller, this should always clear z flag
P0369:              puls     u,pc  
P0369_2             
                    clra                                  ; set Z flag
                    puls     u,pc  ; 
*
*        This is one of the 'indirect jump' functions.  It is the
* active jump function when the player gets killed.  It
* will cause the player to be drawn with a variety of
* intensities (simulating electrocution), and will update
* the current high score, if necessary.  It then resets
* the starting positions for the players, and tries to
* determine whose turn it is next.  If there are two
* players, then it tries to switch to the other player,
* unless that player has 0 lives left                     ; in that case, it
* will stick with the current player, unless he, too, has
* 0 lives left, at which point the game ends, and it
* waits for either the user to press a button to restart
* the game, or a timeout to elapse (to cold start the
* system).
PlayerHasDied: 
                    lda      pitch_addr 
                    bne      noVoxAgain 
                    INIT_VOX_SPEECH_DIRECT  gotSentence, pitchNormal0 
noVoxAgain 
                    clr      <$F1                         ; 
                    clr      <$CD                         ; 
                    jsr      ProcessAndUpdateRobots       ; 
                    jsr      ProcessAndUpdateBullets      ; 
                    lda      <$86                         ; 
                    anda     #$0F                         ; 
                    ldu      #PlayerFriedScales           ; 
                    ldb      a,u                          ; 
                    stb      <$D0                         ; * Save off next intensity value 
                    dec      <$86                         ; 
                    lbpl     MakeSoundThenGotoMainLoop    ; 
                    lda      #$A0                         ; 
                    sta      <$A8                         ; 
                    ldx      <$80                         ; * Check for new high score 
                    ldu      #$CBEB                       ; 
                    jsr      check_4_new_hi_score         ; 
* Loop, doing some music mucking
                    lda      #$50                         ; 
                    sta      <$85                         ; 
                    clr      <$86                         ; 
                    dec      <$86                         ; 
P039A: 
                    if       NO_OTTO = 1 
                    else     
                    jsr      CheckForOttoStart            ; 
                    endif    
                    jsr      playGameSounds               ; 
                    jsr      waitrecal                    ; 
                    jsr      voxSentence 
                    jsr      DrawMaze                     ; 
                    jsr      do_sound                     ; 
                    jsr      dptoC8                       ; 
                    dec      <$85                         ; 
                    bne      P039A                        ; 
                    ldx      $c880                        ; * player 
                    tst      8,x                          ; * had any lives left. 
                    bne      noHighscoreTest 
                    jsr      testHighScore 
; sjksg
noHighscoreTest 
* Reset player's starting positions
                    ldx      #$C900                       ; * Player 1 
                    ldd      #$00A5                       ; 
                    std      10,x                         ; 
                    ldx      #$C90E                       ; * Player 2 
                    ldd      #$005B                       ; 
                    std      10,x                         ; 
* Determine whose turn it is
                    clr      <$C6                         ; * Start with no door blocked 
                    tst      <$82                         ; * Two player game? 
                    beq      P03D9                        ; 
                    ldx      #$C900                       ; * Yes 
                    tst      <$81                         ; * Is player 2 the active player? 
                    bne      P03D0                        ; 
                    ldx      #$C90E                       ; * No, so change to player 2 
P03D0:              tst      8,x                          ; * Does this player have any lives? 
                    beq      P03D9                        ; 
                    stx      <$80                         ; * Yep; so let this player play 
                    jmp      StartNewTurn 

P03D9:              ldx      <$80                         ; * Nope; so see if original player 
                    tst      8,x                          ; * had any lives left. 
                    lbne     StartNewTurn 
* The game is over                                        ; loop waiting for next game
                    clr      <$9F                         ; * Clear the Easter Egg semaphore 
                    ldx      #100                         ; 2s #$0C00 ; * How long to wait for restart 
LoopWaitingForRestart: 
                    stx      <$9D                         ; 
                    jsr      playGameSounds               ; 
                    jsr      waitrecal                    ; 
                    lda      #currentStringColor 
                    jsr      intensity_to_a 
                    jsr      do_sound                     ; 
                    tst      $C89F                        ; 
                    beq      DisplayHighScore             ; 
* Display the Easter Egg
                    ldu      #AuthorsInitials             ; 
                    jsr      print_1_string               ; 
*
* Display the current high score, along with the scores
* for players 1 and 2.
DisplayHighScore: 
                    direct   $d0 
                    ldu      #HighScoreString             ; 
                    jsr      print_1_string               ; 
                    ldu      #$CBEB                       ; * height & width 
                    ldd      #$60E0                       ; * rel y & rel x 
                    jsr      print_at_d                   ; 
                    jsr      DisplayBothPlayersScore      ; 
*
*        Display "Got You Humanoid" 2 times.
                    ldy      #HumanoidStringPositions     ; 
                    ldd      ,y++                         ; 
                    std      $C82A                        ; 
                    ldd      ,y++                         ; 
                    ldu      #HumanoidString              ; 
                    pshs     u                            ; 
                    jsr      print_at_d                   ; 
                    puls     u                            ; 
                    ldd      ,y++                         ; 
                    jsr      print_at_d                   ; 
                    pshs     u                            ; 
                    ldd      ,y++                         ; 
                    jsr      print_at_d                   ; 
                    ldd      ,y++                         ; 
                    puls     u                            ; 
                    jsr      print_at_d                   ; 
*
* If only buttons 1, 3 and 4 are pressed, toggle the
* flag (C89F) which will cause the author's initials
* to be displayed (Easter Egg).  Otherwise, if any buttons
* are pressed, then restart the game.  If no buttons are
* pressed, then eventually (after a predetermined
* amount of time), jump back to the start of the
* Exec Rom.  Note that if the Easter Egg gets enabled,
* then pressing buttons will not restart a new game       ;
* you must wait for the timeout to expire.
                    jsr      read_switches2               ; 
                    jsr      dptoC8                       ; 
                    direct   $c8 
                    tst      <$9F                         ; * Bypass if Easter Egg active 
                    bne      P0453                        ; 
                    lda      <$0F                         ; * Button states 
                    anda     #$f 
                    eora     #$0D                         ; * Mask for buttons 1, 3 and 4 
                    bne      P044D                        ; 
                    inc      <$9F                         ; * Turn on the Easter egg 
                    bra      P0453                        ; 

; TODO timing loop
P044D:              lda      <$0F                         ; * Button states 
                    anda     #$f 
                    lbne     ttetet                       ;RestartBerzerk ; 
P0453:              ldx      <$9D                         ; * Decrement timer 
                    leax     -1,x                         ; 
                    bne      LoopWaitingForRestart        ; 
ttetet 
                    jmp      start_of_OS_ROM              ; 

HumanoidString: 
                    DB       "GOT "
                    DB       "YOU",$FF
                    DB       "HUMANOID",$FF               ;
HighScoreString: 
                    DB       $F8                          ; * height 
                    DB       $38                          ; * width 
                    DB       $70                          ; * rel y 
                    DB       $D8                          ; * rel x 
                    DB       "HIGH SCORE "                ;
HumanoidStringPositions: 
                    DB       $F2                          ; * rel y 
                    DB       $4A                          ; * rel x 
                    DB       $22                          ; * rel y 
                    DB       $D8                          ; * rel x 
                    DB       $20                          ; * rel y 
                    DB       $D8                          ; * rel x 
                    DB       $00                          ; * rel y 
                    DB       $CF                          ; * rel x 
                    DB       $FE                          ; * rel y 
                    DB       $CF                          ; * rel x 
*
* Draw the outer and inner maze walls, and if necessary,
* block off one of the maze doors.
                    direct   $d0 
DrawMaze: 
                    lda      isScrolling 
                    lbne     doOldWay 
                    lda      currentMazeColor 
                    jsr      intensity_to_a 
                    lda      #$7F                         ; 
                    sta      VIA_t1_cnt_lo                ; * Drawing intensity 
; outer walls
; data as in OutsideMazeWallData
                    DOUBLE_DRAW_OUTER_ZERO  $60ec, $b0, $c0 
                    tfr      a,a 
                    tfr      a,a                          ; cranky! 
                    tfr      a,a 
                    DOUBLE_DRAW_OUTER_ZERO  $a0ec, $b0, $40 
                    tfr      a,a 
                    tfr      a,a                          ; cranky! 
                    tfr      a,a 
                    DOUBLE_DRAW_OUTER_ZERO  $a014, $50, $40 
                    tfr      a,a 
                    tfr      a,a                          ; cranky! 
                    tfr      a,a 
                    DOUBLE_DRAW_OUTER_ZERO  $6014, $50, $c0 
                    tfr      a,a 
                    if       DALEK = 1 
                    else     
; inner walls
* Draw the 8 inner walls
                    lda      #$08                         ; 
                    sta      -15,s                        ; * Loop counter 
                    ldy      #$C8AB                       ; * Array of wall indicies 
                    ldu      #InnerWallPositionTable      ; 
P04BC_1: 
                    ldd      ,u++ 
                    ldx      #InnerWallVectorTable        ; out of the move, because exact zeroing 
                    nop      2                            ; 100% correct placement! - zeroing 
                    MY_MOVE_TO_D_START  
                    ldb      ,y+                          ; 
                    abx                                   ; 
                    lda      ,x+ 
                    MY_MOVE_TO_B_END  
                    clrb     
                    STA      <VIA_port_a                  ;Send Y to A/D 
                    STb      <VIA_port_b                  ;Enable mux 
                    lda      ,x 
                    INC      <VIA_port_b                  ;Disable mux 
                    STa      <VIA_port_a                  ;Send X to A/D 
                    lda      #$ff 
                    STb      <VIA_t1_cnt_hi               ;Set T1H (scale factor?) 
                    STa      <VIA_shift_reg               ;Put pattern in shift register 
                    LDa      #$40                         ;B-reg = T1 interrupt bit 
LF3F4_1_r2 
                    BITa     <VIA_int_flags               ;Wait for T1 to time out 
                    BEQ      LF3F4_1_r2 
                    LDa      #$CC 
                    nop      2                            ; 100% correct placement! - wait for movement to finish before swicthing light off 
                    STb      <VIA_shift_reg               ;Put pattern in shift register 
                    STa      VIA_cntl                     ;/BLANK low and /ZERO low 
                    dec      -15,s                        ; 
                    bne      P04BC_1                      ; 
                    endif    
; closed door
* Block off one door, if signaled to do so
                    ldu      #BlockedDoorVectorTable      ; 
                    lda      $C8C6                        ; * Door to block, if any 
                    lbeq     DisplayBonusString           ; 
                    ldu      a,u                          ; 
                    lda      currentDoorColor 
                    jsr      intensity_to_a 
                    ldd      ,u 
                    MY_MOVE_TO_D_START  
                    lda      2,u 
                    leau     3,u 
                    MY_MOVE_TO_B_END  
                    clrb     
                    STA      <VIA_port_a                  ;Send Y to A/D 
                    STb      <VIA_port_b                  ;Enable mux 
                    lda      ,u 
                    INC      <VIA_port_b                  ;Disable mux 
                    STa      <VIA_port_a                  ;Send X to A/D 
                    lda      #$ff 
                    STa      <VIA_shift_reg               ;Put pattern in shift register 
                    STb      <VIA_t1_cnt_hi               ;Set T1H (scale factor?) 
                    LDa      #$40                         ;B-reg = T1 interrupt bit 
LF3F4_1_r 
                    BITa     <VIA_int_flags               ;Wait for T1 to time out 
                    BEQ      LF3F4_1_r 
                    LDa      #$CC 
                    nop      2 
                    STb      <VIA_shift_reg               ;Put pattern in shift register 
                    STa      VIA_cntl                     ;/BLANK low and /ZERO low 
                    jmp      DisplayBonusString 

doOldWay 
                    lda      currentMazeColor 
                    jsr      intensity_to_a 
                    lda      #$7F                         ; 
                    sta      drawScale                    ; * Drawing intensity 
                    ldu      #OutsideMazeWallData         ; 
                    inc      $C824                        ; 
                    ldx      #draw_VL_with_count1         ; 
                    stx      drawFunction                 ; * Indirect jump pointer 
* Draw 4 sets of vectors making up outer maze walls
                    ldb      #$04                         ; 
                    stb      -15,s                        ; * Loop counter 
P049E:              lda      #$01                         ; * Vector count - 1 
                    sta      $C823                        ; 
                    ldx      2,u                          ; * Pointer to vector table 
                    jsr      DrawWithPossibleScrolling    ; 
                    leau     4,u                          ; * Process to next wall 
                    dec      -15,s                        ; 
                    bne      P049E                        ; 
* Draw the 8 inner walls
                    if       DALEK = 1 
                    else     
                    clr      $C823                        ; 
                    lda      #$08                         ; 
                    sta      -15,s                        ; * Loop counter 
                    ldy      #$C8AB                       ; * Array of wall indicies 
                    ldu      #InnerWallPositionTable      ; 
P04BC:              ldb      ,y+                          ; 
; todo insert no draw for negatives
; bm i
                    ldx      #InnerWallVectorTable        ; 
                    abx                                   ; 
                    jsr      DrawWithPossibleScrolling    ; 
                    leau     2,u                          ; * Process next segment 
                    dec      -15,s                        ; 
                    bne      P04BC 
                    endif                                 ; 
* Block off one door, if signaled to do so
                    ldu      #BlockedDoorVectorTable      ; 
                    lda      $C8C6                        ; * Door to block, if any 
                    beq      DisplayBonusString           ; 
                    ldu      a,u                          ; 
                    leax     2,u                          ; 
                    clra                                  ; 
                    jsr      DrawWithPossibleScrolling    ; 
*
* If the player killed all of the robots, then display
* a message telling them they got a bonus.  We can tell
* if we need to display the bonus string, by checking to
*        to see if the second digit in the bonus string is not '0'
*        (" x0" is the default, where 'x' is initially set to
* a space, but will be a digit when the bonus string has
* been assigned a value).
*
*        BUG ALERT: when the above check occurs, the 'ble' command
*        really should be a 'blt'!!  This explains why
* when a player clears a maze containing 10
* robots, the bonus string was not displayed.
*        In this case the bonus string is "   100",
* and this fails the above check!
DisplayBonusString: 
                    lda      currentStringColor 
                    jsr      intensity_to_a 
                    ldd      #$F838                       ; 
                    std      $C82A                        ; * Set height & width 
                    lda      $C8D5                        ; 
                    cmpa     #$30                         ; 
                    bgt      $07 
                    lda      >$c8d4 
                    cmpa     #$30                         ; 
                    ble      $0f 
                    ldu      #BonusString                 ; 
                    jsr      print_with_dft_hw            ; 
                    ldd      #$70F0                       ; * rel y & rel x 
                    ldu      #$C8D1                       ; * Bonus score buffer 
                    jsr      print_at_d                   ; 
*
* Display both player's scores, and depending upon the
* active player, display the number of lives for that
* player.
DrawActivePieces: 
                    jsr      DisplayBothPlayersScore      ; 
                    ldd      #$F820                       ; * Set height & width 
                    std      $C82A                        ; 
                    ldu      #$C8D8                       ; * Display # of lives 
                    tst      $C881                        ; 
                    beq      P0510                        ; 
                    ldd      #$9840                       ; * Player 2 rel y & rel x 
                    jsr      print_at_d                   ; 
                    bra      DrawActiveBullets            ; 

P0510:              ldd      #$98A0                       ; * Player 1 rel y & rel x 
                    jsr      print_at_d                   ; 
* Draw all active bullets
DrawActiveBullets: 
                    lda      currentShotColor 
                    _INTENSITY_A  
; upon scrolling all bullets are removed
; so bullets are never "scrolled"
                    ldu      bullet_objects_head 
drawNextBullet 
                    cmpu     #OBJECT_LIST_COMPARE_ADDRESS 
                    lbls     DrawAliveRobots              ;drawBulletsDone 
                    ldd      ,u 
                    MY_MOVE_TO_D_START  
                    LDD      B_HEIGHT,u 
                    ldx      #sizePointer 
                    lda      a,x 
                    ldb      b,x 
                    std      tmpWord 
                    ldb      #3 
                    stb      <VIA_t1_cnt_lo 
                    lda      #$c8 
                    tfr      a,dp 
                    direct   $c8 
                    UPDATE_ONE_BULLET  
                    ldu      B_NEXT_OBJECT,u 
skipUpdate 
                    direct   $d0 
                    ldb      #$d0 
                    tfr      b,dp 
                    lda      tmpWord 
                    MY_MOVE_TO_B_END  
                    STA      <VIA_port_a                  ;Send Y to A/D 
                    CLR      <VIA_port_b                  ;Enable mux 
                    ldb      tmpWord+1 
                    INC      <VIA_port_b                  ;Disable mux 
                    STB      <VIA_port_a                  ;Send X to A/D 
                    LDD      #$FF00                       ;Shift reg=$FF (solid line), T1H=0 
                    STB      <VIA_t1_cnt_hi               ;Set T1H (scale factor?) 
                    STA      <VIA_shift_reg               ;Put pattern in shift register 
                    lda      #$7f 
                    sta      <VIA_t1_cnt_lo 
                    LDA      #$CC 
                    STb      <VIA_shift_reg               ;Clear shift register (blank output) 
                    STA      VIA_cntl                     ;/BLANK low and /ZERO low 
                    jmp      drawNextBullet 

; -2
                    db       -90 
                    db       0 
; 0
sizePointer 
                    db       0 
                    db       0 
; 2
                    db       90 
*
* Draw All robots.  When drawing the robot, it is drawn
* in two halves                                           ; first the right half, and then the left
* half.  When each half is drawn, that half of the body
* outline is drawn, and then the extremities are drawn
* (legs and feet).  The extremities are dependent upon
* whether the robot is moving or standing still (searching
* for the player).  The last thing drawn is the robot's
* eye box                                                 ; this, too, is dependent upon whether the robot
* moving or standing still.  When moving, the eye box is
* aimed in the direction of movement (exception is down,
* in which case no eye box is displayed).  When the robot
* is searching for the player, the eye box will shuffle
* through all of the available positions.
* C91C-C973     Array of robot structures (11 entries)
*
*               byte    meaning
*               ----    -------
*               0       robot's y position.
*               1       robot's x position.
*               2       flag: $80 = alive, 0 = gone, $0F = just killed
*               3       direction of travel(0-10)         ; indexes into extremities tbl
*               4       Eye box to draw (0, 2, 4, 6)
*               5       Delay before robot starts shooting (0-7)
*               6       # of bullets robot can fire in a burst
*               7       Delay between processing a robot (moving, firing, etc)
*
offsetDrawingDrawAliveRobots 
                    lda      currentMazeColor 
                    _INTENSITY_A  
P053E_o 
                    lda      2,y                          ; 
                    lbeq     processNextRobot_o           ; 
                    lbpl     processNextRobot_o           ; 
;;;;;
                    direct   $c8 
                    jsr      dptoC8                       ; 
                    ldb      ,y                           ; 
                    sex                                   ; 
                    addd     <$E9                         ; 
                    stb      <$9D                         ; * starting y pos + scrolling offset 
                    bpl      P08E6_o1                     ; 
                    coma                                  ; 
                    lbne     DoNoDrawing_o1               ; 
                    tst      <$Ed                         ; 
                    bmi      P08F7_o1                     ; 
                    tst      ,u                           ; 
                    lbpl     DoNoDrawing_o1               ; 
                    lda      #$3F                         ; 
                    cmpa     <$E7                         ; 
                    lblt     DoNoDrawing_o1               ; 
                    bra      P08F7_o1                     ; 

P08E6_o1:           tsta                                  ; 
                    lbne     DoNoDrawing_o1               ; 
                    tst      <$Ed                         ; 
                    bpl      P08F7_o1                     ; 
                    tst      ,y                           ; 
                    lbmi     DoNoDrawing_o1               ; 
                    lda      #$C1                         ; 
                    cmpa     <$E7                         ; 
                    lbge     DoNoDrawing_o1               ; 
P08F7_o1:           ldb      1,y                          ; 
                    sex                                   ; 
                    addd     <$Eb                         ; 
                    stb      <$9E                         ; * starting x pos + scrolling offset 
                    bpl      P0913_o1                     ; 
                    coma                                  ; 
                    bne      DoNoDrawing_o1               ; 
                    tst      <$Ed                         ; 
                    bmi      P0924_o1                     ; 
                    tst      1,y                          ; 
                    bpl      DoNoDrawing_o1               ; 
                    lda      #$3F                         ; 
                    cmpa     <$E8                         ; 
                    blt      DoNoDrawing_o1               ; 
                    bra      P0924_o1                     ; 

P0913_o1:           tsta                                  ; 
                    bne      DoNoDrawing_o1               ; 
                    tst      <$Ed                         ; 
                    bpl      P0924_o1                     ; 
                    tst      1,u                          ; 
                    bmi      DoNoDrawing_o1               ; 
                    lda      #$C1                         ; 
                    cmpa     <$E8                         ; 
                    bge      DoNoDrawing_o1               ; 
P0924_o1:           jsr      dptoD0                       ; 
                    direct   $d0 
                    ldd      $C89D                        ; * Goto modified drawing origin 
;;;;
                    MY_MOVE_TO_D_START  
; figure out correct list - and display robot
                    ldu      #newRobotExtremitiesVectorTable ; 
                    lda      3,y                          ; 
                    asla                                  ; 
                    ldu      a,u                          ; * Get correct robot extremities 
; in y now a pointer to 8 vectorlists for each direction a leg1/leg2
                    lda      4,y                          ; 
                    anda     #$02                         ; 
; 0 or two, leg 1 or leg 2
                    sta      -1,s 
                    lda      4,y                          ; * Get correct eye box to draw 
                    anda     #$06                         ; 
                    asla     
                    adda     -1,s 
; in a 0,4,8,12 eyes
                    ldu      a,u 
                    lda      #11 
                    sta      <VIA_t1_cnt_lo 
                    MY_MOVE_TO_B_END  
                    jsr      drawSmart 
P058F_o: 
                    leay     8,y                          ; 
                    dec      -15,s                        ; 
                    lbne     P053E_o                      ; 
                    jmp      P05C6                        ; 

DoNoDrawing_o1: 
                    jsr      dptoD0                       ; 
                    leay     8,y                          ; 
                    dec      -15,s                        ; 
                    lbne     P053E_o                      ; 
                    jmp      P05C6                        ; 

* Process next robot structure
processNextRobot_o 
                    leay     8,y                          ; 
                    dec      -15,s                        ; 
                    lbne     P053E_o                      ; 
                    jmp      P05C6 

processNextRobot_short 
                    leay     8,y                          ; 
                    dec      -15,s                        ; 
                    bne      P053E                        ; 
                    jmp      P05C6 

DrawAliveRobots: 
                    lda      $C8C7                        ; * Number of robots 
                    lbeq     DrawThePlayer                ; 
                    sta      -15,s                        ; 
                    ldy      #$C91C                       ; * Robot structure 
                    lda      isScrolling 
                    lbne     offsetDrawingDrawAliveRobots 
                    lda      currentRobotColor 
                    _INTENSITY_A  
P053E: 
                    lda      2,y                          ; 
                    beq      processNextRobot_short 
                    bpl      DrawRobotDisappearing        ; 
                    ldd      ,y 
                    MY_MOVE_TO_D_START  
;.......
                    tst      7,y                          ; 
                    lbne     noProcessing 
noProcessing 
;.......
                    tst      7,y                          ; 
                    beq      P0544                        ; 
                    dec      7,y                          ; 
P0544: 
; figure out correct list - and display robot
                    ldu      #newRobotExtremitiesVectorTable ; 
                    lda      3,y                          ; 
                    asla                                  ; 
                    ldu      a,u                          ; * Get correct robot extremities 
; in y now a pointer to 8 vectorlists for each direction a leg1/leg2
                    lda      4,y                          ; 
                    anda     #$02                         ; 
; 0 or two, leg 1 or leg 2
                    sta      -1,s 
                    lda      4,y                          ; * Get correct eye box to draw 
                    anda     #$06                         ; 
                    asla     
                    adda     -1,s 
; in a 0,4,8,12 eyes
                    ldu      a,u 
                    lda      #11 
                    sta      <VIA_t1_cnt_lo 
                    MY_MOVE_TO_B_END  
                    jsr      drawSmart 
P058F:              leay     8,y                          ; 
                    dec      -15,s                        ; 
                    lbne     P053E                        ; 
                    jmp      P05C6                        ; 

*
* When a robot is killed, it is replaced by a cloud of
* random dots.
DrawRobotDisappearing: 
                    ldd      ,y 
                    MY_MOVE_TO_D_START  
                    jsr      get_random_a                 ; 
                    tfr      a,b                          ; 
                    clra                                  ; 
                    tfr      d,x                          ; 
                    lda      #$06                         ; 
                    sta      -7,s                         ; 
                    lda      #15 
                    sta      <VIA_t1_cnt_lo 
                    MY_MOVE_TO_B_END  
P05A5: 
                    ldd      ,x++                         ; 
                    andb     #$3f                         ; 
                    anda     #$3f                         ; 
                    std      tmpWord 
                    MY_MOVE_TO_D_START  
                    ldd      tmpWord 
                    nega     
                    negb     
                    std      tmpWord 
                    MY_MOVE_TO_B_END  
                    LDA      #$FF                         ;Set pattern to all 1's 
                    STA      <VIA_shift_reg               ;Store in VIA shift register 
                    LDB      $C828                        ;Get dot dwell (brightness) 
LF2CC_o:            DECB                                  ;Delay leaving beam in place 
                    BNE      LF2CC_o 
                    CLR      <VIA_shift_reg               ;Blank beam in VIA shift register 
                    ldd      tmpWord 
                    MY_MOVE_TO_D_START  
                    MY_MOVE_TO_B_END  
                    dec      -7,s                         ; 
                    bne      P05A5                        ; 
                    _ZERO_VECTOR_BEAM  
                    lda      #$7f 
                    sta      <VIA_t1_cnt_lo 
                    dec      2,y                          ; * Prepare for next pass 
* Process next robot structure
processNextRobot 
                    leay     8,y                          ; 
                    dec      -15,s                        ; 
                    lbne     P053E                        ; 
*
* If the player has been in the maze for a certain amount
                    *        of time, then it 's time to bring out 'Otto'!
P05C6:              tst      $C82E                        ; 
                    bne      DrawThePlayer                ; 
                    tst      $C82F                        ; 
                    bne      DrawThePlayer                ; 
                    if       NO_OTTO = 1 
                    bra      DrawThePlayer                ; 

                    endif    
                    lda      #$14                         ; 
                    sta      drawScale                    ; * Drawing intensity 
                    ldx      #next_pt                     ; 
                    stx      drawFunction                 ; * Drawing function 
                    ldx      #OttoVectors                 ; 
                    ldu      #$C893                       ; * Otto's position 
                    jsr      DrawWithPossibleScrolling    ; 
                    _ZERO_VECTOR_BEAM  
                    lda      #$7f 
                    sta      VIA_t1_cnt_lo 
*
* Unless C886 < 0, draw the player.  If the player is in
* the middle of getting fried, then redraw the player again,
* using a higher intensity.
DrawThePlayer: 
                    tst      $C886                        ; * Should player be drawn? 
                    lbmi     exitHere                     ; 
                    lda      isScrolling 
                    lbne     DrawThePlayerWithScrolling 
                    lda      currentPlayerColor 
                    _INTENSITY_A  
                    ldd      player_y 
                    MY_MOVE_TO_D_START  
                    lda      #$09                         ; 
                    sta      VIA_t1_cnt_lo                ; * Drawin intensity 
                    ldb      PlayerLegState                        ; * Running or standing still? 

                    beq      drawManStand                 ; 
drawManRunning 
                    ldu      #manRunningList              ; 
                    ldu      b,u                          ; 
                    bra      manDisplayDone 

drawManStand 

                    ldb      PlayerArmState                        ; 
                    ldu      #manStandingList 
                    ldu      b,u                          ; * Player is shooting 
manDisplayDone 
                    MY_MOVE_TO_B_END  
                    jsr      drawSmart 
                    lda      $C8C5                        ; * See if player is being fried 
                    cmpa     #$04                         ; 
                    bne      exitHere                     ; 
                    lda      #$7f                         ; 
                    sta      VIA_t1_cnt_lo                ; * Drawin intensity 
                    lda      isColoredMode 
                    beq      nopeNoColor 
                    lda      #WHITE 
                    _INTENSITY_A  
nopeNoColor 
                    ldd      player_y 
                    MY_MOVE_TO_D_START  
                    lda      $C8D0                        ; * Being fried 
                    sta      VIA_t1_cnt_lo                ; * Drawin intensity 
                    ldb      PlayerLegState                        ; * Running or standing still? 

                    beq      drawManStand1                ; 
drawManRunning1 
                    ldu      #manRunningList              ; 
                    ldu      b,u                          ; 
                    bra      manDisplayDone1 

drawManStand1 
                    ldb      PlayerArmState                        ; 
                    ldu      #manStandingList 
                    ldu      b,u                          ; * Player is shooting 
manDisplayDone1 
                    MY_MOVE_TO_B_END  
                    jmp      drawSmart 

exitHere            rts      

DrawPlayerFunctionSmart 

                    ldb      PlayerLegState                        ; * Running or standing still? 
                    beq      drawManStand2                ; 
drawManRunning2 
                    ldu      #manRunningList              ; 
                    ldu      b,u                          ; 
                    bra      manDisplayDone2 

drawManStand2 
                    ldb      PlayerArmState
                    ldu      #manStandingList 
                    ldu      b,u                          ; * Player is shooting 
manDisplayDone2 
                    jmp      drawSmart 

DrawThePlayerWithScrolling 
                    ldx      #DrawPlayerFunctionSmart     ; 
                    stx      drawFunction                 ; * Drawing function 
                    lda      #$09                         ; 
                    sta      drawScale                    ; * Drawin intensity 
                    ldu      #player_y                    ; * Player's position 
                    jmp      DrawWithPossibleScrolling    ; * Draw the player 

*
* DisplayBothPlayersScore()
*
* This displays player 1's score, and if 2 players are
* playing, then player 2's score also.
;DisplayBothPlayersScore: 
;                    lda      currentStringColor 
;                    _INTENSITY_A  
;                    ldd      #$7086                       ; * rel y & rel x 
;                    ldu      #$C900  +1                     ; * player 1 structure - never a score of over 99999
;                    jsr      print_at_d                   ; 
;                    tst      $C882                        ; * 2 players? 
;                    beq      P0649                        ; 
;                    ldd      #$7024                       ; * rel y & rel x 
;                    ldu      #$C90E        +1               ; * player 2 structue 
;                    jsr      print_at_d                   ; 
;P0649:              rts                                   ;     
; display only one score
DisplayBothPlayersScore: 
                    lda      currentStringColor 
                    _INTENSITY_A  
                    lda      $C882 
                    bne      possiblyTwoPlayers 
firstPlayer 
                    ldd      #$7086                       ; * rel y & rel x 
                    ldu      #$C900 +1                    ; * player 1 structure - never a score of over 99999 
                    jsr      print_at_d                   ; 
                    rts      

possiblyTwoPlayers 
                    lda      $c881 
                    beq      firstPlayer 
                    ldd      #$7024                       ; * rel y & rel x 
                    ldu      #$C90E +1                    ; * player 2 structue 
                    jsr      print_at_d                   ; 
                    rts                                   ; 

                    direct   $c8 
*
* DetermineDirectionToMove()
*
* This function takes a point, and returns a mask
* indicating the direction the point must travel, in
* in order to reach the player.  It also returns the
* distance (rise and run) describing how far away the
* point is from the player.
*
* Entry:
*   a = y position
*   b = x position
*
* Exit:
*   C8e5: y distance between point and player.
*   C8e6: x distance between point and player.
*   b = mask, indicating desired direction of travel
*
*              ---------------------------------
*              | 0 | 0 | x | 0 | x | x | x | x |
*              ---------------------------------
*   x and/or y is same --^
*   Down ------------------------^
*   Up ------------------------------^
*   Right -------------------------------^
*   Left ------------------------------------^
                    direct   $c8 
; appears equal for 
BULLET_SHOT_THRESHOLD  =     2 
DetermineDirectionToMove3: 
; position from 0 - 255 instead of -127 - + 127
                    adda     #$80                         ; 
                    addb     #$80                         ; 
                    std      <$Ee                         ; 
                    lda      <player_y                    ; 
                    adda     #$80                         ; 
                    clrb                                  ; 
                    suba     <$Ee                         ; 
                    beq      P0F7A_v                      ; 
                    blo      P0F7E_v                      ; 
                    bpl      noNeg_d2 
                    nega     
                    cmpa     #BULLET_SHOT_THRESHOLD 
                    bls      yPosNearlySame 
                    nega     
                    orb      #$04                         ; 
                    bra      P0F80_v                      ; 

noNeg_d2 
                    cmpa     #BULLET_SHOT_THRESHOLD 
                    bls      yPosNearlySame 
                    orb      #$04                         ; 
                    bra      P0F80_v                      ; 

yPosNearlySame 
P0F7A_v: 
                    orb      #$20                         ; 
                    bra      P0F80_v                      ; 

P0F7E_v: 
                    bpl      noNeg_d1 
                    nega     
                    cmpa     #BULLET_SHOT_THRESHOLD 
                    bls      yPosNearlySame 
                    orb      #$08                         ; 
                    nega     
                    bra      P0F80_v 

noNeg_d1 
                    cmpa     #BULLET_SHOT_THRESHOLD 
                    bls      yPosNearlySame 
                    orb      #$08                         ; 
P0F80_v: 
                    nega                                  ; 
                    sta      <$e5                         ; 
                    lda      <player_x                    ; 
                    adda     #$80                         ; 
                    suba     <$Ef                         ; 
                    sta      <$e6                         ; 
                    beq      P0F92_v                      ; 
                    blo      P0F95_v                      ; 
                    bpl      noNeg_d4 
                    nega     
noNeg_d4 
                    cmpa     #BULLET_SHOT_THRESHOLD 
                    bls      xPosNearlySame 
                    orb      #$02                         ; 
                    rts                                   ; 

P0F92_v: 
xPosNearlySame 
                    orb      #$20                         ; 
                    rts                                   ; 

P0F95_v: 
                    bpl      noNeg_d3 
                    nega     
noNeg_d3 
                    cmpa     #BULLET_SHOT_THRESHOLD 
                    bls      xPosNearlySame 
                    orb      #$01                         ; 
                    rts                                   ; 

*
* ProcessAndUpdateRobots()
*
* This function loops through each of the robots, and
* determines where the robot is relative to the player,
* and will alter the robot's position (based on some
* criteria), and will determine if it is safe to fire
* at the player.  It also checks to see if the robot has
* hit a wall, collided with another robot or collided with
* the player.  Lastly, it updates Otto's position, if
* necessary, and checks to see if Otto has collided with
* the player.
*
* The robot will only fire if either it is 90 degrees or
* 45 degrees from the player                              
; 45 degrees occurs when
* |x delta| == |y delta|.
*
* As the robot is moving, if it is in a different quad
* from the player, then it will only update it's position
* (in an attempt to avoid running into walls) when it is
* not in the center of the quadrant.
;
; shoot also, when "nearly" in line!
; this does NOT loop
; only one robot is processed per round!
ProcessAndUpdateRobots: 
                    lda      <$C8                         ; * Check if any robots are left 
                    lbeq     CheckForOttoStart            ; 
                    clr      <$9D                         ; 
                    lda      <$A9                         ; 
P0654:              ldu      #$C91C                       ; * Ptr to robot array 

startProcessingThisRobot 
                    if       DALEK = 1 
                    tst      2,u                          ; * Skip this robot if he is dead or 
                    lbpl     nextRobotProcessing          ; * not displayed. 
                    else     

 if LIMIT_SUPPORT = 1
 tst limitReached
 beq singleRobotProcessing
    tst      2,u                          ; * Skip this robot if he is dead or 
    lbpl     nextRobotProcessing          ; * not displayed. 
 bra oneRobotProcess
singleRobotProcessing
 endif
                    leau     a,u                          ; 
                    adda     #$08                         ; 
                    cmpa     <$AA                         ; 
                    bls      P0669                        ; 
                    tst      <$9D                         ; 
                    beq      P0666                        ; 
                    jmp      CheckForOttoStart            ; 

P0666:              inc      <$9D                         ; 
                    clra                                  ; 
P0669:              sta      <$A9                         ; 
                    tst      2,u                          ; * Skip this robot if he is dead or 
                    bpl      P0654                        ; * not displayed. 
                    endif    



                    tst      7,u                          ; 
                    if       DALEK = 1 
                    lbne     nextRobotProcessing          ; * Check processing delay 
                    else     
                    lbne     CheckForOttoStart            ; * Check processing delay 
                    endif    
 if LIMIT_SUPPORT = 1
oneRobotProcess
 endif
                    lda      <$A7                         ; 
                    sta      7,u                          ; * Reset the processing delay value 
                    lda      5,u                          ; * See if the robot can now start 
                    beq      P067F                        ; * shooting. 
                    dec      5,u                          ; 
P067F:              inc      4,u                          ; * Update to display next eye box 
                    ldd      ,u                           ; * Where is robot relative to player? 
                    jsr      DetermineDirectionToMove3    ; 

                    bitb     #$20                         ; * Do they line up in at least 1 dim? 
                    bne      PrepareToFire                ; * Yes 
                    lda      <$e5                         ; * No, so see if the x and y deltas 
                    cmpa     <$e6                         ; * are the same; i.e. 45 degrees. 
                    beq      PrepareToFire                ; 
                    nega                                  ; 
                    cmpa     <$e6                         ; 


                    bne      RobotMoving                  ; 
*
* Only fire if the robot's delay counter has decremented
* to 0.  Once all bullets have been spent, reset the counter
* and the bullet count.  Robots have a delay, which controls
* how often they can fire, along with a bullet counter, which
* controls how many bullets they can fire in a burst.
 if LIMIT_SUPPORT = 1
RobotMovingBack
 tst limitReached
 beq RobotMoving
 lda #ROBOT_FIRE_DELAY
                    sta      5,u                          ; * Reset shooting delay 
                    bra      RobotMoving                  ; * Nope 
 endif
PrepareToFire: 

 if LIMIT_SUPPORT = 1
                    lda      5,u                          ; * Check if shooting delay expired 
                    bne      RobotMovingBack                  ; * Nope 
; tst limitReached
; beq PrepCont
 lda #ROBOT_FIRE_DELAY
 sta      5,u                          ; * Reset shooting delay 
 bra RobotFiring
PrepCont
 else
                    lda      5,u                          ; * Check if shooting delay expired 
                    bne      RobotMoving                  ; * Nope 
 endif
                    dec      6,u                          ; * Dec robot's bullet burst counter 
                    bpl      RobotFiring                  ; 
                    lda      <$83                         ; * Fired all we can for now. 
                    sta      6,u                          ; * Reset bullet burst counter 
                    lda      #ROBOT_FIRE_DELAY                         ; 
                    sta      5,u                          ; * Reset shooting delay 
                    bra      RobotMoving                  ; 

*
* The robot is firing.  Determine the starting position for
* the bullet, the direction and the velocity.  Lastly, make
* a sound.
RobotFiring: 
                    orb      #$80                         ; 
                    andb     #$0F                         ; 
                    stb      <$f2                         ; * Save direction to fire bullet 
                    leax     ,u                           ; * Determine starting position for 
                    lda      mainJumpIndex 
                    bne      RobotMoving                  ; robots dont fire when player is dead, otherwise they can kill themselfs! 
                    lda      #$07                         ; * bullet; a little away from robot 
                    jsr      CalculateUpdatedPosition     ; 
                    std      <$9E                         ; 
                    ldx      #$C89E                       ; * Starting position 
                    ldb      <$84                         ; * Velocity factor 
                    if       ROBOT_BULLET_RESTRICTION = 1 
                    lda      robotShotCounter 
                    cmpa     #MAX_ROBOT_BULLETS 
                    bhs      notanotherbulletnow 
                    jsr      FireRobotBullet                   ; 
                    beq      FireError2
                    inc      robotShotCounter 
notanotherbulletnow 
                    else     
                    jsr      FireRobotBullet                   ; 
                    beq      FireError2
                    endif    
                    lda      #SOUND_ROBOT_BULLET_START_BIT ; * Make a sound 
                    ora      <soundTypes                  ; 
                    sta      <soundTypes                  ; 
FireError2
                    bra      CheckForRobotContactWithWall ; 

*
* The robot is moving, and not firing.  Update the robot's
* extremities, based upon the direction of movement.  If
* the robot is in a different quadrant than the player,
* then attempt to avoid walls in the quadrant, only if the
* robot is not in the center of the quadrant.  Usually, the
* robot's direction of travel is determined by trying to
* avoid walls in the quadrant.
RobotMoving: 
                    andb     #$0F                         ; 
                    stb      3,u                          ; * Set robot's extremities 
; moved to move
                    ldd      <player_y                    ; * Get player's quadrant 
                    jsr      MapPointToQnMakeRel          ;MapPointToQuadrantAndMakeRelative; 
                    lda      <quadrantOfPoint             ; 
                    pshs     a                            ; 
                    ldd      ,u                           ; * Get robot's quadrant 
                    jsr      MapPointToQnMakeRel          ;MapPointToQuadrantAndMakeRelative; 
                    puls     a                            ; 
                    cmpa     <quadrantOfPoint             ; * In same quadrant? 
                    beq      MoveInCurrentDirection       ; * yes 
                    lda      <$9B                         ; * In different quadrants 
                    cmpa     #$12                         ; * Avoid walls, if not in quad center 
                    blt      AttemptToAvoidWalls          ; 
                    cmpa     #$2E                         ; 
                    bgt      AttemptToAvoidWalls          ; 
                    lda      <$9C                         ; 
                    cmpa     #$12                         ; 
                    blt      AttemptToAvoidWalls          ; 
                    cmpa     #$16                         ; 
                    bgt      AttemptToAvoidWalls          ; 
                    bra      MoveInCurrentDirection       ; 

*
* Take the mask indicating which walls are present in this
* quadrant, and complement it, to determine the direction
* to move to avoid the walls.  If the resulting direction
* is 0, then the robot is completely enclosed.
AttemptToAvoidWalls: 
 if LIMIT_SUPPORT = 1
                    bra      MoveInCurrentDirection       ; 
 endif
                    lda      <$99                         ; * Wall mask for quadrant 
                    coma                                  ; 
                    anda     #$0F                         ; 
                    anda     3,u                          ; * Update robot's direction of travel 
                    sta      3,u                          ; 
                    beq      CheckForRobotContactWithWall ; * Enclosed room 
*
*        Don 't update the robot's direction of travel, but
* instead, just keep moving it in its current direction.
MoveInCurrentDirection: 
                    leax     ,u                           ; * Get current position 
                    lda      #$01                         ; * Set velocity factor 
                    ldb      3,u                          ; * Get current direction 
                    jsr      CalculateUpdatedPosition     ; 
                    std      ,u                           ; * Update robot's position 
                    inc      4,u                          ; * Update eye box index 
; moved to move EN
* See if the robot has run into a wall
CheckForRobotContactWithWall: 


                    tst      2,u                          ; * Skip if the robot is already dead 
                    bpl      CheckForRobotCollision       ; 
                    ldd      ,u                           ; * Robot's position 
                    jsr      MapPointToQnMakeRel          ;MapPointToQuadrantAndMakeRelative; 
                    CHECK_FOR_QUADRANT_EDGE_CROSSING  RobotThresholds 
                    bitb     <$99                         ; * Contact? 
                    beq      CheckForRobotCollision       ; 
                    tfr      u,x                          ; 
                    jsr      CreditPlayerWithDeadRobot    ; 
* See if the robot has run into another robot.
CheckForRobotCollision: 
                    ldx      #$C91C                       ; 
                    lda      <$C7                         ; 
                    sta      <$9D                         ; 
P072B:              ldd      ,u                           ; 
                    cmpd     ,x                           ; * Don't compare against yourself 
                    beq      GetNextRobotPtr              ; 
                    tst      2,x                          ; * Skip if next robot is dead 
                    bpl      GetNextRobotPtr              ; 
                    tst      2,u                          ; * Skip if current robot is now dead 
                    bpl      GetNextRobotPtr              ; 
                    ldy      #RobotContactBounds          ; 
                    jsr      CheckForContact              ; 
                    bhs      GetNextRobotPtr              ; * Contact? 
                    jsr      CreditPlayerWithDeadRobot    ; 
                    exg      x,u                          ; 
                    jsr      CreditPlayerWithDeadRobot    ; 
                    exg      x,u                          ; 
GetNextRobotPtr: 
                    leax     8,x                          ; * Get pointer to next robot 
                    dec      <$9D                         ; 
                    bne      P072B                        ; 

 if LIMIT_SUPPORT = 1
* See if robot collided with the player
 tst limitReached
 beq normalProcessing
;                    lda      2,u                          ; 
;                    bpl      nextRobotProcessing
 bra      nextRobotProcessing
normalProcessing
 endif

                    lda      2,u                          ; 
                    if       DALEK = 1 
                    bpl      nextRobotProcessing 
                    else     
                    bpl      CheckForOttoStart            ; * Skip if robot is dead 
                    endif    
cont_np
                    ldd      ,u                           ; 
                    ldx      #player_y                    ; 
                    ldy      #PlayerContactBounds         ; 
                    jsr      CheckForContact              ; 

 if INVINCIBLE_ROBOT = 1
                    bra      CheckForOttoStart            ; 
 endif
                    if       DALEK = 1 
                    bhs      nextRobotProcessing 
                    else     
                    bhs      CheckForOttoStart            ; 
                    endif    
                    tfr      u,x                          ; 
                    jsr      CreditPlayerWithDeadRobot    ; 
                    lda      #$04                         ; * Tag player as dead 
                    sta      <mainJumpIndex               ; 
                    lda      #SOUND_PLAYER_DEATH_START_BIT ; 
                    ora      <soundTypes                  ; 
                    sta      <soundTypes                  ; * Make some sound 
 bra CheckForOttoStart

* Wait for counters to expire, before starting Otto
;                    if       DALEK = 1 
nextRobotProcessing 
                    leau     8,u                          ; * Get pointer to next robot 
                    cmpu     #$C91C + 11 *8 
                    lblo     startProcessingThisRobot 
;                    endif    
CheckForOttoStart: 
                    tst      <$2E                         ; 
                    bne      P07ED                        ; 
                    tst      <$2F                         ; 
                    bne      P07ED                        ; 
                    if       NO_OTTO = 1 
                    bra      P07ED 
                    endif    
                    lda      <$26                         ; 
                    bita     #$03                         ; 
                    bne      P079B                        ; 
*
* Determine Otto's position and direction, do some
* occassional sound mucking, and then see if Otto collided
* with the player.
                    ldd      <$91                         ; * Determine Otto's travel direction 
                    jsr      DetermineDirectionToMove     ; 
                    ldx      #$C891                       ; * Otto's position 
                    lda      <$98                         ; * Otto's velocity 
                    jsr      CalculateUpdatedPosition     ; 
                    std      <$91                         ; * Update Otto's position 
                    ldb      <$97                         ; * Since Otto bounces up and down, 
                    subb     <$98                         ; * determine how to offset his 'y' 
                    bpl      P0799                        ; * position. 
                    ldb      #$20                         ; 
P0799:              stb      <$97                         ; 
P079B:              ldb      <$97                         ; 
                    cmpb     #$1A                         ; 
                    bne      P07A7                        ; 
                    lda      #SOUND_OTTO_START_BIT        ; 
                    ora      <soundType2                  ; * Make some sound 
                    sta      <soundType2                  ; 
P07A7:              lda      <$91                         ; * Make Otto bounce up and down 
                    ldu      #OttoBounceOffsets           ; 
                    adda     b,u                          ; 
                    ldb      <$92                         ; 
                    std      <$93                         ; * Update Otto's position 
                    tst      <mainJumpIndex               ; * Is player still alive? 
                    bne      CheckForRobotHitByOtto       ; 
                    ldx      #player_y                    ; * Otto and player collide? 
                    ldy      #PlayerContactBounds         ; 
                    jsr      CheckForContact              ; 
                    bhs      CheckForRobotHitByOtto       ; 
                    inc      <$98                         ; * Increment Otto's velocity 
                    lda      #SOUND_PLAYER_DEATH_START_BIT ; 
                    ora      <soundTypes                  ; 
                    sta      <soundTypes                  ; * Make a sound 
                    lda      #$04                         ; 
                    sta      <mainJumpIndex               ; * Tag player as dead 
* Kill off any robots which collide with Otto
CheckForRobotHitByOtto: 
                    ldx      #$C91C                       ; * Addr of robot array 
                    lda      <$C7                         ; * Number of array entries 
                    sta      <$9D                         ; * Loop counter 
                    ldy      #RobotContactBounds          ; * 'Accuracy' table 
P07D9:              tst      2,x                          ; * Make sure robot is alive 
                    bpl      P07E7                        ; 
                    ldd      <$93                         ; * Get Otto's position 
                    jsr      CheckForContact              ; 
                    bhs      P07E7                        ; 
                    jsr      CreditPlayerWithDeadRobot    ; 
P07E7:              leax     8,x                          ; 
                    dec      <$9D                         ; 
                    bne      P07D9                        ; 
P07ED:              rts                                   ; 

OttoBounceOffsets: 
                    DB       $00                          ; 
                    DB       $08                          ; 
                    DB       $0C                          ; 
                    DB       $0E                          ; 
                    DB       $10                          ; 
                    DB       $12                          ; 
                    DB       $12                          ; 
                    DB       $12                          ; 
                    DB       $14                          ; 
                    DB       $16                          ; 
                    DB       $16                          ; 
                    DB       $16                          ; 
                    DB       $16                          ; 
                    DB       $16                          ; 
                    DB       $14                          ; 
                    DB       $12                          ; 
                    DB       $12                          ; 
                    DB       $12                          ; 
                    DB       $10                          ; 
                    DB       $0E                          ; 
                    DB       $0C                          ; 
                    DB       $08                          ; 
                    DB       $00                          ; 
                    DB       $FC                          ; 
                    DB       $F8                          ; 
                    DB       $F4                          ; 
                    DB       $EE                          ; 
                    DB       $EA                          ; 
                    DB       $EE                          ; 
                    DB       $F4                          ; 
                    DB       $F8                          ; 
                    DB       $FC                          ; 
                    DB       $00                          ; 
*
* CreditPlayerWithDeadRobot()
*
* This function decrements the number of active robots
* (C8C8), marks the robot as dead, and credits the player
* with 50 points.
*
* Entry:
*    x = pointer to dead robot structure.
CreditPlayerWithDeadRobot: 
                    ldd      <$2E 
                    beq      noOttoAdd 
                    bmi      noOttoAdd 
                    addd     #50 
                    std      <$2E                         ; add OTTO time out for each killed robot 
noOttoAdd 
                    dec      <$C8                         ; 
                    lda      #$0F                         ; 
                    sta      2,x                          ; 
                    lda      #SOUND_ROBOT_DEATH_START_BIT 
                    sta      <soundTypes                  ; 
                    pshs     x                            ; 
                    ldx      <$80                         ; 
                    ldd      #$0050                       ; 
                    jsr      add_d_to_x_in_bcd            ; 
                    puls     x,pc                         ; 
*
* ProcessAndUpdateBullets()
*
* This function updates the head and tail of each bullet,
* and checks to see if a bullet has run into a wall,
* hit a robot, or hit the player.  A bullet's position
* is updated multiple times, based on its velocity setting ;
* a higher velocity setting causes a bullet to be updated
* more often.  A bullet is a short vector made up of a
* head and a tail.
exitBulletProcessing2 
                    rts      

ProcessAndUpdateBullets: 
                    ldu      bullet_objects_head 
processNextBullet 
                    cmpu     #OBJECT_LIST_COMPARE_ADDRESS 
                    bls      exitBulletProcessing2        ;drawBulletsDone 
                    lda      B_VELOCITY,u                 ; 
                    deca                                  ; original map is for 1 
                    sta      <$f2                         ; tmp 
;
; idea:
; seperate player/robot bullets in the processing
; than we don't have tobuild that everytime when bullets "alternate"
; create a BulletToWallThresholds in relation to speed
;
* See if bullet hit any of the robots
; build robot bounding box in relation to speed
; the bounding box is changed in relation to velocity and direction
; so that only one test must be done, not 
; one test for each 1 velocity step!
;
* The following 3 tables define the bounding box around
* some figure (robot, player).  It is used to determine
* when something has hit the figure.  Each table contains
* the following values:
*
*     + y delta
*     - y delta
*     + x delta
*     - x delta
;RobotContactBounds:
;       DB    $06;
;       DB    $FA;
;       DB    $06;
;       DB    $FA;
                    lda      B_DIRECTION,u                ; 
                    adda     B_VELOCITY,u                 ; 
                    cmpa     currentCombinedVelocityDirection 
                    beq      alreadyDoneMapping 
                    sta      currentCombinedVelocityDirection 
                                                          ; todo check for dir + velo = already done tables since shots come in "bundels" 
                    ldb      B_DIRECTION,u                ; 
                    bitb     #1                           ; LEFT 
                    beq      noLeft_r 
                    ldd      RobotContactBounds+2 
                    subb     <$f2 
                    std      currentRobotMapVelocity+2 
                    ldd      PlayerContactBounds2+2 
                    subb     <$f2 
                    std      currentPlayerMapVelocity+2 
                    bra      horizontalDone_r 

horizontalNotDone_r 
                    ldd      RobotContactBounds+2 
                    std      currentRobotMapVelocity+2 
                    ldd      PlayerContactBounds2+2 
                    std      currentPlayerMapVelocity+2 
                    bra      horizontalDone_r 

noLeft_r 
                    bitb     #2                           ; RIGHT 
                    beq      horizontalNotDone_r 
                    ldd      RobotContactBounds+2 
                    adda     <$f2 
                    std      currentRobotMapVelocity+2 
                    ldd      currentPlayerMapVelocity+2 
                    adda     <$f2 
                    std      currentPlayerMapVelocity+2 
horizontalDone_r 
                    bitb     #4                           ; UP 
                    beq      noUP_r 
                    ldd      RobotContactBounds 
                    adda     <$f2 
                    std      currentRobotMapVelocity 
                    ldd      PlayerContactBounds2 
                    adda     <$f2 
                    std      currentPlayerMapVelocity 
                    bra      verticalDone_r 

noDOWN_verticalNotDone_r 
                    ldd      RobotContactBounds 
                    std      currentRobotMapVelocity 
                    ldd      PlayerContactBounds2 
                    std      currentPlayerMapVelocity 
                    bra      verticalDone_r 

noUP_r 
                    bitb     #8                           ; DOWN 
                    beq      noDOWN_verticalNotDone_r 
                    ldd      RobotContactBounds 
                    subb     <$f2 
                    std      currentRobotMapVelocity 
                    ldd      PlayerContactBounds2 
                    subb     <$f2 
                    std      currentPlayerMapVelocity 
noDOWN_r 
verticalDone_r 
alreadyDoneMapping 

 if NO_OTTO = 1
                    bra      noOttoCheck 
 endif

; check bullet otto, for vtk!
                    ldd      <$2E 
                    bgt      noOttoCheck 
                    ldx      #$c893                       ; position of otto REAL 
                    ldd      B_YPOS_TAIL,u                ; * Position of bullet's head 
                    ldy      #OttoContactBounds 
                    jsr      CheckForContact 
                    bhs      noOttoCheck                  ; 
                    jsr      removeBullet 
                    jmp      processNextBullet 

OttoContactBounds: 
                    DB       $05                          ; 
                    DB       $FA                          ; 
                    DB       $05                          ; 
                    DB       $FA                          ; 
noOttoCheck 
; check bullet otto - end
                    ldx      #$C91C                       ; 
                    lda      <$C7                         ; 
                    sta      <$9D                         ; 
P0874:              lda      2,x                          ; * Skip robot if already dead 
                    bpl      checkNextRobot               ; 
                    ldd      B_YPOS_TAIL,u                ; * Position of bullet's head 
                    CHECK_FOR_CONTACT  currentRobotMapVelocity 
                    bhs      checkNextRobot               ; 
                    jsr      CreditPlayerWithDeadRobot    ; 
                    jsr      removeBullet 
                    jmp      processNextBullet 

checkNextRobot: 
                    leax     8,x                          ; 
                    dec      <$9D                         ; 
                    bne      P0874                        ; 
                    lda      B_ORIGIN,u                   ; is a player bullet? 
                    lbeq     checkPlayerHit               ; nope 
; check if player bullet hit a robot bullet
                    ldx      bullet_objects_head 
processnextbulletbullettest 
                    lda      B_ORIGIN,x 
                    bne      nextBulletCheck 
                    lda      B_DIRECTION,u 
                    adda     B_DIRECTION,x                ; 
                    cmpa     #3 
                    beq      horizontalDetection 
                    cmpa     #12 
                    beq      verticalDetection 
                    cmpa     #1+2+4+8 
                    bne      nextBulletCheck 
diagonalDetection 
                    lda      B_XPOS, x 
                    suba     B_XPOS, u 
                    bpl      noNegx 
                    nega     
noNegx 
                    ldb      B_VELOCITY,u 
                    addb     B_VELOCITY,x 
                    stb      -1,s 
                    cmpa     -1,s 
                    bhi      nextBulletCheck 
                    lda      B_YPOS, x 
                    suba     B_YPOS, u 
                    bpl      noNegy 
                    nega     
noNegy 
                    ldb      B_VELOCITY,u 
                    addb     B_VELOCITY,x 
                    stb      -1,s 
                    cmpa     -1,s 
                    bhi      nextBulletCheck 
                    bra      removeDoubleBullets 

verticalDetection 
                                                          ; x pos must be nearly the same 
                    lda      B_XPOS, x 
                    suba     B_XPOS, u 
                    bpl      noNeg1 
                    nega     
noNeg1 
                    cmpa     #BULLET_BULLET_THRESHOLD 
                    bhi      nextBulletCheck 
                    lda      B_YPOS, x 
                    suba     B_YPOS, u 
                    bpl      noNeg2 
                    nega     
noNeg2 
                    ldb      B_VELOCITY,u 
                    addb     B_VELOCITY,x 
                    stb      -1,s 
                    cmpa     -1,s 
                    bhi      nextBulletCheck 
; bullets hit!
                    bra      removeDoubleBullets 

horizontalDetection 
                                                          ; y pos must be nearly the same 
                    lda      B_YPOS, x 
                    suba     B_YPOS, u 
                    bpl      noNeg3 
                    nega     
noNeg3 
                    cmpa     #BULLET_BULLET_THRESHOLD 
                    bhi      nextBulletCheck 
                    lda      B_XPOS, x 
                    suba     B_XPOS, u 
                    bpl      noNeg4 
                    nega     
noNeg4 
                    ldb      B_VELOCITY,u 
                    addb     B_VELOCITY,x 
                    stb      -1,s 
                    cmpa     -1,s 
                    bhi      nextBulletCheck 
; bullets hit!
removeDoubleBullets 
                    pshs     u 
                    leau     ,x 
                    jsr      removeBullet 
                    puls     u 
                    jsr      removeBullet 
                    jmp      processNextBullet 

nextBulletCheck 
                    ldx      B_NEXT_OBJECT,x 
                    cmpx     #OBJECT_LIST_COMPARE_ADDRESS 
                    lbhi     processnextbulletbullettest 
                    bra      checkPlayerHitDone 

checkPlayerHit 
                    if       INVINCIBLE = 1 
                    bra      checkPlayerHitDone 

                    endif    
* See if bullet hit the player
                    ldd      B_YPOS_TAIL,u                ; * Position of bullet's head 
                    ldx      #player_y                    ; * Check for contact with player ? 
                    CHECK_FOR_CONTACT  currentPlayerMapVelocity 
                    bhs      checkPlayerHitDone           ; 
                    tst      <mainJumpIndex               ; * Do nothing if player is already 
                    bne      checkPlayerHitDone           ; * dead or has left the maze. 
                    lda      #SOUND_PLAYER_DEATH_START_BIT ; 
                    sta      <soundTypes                  ; * Make a sound 
                    lda      #$04                         ; 
                    sta      <mainJumpIndex               ; * Flag player as dead 
                    jsr      removeBullet 
                    jmp      processNextBullet 

checkPlayerHitDone 
P08B2: 
* Update the delta between the head and tail of bullet
P08B6:              ldd      B_YPOS_TAIL,u                ; 
                    suba     B_YPOS,u                     ; 
                    subb     B_XPOS,u                     ; 
                    std      B_HEIGHT,u                   ; 
* Loop until all bullets have been processed
                    ldu      B_NEXT_OBJECT,u 
                    jmp      processNextBullet 

exitBulletProcessing: 
                    rts                                   ; 

*
* DrawWithPossibleScrolling()
*
* This function draws an object at its scrolled position,
* until the starting point for the object being drawn
* scrolls off, at which point, the object is no longer
* drawn.  The scrolling offsets are incremented each time
* PlayerHasLeftMaze() is called.  Most of the code below
* is trying to determine if the starting point has
* scrolled off in the expected direction.
*
* Entry:
*   x = ptr to vector list (for generic draw functions)
*   u = ptr to drawing position
*   C8C9 = drawing function to call
DrawWithPossibleScrolling: 
                    jsr      dptoC8                       ; 
                    ldb      ,u                           ; 
                    sex                                   ; 
                    addd     <$E9                         ; 
                    stb      <$9D                         ; * starting y pos + scrolling offset 
                    bpl      P08E6                        ; 
                    coma                                  ; 
                    bne      DoNoDrawing                  ; 
                    tst      <$Ed                         ; 
                    bmi      P08F7                        ; 
                    tst      ,u                           ; 
                    bpl      DoNoDrawing                  ; 
                    lda      #$3F                         ; 
                    cmpa     <$E7                         ; 
                    blt      DoNoDrawing                  ; 
                    bra      P08F7                        ; 

P08E6:              tsta                                  ; 
                    bne      DoNoDrawing                  ; 
                    tst      <$Ed                         ; 
                    bpl      P08F7                        ; 
                    tst      ,u                           ; 
                    bmi      DoNoDrawing                  ; 
                    lda      #$C1                         ; 
                    cmpa     <$E7                         ; 
                    bge      DoNoDrawing                  ; 
P08F7:              ldb      1,u                          ; 
                    sex                                   ; 
                    addd     <$Eb                         ; 
                    stb      <$9E                         ; * starting x pos + scrolling offset 
                    bpl      P0913                        ; 
                    coma                                  ; 
                    bne      DoNoDrawing                  ; 
                    tst      <$Ed                         ; 
                    bmi      P0924                        ; 
                    tst      1,u                          ; 
                    bpl      DoNoDrawing                  ; 
                    lda      #$3F                         ; 
                    cmpa     <$E8                         ; 
                    blt      DoNoDrawing                  ; 
                    bra      P0924                        ; 

P0913:              tsta                                  ; 
                    bne      DoNoDrawing                  ; 
                    tst      <$Ed                         ; 
                    bpl      P0924                        ; 
                    tst      1,u                          ; 
                    bmi      DoNoDrawing                  ; 
                    lda      #$C1                         ; 
                    cmpa     <$E8                         ; 
                    bge      DoNoDrawing                  ; 
P0924:              jsr      dptoD0                       ; 
                    direct   $d000 
                    ldd      $C89D                        ; * Goto modified drawing origin 
                    jsr      move_pen7F_to_d              ; 
                    lda      drawScale                    ; * Set drawing intensity 
                    sta      <$04                         ; 
                    jsr      [drawFunction]               ; * Jump to drawing function 
DoNoDrawing: 
                    jmp      dptoD0                       ; 

*
* CalculateUpdatedPosition()
*
* This function updates a position, based upon a direction
* of travel, and a velocity setting.  The velocity is
* broken up into a rise and a run value, and added to the
* incoming position.
*
* Entry:
*   b = direction of travel
*   a = velocity setting
*   x = pointer to a position
*
* Exit:
*   d = updated position (y,x)
                    direct   $c800 
CalculateUpdatedPosition: 
                    bitb     #$03                         ; 1, 2 left right 
                    beq      AddVelocityToEndpoints       ; 
                    bitb     #$0C                         ; 4, 8 up, down 
                    beq      AddVelocityToEndpoints       ; 
* Break up velocity into rise and run components
                    pshs     b                            ; 
                    ldb      #$03                         ; 
                    mul                                   ; 
                    asrb                                  ; 
                    bhs      P094A                        ; 
                    incb                                  ; 
P094A:              asrb                                  ; 
                    bhs      P094E                        ; 
                    incb                                  ; 
P094E:              tfr      b,a                          ; 
                    puls     b                            ; 
AddVelocityToEndpoints: 
                    sta      <$9D                         ; * Save adjusted velocity 
                    bitb     #$11                         ; 
                    beq      P095B                        ; 
                    nega                                  ; * Going left; - velocity 
                    bra      P0960                        ; 

P095B:              bitb     #$02                         ; 
                    bne      P0960                        ; * Going right; + velocity 
                    clra                                  ; * No movement in x direction 
P0960:              adda     1,x                          ; * Update the x position 
                    pshs     a                            ; 
                    lda      <$9D                         ; * Reload adjusted velocity 
                    bitb     #$08                         ; 
                    beq      P096D                        ; 
                    nega                                  ; * Going down; - velocity 
                    bra      P0972                        ; 

P096D:              bitb     #$04                         ; 
                    bne      P0972                        ; * Going up; + velocity 
                    clra                                  ; * No movement in y direction 
P0972:              adda     ,x                           ; * Update the y position 
                    puls     b,pc                         ; 






*
* CalculateUpdatedPosition()
*
* This function updates a position, based upon a direction
* of travel, and a velocity setting.  The velocity is
* broken up into a rise and a run value, and added to the
* incoming position.
*
* Entry:
*   b = direction of travel
*   a = velocity setting
*   x = pointer to a position
*
* Exit:
*   d = updated position (y,x)
                    direct   $c800 
CalculateUpdatedPositionPlayer: 

; slowly increase player speed
; till in level 25 it reaches double...
 pshs b
 ldb counter16
 addb option8
 bcs doSpeedAdd
 addb option8
 bcs doSpeedAdd
 addb option8
 bcs doSpeedAdd
 addb option8
 bcs doSpeedAdd
 addb option8
 bcs doSpeedAdd
 addb option8
 bcs doSpeedAdd
 addb option8
 bcs doSpeedAdd
; addb option8
; bcs doSpeedAdd
; addb option8
; bcs doSpeedAdd
; addb option8
; bcs doSpeedAdd
 bra SpeedAddDone
doSpeedAdd
 adda #01
SpeedAddDone
 stb counter16
 puls b


                    bitb     #$03                         ; 1, 2 left right 
                    beq      AddVelocityToEndpointsP       ; 
                    bitb     #$0C                         ; 4, 8 up, down 
                    beq      AddVelocityToEndpointsP       ; 
* Break up velocity into rise and run components
                    pshs     b                            ; 
                    ldb      #$03                         ; 
                    mul                                   ; 

                    asrb                                  ; 
                    bhs      P094AP                        ; 
                    incb                                  ; 
P094AP:              asrb                                  ; 
                    bhs      P094EP                        ; 
                    incb                                  ; 
P094EP:              tfr      b,a                          ; 
                    puls     b                            ; 
AddVelocityToEndpointsP: 
                    sta      <$9D                         ; * Save adjusted velocity 
                    bitb     #$11                         ; 
                    beq      P095BP                        ; 
                    nega                                  ; * Going left; - velocity 
                    bra      P0960P                        ; 

P095BP:              bitb     #$02                         ; 
                    bne      P0960P                        ; * Going right; + velocity 
                    clra                                  ; * No movement in x direction 
P0960P:              adda     1,x                          ; * Update the x position 
                    pshs     a                            ; 
                    lda      <$9D                         ; * Reload adjusted velocity 
                    bitb     #$08                         ; 
                    beq      P096DP                        ; 
                    nega                                  ; * Going down; - velocity 
                    bra      P0972P                        ; 

P096DP:              bitb     #$04                         ; 
                    bne      P0972P                        ; * Going up; + velocity 
                    clra                                  ; * No movement in y direction 
P0972P:              adda     ,x                           ; * Update the y position 
                    puls     b,pc                         ; 






*
*        This is one of the 'indirect jump' functions.  It is the
* active jump function when the player successfully leaves
* a maze.  When the player exits from a door in the maze,
* the maze will then scroll off in the opposite direction ;
* i.e. If the player exits through the left door, the maze
* will scroll off to the right.  Each time this function
* is called, it will bump the scrolling deltas            ; this
* continues until scrolling is complete.  It also sets
* the player's starting position in the next maze; if the
* player exited from the left, then he will start at the
* right edge of the next maze, and the door through which
* he exited will be blocked.
PlayerHasLeftMaze: 
                    ldb      #$80                         ; 
                    stb      <$Ed                         ; 
                    ldb      <$f1                         ; * Determine door player exited thru 
                    bitb     #$01                         ; 
                    beq      P0986                        ; 
                    clr      <$Ed                         ; * Left door, so scroll right 
                    inc      <$E8                         ; 
                    bra      P09B2                        ; 

P0986:              bitb     #$02                         ; 
                    beq      P098E                        ; 
                    dec      <$E8                         ; * Right door, so scroll left 
                    bra      P09A0                        ; 

P098E:              bitb     #$04                         ; 
                    beq      P0996                        ; 
                    dec      <$E7                         ; * Top door, so scroll down 
                    bra      P09A0                        ; 

P0996:              bitb     #$08                         ; 
                    beq      P09A0                        ; 
                    clr      <$Ed                         ; * Bottom door, so scroll up 
                    inc      <$E7                         ; 
                    bra      P09B2                        ; 

*
* This block checks to see if we have finished scrolling,
* and if so, prepares us for entering the next maze.
* It will also set the player's starting position in the
* next maze.
P09A0:              bmi      SaveScrollingDeltas          ; 
                    bitb     #$02                         ; 
                    beq      P09AC                        ; * Exited thru right door? 
                    lda      <player_y                    ; 
                    ldb      #$A5                         ; * Yes; start at left edge 
                    bra      P09C2                        ; 

P09AC:              lda      #$B0                         ; * Exited thru top door 
                    ldb      <player_x                    ; * Start at bottom 
                    bra      P09C2                        ; 

P09B2:              bpl      SaveScrollingDeltas          ; 
                    bitb     #$08                         ; * Exited thru bottom door? 
                    beq      P09BE                        ; 
                    lda      #$54                         ; * Yes; start at top edge 
                    ldb      <player_x                    ; 
                    bra      P09C2                        ; 

P09BE:              lda      <player_y                    ; * Exited thru left door 
                    ldb      #$5B                         ; * Start at right edge 
P09C2:              ldx      <$80                         ; 
                    std      10,x                         ; * Update player's position 
                    lda      <$f1                         ; * This tells the exit door 
                    anda     #$0F                         ; 
                    asla                                  ; 
                    sta      <$C6                         ; * Block door player exited thru 
                    lda      #$3F                         ; 
                    sta      <$85                         ; * Set a loop counter 
                    lda      #$A0                         ; * Reset the threshold used to 
                    tst      <$D1                         ; * determine if a robot is placed in 
                    bne      P09D9                        ; * the maze. 
                    lda      #$50                         ; 
P09D9:              sta      <$A8                         ; 
                    jmp      EnterNextMaze                ; 

*
* This block saves the x and y delta values, which are
* then used by the drawing function, to offset all
* drawing origins.
SaveScrollingDeltas: 
                    ldb      <$E7                         ; 
                    sex                                   ; 
                    aslb                                  ; 
                    rola                                  ; 
                    std      <$E9                         ; * Save 16-bit y delta 
                    ldb      <$E8                         ; 
                    sex                                   ; 
                    aslb                                  ; 
                    rola                                  ; 
                    std      <$Eb                         ; * Save 16-bit x delta 
                    jmp      MakeSoundThenGotoMainLoop    ; 

*
* DrawPlayerFunction()
*
* This function draws the player's body, and then the
* arms and legs                                           ; the types of arms and legs is dependent
* upon whether the player is standing still, running
* or shooting.  (C8CE) is the index indicating movement,
* while (C8CF) is the index indicating arm position
* (running, standing still or shooting).
                    direct   -1 
*
* Whenever the player's score passes certain cutoff level,
* the game becomes more challenging.  The cutoff levels are:
* 300, 1500, 3000, 4500, 5000, 6000 and 7500
SkillLevelCutoffs: 
                    DW       $2033                        ; * " 3" 
                    DW       $3135                        ; * "15" 
                    DW       $3330                        ; * "30" 
                    DW       $3435                        ; * "45" 
                    DW       $3530                        ; * "50" 
                    DW       $3630                        ; * "60" 
                    DW       $3735                        ; * "75" 
                    DB       $80                          ; 
*
* This table is used to fill in the player's arm
* movement value (C8CF), when standing still.  The
* value is obtained from this array, by indexing into
* it using the joystick state:
*
*         ---------------------------------
*         | 0 | 0 | B | L | D | U | R | L |
*         ---------------------------------
*   Button Pressed--^
*   Joystick Left-------^---------------^
*   Joystick Down-----------^
*   Joystick Up-----------------^
*   Joystick Right------------------^
ArmsShootingIndicesTable: 
                    DB       $80                          ; * idx = $00; Invalid setting 
                    DB       $02                          ; * idx = $01; shooting left 
                    DB       $04                          ; * idx = $02; shooting right 
                    DB       $80                          ; * idx = $03; Invalid setting 
                    DB       $06                          ; * idx = $04; shooting up 
                    DB       $08                          ; * idx = $05; shooting upper left 
                    DB       $0A                          ; * idx = $06; shooting upper right 
                    DB       $80                          ; * idx = $07; Invalid setting 
                    DB       $0C                          ; * idx = $08; shooting down 
                    DB       $0E                          ; * idx = $09; shooting lower left 
                    DB       $10                          ; * idx = $0A; shooting lower right 
BonusString: 
                    DB       $70                          ; * rel y 
                    DB       $DA                          ; * rel x 
                    DB       "BONUS",$FF                  ;
*
* As the player gets better, the game will start to
* block off one of the maze doors.  The value in C8C6
* is used to index into this array, when the value > 0.
* The value in C8C6 has the following format:
*
*         ---------------------------------
*         | 0 | 0 | 0 | T | B | L | R | 0 |
*         ---------------------------------
*   Block Top Door------^
*   Block Bottom Door-------^
*   Block Left Door-------------^
*   Block Right Door----------------^
BlockedDoorVectorTable: 
                    DW       BlockedDoorVectorTable       ; * idx = 0; Invalid 
                    DW       RightDoorVectors             ; * idx = 2 
                    DW       LeftDoorVectors              ; * idx = 4 
                    DW       BlockedDoorVectorTable       ; * idx = 6; Invalid 
                    DW       BottomDoorVectors            ; * idx = 8 
                    DW       BlockedDoorVectorTable       ; * idx = 10; Invalid 
                    DW       BlockedDoorVectorTable       ; * idx = 12; Invalid 
                    DW       BlockedDoorVectorTable       ; * idx = 14; Invalid 
                    DW       TopDoorVectors               ; * idx = 16 
*
* Each of the following tables contain the relative (y,x)
* point to move to, followed by the (y,x) point to draw to.
RightDoorVectors: 
                    DB       $E0                          ; * move to absolute y 
                    DB       $64                          ; * move to absolute x 
                    DB       $40                          ; * vector rel y 
                    DB       $00                          ; * vector rel x 
LeftDoorVectors: 
                    DB       $E0                          ; * move to absolute y 
                    DB       $9C                          ; * move to absolute x 
                    DB       $40                          ; * vector rel y 
                    DB       $00                          ; * vector rel x 
TopDoorVectors: 
                    DB       $60                          ; * move to absolute y 
                    DB       $EC                          ; * move to absolute x 
                    DB       $00                          ; * vector rel y 
                    DB       $28                          ; * vector rel x 
BottomDoorVectors: 
                    DB       $A0                          ; * move to absolute y 
                    DB       $EC                          ; * move to absolute x 
                    DB       $00                          ; * vector rel y 
                    DB       $28                          ; * vector rel x 
*
* This structure contains 4 entries                       ; 1 for each of the
* sections of outer wall which must be drawn.  Each
* entry contains an absolute (y,x) position, to which
* the pen should be moved before doing any drawing, and
* a pointer to the appropriate vector list.
OutsideMazeWallData: 
                    DB       $60                          ; * absolute y 
                    DB       $EC                          ; * absolute x 
                    DW       UpperLeftWall                ; 
                    DB       $A0                          ; * absolute y 
                    DB       $EC                          ; * absolute x 
                    DW       LowerLeftWall                ; 
                    DB       $A0                          ; * absolute y 
                    DB       $14                          ; * absolute x 
                    DW       LowerRightWall               ; 
                    DB       $60                          ; * absolute y 
                    DB       $14                          ; * absolute x 
                    DW       UpperRightWall               ; 
*
* The following is a table of 8 (y,x) points, which
* represent the starting points for all interior maze
* walls.  The maze is divided into 15 quadrants, and
* these points represent the points where 4 quadrants
                    *        meet (as indicated by the '+' character:
*
*     -100 -60 -20 +20 +60 +100
*       --------------------- +96
*       | 0 | 1 | 2 | 3 | 4 |
*       ----+---+---+---+---- +32
*       | 5 | 6 | 7 | 8 | 9 |
*       ----+---+---+---+---- -32
*       |10 |11 |12 |13 |14 |
*       --------------------- -96
InnerWallPositionTable: 
                    DB       $20                          ; 
                    DB       $C4                          ; 
                    DB       $20                          ; 
                    DB       $EC                          ; 
                    DB       $20                          ; 
                    DB       $14                          ; 
                    DB       $20                          ; 
                    DB       $3C                          ; 
                    DB       $E0                          ; 
                    DB       $C4                          ; 
                    DB       $E0                          ; 
                    DB       $EC                          ; 
                    DB       $E0                          ; 
                    DB       $14                          ; 
                    DB       $E0                          ; 
                    DB       $3C                          ; 
*
* The following 4 structures contain the (y,x) points
* needed to draw 2 of the outer maze walls.
UpperLeftWall: 
                    DB       $00                          ; 
                    DB       $B0                          ; 
                    DB       $C0                          ; 
                    DB       $00                          ; 
LowerLeftWall: 
                    DB       $00                          ; 
                    DB       $B0                          ; 
                    DB       $40                          ; 
                    DB       $00                          ; 
LowerRightWall: 
                    DB       $00                          ; 
                    DB       $50                          ; 
                    DB       $40                          ; 
                    DB       $00                          ; 
UpperRightWall: 
                    DB       $00                          ; 
                    DB       $50                          ; 
                    DB       $C0                          ; 
                    DB       $00                          ; 
*
* The following is a table of 4 single vectors, each
* representing a posible vector which might be drawn
* as an inner wall.  From any one of the inner points,
* a wall could be drawn:
*
*   1) upward
*   2) downward
*   3) to the right
*   4) to the left
*
* When drawing the maze, the 8 byte array C8AB-C8B2
* (which contains one entry for each of the 8 possible
* inner wall positions) indicates which of the segments
* should be drawn                                         ; the index is either 0, 2, 4 or 6.
InnerWallVectorTable: 
                    DB       $40                          ; * Draw upward from point 
                    DB       $00                          ; 
                    DB       $C0                          ; * Draw downward from point 
                    DB       $00                          ; 
                    DB       $00                          ; * Draw right from point 
                    DB       $28                          ; 
                    DB       $00                          ; * Draw left from point 
                    DB       $D8                          ; 
OttoVectors: 
                    DB       $00                          ; 
                    DB       $18                          ; 
                    DB       $0A                          ; 
                    DB       $FF                          ; 
                    DB       $F2                          ; 
                    DB       $0E                          ; 
                    DB       $FF                          ; 
                    DB       $EC                          ; 
                    DB       $00                          ; 
                    DB       $FF                          ; 
                    DB       $F2                          ; 
                    DB       $F2                          ; 
                    DB       $FF                          ; 
                    DB       $00                          ; 
                    DB       $EC                          ; 
                    DB       $FF                          ; 
                    DB       $0E                          ; 
                    DB       $F2                          ; 
                    DB       $FF                          ; 
                    DB       $14                          ; 
                    DB       $00                          ; 
                    DB       $FF                          ; 
                    DB       $0E                          ; 
                    DB       $0E                          ; 
                    DB       $FF                          ; 
                    DB       $00                          ; 
                    DB       $14                          ; 
                    DB       $00                          ; 
                    DB       $F4                          ; 
                    DB       $03                          ; 
                    DB       $FF                          ; 
                    DB       $FD                          ; 
                    DB       $FB                          ; 
                    DB       $00                          ; 
                    DB       $00                          ; 
                    DB       $F2                          ; 
                    DB       $FF                          ; 
                    DB       $03                          ; 
                    DB       $FB                          ; 
                    DB       $00                          ; 
                    DB       $EC                          ; 
                    DB       $00                          ; 
                    DB       $FF                          ; 
                    DB       $FD                          ; 
                    DB       $05                          ; 
                    DB       $FF                          ; 
                    DB       $00                          ; 
                    DB       $0E                          ; 
                    DB       $FF                          ; 
                    DB       $03                          ; 
                    DB       $05                          ; 
                    DB       $01                          ; 
AuthorsInitials: 
                    DB       $F8                          ; * height 
                    DB       $30                          ; * width 
                    DB       $A0                          ; * y 
                    DB       $30                          ; * x 
                    DB       "CMK",$80                    ;
*
* This contains the starting positions for
* robots                                                  ; a random number is used to index into this
* table.  It is formated as (y, x) pairs.
RobotStartingPositionTable: 
                    DB       $40                          ; 
                    DB       $B0                          ; 
                    DB       $C0                          ; 
                    DB       $B0                          ; 
                    DB       $40                          ; 
                    DB       $D8                          ; 
                    DB       $00                          ; 
                    DB       $D8                          ; 
                    DB       $C0                          ; 
                    DB       $D8                          ; 
                    DB       $00                          ; 
                    DB       $00                          ; 
                    DB       $40                          ; 
                    DB       $28                          ; 
                    DB       $00                          ; 
                    DB       $28                          ; 
                    DB       $C0                          ; 
                    DB       $28                          ; 
                    DB       $40                          ; 
                    DB       $50                          ; 
                    DB       $C0                          ; 
                    DB       $50                          ; 
*
* By default, certain quadrants within the maze*always*
* have certain walls present                              ; i.e. any of the outside
                    *        quadrants. This maze contains the default 'wall mask'
* values, which are used to seed the array C8B5-C8C3.
* That array is then further modified as the interior
* walls are added.  The mask values have the following
* meaning:
*
*              ---------------------------------
*              | 0 | 0 | 0 | 0 | B | T | R | L |
*              ---------------------------------
*   Quadrant has Bottom Wall-----^
*   Quadrant has Top Wall------------^
*   Quadrant has Right Wall--------------^
*   Quadrant has Left Wall-------------------^
*
* The quadrants are defined as follows:
*
*       ---------------------
*       | 0 | 1 | 2 | 3 | 4 |
*       ----+---+---+---+----
*       | 5 | 6 | 7 | 8 | 9 |
*       ----+---+---+---+----
*       |10 |11 |12 |13 |14 |
*       ---------------------
DefaultMazeWallsPerQuadrant: 
                    DB       $05                          ; * left and top walls 
                    DB       $04                          ; * top wall 
                    DB       $04                          ; * top wall 
                    DB       $04                          ; * top wall 
                    DB       $06                          ; * top and right walls 

                    DB       $01                          ; * left wall 
                    DB       $00                          ; * no walls 
                    DB       $00                          ; * no walls 
                    DB       $00                          ; * no walls 
                    DB       $02                          ; * right wall 

                    DB       $09                          ; * bottom and left wall 
                    DB       $08                          ; * bottom wall 
                    DB       $08                          ; * bottom wall 
                    DB       $08                          ; * bottom wall 
                    DB       $0A                          ; * bottom and right walls 
PlayerFriedScales: 
                    DB       $08                          ; 
                    DB       $07                          ; 
                    DB       $06                          ; 
                    DB       $05                          ; 
                    DB       $05                          ; 
                    DB       $05                          ; 
                    DB       $06                          ; 
                    DB       $07                          ; 
                    DB       $08                          ; 
                    DB       $09                          ; 
                    DB       $08                          ; 
                    DB       $07                          ; 
                    DB       $06                          ; 
                    DB       $05                          ; 
                    DB       $06                          ; 
                    DB       $07                          ; 
*
* CheckForContact()
*
* This function checks to see if two objects have come
* into contact (such as a bullet and a robot, a bullet
* and the player, or a robot and the player.  The 2
* objects do not need to come into exact contact, but
* rather, they only need to come within a prespecified
* distance.
*
* Entry:
*   x = ptr to object to which contact is being checked.
*   a = y position to check for contact with object.
*   b = x position to check for contact with object.
*   y = ptr to table telling how close contact must be.
*
* Exit:
*   The carry bit will be set if contact was made.
CheckForContact: 
                    suba     ,x                           ; 
                    subb     1,x                          ; 
                    cmpa     ,y                           ; 
                    bgt      P0D32                        ; 
                    cmpa     1,y                          ; 
                    blt      P0D32                        ; 
                    cmpb     2,y                          ; 
                    bgt      P0D32                        ; 
                    cmpb     3,y                          ; 
                    blt      P0D32                        ; 
                    orcc     #$01                         ; 
                    rts                                   ; 

P0D32:              andcc    #$FE                         ; 
                    rts                                   ; 

*
* The following 3 tables define the bounding box around
* some figure (robot, player).  It is used to determine
* when something has hit the figure.  Each table contains
* the following values:
*
*     + y delta
*     - y delta
*     + x delta
*     - x delta
RobotContactBounds: 
                    DB       $06                          ; 
                    DB       $FA                          ; 
                    DB       $06                          ; 
                    DB       $FA                          ; 
PlayerContactBounds: 
                    DB       $05                          ; 
                    DB       $F7                          ; 
                    DB       $05                          ; 
                    DB       $FB                          ; 
; radius for wall check
; was lower than radius for bullet check
; -> shot thru wall possible!
; when player near wall!
PlayerContactBounds2: 
                    DB       $04                          ; 
                    DB       $Fc 
                    DB       $04                          ; 
                    DB       $Fc                          ; 
; org
;       DB    $05;
;       DB    $FB;
;       DB    $05;
;       DB    $FB;
*
* This function appears to do some random mucking relating
* to sound.
PSG_Shadow_REG1_ACourse  equ  $c84b 
PSG_Shadow_REG1_ACourse_and_Fine  equ  $c84b 
PSG_Shadow_REG3_BCourse  equ  $c849 
PSG_Shadow_REG3_BCourse_and_Fine  equ  $c849 
PSG_Shadow_REG5_CCourse  equ  $c847 
PSG_Shadow_REG5_CCourse_and_Fine  equ  $c847 
PSG_Shadow_REG7_Enable  equ  $c845 
PSG_Shadow_REG8_AAmplitude  equ  $c844 
PSG_Shadow_REG9_BAmplitude  equ  $c843 
PSG_Shadow_REG10_CAmplitude  equ  $c842 
                    direct   $c800 
playGameSounds: 
                    inc      <soundCounter                ; increment sound counter 
                    lda      #$0E                         ; Sound volume 
                    sta      <PSG_Shadow_REG8_AAmplitude  ; all channels to $e 
                    sta      <PSG_Shadow_REG9_BAmplitude  ; 
                    sta      <PSG_Shadow_REG10_CAmplitude ; 
                    lda      <soundTypes                  ; kind of sound(s) to play 
; PLAYER DEATH SOUND CHECK
                    bita     #SOUND_PLAYER_DEATH_CONTNUE_BIT ; is continue? 
                    beq      sound_noPlayerDeathContinue  ; 
                    jmp      sound_continuePlayerDeath    ; 

sound_noPlayerDeathContinue: 
                    bita     #SOUND_PLAYER_DEATH_START_BIT ; IS PLAYER DEAD? (sound) 
P0D56:              beq      sound_noPlayerDeathStart     ; 
                    ldd      #$00*256 + SOUND_PLAYER_DEATH_CONTNUE_BIT ; mark as continue (80), and counter = 0 
                    sta      <soundCounter                ; 
                    stb      <soundTypes                  ; 
                    jmp      sound_initPlayerDeath        ; 

sound_noPlayerDeathStart: 
; ROBOT DEATH SOUND CHECK
                    bita     #SOUND_ROBOT_DEATH_CONTNUE_BIT ; 
                    beq      sound_noRobotDeathContinue   ; 
                    jmp      sound_continueRobotDeath     ; 

sound_noRobotDeathContinue: 
                    bita     #SOUND_ROBOT_DEATH_START_BIT ; 
                    beq      sound_noRobotDeathStart      ; 
                    ldd      #$00*256 + SOUND_ROBOT_DEATH_CONTNUE_BIT ; mark as continue (40), and counter = 0 
                    sta      <soundCounter                ; 
                    stb      <soundTypes                  ; 
                    jmp      sound_initRobotDeath         ; 

sound_noRobotDeathStart: 
; ROBOT BULLET SOUND CHECK
                    bita     #SOUND_ROBOT_BULLET_START_BIT ; 
                    beq      sound_noRobotBulletStart     ; 
                    ldd      #$00*256 + SOUND_ROBOT_BULLET_CONTINUE_BIT 
                    sta      <soundCounter                ; 
                    stb      <soundTypes                  ; 
                    jmp      sound_initRobotBullet        ; 

sound_noRobotBulletStart: 
                    bita     #SOUND_ROBOT_BULLET_CONTINUE_BIT ; 
                    beq      sound_noRobotBulletContinue  ; 
                    jmp      sound_continueRobotBullet    ; 

sound_noRobotBulletContinue: 
; PLAYER BULLET SOUND CHECK
                    bita     #SOUND_PLAYER_BULLET_START_BIT ; 
                    beq      sound_noPlayerBulletStart    ; 
                    ldd      #$00*256+ SOUND_PLAYER_BULLET_CONTINUE_BIT 
                    sta      <soundCounter                ; 
                    stb      <soundTypes                  ; 
                    jmp      sound_initPlayerBullet       ; 

sound_noPlayerBulletStart: 
                    bita     #SOUND_PLAYER_BULLET_CONTINUE_BIT ; 
                    beq      sound_noPlayerBulletContinue ; 
                    jmp      sound_continuePlayerBullet   ; 

sound_noPlayerBulletContinue: 
; OTTO SOUND CHECK
                    lda      <soundType2                  ; 
                    bita     #SOUND_OTTO_CONTINUE_BIT     ; 
                    beq      sound_noOttoContinue         ; 
                    jmp      sound_ottoContinue           ; 

sound_noOttoContinue: 
                    bita     #SOUND_OTTO_START_BIT        ; 
                    beq      sound_noOttoStart            ; 
                    jmp      sound_initOtto               ; 

sound_noOttoStart: 
                    rts                                   ; 

sound_initRobotBullet: 
                    ldd      #$0039                       ; 
                    std      <$4B                         ; 
                    ldd      #$0030                       ; 
                    std      <$49                         ; 
                    ldd      #$0100                       ; 
                    std      <$47                         ; 
                    bra      P0DD8                        ; 

sound_continueRobotBullet: 
                    ldd      <$4B                         ; 
                    addd     #$0014                       ; 
                    std      <$4B                         ; 
                    ldd      <$49                         ; 
                    addd     #$0024                       ; 
                    std      <$49                         ; 
                    ldd      <$47                         ; 
                    addd     #$0060                       ; 
                    std      <$47                         ; 
P0DD8:              lda      #$24                         ; 
;
; compares current soundCounter
; to max length given in reg A
; if max length reached deinit sound channels
sound_exitRoutined: 
                    cmpa     <soundCounter                ; compare with max counter 
                    bls      sound_clearRegs              ; if max is lower than counter, than sound is done - branch 
                    lda      #$38                         ; enable all tone channels 
                    bra      P0DE8                        ; 

sound_clearRegs: 
                    lda      #$3F                         ; disable all tone & noise channels 
                    clr      <soundTypes                  ; clear current played type 
                    clr      <soundType2                  ; 
P0DE8: 
                    sta      <PSG_Shadow_REG7_Enable      ; 
                    rts                                   ; 

sound_initPlayerDeath: 
                    ldd      #$00C0                       ; 
                    std      <$4B                         ; 
                    clr      muck_6a                      ; 
sound_continuePlayerDeath: 
                    lda      <soundCounter                ; 
                    bita     #$01                         ; 
                    beq      P0DFF                        ; 
                    ldd      <$4B                         ; 
                    addd     #$0010                       ; 
                    bra      P0E04                        ; 

P0DFF:              ldd      <$4B                         ; 
                    subd     #$0013                       ; 
P0E04:              std      <$4B                         ; 
                    ldb      muck_6a                      ; 
                    incb                                  ; 
                    cmpb     #$08                         ; 
                    bls      P0E0F                        ; 
                    ldb      #$06                         ; 
P0E0F:              stb      muck_6a                      ; 
                    lda      #$50                         ; 
                    mul                                   ; 
                    std      <$49                         ; 
                    ldb      muck_6a                      ; 
                    lda      #$31                         ; 
                    mul                                   ; 
                    std      <$47                         ; 
                    lda      #$2C                         ; 
                    jmp      sound_exitRoutined           ; 

sound_initRobotDeath: 
                    ldd      #$0030                       ; 
                    std      <PSG_Shadow_REG1_ACourse_and_Fine ; 
                    ldd      #$0000                       ; 
                    std      <PSG_Shadow_REG3_BCourse_and_Fine ; 
                    ldd      #$0120                       ; 
                    std      <PSG_Shadow_REG5_CCourse_and_Fine ; 
                    rts                                   ; 

sound_continueRobotDeath: 
                    ldx      #PSG_Shadow_REG1_ACourse_and_Fine ; 
                    bsr      robotDeathSoundTriller       ; 
;  ORG                  ldx      #PSG_Shadow_REG3_BCourse_and_Fine ; BUG? should that perhaps be channel C? 
                    ldx      #PSG_Shadow_REG5_CCourse_and_Fine ; BUG? should that perhaps be channel C? 
                    bsr      robotDeathSoundTriller       ; 
                    lda      #$18                         ; max "length" of sound 
                    jmp      sound_exitRoutined           ; 

*
* This function appears to do some random mucking relating
* to sound.
*
* entry: X shadow pointer to channel period registers
robotDeathSoundTriller: 
                    lda      <soundCounter                ; 
                    bita     #$01                         ; 
                    beq      P0E4E                        ; 
                    ldd      ,x                           ; 
                    addd     #$0023                       ; 
                    bra      P0E53                        ; 

P0E4E:              ldd      ,x                           ; 
                    subd     #$0010                       ; 
P0E53:              std      ,x                           ; 
                    rts                                   ; 

*
* This function appears to do some random mucking relating
* to sound.
sound_initPlayerBullet: 
                    lda      #$03                         ; 
                    sta      muck_6a                      ; 
                    ldd      #$0039                       ; 
                    std      <$4B                         ; 
                    ldd      #$0030                       ; 
                    std      <$49                         ; 
                    ldd      #$0040                       ; 
                    std      <$47                         ; 
                    rts                                   ; 

*
* This function appears to do some random mucking relating
* to sound.
sound_continuePlayerBullet: 
                    lda      muck_6a                      ; 
                    bmi      P0E7F                        ; 
                    ldb      <soundCounter                ; 
                    cmpb     #$04                         ; 
                    bls      P0E7F                        ; 
                    clr      <soundCounter                ; 
                    dec      muck_6a                      ; 
                    ldd      #$0039                       ; 
                    std      <$4B                         ; 
                    bra      P0E86                        ; 

P0E7F:              ldd      <$4B                         ; 
                    addd     #$001A                       ; 
                    std      <$4B                         ; 
P0E86:              ldd      <$49                         ; 
                    addd     #$0024                       ; 
                    std      <$49                         ; 
                    ldd      <$47                         ; 
                    addd     #$0024                       ; 
                    std      <$47                         ; 
                    lda      #$1C                         ; 
                    jmp      sound_exitRoutined           ; 

sound_initOtto: 
                    lda      #SOUND_OTTO_CONTINUE_BIT     ; 
                    clr      <soundTypes                  ; 
                    clr      <soundCounter                ; 
                    sta      <soundType2                  ; 
                    ldd      #$0000                       ; 
                    std      <$47                         ; 
                    ldd      #$0300                       ; 
                    std      <$4B                         ; 
sound_ottoContinue:  lda     <soundCounter                ; 
                    cmpa     #$06                         ; 
                    bls      P0EBC                        ; 
                    cmpa     #$0C                         ; 
                    bhi      P0EBC                        ; 
                    ldd      <$4B                         ; 
                    subd     #$0040                       ; 
                    std      <$4B                         ; 
P0EBC:              ldd      <$4B                         ; 
                    lsra                                  ; 
                    rorb                                  ; 
                    std      <$49                         ; 
                    lda      #$18                         ; 
                    jmp      sound_exitRoutined           ; 

*
* CheckForQuadrantEdgeCrossing()
*
* This function determines whether an object has crossed
* over the edge of a quadrant                             ; this helps to determine
* when a player runs into a wall, or exits through an
* open door.  It returns a mask, which indicates which
* edge (or edges) have been crossed.
*
* Entry:
*   d = a point in the quadrant, relative to the lower left
*       corner of the quadrant.
*   x = pointer to a table of boundary conditions, which
*       tell how close the point must be to the actual
*       edge, to consider an edge crossing as having
*       occurred (these offsets are relative to the lower
*       left corner of the quadrant).  The values in this
*       table are:
*
*             yLeft, yRight, xLeft, xRight
*
* Exit:
*   b = mask (having the following meaning):
*
*         ---------------------------------
*         | 0 | 0 | 0 | 0 | B | T | R | L |
*         ---------------------------------
*
*   Bottom Edge Crossing----^
*   Top Edge Crossing-----------^
*   Right Edge Crossing-------------^
*   Left Edge Crossing------------------^
CheckForQuadrantEdgeCrossing: 
                    clrb                                  ; 
                    cmpa     ,x                           ; 
                    bge      P0ED0                        ; 
                    orb      #$08                         ; 
                    bra      P0ED6                        ; 

P0ED0:              cmpa     1,x                          ; 
                    ble      P0ED6                        ; 
                    orb      #$04                         ; 
P0ED6:              lda      <$9C                         ; 
                    cmpa     2,x                          ; 
                    ble      P0EDF                        ; 
                    orb      #$02                         ; 
                    rts                                   ; 

P0EDF:              cmpa     3,x                          ; 
                    bge      P0EE5                        ; 
                    orb      #$01                         ; 
P0EE5:              rts                                   ; 

*
* The following are all threshold tables, used when
* calling CheckForQuadrantEdgeContact()                   ; a different
* table is used, depending upon what object is represented
* by the point (a robot, the player, a bullet, etc).
* Quadrants are all 40 wide x 64 high.
* Each of the tables is composed of the following values:
*
*     y bottom threshold
*     y top threshold
*     x left threshold
*     x right threshold
RobotThresholds: 
                    DB       6                            ;10;1; $08-7 ; 
                    DB       $3f-6                        ; $38 +5 ; 
                    DB       $27-6                        ; $20 +5 ; 
                    DB       6                            ; 1; $08 -5 ; 
PlayerThresholds: 
                    DB       $c                           ; $0A; 
                    DB       $3e                          ; $3C; 
                    DB       $24                          ; 
                    DB       $04                          ; 
BulletThresholds: 
                    DB       $04                          ; 
                    DB       $3C                          ; 
                    DB       $24                          ; 
                    DB       $04                          ; 
BulletToWallThresholds: 
                    DB       $01                          ; 
                    DB       $3F                          ; 
                    DB       $27                          ; 
                    DB       $01                          ; 
*
* DetermineMazeWalls()
*
* This function calculates the 8 walls (and the optional
* door) which are present in a maze.  The maze has 8
* internal points from which a wall can start, and fromright
* each point, the wall can go either upwards, downwards,
* to the right or to the left.  The array at C8AB will be
* filled with an integer index telling which walls are to
* be drawn (0=up, 2=down, 4=to right, 6=to left).  
* The
* array at C8B5 is a collection of bitmasks, indicating
* which walls are present for each quadrant (there is one
* entry in the array per quadrant).  The mask has the
* following format:
*
*              ---------------------------------
*              | 0 | 0 | 0 | 0 | B | T | R | L |
*              ---------------------------------
*   Quadrant has Bottom Wall-----^
*   Quadrant has Top Wall------------^
*   Quadrant has Right Wall--------------^
*   Quadrant has Left Wall-------------------^
DetermineMazeWalls: 
                    lda      #$0F                         ; 
                    ldu      #DefaultMazeWallsPerQuadrant ; 
                    ldx      #$C8B5                       ; 
                    leay     ,x                           ; 
                    jsr      move_block2                  ; 
                    if       DALEK = 1 
                    rts      

                    endif    
                    leax     ,y                           ; 
                    ldb      #$08                         ; 
                    stb      <$9F                         ; 
                    ldu      #$C8AB                       ; 
P0F0C:              jsr      get_random_a                 ; 
                    anda     #$03                         ; 
                    cmpa     #$03                         ; 
                    blt      P0F23                        ; 
                    ldb      ,x                           ; * Draw to the left from point 
                    orb      #$08                         ; * This quad has a bottom wall, and 
                    stb      ,x                           ; 
                    ldb      5,x                          ; 
                    orb      #$04                         ; * the quad below it has a top wall 
                    stb      5,x                          ; 
                    bra      P0F53                        ; 

P0F23:              cmpa     #$02                         ; 
                    blt      P0F35                        ; 
                    ldb      1,x                          ; * Draw to the right from point 
                    orb      #$08                         ; * Next quad has a bottom wall, and 
                    stb      1,x                          ; 
                    ldb      6,x                          ; 
                    orb      #$04                         ; * Quad below it has a top wall 
                    stb      6,x                          ; 
                    bra      P0F53                        ; 

P0F35:              cmpa     #$01                         ; 
                    blt      P0F47                        ; 
                    ldb      5,x                          ; * Draw downwards from point 
                    orb      #$02                         ; * Quad below has a right wall, and 
                    stb      5,x                          ; 
                    ldb      6,x                          ; 
                    orb      #$01                         ; * quad next to it has a left wall 
                    stb      6,x                          ; 
                    bra      P0F53                        ; 

P0F47:              ldb      ,x                           ; * Draw upwards from point 
                    orb      #$02                         ; * This quad has a right wall, and 
                    stb      ,x                           ; 
                    ldb      1,x                          ; 
                    orb      #$01                         ; * quad next to it has a left wall 
                    stb      1,x                          ; 
P0F53:              asla                                  ; 
                    sta      ,u+                          ; * Save the wall index 
                    ldb      #$05                         ; 
                    cmpb     <$9F                         ; 
                    bne      P0F5E                        ; 
                    leax     1,x                          ; 
P0F5E:              leax     1,x                          ; 
                    dec      <$9F                         ; 
                    bne      P0F0C                        ; 
                    jsr      checkPrison 
                    jsr      checkDoubleWalls 

                    rts                                   ; 

*
* DetermineDirectionToMove()
*
* This function takes a point, and returns a mask
* indicating the direction the point must travel, in
* in order to reach the player.  It also returns the
* distance (rise and run) describing how far away the
* point is from the player.
*
* Entry:
*   a = y position
*   b = x position
*
* Exit:
*   C8DE: y distance between point and player.
*   C8DF: x distance between point and player.
*   b = mask, indicating desired direction of travel
*
*              ---------------------------------
*              | 0 | 0 | x | 0 | x | x | x | x |
*              ---------------------------------
*   x and/or y is same --^
*   Down ------------------------^
*   Up ------------------------------^
*   Right -------------------------------^
*   Left ------------------------------------^
                    direct   $c800 
DetermineDirectionToMove: 
                    adda     #$80                         ; 
                    addb     #$80                         ; 
                    std      <$Ee                         ; 
                    lda      <player_y                    ; 
                    adda     #$80                         ; 
                    clrb                                  ; 
                    suba     <$Ee                         ; 
                    beq      P0F7A                        ; 
                    blo      P0F7E                        ; 
                    orb      #$04                         ; 
                    bra      P0F80                        ; 

P0F7A:              orb      #$20                         ; 
                    bra      P0F80                        ; 

P0F7E:              orb      #$08                         ; 
P0F80:              nega                                  ; 
                    sta      <$e5                         ; 
                    lda      <player_x                    ; 
                    adda     #$80                         ; 
                    suba     <$Ef                         ; 
                    sta      <$e6                         ; 
                    beq      P0F92                        ; 
                    blo      P0F95                        ; 
                    orb      #$02                         ; 
                    rts                                   ; 

P0F92:              orb      #$20                         ; 
                    rts                                   ; 

P0F95:              orb      #$01                         ; 
                    rts                                   ; 

*
* MapPointToQnMakeRel                                     ;MapPointToQuadrantAndMakeRelative()
*
* This function takes a point, and determines which of
* 15 quadrants it falls into.  It also maps the point
* so that it is relative to the lower left corner of the
* quadrant, and returns the bitmask indicating which walls
* are defined for this quadrant.
*
* Entry:
*   d = (y,x) position
*
* Exit:
*   C89A = quadrant (0 - 15)
*   C899 = wall bitmask
*   C89B-C89C = point, relative to lower left quad corner
*   d = point, relative to lower left quad corner
*
* where the bitmask has the following format:
*
*              ---------------------------------
*              | 0 | 0 | 0 | 0 | B | T | R | L |
*              ---------------------------------
*   Quadrant has Bottom Wall-----^
*   Quadrant has Top Wall------------^
*   Quadrant has Right Wall--------------^
*   Quadrant has Left Wall-------------------^
*
* and the quadrants are defined as follows:
*
*     -100 -60 -20 +20 +60 +100
*       --------------------- +96
*       | 0 | 1 | 2 | 3 | 4 |
*       ----+---+---+---+---- +32
*       | 5 | 6 | 7 | 8 | 9 |
*       ----+---+---+---+---- -32
*       |10 |11 |12 |13 |14 |
*       --------------------- -96
MapPointToQnMakeRel:                                      ;MapPointToQuadrantAndMakeRelative  
                    std      <$9B                         ; 
                    clrb                                  ; 
                    cmpa     #$E0                         ; * -32 
                    bgt      P0FA3                        ; 
                    addb     #$0A                         ; 
                    bra      P0FA9                        ; 

P0FA3:              cmpa     #$20                         ; * 32 
                    bgt      P0FA9                        ; 
                    addb     #$05                         ; 
P0FA9:              lda      <$9C                         ; 
                    cmpa     #$C4                         ; * -60 
                    ble      P0FBF                        ; 
                    incb                                  ; 
                    cmpa     #$EC                         ; * -20 
                    ble      P0FBF                        ; 
                    incb                                  ; 
                    cmpa     #$14                         ; * 20 
                    ble      P0FBF                        ; 
                    incb                                  ; 
                    cmpa     #$3C                         ; * 60 
                    ble      P0FBF                        ; 
                    incb                                  ; 
P0FBF:              stb      <quadrantOfPoint             ; * Save the quadrant number 
                    ldx      #$C8B5                       ; 
                    lda      b,x                          ; 
                    sta      <$99                         ; * Save the wall bitmask 
                    ldx      #RelativeOffsetConversionTable ; 
                    aslb                                  ; 
                    ldd      b,x                          ; 
                    addd     <$9B                         ; 
                    std      <$9B                         ; * Make into a relative point 
                    rts                                   ; 

RelativeOffsetConversionTable: 
                    DB       $E0                          ; * Quad 0 y 
                    DB       $64                          ; * Quad 0 x 
                    DB       $E0                          ; * Quad 1 y 
                    DB       $3C                          ; * Quad 1 x 
                    DB       $E0                          ; * Quad 2 y 
                    DB       $14                          ; * Quad 2 x 
                    DB       $E0                          ; * Quad 3 y 
                    DB       $EC                          ; * Quad 3 x 
                    DB       $E0                          ; * Quad 4 y 
                    DB       $C4                          ; * Quad 4 x 
                    DB       $20                          ; * Quad 5 y 
                    DB       $64                          ; * Quad 5 x 
                    DB       $20                          ; * Quad 6 y 
                    DB       $3C                          ; * Quad 6 x 
                    DB       $20                          ; * Quad 7 y 
                    DB       $14                          ; * Quad 7 x 
                    DB       $20                          ; * Quad 8 y 
                    DB       $EC                          ; * Quad 8 x 
                    DB       $20                          ; * Quad 9 y 
                    DB       $C4                          ; * Quad 9 x 
                    DB       $60                          ; * Quad 10 y 
                    DB       $64                          ; * Quad 10 x 
                    DB       $60                          ; * Quad 11 y 
                    DB       $3C                          ; * Quad 11 x 
                    DB       $60                          ; * Quad 12 y 
                    DB       $14                          ; * Quad 12 x 
                    DB       $60                          ; * Quad 13 y 
                    DB       $EC                          ; * Quad 13 x 
                    DB       $60                          ; * Quad 14 y 
                    DB       $C4                          ; * Quad 14 x 
; check for double draw
; e.g.
; going down and up from point 0 and 4, results in double draw!
;
* C8AB-C8B2     Array of byte-long offsets, telling which walls are to be
*               drawn from each of the 8 possible starting points within
*               the maze (0=up, 2=down, 4=to right, 6=to left).  The 8
*               possible starting points are shown below:
*
*                   -100 -60 -20 +20 +60 +100
*                     +---+---+---+---+---+ +96
*                     |                   |
*                     +   o   o   o   o   + +32
*                     |                   |
*                     +   o   o   o   o   + -32
*                     |                   |
*                     +---+---+---+---+---+ -96
*              ---------------------------------
*              | 0 | 0 | 0 | 0 | B | T | R | L |
*              ---------------------------------
*   Quadrant has Bottom Wall-----^
*   Quadrant has Top Wall------------^
*   Quadrant has Right Wall--------------^
*   Quadrant has Left Wall-------------------^
checkDoubleWalls 
; vertical
                    lda      #4                           ; vertical checks 
                    sta      <$9F                         ; 
                    ldu      #$C8AB+0                     ; first 4 points 
                    ldx      #$C8B5+10                    ; 5 lower quadrants 
doNextVerticalCheck 
; start point 0
                    lda      ,u 
                    cmpa     #2                           ; down 
                    bne      nextVerticalCheck1 
                    lda      4,u 
; 0 = up
                    bne      nextVerticalCheck1 
                    lda      #2                           ; swap up/down in lower section 
                    sta      4,u 
                                                          ; update quadrant info 
                    lda      ,x 
                    ora      #2                           ; quadrant 10 now also has a right wall 
                    sta      ,x 
                    lda      1,x 
                    ora      #1                           ; quadrant 11 now also has a left wall 
                    sta      1,x 
nextVerticalCheck1 
                    dec      <$9F                         ; 
                    beq      verticalDone 
                    leau     1,u 
                    leax     1,x 
                    bra      doNextVerticalCheck 

verticalDone 
                    lda      #3                           ; vertical checks 
                    sta      <$9F                         ; 
                    ldu      #$C8AB+0 
                    ldx      #$C8B5+1 
doNextUpperHorizontalCheck 
; start point 0
                    lda      ,u 
                    cmpa     #4                           ; right 
                    bne      nextUpperHorizontalCheck1 
                    lda      1,u 
                    cmpa     #6                           ; left 
                    bne      nextUpperHorizontalCheck1 
                    lda      #0                           ; up instead of left 
                    sta      1,u 
                                                          ; update quadrant info 
                    lda      ,x 
                    ora      #2                           ; quadrant 1 now also has a right wall 
                    sta      ,x 
                    lda      1,x 
                    ora      #1                           ; quadrant 2 now also has a left wall 
                    sta      1,x 
nextUpperHorizontalCheck1 
                    dec      <$9F                         ; 
                    beq      upperHorizontalDone 
                    leau     1,u 
                    leax     1,x 
                    bra      doNextUpperHorizontalCheck 

upperHorizontalDone 
                    lda      #3                           ; vertical checks 
                    sta      <$9F                         ; 
                    ldu      #$C8AB+4 
                    ldx      #$C8B5+11 
doNextLowerHorizontalCheck 
; start point 0
                    lda      ,u 
                    cmpa     #4                           ; right 
                    bne      nextLowerHorizontalCheck1 
                    lda      1,u 
                    cmpa     #6                           ; left 
                    bne      nextLowerHorizontalCheck1 
                    lda      #2                           ; down instead of left 
                    sta      1,u 
                                                          ; update quadrant info 
                    lda      ,x 
                    ora      #2                           ; quadrant 11 now also has a right wall 
                    sta      ,x 
                    lda      1,x 
                    ora      #1                           ; quadrant 12 now also has a left wall 
                    sta      1,x 
nextLowerHorizontalCheck1 
                    dec      <$9F                         ; 
                    beq      lowerHorizontalDone 
                    leau     1,u 
                    leax     1,x 
                    bra      doNextLowerHorizontalCheck 

lowerHorizontalDone 
                    rts      

; one of the four center pieces could contain a "prison"
; if the four surrounding "draw" points
; have walls like
; left, down, right, up
; 
; -> change one of them!
*                      ---------------------------------
*                      | 0 | 0 | 0 | 0 | B | T | R | L |
*                      ---------------------------------
*           Quadrant has Bottom Wall-----^
*           Quadrant has Top Wall------------^
*           Quadrant has Right Wall--------------^
*           Quadrant has Left Wall-------------------^
*
* C89A          Quadrant into which a point falls:
*
*                   -100 -60 -20 +20 +60 +100
*                     +---+---+---+---+---+ +96
*                     | 0 | 1 | 2 | 3 | 4 |
*                     +---o---o---o---o---+ +32
*                     | 5 | 6 | 7 | 8 | 9 |
*                     +---o---o---o---o---+ -32
*                     |10 |11 |12 |13 |14 |
*                     +---+---+---+---+---+ -96
; prisons can be clock wise or antic clock wise
; I do not check for direction, I set all 4
; draws to a correct settings
; (0=up, 2=down, 4=to right, 6=to left)
; YES 6 because these are ADDS! to a pointer to walls!
; each point can only have ONE line!
checkPrison 
                    ldb      $C8B5+6                      ; left most center section 
                    cmpb     #$f 
                    bne      noLeftPrison 
; change point 1 from left to up 
;
; change Quadrant info
; update two quadrants, collsion detection remove one wall
                    andb     #$f-4                        ; no top wall 
                    stb      $C8B5+6 
                    lda      $C8B5+1 
                    anda     #$f-8                        ; no bottom wall 
; and add another instead
                    ora      #$1                          ; but a left wall 
                    sta      $C8B5+1 
                    lda      $C8B5 
                    ora      #$2                          ; and a right wall 
                    sta      $C8B5 
; change draw info, clockwise
                    lda      #2 
                    sta      $C8AB+1                      ; draw down 
                    lda      #6 ; 
                    sta      $C8AB+5                      ; draw left 
                    lda      #0 
                    sta      $C8AB+4                      ; draw up 
                    sta      $C8AB+0                      ; draw up in first point 
noLeftPrison 
                    ldb      $C8B5+7                      ; center most center section 
                    cmpb     #$f 
                    bne      noCenterPrison 
; change point 1 from left to up 
;
; change Quadrant info
; update two quadrants, remove one wall
                    andb     #$f-4                        ; no top wall 
                    stb      $C8B5+7 
                    lda      $C8B5+2 
                    anda     #$f-8                        ; no bottom wall 
; and add another instead
                    ora      #$1                          ; but a left wall 
                    sta      $C8B5+2 
                    lda      $C8B5+1 
                    ora      #$2                          ; and a right wall 
                    sta      $C8B5+1 
; change draw info, clockwise
                    lda      #2 
                    sta      $C8AB+2                      ; draw down 
                    lda      #6 
                    sta      $C8AB+6                      ; draw left 
                    lda      #0 
                    sta      $C8AB+5                      ; draw up 
                    sta      $C8AB+1                      ; draw up in first point 
noCenterPrison 
                    ldb      $C8B5+8                      ; center most center section 
                    cmpb     #$f 
                    bne      noRightPrison 
; change point 1 from left to up 
;
; change Quadrant info
; update two quadrants, remove one wall
                    andb     #$f-4                        ; no top wall 
                    stb      $C8B5+8 
                    lda      $C8B5+3 
                    anda     #$f-8                        ; no bottom wall 
; and add another instead
                    ora      #$1                          ; but a left wall 
                    sta      $C8B5+3 
                    lda      $C8B5+2 
                    ora      #$2                          ; and a right wall 
                    sta      $C8B5+2 
; change draw info, clockwise
                    lda      #2 
                    sta      $C8AB+3                      ; draw down 
                    lda      #6 
                    sta      $C8AB+7                      ; draw left 
                    lda      #0 
                    sta      $C8AB+6                      ; draw up 
                    sta      $C8AB+2                      ; draw up in first point 
noRightPrison 
                    rts      

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
initBulletList                                            ;#isfunction  
; 
                    lda      #MAX_BULLETS_OBJECTS 
                    ldu      #bulletObjects 
                    stu      bullet_empty_head 
                    ldy      #BULLET_NONE 
next_PS_list_entry_ig3 
                    leax     BulletStruct,u 
                    stx      B_NEXT_OBJECT,u 
                    leau     ,x 
                    deca     
                    bne      next_PS_list_entry_ig3 
                    leau     -BulletStruct,u 
                    sty      B_NEXT_OBJECT,u 
                    sty      bullet_objects_head 
                    clr      bulletCount 
                    rts      

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; new list object to U
; leaves with flags set to result
; (positive = not successfull) ROM
; negative = successfull RAM
; destroys d, u 
newBulletObject                                           ;#isfunction  
                    ldu      bullet_empty_head 
                    cmpu     #OBJECT_LIST_COMPARE_ADDRESS 
                    bls      newBulletObject_nops 
                                                          ; set the new empty head 
                    ldd      B_NEXT_OBJECT,u              ; the next in out empty list will be the new 
                    std      bullet_empty_head            ; head of our empty list 
                                                          ; load last of current object list 
; the old head is always our next
                    ldd      bullet_objects_head 
                    std      B_NEXT_OBJECT,u 
; newobject is always head
                    stu      bullet_objects_head 
                    inc      bulletCount                  ; and remember that we created a new object 
newBulletObject_nops 
                    rts      

; 
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; bullet pointer in u
; leaves with "next" bullet in u
removeBullet 
                    if       ROBOT_BULLET_RESTRICTION = 1 
                    lda      B_ORIGIN, u 
                    bne      noRobotBuellt 
                    dec      robotShotCounter 
noRobotBuellt 
                    endif    
                    cmpu     bullet_objects_head          ; is it the first? 
                    bne      was_not_first_re             ; no -> jump 
was_first_re 
                    ldx      B_NEXT_OBJECT,u              ; s pointer to next objext 
                    stx      bullet_objects_head          ; the next object will be the first 
                    bra      removeBulletEnd 

was_not_first_re                                          ;        find previous, go thru all objects from first and look where "I" am the next... 
                    ldy      bullet_objects_head          ; start at list head 
try_next_re 
                    cmpu     B_NEXT_OBJECT,y              ; am I the next object of the current investigated list element 
                    beq      found_next_switch_re         ; jup -> jump 
                    ldy      B_NEXT_OBJECT,y              ; otherwise load the next as new current 
                    bra      try_next_re                  ; and search further 

found_next_switch_re 
                    ldx      B_NEXT_OBJECT,u              ; we load "our" next object to s 
                    stx      B_NEXT_OBJECT,y              ; and store our next in the place of our previous next and thus eleminate ourselfs 
removeBulletEnd 
                    dec      bulletCount 
                    ldy      bullet_empty_head            ; set u free, as new free head 
                    sty      B_NEXT_OBJECT,u              ; load to u the next linked list element 
                    stu      bullet_empty_head 
                    tfr      x,u 
                    rts      

;
colorSetup 
                    lda      isColoredMode 
                    beq      bwSetup 
                    lda      #BLUE 
                    sta      currentMazeColor 
                    lda      #RED 
                    sta      currentDoorColor 
                    lda      #GREEN 
                    sta      currentPlayerColor 
                    sta      currentStringColor 
                    lda      #YELLOW 
                    ldb      $C883 
                    cmpb     #2 
                    blt      noBadRobots2 
                    lda      #RED 
                    cmpb     #3 
                    ble      noBadRobots2 
                    lda      #CYAN 
noBadRobots2 
                    sta      currentRobotColor 
                    sta      currentShotColor 
                    rts      

bwSetup 
                    lda      #$7f 
                    sta      currentShotColor 
                    lda      #$5f 
                    sta      currentPlayerColor 
                    sta      currentMazeColor 
                    sta      currentDoorColor 
                    sta      currentStringColor 
                    sta      currentRobotColor 
                    rts      

;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
                    direct   $d000 
                    include  "smartList.asm"
                    include  "smartLists.asm"
;***************************************************************************
                    INCLUDE  "epromStuff.i"
;***************************************************************************
goback 
ramfunction         equ      newEepromRAMStart 
;
;   Immediately back to menu
;
                    ldu      #ramfunctiondata             ; copy the vec4ever switching function into place 
                    ldx      #ramfunction 
                    lda      #1+ramfuncend-ramfunctiondata 
                    jsr      Move_Mem_a 
                    ldx      #$1000                       ; the 'switch back to menu' command 
                    jmp      ramfunction                  ; up up and away 

COPY_RAM_TO_VECROM 
                    ldu      #v4e_saveBlockStart          ; Source copy the vec4ever switching function into place 
                    ldx      #vec4SaveBuffer              ; destination 
                    lda      #1+(v4e_saveBlockEnd-v4e_saveBlockStart) 
                    jmp      Move_Mem_a 

store_score 
                    bsr      COPY_RAM_TO_VECROM 
                    ldu      #StoreHiscoreFnc             ; copy the vec4ever switching function into place 
                    ldx      #ramfunction 
                    lda      #1+StoreHiscoreFncEnd-StoreHiscoreFnc 
                    jsr      Move_Mem_a 
                    ldx      #$4000                       ; the 'store data' command 
                    jmp      ramfunction                  ; up up and away 

;
; the function below does the magic handshake with the cart,
; then waits for the new cart data to appear in the cart address
; space and jumps back to the v4e cart
;
StoreHiscoreFnc 
                    lda      $7ff0                        ; notify v4e 
                    lda      ,x                           ; and put command on the bus 
                    ldd      # "g "
v4eloop             cmpd     $0                           ; while the cart is working there is only one data byte 
                    bne      v4eloop                      ; header just in case 
                    rts      

StoreHiscoreFncEnd 
;
; the function below does the magic handshake with the cart,
; then waits for the new cart data to appear in the cart address
; space and jumps back to the menu
;
ramfunctiondata 
                    ldb      $7ff0                        ; notify the cart uProc 
                    ldb      ,x                           ; put command on the bus 
                    ldx      #0 
                    ldd      # "g "
ramloop             cmpd     0,x                          ; while the cart is setting up itself there is only one data byte 
                    bne      ramloop                      ; available, so check for .two. known and different ones 
                    leax     $D,x                         ; 0-A: "GCE xxxx",$80 / B+C: music pointer (could contain a zero..) 
ramloop2            lda      ,x+                          ; look for end of menu cart header 
                    bne      ramloop2 
                    tfr      x,pc                         ; return to menu code data 
ramfuncend 
vec4SaveBuffer: 
                    ds       (v4e_saveBlockEnd-v4e_saveBlockStart) 
                    include  "malbanPresents.asm"
                    include  "berzerkRasterIntro.asm"
                    include  "vecvox.i"
; copy of BIOS
; routine, needed to initiat "coin detected..."
Vec_Max_Games       EQU      $C850                        ;Maximum number of games for Select_Game 
Vec_Max_Players     EQU      $C84F                        ;Maximum number of players for Select_Game 
Vec_Num_Players     EQU      $C879                        ;Number of players selected in Select_Game 
Vec_Num_Game        EQU      $C87A                        ;Game number selected in Select_Game 
Vec_Text_HW         EQU      $C82A                        ;Default text height and width 
Vec_Button_1_1      EQU      $C812                        ;Current toggle state of stick 1 button 1 
Vec_Button_1_2      EQU      $C813                        ;Current toggle state of stick 1 button 2 
Vec_Button_1_3      EQU      $C814                        ;Current toggle state of stick 1 button 3 
Vec_Button_1_4      EQU      $C815                        ;Current toggle state of stick 1 button 4 
Vec_Btn_State       EQU      $C80F                        ;Current state of all joystick buttons 
Read_Btns_Mask      EQU      $F1B4                        ; 
DP_to_D0            EQU      $F1AA                        ; 
DP_to_C8            EQU      $F1AF                        ; 
Dec_3_Counters      EQU      $F55A                        ; 
Intensity_7F        EQU      $F2A9                        ; 
Display_Option      EQU      $F835 
;
Player_Str:         FDB      $20C0 
                    FDB      $40C0 
                    FCC      "PLAYER"
                    FCB      $80 
;
Game_Str:           FDB      $E0C0 
                    FDB      $01C0 
                    FCC      " GAME"
                    FCB      $80 
;
LF793_RTS:          RTS      

LF793_RTS2: 
                    RTS      

select_game 
                    STD      Vec_Max_Players              ;Save max players and games 
                    TSTA                                  ;If non-zero players specified, 
                    BEQ      LF7B1 
                    LDA      #$01                         ; set selection to 1 
LF7B1:              TSTB                                  ;If non-zero games specified, 
                    BEQ      LF7B6 
                    LDB      #$01                         ; set selection to 1 
LF7B6:              STD      Vec_Num_Players              ;Save default selection 
                    JSR      DP_to_C8                     ;DP to RAM 
                    LDD      #$F850 
                    STD      <Vec_Text_HW 
                    STA      <$C83C                       ;Set $C83C flag to non-zero 
                    BRA      LF82C 

LF7C5:              JSR      Wait_Recal                   ;Start with a fresh frame, DP to I/O 
                    jsr      PLY_PLAY 
                    jsr      do_ym_sound2 
                    CLRA                                  ;Read buttons, all in direct mode 
                    JSR      Read_Btns_Mask 
                    JSR      Dec_3_Counters 
                    JSR      Intensity_7F                 ;Brightness to normal 
                    LDA      Vec_Num_Players              ;Display number of players 
                    LDY      #Player_Str 
                    JSR      Display_Option 
                    LDA      Vec_Num_Game                 ;Display currently selected game 
                    LDY      #Game_Str 
                    JSR      Display_Option 
                    JSR      DP_to_C8                     ;DP to RAM 
                    LDA      <$C83C                       ;If $C83C=0, check buttons 
                    BEQ      LF7F1 
                    LDA      <Vec_Btn_State 
                    anda     #$f 
                    cmpa     #1 
                    bne      noOptions4 
                    jsr      editOptions 
                    bra      noOptionsDone4 

noOptions4 
                    tsta     
                    BNE      LF82C                        ;If any button pressed, reset timers 
noOptionsDone4 
                    CLR      <$C83C                       ;Clear $C83C flag 
LF7F1:              LDA      <Vec_Counter_2               ;Return if counter 2 timed out 
                    BEQ      displayHighScoreListDemo 
                    LDA      <Vec_Counter_1               ;If repeat timer not timed out, 
                    BNE      LF7C5                        ; ignore the buttons 
                    LDA      <Vec_Button_1_4 
                    BNE      LF793_RTS                    ;Return if button 4 pressed 
                    LDA      <Vec_Button_1_1 
                    BEQ      LF810 
                    LDA      <Vec_Num_Players             ; Ignore if no players option 
                    BEQ      LF810                        ;If button 1 pressed, 
                    INCA                                  ; increment number of players 
                    CMPA     <Vec_Max_Players 
                    BLS      LF80C 
                    LDA      #$01                         ;Reset to 1 if max players exceeded 
LF80C:              STA      <Vec_Num_Players             ; Update number of players 
                    BRA      LF82C                        ;Update timers and go back to the loop 

LF810:              LDA      <Vec_Num_Game                ;Return to the loop if no game options 
                    BEQ      LF7C5 
                    LDB      <Vec_Button_1_2 
                    BEQ      LF821 
                    INCA                                  ;If button 2 down, increment game 
                    CMPA     <Vec_Max_Games 
                    BLS      LF82A 
                    LDA      #$01                         ;Reset to 1 if maximum exceeded 
                    BRA      LF82A 

LF821:              LDB      <Vec_Button_1_3 
                    BEQ      LF7C5 
                    DECA                                  ;If button 3 down, decrement game 
                    BNE      LF82A 
                    LDA      <Vec_Max_Games               ;Reset to max if zero reached 
LF82A:              STA      <Vec_Num_Game 
LF82C:              LDA      #$F3                         ;Reset timers 
                    STA      <Vec_Counter_2 
                    COMA     
                    STA      <Vec_Counter_1 
                    jmp      LF7C5                        ;Go back to the loop 

;
                    include  "highscoreStuff.asm"
                    include  "optionEdit.asm"
;
Vec_Snd_Shadow      EQU      $C800                        ;Shadow of sound chip registers (15 bytes) 
;
do_ym_sound2 
                                                          ;#isfunction 
                    direct   $d0 
; copy all shadows
                    lda      #13                          ; number of regs to copy (+1) 
                    ldx      #Vec_Music_Work              ; music players write here 
                    ldu      #Vec_Snd_Shadow              ; shadow of actual PSG 
next_reg_dsy: 
                    ldb      a, x 
                    cmpb     a, u 
                    beq      inc_reg_dsy 
; no put to psg
                    stb      a,u                          ; ensure shadow has copy 
; a = register
; b = value
                    STA      <VIA_port_a                  ;store register select byte 
                    LDA      #$19                         ;sound BDIR on, BC1 on, mux off _ LATCH 
                    STA      <VIA_port_b 
                    LDA      #$01                         ;sound BDIR off, BC1 off, mux off - INACTIVE 
                    STA      <VIA_port_b 
                    LDA      <VIA_port_a                  ;read sound chip status (?) 
                    STB      <VIA_port_a                  ;store data byte 
                    LDB      #$11                         ;sound BDIR on, BC1 off, mux off - WRITE 
                    STB      <VIA_port_b 
                    LDB      #$01                         ;sound BDIR off, BC1 off, mux off - INACTIVE 
                    STB      <VIA_port_b 
inc_reg_dsy: 
                    deca     
                    bpl      next_reg_dsy 
doneSound_2: 
                    rts      

                    include  "arkosPlayer.i"
;***************************************************************************
; DATA SECTION
;***************************************************************************
aks_song: 
                    include  "berzerkMusicAKS.asm"
version:
    db "V 1.4",$80