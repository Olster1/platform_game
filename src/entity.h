#define ENUM(value) value,
#define STRING(value) #value,

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
FUNC(PLATFORM_LINEAR) \
FUNC(PLATFORM_FIGURE_OF_EIGHT) \

typedef enum {
	PLATFORM_TYPE(ENUM)
} PlatformType;

static char *PlatformTypeStrings[] = { PLATFORM_TYPE(STRING) };

typedef struct {
	int ID;

	char *name;
	
	EntityType type;
	unsigned long flags;
	V3 renderPosOffset; //position offset for rendering 

	V3 pos; //position
	V3 dP; //velocity
	V3 ddP; //velocity
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

Entity_Commons *initEntityCommons(Array_Dynamic *commons_, V3 pos, Asset *tex, float inverseWeight, int ID) {
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

	particle_system_settings particleSet = InitParticlesSettings(PARTICLE_SYS_DEFAULT);
	pushParticleBitmap(&particleSet, &globalFireTex_debug);
	InitParticleSystem(&common->particleSystem, &particleSet, 12);

	if(inverseWeight != 0) {
		common->inverse_I = ((1/inverseWeight)*(sqr(common->dim.x) + sqr(common->dim.y))) / 12.0f;	
		//else the inverse_I is zero. 
	} 

	common->inverseWeight = inverseWeight;
    common->AnimationListSentintel.Next = common->AnimationListSentintel.Prev = &common->AnimationListSentintel; 

	return common;
}

void initEntity(Array_Dynamic *commons_, Entity *entity, V3 pos, Asset *tex, float inverseWeight, int ID) {
	memset(entity, 0, sizeof(Entity)); //clear entity to null
	entity->e = initEntityCommons(commons_, pos, tex, inverseWeight, ID);
	entity->e->type = ENT_TYPE_DEFAULT;
	setFlag(entity->e, ENTITY_ANIMATED);
}

void initCollisionEnt(Array_Dynamic *commons_, Collision_Object *entity, V3 pos, Asset *tex, float inverseWeight, int ID) {
	memset(entity, 0, sizeof(Collision_Object)); //clear entity to null
	entity->e = initEntityCommons(commons_, pos, tex, inverseWeight, ID);
	entity->e->dim.x = 4;
	setFlag(entity->e, ENTITY_COLLISION);
	entity->e->type = ENT_TYPE_WORLD_COLLISION;
}

void initPlatformEnt(Array_Dynamic *commons_, Entity *entity, V3 pos, Asset *tex, int ID) {
	memset(entity, 0, sizeof(Entity)); //clear entity to null
	entity->e = initEntityCommons(commons_, pos, tex, 1 / 10000, ID); //the platforms do have a mass but aren't affected by gravity
	entity->e->type = ENT_TYPE_DEFAULT;
	entity->e->dim.x = 4;
	entity->e->type = ENT_TYPE_PLATFORM;
	entity->platformType = PLATFORM_LINEAR;
	entity->e->centerPoint = pos;
	unSetFlag(entity->e, ENTITY_GRAVITY_AFFECTED);
}

void initDoorEnt(Array_Dynamic *commons_, Door *entity, V3 pos, Asset *tex, int ID) {
	memset(entity, 0, sizeof(Door)); //clear entity to null
	entity->e = initEntityCommons(commons_, pos, tex, 0, ID);
	entity->e->type = ENT_TYPE_DEFAULT;
	unSetFlag(entity->e, ENTITY_COLLIDES);
}

void initScenarioEnt(Array_Dynamic *commons_, Entity *entity, V3 pos, Asset *tex, int ID) {
	memset(entity, 0, sizeof(Entity)); //clear entity to null
	entity->e = initEntityCommons(commons_, pos, tex, 0, ID);
	entity->e->type = ENT_TYPE_DEFAULT;
	unSetFlag(entity->e, ENTITY_COLLIDES);
}

void initNoteEnt(Array_Dynamic *commons_, Note *entity, V3 pos, Asset *tex, Asset *sound, int ID) {
	memset(entity, 0, sizeof(Note)); //clear entity to null
	entity->e = initEntityCommons(commons_, pos, tex, 0, ID);
	entity->e->type = ENT_TYPE_DEFAULT;
	entity->sound = sound;
	assert(sound);
	unSetFlag(entity->e, ENTITY_COLLIDES);
	entity->swayTimer = initTimer(0.7f);
}

void initNoteParentEnt(Array_Dynamic *commons_, NoteParent *entity, V3 pos, Asset *tex, int ID) {
	memset(entity, 0, sizeof(NoteParent)); //clear entity to null
	entity->e = initEntityCommons(commons_, pos, tex, 0, ID);
	entity->e->type = ENT_TYPE_DEFAULT;
	unSetFlag(entity->e, ENTITY_COLLIDES);

	initArray(&entity->values, NoteValue);

	entity->shadingLerp = initLerpV4();

	entity->e->particleSystem.Set.VelBias = rect2fMinMax(-1, -1, 1, 1);
	entity->e->particleSystem.Set.type = PARTICLE_SYS_CIRCULAR;
}

void addDialog(Event *event, char *dialog) {
	assert(event->type == EVENT_DIALOG);
	assert(event->dialogCount < arrayCount(event->dialog));
	event->dialog[event->dialogCount++] = dialog;
}

void initNPCEnt(Array_Dynamic *commons_, Array_Dynamic *events, NPC *entity, V3 pos, Asset *tex, float inverseWeight, int ID) {
	memset(entity, 0, sizeof(NPC)); //clear entity to null
	entity->e = initEntityCommons(commons_, pos, tex, inverseWeight, ID);
	entity->e->type = ENT_TYPE_DEFAULT;
	setFlag(entity->e, ENTITY_ANIMATED);
	unSetFlag(entity->e, ENTITY_COLLIDES_WITH_PLAYER);
	entity->stateTimer = initTimer(1);
	entity->state = NPC_IDLE;
	entity->event = addDialogEvent(events, &entity->e->pos, v3_scale(2, entity->e->dim), EVENT_EXPLICIT | EVENT_TRIGGER);
}

#define calculateRenderInfoForEntity(ent, cameraPos, metresToPixels) calculateRenderInfo(v3_plus(ent->pos, ent->renderPosOffset), ent->dim, cameraPos, metresToPixels)
