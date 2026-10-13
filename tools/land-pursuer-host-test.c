#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t UBYTE; typedef uint16_t UWORD; typedef int16_t WORD;
typedef uint32_t ULONG; typedef int32_t LONG;
#define GAME_MODE_ENHANCED 1
#define HAR_STAGE_DO_LAND 3
#define HAR_STAGE_START_PIER 8
#define HUD_TOP 168
#define SCREEN_WIDTH 320
#define ENEMY_SPRITE_WIDTH 16
#define ENEMY_SPRITE_HEIGHT 8
#define POWERUP_EXTRA_AIRCRAFT_SCROLL_X 8000
#define ENEMY_PLANE_DAMAGE_NORMAL 0
#define ENEMY_TARGET_PLAYER 1
#define ENEMY_MISSILE_PURSUER_TYPE 130
#define POWERUP_RESCUED_PILOT 7
#define SFX_FIRE 1
#define RADAR_DETECTION_MAX 1000
typedef struct { UBYTE active,timer,type,direction; WORD x,y,dx,dy,targetY; LONG worldX,targetWorldX; } WeaponState;
typedef struct {
 UBYTE gameMode,pursuerUsed,pursuerActive,ordinaryPlaneKilled,ordinaryPlaneSeen,ejectState,missionNumber,speedLevel;
 UBYTE enemyPlaneDamageState,enemyPlaneBrokenTimer,enemyPlaneRetreating,enemyMissileTarget,radarAlarmTickCountdown;
 UBYTE gameOver,crashTimer,enemyMissileFromShip,rescuedPilotPending;
 UWORD scrollX,campaignSeed,radarDetection,pursuerFireCooldown; WORD playerX,playerY,rescuedPilotY;
 LONG rescuedPilotWorldX; WeaponState enemyPlane,enemyMissile,helicopter;
 struct {UBYTE phase,repairDropPending;}defence;
 struct {UBYTE active;WORD screenY;}wingman;
 struct {UBYTE active,type,fallRate;WORD y,logicalY;LONG worldX;}powerup;
} GameState;
typedef struct {int unused;}LevelSegmentDef;
static UBYTE stage=3,blocked=0;static int shots,impacts;
static WORD radarSurfacePixelYForWorldColumn(LONG x){return blocked ? 0 : 120;}
static const LevelSegmentDef* levelSegmentForWorldColumn(UWORD x){return 0;}
static UBYTE stageForWorldColumn(UWORD x,const LevelSegmentDef* s){return stage;}
static WORD wingmanScreenX(GameState* g){return 60;}
static void maybeStartWingmanIntercept(GameState* g){}
static void startWorldImpact(GameState* g,int x,int y){impacts++;}
static void playSfxAt(int s,int x){shots++;}
static void spawnPowerup(GameState* g,UBYTE t,UBYTE y){g->powerup.active=1;g->powerup.type=t;}
#include "../amiga/land_pursuer.h"
static GameState fixture(void){GameState g;memset(&g,0,sizeof(g));g.gameMode=1;g.ordinaryPlaneSeen=1;g.radarDetection=1000;g.playerX=120;g.playerY=50;g.scrollX=6000;g.speedLevel=8;g.missionNumber=1;return g;}
int main(void){
 GameState g=fixture();g.ordinaryPlaneSeen=0;assert(!trySpawnLandPursuer(&g,790));
 g.ordinaryPlaneSeen=1;assert(!g.ordinaryPlaneKilled);assert(trySpawnLandPursuer(&g,790));assert(g.pursuerUsed&&g.pursuerActive);
 g.enemyPlane.active=0;assert(!trySpawnLandPursuer(&g,790));
 g=fixture();g.gameMode=0;assert(!trySpawnLandPursuer(&g,790));
 g=fixture();blocked=1;assert(!trySpawnLandPursuer(&g,790));blocked=0;
 g.wingman.active=1;g.wingman.screenY=50;assert(trySpawnLandPursuer(&g,790)&&g.enemyPlane.y==26);
 g=fixture();g.radarDetection=0;assert(!trySpawnLandPursuer(&g,790));
 g=fixture();g.enemyMissile.active=1;assert(!trySpawnLandPursuer(&g,790));
 g=fixture();g.defence.phase=1;assert(!trySpawnLandPursuer(&g,790));
 g=fixture();stage=8;assert(!trySpawnLandPursuer(&g,790));stage=3;
 int early=0,late=0;
 for(int i=0;i<256;i++){g=fixture();g.campaignSeed=i;g.scrollX=800;early+=trySpawnLandPursuer(&g,140);g=fixture();g.campaignSeed=i;late+=trySpawnLandPursuer(&g,790);}
 assert(early>0&&early<64&&late==256);
 g=fixture();trySpawnLandPursuer(&g,790);
 for(int i=0;i<250;i++){g.scrollX+=3;updateLandPursuer(&g);}
 assert(g.enemyPlane.x==56&&g.enemyMissile.active&&g.enemyMissile.dx>0&&shots==1);
 WORD launchY=g.enemyMissile.y;
 for(int frame=0;frame<12;frame++){g.playerY+=3;g.scrollX++;advanceLandPursuerMissile(&g);assert(g.enemyMissile.y==launchY&&g.enemyMissile.dy==0);}
 g.enemyMissile.active=0;updateLandPursuer(&g);assert(shots==2);
 g.speedLevel=2;for(int i=0;i<140&&g.enemyPlane.active;i++){g.scrollX+=2;updateLandPursuer(&g);}
 assert(!g.enemyPlane.active&&g.pursuerUsed&&shots==2);
 g=fixture();trySpawnLandPursuer(&g,790);blocked=1;updateLandPursuer(&g);blocked=0;
 assert(!g.enemyPlane.active&&impacts==1&&!g.rescuedPilotPending);
 g=fixture();trySpawnLandPursuer(&g,790);landPursuerWeaponKill(&g,1);assert(!g.rescuedPilotPending);
 g=fixture();landPursuerWeaponKill(&g,0);assert(!g.ordinaryPlaneKilled);
 landPursuerWeaponKill(&g,1);assert(g.ordinaryPlaneKilled);
 trySpawnLandPursuer(&g,790);landPursuerWeaponKill(&g,0);assert(!g.rescuedPilotPending);
 landPursuerWeaponKill(&g,1);assert(g.rescuedPilotPending);
 g.powerup.active=1;deliverRescuedPilot(&g);assert(g.rescuedPilotPending);
 g.powerup.active=0;g.defence.repairDropPending=1;deliverRescuedPilot(&g);assert(g.rescuedPilotPending);
 g.defence.repairDropPending=0;deliverRescuedPilot(&g);
 assert(!g.rescuedPilotPending&&g.powerup.active&&g.powerup.type==7&&g.powerup.worldX==g.enemyPlane.worldX);
 g=fixture();g.rescuedPilotPending=1;g.rescuedPilotWorldX=0;deliverRescuedPilot(&g);assert(!g.rescuedPilotPending&&!g.powerup.active);
 puts("PASS: admission, increasing chance, once per level, collision, cadence, braking, rescue qualification and pickup arbitration");return 0;
}
