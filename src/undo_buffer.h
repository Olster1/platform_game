typedef enum {
	DELETE_ENTITY, 
	CREATE_ENTITY,
	MOVE_ENTITY, 
} ActionType;

typedef enum {
	REDO, 
	UNDO
} UndoType;

typedef struct {
	ActionType type;

	Entity_Commons *commons;
	union {
		struct {
			V3 startPos;
			V3 endPos;
		};
		struct {
		};
	};
} UndoInfo;

typedef struct {
	UndoInfo infos[4096];
	int indexAt;
	int origin;
	int endAt;
	
	bool wrapped;
} UndoState;

UndoInfo *getUndoElement(UndoState *state) {
	assert(state->indexAt < arrayCount(state->infos));
	UndoInfo *info = state->infos + state->indexAt; 
	return info;
}

void addNewUndoInfoIncrement(UndoState *state) {

	state->indexAt++;

	if(state->indexAt >= arrayCount(state->infos)) {
		state->wrapped = true;
		state->indexAt = 0;		
	}

	if(state->wrapped) {
		if(state->indexAt == state->origin) {
			state->origin = state->indexAt + 1;
			if(state->origin >= arrayCount(state->infos)) {
				state->origin = 0;
			}
			assert(state->origin > state->indexAt || state->indexAt == (arrayCount(state->infos) - 1));
		}
	}

	state->endAt = state->indexAt;
	assert(state->endAt >= 0 && state->endAt < arrayCount(state->infos));
}

void undoStateDecrementIndex(UndoState *state) {
	if(state->indexAt != state->origin) {
		state->indexAt--;

		if(state->indexAt < 0) {
			if(state->wrapped) {
				state->indexAt = arrayCount(state->infos) - 1;
				assert(state->indexAt >= state->origin);
			} else {
				state->indexAt = 0;		
				assert(state->origin == 0);
			}
		}
	}
}

void beginUndoMove(UndoState *state, Entity_Commons *commons, V3 startPos) {
	printf("added begin undo move\n");
	UndoInfo *info = getUndoElement(state);
	info->type = MOVE_ENTITY;
	info->startPos = startPos;
	info->commons = commons;
}

void endUndoMove(UndoState *state, V3 pos) {
	printf("added end undo move\n");
	UndoInfo *info = getUndoElement(state);
	assert(info->type == MOVE_ENTITY);
	assert(info->commons);
	info->endPos = pos;
	addNewUndoInfoIncrement(state);
}


//function for adding generic types to the undo buffer
void addToUndoBuffer(UndoState *state, ActionType type, Entity_Commons *commons) {
	printf("added entity to undo buffer\n");
	UndoInfo *info = getUndoElement(state);
	info->type = type;
	info->commons = commons;
	addNewUndoInfoIncrement(state);
}

void processUndoRedoElm(UndoInfo *info, UndoType type) {
	switch(info->type) {
		case DELETE_ENTITY: {
			printf("%s\n", "Delte Entity");
			if(type == REDO) {
				unSetFlag(info->commons, ENTITY_VALID);
			} else if(type == UNDO) {
				setFlag(info->commons, ENTITY_VALID);
			}
		} break;
		case MOVE_ENTITY: {
			printf("%s\n", "Move Entity");
			if(type == REDO) {
				info->commons->pos = info->endPos;
			} else if(type == UNDO) {
				info->commons->pos = info->startPos;
			}
		} break;
		case CREATE_ENTITY: {
			printf("%s\n", "Create Entity");
			if(type == REDO) {
				setFlag(info->commons, ENTITY_VALID);
			} else if(type == UNDO) {
				unSetFlag(info->commons, ENTITY_VALID);
			}
		} break;

	}

}

void redoForUndoState(UndoState *state) {
	// assert(state->indexAt <= state->count);
	
	if(state->indexAt != state->endAt) {
	    UndoInfo *info = getUndoElement(state);
	    assert(info);
	    processUndoRedoElm(info, REDO);

	    state->indexAt++;

    	if(state->indexAt >= arrayCount(state->infos)) {
    		state->indexAt = 0;
    		assert(state->indexAt < state->origin);
	    }
	    assert(state->indexAt != state->origin);

	} else {
	    // printf("Buffer count: %d\n", gameState->undoBuffer.count);
	    // printf("Buffer at: %d\n", gameState->undoBuffer.indexAt);
	    printf("Nothing to redo in the buffer\n");
	}
}

void undoForUndoState(UndoState *state) {
	if(state->indexAt != state->origin) {
	    undoStateDecrementIndex(state);
	    UndoInfo *info = getUndoElement(state);
	    assert(info);
	    processUndoRedoElm(info, UNDO);
	} else {
	    printf("Nothing to undo in the buffer\n");
	}
}