/* Shared campaign encounter; prototype flag only selects the standalone entry. */
#ifndef HAR_BATTLESHIP_PROTOTYPE
#define HAR_BATTLESHIP_PROTOTYPE 0
#endif
#define BATTLESHIP_TARGETS 16
#define BATTLESHIP_BRIDGE 8
#define BATTLESHIP_HELIPAD 9
#define BATTLESHIP_MAX_HEALTH 37
#define BATTLESHIP_SEA_Y 120
#define BATTLESHIP_START_COLUMN 64
#define BATTLESHIP_FIRST_RETURN 352
#define BATTLESHIP_LAST_CAMERA 1312
#define BATTLESHIP_DEPART_CAMERA ((BATTLESHIP_START_COLUMN + 120) * 8 + 64)
static struct {
    UWORD stormSky[3];UBYTE desperationAlarm;
    UBYTE burning,fireDirty,fireFrame[5],fireRow[5];
    UBYTE active,phase,passed,hp[BATTLESHIP_TARGETS],pose[BATTLESHIP_TARGETS];
    UBYTE dirty[BATTLESHIP_TARGETS],flakLife,hitSafe,finished;
    UWORD clock,water,wait,pass;
    UWORD jetDelay;UBYTE jetSide;
    WORD camera16,velocity16,targetY[BATTLESHIP_TARGETS];
    WORD missileVx16,missileVy16,missileSubX,missileSubY;
    LONG flakX; WORD flakY;
    WeaponState flak[4];
    UBYTE hudHealth, savedWingman, stage, lift[BATTLESHIP_TARGETS], flash[BATTLESHIP_TARGETS];
    WORD vx16,vy16,subX,subY; UWORD supplyClock,supplyPass,rescueWait; UBYTE supplyId;

} battleship;
static const UWORD battleshipTargetX[BATTLESHIP_TARGETS]={592,704,816,944,1056,1184,1296,1392,1128,864,656,1336,752,992,1232,1440};
static UWORD *battleshipWaterHigh[4],*battleshipWaterLow[4],*battleshipWaterFine;
static void battleshipBegin(GameState* g);
static void battleshipUpdate(GameState* g,const InputState* in,const InputState* old,const Player2InputState* in2,UBYTE** buffers);
static void battleshipDrawHud(UBYTE* hud);
static void battleshipCommitDamage(UBYTE* bitmap,LONG scrollX);

static UBYTE referenceBattleship(void);

static UBYTE battleshipIsMissile(UBYTE i) { return i==1 || i==6; }
static UBYTE battleshipIsGun(UBYTE i) { return i<8 || i>=12; }
static UBYTE battleshipWidth(UBYTE i) { return i==8 || i==9 ? 24 : 16; }
static UBYTE battleshipHeight(UBYTE i) { return i==8 ? 24 : 16; }

static LONG battleshipResident[128], battleshipStreamColumn=-1;
static UBYTE battleshipStreamRow;

static const UWORD battleshipFireColumn[5]={72,96,120,144,168};
static UBYTE battleshipFireTiles[3][40];
