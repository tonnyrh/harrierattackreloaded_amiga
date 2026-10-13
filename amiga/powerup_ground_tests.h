#if HAR_HEADLESS_POWERUP_GROUND_TEST
static UBYTE powerupGroundContract(void) {
 static GameState g;modPlaying=1;
 cpcLandSkillLevel=1;cpcLandRouteExtension=CPC_LAND_EXTENSION_PER_DIFFICULTY;
 cpcLandProceduralLength=CPC_LAND_PROCEDURAL_BASE_LENGTH+cpcLandRouteExtension;
 initGameState(&g,1,1,1);g.gameMode=GAME_MODE_ENHANCED;
 LONG depot=-1;
 for(UWORD i=0;i<cpcLandProceduralLength;i++) {
  LONG c=CPC_LAND_PROCEDURAL_WORLD_START+i;
  if(isFuelDepotColumn(c)){depot=c;break;}
 }
 if(depot<0)return 1;
 WORD row=terrainYForWorldColumn(depot,levelSegmentForWorldColumn(depot),HAR_TERRAIN_CPC_RANDOM_LAND)-1;
 ObjectCell before,after;
 if(!aircraftObjectCell(depot,row,&before))return 2;
 g.scrollX=depot*8-100;g.playerX=8;g.playerY=8;
 ULONG revision=worldObjectRevision;
 for(UBYTE side=0;side<2;side++) {
  memset(&g.powerup,0,sizeof(g.powerup));
  g.powerup.active=1;g.powerup.type=POWERUP_BOMBS;
  g.powerup.worldX=depot*8+(side ? 7 : -15);
  g.powerup.y=g.powerup.logicalY=row*8-POWERUP_SPRITE_HEIGHT-1;
  if(powerupHitsSolidWorld(&g,&g.powerup))return 3;
  g.powerup.fallRate=8;
  updatePowerup(&g);
  if(g.powerup.active)return 4;
  if(worldObjectRevision!=revision || !aircraftObjectCell(depot,row,&after) ||
     before.id!=after.id || before.tile!=after.tile || before.hp!=after.hp)return 5;
 }
 /* A sideways drift may contact a target without a vertical step. */
 g.powerup.active=1;g.powerup.worldX=depot*8-16;
 g.powerup.y=g.powerup.logicalY=row*8-4;
 g.powerup.fallRate=0;g.powerup.driftRate=8;
 updatePowerup(&g);if(g.powerup.active)return 6;
 if(worldObjectRevision!=revision)return 7;
 return 0;
}
#endif
