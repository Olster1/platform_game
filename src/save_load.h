#define clearAndInitArray(arrayPtr, type, clear) if(clear) { freeArray(arrayPtr); } initArray(arrayPtr, type);

void initWorldDataArrays(GameState *gameState, bool clearArrays) {
    clearAndInitArray(&gameState->commons, Entity_Commons, clearArrays);
    clearAndInitArray(&gameState->entities, Entity, clearArrays);
    clearAndInitArray(&gameState->collisionEnts, Collision_Object, clearArrays);
    clearAndInitArray(&gameState->doorEnts, Door, clearArrays);
    clearAndInitArray(&gameState->undoBuffer, UndoInfo, clearArrays);
    clearAndInitArray(&gameState->platformEnts, Entity, clearArrays);
    clearAndInitArray(&gameState->noteEnts, Note, clearArrays);
    clearAndInitArray(&gameState->noteParentEnts, NoteParent, clearArrays);
    clearAndInitArray(&gameState->particleSystems, particle_system, clearArrays);
    clearAndInitArray(&gameState->npcEntities, NPC, clearArrays);
    clearAndInitArray(&gameState->events, Event, clearArrays);
    clearAndInitArray(&gameState->lights, Light, clearArrays);
    clearAndInitArray(&gameState->renderCircles, RenderCircle, clearArrays);
    
}

typedef struct {
    InfiniteAlloc mem;
    game_file_handle fileHandle;
} EntFileData;

void beginDataType(InfiniteAlloc *mem, char *name) {
    char data[16];
    sprintf(data, "%s: {\n", name);
    addElementInifinteAllocWithCount_(mem, data, strlen(data));
}

void endDataType(InfiniteAlloc *mem) {
    char data[16];
    sprintf(data, "\n}\n");
    addElementInifinteAllocWithCount_(mem, data, strlen(data));
}


void outputCommonData(InfiniteAlloc *mem, Entity_Commons *ent) {
    char *typeString = EntityTypeStrings[ent->type]; //this is from a global

    char data[1028];
    beginDataType(mem, "Commons");

    addVar(mem, &ent->ID, "id", VAR_INT);
    addVar(mem, ent->name, "name", VAR_CHAR_STAR);

    addVar(mem, &ent->flags, "flags", VAR_LONG_INT);
    addVar(mem, typeString, "type", VAR_CHAR_STAR);

    addVar(mem, ent->pos.E, "position", VAR_V3);
    addVar(mem, ent->dP.E, "velocity", VAR_V3);

    addVar(mem, ent->renderPosOffset.E, "renderPosOffset", VAR_V3);
    addVar(mem, ent->dim.E, "dim", VAR_V3);
    addVar(mem, ent->renderScale.E, "renderScale", VAR_V3);
    addVar(mem, ent->shading.E, "shading", VAR_V4);

    if(ent->tex) {
        addVar(mem, ent->tex->name, "texture", VAR_CHAR_STAR);
    }
    addVar(mem, &ent->inverseWeight, "inverseWeight", VAR_FLOAT);
    addVar(mem, &ent->angle, "angle", VAR_FLOAT);

    if(ent->animationParent) {
        addVar(mem, ent->animationParent->name, "animation", VAR_CHAR_STAR);
    }
    endDataType(mem);
}

char *getEntName(char *prepend, int ID) {
    char name[256];
    sprintf(name, "_%d_", ID);
    char *entFileName = concat(prepend, name);
    return entFileName;
}

EntFileData beginEntFileData(char *dirName, char *fileName_, char *entType, int id) {
    EntFileData result = {};

    char *entName = getEntName(entType, id);
    char *a = concat(dirName, entName);
    char *entFileName = concat(a, fileName_);

    result.mem = initInfinteAlloc(char);
    result.fileHandle = platformBeginFileWrite(entFileName);
    assert(!result.fileHandle.HasErrors);
    free(entName);
    free(entFileName);
    free(a);
    return result;
}

void endEntFileData(EntFileData *data) {
    platformWriteFile(&data->fileHandle, data->mem.memory, data->mem.count*data->mem.sizeOfMember, 0);
    platformEndFile(data->fileHandle);
    free(data->mem.memory);
}

