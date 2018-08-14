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

static int testInt = 0;

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
#include "easy_font.h"
#include "easy_timer.h"

#define GJK_IMPLEMENTATION 
#include "easy_gjk.h"
#include "easy_physics.h"
#include "easy_lex.h"
#include "easy_text_io.h"
#include "easy_sdl_joystick.h"
#include "easy_perlin.h"
#include "easy_tweaks.h"

typedef struct {
    V3 pos;
    V3 dim; 
    V3 transformPos;
    V3 transformDim;
    Matrix4 pvm;
} RenderInfo;

RenderInfo calculateRenderInfo(V3 pos, V3 dim, V3 cameraPos, Matrix4 metresToPixels) {
    RenderInfo info = {};
    info.pos = pos;

    info.pvm = Mat4Mult(metresToPixels, Matrix4_translate(mat4(), v3_scale(-1, cameraPos)));
    
    info.transformPos = V4MultMat4(v4(pos.x, pos.y, pos.z, 1), info.pvm).xyz;

    info.dim = dim;
    info.transformDim = transformPositionV3(dim, metresToPixels);
    return info;
}

static Texture globalFireTex_debug = {};
#include "easy_particle_effects.h"

//#include "../shared/easy_ui.h"

#include "animations.h" //this was pulled out because entities need it. and The animation file needs entities...
#include "event.h"
#include "assets.h"
#include "entity.h"
#include "main.h"
#include "undo_buffer.h"
#include "easy_animation.h" //relys on gameState
#include "save_load.h"
#include "menu.h"

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
        if(testEnt && !isFlagSet(ent, ENTITY_CAMERA) && isFlagSet(testEnt, ENTITY_VALID) && isFlagSet(testEnt, ENTITY_COLLIDES) && testEnt != ent) {
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

void pushCurrentEvent(Event *event, Event **currentEvent) {
    *currentEvent = event;
    setEventFlag(*currentEvent, EVENT_FRESH);
}

void pushRenderCircle(Array_Dynamic *array, V3 pos, V3 startDim, V3 endDim, float timeDelta, V4 color, RenderCircleType type) {
    int index = addElement_(array, 0, array->sizeofType);
    RenderCircle *cir = (RenderCircle *)getElement(array, index);
    memset(cir, 0, sizeof(RenderCircle));
    cir->arrayIndex = index;
    cir->pos = pos;
    cir->type = type;
    cir->color = color;
    cir->dimLerp.value = startDim;
    setLerpInfoV3_s(&cir->dimLerp, endDim, timeDelta, &cir->dimLerp.value);
}

void removeRenderCircle(Array_Dynamic *renderCircles, RenderCircle *cir) {
    removeElement_ordered(renderCircles, cir->arrayIndex);
    OpenGLdeleteBufferHandles(&cir->renderHandles);
}

V2 transformWorldPToScreenP(V2 inputA, float zPos, float width, float height, V2 middleP) {
    V4 screenSpace = transformPositionV3ToV4(v2ToV3(inputA, zPos), projectionMatrixToScreen(width, height));
    V3 screenSpaceV3 = v3(inputA.x / screenSpace.w, inputA.y/ screenSpace.w, screenSpace.z / screenSpace.w);
    V2 result = v2_plus(screenSpaceV3.xy, middleP);
    return result;
}

NoteParent *getLatestParent(Note *note) {
    NoteParent *parent = 0;
    //Get the latest parent to push to. 
    //Maybe do something if they are all solved find the one that is in a collision dim?
    for(int i = 0; i < note->parentCount; ++i) {
        parent = note->parents[i];
        if(!parent->solved) {
            break;
        }
    }
    assert(parent);
    return parent;
}

void playChord(Arena *arena, Note **notes, int noteCount) {
    for(int noteAt = 0; noteAt < noteCount; ++noteAt) {
        Asset *soundToPlay = notes[noteAt]->sound;
        playGameSound(arena, getSoundAsset(soundToPlay), 0, AUDIO_FOREGROUND);
    }
}

void submitSoundToParent(Note *note, NoteParent **showPuzzleProgress, Timer *showPuzzleProgressTimer, int *bestPuzzleMatchSoFar, LerpV4 *puzzleProgressColorLerp, Event **currentEvent, Asset *solvedPuzzleSound, NoteParent **lastNoteParent) {
    NoteParent *parent = getLatestParent(note);
    addValueToNoteParent(parent, note->value);
    
    *lastNoteParent = parent;

    playGameSound(&arena, getSoundAsset(note->sound), 0, AUDIO_FOREGROUND);
    *showPuzzleProgress = parent;
    setLerpInfoV4_s(puzzleProgressColorLerp, COLOR_WHITE, 0.5f, &puzzleProgressColorLerp->value);

    assert(parent->valueCount);
    bool match = true; //try break the match
    bool stillOptions = (parent->valueCount > 0);
    int startIndex = (parent->valueCount >= arrayCount(parent->values)) ? parent->valueAt : 0;
    for(int noteIndex = startIndex; stillOptions && match; ) { //incremented below. Have to do all this because it's a ring buffer
        assert(stillOptions);
        bool keepLooking = true;
        *bestPuzzleMatchSoFar = 0;
        for(int parIndex = 0; parIndex < parent->noteValueCount && keepLooking; ++parIndex) {
            ChordInfo parVal = parent->sequence[parIndex];
            int testIndex = noteIndex + parIndex;
            //wrap index
            if(testIndex >= parent->valueCount) {
                testIndex = testIndex - parent->valueCount;
                assert(testIndex >= 0);
            }
            //

            if(testIndex != startIndex || noteIndex == startIndex) { //could overflow past the parent values 
                ChordInfo val = parent->values[testIndex];
                bool equalToEachOther = (val.count == parVal.count);
                for(int valIndex = 0; (valIndex < val.count) && equalToEachOther; ++valIndex) {
                    equalToEachOther &= (val.values_[valIndex] == parVal.values_[valIndex]);
                }
                if(equalToEachOther) {
                    keepLooking = false;
                } else {
                    *bestPuzzleMatchSoFar = (*bestPuzzleMatchSoFar) + 1;
                }
            } else {
                //break both loops
                keepLooking = false;
                match = false;
                break;
            }
        }
        assert(parent->valueCount > 0);
        if(match && keepLooking && parent->noteValueCount > 0) {
            //clear out array 
            parent->valueAt = 0;
            parent->valueCount = 0;
            assert(parent->valueCount == 0);
            *showPuzzleProgressTimer = initTimer(2.0f);
            //
            playGameSound(&arena, getSoundAsset(solvedPuzzleSound), 0, AUDIO_FOREGROUND);
            Reactivate(parent->e->particleSystem);
            assert(!(*currentEvent));
            if(parent->eventToTrigger && !parent->solved) {
                //only run the event if the puzzle was solved
                pushCurrentEvent(parent->eventToTrigger, currentEvent);
                parent->solved = true;
            }

            break;    
        }    

        //Our for loop increment 
        noteIndex++;
        if(noteIndex >= parent->valueCount) {
            noteIndex = 0;
        }
        if(noteIndex == startIndex) {
            stillOptions = false;
            break;
        }
        //
        assert(noteIndex != startIndex);
    }

}

int main(int argc, char *args[]) {
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER|SDL_INIT_GAMECONTROLLER) == 0) {
        
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
        
        int width_ = (int)(980);
        int height_ = (int)(540);    

        V2 screenDim = v2(1280, 720);
        // width_ = screenDim.x;
        // height_ = screenDim.y;
        V2 resolution = v2(width_, height_);

        if(argc > 2) {
            screenDim.x = atoi(args[1]);
            screenDim.y = atoi(args[2]);
        }       

        if(argc > 5) {
            resolution.x = atoi(args[3]);
            resolution.y = atoi(args[4]);
        }       
        
        bufferWidth = resolution.x;
        bufferHeight = resolution.y;

        //TODO: OTRHOGRAPHIC PROJECTIONS (ONES USEING PIXELS) HAVE TO GO THROUGH A SCALING MATRIX AS WELL
        //TODO: USE IDEAL RESOLUTION TO BASE EVERTHING OFF
        float screenRelativeSize = resolution.x / screenDim.x; 
        
        windowHandle = SDL_CreateWindow(
            "NOTE RUNNER",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
            screenDim.x, 
            screenDim.y, 
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        
        SDL_DisplayMode mode;
        SDL_GetCurrentDisplayMode(0, &mode);
        
        float dt = 1.0f / min((float)mode.refresh_rate, 60.0f); //use monitor refresh rate 
        float idealFrameTime = 1.0f / 60.0f;
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
        initWorldDataArrays(gameState, false);
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
        enableOpenGl(bufferWidth, bufferHeight);
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
        Asset *backgroundMusic3 = loadSoundAsset((concat(globalExeBasePath,(char *)"c_pad.wav")), &audioSpec);
        Asset *menuSubmitSound = loadSoundAsset((concat(globalExeBasePath,(char *)"Clank1.wav")), &audioSpec);
        Asset *menuMoveSound = loadSoundAsset((concat(globalExeBasePath,(char *)"menuSound.wav")), &audioSpec);
        
        
        PlayingSound *firstSound = playGameSound(&arena, getSoundAsset(backgroundMusic3), 0, AUDIO_BACKGROUND);
        firstSound->nextSound = firstSound;

        playMenuSound(&arena, getSoundAsset(backgroundSound), 0, AUDIO_BACKGROUND);

        // PlayingSound *secondSound = pushGameSound(&arena, getSoundAsset(backgroundMusic2), secondSound);
        // firstSound->nextSound = secondSound;
        // secondSound->nextSound = firstSound;
        
        // playGameSound(&arena, &backgroundSound, true);
        
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

        const char* texNames[64];
        const char* texNamesFull[64];
        int texCount = 0;

        for(int i = 0; i < fileNames.count; ++i) {
            char *fullName = fileNames.names[i];
            char *shortName = getFileLastPortion(fullName);
            if(shortName[0] != '.') { //don't load hidden file 
                int indexAt = texCount++;
                texNames[indexAt] = shortName;
                //texNamesFull[indexAt] = fullName;
                Asset *asset = findAsset(shortName);
                assert(!asset);
                if(!asset) {
                    asset = loadImageAsset(fullName);
                }
            }
            free(fullName);
        }

        Asset *doorTex = findAsset("door1.png");
        Asset *skyTex = findAsset("sky_background.png");//bg3.jpg
        Asset *scrollTex = findAsset("scrollTexture2.jpg");
        Asset *blockTex = findAsset("moss_block1.bmp");
        Asset *noteTex = findAsset("MusicalNote.png");
        Asset *stevinusTex = findAsset("stevinus.png");
        Asset *flora1Tex = findAsset("flora1.png");
        Asset *flora2Tex = findAsset("flora2.png");
        Asset *flora3Tex = findAsset("flora3.png");
        Asset *enterKeyTex = findAsset("enter_key.png");
        Asset *crateTex = findAsset("crate.jpg");
        Asset *starTex = findAsset("stars.png");

        //TODO: change this to use an Asset type
        globalFireTex_debug = loadImage(concat(globalExeBasePath, (char *)"img/fire.png"));

#include "InitAnimations.h" //load all the animations
        ///////////////

        Collision_Object *collisionEnt = (Collision_Object *)getEmptyElement(&gameState->collisionEnts);
        initCollisionEnt(&gameState->commons, collisionEnt, &gameState->particleSystems, v3(1, 1, -1), scrollTex, 0, gameState->ID++);
        collisionEnt->e->dim.x = 30;
        assert(isFlagSet(collisionEnt->e, ENTITY_COLLISION));

        gameState->player = (Entity *)getEmptyElement(&gameState->entities);
        initEntity(&gameState->commons, gameState->player, &gameState->particleSystems, v3(1, 4, -1), doorTex, 1, gameState->ID++);
        setFlag(gameState->player->e, ENTITY_PLAYER);
        gameState->player->e->name = "player";
        gameState->player->e->animationParent = findAsset((char *)"knight animation");
        assert(gameState->player->e->animationParent);
        AddAnimationToList(gameState, &gameState->longTermArena, gameState->player->e, FindAnimation(gameState->KnightAnimations.anim, gameState->KnightAnimations.count, (char *)"Knight_Idle"));

        gameState->camera = initEntityCommons(0, &gameState->commons, &gameState->particleSystems, v3(0, 0, 0), 0, 0, gameState->ID++, ENTITY_TYPE_CAMERA);
        gameState->camera->pos.z = 0;
        setFlag(gameState->camera, ENTITY_CAMERA);
        unSetFlag(gameState->camera, ENTITY_COLLIDES);
        gameState->camera->name = "camera";

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
        FrameBuffer compositedBufferMultiSampled = createFrameBufferMultiSample(bufferWidth, bufferWidth, FRAMEBUFFER_DEPTH | FRAMEBUFFER_STENCIL, 2); 
        FrameBuffer finalCompositedBuffer = createFrameBuffer(bufferWidth, bufferHeight, FRAMEBUFFER_DEPTH | FRAMEBUFFER_STENCIL); 
        FrameBuffer lightFrameBuffer = createFrameBuffer(bufferWidth, bufferHeight, FRAMEBUFFER_DEPTH | FRAMEBUFFER_STENCIL); 
        
        V2 middleP = v2(0.5f*bufferWidth, 0.5f*bufferHeight);
        
        float ratio = screenRelativeSize * 60.0f / 1.0f;
        printf("%f\n", screenRelativeSize);
        Matrix4 metresToPixels = Matrix4_scale(mat4(), v3(ratio, ratio, 1));

        unsigned int lastTime = SDL_GetTicks();
        
        bool running = true;
        InteractItem interacting = {}; 
        int entityType = 1; //TODO: Maybe use actual types 

        V4 noteShades[NOTE_COUNT] = {COLOR_RED, COLOR_PINK, COLOR_GREEN, COLOR_YELLOW, COLOR_WHITE, COLOR_BLUE, COLOR_BLACK, v4(0.4f, 0.8f, 0.2f, 1.0f)};

        easyJoyStickOpenControllers();

        Entity_Commons *lastSelectedEnt = 0;
        
        Tweaker tweaker = {};
        char *tweakFile = concat(globalExeBasePath, "../src/tweakFile.txt");


        ///IMGUI variables
        float xPower = 60;
        float yPower = 1800;
        float gravityPower = 80;
        bool gravityIsOn = true;
        float initWeight = 0;
        PlatformType platformType = PLATFORM_LINEAR_X;
        bool followPlayer = true;
        float distanceFromLayer = 0.4f;


        LerpV3 cameraLerp = initLerpV3();
        bool MultiSample = true;
        float ReboundCoefficient = 0.4f;
        bool shadeColor = false;
        bool debugUI_isOn = false;
        //NoteValue noteType = C_NOTE;
        char noteBuf[32] = {};
        float initEntityZPos = -1;
        NoteParent *showPuzzleProgress = 0;
        LerpV4 puzzleProgressColorLerp = initLerpV4();

        int bestPuzzleMatchSoFar = 0;

        NoteParent *playingParentNote = 0;
        NoteParent *mostRecentParentNote = 0;

        Asset *currentTex = flora1Tex;
        
        Event *currentEvent = 0;

        Event *selectedEvent = 0;

        Timer lastNoteTimer = initTimer(0.5f);
        turnTimerOff(&lastNoteTimer);

        char *loadDir = concat(globalExeBasePath, "levels/");
        char *loadProgressDir = concat(globalExeBasePath, "progress/");
        bool loadFresh = true;

//THIS IS LOADING UP A PLAYERS GAME WHERE THEY LEFT OFF LAST TIME
        Tweaker gameSessionTweaker = {};
        char *gameSessionTweakFile = concat(globalExeBasePath, "../res/gameSession.txt");
        refreshTweakFile(gameSessionTweakFile, &gameSessionTweaker);
        long progressId = getLongFromTweakData(&gameSessionTweaker, "saveSlot");
        if(progressId != 0) {
            char progressString[256] = {};
            sprintf(progressString, "%ld", progressId);
            char folderName[256] = {};
            sprintf(folderName, "%s%s", loadProgressDir, progressString);
            if(platformDoesDirectoryExist(folderName)) {
                loadFresh = false;
                loadLevelFromFile(gameState, loadProgressDir, progressString);
            }
        }
        if(loadFresh) {
            loadLevelFromFile(gameState, loadDir, "level1");       
        }
//

        MenuInfo menuInfo = {};
        menuInfo.font = &mainFontLarge;
        menuInfo.windowHandle = windowHandle;
        menuInfo.running = &running;
#define START_WITH_MENU 0
#if START_WITH_MENU
        menuInfo.lastMode = menuInfo.gameMode = MENU_MODE;
        setSoundType(AUDIO_FLAG_MENU);
#else 
        menuInfo.lastMode = menuInfo.gameMode = PLAY_MODE;
        setSoundType(AUDIO_FLAG_MAIN);
#endif


// //////////////CREATE PERLIN NOISE DATA///////////////
// #define perlinWidth 100
// #define perlinHeight 100
//         u32 perlinImageData[perlinHeight*perlinWidth] = {};
//         int arrayAt = 0;
//         //TODO: THIS CAN BE SIMD 
//         for(int y = 0; y < perlinHeight; y++) {
//             for(int x = 0; x < perlinWidth; x++) {
//                 float perlinValue = perlin2d(x, y, 0.1, 8);
//                 V4 color = lerpV4(COLOR_BLACK, perlinValue, COLOR_WHITE);
//                 u32 colorU32 = ((u32)(255.0f*color.x) << 16) | ((u32)(255.0f*color.y) << 8) | 
//                 ((u32)(255.0f*color.z) << 0) | 0xFF << 24;
//                 // ((u32)(255.0f*color.w)
                
//                 perlinImageData[arrayAt++] = colorU32;
//             }
//         }

//         Texture perlinTexture = createTextureOnGPU((unsigned char *)perlinImageData, perlinWidth, perlinHeight, 4);
// ////////////////

        static bool lastBestPuzzleMatch = 0;
        Timer showPuzzleProgressTimer = {};
        showPuzzleProgressTimer.value = -1;
        V4 progColor = COLOR_WHITE;
        NoteParentType currentNoteParentType = NOTE_PARENT_DEFAULT;
        NoteParent *lastNoteParent = 0;
        while(running) {
            testInt = 0;
            if(refreshTweakFile(tweakFile, &tweaker)) {
                gravityIsOn = getBoolFromTweakData(&tweaker, "gravityIsOn");
                followPlayer = getBoolFromTweakData(&tweaker, "followPlayer"); 
                globalSoundOn = getBoolFromTweakData(&tweaker, "globalSoundOn"); 
            }   
            
            //Save state of last frame game buttons 
            bool mouseWasDown = isDown(gameButtons, BUTTON_LEFT_MOUSE);
            bool mouseWasDownRight = isDown(gameButtons, BUTTON_RIGHT_MOUSE);
            bool leftArrowWasDown = isDown(gameButtons, BUTTON_LEFT);
            bool rightArrowWasDown = isDown(gameButtons, BUTTON_RIGHT);
            bool shiftWasDown = isDown(gameButtons, BUTTON_SHIFT);
            bool spaceWasDown = isDown(gameButtons, BUTTON_SPACE);
            /////
            zeroArray(gameButtons);
            
            assert(gameButtons[BUTTON_LEFT_MOUSE].transitionCount == 0);
            //ask player for new input
            SDL_Event event;
            int scrollWheelY = 0;
            
            while( SDL_PollEvent( &event ) != 0 ) {
#if IMGUI_ON
                ImGui_ImplSdlGL3_ProcessEvent(&event);
#endif
                if (event.type == SDL_WINDOWEVENT) {
                    switch(event.window.event) {
                        case SDL_WINDOWEVENT_RESIZED: {
                            screenDim.x = event.window.data1;
                            screenDim.y = event.window.data2;
                        } break;
                        default: {
                        }
                    }
                }  else if( event.type == SDL_QUIT ) {
                    running = false;
                } else if(event.type == SDL_MOUSEWHEEL) {
                    scrollWheelY = event.wheel.y;
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
                        case SDLK_1: {
                            buttonType = BUTTON_1;
                        } break;
                        case SDLK_BACKQUOTE: {
                            buttonType = BUTTON_TILDE;
                        } break;
                        case SDLK_F1: {
                            buttonType = BUTTON_F1;
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
            
            V2 mouseP = v2(mouseX/screenDim.x*bufferWidth, mouseY/screenDim.y*bufferHeight);
            
            V2 mouseP_yUp = v2(mouseP.x, -1*mouseP.y + bufferHeight);

#if IMGUI_ON
            if(!io.WantCaptureMouse) 
#endif
            {
                bool leftMouseDown = mouseState & SDL_BUTTON_LMASK;
                sdlProcessGameKey(&gameButtons[BUTTON_LEFT_MOUSE], leftMouseDown, leftMouseDown == mouseWasDown);
                
                bool rightMouseDown = mouseState & SDL_BUTTON_RMASK;
                sdlProcessGameKey(&gameButtons[BUTTON_RIGHT_MOUSE], rightMouseDown, rightMouseDown == mouseWasDownRight);
            } else {
                //Busy in the editor 
                scrollWheelY = 0;
            }

            ////GET JOYSTICK INPUT
            int controllersAvailable = easyJoyStickPoll();
            if(controllersAvailable) {
                assert(controllersAvailable > 0);
                GameController firstController = globalControllerButtons[0];
            }
            ///////

            //////CLEAR BUFFERS 
            clearBufferAndBind(0, COLOR_BLACK);
            clearBufferAndBind(finalCompositedBuffer.bufferId, COLOR_PINK);
            clearBufferAndBind(lightFrameBuffer.bufferId, COLOR_NULL);
#define MULTI_SAMPLE 1
#if MULTI_SAMPLE
            if (MultiSample) {
                clearBufferAndBind(compositedBufferMultiSampled.bufferId, COLOR_PINK); 
            }
#endif
            if(drawMenu(gameState, loadProgressDir, &menuInfo, gameButtons, getTextureAsset(skyTex), getSoundAsset(menuSubmitSound), getSoundAsset(menuMoveSound), dt, screenRelativeSize, mouseP)) {

            renderDisableDepthTest(&globalRenderGroup);
            static GLBufferHandles backgroundBufferHandles = {};
            openGlTextureCentreDim(&backgroundBufferHandles, getTextureAsset(skyTex)->id, v2ToV3(middleP, -1), v2(bufferWidth, bufferHeight), COLOR_WHITE, 0, mat4(), 1, OrthoMatrixToScreen(bufferWidth, bufferHeight, 1), mat4());
            renderEnableDepthTest(&globalRenderGroup);
            ///////
            //////INPUT/////
            ///////EDITOR INPUT///////
            if(wasPressed(gameButtons, BUTTON_TILDE)) {
                debugUI_isOn = !debugUI_isOn;
                followPlayer = !debugUI_isOn;
            }
            if(wasPressed(gameButtons, BUTTON_ENTER) && isDown(gameButtons, BUTTON_SHIFT)) {
                gravityIsOn = !gravityIsOn;

            }

            //////

            //Event Interaction
            Event *hotEvent = 0;

            for(int entIndex = 0; entIndex < gameState->events.count; entIndex++) {
                Event *ent = (Event *)getElement(&gameState->events, entIndex);
                if(ent && isEventFlagSet(ent, EVENT_TRIGGER)) {
                    Rect3f triggerDim = rect3fCenterDimV3(*ent->pos, ent->dim);
                    if(inBoundsV3(gameState->player->e->pos, triggerDim)) {
                        hotEvent = ent;
                    }
                }
            }

            if(hotEvent && !currentEvent) {
                static float theta = 0;
                theta += dt;
                V3 posAt = *hotEvent->pos;
                posAt.z += 0.01f;
                posAt.y += 0.1f*sin(theta);
                RenderInfo renderInfo = calculateRenderInfo(posAt, v3(0.5f, 0.5f, 0), gameState->camera->pos, metresToPixels);
                static GLBufferHandles eventSignalBufferHandles = {};
                openGlTextureCentreDim(&eventSignalBufferHandles, getTextureAsset(enterKeyTex)->id, renderInfo.pos, renderInfo.dim.xy, COLOR_WHITE, 0, mat4(), 1, renderInfo.pvm, projectionMatrixToScreen(bufferWidth, bufferHeight));
                if(!isEventFlagSet(hotEvent, EVENT_EXPLICIT) || wasPressed(gameButtons, BUTTON_SPACE)) {
                    currentEvent = hotEvent;
                }
            }

            globalLightInfoCount = 0;
            //If lights were just a flat array we wouldn't have to do this. 
            for(int lightIndex = 0; lightIndex < gameState->lights.count; ++lightIndex) {
                Light *ent = (Light *)getElement(&gameState->lights, lightIndex);
                if(ent) {
                    LightInfo *light = globalLightInfos + globalLightInfoCount++;
                    light->pos = ent->e->pos; 
                    light->flux = ent->flux; 
                }
            }

            V3 inputAccel = {};
            // for(int currentEvIndex = 0; currentEvIndex < currentEvents.count; ++currentEvIndex) {
            //     Event *currentEvent = (Event *)getElement(&currentEvents, currentEvIndex);
            //TODO: Move to array so there can be more than one current event at a time. 
                if(currentEvent) {
                    Event *lastEvent = currentEvent;
                    //NOTE: We move to the next event ourselves in each case. So beaware when that happens...
                    switch(currentEvent->type) {
                        case EVENT_V3_PAN: {             
                            if(isEventFlagSet(currentEvent, EVENT_FRESH)) {           
                                renewPanEventV3(currentEvent);
                                assert(currentEvent->lerpValueV3.val);
                                unSetEventFlag(currentEvent, EVENT_FRESH);
                            }
                            updateLerpV3(&currentEvent->lerpValueV3, dt, currentEvent->lerpType);

                            if(!isOn(&currentEvent->lerpValueV3.timer)) {
                                currentEvent = currentEvent->nextEvent;
                            }
                        } break;
                        case EVENT_LOAD_LEVEL: {
                            assert(currentEvent->levelName);
                            loadLevelFromFile(gameState, loadDir, currentEvent->levelName);
                            currentEvent = currentEvent->nextEvent;
                            showPuzzleProgress = 0;
                            bestPuzzleMatchSoFar = 0;
                        } break;
                        case EVENT_DIALOG: {
                            if(isEventFlagSet(currentEvent, EVENT_FRESH)) {           
                                //Don't need to do anything. 
                            }
                            if(wasPressed(gameButtons, BUTTON_UP)) {
                                //TODO: Skip dialog
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
                            
                            static GLBufferHandles dialogBoundsHandles = {};
                            openGlDrawRectCenterDim(&dialogBoundsHandles, v3(0.5f*bufferWidth, y + 0.4f*dimHeight, -1), v2(bufferWidth, dimHeight), v4(0.6f, 0.6f, 0.6f, 0.6f), 0, mat4TopLeftToBottomLeft(bufferHeight), 1, OrthoMatrixToScreen(bufferWidth, bufferHeight, 1));
                            outputTextWithLength(&mainFont, x, y, bufferWidth, bufferHeight, text, stringCount, rect2fMinMax(0.2f*bufferWidth, 0, 0.8f*bufferWidth, bufferHeight), COLOR_BLACK, screenRelativeSize, true);
                            

                            if(timeInfo.finished) {
                                currentEvent->dialogAt++;
                                if(currentEvent->dialogAt >= currentEvent->dialogCount) {
                                    currentEvent->dialogAt = 0;
                                    currentEvent = currentEvent->nextEvent;
                                }
                            }
                        } break;
                        case EVENT_ENTITY_ACTIVE: {
                            Entity_Commons *ent = findEntityFromID(&gameState->commons, currentEvent->entId);
                            setFlag(ent, ENTITY_ACTIVE);
                            currentEvent = currentEvent->nextEvent;
                        } break;
                        case EVENT_FADE_OUT: {
                            TimerReturnInfo timeInfo = updateTimer(&currentEvent->fadeTimer, dt);
                            static GLBufferHandles fadeOutBufferHandle = {};
                            openGlDrawRectCenterDim(&fadeOutBufferHandle, v3(0.5f*bufferWidth, 0.5f*bufferHeight, gameState->camera->pos.z - 0.1f), v2(bufferWidth, bufferHeight), v4(0, 0, 0, lerp(0, timeInfo.canonicalVal, 1)), 0, mat4TopLeftToBottomLeft(bufferHeight), 1, OrthoMatrixToScreen(bufferWidth, bufferHeight, 1));
                            if(timeInfo.finished) {
                                currentEvent->fadeTimer.value = 0;
                                currentEvent = currentEvent->nextEvent;
                            }
                        } break;
                    }
                    
                    if(lastEvent != currentEvent && currentEvent) {
                        setEventFlag(currentEvent, EVENT_FRESH);   
                    }
                }
            // } 

            float divisor = (dt / idealFrameTime);
            Entity_Commons *pCommon = gameState->player->e;
            if(isDown(gameButtons, BUTTON_SPACE)) {
                pCommon->comeBackShading.value.w = 0.7f; //fade a bit
                gameState->player->flags |= ((int)ENTITY_FLAG_INVISIBLE);
            } else if(wasReleased(gameButtons, BUTTON_SPACE)) {
                pCommon->comeBackShading.value.w = 1.0f; //fade a bit
                gameState->player->flags &= (~((int)ENTITY_FLAG_INVISIBLE));
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
                
                

                inputAccel.x = (xPower)*inputAccel.x;
                inputAccel.y = (yPower)*inputAccel.y;
            }

            float physTime = (float)dt / (float)idealFrameTime;
            float remainder = physTime - ((int)physTime);
            int loopCount = (int)ceil(physTime);
            assert(remainder >= 0.0f);
            assert(physTime >= 0.0f);
            assert(loopCount >= 1);

            ////UPDATE CAMERA///
            if(followPlayer && !currentEvent) {
                V3 relPos = v3_minus(gameState->player->e->pos, gameState->camera->pos);
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

                for(int i = 0; i < loopCount; ++i) {
                    float dtValue = getDtValue(idealFrameTime, i, dt, remainder);
                    easy_phys_updatePosAndVel(&gameState->camera->pos, &gameState->camera->dP, camForce, dtValue, 0.6f);
                }
#else
                float camDistFromPlayer = getLengthV3(relPos); 
                if(camDistFromPlayer > distanceFromLayer) {
                    V3 targetCamPos = v2ToV3(player->e->pos.xy, player->e->pos.z + distanceFromLayer); // Does this trash the cache?? 
                    float transitionPeriod = 1;//camDistFromPlayer;//TODO: do we want to make it go faster/slower depending on the distance?
                    setLerpInfoV3_s(&cameraLerp, targetCamPos, transitionPeriod, &gameState->camera.pos);
                    }

                updateLerpV3(&cameraLerp, dt, SMOOTH_STEP_01);
                }
#endif
                
            }

#if DEVELOPER_MODE
            if(debugUI_isOn) {
                gameState->camera->pos.z += 0.3f*dt*scrollWheelY;
            }
#endif



            //PLATFORM UPDATES
            for(int entIndex = 0; entIndex < gameState->platformEnts.count; entIndex++) {
                Entity *ent = (Entity *)getElement(&gameState->platformEnts, entIndex);
                if(isFlagSet(ent->e, ENTITY_ACTIVE)) {
                    switch(ent->platformType) {
                        case PLATFORM_LINEAR_Y: 
                        case PLATFORM_LINEAR_X: {
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
                if(ent->platformType == PLATFORM_LINEAR_X) {
                    ent->e->ddP.y = 0;
                    ent->e->dP.y = 0;
                }
                if(ent->platformType == PLATFORM_LINEAR_Y) {
                    ent->e->ddP.x = 0;
                    ent->e->dP.x = 0;
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
                    if(inBounds(gameState->player->e->pos.xy, entBounds, BOUNDS_RECT)) {
                        npcEnt = ent;
                    }
                }
            }
            bool playerIsInvisible = ((int)gameState->player->flags) & (int)ENTITY_FLAG_INVISIBLE;  
            ///////////////DOOR TELEPORTATION
            Door *door = 0;
            for(int entIndex = 0; entIndex < gameState->doorEnts.count; entIndex++) {
                Door *ent = (Door *)getElement(&gameState->doorEnts, entIndex);
                if(ent && isFlagSet(ent->e, ENTITY_VALID) && floatEqual_withError(ent->e->pos.z, gameState->player->e->pos.z)) {
                    Rect2f entBounds = rect2fCenterDimV2(ent->e->pos.xy, ent->e->dim.xy);
                    if(inBounds(gameState->player->e->pos.xy, entBounds, BOUNDS_RECT) && playerIsInvisible) {
                        door = ent;
                        
                        break;
                    }
                }
            }
            if(door && door != gameState->player->lastDoor) {
                //We found a door to go through
                door = door->partner;
                assert(door);
                gameState->player->e->pos = door->e->pos;
                playGameSound(&arena, getSoundAsset(doorSound), 0, AUDIO_FOREGROUND);
            }
            gameState->player->lastDoor = door;    
            /////////////////

            ////////////PARENT MUSIC NOTE INTERACTION//////////

            float nextSoundPer = 2.0f;
            NoteParent *parentNote = 0;
            for(int entIndex = 0; entIndex < gameState->noteParentEnts.count; entIndex++) {
                NoteParent *ent = (NoteParent *)getElement(&gameState->noteParentEnts, entIndex);
                if(ent && isFlagSet(ent->e, ENTITY_VALID)) {
                    if(floatEqual_withError(ent->e->pos.z, gameState->player->e->pos.z)) { 
                        Rect2f entBounds = rect2fCenterDimV2(ent->e->pos.xy, ent->e->dim.xy);
                        if(inBounds(gameState->player->e->pos.xy, entBounds, BOUNDS_RECT)) {
                            parentNote = ent;
                        }
                    }
                    switch(ent->type) {
                        case NOTE_PARENT_DEFAULT: {
                            // don't do anything. 
                        } break;
                        case NOTE_PARENT_TIME: {    
                            if(!ent->solved && ent->lookedAt) {
                                TimerReturnInfo timeInfo = updateTimer(&ent->autoSoundTimer, dt);
                                if(timeInfo.finished || !ent->autoPlaying) {
                                    ent->autoPlaying = true;
                                    int index = ent->autoSoundAt++; 
                                    if(ent->autoSoundAt >= ent->noteValueCount) {
                                        ent->autoSoundAt = 0;
                                    }
                                    assert(index < ent->noteValueCount && index >= 0);
                                    for(int noteAt = 0; noteAt < ent->sequence[index].count; ++noteAt) {
                                        if(ent->sequence[index].isPlayedByParent[noteAt]) {
                                            submitSoundToParent(ent->sequence[index].notes_[noteAt], &showPuzzleProgress, &showPuzzleProgressTimer, &bestPuzzleMatchSoFar, &puzzleProgressColorLerp, &currentEvent, solvedPuzzleSound, &lastNoteParent);
                                        }
                                    }
                                }
                            }
                        } break;
                        default: {
                            assert(!"not valid");
                        }
                    }
                }
            }

            V4 shadeColorForBlock = hexARGBTo01Color(0xFF7BFF8B);
            
            static float theta = 0;
            if(!playingParentNote) { //parent not isn't playing 
                if(parentNote && parentNote->noteValueCount > 0) {
                    if(wasPressed(gameButtons, BUTTON_SPACE)) {
                        playingParentNote = parentNote; 
                        playingParentNote->soundAt = 1; //we play the first sound here. 
                        
                        playingParentNote->soundTimer = initTimer(nextSoundPer);

                        playChord(&arena, playingParentNote->sequence[0].notes_, playingParentNote->sequence[0].count);
                        
                        setChannelVolume(AUDIO_BACKGROUND, LOW_VOLUME, 0.5f);
                        Reactivate(playingParentNote->e->particleSystem);
                        playingParentNote->e->shading = shadeColorForBlock;
                        setLerpInfoV4_s(&playingParentNote->shadingLerp, COLOR_WHITE, nextSoundPer, &playingParentNote->e->shading);
                        parentNote->showChildren = true;
                    } else {
                        //This is for the enter hovering over the note parents. 
                        V3 posAt = parentNote->e->pos;
                        theta += 4*dt;
                        posAt.y += 0.1f*sin(theta);
                        posAt.z += 0.01;
                        static GLBufferHandles signalParentNote = {};
                        RenderInfo renderInfo = calculateRenderInfo(posAt, v3(0.5f, 0.5f, 0), gameState->camera->pos, metresToPixels);
                        openGlTextureCentreDim(&signalParentNote, getTextureAsset(enterKeyTex)->id, renderInfo.pos, renderInfo.dim.xy, COLOR_WHITE, 0, mat4(), 1, renderInfo.pvm, projectionMatrixToScreen(bufferWidth, bufferHeight));
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
                        playingParentNote->lookedAt = true;
                        playingParentNote = 0; //finished playing the sounds. 
                        setChannelVolume(AUDIO_BACKGROUND, MAX_VOLUME, 0.5f);
                    } else if(playingParentNote->soundAt < playingParentNote->noteValueCount) { //won't be the case if if there is only one note in the array
                        playingParentNote->e->shading = shadeColorForBlock;
                        setLerpInfoV4_s(&playingParentNote->shadingLerp, COLOR_WHITE, nextSoundPer, &playingParentNote->e->shading);

                        ChordInfo chordInfo = playingParentNote->sequence[playingParentNote->soundAt++];
                        playChord(&arena, chordInfo.notes_, chordInfo.count);
                        //playGameSound(&arena, getSoundAsset(soundToPlay), 0, AUDIO_FOREGROUND);
                        Reactivate(playingParentNote->e->particleSystem);
                    }
                }
            } 
           
            ////////////MUSIC NOTE INTERACTION//////////
            static Note *hotNoteGUI = 0;
            Note *nextHotNoteGUI = 0;
            Note *note = 0;
            
            for(int entIndex = 0; entIndex < gameState->noteEnts.count; entIndex++) {
                Note *ent = (Note *)getElement(&gameState->noteEnts, entIndex);
                if(ent && isFlagSet(ent->e, ENTITY_VALID)) {
                    //NOTE: This is just the wiggling
                    TimerReturnInfo timerInfo = updateTimer(&ent->swayTimer, dt);
                    float wiggleFactor = 0.2f;
                    ent->e->renderPosOffset.y = wiggleFactor*sin(TAU32*timerInfo.canonicalVal);
                    //

                    NoteParent *par = ent->parents[0];
                    assert(ent->parentCount > 0);
                    if(par->showChildren) {
                        if(!ent->timerHasRun) {
                            setLerpInfoV4_s(&ent->fadeInLerp, COLOR_WHITE, 1.0f, &ent->e->comeBackShading.value);
                            ent->timerHasRun = true;
                        }
                        updateLerpV4(&ent->fadeInLerp, dt, SMOOTH_STEP_01);

                        if(floatEqual_withError(ent->e->pos.z, gameState->player->e->pos.z)) { 
                            Rect2f entColBounds = rect2fCenterDimV2(ent->e->pos.xy, ent->e->dim.xy);

                            if(inBounds(gameState->player->e->pos.xy, entColBounds, BOUNDS_RECT) && playerIsInvisible) {
                                note = ent;
                            }

                            ////Click on note to play it. ///////////
                            RenderInfo renderInfo = calculateRenderInfo(v3_plus(ent->e->pos, ent->e->renderPosOffset), v3_hadamard(ent->e->dim, ent->e->renderScale), gameState->camera->pos, metresToPixels);
                            V3 transformedPos = renderInfo.transformPos;

                            Rect2f entBounds = rect2fCenterDimV2(transformedPos.xy, renderInfo.transformDim.xy);

                            entBounds.min = transformWorldPToScreenP(entBounds.min, transformedPos.z, bufferWidth, bufferHeight, middleP);
                            entBounds.max = transformWorldPToScreenP(entBounds.max, transformedPos.z, bufferWidth, bufferHeight, middleP);
#define CLICK_ON_NOTE_TO_PLAY 0
#if CLICK_ON_NOTE_TO_PLAY
                            if(inBounds(mouseP_yUp, entBounds, BOUNDS_RECT)) {
                                if(wasPressed(gameButtons, BUTTON_LEFT_MOUSE)) {
                                    playGameSound(&arena, getSoundAsset(ent->sound), 0, AUDIO_FOREGROUND);
                                    pushRenderCircle(&gameState->renderCircles, ent->e->pos, v3(0.0f, 0.0f, 0.0f), v3(2.0f, 2.0f, 2.0f), 2.0f, COLOR_YELLOW, WORLD_RENDER);
                                } else if(!hotNoteGUI || hotNoteGUI != ent) {
                                    pushRenderCircle(&gameState->renderCircles, ent->e->pos, v3(2.0f, 2.0f, 2.0f), v3(0.0f, 0.0f, 0.0f), 1.0f, COLOR_YELLOW, WORLD_RENDER);
                                }
                                nextHotNoteGUI = ent;
                            } 
#endif
                            //////////
                        }
                    } else {
                        ent->e->comeBackShading.value = COLOR_NULL;
                    }

                     if(debugUI_isOn) {
                        ent->e->comeBackShading.value = COLOR_WHITE;   
                     }
                }
            }
            hotNoteGUI = nextHotNoteGUI;

            if(isOn(&lastNoteTimer)) {
                TimerReturnInfo timeInfo = updateTimer(&lastNoteTimer, dt);    
                if(timeInfo.finished) {
                    turnTimerOff(&lastNoteTimer);
                }
            }

            if(note && note != gameState->player->lastNote && !isOn(&lastNoteTimer)) {
                //this is so we don't double collide with notes
                turnTimerOn(&lastNoteTimer);

                //assert(note->parent);

                submitSoundToParent(note, &showPuzzleProgress, &showPuzzleProgressTimer, &bestPuzzleMatchSoFar, &puzzleProgressColorLerp, &currentEvent, solvedPuzzleSound, &lastNoteParent);
            }
            gameState->player->lastNote = note;    

            LerpV4 *progLerp = &puzzleProgressColorLerp;
            if(isOn(&showPuzzleProgressTimer)) {
                //END PUZZLE
                //assert(gameState->player->lastNote);
                assert(lastNoteParent);
                NoteParent *thisParent = lastNoteParent; //gameState->player->lastNote->parent;
                TimerReturnInfo retInfo = updateTimer(&showPuzzleProgressTimer, dt);
                if(retInfo.finished) {
                    showPuzzleProgressTimer.value = -1; //turn off
                    setLerpInfoV4_s(progLerp, COLOR_NULL, 0.5f, &progLerp->value);

                    thisParent->valueAt = 0;
                    thisParent->valueCount = 0;
                    bestPuzzleMatchSoFar = 0;
                    
                }
            }
            bool timerWasOn = isOn(&progLerp->timer);
            updateLerpV4(progLerp, dt, SMOOTH_STEP_01);
            V4 lerpValue = progLerp->value;
            bool timerIsOn = isOn(&progLerp->timer);
            if(timerWasOn && !timerIsOn && v4Equal(lerpValue, COLOR_NULL)) { //timer finished
                NoteParent *thisParent = lastNoteParent;
                for(int i = 0; i < arrayCount(thisParent->puzzleShadeLerp); ++i) {
                    LerpV4 *thisLerp = thisParent->puzzleShadeLerp + i;
                    thisLerp->value = COLOR_RED;
                    thisLerp->timer.value = -1;
                }
            }
            ///////////////// RAY CASTING FOR PLATFORMS AND JUMPING /////
            CastRayInfo rayInfo = {};
            for(int entIndex = 0; entIndex < gameState->commons.count; entIndex++) {
                Entity_Commons *ent = (Entity_Commons *)getElement(&gameState->commons, entIndex);
                if(ent && isFlagSet(ent, ENTITY_VALID) && !isFlagSet(ent, ENTITY_CAMERA)) {
                    if(isFlagSet(ent, ENTITY_PLAYER)) {
                        float distanceToJump = 0.1f;
                        rayInfo = castRayAgainstWorld(gameState, ent, v2_scale(distanceToJump, v2(0, -1)));
                        break;
                    }
                }
            }

            bool onPlatform = false;
            if(rayInfo.hitEnt) { //we know the player is grounded
                if(rayInfo.hitEnt->type == ENT_TYPE_PLATFORM && isFlagSet(rayInfo.hitEnt, ENTITY_ACTIVE)) {
                    //we update the movement of the platform
                    gameState->player->e->ddP = rayInfo.hitEnt->ddP;
                    onPlatform = true;
                } 
                if(inputAccel.y > 0) { //player wants to jump 
                    playGameSound(&arena, getSoundAsset(jumpSound), 0, AUDIO_FOREGROUND);
                }
            } else {
                //stop the player from jumping if it isn't grounded. 
                if(inputAccel.y > 0) {
                    inputAccel.y = 0;
                }
            }
            if(!onPlatform) {
                gameState->player->e->ddP = v3(0, 0, 0);
            }
            ////////

            
            //glBindFramebuffer(GL_FRAMEBUFFER, 0); //back to screen buffer
            glViewport(0, 0, bufferWidth, bufferHeight);
            
            InteractItem hotEnt = {};
            
            for(int entIndex = 0; entIndex < gameState->commons.count; entIndex++) {
                Entity_Commons *ent = (Entity_Commons *)getElement(&gameState->commons, entIndex);
                if(ent && isFlagSet(ent, ENTITY_VALID) && !isFlagSet(ent, ENTITY_CAMERA)) {

                    ///////UPDATE PHYSICS HERE/////
                    V3 force = ent->ddP;
                    
                    isNanErrorV3(force);
                    if(gravityIsOn && isFlagSet(ent, ENTITY_GRAVITY_AFFECTED)) {
                        force.y = -(gravityPower)*ent->inverseWeight; //knock static objects gravity out with inverse weight. 

                        ///////////THIS IS SO THE THINGS DON"T FALL OFF LEDGES
                        if(ent->inverseWeight > 0.0f && ent->type == ENT_TYPE_DEFAULT) {
                            CastRayInfo rayInfo = {};
                            
                            float distanceToJump = 0.1f;
                            rayInfo = castRayAgainstWorld(gameState, ent, v2_scale(distanceToJump, v2(0, -1)));
                            ent->timeSinceLastPoll += dt;
                            if(rayInfo.hitEnt) {
                                ent->timeInAir = 0;
                                if(ent->timeSinceLastPoll >= (1.0f/30.0f)) {
                                    //Only store the postition if it's significantly different
                                    ent->timeSinceLastPoll = 0;
                                    int indexAt = ent->lastGroundedAt++;
                                    
                                    ent->lastGroundedCount++;
                                    if(ent->lastGroundedCount > arrayCount(ent->lastGroundedPos)) {
                                        ent->lastGroundedCount = arrayCount(ent->lastGroundedPos); //clamping the count
                                    }
                                    assert(indexAt <= ent->lastGroundedCount);
                                    if(indexAt == ent->lastGroundedCount) {
                                        ent->lastGroundedAt = indexAt = 0; //warpping the index
                                    }
                                    assert(indexAt >= 0 && indexAt < arrayCount(ent->lastGroundedPos) && indexAt < ent->lastGroundedCount);
                                    ent->lastGroundedPos[indexAt] = ent->pos;
                                } 
                            } else {
                                ent->timeInAir += dt;
                                
                                if(ent->timeInAir > 2.0f/*seconds*/) {
                                    if(ent->lastGroundedCount > 0) { //haven't polled yet
                                        int thisIndexAt = (ent->lastGroundedAt + 1); 
                                        if(thisIndexAt >= ent->lastGroundedCount) {
                                            thisIndexAt = 0; //this is us wrapping the ring buffer
                                        }
                                        assert(thisIndexAt < arrayCount(ent->lastGroundedPos));
                                        ent->pos = ent->lastGroundedPos[thisIndexAt];
                                        ent->dP = v3(0, 0, 0);
                                        ent->timeInAir = 0;
                                        LerpV4 *thisLerp = &ent->comeBackShading;
                                        setLerpInfoV4_s(thisLerp, COLOR_NULL, 0.2f, &thisLerp->value);
                                    }
                                }
                            }

                            updateLerpV4(&ent->comeBackShading, dt, SMOOTH_STEP_01010);
                        }

                        // if(isFlagSet(ent, ENTITY_PLAYER))   //TODO: OPTIMZIATION HERE
                        //////
                    }
                    V3 extraForce = {};
                    if(isFlagSet(ent, ENTITY_PLAYER)) {
                        extraForce = inputAccel; 
                    }

                    //Do multiple physics loop
                    
                    float dragFactor = 0.2f;///((loopCount));
                    V4 shadedColor = COLOR_WHITE;
                    assert(loopCount >= 1);
                    for(int loopIndex = 0; loopIndex < loopCount; loopIndex++) {
                        
                        float dtValue = getDtValue(idealFrameTime, loopIndex, dt, remainder);
                        isNanErrorV3(ent->dP);
                        if(loopIndex > 0) {
                            extraForce.y = 0; //get rid of the jump
                        }
                        V3 thisForce = v3_plus(extraForce, v3_scale(ent->inverseWeight, force)); 
                    
                        if(isFlagSet(ent, ENTITY_ACTIVE)) {
                            ent->pos = v3_plus(ent->pos, v3_plus(v3_scale(sqr(dtValue), thisForce),  v3_scale(dtValue, ent->dP)));
                            ent->dP = v3_plus(ent->dP, v3_minus(v3_scale(dtValue, thisForce), v3_scale(dragFactor, ent->dP)));
                        }
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

                            if(testEnt && !isFlagSet(ent, ENTITY_CAMERA) && isFlagSet(testEnt, ENTITY_VALID) && isFlagSet_(ent->flags & testEnt->flags, ENTITY_COLLIDES) && ent != testEnt && !isFlagSet_(ent->flags & testEnt->flags, ENTITY_COLLISION)) {//&& (signOf(entZValue) == signOf(testEntZValue))) {//dont test for collisions between static collision geometry
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


                RenderInfo renderInfo = calculateRenderInfo(v3_plus(ent->pos, ent->renderPosOffset), v3_hadamard(ent->dim, ent->renderScale), gameState->camera->pos, metresToPixels);
                V3 renderPos = renderInfo.pos;
                V3 renderDim = renderInfo.dim;
                V3 transformedPos = renderInfo.transformPos;

                //This is for the editor
                V3 renderColDim = transformPositionV3(ent->dim, metresToPixels);
                //

#if DEVELOPER_MODE
                if(debugUI_isOn) {

                    //We do this again without the render scale so its easier to select entities!
                    RenderInfo editorRenderInfo = calculateRenderInfo(v3_plus(ent->pos, ent->renderPosOffset), ent->dim, gameState->camera->pos, metresToPixels);
                    //This is for the editor to pick up entities 
                    //Region that would actually be occupied on the screen after the GPU projects it onto the screen 
                    Rect2f entBounds = rect2fCenterDimV2(editorRenderInfo.transformPos.xy, editorRenderInfo.transformDim.xy);

                    entBounds.min = transformWorldPToScreenP(entBounds.min, editorRenderInfo.transformPos.z, bufferWidth, bufferHeight, middleP);
                    entBounds.max = transformWorldPToScreenP(entBounds.max, editorRenderInfo.transformPos.z, bufferWidth, bufferHeight, middleP);
                    /////

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
                         openGlDrawRectCenterDim(0, transformedPos, v2_plus(renderColDim.xy, v2(10, 10)), outlineColor, ent->angle, mat4(), 1, projectionMatrixToScreen(bufferWidth, bufferHeight));

                     }  
                     GLuint edTexId = 0;
                     bool drawHelpIcon = false;
                     if(ent->entType == ENTITY_TYPE_PARTICLE_SYSTEM) {
                        edTexId = getTextureAsset(starTex)->id;
                        drawHelpIcon = true;
                    } else if(ent->entType == ENTITY_TYPE_LIGHT) {
                        edTexId = getTextureAsset(starTex)->id;
                        drawHelpIcon = true;
                    }

                    if(drawHelpIcon) {
                        openGlTextureCentreDim(&ent->bufferHandles, edTexId, renderPos, renderDim.xy, COLOR_WHITE, 0, mat4(), 1, mat4(), Mat4Mult(projectionMatrixToScreen(bufferWidth, bufferHeight), renderInfo.pvm));
                    }

                 }
#endif  
                if(ent->tex) {
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
                    openGlTextureCentreDim(&ent->bufferHandles, textureId, renderPos, renderDim.xy, v4_hadamard(ent->comeBackShading.value, v4_hadamard(shadedColor, ent->shading)), ent->angle, mat4(), 1, mat4(), Mat4Mult(projectionMatrixToScreen(bufferWidth, bufferHeight), renderInfo.pvm));
                }
            }
        }

         ///////UPDATE & DRAW PARTICLE SYSTEMS///
        //TODO: CHnage particle systems to have their own array 
        for(int entIndex = 0; entIndex < gameState->commons.count; entIndex++) {
            Entity_Commons *ent = (Entity_Commons *)getElement(&gameState->commons, entIndex);
            if(ent && isFlagSet(ent, ENTITY_VALID) && !isFlagSet(ent, ENTITY_CAMERA)) {
                drawAndUpdateParticleSystem(ent->particleSystem, dt, v3_plus(ent->pos, ent->particleSystem->Set.offsetP), v3(0, 0, 0), gameState->camera->pos, metresToPixels);
            }
        }
        
        typedef struct {
            NoteParent *parent;
            int index;
        } PuzzleNoteHelper;
        ////////////
        //DRAW PUZZLE PROGRESS
        static PuzzleNoteHelper hotHelperParent = {};
        PuzzleNoteHelper nextHelperParent = {};
        if(showPuzzleProgress) {
            NoteParent *parent = showPuzzleProgress;
            float yAtMin = 0.2f*bufferHeight;
            float yAtMax = 0.4f*bufferHeight;
            float widthPerNote = 0.1f*bufferWidth;
            float xAt = 0.5f*(bufferWidth - (widthPerNote*(parent->noteValueCount + 1)));

            for(int noteIndex = 0; noteIndex < parent->noteValueCount; ++noteIndex) {
                assert(noteIndex < MAX_NOTE_SEQUENCE_SIZE);
                //TODO: take account of minor notes. 
                ChordInfo *thisChord = parent->sequence + noteIndex;
                xAt += widthPerNote;//only move after each chord
                for(int noteAt = 0; noteAt < thisChord->count; noteAt++) {
                    float value = (float)thisChord->values_[noteAt] / (float)NOTE_COUNT;
                    float yAt = lerp(yAtMin, value, yAtMax);

                    LerpV4 *thisLerp = parent->puzzleShadeLerp + noteIndex;

                    updateLerpV4(thisLerp, dt, SMOOTH_STEP_01);

                    V4 color = COLOR_NULL;
                    if(noteIndex < bestPuzzleMatchSoFar) {
                        setLerpInfoV4_s(thisLerp, COLOR_YELLOW, 0.5f, &thisLerp->value);
                    }
                    color = v4_hadamard(puzzleProgressColorLerp.value, thisLerp->value);

                    GLBufferHandles *thisRenderHandle = &parent->puzzleProgressRenderHandles[noteIndex];

                    //MAKE THIS MORE SCREEN RESOLUTION INDEPENENT
                    V2 circleDim = v2(70, 70);
                    V3 helperPos = v3(xAt, yAt, -1);
                    Rect2f circleBounds = rect2fCenterDim(xAt, yAt, 0.3f*circleDim.x, 0.3f*circleDim.y);
                    if(inBounds(mouseP, circleBounds, BOUNDS_CIRCLE)) {
                        if(wasPressed(gameButtons, BUTTON_LEFT_MOUSE)) {
                            playGameSound(&arena, getSoundAsset(thisChord->notes_[noteAt]->sound), 0, AUDIO_FOREGROUND);
                            pushRenderCircle(&gameState->renderCircles, helperPos, v3(0.0f, 0.0f, 0.0f), v3(30.0f, 30.0f, 30.0f), 2.0f, COLOR_YELLOW, SCREEN_RENDER);
                        } else if(!hotHelperParent.parent || hotHelperParent.parent != parent || (hotHelperParent.parent == parent && hotHelperParent.index != noteIndex)) {
                            pushRenderCircle(&gameState->renderCircles, helperPos, v3(15.0f, 15.0f, 15.0f), v3(0.0f, 0.0f, 0.0f), 0.7f, COLOR_YELLOW, SCREEN_RENDER);
                        }
                        nextHelperParent.index = noteIndex;
                        nextHelperParent.parent = parent;
                    }
                    openGlDrawCircle(thisRenderHandle, helperPos, circleDim, color, mat4TopLeftToBottomLeft(bufferHeight), 1, OrthoMatrixToScreen(bufferWidth, bufferHeight, 1), mat4());
                }
            }
        }
        hotHelperParent = nextHelperParent;
        //printf("%d\n", hotHelperParent.index);
        lastBestPuzzleMatch = bestPuzzleMatchSoFar;
        //  

        /////Render Circle array
        for(int entIndex = 0; entIndex < gameState->renderCircles.count; entIndex++) {
            RenderCircle *ent = (RenderCircle *)getElement(&gameState->renderCircles, entIndex);
            if(ent) {
                bool wasLerpOn = isOn(&ent->dimLerp.timer);
                updateLerpV3(&ent->dimLerp, dt, SMOOTH_STEP_01);
                bool isLerpOn = isOn(&ent->dimLerp.timer);
                
                V4 ringColor = smoothStep00V4(COLOR_NULL, ent->dimLerp.timer.value, ent->color);
                if(ent->type == WORLD_RENDER) {
                    RenderInfo renderInfo = calculateRenderInfo(ent->pos, ent->dimLerp.value, gameState->camera->pos, metresToPixels);
                    openGlDrawRing(&ent->renderHandles, renderInfo.pos, renderInfo.dim.xy, ringColor, mat4(), 1, renderInfo.pvm, projectionMatrixToScreen(bufferWidth, bufferHeight));
                } else if(ent->type == SCREEN_RENDER){
                    openGlDrawRing(&ent->renderHandles, ent->pos, v2_scale(10, ent->dimLerp.value.xy), ringColor, mat4TopLeftToBottomLeft(bufferHeight), 1, mat4(), OrthoMatrixToScreen(bufferWidth, bufferHeight, 1));
                }
                
                if(wasLerpOn && !isLerpOn) {
                    removeRenderCircle(&gameState->renderCircles, ent);
                }
                                    
            }
        }
        
    
#if DEVELOPER_MODE
            if(wasPressed(gameButtons, BUTTON_LEFT_MOUSE)) {
                if(hotEnt.e) {
                    interacting = hotEnt;

                    lastSelectedEnt = hotEnt.e;

                    V3 screenSpaceP = v3_minus(interacting.e->pos, gameState->camera->pos);
                    screenSpaceP = transformPositionV3(screenSpaceP, metresToPixels);
                    //screenSpaceP.xy = v2_plus(screenSpaceP.xy, middleP);

                    screenSpaceP.xy = transformWorldPToScreenP(screenSpaceP.xy, interacting.e->pos.z, bufferWidth, bufferHeight, middleP);

                    //gameState->mouseOffset = v2_minus(screenSpaceP.xy, mouseP_yUp);                    
                    //TODO: Update mostRecentParentNote here. 
                    if(interacting.e->entType == ENTITY_TYPE_NOTE_PARENT) {
                        mostRecentParentNote = (NoteParent *)interacting.e->parent;    
                    }
                    
                    beginUndoMove(&gameState->undoBuffer, interacting.e, interacting.e->pos);
                } else {
                    interacting.e = 0; //stop interacting
                    gameState->mouseOffset = v2(0, 0);
                }
            }

            V2 mouseP_worldSpace = v2_plus(v2_scale(1 / ratio, v2_minus(v2_plus(mouseP_yUp, gameState->mouseOffset), middleP)), gameState->camera->pos.xy);

            if(wasPressed(gameButtons, BUTTON_LEFT_MOUSE)) {
                gameState->interactStartingMouseP = v2_scale(1 / ratio, mouseP_yUp);
            }

            if(!hotEnt.e) {
                if(!interacting.e) {
                    if(wasPressed(gameButtons, BUTTON_LEFT_MOUSE)) {
                        if(isDown(gameButtons, BUTTON_SHIFT)) { //create a new entity
                            V3 initPos = v2ToV3(mouseP_worldSpace, initEntityZPos);
                            switch((EntType)entityType) {
                                case ENTITY_TYPE_ENTITY: {
                                    Entity *newEnt = (Entity *)getEmptyElement(&gameState->entities);
                                    initEntity(&gameState->commons, newEnt, &gameState->particleSystems, initPos, crateTex, 1, gameState->ID++);
                                    addToUndoBufferIndex(&gameState->undoBuffer, CREATE_ENTITY, newEnt->e);
                                } break;
                                case ENTITY_TYPE_PARTICLE_SYSTEM: {
                                    Entity_Commons *newEnt = (Entity_Commons *)getEmptyElement(&gameState->commons);
                                    initEntityCommons_(0, newEnt, &gameState->particleSystems, initPos, 0, 1, gameState->ID++, ENTITY_TYPE_PARTICLE_SYSTEM);
                                    newEnt->entType = ENTITY_TYPE_PARTICLE_SYSTEM;
                                    newEnt->particleSystem->Set.Loop = true;
                                    newEnt->particleSystem->Active = true;
                                    Rect2f velBias = rect2fMinMax(-2, -2, 2, 2);
                                    newEnt->particleSystem->Set.VelBias = velBias;
                                    //newEnt->particleSystem->Set.BitmapCount = 0;
                                    unSetFlag(newEnt, ENTITY_GRAVITY_AFFECTED);
                                    unSetFlag(newEnt, ENTITY_COLLIDES);
                                    addToUndoBufferIndex(&gameState->undoBuffer, CREATE_ENTITY, newEnt);
                                } break;
                                case ENTITY_TYPE_LIGHT: {
                                    Light *newEnt = (Light *)getEmptyElement(&gameState->lights);
                                    float flux = 10;
                                    initLight(&gameState->commons, newEnt, &gameState->particleSystems, initPos, flux, gameState->ID++);
                                    addToUndoBufferIndex(&gameState->undoBuffer, CREATE_ENTITY, newEnt->e);
                                } break;
                                case ENTITY_TYPE_COLLISION: {
                                    Collision_Object *newEnt = (Collision_Object *)getEmptyElement(&gameState->collisionEnts);
                                    initCollisionEnt(&gameState->commons, newEnt, &gameState->particleSystems, initPos, scrollTex, initWeight, gameState->ID++);
                                    addToUndoBufferIndex(&gameState->undoBuffer, CREATE_ENTITY, newEnt->e);
                                } break;
                                case ENTITY_TYPE_DOOR: {
                                    Door *newEnt = (Door *)getEmptyElement(&gameState->doorEnts);
                                    initDoorEnt(&gameState->commons, newEnt, &gameState->particleSystems, initPos, doorTex, gameState->ID++);
                                    addToUndoBufferIndex(&gameState->undoBuffer, CREATE_ENTITY, newEnt->e);

                                    Door *newEnt2 = (Door *)getEmptyElement(&gameState->doorEnts);
                                    initDoorEnt(&gameState->commons, newEnt2, &gameState->particleSystems, initPos, doorTex, gameState->ID++);
                                    newEnt2->e->pos = v3_plus(initPos, v3(1, 1, 0));
                                    addToUndoBufferIndex(&gameState->undoBuffer, CREATE_ENTITY, newEnt2->e);
                                    newEnt2->partner = newEnt;
                                    newEnt->partner = newEnt2;
                                } break;
                                case ENTITY_TYPE_PLATFORM: {
                                    Entity *newEnt = (Entity *)getEmptyElement(&gameState->platformEnts);
                                    initPlatformEnt(&gameState->commons, newEnt, &gameState->particleSystems, initPos, blockTex, gameState->ID++, platformType);
                                    addToUndoBufferIndex(&gameState->undoBuffer, CREATE_ENTITY, newEnt->e);
                                } break;
                                // case ENTITY_TYPE_NOTE: {
                                //     Note *newEnt = (Note *)getEmptyElement(&gameState->noteEnts);
                                //     Asset *noteSound = notes[noteType];
                                //     initNoteEnt(&gameState->commons, newEnt, initPos, noteTex, noteSound, gameState->ID++);

                                //     newEnt->parent = mostRecentParentNote;
                                //     assert(newEnt->parent);
                                //     newEnt->value = noteType;

                                //     addToUndoBufferIndex(&gameState->undoBuffer, CREATE_ENTITY, newEnt->e);
                                    
                                // } break;
                                case ENTITY_TYPE_NOTE_PARENT: {
                                    if(noteBuf[0] != '\0') {
                                        NoteParent *newEnt = (NoteParent *)getEmptyElement(&gameState->noteParentEnts);
                                        initNoteParentEnt(&gameState->commons, newEnt, &gameState->particleSystems, initPos, currentNoteParentType, stevinusTex, gameState->ID++);
                                        mostRecentParentNote = newEnt;
                                        char *at = noteBuf;
                                        while(*at) {
                                            char value = *at - 48; //get value relative to 0
                                            if(value < arrayCount(notes)) {
                                                //get new note
                                                Note *newNoteEnt = (Note *)getEmptyElement(&gameState->noteEnts);
                                                //

                                                NoteValue noteType = (NoteValue)value;
                                                Asset *noteSound = notes[noteType];

                                                int foo = newEnt->noteValueCount + 1;
                                                int across = 4;
                                                int x = foo % across;
                                                int y = foo / across;

                                                V3 notePos = v3(initPos.x + x, initPos.y + y, initPos.z);

                                                initNoteEnt(&gameState->commons, newNoteEnt, &gameState->particleSystems, notePos, noteTex, noteSound, gameState->ID++);

                                                assert(newNoteEnt->parentCount < arrayCount(newNoteEnt->parents));
                                                newNoteEnt->parents[newNoteEnt->parentCount++] = mostRecentParentNote;
                                                //assert(newNoteEnt->parent);
                                                newNoteEnt->value = noteType;
                                                newNoteEnt->e->shading = noteShades[noteType];

                                                addToUndoBufferIndex(&gameState->undoBuffer, CREATE_ENTITY, newNoteEnt->e);
                                                ChordInfo *chordInfo = newEnt->sequence + newEnt->noteValueCount++;
                                                int indexAt = chordInfo->count++;
                                                chordInfo->notes_[indexAt] = newNoteEnt;
                                                chordInfo->values_[indexAt] = newNoteEnt->value;
                                                
                                            }
                                            at++;
                                        }

                                        if(selectedEvent) {
                                            // Event_EntCommonsInfo evComInfo = {};
                                            // evComInfo.id = com->ID;
                                            // evComInfo.pos = &com->pos;

                                            newEnt->eventToTrigger = selectedEvent;//addV3PanEventWithOffset(&gameState->events, SMOOTH_STEP_01, EVENT_NULL_FLAG, v3(0.5f, 2, 0), 3, &evComInfo, gameState->eventID++);
                                        }

                                        addToUndoBufferIndex(&gameState->undoBuffer, CREATE_ENTITY, newEnt->e);
                                    } else {
                                        //TODO: Have the fade writing come up
                                        printf("%s\n", "couldn't create note parent");
                                    }

                                } break;
                                case ENTITY_TYPE_SCENARIO: {
                                    Entity *newEnt = (Entity *)getEmptyElement(&gameState->entities);
                                    initScenarioEnt(&gameState->commons, newEnt, &gameState->particleSystems, initPos, currentTex, gameState->ID++);
                                    addToUndoBufferIndex(&gameState->undoBuffer, CREATE_ENTITY, newEnt->e);
                                } break;
                                case ENTITY_TYPE_NPC: {
                                    NPC *newEnt = (NPC *)getEmptyElement(&gameState->npcEntities);
                                    initNPCEnt(&gameState->commons, &gameState->events, newEnt, &gameState->particleSystems, initPos, doorTex, 1, gameState->ID++, gameState->eventID++);
                                    addDialog(newEnt->event, (char *)"Light is the sound of nature, sound is the light of our world.\n");
                                    addDialog(newEnt->event, (char *)"The garden needs your help. You need to listen\n");

                                    Entity_Commons *com = gameState->camera;
                                    
                                    Event_EntCommonsInfo evComInfo = {};
                                    evComInfo.id = com->ID;
                                    evComInfo.pos = &com->pos;

                                    newEnt->event->nextEvent = addV3PanEventWithOffset(&gameState->events, SMOOTH_STEP_00, EVENT_NULL_FLAG, v3(2, 10, 0), 3, &evComInfo, gameState->eventID++);
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
                        interacting.e = gameState->camera;
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
                            float zAt = gameState->camera->pos.z - interacting.e->pos.z;
                            V2 newPos = v2_minus(mouseP_worldSpace, gameState->camera->pos.xy);
                            newPos = v2(newPos.x*zAt, newPos.y*zAt);
                            newPos = v2_plus(newPos, gameState->camera->pos.xy);
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
                //THIS IS THE PARTICLE SYSTEM TYPE
                if(interacting.e->particleSystem) { // == ENTITY_TYPE_PARTICLE_SYSTEM
                    ImGui::Begin("Particle System Panel");

                    particle_system *partSys = interacting.e->particleSystem;
                    particle_system_settings *set = &interacting.e->particleSystem->Set;

                    //ImGui::InputFloat("Life Span", &set->LifeSpan);
                    ImGui::InputFloat("Life Span Of System", &set->MaxLifeSpan);
                    ImGui::Checkbox("Loop", &set->Loop);
                    ImGui::Checkbox("Pressure Affected", &set->pressureAffected);
                    ImGui::Checkbox("Collides with Floor", &set->collidesWithFloor);
                    ImGui::InputInt("max particle count", (int *)&partSys->MaxParticleCount);            

                    ImGui::SliderFloat("Bitmap Scale", &set->bitmapScale, 0.2f, 10);            
                    ImGui::InputFloat2("angle start", set->angleBias.E);            
                    ImGui::InputFloat2("angle velocity", set->angleForce.E);            

                    float lifeSpan_denominator = 1.0f / partSys->creationTimer.period;
                    ImGui::InputFloat("Particle Life Span", &lifeSpan_denominator);
                    partSys->creationTimer.period = 1.0f  / lifeSpan_denominator;

                    ImGui::InputFloat3("Position", set->offsetP.E);            

                    static int particleBitmapAt = 1;
                    ImGui::ListBox("textures\n", &particleBitmapAt, texNames, texCount, 4);

                    char *name = (char *)texNames[particleBitmapAt];
                    Asset *newBitmapAsset = findAsset(name);
                    assert(newBitmapAsset);
                    
                    if (ImGui::Button("Push Bitmap")) { 
                        if(set->BitmapCount < arrayCount(set->Bitmaps)) {
                            pushParticleBitmap(set, getTextureAsset(newBitmapAsset), newBitmapAsset->name);
                        }
                    }
                    if(set->BitmapCount > 0) {
                        static int particleBitmapRemoveAt = 0;
                        
                        ImGui::ListBox("pushed bitmaps\n", &particleBitmapRemoveAt, set->BitmapNames, set->BitmapCount, 4);

                        if (ImGui::Button("Remove Bitmap")) { 
                            removeParticleBitmap(set, particleBitmapRemoveAt);
                        }
                    }

                    ImGui::InputFloat2("Min Pos", set->posBias.min.E);
                    ImGui::InputFloat2("Max Pos", set->posBias.max.E);

                    ImGui::InputFloat2("Min Vel", set->VelBias.min.E);
                    ImGui::InputFloat2("Max Vel", set->VelBias.max.E);

                    bool ReactivateSystem_ = false;
                    if(ImGui::Checkbox("Reactivate", &ReactivateSystem_)) {
                        Reactivate(interacting.e->particleSystem);
                    }
                    
                    // unsigned int BitmapCount;
                    // Texture *Bitmaps[32];
                    // unsigned int BitmapIndex;

                    //ParticleSystemType type;

                    ImGui::End();
                }
                //

                ImGui::Begin("Entity Info");
                ImGui::InputInt("ID", &interacting.e->ID);            
                ImGui::InputFloat3("Position", interacting.e->pos.E);            
                ImGui::InputFloat3("Velocity", interacting.e->dP.E);            
                ImGui::InputFloat3("Acceleration", interacting.e->ddP.E);            
                ImGui::InputFloat3("Dim", interacting.e->dim.E);            
                ImGui::InputFloat3("Render Scale", interacting.e->renderScale.E);        
                ImGui::InputFloat3("Render offset", interacting.e->renderPosOffset.E);        
                ImGui::ColorEdit4("Shading Color", interacting.e->shading.E); 

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
                        initCollisionEnt(&gameState->commons, newEnt, &gameState->particleSystems, v3_plus(interEnt->pos, offsetP), interEnt->tex, interEnt->inverseWeight, gameState->ID++);
                        newCommons = newEnt->e;
                    } else {
                        Entity *newEnt = (Entity *)getEmptyElement(&gameState->entities);
                        initEntity(&gameState->commons, newEnt, &gameState->particleSystems, v3_plus(interEnt->pos, offsetP), interEnt->tex, interEnt->inverseWeight, gameState->ID++);
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
                ImGui::Begin("SaveLoad");
                static char saveDir[256] = "level1";
                //LOAD WORLD button
                FileNameOfType folderFileNames = getDirectoryFolders(loadDir);

                static char* levelItems[64];
                static int levelCount = 0;

                for(int i = 0; i < levelCount; ++i) {
                    free(levelItems[i]);
                }

                levelCount = 0;

                for(int i = 0; i < folderFileNames.count; ++i) {
                    char *name = folderFileNames.names[i];
                    levelItems[levelCount++] = getFileLastPortion(name);
                    free(name);
                }

                static int levelItemCurrent = 0;
                ImGui::ListBox("levels\n", &levelItemCurrent, levelItems, levelCount, 4);

                char *loadDirName = (char *)levelItems[levelItemCurrent];

                if (ImGui::Button("Load Level")) { 
                    char *dirNameExe = concat(globalExeBasePath, "levels/");
                    char *dirName0 = concat(dirNameExe, loadDirName);
                    char *finalDirName = concat(dirName0, "/"); //TODO: probably don't need this 
                    loadWorld(gameState, finalDirName);
                    free(dirNameExe);
                    free(dirName0);
                    free(finalDirName);
                    memcpy(saveDir, loadDirName, sizeof(char)*strlen(loadDirName));
                }
                /////

                ////SAVE LEVEL BUTTON//
                static LerpV4 saveTimerDisplay = initLerpV4();
                static V4 saveColor = COLOR_NULL;

                
                ImGui::InputText("Save Level directory", saveDir, IM_ARRAYSIZE(saveDir));
                static bool callSaveWorld = false;
                static bool createdDirectory = false;
                static char *dirName = 0;
                char *saveFileName = "mm.txt";
                if (ImGui::Button("Save Level")) { 
                    //Maybe there could be a arena you just create, push things on then clear at the end?? instead of the concats with free _each_ string

                    char *dirNameExe = concat(globalExeBasePath, "levels/");
                    char *dirName0 = concat(dirNameExe, saveDir);
                    dirName = concat(dirName0, "/"); //make sure there is a slash at the end. 

                    createdDirectory = platformCreateDirectory(dirName);
                    if(!createdDirectory) {
                        ImGui::OpenPopup("Delete?");
                    } else {
                        callSaveWorld = true;
                    }

                    free(dirName0);
                    free(dirNameExe);
                }

                if (ImGui::BeginPopupModal("Delete?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::Text("All those beautiful files will be deleted.\nThis operation cannot be undone!\n\n");
                    ImGui::Separator();

                    static bool dont_ask_me_next_time = false;
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
                    ImGui::Checkbox("Don't ask me next time", &dont_ask_me_next_time);
                    ImGui::PopStyleVar();

                    if (ImGui::Button("OK", ImVec2(120,0))) { 
                        ImGui::CloseCurrentPopup(); 
                        char *exts[1] = {"txt"};
                        char *copyDir1 = concat(globalExeBasePath, "levelsCopy/");
                        char *copyDir = platformGetUniqueDirName(copyDir1);
                        copyAllFilesOfType(dirName, copyDir, exts, arrayCount(exts));
                        deleteAllFilesOfType(dirName, exts, arrayCount(exts));
                        free(copyDir);
                        free(copyDir1);
                        callSaveWorld = true;
                    }
                    ImGui::SetItemDefaultFocus();
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(120,0))) { ImGui::CloseCurrentPopup(); }
                    ImGui::EndPopup();
                } 

                if(callSaveWorld) {
                    saveWorld(gameState, dirName, saveFileName);
                    saveColor = COLOR_BLACK;
                    setLerpInfoV4_s(&saveTimerDisplay, COLOR_NULL, 1, &saveColor);
                    assert(isOn(&saveTimerDisplay.timer));
                    free(dirName);
                    callSaveWorld = false;
                }

                if(isOn(&saveTimerDisplay.timer)) {
                    updateLerpV4(&saveTimerDisplay, dt, SMOOTH_STEP_01);
                    outputText(&mainFontLarge, 0.4f*bufferWidth, 0.5f*bufferHeight, bufferWidth, bufferHeight, (char *)"SAVED", rect2fMinMax(0.2f*bufferWidth, 0, 0.8f*bufferWidth, bufferHeight), saveColor, 1, true);   
                }
                /////
                ImGui::End();
                //ImGui::ShowDemoWindow();

                //EVENT WINDOW
                typedef struct {
                    Event *event;
                    int index;
                } EventListBoxInfo;

                EventListBoxInfo eventListBox[128];
                char* eventListBoxNames[128];
                int eventBoxCount = 0;

                for(int eventIndex = 0; eventIndex < gameState->events.count; ++eventIndex) {
                    Event *event = (Event *)getElement(&gameState->events, eventIndex);
                    if(event) {
                        int indexToAddTo = eventBoxCount++;
                        EventListBoxInfo *ev = eventListBox + indexToAddTo;
                        ev->event = event;
                        ev->index = eventIndex;
                        char *eventName = "(null)";
                        if(strlen(event->name) > 0) {
                            eventName = event->name;
                        }
                        eventListBoxNames[indexToAddTo] = eventName;
                    }
                }

                ImGui::Begin("Events");
                static int eventListBoxCurrent = 0;
                ImGui::ListBox("Events\n", &eventListBoxCurrent, eventListBoxNames, eventBoxCount, 4);
                EventListBoxInfo *inspectEvent = eventListBox + eventListBoxCurrent;

                if (ImGui::Button("ADD NEW EVENT")) { 
                    //Create new events
                    ArrayElementInfo elmIndo = getEmptyElementWithInfo(&gameState->events);
                    inspectEvent = eventListBox + eventBoxCount++;
                    inspectEvent->event = (Event *)elmIndo.elm;
                    inspectEvent->index = elmIndo.absIndex;

                    inspectEvent->event->ID = gameState->eventID++;
                    char *str = "(null)";
                    nullTerminateBuffer(inspectEvent->event->name, str, strlen(str));
                }
                if(eventBoxCount > 0) {
                    assert(inspectEvent);
                    Event *ev = selectedEvent = inspectEvent->event;
                    ImGui::InputInt("ID", &ev->ID);            
                    ImGui::InputText("Name", ev->name, IM_ARRAYSIZE(ev->name));
                    if(ev->pos) {
                        ImGui::InputFloat3("Position", ev->pos->E);            
                    }
                    ImGui::InputInt("Ent ID", &ev->entId);            
                    ImGui::InputFloat3("dim", ev->dim.E);   

                    ImGui::RadioButton("DIALOG", (int *)(&ev->type), (int)EVENT_DIALOG);         
                    ImGui::RadioButton("V3 PAN", (int *)(&ev->type), (int)EVENT_V3_PAN);         
                    ImGui::RadioButton("ACTIVATE ENTITY", (int *)(&ev->type), (int)EVENT_ENTITY_ACTIVE);         
                    ImGui::RadioButton("EVENT_FADE_OUT", (int *)(&ev->type), (int)EVENT_FADE_OUT);         

                    if(ev->type == EVENT_DIALOG) {
                        ImGui::InputFloat("dialogTimerPeriod", &ev->dialogTimer.period);   
                        ImGui::InputFloat("dialog display value", &ev->dialogDisplayValue);   
                    } else if(ev->type == EVENT_V3_PAN) {
                        //ev->lerpType;
                    } else if(ev->type == EVENT_ENTITY_ACTIVE) {
                      //nothing to put here  
                    } else if(ev->type == EVENT_FADE_OUT) {
                      ImGui::InputFloat("fadeTimer", &ev->fadeTimer.period);   
                    }
                }

                if (ImGui::Button("DELETE")) { 
                    removeElement_ordered(&gameState->events, inspectEvent->index);
                    eventListBoxCurrent = 0;
                }
                ImGui::End();

                //

                ImGui::Begin("General");
                //ImGui::InputFloat3("Camera position", gameState->camera.pos.E);           
                ImGui::SliderFloat("Camera Z Pos", &gameState->camera->pos.z, min(-10, gameState->camera->pos.z), max(10, gameState->camera->pos.z));            
                
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
                
                ImGui::RadioButton("DEFAULT", (int *)(&entityType), (int)ENTITY_TYPE_ENTITY); ImGui::SameLine();
                ImGui::RadioButton("PARTICLE_SYSTEM", (int *)(&entityType), (int)ENTITY_TYPE_PARTICLE_SYSTEM); ImGui::SameLine();
                
                ImGui::RadioButton("COLLISION", (int *)(&entityType), (int)ENTITY_TYPE_COLLISION); ImGui::SameLine();
                ImGui::RadioButton("DOOR", (int *)(&entityType), (int)ENTITY_TYPE_DOOR); ImGui::SameLine();
                ImGui::RadioButton("PLATFORM", (int *)(&entityType), (int)ENTITY_TYPE_PLATFORM); ImGui::SameLine();
                if(mostRecentParentNote) {
                    ImGui::RadioButton("NOTE", (int *)(&entityType), (int)ENTITY_TYPE_NOTE); 
                }
                ImGui::RadioButton("NOTE_PARENT", (int *)(&entityType), (int)ENTITY_TYPE_NOTE_PARENT); 
                ImGui::RadioButton("SCENARIO ITEM", (int *)(&entityType), (int)ENTITY_TYPE_SCENARIO); 
                ImGui::RadioButton("NPC", (int *)(&entityType), (int)ENTITY_TYPE_NPC); 
                ImGui::RadioButton("Light", (int *)(&entityType), (int)ENTITY_TYPE_LIGHT); 
                

                if(entityType == (int)ENTITY_TYPE_COLLISION) {
                    ImGui::InputFloat("Init Inverse Weight", &initWeight);            
                }

                if(entityType == (int)ENTITY_TYPE_PLATFORM) {
                    ImGui::RadioButton("PLATFORM_LINEAR_X", (int *)(&platformType), (int)PLATFORM_LINEAR_X); ImGui::SameLine();
                    ImGui::RadioButton("PLATFORM_LINEAR_Y", (int *)(&platformType), (int)PLATFORM_LINEAR_Y); ImGui::SameLine();
                    ImGui::RadioButton("PLATFORM_CIRCLE", (int *)(&platformType), (int)PLATFORM_CIRCLE); ImGui::SameLine();
                    ImGui::RadioButton("PLATFORM_FIGURE_OF_EIGHT", (int *)(&platformType), (int)PLATFORM_FIGURE_OF_EIGHT);
                }

                static V4 cachedShade = COLOR_WHITE;
                static bool resetShadeColor = false;
                static NoteParent *cachedParent = mostRecentParentNote;

                if(cachedParent != mostRecentParentNote) {
                    assert(mostRecentParentNote);
                    if(cachedParent) { cachedParent->e->shading = cachedShade; }
                    cachedParent = mostRecentParentNote;
                }

                if(entityType != (int)ENTITY_TYPE_NOTE && mostRecentParentNote) {
                    if(resetShadeColor) {
                        mostRecentParentNote->e->shading = cachedShade;
                    } 
                    cachedShade = mostRecentParentNote->e->shading;

                    resetShadeColor = false;
                } 

                // if(entityType == (int)ENTITY_TYPE_NOTE) {
                //     assert(mostRecentParentNote);

                //     mostRecentParentNote->e->shading = COLOR_RED;
                //     ImGui::RadioButton("C NOTE", (int *)(&noteType), (int)C_NOTE); ImGui::SameLine();
                //     ImGui::RadioButton("D NOTE", (int *)(&noteType), (int)D_NOTE); ImGui::SameLine();
                //     ImGui::RadioButton("D sharp NOTE", (int *)(&noteType), (int)D_SHARP_NOTE); ImGui::SameLine();
                //     ImGui::RadioButton("E NOTE", (int *)(&noteType), (int)E_NOTE); ImGui::SameLine();
                //     ImGui::RadioButton("F NOTE", (int *)(&noteType), (int)F_NOTE); 
                //     ImGui::RadioButton("G NOTE", (int *)(&noteType), (int)G_NOTE); ImGui::SameLine();
                //     ImGui::RadioButton("A NOTE", (int *)(&noteType), (int)A_NOTE); ImGui::SameLine();
                //     ImGui::RadioButton("B NOTE", (int *)(&noteType), (int)B_NOTE); ImGui::SameLine();

                //     resetShadeColor = true;
                // }

                if(entityType == (int)ENTITY_TYPE_NOTE_PARENT) {
                    ImGui::InputText("note parent values", noteBuf, IM_ARRAYSIZE(noteBuf));
                    for(int i = 0; i < arrayCount(NoteParentTypeStrings); ++i) {
                        ImGui::RadioButton(NoteParentTypeStrings[i], (int *)(&currentNoteParentType), (int)NoteParentTypeValues[i]); 
                    }
                }
                
                static int listbox_item_current = 1;
                ImGui::ListBox("textures\n", &listbox_item_current, texNames, texCount, 4);

                char *name = (char *)texNames[listbox_item_current];
                Asset *asset = findAsset(name);
                assert(asset);
                if(!asset) {
                    //TODO: Make the directory hot load textures
                    //asset = loadImageAsset(name);
                }
                currentTex = asset;

                //TODO: Maybe there could be a arena you just create, push things on then clear at the end?? instead of the concats with free _each_ string

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
            } //This is if the playmode state is active
            drawRenderGroup(&globalRenderGroup);
#if MULTI_SAMPLE
            if (MultiSample) {
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, finalCompositedBuffer.bufferId);
                glCheckError();
                glBindFramebuffer(GL_READ_FRAMEBUFFER, compositedBufferMultiSampled.bufferId); 
                glCheckError();
                glBlitFramebuffer(0, 0, bufferWidth, bufferHeight, 0, 0, bufferWidth, bufferHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
                glCheckError();
            }
                        ///
#endif      
            static float lightT = 0;
            lightT += dt;
            static GLBufferHandles lightBackgroundHandle = {};
            if(menuInfo.gameMode == PLAY_MODE) {
                //DO LIGHTING HERE
                glDisable(GL_DEPTH_TEST);
                renderDisableDepthTest(&globalRenderGroup); 
                //glBlendEquation(GL_MAX);

                glBindFramebuffer(GL_FRAMEBUFFER, lightFrameBuffer.bufferId); //go to final composite buffer
                setFrameBufferId(&globalRenderGroup, lightFrameBuffer.bufferId);
                 
                float darknessAlpha = 1.0f;
                V4 backgroundColor = v4(0, 0, 0, darknessAlpha);

                // glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
                // glStencilMask(0x00);
                
                //openGlDrawRectCenterDim(&lightBackgroundHandle, v3(0.5f*bufferWidth, 0.5f*bufferHeight, gameState->camera->pos.z - 0.1f), v2(bufferWidth, bufferHeight), backgroundColor, 0, mat4TopLeftToBottomLeft(bufferHeight), 1, OrthoMatrixToScreen(bufferWidth, bufferHeight, 1));                

                //////Keep the  background color and don't add the light Color, will the alpha, belnd the alpha but don't add the source alpha since it will accumulate. Want to dilute the alpha. 
                
                glBlendFuncSeparate(GL_ZERO, GL_ONE, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
                setBlendFuncType(&globalRenderGroup, BLEND_FUNC_ZERO_ONE_ZERO_ONE_MINUS_ALPHA);
                //

                // glStencilFunc(GL_ALWAYS, 1, 0xFF);
                // glStencilMask(0xFF);
                for(int lightIndex = 0; lightIndex < gameState->lights.count; ++lightIndex) {
                    Light *ent = (Light *)getElement(&gameState->lights, lightIndex);
                    if(ent) {
                        float addOn = 0.4f*sin(3*lightT);
                        V3 lightDim = v3(ent->flux + addOn, ent->flux + addOn, 0);
                        RenderInfo renderInfo = calculateRenderInfo(ent->e->pos, lightDim, gameState->camera->pos, metresToPixels);
                        openGlDrawLight(&ent->e->bufferHandles, renderInfo.pos, lightDim.xy, COLOR_BLACK, mat4(), 1, renderInfo.pvm, projectionMatrixToScreen(bufferWidth, bufferHeight));
                    }
                }

                //glBlendEquation(GL_FUNC_ADD);
                glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                setBlendFuncType(&globalRenderGroup, BLEND_FUNC_STANDARD);

                glEnable(GL_DEPTH_TEST);
                renderEnableDepthTest(&globalRenderGroup);
            } else {

                glBindFramebuffer(GL_FRAMEBUFFER, lightFrameBuffer.bufferId); //go to final composite buffer
                setFrameBufferId(&globalRenderGroup, lightFrameBuffer.bufferId);
                renderDisableDepthTest(&globalRenderGroup);
                glDisable(GL_DEPTH_TEST);
                static float menuT = 0;
                menuT += dt;
                float lightWidth = bufferHeight*lerp(0.95f, inverse_lerp(-1, sin(3*menuT), 1.6f), 1.4f);
                
                openGlDrawRectCenterDim(&lightBackgroundHandle, v3(0.5f*bufferWidth, 0.5f*bufferHeight, gameState->camera->pos.z - 0.1f), v2(bufferWidth, bufferHeight), v4(0, 0, 0, 0.9f), 0, mat4TopLeftToBottomLeft(bufferHeight), 1, OrthoMatrixToScreen(bufferWidth, bufferHeight, 1));                
                setBlendFuncType(&globalRenderGroup, BLEND_FUNC_ZERO_ONE_ZERO_ONE_MINUS_ALPHA);
                glBlendFuncSeparate(GL_ZERO, GL_ONE, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);

                static GLBufferHandles menuLightHandle = {};
                openGlDrawLight(&menuLightHandle, v3(0.5f*bufferWidth, 0.5f*bufferHeight, -1), v2(lightWidth, lightWidth), COLOR_BLACK, mat4(), 1, mat4(), OrthoMatrixToScreen(bufferWidth, bufferHeight, 1));
                glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                setBlendFuncType(&globalRenderGroup, BLEND_FUNC_STANDARD);
                glEnable(GL_DEPTH_TEST);
                renderEnableDepthTest(&globalRenderGroup);
            }

            setFrameBufferId(&globalRenderGroup, finalCompositedBuffer.bufferId);
            glBindFramebuffer(GL_FRAMEBUFFER, finalCompositedBuffer.bufferId); //go to final composite buffer
            static GLBufferHandles lightBufferHandles = {};
            openGlTextureCentreDim(&lightBufferHandles, lightFrameBuffer.textureId, v2ToV3(middleP, -1), v2(bufferWidth, bufferHeight), COLOR_WHITE, 0, mat4(), 1, mat4(), OrthoMatrixToScreen(bufferWidth, bufferHeight, 1));

                

                        
            drawRenderGroup(&globalRenderGroup);

//Just blit the texture instead of blending with shader. 
            float screenRatio =  screenDim.x / bufferWidth;
            float h1 = bufferHeight * screenRatio;
            if(h1 > screenDim.y) {
                screenRatio =  screenDim.y / bufferHeight;
                float w1 = bufferWidth * screenRatio;
                assert(w1 <= screenDim.x);
            }

            float screenX = screenRatio*bufferWidth;
            float screenY = screenRatio*bufferHeight;
            assert(screenX <= screenDim.x);
            assert(screenY <= screenDim.y);

            float wResidue = (screenDim.x - screenX) / 2.0f;
            float yResidue = (screenDim.y - screenY) / 2.0f;

            glViewport(0, 0, screenDim.x, screenDim.y);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glCheckError();
            glBindFramebuffer(GL_READ_FRAMEBUFFER, finalCompositedBuffer.bufferId); 
            glCheckError();
            glBlitFramebuffer(0, 0, bufferWidth, bufferHeight, wResidue, yResidue, screenDim.x - wResidue, screenDim.y - yResidue, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            glCheckError();                    
            glViewport(0, 0, bufferWidth, bufferHeight);
            

            unsigned int now = SDL_GetTicks();
            float timeInFrameSeconds = (now - lastTime);
            //TODO: Do our own wait if Vsync isn't on. 
            //NOTE: This is us choosing the best frame time within the intervals of possible frame rates!!!
            dt = timeInFrameSeconds / 1000;
            float frameRates[] = {idealFrameTime*1.0f, idealFrameTime*2.0f, idealFrameTime*4.0f};
            float smallestDiff = 0;
            bool set = false;
            float newRate = idealFrameTime;
            for(int i = 0; i < arrayCount(frameRates); ++i) {
                float val = frameRates[i];
                float diff = dt - val;
                if(diff < 0.0f) {
                    diff *= -1;
                }
                if(diff < smallestDiff || !set) {
                    set = true;
                    smallestDiff = diff;
                    newRate = val;
                }
            }
            dt = newRate;
            assert(dt <= frameRates[2]);
            /////


#if DEVELOPER_MODE
            if(debugUI_isOn) 
            {
                // char secondsInFrameString[256] = {};
                // sprintf(secondsInFrameString, "%f\n", 1.0f / timeInFrameSeconds);
                // globalFontImmediate = true;
                // outputText(&mainFontLarge, 0.1f*bufferWidth, 0.1f*bufferHeight, bufferWidth, bufferHeight, secondsInFrameString, rect2fMinMax(0.2f*bufferWidth, 0, 0.8f*bufferWidth, bufferHeight), COLOR_GREEN, 1, true);   
                // globalFontImmediate = false;
                // drawRenderGroup(&globalRenderGroup);
            }
#endif
            
#if IMGUI_ON
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            ImGui::Render();
            ImGui_ImplSdlGL3_RenderDrawData(ImGui::GetDrawData());
#endif
            updateChannelVolumes(dt);



            
            lastTime = SDL_GetTicks();
            
            SDL_GL_SwapWindow(windowHandle);
            
        }
        //ImGui::DestroyContext();

        easyJoyStickCloseControllers();

        SDL_GL_DeleteContext(renderContext);
        SDL_DestroyWindow(windowHandle);
        SDL_Quit();

    }
        
}