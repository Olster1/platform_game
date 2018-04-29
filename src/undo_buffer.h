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

void beginUndoMove(Array_Dynamic *array, Entity_Commons *commons, V3 startPos) {
	printf("added begin undo move\n");
	removeSectionOfElements(array, REMOVE_ORDERED, array->indexAt, array->count);
	assert(array->indexAt <= array->count);
	UndoInfo *info = (UndoInfo *)getEmptyElement(array);
	info->type = MOVE_ENTITY;
	info->startPos = startPos;
	info->commons = commons;
	array->indexAt++;
}

void endUndoMove(Array_Dynamic *array, V3 pos) {
	printf("added end undo move\n");
	UndoInfo *info = (UndoInfo *)getLastElement(array);
	assert(info->type == MOVE_ENTITY);
	assert(info->commons);
	info->endPos = pos;
}

void addToUndoBufferIndex(Array_Dynamic *array, ActionType type, Entity_Commons *commons) {
	printf("added entity to undo buffer\n");
	removeSectionOfElements(array, REMOVE_ORDERED, array->indexAt, array->count);	
	assert(array->indexAt <= array->count);
	UndoInfo *info = (UndoInfo *)getEmptyElement(array);
	info->type = type;
	info->commons = commons;
	array->indexAt++;
}

void processUndoRedoElm(GameState *gameState, UndoInfo *info, UndoType type) {
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
				printf("hey\n");
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