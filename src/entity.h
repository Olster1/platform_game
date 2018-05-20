typedef enum {
    ENTITY_TYPE_INVALID,
    ENTITY_TYPE_NPC,
    ENTITY_TYPE_NOTE_PARENT,
    ENTITY_TYPE_NOTE,
    ENTITY_TYPE_ENTITY, 
    ENTITY_TYPE_COLLISION,
    ENTITY_TYPE_DOOR,
    ENTITY_TYPE_PLATFORM,
    ENTITY_TYPE_COMMONS,
    ENTITY_TYPE_SCENARIO,
    ENTITY_TYPE_EVENT,
    ENTITY_TYPE_CAMERA
} EntType;

//IMPORTANT: The order of these matters. They are used in preferece for the collision detection
#define ENTITY_TYPE(FUNC) \
FUNC(ENT_TYPE_NULL) \
FUNC(ENT_TYPE_DEFAULT) \
FUNC(ENT_TYPE_PLATFORM) \
FUNC(ENT_TYPE_WORLD_COLLISION) \

typedef enum {
	ENTITY_TYPE(ENUM)
} EntityType;

static char *EntityTypeStrings[] = { ENTITY_TYPE(STRING) };


#define PLATFORM_TYPE(FUNC) \
FUNC(PLATFORM_NULL) \
FUNC(PLATFORM_CIRCLE) \
FUNC(PLATFORM_LINEAR_X) \
FUNC(PLATFORM_LINEAR_Y) \
FUNC(PLATFORM_FIGURE_OF_EIGHT) \

typedef enum {
	PLATFORM_TYPE(ENUM)
} PlatformType;

static char *PlatformTypeStrings[] = { PLATFORM_TYPE(STRING) };

typedef struct {
	int ID;

	char *name;
	
	EntType entType; //this is the actual type; 
	EntityType type;
	unsigned long flags;
	V3 renderPosOffset; //position offset for rendering 

	V3 pos; //position
	V3 dP; //velocity
	V3 ddP; //acceleration for things like platforms, NPCs etc. 
	V3 centerPoint; //for platforms, put it here for the interacting elment only holds commons.

	V4 shading;

	V3 dim;

	V3 renderScale; //render scale

	float angle; //in 2d so always around the z-axis
	float dA; //delta angle
	
	float inverse_I; //inertia value
	float inverseWeight;

	Asset *tex;

	animation_list_item AnimationListSentintel;
	Asset *animationParent; //these is the static/global animation

	particle_system particleSystem;

	void *parent;
} Entity_Commons;

typedef struct Door Door;
typedef struct Door {
	Entity_Commons *e;
	Door *partner;
} Door;


#define NOTE_VALUE_TYPE(FUNC) \
FUNC(C_NOTE) \
FUNC(D_NOTE) \
FUNC(D_SHARP_NOTE) \
FUNC(E_NOTE) \
FUNC(F_NOTE) \
FUNC(G_NOTE) \
FUNC(A_NOTE) \
FUNC(B_NOTE) \

typedef enum {
	NOTE_VALUE_TYPE(ENUM)
} NoteValue;

static char *NoteValueStrings[] = { NOTE_VALUE_TYPE(STRING) };

typedef enum {
	NPC_IDLE,
	NPC_TALK,
	NPC_WALK,
} NPC_State;

typedef struct {
	Entity_Commons *e;

	NPC_State state;
	Timer stateTimer;

	Event *event;
} NPC;


typedef struct NoteParent NoteParent;
typedef struct Note Note;
typedef struct Note {
	Entity_Commons *e;
	Asset *sound;
	NoteValue value;

	Timer swayTimer; //for movement

	NoteParent *parent;
} Note;

typedef struct NoteParent{
	Entity_Commons *e;

	//Used to match whether it is right
	Array_Dynamic values; //NoteValue type
	//This can just be the  same size as the sequence. 

	//////SET ON CREATION////
	int noteValueCount;
	NoteValue sequence[32]; //don't have more than a 32 note sequence. 
	////

	int soundAt; 
	Timer soundTimer;
	LerpV4 shadingLerp;

	bool solved;
	Event *eventToTrigger; // Won't actually be a door, but some event??
} NoteParent;

