typedef enum {
	EVENT_DIALOG, 
	EVENT_V3_PAN
} EventType;

typedef enum {
	EVENT_NULL_FLAG = 0,
	EVENT_EXPLICIT = 1 << 0,
	EVENT_TRIGGER = 1 << 1,
	EVENT_FRESH = 1 << 2, //This is for reseting values when they are pushed as the current event; 
} EventFlag;

typedef struct Event Event;
typedef struct Event {
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

Event *addDialogEvent(Array_Dynamic *events, V3 *pos, V3 bounds, unsigned long flags) {
	Event *event = (Event *)getEmptyElement(events);
	memset(event, 0, sizeof(Event));
	event->type = EVENT_DIALOG;
	event->pos = pos;
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

Event *addV3PanEvent(Array_Dynamic *events, LerpType lerpType, EventFlag flags, V3 *value, V3 targetPos, float timeSpan) {
	Event *event = (Event *)getEmptyElement(events);
	memset(event, 0, sizeof(Event));
	event->type = EVENT_V3_PAN;
	event->lerpType = lerpType;
	event->flags |= flags;
	setEventFlag(event, EVENT_FRESH);
	setLerpInfoV3_s(&event->lerpValueV3, targetPos, timeSpan, value);
	return event;
}

Event *addV3PanEventWithOffset(Array_Dynamic *events, LerpType lerpType, EventFlag flags, V3 *value, V3 offsetPos, float timeSpan) { 
	Event *result = addV3PanEvent(events, lerpType, flags, value, v3_plus(*value, offsetPos), timeSpan);
	return result;
}

