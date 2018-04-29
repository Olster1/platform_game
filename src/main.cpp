#define FIXED_FUNCTION_PIPELINE 0

#if !FIXED_FUNCTION_PIPELINE
#include <GL/gl3w.h> 
#endif

#define IMGUI_ON 1
#if IMGUI_ON
#include "imgui.h"
#include "imgui_sdl/imgui_impl_sdl_gl3.h"
#endif

// #include <stdlib.h>
#include <stdio.h>
#include <time.h> // to init random generator
#include "easy_types.h"
// #include <string.h>

#include <SDL2/sdl.h>

#include "easy.h"

static char* globalExeBasePath;
static Arena arena;

static float bufferHeight;
static float bufferWidth; 
static GameButton gameButtons[BUTTON_COUNT];

#include "easy_files.h"
#include "easy_math.h"
#include "easy_error.h"
#include "easy_array.h"
#include "sdl_audio.h"
#include "easy_opengl.h"
// #include "../shared/easy_3d.h"
#include "../shared/easy_font.h"
#include "easy_timer.h"
#define GJK_IMPLEMENTATION 
#include "easy_gjk.h"
#include "easy_physics.h"

typedef struct {
    V3 pos;
    V3 dim; 
} RenderInfo;

RenderInfo calculateRenderInfo(V3 pos, V3 dim, V3 cameraPos, Matrix4 metresToPixels) {
    RenderInfo info = {};
    info.pos = v3_minus(pos, cameraPos);
    info.pos = transformPositionV3(info.pos, metresToPixels);

    info.dim = transformPositionV3(dim, metresToPixels);
    return info;
}

static Texture globalFireTex_debug = {};
#include "easy_particle_effects.h"

//#include "../shared/easy_ui.h"

#include "animations.h" //this was pulled out because entities need it. and The animation file needs entities...
#include "assets.h"
#include "event.h"
#include "entity.h"
#include "main.h"
#include "undo_buffer.h"
#include "easy_animation.h" //relys on gameState


static bool runningFunc = false;

typedef struct {
    SDL_sem *sem;
    SDL_Window *windowHandle;
    SDL_GLContext *threadContext;
} ThreadInfo;

static int threadFunc(void *ptr) {
    ThreadInfo *info = (ThreadInfo *)ptr;
    assert(info->sem);
    assert(info->threadContext);
    assert(info->windowHandle);
    
    if(SDL_GL_MakeCurrent(info->windowHandle, *info->threadContext) != 0) {
        printf("couldn't make opengl context current in thread\n");
    }
    
    for(;;) {
        SDL_SemWait(info->sem);
    }
}

typedef struct {
    float maxDistance;
    Entity_Commons *hitEnt;
} CastRayInfo;

CastRayInfo castRayAgainstWorld(GameState *gameState, Entity_Commons *ent, V2 ray) {
    CastRayInfo result = {};
    bool isVarSet = false;
    for(int testIndex = 0; testIndex < gameState->commons.count; testIndex++) {
        Entity_Commons *testEnt = (Entity_Commons *)getElement(&gameState->commons, testIndex);
        if(testEnt && isFlagSet(testEnt, ENTITY_VALID) && isFlagSet(testEnt, ENTITY_COLLIDES) && testEnt != ent) {
            Matrix4 aRot = mat4_angle_aroundZ(testEnt->angle);
            
            V2 shapeA_[4];
            transformRectangleToSides(shapeA_, testEnt->pos.xy , testEnt->dim.xy, aRot);
            //TODO: This doesn't take into account if the player is rotated!!!
            Rect2f entDim = rect2fCenterDimV2(ent->pos.xy, ent->dim.xy);
            V2 leftCorner = v2(entDim.minX, entDim.minY);
            V2 rightCorner = v2(entDim.maxX, entDim.minY);
            V2 center = v2(entDim.minX + 0.5f*ent->dim.x, entDim.minY);

            RayCastInfo info = easy_phys_castRay(center, ray, shapeA_, 4);
            if(!info.collided) {
                info = easy_phys_castRay(rightCorner, ray, shapeA_, 4);
                if(!info.collided) {
                    info = easy_phys_castRay(leftCorner, ray, shapeA_, 4);
                }
            } 
            if(info.collided) {
                if(info.distance < result.maxDistance || !isVarSet) {
                    result.hitEnt = testEnt;
                    result.maxDistance = info.distance;
                    isVarSet = true;
                }
            }
        }
    }
    return result;
}

// typedef enum {
//     VAR_CHAR_STAR,
//     VAR_LONG_INT,
//     VAR_INT,
//     VAR_FLOAT,
//     VAR_V2,
//     VAR_V3,
//     VAR_V4,
// } VarType_;

void addVar_(InfiniteAlloc *mem, void *val_, int count, char *varName, VarType type) {
    char data[1028];
    sprintf(data, "\t%s: ", varName);
    addElementInifinteAllocWithCount_(mem, data, strlen(data));

    if(count > 1 && type != VAR_CHAR_STAR) {
        assert(!"array not handled yet");
    }
    switch(type) {
        case VAR_CHAR_STAR: {
            if(count == 1) {
                char *val = (char *)val_;
                sprintf(data, "%s", val);
            } else {
                assert(count > 1);
                printf("isArray\n");

                char **val = (char **)val_;
                char *bracket = "[";
                addElementInifinteAllocWithCount_(mem, bracket, 1);
                for(int i = 0; i < count; ++i) {
                    printf("%s\n", val[i]);
                    sprintf(data, "%s", val[i]);    
                    addElementInifinteAllocWithCount_(mem, data, strlen(data));
                    if(i != count - 1) {
                        char *commaString = ", ";
                        addElementInifinteAllocWithCount_(mem, commaString, 2);
                    }
                }
                bracket = "]";
                addElementInifinteAllocWithCount_(mem, bracket, 1);
                data[0] = 0; //clear data
                
            }
        } break;
        case VAR_LONG_INT: {
            unsigned long *val = (unsigned long *)val_;
            sprintf(data, "%ld", val[0]);
        } break;
        case VAR_INT: {
            int *val = (int *)val_;
            sprintf(data, "%d", val[0]);
        } break;
        case VAR_FLOAT: {
            float *val = (float *)val_;
            sprintf(data, "%f", val[0]);
        } break;
        case VAR_V2: {
            float *val = (float *)val_;
            sprintf(data, "%f %f", val[0], val[1]);
        } break;
        case VAR_V3: {
            float *val = (float *)val_;
            sprintf(data, "%f %f %f", val[0], val[1], val[2]);
        } break;
        case VAR_V4: {
            float *val = (float *)val_;
            sprintf(data, "%f %f %f %f", val[0], val[1], val[2], val[3]);
        } break;
    }
    addElementInifinteAllocWithCount_(mem, data, strlen(data));

    sprintf(data, ";\n");
    addElementInifinteAllocWithCount_(mem, data, strlen(data));
}

#define addVarArray(mem, val_, count, varName, type) addVar_(mem, val_, count, varName, type)
#define addVar(mem, val_, varName, type) addVar_(mem, val_, 1, varName, type)

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
    addVar(mem, ent->renderPosOffset.E, "renderPosOffset", VAR_V3);
    addVar(mem, ent->dim.E, "dim", VAR_V3);
    addVar(mem, ent->renderScale.E, "renderScale", VAR_V3);
    addVar(mem, ent->shading.E, "shading", VAR_V4);

    addVar(mem, ent->tex->name, "texture", VAR_CHAR_STAR);
    addVar(mem, &ent->inverseWeight, "inverseWeight", VAR_FLOAT);

    if(ent->animationParent) {
        addVar(mem, ent->animationParent->name, "animation", VAR_CHAR_STAR);
    }
    endDataType(mem);
}

char *getEntName(char *prepend, Entity_Commons *ent) {
    char name[256];
    sprintf(name, "_%d_", ent->ID);
    char *entFileName = concat(prepend, name);
    return entFileName;
}

EntFileData beginEntFileData(char *dirName, char *fileName_, char *entType, Entity_Commons *ent) {
    EntFileData result = {};

    char *entName = getEntName(entType, ent);
    char *a = concat(dirName, entName);
    char *entFileName = concat(a, fileName_);

    result.mem = initInfinteAlloc(char);
    result.fileHandle = platformBeginFileWrite(entFileName);
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
    
    //TODO: Do events want to be assets

    // initArray(&gameState->particleSystems, particle_system);
    // initArray(&gameState->events, Event);

    for(int entIndex = 0; entIndex < gameState->npcEntities.count; entIndex++) {
        NPC *ent = (NPC *)getElement(&gameState->npcEntities, entIndex);
        if(ent && isFlagSet(ent->e, ENTITY_VALID)) {
            EntFileData fileData = beginEntFileData(dir, fileName, "NPC", ent->e);
            beginDataType(&fileData.mem, "NPC");
            endDataType(&fileData.mem);

            outputCommonData(&fileData.mem, ent->e);
            endEntFileData(&fileData);
        }
    }

    for(int entIndex = 0; entIndex < gameState->noteParentEnts.count; entIndex++) {
        NoteParent *ent = (NoteParent *)getElement(&gameState->noteParentEnts, entIndex);
        if(ent && isFlagSet(ent->e, ENTITY_VALID)) {
            EntFileData fileData = beginEntFileData(dir, fileName, "noteParent", ent->e);

            beginDataType(&fileData.mem, "NoteParent");
            addVar(&fileData.mem, &ent->noteValueCount, "noteValueCount", VAR_INT);
            
            if(ent->noteValueCount > 0) {
                char *noteStrings[32] = {};
                for(int i = 0; i < ent->noteValueCount; ++i) {
                    char *noteValue = NoteValueStrings[ent->sequence[i]];
                    noteStrings[i] = noteValue;
                }
                addVarArray(&fileData.mem, noteStrings, ent->noteValueCount, "noteSequence", VAR_CHAR_STAR);
            }
            //TODO: ALSO EVENTS
            endDataType(&fileData.mem);

            outputCommonData(&fileData.mem, ent->e);

            endEntFileData(&fileData);
        }
    }

    

    for(int entIndex = 0; entIndex < gameState->noteEnts.count; entIndex++) {
        Note *ent = (Note *)getElement(&gameState->noteEnts, entIndex);
        if(ent && isFlagSet(ent->e, ENTITY_VALID)) {
            EntFileData fileData = beginEntFileData(dir, fileName, "note", ent->e);

            beginDataType(&fileData.mem, "Note");
            if(ent->sound) {
                addVar(&fileData.mem, ent->sound->name, "sound", VAR_CHAR_STAR);
            }
            addVar(&fileData.mem, NoteValueStrings[ent->value], "noteValue", VAR_CHAR_STAR);
            addVar(&fileData.mem, &ent->parent->e->ID, "parentID", VAR_INT);
            endDataType(&fileData.mem);

            outputCommonData(&fileData.mem, ent->e);

            endEntFileData(&fileData);
        }
    }

    for(int entIndex = 0; entIndex < gameState->entities.count; entIndex++) {
        Entity *ent = (Entity *)getElement(&gameState->entities, entIndex);
        if(ent && isFlagSet(ent->e, ENTITY_VALID)) {
            EntFileData fileData = beginEntFileData(dir, fileName, "entity", ent->e);

            beginDataType(&fileData.mem, "Entity");
            endDataType(&fileData.mem);

            //TODO: Event *event; Also has an event

            outputCommonData(&fileData.mem, ent->e);

            endEntFileData(&fileData);
        }
    }

    for(int entIndex = 0; entIndex < gameState->platformEnts.count; entIndex++) {
        Entity *ent = (Entity *)getElement(&gameState->platformEnts, entIndex);
        if(ent && isFlagSet(ent->e, ENTITY_VALID)) {
            EntFileData fileData = beginEntFileData(dir, fileName, "platform", ent->e);

            beginDataType(&fileData.mem, "Platform");
            addVar(&fileData.mem, ent->centerPoint.E, "centerPoint", VAR_V3);
            addVar(&fileData.mem, PlatformTypeStrings[ent->platformType], "platformType", VAR_CHAR_STAR);
            endDataType(&fileData.mem);

            //TODO: Event *event; Also has an event

            outputCommonData(&fileData.mem, ent->e);

            endEntFileData(&fileData);
        }
    }

    for(int entIndex = 0; entIndex < gameState->collisionEnts.count; entIndex++) {
        Collision_Object *ent = (Collision_Object *)getElement(&gameState->collisionEnts, entIndex);
        if(ent && isFlagSet(ent->e, ENTITY_VALID)) {
            EntFileData fileData = beginEntFileData(dir, fileName, "collision", ent->e);
            beginDataType(&fileData.mem, "Collision");
            endDataType(&fileData.mem);

            outputCommonData(&fileData.mem, ent->e);
            endEntFileData(&fileData);
        }
    }

    for(int entIndex = 0; entIndex < gameState->doorEnts.count; entIndex++) {
        Door *ent = (Door *)getElement(&gameState->doorEnts, entIndex);
        if(ent && isFlagSet(ent->e, ENTITY_VALID)) {
            EntFileData fileData = beginEntFileData(dir, fileName, "door", ent->e);
            beginDataType(&fileData.mem, "Door");
            endDataType(&fileData.mem);

            outputCommonData(&fileData.mem, ent->e);

            Door *ent = (Door *)getElement(&gameState->doorEnts, ++entIndex);
            if(isFlagSet(ent->e, ENTITY_VALID)) {
                //TODO: if we delete an entity make sure we delete the other door
                assert(ent);
                beginDataType(&fileData.mem, "Door");
                endDataType(&fileData.mem);

                outputCommonData(&fileData.mem, ent->e);
            }
            endEntFileData(&fileData);
        }
    }
} 

