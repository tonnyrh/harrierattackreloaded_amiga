/* Scene transitions are committed between logical updates, never inside a renderer. */
enum { CAMPAIGN_NONE, CAMPAIGN_TAKEOFF, CAMPAIGN_TERRAIN, CAMPAIGN_CARRIER, CAMPAIGN_OIL, CAMPAIGN_BATTLESHIP, CAMPAIGN_LANDING, CAMPAIGN_RETURN };
static struct {UBYTE request,transition,specialDone,cheated,debugOpen,debugSelected,oilRoute,oceanReturn;} campaign;
static UBYTE debugSceneJumpEnabled;
static UBYTE campaignSortie(UBYTE mission) {return (mission ? (mission-1)%3 : 0)+1;}
static const char* const campaignSceneNames[]={"", "TAKEOFF", "LAND + CITY", "CARRIER ATTACK", "OIL PLATFORM", "BATTLESHIP", "LANDING"};

static UBYTE campaignContract(void);