void saveWorld(GameState *gameState, char *dir, char *fileName) {
    
    // initArray(&gameState->particleSystems, particle_system);
    for(int entIndex = 0; entIndex < gameState->npcEntities.count; entIndex++) {
        NPC *ent = (NPC *)getElement(&gameState->npcEntities, entIndex);
        if(ent && isFlagSet(ent->e, ENTITY_VALID)) {
            EntFileData fileData = beginEntFileData(dir, fileName, "NPC", ent->e->ID);
            beginDataType(&fileData.mem, "NPC");
            if(ent->event) {
                addVar(&fileData.mem, &ent->event->ID, "eventID", VAR_INT);
            }
            endDataType(&fileData.mem);

            outputCommonData(&fileData.mem, ent->e);
            endEntFileData(&fileData);
        }
    }

    for(int entIndex = 0; entIndex < gameState->lights.count; entIndex++) {
        Light *ent = (Light *)getElement(&gameState->lights, entIndex);
        if(ent && isFlagSet(ent->e, ENTITY_VALID)) {
            EntFileData fileData = beginEntFileData(dir, fileName, "Light", ent->e->ID);
            beginDataType(&fileData.mem, "Light");
            
            addVar(&fileData.mem, &ent->flux, "flux", VAR_FLOAT);
            endDataType(&fileData.mem);

            outputCommonData(&fileData.mem, ent->e);
            endEntFileData(&fileData);
        }
    }

    for(int entIndex = 0; entIndex < gameState->noteParentEnts.count; entIndex++) {
        NoteParent *ent = (NoteParent *)getElement(&gameState->noteParentEnts, entIndex);
        if(ent && isFlagSet(ent->e, ENTITY_VALID)) {
            EntFileData fileData = beginEntFileData(dir, fileName, "noteParent", ent->e->ID);

            beginDataType(&fileData.mem, "NoteParent");
            addVar(&fileData.mem, &ent->solved, "solved", VAR_BOOL);
            addVar(&fileData.mem, &ent->showChildren, "showChildren", VAR_BOOL);
            addVar(&fileData.mem, &ent->noteValueCount, "noteValueCount", VAR_INT);
            addVar(&fileData.mem, NoteParentTypeStrings[ent->type], "noteParentType", VAR_CHAR_STAR);
            
            if(ent->noteValueCount > 0) {
                int noteIds[32] = {};
                for(int i = 0; i < ent->noteValueCount; ++i) {
                    noteIds[i] = ent->sequence[i]->e->ID;
                }
                addVarArray(&fileData.mem, noteIds, ent->noteValueCount, "noteSequenceIds", VAR_INT);
            }
            if(ent->eventToTrigger) {
                addVar(&fileData.mem, &ent->eventToTrigger->ID, "eventID", VAR_INT);
            }

            endDataType(&fileData.mem);

            outputCommonData(&fileData.mem, ent->e);

            endEntFileData(&fileData);
        }
    }

    

    for(int entIndex = 0; entIndex < gameState->noteEnts.count; entIndex++) {
        Note *ent = (Note *)getElement(&gameState->noteEnts, entIndex);
        if(ent && isFlagSet(ent->e, ENTITY_VALID)) {
            EntFileData fileData = beginEntFileData(dir, fileName, "note", ent->e->ID);

            beginDataType(&fileData.mem, "Note");
            if(ent->sound) {
                addVar(&fileData.mem, ent->sound->name, "sound", VAR_CHAR_STAR);
            }
            addVar(&fileData.mem, NoteValueStrings[ent->value], "noteValue", VAR_CHAR_STAR);
            addVar(&fileData.mem, &ent->parent->e->ID, "parentID", VAR_INT);
            addVar(&fileData.mem, &ent->isPlayedByParent, "isPlayedByParent", VAR_BOOL);
            
            endDataType(&fileData.mem);

            outputCommonData(&fileData.mem, ent->e);

            endEntFileData(&fileData);
        }
    }

    for(int entIndex = 0; entIndex < gameState->entities.count; entIndex++) {
        Entity *ent = (Entity *)getElement(&gameState->entities, entIndex);
        if(ent && isFlagSet(ent->e, ENTITY_VALID)) {
            EntFileData fileData = beginEntFileData(dir, fileName, "entity", ent->e->ID);

            beginDataType(&fileData.mem, "Entity");
            if(ent->event) {
                addVar(&fileData.mem, &ent->event->ID, "eventID", VAR_INT);
            }
            endDataType(&fileData.mem);

            outputCommonData(&fileData.mem, ent->e);

            endEntFileData(&fileData);
        }
    }

    for(int entIndex = 0; entIndex < gameState->platformEnts.count; entIndex++) {
        Entity *ent = (Entity *)getElement(&gameState->platformEnts, entIndex);
        if(ent && isFlagSet(ent->e, ENTITY_VALID)) {
            EntFileData fileData = beginEntFileData(dir, fileName, "platform", ent->e->ID);

            beginDataType(&fileData.mem, "Platform");
            addVar(&fileData.mem, ent->centerPoint.E, "centerPoint", VAR_V3);
            addVar(&fileData.mem, PlatformTypeStrings[ent->platformType], "platformType", VAR_CHAR_STAR);
            if(ent->event) { //I don't think platforms have events. 
                addVar(&fileData.mem, &ent->event->ID, "eventID", VAR_INT);
            }
            endDataType(&fileData.mem);

            outputCommonData(&fileData.mem, ent->e);

            endEntFileData(&fileData);
        }
    }

    for(int entIndex = 0; entIndex < gameState->collisionEnts.count; entIndex++) {
        Collision_Object *ent = (Collision_Object *)getElement(&gameState->collisionEnts, entIndex);
        if(ent && isFlagSet(ent->e, ENTITY_VALID)) {
            EntFileData fileData = beginEntFileData(dir, fileName, "collision", ent->e->ID);
            beginDataType(&fileData.mem, "Collision");
            endDataType(&fileData.mem);

            outputCommonData(&fileData.mem, ent->e);
            endEntFileData(&fileData);
        }
    }

    for(int entIndex = 0; entIndex < gameState->doorEnts.count; entIndex++) {
        Door *ent = (Door *)getElement(&gameState->doorEnts, entIndex);
        if(ent && isFlagSet(ent->e, ENTITY_VALID)) {
            EntFileData fileData = beginEntFileData(dir, fileName, "door", ent->e->ID);
            beginDataType(&fileData.mem, "Door");
            addVar(&fileData.mem, &ent->partner->e->ID, "partnerID", VAR_INT);
            endDataType(&fileData.mem);

            outputCommonData(&fileData.mem, ent->e);
            endEntFileData(&fileData);
            
        }
    }

    for(int entIndex = 0; entIndex < gameState->events.count; entIndex++) {
        Event *ent = (Event *)getElement(&gameState->events, entIndex);
        if(ent) {
            EntFileData fileData = beginEntFileData(dir, fileName, "event", ent->ID);
            beginDataType(&fileData.mem, "Event");
            addVar(&fileData.mem, &ent->ID, "ID", VAR_INT);
            addVar(&fileData.mem, EventTypeStrings[ent->type], "type", VAR_CHAR_STAR);
            addVar(&fileData.mem, ent->name, "name", VAR_CHAR_STAR);
            addVar(&fileData.mem, &ent->entId, "entId", VAR_INT);
            addVar(&fileData.mem, ent->dim.E, "dim", VAR_V3);
            addVar(&fileData.mem, &ent->flags, "flags", VAR_LONG_INT);
            if(ent->sound) {
                addVar(&fileData.mem, ent->sound->name, "sound", VAR_CHAR_STAR);
            }
            if(ent->nextEvent) {
                addVar(&fileData.mem, &ent->nextEvent->ID, "nextEventID", VAR_INT);
            }
            
            if(ent->type == EVENT_DIALOG) {
                addVar(&fileData.mem, &ent->dialogCount, "dialogCount", VAR_INT);
                for(int i = 0; i < ent->dialogCount; ++i) {
                    printf("%s\n", ent->dialog[i]);
                }
                void *dialogArray = ent->dialog;
                if(ent->dialogCount == 1) { dialogArray = ent->dialog[0]; } 
                addVarArray(&fileData.mem, dialogArray, ent->dialogCount, "dialog", VAR_CHAR_STAR);
                addVar(&fileData.mem, &ent->dialogTimer.period, "dialogTimerPeriod", VAR_FLOAT);
                addVar(&fileData.mem, &ent->dialogDisplayValue, "dialogDisplayValue", VAR_FLOAT);
            } else if(ent->type == EVENT_V3_PAN) {
                addVar(&fileData.mem, LerpTypeStrings[ent->lerpType], "lerpType", VAR_CHAR_STAR);
                addVar(&fileData.mem, ent->lerpValueV3.b.E, "lerpB", VAR_V3);
                addVar(&fileData.mem, &ent->entId, "lerpEntId", VAR_INT);
                addVar(&fileData.mem, &ent->lerpValueV3.timer.period, "lerpTimerPeriod", VAR_FLOAT);
            } else if(ent->type == EVENT_FADE_OUT) {
                addVar(&fileData.mem, &ent->fadeTimer.period, "fadeTimerPeriod", VAR_FLOAT);
            } else if(ent->type == EVENT_LOAD_LEVEL) {
                addVar(&fileData.mem, &ent->levelName, "levelToLoad", VAR_CHAR_STAR);
            }

            endDataType(&fileData.mem);

            endEntFileData(&fileData);
        }
    }

    {
        assert(gameState->camera);
        EntFileData fileData = beginEntFileData(dir, fileName, "camera", gameState->camera->ID);

        beginDataType(&fileData.mem, "Camera");
        endDataType(&fileData.mem);

        outputCommonData(&fileData.mem, gameState->camera);
        endEntFileData(&fileData);
    }
} 