typedef struct {
	Entity_Commons *e;
	Door *lastDoor;
	Note *lastNote;
	NoteParent *lastParentNote;

	//platform attributes
	PlatformType platformType; //do we want a seperate class for platforms
	V3 centerPoint; //for platforms, put it here for the interacting elment only holds commons.

	Event *event;
	
} Entity;

typedef struct {
	Entity_Commons *e;
} Collision_Object;


typedef enum {
	GRAB_CENTER,
	GRAB_LEFT,
	GRAB_RIGHT,
	GRAB_TOP,
	GRAB_BOTTOM,
	GRAB_CAMERA,
} InteractType;

typedef struct {
	Entity_Commons *e;
	
	InteractType type;
	int index;
	Array_Dynamic *array;
} InteractItem; 

typedef enum {
	ENTITY_VALID = 1 << 0,
	ENTITY_PLAYER = 1 << 1,
	ENTITY_COLLISION = 1 << 2,
	ENTITY_COLLIDES = 1 << 3,
	ENTITY_ACTIVE = 1 << 4, //this is to say if platforms are active
	ENTITY_GRAVITY_AFFECTED = 1 << 5,
	ENTITY_ANIMATED = 1 << 6,
	ENTITY_COLLIDES_WITH_PLAYER = 1 << 7,
	ENTITY_CAMERA = 1 << 8,
} EntityFlag;



bool isFlagSet_(unsigned long flags, EntityFlag flag) {
	bool result = flags & flag;
	return result;
}

bool isFlagSet(Entity_Commons *commons, EntityFlag flag) {
	return isFlagSet_(commons->flags, flag);
}


void setFlag(Entity_Commons *commons, EntityFlag flag) {
	commons->flags |= flag;
}

void unSetFlag(Entity_Commons *commons, EntityFlag flag) {
	commons->flags &= (~flag);
}

//TODO: Make this into a hash table for quick retrieval
Entity_Commons *findEntityFromID(Array_Dynamic *ents, int id) {
	Entity_Commons *result = 0;
	for(int entIndex = 0; entIndex < ents->count; entIndex++) {
	    Entity_Commons *ent = (Entity_Commons *)getElement(ents, entIndex);
	    if(ent && isFlagSet(ent, ENTITY_VALID)) {
	    	if(ent->ID == id) {
	    		result = ent;
	    		break;
	    	}
	    }
	}
	return result;
}

//This is for just setting up things in the game that aren't unique and therefore not saved. 
void setupEntityCommons(Entity_Commons *common, void *entParent, EntType entType) {
	particle_system_settings particleSet = InitParticlesSettings(PARTICLE_SYS_DEFAULT);
	pushParticleBitmap(&particleSet, &globalFireTex_debug);
	InitParticleSystem(&common->particleSystem, &particleSet, 12);
	common->AnimationListSentintel.Next = common->AnimationListSentintel.Prev = &common->AnimationListSentintel; 
	common->parent = entParent;
	common->entType = entType;
}

Entity_Commons *initEntityCommons(void *entParent, Array_Dynamic *commons_, V3 pos, Asset *tex, float inverseWeight, int ID, EntType entType) {
	Entity_Commons *common = (Entity_Commons *)getEmptyElement(commons_);
	memset(common, 0, sizeof(Entity_Commons));

	setFlag(common, ENTITY_VALID);
	setFlag(common, ENTITY_COLLIDES);
	setFlag(common, ENTITY_ACTIVE);
	setFlag(common, ENTITY_GRAVITY_AFFECTED);
	setFlag(common, ENTITY_COLLIDES_WITH_PLAYER);

	common->ID = ID;
	common->tex = tex;
	common->pos = pos;
	common->renderScale = v3(1, 1, 1);
	common->dim = v3(1, 1, 1);
	common->shading = v4(1, 1, 1, 1);

	/* THis setups moment of inertia
	if(inverseWeight != 0) {
		common->inverse_I = ((1/inverseWeight)*(sqr(common->dim.x) + sqr(common->dim.y))) / 12.0f;	
		//else the inverse_I is zero. 
	} 
	*/

	common->inverseWeight = inverseWeight;
    	
    setupEntityCommons(common, entParent, entType);

	return common;
}

