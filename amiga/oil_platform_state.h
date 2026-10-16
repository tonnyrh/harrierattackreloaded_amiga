#ifndef HAR_OIL_PLATFORM_PROTOTYPE
#define HAR_OIL_PLATFORM_PROTOTYPE 0
#endif
#define OIL_ORIGIN_X 64
#define OIL_ORIGIN_Y 64
#define OIL_WIDTH 96
#define OIL_HEIGHT 64
#define OIL_PAD_LEFT 73
#define OIL_PAD_RIGHT 112
#define OIL_DECK_Y (OIL_ORIGIN_Y+30)
#define OIL_PLAYER_Y (OIL_DECK_Y-8)
/* Own actor state, never a duplicate GameState or a Chip bitmap. */
typedef struct OilPlatformState {
 UBYTE carrierHull,carrierCargo,carrierGuns[2];
 UBYTE active, scene, completed, approach, cpuState, cpuGunPose, fireFrame, lastBurn;
 UBYTE p2Present, p2Landed, p2Health, p2Safe, p2Respawn;
 UBYTE facing, pose, turnTicks, turnTarget, previousFire, previousBomb;
 WORD vx,vy,ax,ay,subX,subY;
 UWORD fuelTicks,p2FuelQuanta;
 WORD approachVX16,approachSub16;
} OilPlatformState;
static OilPlatformState oil;
static void oilDamageP2(GameState* g, UBYTE amount);
static void oilBegin(GameState* g);
static void oilBeginApproach(GameState* g);
static void oilUpdate(GameState* g,const InputState* in,const InputState* previous,
 const Player2InputState* in2,UBYTE** buffers);
static UBYTE oilHullOverlap(WORD x,WORD y,UBYTE w,UBYTE h);