typedef struct {
    EntType type;

    union {
        struct {
            Note *note;
        };
        struct {
            NoteParent *noteParent;
        };
        struct {
            Door *door;
        };
        struct {    
            Entity *platform;
        };
        struct {    
            Entity *entity;
        };
        struct {    
            Collision_Object *collision;
        };
        struct {
            NPC *npc;
        };
        struct {
            Light *light;
        };
        struct {
            Event *event;
        };
    };

    Entity_Commons *commons;
} EntityData;


typedef enum {
    PATCH_TYPE_ENTITY_POS,
    PATCH_TYPE_ENTITY,
    PATCH_TYPE_COMMON,
    PATCH_TYPE_EVENT
} PatchType;

typedef struct {
    PatchType type;

    int id; //find entity with this 
    void **ptr; //patch this ptr

} PointerToPatch;



void loadWorld(GameState *gameState, char *dir) {
    char *fileTypes[] = {"txt"};
    FileNameOfType fileNames = getDirectoryFilesOfType(dir, fileTypes, arrayCount(fileTypes));

    int patchCount = 0; 
    PointerToPatch patches[256] = {}; 

    //Init data here. 
    initWorldDataArrays(gameState, true);

    int maxId = 0;
    int maxEventId = 0;

    for(int fileIndex = 0; fileIndex < fileNames.count; ++fileIndex) {
        char *name = fileNames.names[fileIndex];

        FileContents contents = getFileContentsNullTerminate(name);

        assert(contents.memory);
        assert(contents.valid);

        EasyTokenizer tokenizer = lexBeginParsing((char *)contents.memory, true);

        EntityData entData = {};
        entData.commons = (Entity_Commons *)getEmptyElement(&gameState->commons);
        EntType tempType = ENTITY_TYPE_INVALID; //this is to use to see if it is a common type

        bool parsing = true;
        while(parsing) {
            EasyToken token = lexGetNextToken(&tokenizer);
            //lexPrintToken(&token);
            InfiniteAlloc data = {};
            switch(token.type) {
                case TOKEN_NULL_TERMINATOR: {
                    parsing = false;
                } break;
                case TOKEN_WORD: {
                    if(stringsMatchNullN("Platform", token.at, token.size)) {
                        entData.type = ENTITY_TYPE_PLATFORM;
                        entData.platform = (Entity *)getEmptyElement(&gameState->platformEnts);
                        setupPlatformEnt(entData.platform, entData.commons, &gameState->particleSystems);
                    }
                    if(stringsMatchNullN("Commons", token.at, token.size)) {
                        tempType = ENTITY_TYPE_COMMONS;
                    } 
                    if(stringsMatchNullN("Door", token.at, token.size)) {
                        entData.type = ENTITY_TYPE_DOOR;
                        entData.door = (Door *)getEmptyElement(&gameState->doorEnts);
                        setupDoorEnt(entData.door, entData.commons, &gameState->particleSystems);
                    }
                    if(stringsMatchNullN("Collision", token.at, token.size)) {
                        entData.type = ENTITY_TYPE_COLLISION;
                        entData.collision = (Collision_Object *)getEmptyElement(&gameState->collisionEnts);
                        setupCollisionEnt(entData.collision, entData.commons, &gameState->particleSystems);
                    }
                    if(stringsMatchNullN("Camera", token.at, token.size)) {
                        entData.type = ENTITY_TYPE_CAMERA;
                        gameState->camera = entData.commons;
                    }
                    if(stringsMatchNullN("Entity", token.at, token.size)) {
                        entData.type = ENTITY_TYPE_ENTITY;
                        entData.entity = (Entity *)getEmptyElement(&gameState->entities);
                        setupEnt(entData.entity, entData.commons, &gameState->particleSystems);
                    }
                    if(stringsMatchNullN("Note", token.at, token.size)) {
                        entData.type = ENTITY_TYPE_NOTE;
                        entData.note = (Note *)getEmptyElement(&gameState->noteEnts);
                        setupNoteEnt(entData.note, entData.commons, &gameState->particleSystems);
                    }
                    if(stringsMatchNullN("NoteParent", token.at, token.size)) {
                        entData.type = ENTITY_TYPE_NOTE_PARENT;
                        entData.noteParent = (NoteParent *)getEmptyElement(&gameState->noteParentEnts);
                        setupNoteParent(entData.noteParent, entData.commons, &gameState->particleSystems);
                    }
                    if(stringsMatchNullN("NPC", token.at, token.size)) {
                        entData.type = ENTITY_TYPE_NPC;
                        entData.npc = (NPC *)getEmptyElement(&gameState->npcEntities);
                        setupNPCEnt(entData.npc, entData.commons, &gameState->particleSystems);
                    }
                    if(stringsMatchNullN("Light", token.at, token.size)) {
                        entData.type = ENTITY_TYPE_LIGHT;
                        entData.light = (Light *)getEmptyElement(&gameState->lights);
                        setupLight(entData.light, entData.commons, &gameState->particleSystems);
                    }
                    if(stringsMatchNullN("Event", token.at, token.size)) {
                        entData.type = ENTITY_TYPE_EVENT;
                        entData.event = (Event *)getEmptyElement(&gameState->events);
                        setupEmptyEvent(entData.event);
                    }

                    if(entData.type == ENTITY_TYPE_EVENT) {
                        if(stringsMatchNullN("ID", token.at, token.size)) {
                            entData.event->ID = getIntFromDataObjects(&data, &tokenizer);
                            maxEventId = max(entData.event->ID, maxEventId);
                        }
                        if(stringsMatchNullN("type", token.at, token.size)) {
                            char *name = getStringFromDataObjects(&data, &tokenizer);
                            int typeAsInt = findEnumValue(name, EventTypeStrings, arrayCount(EventTypeStrings));
                            assert(typeAsInt >= 0);
                            entData.event->type = (EventType)typeAsInt;
                        }
                        if(stringsMatchNullN("name", token.at, token.size)) {
                            char *string = getStringFromDataObjects(&data, &tokenizer);
                            nullTerminateBuffer(entData.event->name, string, strlen(string));
                        }
                        if(stringsMatchNullN("dim", token.at, token.size)) {
                            entData.event->dim = buildV3FromDataObjects(&data, &tokenizer);
                        }
                        if(stringsMatchNullN("flags", token.at, token.size)) {
                            entData.event->flags = getIntFromDataObjects(&data, &tokenizer);
                        }
                        if(stringsMatchNullN("sound", token.at, token.size)) {
                            char *name = getStringFromDataObjects(&data, &tokenizer);
                            entData.event->sound = findAsset(name);
                            assert(entData.event->sound);
                        }
                        if(stringsMatchNullN("levelToLoad", token.at, token.size)) {
                            char *string = getStringFromDataObjects(&data, &tokenizer);
                            nullTerminateBuffer(entData.event->levelName, string, strlen(string));
                        }
                        if(stringsMatchNullN("nextEventID", token.at, token.size)) {
                            int nextEventID = getIntFromDataObjects(&data, &tokenizer);
                            
                            assert(patchCount < arrayCount(patches));
                            PointerToPatch *patch = patches + patchCount++;
                            patch->type = PATCH_TYPE_EVENT;
                            patch->id = nextEventID;
                            patch->ptr = (void **)(&entData.event->nextEvent);
                            
                        }
                        if(stringsMatchNullN("entId", token.at, token.size)) {
                            int entId = getIntFromDataObjects(&data, &tokenizer);
                            entData.event->entId = entId;

                            assert(patchCount < arrayCount(patches));
                            PointerToPatch *patch = patches + patchCount++;
                            patch->type = PATCH_TYPE_ENTITY_POS;
                            patch->id = entId;
                            patch->ptr = (void **)(&entData.event->pos);
                        }
                        if(stringsMatchNullN("dialogCount", token.at, token.size)) {
                            entData.event->dialogCount = getIntFromDataObjects(&data, &tokenizer);
                        }
                        if(stringsMatchNullN("dialog", token.at, token.size)) {
                            data = getDataObjects(&tokenizer);
                            DataObject *objs = (DataObject *)data.memory;
                            
                            assert(entData.event->dialogCount == data.count || entData.event->dialogCount == 0);
                            
                            for(int dialogIndex = 0; dialogIndex < data.count; dialogIndex++) {
                                assert(objs[dialogIndex].type == VAR_CHAR_STAR);    
                                //TODO: Do we want to allocate this to an arena??
                                char *tempStr = objs[dialogIndex].stringVal;
                                char *dialogString = nullTerminate(tempStr, strlen(tempStr));
                                entData.event->dialog[dialogIndex] = dialogString;
                            }
                        }
                        if(stringsMatchNullN("dialogTimerPeriod", token.at, token.size)) {
                            entData.event->dialogTimer.period = getFloatFromDataObjects(&data, &tokenizer);
                        }
                        if(stringsMatchNullN("dialogDisplayValue", token.at, token.size)) {
                            entData.event->dialogDisplayValue = getFloatFromDataObjects(&data, &tokenizer);
                        }
                        if(stringsMatchNullN("fadeTimerPeriod", token.at, token.size)) {
                            entData.event->fadeTimer.period = getFloatFromDataObjects(&data, &tokenizer);
                        }
                        if(stringsMatchNullN("lerpType", token.at, token.size)) {
                            char *name = getStringFromDataObjects(&data, &tokenizer);
                            int type = findEnumValue(name, LerpTypeStrings, arrayCount(LerpTypeStrings));
                            entData.event->lerpType = (LerpType)type;
                        }
                        if(stringsMatchNullN("lerpB", token.at, token.size)) {
                            entData.event->lerpValueV3.b = buildV3FromDataObjects(&data, &tokenizer);
                        }
                        if(stringsMatchNullN("lerpEntId", token.at, token.size)) {
                            int entId = getIntFromDataObjects(&data, &tokenizer);

                            assert(patchCount < arrayCount(patches));
                            PointerToPatch *patch = patches + patchCount++;
                            patch->type = PATCH_TYPE_ENTITY_POS;
                            patch->id = entId;
                            patch->ptr = (void **)(&entData.event->lerpValueV3.val);
                        }
                        if(stringsMatchNullN("lerpTimerPeriod", token.at, token.size)) {
                            entData.event->lerpValueV3.timer.period = getFloatFromDataObjects(&data, &tokenizer);
                        }
                    }

                    if(entData.type == ENTITY_TYPE_NPC) {
                        if(stringsMatchNullN("eventID", token.at, token.size)) {
                            int entId = getIntFromDataObjects(&data, &tokenizer);

                            assert(patchCount < arrayCount(patches));
                            PointerToPatch *patch = patches + patchCount++;
                            patch->type = PATCH_TYPE_EVENT;
                            patch->id = entId;
                            patch->ptr = (void **)(&entData.npc->event);
                        }
                    }

                    if(entData.type == ENTITY_TYPE_LIGHT) {
                        if(stringsMatchNullN("flux", token.at, token.size)) {
                            float flux = getFloatFromDataObjects(&data, &tokenizer);
                            entData.light->flux = flux;
                        }
                    }
                    //platform data
                    if(entData.type == ENTITY_TYPE_PLATFORM) {
                        if(stringsMatchNullN("centerPoint", token.at, token.size)) {
                            entData.platform->centerPoint = buildV3FromDataObjects(&data, &tokenizer);
                        }
                        if(stringsMatchNullN("platformType", token.at, token.size)) {
                            char *name = getStringFromDataObjects(&data, &tokenizer);
                            int typeAsInt = findEnumValue(name, PlatformTypeStrings, arrayCount(PlatformTypeStrings));
                            assert(typeAsInt >= 0);
                            entData.platform->platformType = (PlatformType)typeAsInt;
                        }
                        if(stringsMatchNullN("eventID", token.at, token.size)) {
                            int entId = getIntFromDataObjects(&data, &tokenizer);

                            assert(patchCount < arrayCount(patches));
                            PointerToPatch *patch = patches + patchCount++;
                            patch->type = PATCH_TYPE_EVENT;
                            patch->id = entId;
                            patch->ptr = (void **)(&entData.platform->event);
                        }
                    }
                    /// 
                    //Note data
                    if(entData.type == ENTITY_TYPE_NOTE) {
                        if(stringsMatchNullN("parentID", token.at, token.size)) {
                            int parentID = getIntFromDataObjects(&data, &tokenizer);
                            
                            assert(patchCount < arrayCount(patches));
                            PointerToPatch *patch = patches + patchCount++;
                            patch->type = PATCH_TYPE_ENTITY;
                            patch->id = parentID;
                            patch->ptr = (void **)(&entData.note->parent);
                            //
                        }
                        if(stringsMatchNullN("noteValue", token.at, token.size)) {
                            char *name = getStringFromDataObjects(&data, &tokenizer);
                            int typeAsInt = findEnumValue(name, NoteValueStrings, arrayCount(NoteValueStrings));
                            assert(typeAsInt >= 0);
                            entData.note->value = (NoteValue)typeAsInt;
                        }
                        if(stringsMatchNullN("isPlayedByParent", token.at, token.size)) {
                            entData.note->isPlayedByParent = getBoolFromDataObjects(&data, &tokenizer);
                        }
                        
                        if(stringsMatchNullN("sound", token.at, token.size)) {
                            char *name = getStringFromDataObjects(&data, &tokenizer);
                            entData.note->sound = findAsset(name);
                            assert(entData.note->sound);
                        }
                    }
                    //
                    //Note parent data
                    if(entData.type == ENTITY_TYPE_NOTE_PARENT) {
                        if(stringsMatchNullN("noteValueCount", token.at, token.size)) {
                            entData.noteParent->noteValueCount = getIntFromDataObjects(&data, &tokenizer);
                        }
                        if(stringsMatchNullN("noteParentType", token.at, token.size)) {
                            char *name = getStringFromDataObjects(&data, &tokenizer);
                            int typeAsInt = findEnumValue(name, NoteParentTypeStrings, arrayCount(NoteParentTypeStrings));
                            assert(typeAsInt >= 0);
                            entData.noteParent->type = NoteParentTypeValues[typeAsInt];
                        }
                        if(stringsMatchNullN("solved", token.at, token.size)) {
                            entData.noteParent->solved = getBoolFromDataObjects(&data, &tokenizer);
                        }
                        if(stringsMatchNullN("showChildren", token.at, token.size)) {
                            entData.noteParent->showChildren = getBoolFromDataObjects(&data, &tokenizer);
                        }
                        //add all notes for the sequence
                        if(stringsMatchNullN("noteSequenceIds", token.at, token.size)) {
                            data = getDataObjects(&tokenizer);
                            DataObject *objs = (DataObject *)data.memory;
                            assert(entData.noteParent->noteValueCount == data.count || entData.noteParent->noteValueCount == 0);
                            for(int noteIndex = 0; noteIndex < data.count; noteIndex++) {
                                assert(objs[noteIndex].type == VAR_INT);    
                                int noteId = objs[noteIndex].intVal;

                                assert(patchCount < arrayCount(patches));
                                PointerToPatch *patch = patches + patchCount++;
                                patch->type = PATCH_TYPE_ENTITY;
                                patch->id = noteId;
                                patch->ptr = (void **)(&entData.noteParent->sequence[noteIndex]);
                            }
                        }
                        if(stringsMatchNullN("eventID", token.at, token.size)) {
                            int entId = getIntFromDataObjects(&data, &tokenizer);

                            assert(patchCount < arrayCount(patches));
                            PointerToPatch *patch = patches + patchCount++;
                            patch->type = PATCH_TYPE_EVENT;
                            patch->id = entId;
                            patch->ptr = (void **)(&entData.noteParent->eventToTrigger);
                        }
                    }
                    //
                    //Door data
                    if(entData.type == ENTITY_TYPE_DOOR) {
                        if(stringsMatchNullN("partnerID", token.at, token.size))  {
                            assert(patchCount < arrayCount(patches));
                            PointerToPatch *patch = patches + patchCount++;
                            patch->type = PATCH_TYPE_ENTITY;
                            patch->id = getIntFromDataObjects(&data, &tokenizer);
                            patch->ptr = (void **)(&entData.door->partner);
                            assert(patch->ptr);
                        }
                    }
                    //parse entity 
                    if(entData.type == ENTITY_TYPE_ENTITY) {
                        if(stringsMatchNullN("eventID", token.at, token.size)) {
                            int entId = getIntFromDataObjects(&data, &tokenizer);

                            assert(patchCount < arrayCount(patches));
                            PointerToPatch *patch = patches + patchCount++;
                            patch->type = PATCH_TYPE_EVENT;
                            patch->id = entId;
                            patch->ptr = (void **)(&entData.entity->event);
                        }
                    }
                    //parse commons data
                    if(tempType == ENTITY_TYPE_COMMONS) {
                        if(stringsMatchNullN("id", token.at, token.size))  {
                            entData.commons->ID = getIntFromDataObjects(&data, &tokenizer);
                            maxId = max(entData.commons->ID, maxId);
                            assert(data.memory);
                        }
                        if(stringsMatchNullN("name", token.at, token.size))  {
                            char *string = getStringFromDataObjects(&data, &tokenizer);
                            entData.commons->name = nullTerminate(string, strlen(string));
                            if(cmpStrNull(entData.commons->name, "player")) {
                                gameState->player = entData.entity;
                                assert(entData.entity);
                            }
                        }
                        if(stringsMatchNullN("flags", token.at, token.size))  {
                            entData.commons->flags = getIntFromDataObjects(&data, &tokenizer);
                        }
                        if(stringsMatchNullN("type", token.at, token.size))  {
                            char *name = getStringFromDataObjects(&data, &tokenizer);
                            int type = findEnumValue(name, EntityTypeStrings, arrayCount(EntityTypeStrings));
                            entData.commons->type = (EntityType)type;
                        }
                        if(stringsMatchNullN("position", token.at, token.size))  {
                            entData.commons->pos = buildV3FromDataObjects(&data, &tokenizer);
                        }
                        if(stringsMatchNullN("velocity", token.at, token.size))  {
                            entData.commons->dP = buildV3FromDataObjects(&data, &tokenizer);
                        }
                        if(stringsMatchNullN("renderPosOffset", token.at, token.size))  {
                            entData.commons->renderPosOffset = buildV3FromDataObjects(&data, &tokenizer);
                        }
                        if(stringsMatchNullN("dim", token.at, token.size))  {
                            entData.commons->dim = buildV3FromDataObjects(&data, &tokenizer);
                        }
                        if(stringsMatchNullN("renderScale", token.at, token.size))  {
                            entData.commons->renderScale = buildV3FromDataObjects(&data, &tokenizer);
                        }
                        if(stringsMatchNullN("shading", token.at, token.size))  {
                            entData.commons->shading = buildV4FromDataObjects(&data, &tokenizer);
                        }
                        if(stringsMatchNullN("texture", token.at, token.size))  {
                            char *name = getStringFromDataObjects(&data, &tokenizer);
                            entData.commons->tex = findAsset(name);
                            assert(entData.commons->tex);
                        }
                        if(stringsMatchNullN("inverseWeight", token.at, token.size))  {
                            entData.commons->inverseWeight = getFloatFromDataObjects(&data, &tokenizer);
                        }
                        if(stringsMatchNullN("angle", token.at, token.size))  {
                            entData.commons->angle = getFloatFromDataObjects(&data, &tokenizer);;
                        }
                        if(stringsMatchNullN("animation", token.at, token.size))  {
                            char *name = getStringFromDataObjects(&data, &tokenizer);
                            Asset *animationAsset = findAsset(name);
                            entData.commons->animationParent = animationAsset;
                            assert(animationAsset);

                            AnimationParent *animParent = (AnimationParent *)animationAsset->file;
                            AddAnimationToList(gameState, &gameState->longTermArena, entData.commons, FindAnimationWithState(animParent->anim, animParent->count, ANIM_IDLE));
                        }
                    }
                } break;
                case TOKEN_STRING: {
                    assert(!"invalid code path");
                } break;
                case TOKEN_INTEGER: {
                    assert(!"invalid code path");
                } break;
                case TOKEN_FLOAT: {
                    assert(!"invalid code path");
                } break;
                case TOKEN_BOOL: {
                    assert(!"invalid code path");
                } break;
                default : {
                    
                }
            }
            //the infinite alloc
            if(data.memory) {
                free(data.memory);
            }
        }
        free(contents.memory);
    }


    for(int patchIndex = 0; patchIndex < patchCount; ++patchIndex) {
        PointerToPatch *patch = patches + patchIndex;
        
        void *toSet = 0;
        switch(patch->type) {
            case PATCH_TYPE_EVENT: {
                Event *event = findEventFromID(&gameState->events, patch->id);
                assert(event);
                toSet = event;
            } break;
            case PATCH_TYPE_COMMON: {
                Entity_Commons *ent = findEntityFromID(&gameState->commons, patch->id);
                assert(ent);
                toSet = ent;
            } break;
            case PATCH_TYPE_ENTITY: {
                Entity_Commons *ent = findEntityFromID(&gameState->commons, patch->id);
                assert(ent);
                toSet = ent->parent;
            } break;
            case PATCH_TYPE_ENTITY_POS: {
                Entity_Commons *ent = findEntityFromID(&gameState->commons, patch->id);
                assert(ent);
                toSet = &ent->pos;
            } break;
        }
        assert(toSet);
        *patch->ptr = toSet; 
    }   

    gameState->ID = maxId + 1;
    gameState->eventID = maxEventId + 1;

    assert(gameState->player);
    assert(gameState->camera);
}

void freeFolderNames(FileNameOfType* folderFileNames) {
    for(int i = 0; i < folderFileNames->count; ++i) {
        free(folderFileNames->names[i]);
    }
}

bool loadLevelFromFile(GameState *gameState, char *loadDirNameFull, char *folderName) {
    bool result = false;
    FileNameOfType folderFileNames = getDirectoryFolders(loadDirNameFull);
    for(int i = 0; i < folderFileNames.count; i++) {
        char buf[256] = {};
        getFileLastPortionWithBuffer(buf, 256, folderFileNames.names[i]);
        printf("found Name: %s\n", folderFileNames.names[i]);
        printf("folder Name: %s\n", folderName);
        printf("folder Name: %s\n", buf);
        if(cmpStrNull(buf, folderName)) {
            char *loadDir = concat(folderFileNames.names[i], "/");
            loadWorld(gameState, loadDir);
            result = true;
            free(loadDir);
            break;
        }
    }
    freeFolderNames(&folderFileNames);
    return result;
}