void setupEnt(Entity *entity, Entity_Commons *common) {
	if(common) {
		entity->e = common;
		setupEntityCommons(common, entity, ENTITY_TYPE_ENTITY);
	}
}

void initEntity(Array_Dynamic *commons_, Entity *entity, V3 pos, Asset *tex, float inverseWeight, int ID) {
	memset(entity, 0, sizeof(Entity)); //clear entity to null
	entity->e = initEntityCommons(entity, commons_, pos, tex, inverseWeight, ID, ENTITY_TYPE_ENTITY);
	entity->e->type = ENT_TYPE_DEFAULT;
	setFlag(entity->e, ENTITY_ANIMATED);

	setupEnt(entity, 0);
}

void setupCollisionEnt(Collision_Object *entity, Entity_Commons *common) {
	if(common) {
		entity->e = common;
		setupEntityCommons(common, entity, ENTITY_TYPE_COLLISION);
	}
}

void initCollisionEnt(Array_Dynamic *commons_, Collision_Object *entity, V3 pos, Asset *tex, float inverseWeight, int ID) {
	memset(entity, 0, sizeof(Collision_Object)); //clear entity to null
	entity->e = initEntityCommons(entity, commons_, pos, tex, inverseWeight, ID, ENTITY_TYPE_COLLISION);
	entity->e->dim.x = 4;
	setFlag(entity->e, ENTITY_COLLISION);
	entity->e->type = ENT_TYPE_WORLD_COLLISION;

	setupCollisionEnt(entity, 0);
}

void setupPlatformEnt(Entity *entity, Entity_Commons *common) {
	if(common) {
		entity->e = common;
		setupEntityCommons(common, entity, ENTITY_TYPE_PLATFORM);
	}
}

void initPlatformEnt(Array_Dynamic *commons_, Entity *entity, V3 pos, Asset *tex, int ID, PlatformType platformType) {
	memset(entity, 0, sizeof(Entity)); //clear entity to null
	entity->e = initEntityCommons(entity, commons_, pos, tex, 0.0001, ID, ENTITY_TYPE_PLATFORM); //the platforms do have a mass but aren't affected by gravity
	entity->e->type = ENT_TYPE_DEFAULT;
	entity->e->dim.x = 4;
	entity->e->type = ENT_TYPE_PLATFORM;
	entity->platformType = platformType;
	entity->e->centerPoint = pos;
	unSetFlag(entity->e, ENTITY_GRAVITY_AFFECTED);

	setupPlatformEnt(entity, 0);
}

void setupDoorEnt(Door *entity, Entity_Commons *common) {
	if(common) {
		entity->e = common;
		setupEntityCommons(common, entity, ENTITY_TYPE_DOOR);
	}
}

void initDoorEnt(Array_Dynamic *commons_, Door *entity, V3 pos, Asset *tex, int ID) {
	memset(entity, 0, sizeof(Door)); //clear entity to null
	entity->e = initEntityCommons(entity, commons_, pos, tex, 0, ID, ENTITY_TYPE_DOOR);
	entity->e->type = ENT_TYPE_DEFAULT;
	unSetFlag(entity->e, ENTITY_COLLIDES);

	setupDoorEnt(entity, 0);
}

void setupScenarioEnt(Entity *entity, Entity_Commons *common) {
	if(common) {
		entity->e = common;
		setupEntityCommons(common, entity, ENTITY_TYPE_SCENARIO);
	}
}

