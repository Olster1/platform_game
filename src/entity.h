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
    ENTITY_TYPE_LIGHT,
    ENTITY_TYPE_CAMERA,
    ENTITY_TYPE_PARTICLE_SYSTEM,
    ENTITY_TYPE_BACKGROUND_IMAGE
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
	
	
	float inverse_I; //inertia value
	float inverseWeight;

	GLBufferHandles bufferHandles;

	Asset *tex;

	float timeInAir;

	float timeSinceLastPoll; //this is polling grounded positions

	int lastGroundedAt;
	int lastGroundedCount;
	V3 lastGroundedPos[20];

	LerpV4 comeBackShading; // this should be a buffer so we can't keep pushing different shadings on, like being hurt etc. 

	animation_list_item AnimationListSentintel;
	Asset *animationParent; //these is the static/global animation

	float angle; //in 2d so always around the z-axis
	float dA; //delta angle

	particle_system *particleSystem;

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
FUNC(NOTE_COUNT) \

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

	LerpV4 fadeInLerp;
	bool timerHasRun;

	Timer swayTimer; //for movement

	bool isPlayedByParent;

	int parentCount;
#define NOTE_PARENT_COUNT 4
	NoteParent *parents[NOTE_PARENT_COUNT];
} Note;

//TODO: look up flux shaders. 
typedef struct {
	float flux;
	Entity_Commons *e;
} Light;

#define NOTE_PARENT_TYPE(FUNC) \
FUNC(NOTE_PARENT_NULL) \
FUNC(NOTE_PARENT_DEFAULT) \
FUNC(NOTE_PARENT_TIME) \

typedef enum {
	NOTE_PARENT_TYPE(ENUM)
} NoteParentType;

static char *NoteParentTypeStrings[] = { NOTE_PARENT_TYPE(STRING) };
static NoteParentType NoteParentTypeValues[] = { NOTE_PARENT_TYPE(ENUM) };

typedef struct {
#define NOTE_VALUE_SIZE 8
	int count;
	Note *notes_[NOTE_VALUE_SIZE];	

	float timeValue;
	bool isPlayedByParent[NOTE_VALUE_SIZE];
} ChordInfo;

#define MAX_NOTE_SEQUENCE_SIZE 16
typedef struct NoteParent{
	Entity_Commons *e;

	NoteParentType type;

	bool lookedAt;
	//Used to match whether it is right
	//This is a ring buffer
	int valueAt;
	int valueCount;
	ChordInfo values[MAX_NOTE_SEQUENCE_SIZE];
	Timer inputTimer;
	int chordAt; 
	//

	bool showChildren;

	GLBufferHandles puzzleProgressRenderHandles[MAX_NOTE_SEQUENCE_SIZE];
	LerpV4 puzzleShadeLerp[MAX_NOTE_SEQUENCE_SIZE];

	//////SET ON CREATION////
	int noteValueCount;
	ChordInfo sequence[MAX_NOTE_SEQUENCE_SIZE]; //don't have more than a 32 note sequence. 
	////

	int soundAt; 
	Timer soundTimer;
	LerpV4 shadingLerp;

	int autoSoundAt; 
	Timer autoSoundTimer;
	bool autoPlaying;

	bool solved; //this will be saved for player progress 
	Event *eventToTrigger; 
} NoteParent;

typedef enum {
	ENTITY_FLAG_INVISIBLE = 1 << 0,
} EntityPlayerFlag; 