void loadWorld(char *dir) {
    
}

V2 transformWorldPToScreenP(V2 inputA, float zPos, float width, float height, V2 middleP) {
    V4 screenSpace = transformPositionV3ToV4(v2ToV3(inputA, zPos), projectionMatrixToScreen(width, height));
    V3 screenSpaceV3 = v3(inputA.x / screenSpace.w, inputA.y/ screenSpace.w, screenSpace.z / screenSpace.w);
    V2 result = v2_plus(screenSpaceV3.xy, middleP);
    return result;
}

int main(int argc, char *args[]) {
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER) == 0) {
        
        SDL_Window *windowHandle = 0;
        
#if FIXED_FUNCTION_PIPELINE
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#else 
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
        
        
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
        
        int width = (int)(1280);
        int height = (int)(720);
        
        bufferWidth = width;
        bufferHeight = height;
        
        windowHandle = SDL_CreateWindow(
            "Mind Man",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
            width, 
            height, 
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        
        SDL_DisplayMode mode;
        SDL_GetCurrentDisplayMode(0, &mode);
        
        float dt = 1 / (float)mode.refresh_rate; //use monitor refresh rate 
        
        SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1); 
        SDL_GLContext threadContext = SDL_GL_CreateContext(windowHandle);
        SDL_GLContext renderContext = SDL_GL_CreateContext(windowHandle);
        if(renderContext) {
            
            if(SDL_GL_MakeCurrent(windowHandle, renderContext) == 0) {
                
                if(SDL_GL_SetSwapInterval(1) == 0) {
                    //Success
                } else {
                    printf("Couldn't set swap interval\n");
                }
            } else {
                printf("Couldn't make context current\n");
            }
        }
        
#if !FIXED_FUNCTION_PIPELINE
        gl3wInit();
#endif
        
#if DEVELOPER_MODE //replace the source loaded with the /res file path. 
         //We move back one folder, out of the src file 
        char *globalExeBasePath_ = SDL_GetBasePath();
        // printf("%s\n", globalExeBasePath_);
        unsigned int execPathLength = strlen(globalExeBasePath_);
        globalExeBasePath = (char *)calloc(execPathLength, 1);
        
        char *at = globalExeBasePath_;
        char *dest = globalExeBasePath;
        char *mostRecent = at;
        while(*at) {
            if(*at == '/' && at[1]) { //don't collect last slash
                mostRecent = at;
            }
            *dest = *at;
            at++;
            dest++;
        }
        int indexAt = (int)(mostRecent - globalExeBasePath_) + 1; //plus one to keep the slash
        globalExeBasePath[indexAt] = 'r';
        globalExeBasePath[indexAt + 1] = 'e';
        globalExeBasePath[indexAt + 2] = 's';
        globalExeBasePath[indexAt + 3] = '/';
        globalExeBasePath[indexAt + 4] = '\0';
        assert(strlen(globalExeBasePath) <= execPathLength);

#else 
        globalExeBasePath = SDL_GetBasePath();
        //printf("%s\n", globalExeBasePath);
#endif
        ////SET UP GAME STATE//////
        GameState gameState_ = {};
        GameState *gameState = &gameState_;
        ////CREATE ARENAS///////
        arena = createArena(Megabytes(200));
        gameState->longTermArena = createArena(Kilobytes(500));
        gameState->scratchMemoryArena = createArena(Kilobytes(500));
        //////

        ////INIT ARRAYS
        initArray(&gameState->commons, Entity_Commons);
        initArray(&gameState->entities, Entity);
        initArray(&gameState->collisionEnts, Collision_Object);
        initArray(&gameState->doorEnts, Door);
        initArray(&gameState->undoBuffer, UndoInfo);
        initArray(&gameState->platformEnts, Entity);
        initArray(&gameState->noteEnts, Note);
        initArray(&gameState->noteParentEnts, NoteParent);
        initArray(&gameState->particleSystems, particle_system);
        initArray(&gameState->npcEntities, NPC);
        initArray(&gameState->events, Event);
        ///// ///////


        //for stb_image so the images aren't upside down.
        stbi_set_flip_vertically_on_load(true);
        //

        ////INIT FONTS
        Font mainFont = initFont(concat(globalExeBasePath,(char *)"Merriweather-Regular.ttf"), 32);
        Font mainFontLarge = initFont(concat(globalExeBasePath,(char *)"Merriweather-Regular.ttf"), 64);
        ///
        //////////SETUP AUDIO/////
        SDL_AudioSpec audioSpec = {};
        initAudioSpec(&audioSpec, 44100);
        initAudio(&audioSpec);
        //////////

        ////SETUP OPEN GL//
        enableOpenGl(width, height);
        glCheckError();
        //////

        ////INIT RANDOM GENERATOR
        srand (time(NULL));
        //

        ///////LOAD SOUNDS//////
        //TODO: Free the strings after we load the wav file;
        Asset *backgroundSound = loadSoundAsset((concat(globalExeBasePath,(char *)"Cave and Wind.wav")), &audioSpec);
        Asset *backgroundMusic = loadSoundAsset((concat(globalExeBasePath,(char *)"Chill.wav")), &audioSpec);
        Asset *backgroundMusic2 = loadSoundAsset((concat(globalExeBasePath,(char *)"Chill Major.wav")), &audioSpec);
        
        PlayingSound *firstSound = playSound(&arena, getSoundAsset(backgroundMusic), 0);
        PlayingSound *secondSound = pushSound(&arena, getSoundAsset(backgroundMusic2), secondSound);
        firstSound->nextSound = secondSound;
        
        // playSound(&arena, &backgroundSound, true);
        
        Asset *jumpSound = loadSoundAsset((concat(globalExeBasePath,(char *)"Bottle_Cork.wav")), &audioSpec);

        Asset *doorSound = loadSoundAsset((concat(globalExeBasePath,(char *)"Bird_Owl.wav")), &audioSpec);

        Asset *solvedPuzzleSound = loadSoundAsset((concat(globalExeBasePath,(char *)"ambience_short.wav")), &audioSpec);
        //loadSoundAsset(&solvedPuzzleSound, (concat(globalExeBasePath,(char *)"Success2.wav")), &audioSpec);
        
        //NoteBundle
        Asset *notes[8];
        notes[C_NOTE] = loadSoundAsset((concat(globalExeBasePath,(char *)"note_sounds/c_note.wav")), &audioSpec);
        notes[D_NOTE] = loadSoundAsset((concat(globalExeBasePath,(char *)"note_sounds/d_note.wav")), &audioSpec);
        notes[D_SHARP_NOTE] = loadSoundAsset((concat(globalExeBasePath,(char *)"note_sounds/dSharp_Note.wav")), &audioSpec);
        notes[E_NOTE] = loadSoundAsset((concat(globalExeBasePath,(char *)"note_sounds/e_note.wav")), &audioSpec);
        notes[F_NOTE] = loadSoundAsset((concat(globalExeBasePath,(char *)"note_sounds/f_note.wav")), &audioSpec);
        notes[G_NOTE] = loadSoundAsset((concat(globalExeBasePath,(char *)"note_sounds/g_note.wav")), &audioSpec);
        notes[A_NOTE] = loadSoundAsset((concat(globalExeBasePath,(char *)"note_sounds/a_note.wav")), &audioSpec);
        notes[B_NOTE] = loadSoundAsset((concat(globalExeBasePath,(char *)"note_sounds/b_note.wav")), &audioSpec);

        ///LOAD IMAGES////////
        char *imgFileTypes[] = {"jpg", "jpeg", "png", "bmp"};
        FileNameOfType fileNames = getDirectoryFilesOfType(concat(globalExeBasePath, (char *)"img/"), imgFileTypes, arrayCount(imgFileTypes));

        const char* listbox_items[64];
        int listBoxCount = 0;

        for(int i = 0; i < fileNames.count; ++i) {
            char *name = fileNames.names[i];
            char *shortName = getFileLastPortion(name);
            listbox_items[listBoxCount++] = shortName;
            Asset *asset = findAsset(shortName);
            assert(!asset);
            if(!asset) {
                //asset = loadImageAsset(shortName);
            }
            free(name);
        }

        Asset *doorTex = loadImageAsset(concat(globalExeBasePath, (char *)"img/door1.png"));
        Asset *skyTex = loadImageAsset(concat(globalExeBasePath, (char *)"img/bg3.jpg"));
        Asset *scrollTex = loadImageAsset(concat(globalExeBasePath, (char *)"img/scrollTexture2.jpg"));
        Asset *blockTex = loadImageAsset(concat(globalExeBasePath, (char *)"img/moss_block1.bmp"));
        Asset *noteTex = loadImageAsset(concat(globalExeBasePath, (char *)"img/MusicalNote.png"));
        Asset *stevinusTex = loadImageAsset(concat(globalExeBasePath, (char *)"img/stevinus.png"));
        Asset *flora1Tex = loadImageAsset(concat(globalExeBasePath, (char *)"img/flora1.png"));
        Asset *flora2Tex = loadImageAsset(concat(globalExeBasePath, (char *)"img/flora2.png"));
        Asset *flora3Tex = loadImageAsset(concat(globalExeBasePath, (char *)"img/flora3.png"));
        Asset *enterKeyTex = loadImageAsset(concat(globalExeBasePath, (char *)"img/enter_key.png"));

        //TODO: change this to use an Asset type
        globalFireTex_debug = loadImage(concat(globalExeBasePath, (char *)"img/fire.png"));

#include "InitAnimations.h" //load all the animations
        ///////////////

        Collision_Object *collisionEnt = (Collision_Object *)getEmptyElement(&gameState->collisionEnts);
        initCollisionEnt(&gameState->commons, collisionEnt, v3(1, 1, -1), scrollTex, 0, gameState->ID++);
        collisionEnt->e->dim.x = 30;
        assert(isFlagSet(collisionEnt->e, ENTITY_COLLISION));

        Entity *player = (Entity *)getEmptyElement(&gameState->entities);
        initEntity(&gameState->commons, player, v3(1, 4, -1), doorTex, 1, gameState->ID++);
        setFlag(player->e, ENTITY_PLAYER);
        player->e->name = "player";
        player->e->animationParent = findAsset((char *)"knight animation");
        assert(player->e->animationParent);
        AddAnimationToList(gameState, &gameState->longTermArena, player->e, FindAnimation(gameState->KnightAnimations.anim, gameState->KnightAnimations.count, (char *)"Knight_Idle"));

        gameState->camera.pos.z = 0;
        //////SETUP THREADS
        SDL_sem* semaphore = SDL_CreateSemaphore(0);
        
        ThreadInfo threadInfo = {};
        threadInfo.sem = semaphore;
        threadInfo.threadContext = &threadContext;
        threadInfo.windowHandle = windowHandle;
        
        SDL_Thread* thread = SDL_CreateThread(threadFunc, "threadFunc", &threadInfo);
        ///////////////

        
#if IMGUI_ON
        //glCheckError();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); 
        (void)io;
        
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
        
        // Setup style
        //ImGui::StyleColorsDark();
        ImGui::StyleColorsClassic();
        
        ImGui_ImplSdlGL3_Init(windowHandle);
        
#endif
        
        FrameBuffer compositedBufferMultiSampled = createFrameBufferMultiSample(bufferWidth, bufferWidth, true, 2); 
        //FrameBuffer compositedBuffer = createFrameBuffer(tileWidth, tileHeight, true); 
        
        V2 middleP = v2(0.5f*bufferWidth, 0.5f*bufferHeight);
        
        float ratio = 60 / 1;
        Matrix4 metresToPixels = Matrix4_scale(mat4(), v3(ratio, ratio, 1));

        unsigned int lastTime = SDL_GetTicks();
        
        bool running = true;
        InteractItem interacting = {}; //get rid of this!
        int entityType = 1;
        
        ///IMGUI variables
        float xPower = 60;
        float yPower = 1800;
        float gravityPower = 80;
        bool gravityIsOn = true;
        float initWeight = 0;
        PlatformType platformType = PLATFORM_LINEAR;
        bool followPlayer = false;
        float distanceFromLayer = 0.4f;

        ////This make sures 
        char *exts = {"txt"};
        char saveDir[256] = "../res/levels/level1";
        FileNameOfType files = getDirectoryFilesOfType(saveDir, exts, 1);
        while(true) {
            for(int i = 0; i < files.count; ++i) {
                if(cmpStrNull(files.names[i], saveDir)) {
                    found = true;
                    break;
                }
            }
            if(found) { 
                saveDir = concat("../res/levels/level1");
            } else {
                break;
            }
        }
        



        LerpV3 cameraLerp = initLerpV3();
        bool MultiSample = true;
        float ReboundCoefficient = 0.4f;
        bool shadeColor = false;
        bool debugUI_isOn = true;
        NoteValue noteType = C_NOTE;
        char noteBuf[32] = {};
        float initEntityZPos = -1;

        NoteParent *playingParentNote = 0;
        NoteParent *lastPlayParentNote = 0;
        NoteParent *mostRecentParentNote = 0;

        Asset *currentTex = flora1Tex;
        
        Event *currentEvent = 0;

        while(running) {
            //Save state of last frame game buttons 
            bool mouseWasDown = isDown(gameButtons, BUTTON_LEFT_MOUSE);
            bool mouseWasDownRight = isDown(gameButtons, BUTTON_RIGHT_MOUSE);
            bool leftArrowWasDown = isDown(gameButtons, BUTTON_LEFT);
            bool rightArrowWasDown = isDown(gameButtons, BUTTON_RIGHT);
            bool shiftWasDown = isDown(gameButtons, BUTTON_SHIFT);
            /////
            zeroArray(gameButtons);
            
            assert(gameButtons[BUTTON_LEFT_MOUSE].transitionCount == 0);
            //ask player for new input
            SDL_Event event;
            bool submitTurn = false;
            char *outputFilename;
            
            while( SDL_PollEvent( &event ) != 0 ) {
#if IMGUI_ON
                ImGui_ImplSdlGL3_ProcessEvent(&event);
#endif
                if (event.type == SDL_WINDOWEVENT) {
                    switch(event.window.event) {
                        case SDL_WINDOWEVENT_RESIZED: {
                            // TODO(Oliver): Work on this! Need to think harder about resizing the screen. 
                            height = bufferHeight = event.window.data2;
                            width = bufferWidth = event.window.data1;
                            
                            //OpenGlAdjustScreenDim(width, height);
                            
                            glViewport(0, 0, event.window.data1, event.window.data2);
                        } break;
                        default: {
                        }
                    }
                }  else if( event.type == SDL_QUIT ) {
                    running = false;
                } else if(event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                    
                    SDL_Keycode keyCode = event.key.keysym.sym;
                    
                    bool altKeyWasDown = (event.key.keysym.mod & KMOD_ALT);
                    bool shiftKeyWasDown = (event.key.keysym.mod & KMOD_SHIFT);
                    bool isDown = (event.key.state == SDL_PRESSED);
                    bool repeated = event.key.repeat;
                    ButtonType buttonType = BUTTON_NULL;
                    switch(keyCode) {
                        case SDLK_RETURN: {
                            buttonType = BUTTON_ENTER;
                            if(isDown) {
                            }
                        } break;
                        case SDLK_SPACE: {
                            buttonType = BUTTON_SPACE;
                        } break;
                        case SDLK_UP: {
                            buttonType = BUTTON_UP;
                        } break;
                        case SDLK_ESCAPE: {
                            buttonType = BUTTON_ESCAPE;
                        } break;
                        case SDLK_DOWN: {
                            buttonType = BUTTON_DOWN;
                        } break;
                        case SDLK_LSHIFT: {
                            //buttonType = BUTTON_SHIFT;
                        } break;
                        case SDLK_LEFT: {
                            if(isDown) {
                            }
                            //buttonType = BUTTON_LEFT; //NOTE: use key states instead
                        } break;
                        case SDLK_RIGHT: {
                            if(isDown) {
                            }
                            //buttonType = BUTTON_RIGHT; //NOTE: use key states instead
                        } break;
                        case SDLK_BACKSPACE: {
                            if(isDown) {
                            }
                            
                        } break;
                    }
                    if(buttonType) {
                        sdlProcessGameKey(&gameButtons[buttonType], isDown, repeated);
                    }
                    
                } else if( event.type == SDL_TEXTINPUT ) {
                    char *inputString = event.text.text;
                    //splice(&inputBuffer, inputString, 1);
                    //cursorTimer.value = 0;
                }
            }
            
#if IMGUI_ON
            ImGui_ImplSdlGL3_NewFrame(windowHandle);
#endif

            const Uint8* keystates = SDL_GetKeyboardState(NULL);

            bool leftArrowIsDown = keystates[SDL_SCANCODE_LEFT];
            bool rightArrowIsDown = keystates[SDL_SCANCODE_RIGHT];
            bool shiftIsDown = keystates[SDL_SCANCODE_LSHIFT];
            
            sdlProcessGameKey(&gameButtons[BUTTON_LEFT], leftArrowIsDown, leftArrowWasDown == leftArrowIsDown);
            sdlProcessGameKey(&gameButtons[BUTTON_RIGHT], rightArrowIsDown, rightArrowWasDown == rightArrowIsDown);
            sdlProcessGameKey(&gameButtons[BUTTON_SHIFT], shiftIsDown, shiftWasDown == shiftIsDown);

            int mouseX, mouseY;

            unsigned int mouseState = SDL_GetMouseState(&mouseX, &mouseY);
            
            V2 mouseP = v2(mouseX, mouseY);
            
            V2 mouseP_yUp = v2(mouseP.x, -1*mouseP.y + bufferHeight);
            
#if IMGUI_ON
            if(!io.WantCaptureMouse) 
#endif
            {
                bool leftMouseDown = mouseState & SDL_BUTTON_LMASK;
                sdlProcessGameKey(&gameButtons[BUTTON_LEFT_MOUSE], leftMouseDown, leftMouseDown == mouseWasDown);
                
                bool rightMouseDown = mouseState & SDL_BUTTON_RMASK;
                sdlProcessGameKey(&gameButtons[BUTTON_RIGHT_MOUSE], rightMouseDown, rightMouseDown == mouseWasDownRight);
            }

            //////CLEAR BUFFERS 
            clearBufferAndBind(0, COLOR_PINK);
#define MULTI_SAMPLE 1
#if MULTI_SAMPLE
            if (MultiSample) {
                clearBufferAndBind(compositedBufferMultiSampled.bufferId, COLOR_PINK); 
            }
#endif
            glDisable(GL_DEPTH_TEST);
            openGlTextureCentreDim(getTextureAsset(skyTex)->id, v2ToV3(middleP, -1), v2(bufferWidth, bufferHeight), COLOR_WHITE, 0, mat4(), 1, OrthoMatrixToScreen(bufferWidth, bufferHeight, 1));
            glEnable(GL_DEPTH_TEST);
            ///////
            //////INPUT/////
            ///////EDITOR INPUT///////
            if(wasPressed(gameButtons, BUTTON_ESCAPE)) {
                /* FULLSCREEN STUFF
                bool isFullScreen = (windowFlags & SDL_WINDOW_FULLSCREEN);
                if(SDL_SetWindowFullscreen(windowHandle, false) < 0) {
                    printf("couldn't un-set to full screen\n");
                }
                if(SDL_SetWindowFullscreen(windowHandle, SDL_WINDOW_FULLSCREEN) < 0) {
                    printf("couldn't set to full screen\n");
                }
                */
                debugUI_isOn = !debugUI_isOn;
            }
            if(wasPressed(gameButtons, BUTTON_ENTER)) {
                gravityIsOn = !gravityIsOn;
            }
            //////

            //Event Interaction
            Event *hotEvent = 0;

            for(int entIndex = 0; entIndex < gameState->events.count; entIndex++) {
                Event *ent = (Event *)getElement(&gameState->events, entIndex);
                if(ent && isEventFlagSet(ent, EVENT_TRIGGER)) {
                    Rect3f triggerDim = rect3fCenterDimV3(*ent->pos, ent->dim);
                    if(inBoundsV3(player->e->pos, triggerDim)) {
                        hotEvent = ent;
                    }
                }
            }

            if(hotEvent && !currentEvent) {
                static float theta = 0;
                theta += dt;
                V3 posAt = *hotEvent->pos;
                posAt.y += 0.1f*sin(theta);
                RenderInfo renderInfo = calculateRenderInfo(posAt, v3(0.5f, 0.5f, 0), gameState->camera.pos, metresToPixels);
                //printf("%f\n", hotEvent->pos->z);
                //printf("%f\n---\n", renderInfo.pos.z);
                
                openGlTextureCentreDim(getTextureAsset(enterKeyTex)->id, renderInfo.pos, renderInfo.dim.xy, COLOR_WHITE, 0, mat4(), 1, projectionMatrixToScreen(bufferWidth, bufferHeight));
                if(!isEventFlagSet(hotEvent, EVENT_EXPLICIT) || wasPressed(gameButtons, BUTTON_SPACE)) {
                    currentEvent = hotEvent;
                }
            }

            V3 inputAccel = {};
            if(currentEvent) {
                Event *lastEvent = currentEvent;
                switch(currentEvent->type) {
                    case EVENT_V3_PAN: {             
                        if(isEventFlagSet(currentEvent, EVENT_FRESH)) {           
                            renewPanEventV3(currentEvent);
                            unSetEventFlag(currentEvent, EVENT_FRESH);
                        }
                        updateLerpV3(&currentEvent->lerpValueV3, dt, currentEvent->lerpType);

                        if(!isOn(&currentEvent->lerpValueV3.timer)) {
                            currentEvent = currentEvent->nextEvent;
                        }
                    } break;
                    case EVENT_DIALOG: {
                        if(isEventFlagSet(currentEvent, EVENT_FRESH)) {           
                            //Don't need to do anything. 
                        }
                        if(wasPressed(gameButtons, BUTTON_UP)) {
                            inputAccel.y = 1;
                        }
                        if(isDown(gameButtons, BUTTON_DOWN)) {
                            inputAccel.y = -1;
                        }

                        char *text = currentEvent->dialog[currentEvent->dialogAt];    
                        TimerReturnInfo timeInfo = updateTimer(&currentEvent->dialogTimer, dt);
                        
                        assert(text);
                        assert(currentEvent);
                        float tValue = clamp01(timeInfo.canonicalVal / currentEvent->dialogDisplayValue);
                        int stringCount = (int)lerp(0, tValue, strlen(text));
                        
                        float x = 0.2f*bufferWidth;
                        float y = 0.7f*bufferHeight;
                        float dimHeight = 0.3f*bufferHeight;
                        
                        outputTextWithLength(&mainFont, x, y, bufferWidth, bufferHeight, text, stringCount, rect2fMinMax(0.2f*bufferWidth, 0, 0.8f*bufferWidth, bufferHeight), COLOR_BLACK, 1, true);
                        openGlDrawRectCenterDim(v3(0.5f*bufferWidth, y + 0.4f*dimHeight, -1), v2(bufferWidth, dimHeight), v4(0.6f, 0.6f, 0.6f, 0.6f), 0, mat4TopLeftToBottomLeft(bufferHeight), 1, OrthoMatrixToScreen(bufferWidth, bufferHeight, 1));
                        

                        if(timeInfo.finished) {
                            currentEvent->dialogAt++;
                            if(currentEvent->dialogAt >= currentEvent->dialogCount) {
                                currentEvent->dialogAt = 0;
                                currentEvent = currentEvent->nextEvent;
                            }
                        }
                    } break;
                }

                if(lastEvent != currentEvent && currentEvent) {
                    setEventFlag(currentEvent, EVENT_FRESH);   
                }
            } 


            if(!currentEvent){
                ////PLAYER INPUT
                
                if(isDown(gameButtons, BUTTON_LEFT)) {
                    inputAccel.x = -1;
                }
                if(isDown(gameButtons, BUTTON_RIGHT)) {
                    inputAccel.x = 1;
                }
                if(wasPressed(gameButtons, BUTTON_UP)) {
                    inputAccel.y = 1;
                }
                if(isDown(gameButtons, BUTTON_DOWN)) {
                    inputAccel.y = -1;
                }
                
                inputAccel.x = xPower*inputAccel.x;
                inputAccel.y = yPower*inputAccel.y;
            }

            
            ////UPDATE CAMERA///
            if(followPlayer && !currentEvent) {
                V3 relPos = v3_minus(player->e->pos, gameState->camera.pos);
                V3 followDim = v3(2, 2, 2);
#if 1
                float power = 100;
                    V3 camForce = v3(0, 0, 0);
                    if(absVal(relPos.x) > followDim.x) {
                        camForce.x += power*signOf(relPos.x);
                    }
                    if(absVal(relPos.y) > followDim.y) {
                        camForce.y += power*signOf(relPos.y);
                    }
                    if(absVal(relPos.z) > followDim.z) {
                        camForce.z += power*signOf(relPos.z);
                    }

                    easy_phys_updatePosAndVel(&gameState->camera.pos, &gameState->camera.dP, camForce, dt, 0.6f);
#else
                float camDistFromPlayer = getLengthV3(relPos); 
                if(camDistFromPlayer > distanceFromLayer) {
                    V3 targetCamPos = v2ToV3(player->e->pos.xy, player->e->pos.z + distanceFromLayer); // Does this trash the cache?? 
                    float transitionPeriod = 1;//camDistFromPlayer;//TODO: do we want to make it go faster/slower depending on the distance?
                    setLerpInfoV3_s(&cameraLerp, targetCamPos, transitionPeriod, &gameState->camera.pos);

                updateLerpV3(&cameraLerp, dt, SMOOTH_STEP_01);
                }
#endif
                
            }



            //PLATFORM UPDATES
            for(int entIndex = 0; entIndex < gameState->platformEnts.count; entIndex++) {
                Entity *ent = (Entity *)getElement(&gameState->platformEnts, entIndex);
                if(isFlagSet(ent->e, ENTITY_ACTIVE)) {
                    switch(ent->platformType) {
                        case PLATFORM_LINEAR: {
                            float dpLength = getLengthV3(ent->e->dP);
                            ent->e->ddP = v3_scale(10, normalize_V3(ent->e->dP, dpLength));
                        } break;
                        case PLATFORM_CIRCLE: {
                            V3 relP = v3_minus(ent->e->pos, ent->centerPoint);
                            //TODO: 3D not handled here
                            V2 forceDp = perp(normalizeV3(relP).xy);
                            ent->e->dP.xy = v2_scale(100, forceDp);
                        } break;
                        case PLATFORM_FIGURE_OF_EIGHT: {

                        } break;
                        default: {
                            assert(!"case not handled");
                        }
                    }
                }
            }

            //////////////NPC interaction

            NPC *npcEnt = 0;
            for(int entIndex = 0; entIndex < gameState->npcEntities.count; entIndex++) {
                NPC *ent = (NPC *)getElement(&gameState->npcEntities, entIndex);
                if(ent && isFlagSet(ent->e, ENTITY_VALID)) {
                    if(ent->state == NPC_TALK) {
                        ent->e->ddP.x = 0;
                        ent->e->dP.x = 0;
                    } else {
                        TimerReturnInfo timeInfo = updateTimer(&ent->stateTimer, dt);
                        if(timeInfo.finished) {
                            int value = (int)randomBetween(0, 1.999);
                            assert(value == 0 || value == 1);
                            NPC_State state = NPC_IDLE;
                            switch(value) {
                                case 0: {
                                    state = NPC_WALK;
                                } break;
                                case 1:   {
                                    state = NPC_IDLE;
                                } break;
                                default: {
                                    assert(!"case not handled");
                                }
                            }
                            ent->state = state;
                        }    

                        switch(ent->state) {
                            case NPC_IDLE: {
                                ent->e->ddP.x = 0;
                                ent->e->dP.x = 0;
                            } break;
                            case NPC_WALK: {
                                #if 0
                                int signOfVelX = signOf(ent->e->dP.x);
                                float accelX = signOfVelX*20;
                                V2 vec = v2_scale(10, v2(signOfVelX*0.707, -0.707));
                                openGlDrawRectCenterDim(v2ToV3(transformPosition(ent->e->pos.xy, metresToPixels), ent->e->pos.z - gameState->camera.pos.z), transformPosition(v2(5, getLength(vec)), metresToPixels), COLOR_PINK, 0.75f*TAU32, mat4(), 1, projectionMatrixToScreen(bufferWidth, bufferHeight));                                
                                CastRayInfo rayInfo = castRayAgainstWorld(gameState, ent->e, vec);
                                if(!rayInfo.hitEnt) {
                                    accelX *= -1;
                                    ent->e->ddP.x *= -1;
                                }   
                                ent->e->ddP.x = accelX;
                                #endif
                            } break;
                            default: {
                                assert(!"case not handled");
                            }
                        }
                    }

                    Rect2f entBounds = rect2fCenterDimV2(ent->e->pos.xy, ent->e->dim.xy);
                    if(inBounds(player->e->pos.xy, entBounds, BOUNDS_RECT)) {
                        npcEnt = ent;
                    }
                }
            }
            ///////////////DOOR TELEPORTATION
            Door *door = 0;
            for(int entIndex = 0; entIndex < gameState->doorEnts.count; entIndex++) {
                Door *ent = (Door *)getElement(&gameState->doorEnts, entIndex);
                if(ent && isFlagSet(ent->e, ENTITY_VALID) && floatEqual_withError(ent->e->pos.z, player->e->pos.z)) {
                    Rect2f entBounds = rect2fCenterDimV2(ent->e->pos.xy, ent->e->dim.xy);
                    if(inBounds(player->e->pos.xy, entBounds, BOUNDS_RECT)) {
                        door = ent;
                        break;
                    }
                }
            }
            if(door && door != player->lastDoor) {
                //We found a door to go through
                door = door->partner;
                player->e->pos = door->e->pos;
                playSound(&arena, getSoundAsset(doorSound), 0);
            }
            player->lastDoor = door;    
            /////////////////

            ////////////PARENT MUSIC NOTE INTERACTION//////////
            NoteParent *parentNote = 0;
            for(int entIndex = 0; entIndex < gameState->noteParentEnts.count; entIndex++) {
                NoteParent *ent = (NoteParent *)getElement(&gameState->noteParentEnts, entIndex);
                if(ent && isFlagSet(ent->e, ENTITY_VALID)) {
                    if(floatEqual_withError(ent->e->pos.z, player->e->pos.z)) { 
                        Rect2f entBounds = rect2fCenterDimV2(ent->e->pos.xy, ent->e->dim.xy);
                        if(inBounds(player->e->pos.xy, entBounds, BOUNDS_RECT)) {
                            parentNote = ent;
                        }
                    }
                }
            }

            V4 shadeColorForBlock = hexARGBTo01Color(0xFF7BFF8B);
            float nextSoundPer = 2.0f;
            static float theta = 0;
            if(!playingParentNote) { //parent not isn't playing 
                if(parentNote && parentNote->noteValueCount > 0) {
                    if(wasPressed(gameButtons, BUTTON_SPACE)) {
                        playingParentNote = parentNote; 
                        playingParentNote->soundAt = 1; //we play the first sound here. 
                        
                        playingParentNote->soundTimer = initTimer(nextSoundPer);
                        Asset *soundToPlay = notes[playingParentNote->sequence[0]];
                        playSound(&arena, getSoundAsset(soundToPlay), 0);
                        Reactivate(&playingParentNote->e->particleSystem);
                        playingParentNote->e->shading = shadeColorForBlock;
                        setLerpInfoV4_s(&playingParentNote->shadingLerp, COLOR_WHITE, nextSoundPer, &playingParentNote->e->shading);
                    } else {
                        V3 posAt = parentNote->e->pos;
                        
                        theta += 4*dt;
                        posAt.y += 0.1f*sin(theta);
                        posAt.z += 0.01;
                        RenderInfo renderInfo = calculateRenderInfo(posAt, v3(0.5f, 0.5f, 0), gameState->camera.pos, metresToPixels);
                        openGlTextureCentreDim(getTextureAsset(enterKeyTex)->id, renderInfo.pos, renderInfo.dim.xy, COLOR_WHITE, 0, mat4(), 1, projectionMatrixToScreen(bufferWidth, bufferHeight));
                    }
                } else {
                    theta = 0;
                }
                //player->lastParentNote = parentNote;    
            }

            if(playingParentNote) {
                updateLerpV4(&playingParentNote->shadingLerp, dt, SMOOTH_STEP_01);    
                TimerReturnInfo info = updateTimer(&playingParentNote->soundTimer, dt);
                if(info.finished) {
                    if(playingParentNote->soundAt >= playingParentNote->noteValueCount) {
                        assert(playingParentNote->soundAt <= playingParentNote->noteValueCount);
                        playingParentNote = 0; //finsihed playing the sounds. 
                    } else if(playingParentNote->soundAt < playingParentNote->noteValueCount) { //won't be the case if if there is only one note in the array
                        playingParentNote->e->shading = shadeColorForBlock;
                        setLerpInfoV4_s(&playingParentNote->shadingLerp, COLOR_WHITE, nextSoundPer, &playingParentNote->e->shading);

                        Asset *soundToPlay = notes[playingParentNote->sequence[playingParentNote->soundAt++]];
                        playSound(&arena, getSoundAsset(soundToPlay), 0);
                        Reactivate(&playingParentNote->e->particleSystem);
                    }
                }
            } 
           
            ////////////MUSIC NOTE INTERACTION//////////
            Note *note = 0;
            for(int entIndex = 0; entIndex < gameState->noteEnts.count; entIndex++) {
                Note *ent = (Note *)getElement(&gameState->noteEnts, entIndex);
                if(ent && isFlagSet(ent->e, ENTITY_VALID)) {
                    TimerReturnInfo timerInfo = updateTimer(&ent->swayTimer, dt);
                    float wiggleFactor = 0.2f;
                    ent->e->renderPosOffset.y = wiggleFactor*sin(TAU32*timerInfo.canonicalVal);
                    
                    if(floatEqual_withError(ent->e->pos.z, player->e->pos.z)) { 
                        Rect2f entBounds = rect2fCenterDimV2(ent->e->pos.xy, ent->e->dim.xy);
                        if(inBounds(player->e->pos.xy, entBounds, BOUNDS_RECT)) {
                            note = ent;
                        }
                    }
                }
            }
            if(note && note != player->lastNote) {
                assert(note->parent);
                NoteParent *parent = note->parent;
                int temp = addElement(&parent->values, note->value);
                playSound(&arena, getSoundAsset(note->sound), 0);

                assert(parent->values.count);
                bool match = true; //try break the match
                for(int noteIndex = 0; noteIndex < parent->values.count && match; noteIndex++) {
                    bool keepLooking = true;
                    for(int parIndex = 0; parIndex < parent->noteValueCount && keepLooking; ++parIndex) {
                        NoteValue parVal = parent->sequence[parIndex];
                        int testIndex = noteIndex + parIndex;
                        if(testIndex < parent->values.count) { //could overflow past the parent values 
                            NoteValue *valPtr = (NoteValue *)getElement(&parent->values, testIndex);
                            if(valPtr) { //can't assert on this since it is not always stack based as the array doesn't sort based on index. 
                                NoteValue val = *valPtr;
                                if(parVal != val) {
                                    keepLooking = false;
                                }
                            }
                        } else {
                            //break both loops
                            keepLooking = false;
                            match = false;
                            break;
                        }
                    }
                    assert(parent->values.count > 0);
                    if(match && keepLooking && parent->noteValueCount > 0) {
                        parent->solved = true;
                        //clear out array 
                        removeSectionOfElements(&parent->values, REMOVE_ORDERED, 0, parent->values.count);
                        assert(parent->values.count == 0);
                        playSound(&arena, getSoundAsset(solvedPuzzleSound), 0);
                        Reactivate(&parent->e->particleSystem);
                        break;    
                    }    
                }
            }
            player->lastNote = note;    
            ///////////////// RAY CASTING FOR PLATFORMS AND JUMPING /////
            CastRayInfo rayInfo = {};
            for(int entIndex = 0; entIndex < gameState->commons.count; entIndex++) {
                Entity_Commons *ent = (Entity_Commons *)getElement(&gameState->commons, entIndex);
                if(ent && isFlagSet(ent, ENTITY_VALID)) {
                    if(isFlagSet(ent, ENTITY_PLAYER)) {
                        float distanceToJump = 0.1f;
                        rayInfo = castRayAgainstWorld(gameState, ent, v2_scale(distanceToJump, v2(0, -1)));
                        break;
                    }
                }
            }

            bool onPlatform = false;
            if(rayInfo.hitEnt) { //we know the player is grounded
                if(rayInfo.hitEnt->type == ENT_TYPE_PLATFORM) {
                    player->e->ddP = rayInfo.hitEnt->ddP;
                    onPlatform = true;
                } 
                if(inputAccel.y > 0) { //player wants to jump 
                    playSound(&arena, getSoundAsset(jumpSound), 0);
                }
            } else {
                //stop the player from jumping if it isn't grounded. 
                if(inputAccel.y > 0) {
                    inputAccel.y = 0;
                }
            }
            if(!onPlatform) {
                player->e->ddP = v3(0, 0, 0);
            }
            ////////

            
            //glBindFramebuffer(GL_FRAMEBUFFER, 0); //back to screen buffer
            glViewport(0, 0, bufferWidth, bufferHeight);
            
            InteractItem hotEnt = {};
            
            for(int entIndex = 0; entIndex < gameState->commons.count; entIndex++) {
                Entity_Commons *ent = (Entity_Commons *)getElement(&gameState->commons, entIndex);
                if(ent && isFlagSet(ent, ENTITY_VALID)) {

                    ///////UPDATE PHYSICS HERE/////
                    V3 force = ent->ddP;
                    isNanErrorV3(force);
                    if(gravityIsOn && isFlagSet(ent, ENTITY_GRAVITY_AFFECTED)) {
                        force.y = -gravityPower*ent->inverseWeight; //knock static objects gravity out with inverse weight. 
                    }
                    if(isFlagSet(ent, ENTITY_PLAYER)) {
                        force = v3_plus(inputAccel, v3_scale(ent->inverseWeight, force)); 
                    }

                    
                    //Do multiple physics loop
                    int loopCount = 1;
                    float dragFactor = 0.2f/loopCount;
                    V4 shadedColor = COLOR_WHITE;
                    float dtValue = dt;// / loopCount;
                    for(int i = 0; i < loopCount; i++) {
                        isNanErrorV3(ent->dP);
                        ent->pos = v3_plus(ent->pos, v3_plus(v3_scale(sqr(dtValue), force),  v3_scale(dtValue, ent->dP)));
                        ent->dP = v3_plus(ent->dP, v3_minus(v3_scale(dtValue, force), v3_scale(dragFactor, ent->dP)));
                        isNanErrorV3(ent->pos);
                        isNanErrorV3(ent->dP);
                        
                        //No angle forces as of yet. 
                        ent->angle = dtValue*ent->dA + ent->angle;
                        ent->dA = ent->dA - dragFactor*ent->dA;
                        ///////////

                        // V3 relPosEnt = v3_minus(ent->pos, gameState->camera.pos);
                        // Rect2f entProjBounds = rect2fCenterDimV2(relPosEnt.xy, ent->dim.xy);
                        // float entZValue = 1;//gameState->camera.pos.z - ent->pos.z;
                        // entProjBounds.min = v2(entProjBounds.min.x/entZValue, entProjBounds.min.y/entZValue);
                        // entProjBounds.max = v2(entProjBounds.max.x/entZValue, entProjBounds.max.y/entZValue);

                        // V3 entP = ent->pos;//v2ToV3(getCenter(entProjBounds), ent->pos.z);
                        // V2 entDim = ent->dim.xy;//getDim(entProjBounds);
                        
                        
                        /////COLLISION DETECTION HERE!!!!
                        for(int testIndex = 0; testIndex < gameState->commons.count; testIndex++) {
                            Entity_Commons *testEnt = (Entity_Commons *)getElement(&gameState->commons, testIndex);
                            //float testEntZValue = 1;//gameState->camera.pos.z - testEnt->pos.z;
                            //&& floatEqual_withError(ent->pos.z, testEnt->pos.z)
                            if((isFlagSet(ent, ENTITY_PLAYER) && !isFlagSet(testEnt, ENTITY_COLLIDES_WITH_PLAYER)) ||
                                (isFlagSet(testEnt, ENTITY_PLAYER) && !isFlagSet(ent, ENTITY_COLLIDES_WITH_PLAYER))) {
                                continue;
                            }

                            if(testEnt && isFlagSet(testEnt, ENTITY_VALID) && isFlagSet_(ent->flags & testEnt->flags, ENTITY_COLLIDES) && ent != testEnt && !isFlagSet_(ent->flags & testEnt->flags, ENTITY_COLLISION)) {//&& (signOf(entZValue) == signOf(testEntZValue))) {//dont test for collisions between static collision geometry
                                 //&& floatEqual_withError(ent->pos.z, testEnt->pos.z)
                            
                                // V3 relPosTest = v3_minus(testEnt->pos, gameState->camera.pos);
                                // entProjBounds = rect2fCenterDimV2(relPosTest.xy, testEnt->dim.xy);
                                
                                // entProjBounds.min = v2(entProjBounds.min.x/testEntZValue, entProjBounds.min.y/testEntZValue);
                                // entProjBounds.max = v2(entProjBounds.max.x/testEntZValue, entProjBounds.max.y/testEntZValue);

                                // V3 testPos = testEnt->pos;//v2ToV3(getCenter(entProjBounds), testEnt->pos.z);
                                // V2 testDim = testEnt->dim.xy;//getDim(entProjBounds);
                                //assert(ent->pos.z < 0 && testEnt->pos.z < 0);
                                Matrix4 aRot = mat4_angle_aroundZ(ent->angle);
                                Matrix4 bRot = mat4_angle_aroundZ(testEnt->angle);

                                V2 shapeA_[4];
                                V2 shapeB_[4];
                                transformRectangleToSides(shapeA_, ent->pos.xy , ent->dim.xy, aRot);
                                transformRectangleToSides(shapeB_, testEnt->pos.xy , testEnt->dim.xy, bRot);

                                V2 *shapeA = shapeA_;
                                V2 *shapeB = shapeB_;

                                isNanErrorV2(ent->pos.xy);
                                isNanErrorV2(testEnt->pos.xy);

                                Entity_Commons *ent1 = ent;
                                Entity_Commons *ent2 = testEnt;

                                if(ent1->type > ent2->type) { //Entities with lower type will be moved in prefernce of a higher type entity
                                    //assert(!isFlagSet(ent2, ENTITY_COLLISION));
                                    V2 *temp = shapeA;
                                    shapeA = shapeB;
                                    shapeB = temp;

                                    Entity_Commons *tempE = ent1;
                                    ent1 = ent2;
                                    ent2 = tempE;
                                }

                                Gjk_EPA_Info info = gjk_objectsCollide_withEPA((gjk_v2 *)shapeA, 4, (gjk_v2 *)shapeB, 4);
                                if(info.collided) {
                                    if(shadeColor) {
                                        shadedColor = COLOR_RED;
                                    }
                                    //RESOLVE COLLISION STRAIGHT AWAY
                                    V2 resolveVector = v2(info.normal.x, info.normal.y);
                                    isNanErrorV2(resolveVector);

                                    assert(!isFlagSet(ent1, ENTITY_COLLISION));
                                    ent1->pos.xy = v2_plus(ent1->pos.xy, v2_scale(info.distance, (resolveVector)));
                                    isNanErrorV2(ent1->pos.xy);
                                    
                                    V2 rayDirection = normalizeV2(v2_minus(ent2->pos.xy, ent1->pos.xy));
                                    //TODO: This is right. Need to find a better way of finding the hit point
                                    isNanErrorV2(rayDirection);
                                    //RayCastInfo castInfo = easy_phys_castRay(ent1->pos.xy, v2_scale(10000, rayDirection), shapeB, 4); //info.distance + 0.01
                                    //V2 hitPoint = castInfo.point;
                                    //isNanErrorV2(hitPoint);

                                    /////////IMPULSE COLLISION  
                                    // assert(castInfo.collided);
                                    V2 SurfaceNormal = v2_negate(resolveVector);

                                    V2 VelRel = v2_minus(ent1->dP.xy, ent2->dP.xy);
                                    

                                    float inverseWeightA = ent1->inverseWeight;
                                    float inverseWeightB = ent2->inverseWeight;
                                    if(ent1->type == ENT_TYPE_DEFAULT && ent2->type == ENT_TYPE_PLATFORM) {
                                        inverseWeightB = 0; //platform don't get affect by other entities
                                    }
                                    assert(ent1->type <= ent2->type);
                                    //Because of this equation two entities colliding can't have _NO_WEIGHT_, instead we give them a little bit of weight, but have a flag that says "not affected by gravity"
                                    //We could also use a safeRatio and say that two colliding entities can't resolve the velocity
                                    float K = safeRatio0(dotV2(SurfaceNormal, v2_scale((1 + ReboundCoefficient), VelRel)), 
                                        dotV2(v2_scale((inverseWeightA + inverseWeightB), SurfaceNormal), SurfaceNormal));
                                    
                                    isNanErrorf(K);
                                    V2 CollisionImpulse = v2_scale(-(K * ent1->inverseWeight), SurfaceNormal);
                                    V2 HitEntityCollisionImpulse = v2_scale((K * ent2->inverseWeight), SurfaceNormal);
                                    
                                    ent2->dP.xy = v2_plus(ent2->dP.xy, HitEntityCollisionImpulse);
                                    ent1->dP.xy = v2_plus(ent1->dP.xy, CollisionImpulse);
                                    
                                    //////////////////
                                }
                                // ent->pos = entP;//v3_plus(v3(entP.x*entZValue, entP.y*entZValue, relPosEnt.z), gameState->camera.pos); 
                                // testEnt->pos = testPos;//v3_plus(v3(testPos.x*testEntZValue, testPos.y*testEntZValue, relPosTest.z), gameState->camera.pos);
                            }
                        }
                    }
                    //////////////////////


                //Render positions we send to the GPU. Basically the positions in pixels offset from the center of the screen
                V3 renderPos = v3_minus(v3_plus(ent->pos, ent->renderPosOffset), gameState->camera.pos);
                renderPos = transformPositionV3(renderPos, metresToPixels);
                //
                V3 renderDim = transformPositionV3(v3_hadamard(ent->dim, ent->renderScale), metresToPixels);
                V3 renderColDim = transformPositionV3(ent->dim, metresToPixels);

                //Region that would actually be occupied on the screen after the GPU projects it onto the screen 
                Rect2f entBounds = rect2fCenterDimV2(renderPos.xy, renderDim.xy);

                entBounds.min = transformWorldPToScreenP(entBounds.min, renderPos.z, width, height, middleP);
                entBounds.max = transformWorldPToScreenP(entBounds.max, renderPos.z, width, height, middleP);

#if DEVELOPER_MODE
                if(debugUI_isOn) {
                    V4 outlineColor = COLOR_NULL;
                    float pixelHandleSize = 10; //TODO: do we want to make this a percentage
                    if(inBounds(mouseP_yUp, entBounds, BOUNDS_RECT)) {
                        hotEnt.e = ent;
                        hotEnt.array = &gameState->entities;
                        hotEnt.index = entIndex;
                        hotEnt.type = GRAB_CENTER;
                        //find different grab type
                        // if(inBounds(mouseP_yUp, rect2fMinDim(entBounds.minX, entBounds.minY, pixelHandleSize, renderDim.y), BOUNDS_RECT)) {
                        //     hotEnt.type = GRAB_LEFT;
                        // }
                        // if(inBounds(mouseP_yUp, rect2fMinDim(entBounds.minX, entBounds.maxY - pixelHandleSize, renderDim.x, pixelHandleSize), BOUNDS_RECT)) {
                        //     hotEnt.type = GRAB_TOP;
                        // }
                        // if(inBounds(mouseP_yUp, rect2fMinDim(entBounds.maxX - pixelHandleSize, entBounds.minY, pixelHandleSize, renderDim.y), BOUNDS_RECT)) {
                        //     hotEnt.type = GRAB_RIGHT;
                        // }
                        // if(inBounds(mouseP_yUp, rect2fMinDim(entBounds.minX, entBounds.minY, renderDim.x, pixelHandleSize), BOUNDS_RECT)) {
                        //     hotEnt.type = GRAB_BOTTOM;
                        // }

                        outlineColor = COLOR_GREEN;
                    }

                     if(hotEnt.e || interacting.e == ent) {

                        if(interacting.e  == ent) {
                            outlineColor = COLOR_YELLOW;
                        }
                         openGlDrawRectCenterDim(renderPos, v2_plus(renderColDim.xy, v2(10, 10)), outlineColor, ent->angle, mat4(), 1, projectionMatrixToScreen(bufferWidth, bufferHeight));

                     }
                 }
#endif  
                GLuint textureId = getTextureAsset(ent->tex)->id;
                if(isFlagSet(ent, ENTITY_ANIMATED)) {
                    if(!IsEmpty(&ent->AnimationListSentintel)) {
                        
                        textureId = getCurrentTexture(&ent->AnimationListSentintel)->id;
                        animation *NewAnimation = 0;
                        
                        AnimationState animState = ANIM_IDLE;
                        float Speed = getLength(ent->dP.xy);
                        if(Speed < 1.0f) {
                            animState = ANIM_IDLE;
                            //AnimationName = (char *)"Knight_Idle";
                        } else {
                            float DirectionValue = GetDirectionInRadians(ent->dP.xy);
                            DirectionValue =  (4*DirectionValue / TAU32);
                            int DirectionValue01 = (((int)DirectionValue) + 0.5f < DirectionValue) ? ceil(DirectionValue) : floor(DirectionValue); 
                            DirectionValue = ((float)DirectionValue01/4.0f); //round direction to nearest direction (NORTH EAST SOUTH WEST)

                            if (DirectionValue == 0 || DirectionValue == 1) {
                                animState = ANIM_RIGHT;
                                //AnimationName = (char *)"Knight_Run_Right";
                            } 
                            else if (DirectionValue == 0.25f) {
                                animState = ANIM_UP;
                                //AnimationName = (char *)"Knight_Run_Up";
                            } 
                            else if (DirectionValue == 0.5f) {
                                animState = ANIM_LEFT;
                                //AnimationName = (char *)"Knight_Run_Left";
                            } 
                            else if (DirectionValue == 0.75f) {
                                animState = ANIM_DOWN;
                                //AnimationName = (char *)"Knight_Run_Down";
                            } else {
                                animState = ANIM_IDLE;
                                //AnimationName = (char *)"Knight_Idle";
                            }
                        }

                        //assert(AnimationName);
                        AnimationParent *animParent = getAnimationAsset(ent->animationParent);
                        NewAnimation = FindAnimationWithState(animParent->anim, animParent->count, animState);

                        assert(NewAnimation);
                        UpdateAnimation(gameState, ent, dt, NewAnimation);
                    } 
                }
            
                openGlTextureCentreDim(textureId, renderPos, renderDim.xy, v4_hadamard(shadedColor, ent->shading), ent->angle, mat4(), 1, projectionMatrixToScreen(bufferWidth, bufferHeight));
            }
        }

         ///////UPDATE & DRAW PARTICLE SYSTEMS///
        //TODO: CHnage particle systems to have their own array 
        for(int entIndex = 0; entIndex < gameState->commons.count; entIndex++) {
            Entity_Commons *ent = (Entity_Commons *)getElement(&gameState->commons, entIndex);
            if(ent && isFlagSet(ent, ENTITY_VALID)) {
                drawAndUpdateParticleSystem(&ent->particleSystem, dt, ent->pos, v3(0, 0, 0), gameState->camera.pos, metresToPixels);
            }
        }
        ////////////

    
#if DEVELOPER_MODE
            if(wasPressed(gameButtons, BUTTON_LEFT_MOUSE)) {
                if(hotEnt.e) {
                    interacting = hotEnt;

                    V3 screenSpaceP = v3_minus(interacting.e->pos, gameState->camera.pos);
                    screenSpaceP = transformPositionV3(screenSpaceP, metresToPixels);
                    //screenSpaceP.xy = v2_plus(screenSpaceP.xy, middleP);

                    screenSpaceP.xy = transformWorldPToScreenP(screenSpaceP.xy, interacting.e->pos.z, width, height, middleP);

                    //gameState->mouseOffset = v2_minus(screenSpaceP.xy, mouseP_yUp);                    
                    //TODO: Update mostRecentParentNote here. 
                    beginUndoMove(&gameState->undoBuffer, interacting.e, interacting.e->pos);
                } else {
                    interacting.e = 0; //stop interacting
                    gameState->mouseOffset = v2(0, 0);
                }
            }

            V2 mouseP_worldSpace = v2_plus(v2_scale(1 / ratio, v2_minus(v2_plus(mouseP_yUp, gameState->mouseOffset), middleP)), gameState->camera.pos.xy);

            if(wasPressed(gameButtons, BUTTON_LEFT_MOUSE)) {
                gameState->interactStartingMouseP = v2_scale(1 / ratio, mouseP_yUp);
            }

            if(!hotEnt.e) {
                if(!interacting.e) {
                    if(wasPressed(gameButtons, BUTTON_LEFT_MOUSE)) {
                        if(isDown(gameButtons, BUTTON_SHIFT)) { //create a new entity
                            printf("created Entity\n");
                            switch(entityType) {
                                case 1: {
                                    Entity *newEnt = (Entity *)getEmptyElement(&gameState->entities);
                                    initEntity(&gameState->commons, newEnt, v2ToV3(mouseP_worldSpace, initEntityZPos), doorTex, 1, gameState->ID++);
                                    addToUndoBufferIndex(&gameState->undoBuffer, CREATE_ENTITY, newEnt->e);
                                } break;
                                case 2: {
                                    Collision_Object *newEnt = (Collision_Object *)getEmptyElement(&gameState->collisionEnts);
                                    initCollisionEnt(&gameState->commons, newEnt, v2ToV3(mouseP_worldSpace, initEntityZPos), scrollTex, initWeight, gameState->ID++);
                                    addToUndoBufferIndex(&gameState->undoBuffer, CREATE_ENTITY, newEnt->e);
                                } break;
                                case 3: {
                                    Door *newEnt = (Door *)getEmptyElement(&gameState->doorEnts);
                                    initDoorEnt(&gameState->commons, newEnt, v2ToV3(mouseP_worldSpace, initEntityZPos), doorTex, gameState->ID++);
                                    addToUndoBufferIndex(&gameState->undoBuffer, CREATE_ENTITY, newEnt->e);

                                    Door *newEnt2 = (Door *)getEmptyElement(&gameState->doorEnts);
                                    initDoorEnt(&gameState->commons, newEnt2, v2ToV3(mouseP_worldSpace, initEntityZPos), doorTex, gameState->ID++);
                                    newEnt2->e->pos = v2ToV3(v2_plus(mouseP_worldSpace, v2(1, 1)), initEntityZPos);
                                    addToUndoBufferIndex(&gameState->undoBuffer, CREATE_ENTITY, newEnt2->e);
                                    newEnt2->partner = newEnt;
                                    newEnt->partner = newEnt2;
                                } break;
                                case 4: {
                                    Entity *newEnt = (Entity *)getEmptyElement(&gameState->platformEnts);
                                    initPlatformEnt(&gameState->commons, newEnt, v2ToV3(mouseP_worldSpace, initEntityZPos), scrollTex, gameState->ID++);
                                    addToUndoBufferIndex(&gameState->undoBuffer, CREATE_ENTITY, newEnt->e);
                                    newEnt->platformType = platformType;
                                    
                                } break;
                                case 5: {
                                    Note *newEnt = (Note *)getEmptyElement(&gameState->noteEnts);
                                    Asset *noteSound = notes[noteType];
                                    initNoteEnt(&gameState->commons, newEnt, v2ToV3(mouseP_worldSpace, initEntityZPos), noteTex, noteSound, gameState->ID++);

                                    newEnt->parent = mostRecentParentNote;
                                    assert(newEnt->parent);
                                    newEnt->value = noteType;

                                    addToUndoBufferIndex(&gameState->undoBuffer, CREATE_ENTITY, newEnt->e);
                                    
                                } break;
                                case 6: {
                                    NoteParent *newEnt = (NoteParent *)getEmptyElement(&gameState->noteParentEnts);
                                    initNoteParentEnt(&gameState->commons, newEnt, v2ToV3(mouseP_worldSpace, initEntityZPos), stevinusTex, gameState->ID++);
                                    mostRecentParentNote = newEnt;
                                    char *at = noteBuf;
                                    while(*at) {
                                        char value = *at - 48; //get value relative to 0
                                        if(value < arrayCount(notes)) {
                                            newEnt->sequence[newEnt->noteValueCount++] = (NoteValue)value;
                                        }
                                        at++;
                                    }

                                    addToUndoBufferIndex(&gameState->undoBuffer, CREATE_ENTITY, newEnt->e);

                                } break;
                                case 7: {
                                    Entity *newEnt = (Entity *)getEmptyElement(&gameState->entities);
                                    initScenarioEnt(&gameState->commons, newEnt, v2ToV3(mouseP_worldSpace, initEntityZPos), currentTex, gameState->ID++);
                                    addToUndoBufferIndex(&gameState->undoBuffer, CREATE_ENTITY, newEnt->e);
                                } break;
                                case 8: {
                                    NPC *newEnt = (NPC *)getEmptyElement(&gameState->npcEntities);
                                    initNPCEnt(&gameState->commons, &gameState->events, newEnt, v2ToV3(mouseP_worldSpace, initEntityZPos), doorTex, 1, gameState->ID++);
                                    addDialog(newEnt->event, (char *)"Light is the sound of nature, sound is the light of our world.\n");
                                    addDialog(newEnt->event, (char *)"The garden needs your help. You need to listen\n");

                                    newEnt->event->nextEvent = addV3PanEventWithOffset(&gameState->events, SMOOTH_STEP_00, EVENT_NULL_FLAG, &gameState->camera.pos, v3(2, 10, 0), 3);
                                    newEnt->e->renderScale.x = newEnt->e->renderScale.y = 2;
                                    newEnt->e->renderPosOffset.y = 0.3f;
                                    newEnt->e->animationParent = findAsset((char *)"chubby man animation");
                                    AddAnimationToList(gameState, &gameState->longTermArena, newEnt->e, FindAnimationWithState(gameState->ManAnimations.anim, gameState->ManAnimations.count, ANIM_IDLE));
                                    addToUndoBufferIndex(&gameState->undoBuffer, CREATE_ENTITY, newEnt->e);
                                } break;
                                default: {
                                    printf("case not handled\n");                            
                                }
                            }
                        }
                        interacting.e = &gameState->camera;
                        interacting.type = GRAB_CAMERA;
                        beginUndoMove(&gameState->undoBuffer, interacting.e, interacting.e->pos);
                    }
                } 
            } 

            if(interacting.e) {
                if(isDown(gameButtons, BUTTON_LEFT_MOUSE)) {
                    switch(interacting.type) {
                        case GRAB_CAMERA: {
                            V2 mouseAt = v2_scale(1 / ratio, mouseP_yUp);
                            interacting.e->pos.xy = v2_plus(v2_scale(0.03f, v2_minus(gameState->interactStartingMouseP, mouseAt)), interacting.e->pos.xy); //TODO: can this be compressed into the 'GRAB_CENTER' type?
                        } break;
                        default: {
                            //TODO: Test why this is like this. 
                            float zAt = gameState->camera.pos.z - interacting.e->pos.z;
                            V2 newPos = v2_minus(mouseP_worldSpace, gameState->camera.pos.xy);
                            newPos = v2(newPos.x*zAt, newPos.y*zAt);
                            newPos = v2_plus(newPos, gameState->camera.pos.xy);
                            interacting.e->pos.xy = newPos;

                        }
                        // case GRAB_TOP: {
                        //     interacting.e->dim.y = mouseP_worldSpace.y - gameState->interactStartingMouseP.y;
                        // } break;
                        // case GRAB_BOTTOM: {
                        //     interacting.e->dim.y = mouseP_worldSpace.y - gameState->interactStartingMouseP.y;
                        // } break;
                        // case GRAB_LEFT: {
                        //     interacting.e->dim.x = mouseP_worldSpace.x - gameState->interactStartingMouseP.x;
                        // } break;
                        // case GRAB_RIGHT: {
                        //     interacting.e->dim.x = mouseP_worldSpace.x - gameState->interactStartingMouseP.x;
                        // } break;
                    }
                }

                if(wasReleased(gameButtons, BUTTON_LEFT_MOUSE)) {
                    //if(interacting.e->type == ENTITY_NOTE)
                    endUndoMove(&gameState->undoBuffer, interacting.e->pos);
                }
#if IMGUI_ON
            if(debugUI_isOn) {
                ImGui::Begin("Entity Info");
                ImGui::InputFloat3("Position", interacting.e->pos.E);            
                ImGui::InputFloat3("Velocity", interacting.e->dP.E);            
                ImGui::InputFloat3("Acceleration", interacting.e->ddP.E);            
                ImGui::InputFloat3("Dim", interacting.e->dim.E);            
                ImGui::InputFloat3("Scale", interacting.e->renderScale.E);        
                ImGui::InputFloat3("Render offset", interacting.e->renderPosOffset.E);        

                ImGui::SliderFloat("Angle", &interacting.e->angle, -PI32, PI32);            

                bool flag1 = isFlagSet(interacting.e, ENTITY_VALID);
                if(ImGui::Checkbox("IS_VALID", &flag1)) {
                    if(flag1) {
                        setFlag(interacting.e, ENTITY_VALID);
                    } else {
                        unSetFlag(interacting.e, ENTITY_VALID);
                    }
                }
                bool flag2 = isFlagSet(interacting.e, ENTITY_PLAYER);
                if(ImGui::Checkbox("ENTITY_PLAYER", &flag2)) {
                    if(flag2) {
                        setFlag(interacting.e, ENTITY_PLAYER);
                    } else {
                        unSetFlag(interacting.e, ENTITY_PLAYER);
                    }
                }
                bool flag3 = isFlagSet(interacting.e, ENTITY_COLLISION);
                if(ImGui::Checkbox("ENTITY_COLLISION", &flag3)) {
                    if(flag3) {
                        setFlag(interacting.e, ENTITY_COLLISION);
                    } else {
                        unSetFlag(interacting.e, ENTITY_COLLISION);
                    }
                }
                bool flag4 = isFlagSet(interacting.e, ENTITY_COLLIDES);
                if(ImGui::Checkbox("ENTITY_COLLIDES", &flag4)) {
                    if(flag4) {
                        setFlag(interacting.e, ENTITY_COLLIDES);
                    } else {
                        unSetFlag(interacting.e, ENTITY_COLLIDES);
                    }
                }
                bool flag5 = isFlagSet(interacting.e, ENTITY_ACTIVE);
                if(ImGui::Checkbox("ENTITY_ACTIVE", &flag5)) {
                    if(flag5) {
                        setFlag(interacting.e, ENTITY_ACTIVE);
                    } else {
                        unSetFlag(interacting.e, ENTITY_ACTIVE);
                    }
                }
                bool flag6 = isFlagSet(interacting.e, ENTITY_GRAVITY_AFFECTED);
                if(ImGui::Checkbox("ENTITY_GRAVITY_AFFECTED", &flag6)) {
                    if(flag6) {
                        setFlag(interacting.e, ENTITY_GRAVITY_AFFECTED);
                    } else {
                        unSetFlag(interacting.e, ENTITY_GRAVITY_AFFECTED);
                    }
                }
                bool flag7 = isFlagSet(interacting.e, ENTITY_ANIMATED);
                if(ImGui::Checkbox("ENTITY_ANIMATED", &flag7)) {
                    if(flag7) {
                        setFlag(interacting.e, ENTITY_ANIMATED);
                    } else {
                        unSetFlag(interacting.e, ENTITY_ANIMATED);
                    }
                }

                bool flag8 = isFlagSet(interacting.e, ENTITY_COLLIDES_WITH_PLAYER);
                if(ImGui::Checkbox("ENTITY_COLLIDES_WITH_PLAYER", &flag8)) {
                    if(flag8) {
                        setFlag(interacting.e, ENTITY_COLLIDES_WITH_PLAYER);
                    } else {
                        unSetFlag(interacting.e, ENTITY_COLLIDES_WITH_PLAYER);
                    }
                }

                float inverseWeight;
                
                if (ImGui::Button("Delete Entity")) { 
                    unSetFlag(interacting.e, ENTITY_VALID);
                    addToUndoBufferIndex(&gameState->undoBuffer, DELETE_ENTITY, interacting.e);

                    interacting.e = 0; // no longer interacting.
                }
                
                if (ImGui::Button("Copy Entity")) { 
                    Entity_Commons *interEnt = interacting.e;
                    V3 offsetP = v3(1, 1, 0); //offset by 1 meter when we copy entity
                    
                    
                    //change copy entity to types
                    Entity_Commons *newCommons = 0;
                    if(isFlagSet(interEnt, ENTITY_COLLISION)) {
                        Collision_Object *newEnt = (Collision_Object *)getEmptyElement(&gameState->collisionEnts);
                        initCollisionEnt(&gameState->commons, newEnt, v3_plus(interEnt->pos, offsetP), interEnt->tex, interEnt->inverseWeight, gameState->ID++);
                        newCommons = newEnt->e;
                    } else {
                        Entity *newEnt = (Entity *)getEmptyElement(&gameState->entities);
                        initEntity(&gameState->commons, newEnt, v3_plus(interEnt->pos, offsetP), interEnt->tex, interEnt->inverseWeight, gameState->ID++);
                        newCommons = newEnt->e;
                    }
                    
                    //cant block copy because some ptrs and not all 'static' // could prefiix 
                    newCommons->dim = interEnt->dim;
                    newCommons->renderScale = interEnt->renderScale;
                    addToUndoBufferIndex(&gameState->undoBuffer, CREATE_ENTITY, newCommons);
                }
                ImGui::End();
            }
#endif
            }

#if IMGUI_ON
            if(debugUI_isOn) {
                ImGui::ShowDemoWindow();

                ImGui::Begin("General");
                //ImGui::InputFloat3("Camera position", gameState->camera.pos.E);           
                ImGui::SliderFloat("Camera Z Pos", &gameState->camera.pos.z, min(-10, gameState->camera.pos.z), max(10, gameState->camera.pos.z));            
                
                initEntityZPos = roundToHalf(initEntityZPos);
                ImGui::SliderFloat("Init Entity Pos", &initEntityZPos, -1, -10);            

                //ImGui::SliderFloat("Rebound Coefficient", &ReboundCoefficient, 0, 1);            

                //ImGui::SliderFloat("X-Power", &xPower, 0, 400);            
                //ImGui::SliderFloat("Y-Power", &yPower, 0, 2000);   
                ImGui::SliderFloat("Distance From layer", &distanceFromLayer, 0, 1);   

                ImGui::Checkbox("Multisample Active", &MultiSample);
                ImGui::Checkbox("Follow Player", &followPlayer);
                ImGui::Checkbox("Gravity Active", &gravityIsOn);
                //ImGui::Checkbox("Shade Collisions", &shadeColor);
                //ImGui::SliderFloat("Gravity Power", &gravityPower, 0, 200);            
                
                ImGui::RadioButton("DEFAULT", (int *)(&entityType), 1); ImGui::SameLine();
                ImGui::RadioButton("COLLISION", (int *)(&entityType), 2); ImGui::SameLine();
                ImGui::RadioButton("DOOR", (int *)(&entityType), 3); ImGui::SameLine();
                ImGui::RadioButton("PLATFORM", (int *)(&entityType), 4); ImGui::SameLine();
                if(mostRecentParentNote) {
                    ImGui::RadioButton("NOTE", (int *)(&entityType), 5); 
                }
                ImGui::RadioButton("NOTE_PARENT", (int *)(&entityType), 6); 
                ImGui::RadioButton("SCENARIO ITEM", (int *)(&entityType), 7); 
                ImGui::RadioButton("NPC", (int *)(&entityType), 8); 

                if(entityType == 2) {
                    ImGui::InputFloat("Init Inverse Weight", &initWeight);            
                }

                if(entityType == 4) {
                    ImGui::RadioButton("PLATFORM_LINEAR", (int *)(&platformType), (int)PLATFORM_LINEAR); ImGui::SameLine();
                    ImGui::RadioButton("PLATFORM_CIRCLE", (int *)(&platformType), (int)PLATFORM_CIRCLE); ImGui::SameLine();
                    ImGui::RadioButton("PLATFORM_FIGURE_OF_EIGHT", (int *)(&platformType), (int)PLATFORM_FIGURE_OF_EIGHT);
                }

                if(entityType == 5) {
                    ImGui::RadioButton("C NOTE", (int *)(&noteType), (int)C_NOTE); ImGui::SameLine();
                    ImGui::RadioButton("D NOTE", (int *)(&noteType), (int)D_NOTE); ImGui::SameLine();
                    ImGui::RadioButton("D sharp NOTE", (int *)(&noteType), (int)D_SHARP_NOTE); ImGui::SameLine();
                    ImGui::RadioButton("E NOTE", (int *)(&noteType), (int)E_NOTE); ImGui::SameLine();
                    ImGui::RadioButton("F NOTE", (int *)(&noteType), (int)F_NOTE); 
                    ImGui::RadioButton("G NOTE", (int *)(&noteType), (int)G_NOTE); ImGui::SameLine();
                    ImGui::RadioButton("A NOTE", (int *)(&noteType), (int)A_NOTE); ImGui::SameLine();
                    ImGui::RadioButton("B NOTE", (int *)(&noteType), (int)B_NOTE); ImGui::SameLine();
                }

                if(entityType == 6) {
                    ImGui::InputText("note parent values", noteBuf, IM_ARRAYSIZE(noteBuf));
                }

                
                static int listbox_item_current = 1;
                ImGui::ListBox("textures\n", &listbox_item_current, listbox_items, listBoxCount, 4);

                char *name = (char *)listbox_items[listbox_item_current];
                Asset *asset = findAsset(name);
                if(!asset) {
                    asset = loadImageAsset(name);
                }
                currentTex = asset;

                ////SAVE LEVEL BUTTON//
                static LerpV4 saveTimerDisplay = initLerpV4();
                static V4 saveColor = COLOR_NULL;

                ImGui::InputText("Save Level directory", saveDir, IM_ARRAYSIZE(saveDir));
                
                if (ImGui::Button("Save Level")) { 

                    char *saveFileName = "mm.txt";

                    //Maybe there could be a arena you just create, push things on then clear at the end?? instead of the concats with free _each_ string
                    char *dirName0 = concat("../res/levels/", saveDir);
                    char *dirName = concat(dirName0, "/"); //make sure there is a slash at the end. 

                    bool createdDirectory = platformCreateDirectory(dirName);

                    ImGui::OpenPopup("Delete?");
                    if (ImGui::BeginPopupModal("Delete?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
                    {
                        ImGui::Text("All those beautiful files will be deleted.\nThis operation cannot be undone!\n\n");
                        ImGui::Separator();

                        //static int dummy_i = 0;
                        //ImGui::Combo("Combo", &dummy_i, "Delete\0Delete harder\0");

                        static bool dont_ask_me_next_time = false;
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
                        ImGui::Checkbox("Don't ask me next time", &dont_ask_me_next_time);
                        ImGui::PopStyleVar();

                        if (ImGui::Button("OK", ImVec2(120,0))) { ImGui::CloseCurrentPopup(); }
                        ImGui::SetItemDefaultFocus();
                        ImGui::SameLine();
                        if (ImGui::Button("Cancel", ImVec2(120,0))) { ImGui::CloseCurrentPopup(); }
                        ImGui::EndPopup();
                    }

                    if(createdDirectory) {
                        char *exts[1] = {"txt"};
                        deleteAllFilesOfType(dirName, exts, arrayCount(exts));
                    } else {
                        printf("%s\n", "created folder");
                    }

                    saveWorld(gameState, dirName, saveFileName);

                    free(dirName);

                    saveColor = COLOR_BLACK;
                    setLerpInfoV4_s(&saveTimerDisplay, COLOR_NULL, 1, &saveColor);
                    assert(isOn(&saveTimerDisplay.timer));
                }
                if(isOn(&saveTimerDisplay.timer)) {
                    updateLerpV4(&saveTimerDisplay, dt, SMOOTH_STEP_01);
                    outputText(&mainFontLarge, 0.4f*bufferWidth, 0.5f*bufferHeight, bufferWidth, bufferHeight, (char *)"SAVED", rect2fMinMax(0.2f*bufferWidth, 0, 0.8f*bufferWidth, bufferHeight), saveColor, 1, true);   
                }
                /////

                if (ImGui::Button("Undo")) { 
                    printf("Buffer count: %d\n", gameState->undoBuffer.count);
                    if(gameState->undoBuffer.indexAt > 0) {
                        gameState->undoBuffer.indexAt--;
                        UndoInfo *info = (UndoInfo *)getElement(&gameState->undoBuffer, gameState->undoBuffer.indexAt);
                        assert(info);
                        processUndoRedoElm(gameState, info, UNDO);
                    } else {
                        printf("Nothing to undo in the buffer\n");
                    }
                }
                
                if (ImGui::Button("Redo")) { 
                    printf("Buffer count: %d\n", gameState->undoBuffer.count);
                    assert(gameState->undoBuffer.indexAt <= gameState->undoBuffer.count);
                    if(gameState->undoBuffer.indexAt < gameState->undoBuffer.count) {
                        UndoInfo *info = (UndoInfo *)getElement(&gameState->undoBuffer, gameState->undoBuffer.indexAt);
                        assert(info);
                        processUndoRedoElm(gameState, info, REDO);

                        gameState->undoBuffer.indexAt++;
                    } else {
                        printf("Buffer count: %d\n", gameState->undoBuffer.count);
                        printf("Buffer at: %d\n", gameState->undoBuffer.indexAt);
                        printf("Nothing to redo in the buffer\n");
                    }
                }
                
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
                ImGui::End();
            }
#endif
            
#endif

#if MULTI_SAMPLE
            if (MultiSample) {
                        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                        glCheckError();
                        glBindFramebuffer(GL_READ_FRAMEBUFFER, compositedBufferMultiSampled.bufferId); 
                        glCheckError();
                        glBlitFramebuffer(0, 0, bufferWidth, bufferHeight, 0, 0, bufferWidth, bufferHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
                        glCheckError();
            }
                        ///
#endif      
                        glBindFramebuffer(GL_FRAMEBUFFER, 0); //back to screen buffer
#if IMGUI_ON
            ImGui::Render();
            ImGui_ImplSdlGL3_RenderDrawData(ImGui::GetDrawData());
#endif


            
            unsigned int now = SDL_GetTicks();
            float timeInFrame = (now - lastTime);
            //printf("%f\n", timeInFrame);
            lastTime = SDL_GetTicks();
            
            SDL_GL_SwapWindow(windowHandle);
            
        }
    }
}