void initScenarioEnt(Array_Dynamic *commons_, Entity *entity, V3 pos, Asset *tex, int ID) {
	memset(entity, 0, sizeof(Entity)); //clear entity to null
	entity->e = initEntityCommons(entity, commons_, pos, tex, 0, ID, ENTITY_TYPE_SCENARIO);
	entity->e->type = ENT_TYPE_DEFAULT;
	unSetFlag(entity->e, ENTITY_COLLIDES);
	setupScenarioEnt(entity, 0);
}

void setupNoteEnt(Note *entity, Entity_Commons *common) {
	if(common) {
		entity->e = common;
		setupEntityCommons(common, entity, ENTITY_TYPE_NOTE);
	}
	entity->swayTimer = initTimer(0.7f);
}

void initNoteEnt(Array_Dynamic *commons_, Note *entity, V3 pos, Asset *tex, Asset *sound, int ID) {
	memset(entity, 0, sizeof(Note)); //clear entity to null
	entity->e = initEntityCommons(entity, commons_, pos, tex, 0, ID, ENTITY_TYPE_NOTE);
	entity->e->type = ENT_TYPE_DEFAULT;
	entity->sound = sound;
	assert(sound);
	unSetFlag(entity->e, ENTITY_COLLIDES);
	setupNoteEnt(entity, 0);
}

void setupNoteParent(NoteParent *entity, Entity_Commons *common) {
	if(common) {
		entity->e = common;
		setupEntityCommons(common, entity, ENTITY_TYPE_NOTE_PARENT);
	}

	initArray(&entity->values, NoteValue);

	entity->e->particleSystem.Set.VelBias = rect2fMinMax(-1, -1, 1, 1);
	entity->e->particleSystem.Set.type = PARTICLE_SYS_CIRCULAR;
	entity->shadingLerp = initLerpV4();
}

void initNoteParentEnt(Array_Dynamic *commons_, NoteParent *entity, V3 pos, Asset *tex, int ID) {
	memset(entity, 0, sizeof(NoteParent)); //clear entity to null
	entity->e = initEntityCommons(entity, commons_, pos, tex, 0, ID, ENTITY_TYPE_NOTE_PARENT);
	entity->e->type = ENT_TYPE_DEFAULT;
	unSetFlag(entity->e, ENTITY_COLLIDES);

	setupNoteParent(entity, 0);
}

void addDialog(Event *event, char *dialog) {
	assert(event->type == EVENT_DIALOG);
	assert(event->dialogCount < arrayCount(event->dialog));
	event->dialog[event->dialogCount++] = dialog;
}

void setupNPCEnt(NPC *entity, Entity_Commons *common) {
	if(common) {
		entity->e = common;
		setupEntityCommons(common, entity, ENTITY_TYPE_NPC);
	}
	
	entity->stateTimer = initTimer(1);
	entity->state = NPC_IDLE;
}

void initNPCEnt(Array_Dynamic *commons_, Array_Dynamic *events, NPC *entity, V3 pos, Asset *tex, float inverseWeight, int ID, int eventId) {
	memset(entity, 0, sizeof(NPC)); //clear entity to null
	entity->e = initEntityCommons(entity, commons_, pos, tex, inverseWeight, ID, ENTITY_TYPE_NPC);
	entity->e->type = ENT_TYPE_DEFAULT;
	setFlag(entity->e, ENTITY_ANIMATED);
	unSetFlag(entity->e, ENTITY_COLLIDES_WITH_PLAYER);
		
	setupNPCEnt(entity, 0);
	Entity_Commons *com = entity->e;
	
	Event_EntCommonsInfo evComInfo = {};
	evComInfo.id = com->ID;
	evComInfo.pos = &com->pos;
	
	entity->event = addDialogEvent(events, &evComInfo, v3_scale(2, entity->e->dim), EVENT_EXPLICIT | EVENT_TRIGGER, eventId);
}

#define calculateRenderInfoForEntity(ent, cameraPos, metresToPixels) calculateRenderInfo(v3_plus(ent->pos, ent->renderPosOffset), ent->dim, cameraPos, metresToPixels)