typedef struct {
	Entity_Commons *e;
	Door *lastDoor;
	Note *lastNote;
	NoteParent *lastParentNote;

	long int flags;
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
void setupEntityCommons(Entity_Commons *common, Array_Dynamic *particleSystemArray, void *entParent, EntType entType) {
	particle_system_settings particleSet = InitParticlesSettings(PARTICLE_SYS_DEFAULT);
	// pushParticleBitmap(&particleSet, &globalFireTex_debug);
	common->particleSystem = (particle_system *)getEmptyElement(particleSystemArray);
	InitParticleSystem(common->particleSystem, &particleSet, 12);
	common->AnimationListSentintel.Next = common->AnimationListSentintel.Prev = &common->AnimationListSentintel; 
	common->parent = entParent;
	common->entType = entType;
	common->comeBackShading = initLerpV4();
	common->comeBackShading.value = COLOR_WHITE;
}

Entity_Commons *initEntityCommons_(void *entParent, Entity_Commons *common, Array_Dynamic *particleSystemArray, V3 pos, Asset *tex, float inverseWeight, int ID, EntType entType) {
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
    	
    setupEntityCommons(common, particleSystemArray, entParent, entType);

	return common;
}

Entity_Commons *initEntityCommons(void *entParent, Array_Dynamic *commons_, Array_Dynamic *particleSystemArray, V3 pos, Asset *tex, float inverseWeight, int ID, EntType entType) {
	Entity_Commons *common = (Entity_Commons *)getEmptyElement(commons_);

	common = initEntityCommons_(entParent, common, particleSystemArray, pos, tex, inverseWeight, ID, entType);
	return common;
}

void setupEnt(Entity *entity, Entity_Commons *common, Array_Dynamic *particleSystemArray) {
	if(common) {
		entity->e = common;
		setupEntityCommons(common, particleSystemArray, entity, ENTITY_TYPE_ENTITY);
	}
}

void initEntity(Array_Dynamic *commons_, Entity *entity, Array_Dynamic *particleSystemArray, V3 pos, Asset *tex, float inverseWeight, int ID) {
	memset(entity, 0, sizeof(Entity)); //clear entity to null
	entity->e = initEntityCommons(entity, commons_, particleSystemArray, pos, tex, inverseWeight, ID, ENTITY_TYPE_ENTITY);
	entity->e->type = ENT_TYPE_DEFAULT;
	setFlag(entity->e, ENTITY_ANIMATED);

	setupEnt(entity, 0, particleSystemArray);
}

void setupCollisionEnt(Collision_Object *entity, Entity_Commons *common, Array_Dynamic *particleSystemArray) {
	if(common) {
		entity->e = common;
		setupEntityCommons(common, particleSystemArray, entity, ENTITY_TYPE_COLLISION);
	}
}

void initCollisionEnt(Array_Dynamic *commons_, Collision_Object *entity, Array_Dynamic *particleSystemArray, V3 pos, Asset *tex, float inverseWeight, int ID) {
	memset(entity, 0, sizeof(Collision_Object)); //clear entity to null
	entity->e = initEntityCommons(entity, commons_, particleSystemArray, pos, tex, inverseWeight, ID, ENTITY_TYPE_COLLISION);
	entity->e->dim.x = 4;
	setFlag(entity->e, ENTITY_COLLISION);
	entity->e->type = ENT_TYPE_WORLD_COLLISION;

	setupCollisionEnt(entity, 0, particleSystemArray);
}

void setupPlatformEnt(Entity *entity, Entity_Commons *common, Array_Dynamic *particleSystemArray) {
	if(common) {
		entity->e = common;
		setupEntityCommons(common, particleSystemArray, entity, ENTITY_TYPE_PLATFORM);
	}
}

void initPlatformEnt(Array_Dynamic *commons_, Entity *entity, Array_Dynamic *particleSystemArray, V3 pos, Asset *tex, int ID, PlatformType platformType) {
	memset(entity, 0, sizeof(Entity)); //clear entity to null
	entity->e = initEntityCommons(entity, commons_, particleSystemArray, pos, tex, 0.01, ID, ENTITY_TYPE_PLATFORM); //the platforms do have a mass but aren't affected by gravity
	entity->e->type = ENT_TYPE_DEFAULT;
	entity->e->dim.x = 4;
	entity->e->type = ENT_TYPE_PLATFORM;
	entity->platformType = platformType;
	entity->e->centerPoint = pos;
	unSetFlag(entity->e, ENTITY_GRAVITY_AFFECTED);

	setupPlatformEnt(entity, 0, particleSystemArray);
}

void setupDoorEnt(Door *entity, Entity_Commons *common, Array_Dynamic *particleSystemArray) {
	if(common) {
		entity->e = common;
		setupEntityCommons(common, particleSystemArray, entity, ENTITY_TYPE_DOOR);
	}
}

void initDoorEnt(Array_Dynamic *commons_, Door *entity, Array_Dynamic *particleSystemArray, V3 pos, Asset *tex, int ID) {
	memset(entity, 0, sizeof(Door)); //clear entity to null
	entity->e = initEntityCommons(entity, commons_, particleSystemArray, pos, tex, 0, ID, ENTITY_TYPE_DOOR);
	entity->e->type = ENT_TYPE_DEFAULT;
	unSetFlag(entity->e, ENTITY_COLLIDES);

	setupDoorEnt(entity, 0, particleSystemArray);
}

void setupScenarioEnt(Entity *entity, Entity_Commons *common, Array_Dynamic *particleSystemArray) {
	if(common) {
		entity->e = common;
		setupEntityCommons(common, particleSystemArray, entity, ENTITY_TYPE_SCENARIO);
	}
}

void initScenarioEnt(Array_Dynamic *commons_, Entity *entity, Array_Dynamic *particleSystemArray, V3 pos, Asset *tex, int ID) {
	memset(entity, 0, sizeof(Entity)); //clear entity to null
	entity->e = initEntityCommons(entity, commons_, particleSystemArray, pos, tex, 0, ID, ENTITY_TYPE_SCENARIO);
	entity->e->type = ENT_TYPE_DEFAULT;
	unSetFlag(entity->e, ENTITY_COLLIDES);
	setupScenarioEnt(entity, 0, particleSystemArray);
}

void setupNoteEnt(Note *entity, Entity_Commons *common, Array_Dynamic *particleSystemArray) {
	if(common) {
		entity->e = common;
		setupEntityCommons(common, particleSystemArray, entity, ENTITY_TYPE_NOTE);
	}
	entity->swayTimer = initTimer(0.7f);
	entity->fadeInLerp = initLerpV4();
	entity->fadeInLerp.value = COLOR_NULL;


}

void initNoteEnt(Array_Dynamic *commons_, Note *entity, Array_Dynamic *particleSystemArray, V3 pos, Asset *tex, Asset *sound, int ID) {
	memset(entity, 0, sizeof(Note)); //clear entity to null
	entity->e = initEntityCommons(entity, commons_, particleSystemArray, pos, tex, 0, ID, ENTITY_TYPE_NOTE);
	entity->e->type = ENT_TYPE_DEFAULT;
	entity->sound = sound;
	assert(sound);
	unSetFlag(entity->e, ENTITY_COLLIDES);
	setupNoteEnt(entity, 0, particleSystemArray);
}

void setupNoteParent(NoteParent *entity, Entity_Commons *common, Array_Dynamic *particleSystemArray) {
	if(common) {
		entity->e = common;
		setupEntityCommons(common, particleSystemArray, entity, ENTITY_TYPE_NOTE_PARENT);
	}

	entity->e->particleSystem->Set.VelBias = rect2fMinMax(-1, -1, 1, 1);
	entity->e->particleSystem->Set.posBias = rect2fMinMax(-0.01, -0.01, 0, 0);
	entity->inputTimer = initTimer(2.0f);
	entity->chordAt = 0;
	
	entity->e->particleSystem->Set.type = PARTICLE_SYS_CIRCULAR;
	entity->shadingLerp = initLerpV4();
	for(int i = 0; i < arrayCount(entity->puzzleShadeLerp); ++i) {
		LerpV4 *l = entity->puzzleShadeLerp + i;
		*l = initLerpV4();	
		l->value = COLOR_RED; //starting color for the red
	}
	entity->solved = false;
	entity->autoSoundTimer = initTimer(2.0f); //THIS NEEDS TO BE SET IN THE SAVE LOAD FILE
	entity->type = NOTE_PARENT_DEFAULT;
}

// void initParticleSystem() {
// 	particleSystem
// }

void initNoteParentEnt(Array_Dynamic *commons_, NoteParent *entity, Array_Dynamic *particleSystemArray, V3 pos, NoteParentType type, Asset *tex, int ID) {
	memset(entity, 0, sizeof(NoteParent)); //clear entity to null
	entity->e = initEntityCommons(entity, commons_, particleSystemArray, pos, tex, 0, ID, ENTITY_TYPE_NOTE_PARENT);
	entity->e->type = ENT_TYPE_DEFAULT;
	unSetFlag(entity->e, ENTITY_COLLIDES);

	setupNoteParent(entity, 0, particleSystemArray);

	entity->type = type;
}

void setupLight(Light *entity, Entity_Commons *common, Array_Dynamic *particleSystemArray) {
	if(common) {
		entity->e = common;
		setupEntityCommons(common, particleSystemArray, entity, ENTITY_TYPE_LIGHT);
	}
}

void initLight(Array_Dynamic *commons_, Light *entity, Array_Dynamic *particleSystemArray, V3 pos, float flux, int ID) {
	memset(entity, 0, sizeof(Light)); //clear entity to null
	Entity_Commons *common = (Entity_Commons *)malloc(sizeof(Entity_Commons));
	entity->e = initEntityCommons_(entity, common, particleSystemArray, pos, 0, 0, ID, ENTITY_TYPE_LIGHT);
	//entity->e = initEntityCommons_(entity, commons_, pos, 0, 0, ID, ENTITY_TYPE_LIGHT);
	entity->e->type = ENT_TYPE_DEFAULT;
	unSetFlag(entity->e, ENTITY_COLLIDES);
	unSetFlag(entity->e, ENTITY_GRAVITY_AFFECTED);

	setupLight(entity, 0, particleSystemArray);

	entity->flux = flux;
}

void addDialog(Event *event, char *dialog) {
	assert(event->type == EVENT_DIALOG);
	assert(event->dialogCount < arrayCount(event->dialog));
	event->dialog[event->dialogCount++] = dialog;
}

void setupNPCEnt(NPC *entity, Entity_Commons *common, Array_Dynamic *particleSystemArray) {
	if(common) {
		entity->e = common;
		setupEntityCommons(common, particleSystemArray, entity, ENTITY_TYPE_NPC);
	}
	
	entity->stateTimer = initTimer(1);
	entity->state = NPC_IDLE;
}

void initNPCEnt(Array_Dynamic *commons_, Array_Dynamic *events, NPC *entity, Array_Dynamic *particleSystemArray, V3 pos, Asset *tex, float inverseWeight, int ID, int eventId) {
	memset(entity, 0, sizeof(NPC)); //clear entity to null
	entity->e = initEntityCommons(entity, commons_, particleSystemArray, pos, tex, inverseWeight, ID, ENTITY_TYPE_NPC);
	entity->e->type = ENT_TYPE_DEFAULT;
	setFlag(entity->e, ENTITY_ANIMATED);
		
	setupNPCEnt(entity, 0, particleSystemArray);
	Entity_Commons *com = entity->e;
	
	Event_EntCommonsInfo evComInfo = {};
	evComInfo.id = com->ID;
	evComInfo.pos = &com->pos;

	unSetFlag(entity->e, ENTITY_COLLIDES_WITH_PLAYER);
	unSetFlag(entity->e, ENTITY_COLLIDES);
	unSetFlag(entity->e, ENTITY_GRAVITY_AFFECTED);
	
	entity->event = addDialogEvent(events, &evComInfo, v3_scale(2, entity->e->dim), EVENT_EXPLICIT | EVENT_TRIGGER, eventId);
}

void addValueToNoteParent(NoteParent *parent, Note *note) {
	if(parent->type == NOTE_PARENT_TIME) {
		ChordInfo *info = parent->values + parent->chordAt;		
		if(info->count < arrayCount(info->notes_)) {
			info->notes_[info->count++] = note;
		}
	}

	// assert(parent->valueAt < arrayCount(parent->values) && parent->valueAt >= 0);

	// if(parent->valueAt >= arrayCount(parent->values)) {
	//     parent->valueAt = 0;
	// }
	// //TODO: There needs to be a timer for a threshold 

	// ChordInfo *info = parent->values + chordAt;
	// if(info->count < arrayCount(info->notes_)) {
	// 	assert(info->count < arrayCount(info->notes_));
	// 	info->notes_[info->count++] = note;
	// }
	// parent->valueCount = min(arrayCount(parent->values), ++parent->valueCount);
	// assert(parent->valueCount <= arrayCount(parent->values));
}

#define calculateRenderInfoForEntity(ent, cameraPos, metresToPixels) calculateRenderInfo(v3_plus(ent->pos, ent->renderPosOffset), ent->dim, cameraPos, metresToPixels)
