#if HAR_HEADLESS_SURFACE_BOUNCE_TEST_ONLY
static UBYTE referenceSurfaceBounce(void) {
 static GameState g;InputState in={0};UBYTE savedMod=modPlaying;modPlaying=1;
 memset(&oil,0,sizeof(oil));
 initGameState(&g,12040,12040,2);g.gameMode=GAME_MODE_ENHANCED;
 g.takeoffState=TAKEOFF_STATE_AIRBORNE;g.defence.phase=DEFENCE_WAVE;
 g.respawnSafeTimer=0;g.playerX=112;g.playerY=100;
 if(!carrierHullOverlap(g.playerX+2,g.playerY,12,7))return 1;
 carrierMoveVtol(&g,&in);
 if(g.crashTimer || g.armour!=62 || g.groundBounceTicks!=8 || g.playerY>=100)return 2;
 if(carrierHullOverlap(g.playerX+2,g.playerY,12,7))return 3;
 WORD y=g.playerY;carrierMoveVtol(&g,&in);
 if(g.armour!=62 || g.playerY>=y || g.groundBounceTicks!=7)return 4;
 /* Oil geometry, away from the helipad: find a solid contact in the tower. */
 initGameState(&g,12040,12040,2);g.gameMode=GAME_MODE_ENHANCED;oilBegin(&g);
 g.respawnSafeTimer=0;UBYTE found=0;
 for(WORD x=116;x<152 && !found;x++)for(WORD py=68;py<112;py++) {
  if(oilAircraftHitsStructure(x,py)){g.playerX=x;g.playerY=py;found=1;break;}
 }
 if(!found)return 5;y=g.playerY;oilPlayerMove(&g,&in);
 if(g.crashTimer || g.armour!=62 || g.groundBounceTicks!=8 || g.playerY>=y)return 6;
 if(oilAircraftHitsStructure(g.playerX,g.playerY))return 7;
 y=g.playerY;oilPlayerMove(&g,&in);
 if(g.armour!=62 || g.playerY>=y)return 8;
 /* P2 also bounces off the rig instead of losing an entire aircraft. */
 oil.p2Present=1;oil.p2Landed=oil.p2Safe=0;oil.p2Health=100;oil.p2FuelQuanta=1000;
 oil.vx=oil.vy=oil.ax=oil.ay=oil.subX=oil.subY=0;
 g.wingman.interceptScreenX=128;g.wingman.screenY=90;g.lives=3;
 if(!oilAircraftHitsStructure(128,90))return 13;
 Player2InputState p2={0};oilP2Update(&g,&p2);
 if(!oil.p2Present || oil.p2Health!=62 || g.lives!=3 || oil.vy>=0)return 14;
 if(oilAircraftHitsStructure(g.wingman.interceptScreenX,g.wingman.screenY))return 15;
 /* The same routine handles the boss's surface and never stacks damage during recoil. */
 oil.active=oil.scene=0;g.defence.phase=0;g.groundBounceTicks=0;
 g.armour=100;g.missileDamageThirds=g.flakDamageCount=0;
 g.scrollX=BATTLESHIP_START_COLUMN*8;g.playerX=80;g.playerY=116;
 battleshipPlayerSurfaceCollision(&g);
 if(g.armour!=62 || !g.groundBounceTicks)return 9;
 startEnhancedSurfaceBounce(&g,80,115);
 if(g.armour!=62)return 10;
 y=g.playerY;updatePlayerGroundBounce(&g);if(g.playerY>=y)return 11;
 /* Low armour still fails normally; a bump is not immortality. */
 g.groundBounceTicks=0;g.missileDamageThirds=250;
 startEnhancedSurfaceBounce(&g,80,115);
 if(g.armour || !g.aircraftFailureState)return 12;
 memset(&oil,0,sizeof(oil));modPlaying=savedMod;return 0;
}
#endif
