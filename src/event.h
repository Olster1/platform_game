typedef struct Asset Asset;

#define EVENT_TYPE(FUNC) \
FUNC(EVENT_DIALOG) \
FUNC(EVENT_V3_PAN) \
FUNC(EVENT_ENTITY_ACTIVE) \

typedef enum {
	EVENT_TYPE(ENUM)
} EventType;

static char *EventTypeStrings[] = { EVENT_TYPE(STRING) };

typedef enum {
	EVENT_NULL_FLAG = 0,
	EVENT_EXPLICIT = 1 << 0,
	EVENT_TRIGGER = 1 << 1,
	EVENT_FRESH = 1 << 2, //This is for reseting values when they are pushed as the current event; 
} EventFlag;

typedef struct Event Event;
typedef struct Event {
	int ID;

	int entId;

	char name[256];

	EventType type;
	V3 *pos;
	V3 dim;
	unsigned long flags;
	Asset *sound;
	
	union {
		struct {
			int dialogAt;
			int dialogCount;
			char *dialog[32];
			Timer dialogTimer;
			float dialogDisplayValue; //this for outputing the text one at a time. 
		};
		struct {
			LerpType lerpType;
			LerpV3 lerpValueV3;

		};
	};

	Event *nextEvent; 
} Event;

Event *findEventFromID(Array_Dynamic *events, int id) {
	Event *result = 0;
	for(int entIndex = 0; entIndex < events->count; entIndex++) {
	    Event *event = (Event *)getElement(events, entIndex);
	    if(event) {
	    	if(event->ID == id) {
	    		result = event;
	    		break;
	    	}
	    }
	}
	return result;
}

bool isEventFlagSet(Event *event, EventFlag flag) {
	bool result = event->flags & flag;
	return result;
}

void setEventFlag(Event *event, EventFlag flag) {
	event->flags |= flag;
}

void unSetEventFlag(Event *event, EventFlag flag) {
	event->flags &= (~flag);
}

void setupEmptyEvent(Event *event) {
	setEventFlag(event, EVENT_FRESH);
}

typedef struct {
	int id;
	V3 *pos;
} Event_EntCommonsInfo;

Event *addDialogEvent(Array_Dynamic *events, Event_EntCommonsInfo *commonInfo, V3 bounds, unsigned long flags, int id) {
	Event *event = (Event *)getEmptyElement(events);
	memset(event, 0, sizeof(Event));
	event->ID = id;
	event->type = EVENT_DIALOG;
	event->entId = commonInfo->id;
	event->pos = commonInfo->pos;
	event->dim = bounds;
	event->dialogTimer = initTimer(2);
	event->dialogDisplayValue = 0.3f;
	event->flags |= flags;
	setEventFlag(event, EVENT_FRESH);
	return event;
}

void renewPanEventV3(Event *event) {
	assert(event->type == EVENT_V3_PAN);
	setLerpInfoV3_s(&event->lerpValueV3, event->lerpValueV3.b, event->lerpValueV3.timer.period, event->lerpValueV3.val);
}

Event *addV3PanEvent(Array_Dynamic *events, LerpType lerpType, EventFlag flags, V3 targetPos, float timeSpan, Event_EntCommonsInfo *ent, int id) {
	Event *event = (Event *)getEmptyElement(events);
	memset(event, 0, sizeof(Event));
	event->ID = id;
	event->entId = ent->id;
	event->type = EVENT_V3_PAN;
	event->lerpType = lerpType;
	event->flags |= flags;
	setEventFlag(event, EVENT_FRESH);
	setLerpInfoV3_s(&event->lerpValueV3, targetPos, timeSpan, ent->pos);
	return event;
}

Event *addV3PanEventWithOffset(Array_Dynamic *events, LerpType lerpType, EventFlag flags, V3 offsetPos, float timeSpan, Event_EntCommonsInfo *ent, int id) { 
	Event *result = addV3PanEvent(events, lerpType, flags, v3_plus(*ent->pos, offsetPos), timeSpan, ent, id);
	return result;
}

