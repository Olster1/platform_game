typedef enum {
    MENU_MODE,
    PAUSE_MODE,
    PLAY_MODE,
    LOAD_MODE,
    SAVE_MODE,
    QUIT_MODE,
    DIED_MODE,
    SETTINGS_MODE
} GameMode;

typedef struct {
    char *options[32];
    int count;
} MenuOptions;

typedef struct {
    int menuCursorAt;
    bool *running;
    GameMode gameMode;
    GameMode lastMode;
    Font *font;
    //TODO: Make this platform independent
    SDL_Window *windowHandle;

    V2 lastMouseP;
} MenuInfo;

bool updateMenu(MenuOptions *menuOptions, GameButton *gameButtons, MenuInfo *info, Arena *arenaForSounds, WavFile *moveSound) {
    bool active = true;
    if(wasPressed(gameButtons, BUTTON_DOWN)) {
        active = false;
        playMenuSound(arenaForSounds, moveSound, 0, AUDIO_BACKGROUND);
        info->menuCursorAt++;
        if(info->menuCursorAt >= menuOptions->count) {
            info->menuCursorAt = 0;
        }
    } 
    
    if(wasPressed(gameButtons, BUTTON_UP)) {
        active = false;
        playMenuSound(arenaForSounds, moveSound, 0, AUDIO_BACKGROUND);
        info->menuCursorAt--;
        if(info->menuCursorAt < 0) {
            info->menuCursorAt = menuOptions->count - 1;
        }
    }
    return active;
}

void renderMenu(GLuint backgroundTexId, MenuOptions *menuOptions, MenuInfo *info, Lerpf *sizeTimers, float dt, float screenRelativeSize, V2 mouseP, bool mouseActive) {
    
    char *titleAt = menuOptions->options[info->menuCursorAt];
        
    static GLBufferHandles backgroundMenuRenderHandles = {};
    glDisable(GL_DEPTH_TEST);
    renderDisableDepthTest(&globalRenderGroup);
    openGlTextureCentreDim(&backgroundMenuRenderHandles, backgroundTexId, v3(0.5f*bufferWidth, 0.5f*bufferHeight, -1), v2(bufferWidth, bufferHeight), COLOR_WHITE, 0, mat4(), 1, OrthoMatrixToScreen(bufferWidth, bufferHeight, 1), mat4());
    renderEnableDepthTest(&globalRenderGroup);
    glEnable(GL_DEPTH_TEST);

    float yIncrement = bufferHeight / (menuOptions->count + 1);
    Rect2f menuMargin = rect2f(0, 0, bufferWidth, bufferHeight);
    
    float xAt_ = (bufferWidth / 2);
    float yAt = yIncrement;
    static float dtValue = 0;
    dtValue += dt;

    for(int menuIndex = 0;
        menuIndex < menuOptions->count;
        ++menuIndex) {
        
        
        float fontSize = screenRelativeSize;//mapValue(sin(dtValue), -1, 1, 0.7f, 1.2f);
        if(sizeTimers) {
            // updateLerpf(sizeTimers[menuIndex], dt, LINEAR);
            fontSize = sizeTimers[menuIndex].value;
        }

        char *title = menuOptions->options[menuIndex];
        float xAt = xAt_ - (getBounds(title, menuMargin, info->font, fontSize).x / 2);


        Rect2f outputDim = outputText(info->font, xAt, yAt, bufferWidth, bufferHeight, title, menuMargin, COLOR_WHITE, fontSize, false);
        //spread across screen so we hit is more easily
        outputDim.min.x = 0;
        outputDim.max.x = bufferWidth;
        //
        if(inBounds(mouseP, outputDim, BOUNDS_RECT) && mouseActive) {
            info->menuCursorAt = menuIndex;
        }

        V4 menuItemColor = COLOR_BLUE;
        
        if(menuIndex == info->menuCursorAt) {
            menuItemColor = COLOR_RED;
        }
        
        outputText(info->font, xAt, yAt, bufferWidth, bufferHeight, title, menuMargin, menuItemColor, fontSize, true);
        
        

        yAt += yIncrement;
    }
}

bool drawMenu(GameState *gameState, char *loadDir, MenuInfo *info, GameButton *gameButtons, Texture *backgroundTex, WavFile *submitSound, WavFile *moveSound, float dt, float screenRelativeSize, V2 mouseP) {
    bool isPlayMode = false;
    MenuOptions menuOptions = {};


    if(wasPressed(gameButtons, BUTTON_ESCAPE)) {
        if(info->gameMode == PLAY_MODE) {
            info->gameMode = PAUSE_MODE;
            info->lastMode = PLAY_MODE;    
        } else {
            info->lastMode = info->gameMode;    
            info->gameMode = PLAY_MODE;
        }
    }
    bool mouseActive = false;
    bool changeMenuKey = wasPressed(gameButtons, BUTTON_ENTER) || wasPressed(gameButtons, BUTTON_LEFT_MOUSE);

    bool mouseChangedPos = !(info->lastMouseP.x == mouseP.x && info->lastMouseP.y == mouseP.y);

    //These are out of the switch scope because we have to render the menu before we release all the memory, so we do it at the end of the function
    InfiniteAlloc tempTitles = initInfinteAlloc(char *); 
    InfiniteAlloc tempStrings = initInfinteAlloc(char *);
    //
    
    Lerpf *thisSizeTimers = 0;
    switch(info->gameMode) {
        case LOAD_MODE:{
            

            FileNameOfType folderFileNames = getDirectoryFolders(loadDir);
            for(int folderIndex = 0; folderIndex < folderFileNames.count; folderIndex++) {
                char *folderName = folderFileNames.names[folderIndex];
                char *title = getFileLastPortion(folderName);
                addElementInifinteAlloc_(&tempTitles, &title);
                assert(tempTitles.count > 0);
                char **titlePtr = getElementFromAlloc(&tempTitles, folderIndex, char *);
                assert(titlePtr[0]);
                char *timeStamp = titlePtr[0];
                char *endStr;
                time_t val = (time_t)strtoul(timeStamp, &endStr, 10);
                
                char *formattedTime = (char *)calloc(sizeof(char)*256, 1);
                addElementInifinteAlloc_(&tempStrings, &formattedTime);

                struct tm* tm_info = localtime(&val); //convert to local time 
                sprintf(formattedTime, "%s\n", asctime(tm_info));
                //strftime(formattedTime, 26, "%Y-%m-%d %H:%M:%S", tm_info);
                
                menuOptions.options[menuOptions.count++] = formattedTime; 
                free(folderName);
            }

            menuOptions.options[menuOptions.count++] = "Go Back";

            mouseActive = updateMenu(&menuOptions, gameButtons, info, &gameState->longTermArena, moveSound);
            
            if(changeMenuKey) {
                playMenuSound(&gameState->longTermArena, submitSound, 0, AUDIO_BACKGROUND);
                if (info->menuCursorAt == menuOptions.count - 1) {
                    info->gameMode = info->lastMode;
                    info->menuCursorAt = 0;
                } else {
                    assert(info->menuCursorAt >= 0 && info->menuCursorAt < (menuOptions.count - 1));
                    char **toLoad = getElementFromAlloc(&tempTitles, info->menuCursorAt, char *);

                    loadLevelFromFile(gameState, loadDir, toLoad[0]);
                    info->gameMode = info->lastMode;
                    info->menuCursorAt = 0;
                } 
                
                info->lastMode = LOAD_MODE;
            }

        } break;
        case QUIT_MODE:{
            menuOptions.options[menuOptions.count++] = "Really Quit?";
            menuOptions.options[menuOptions.count++] = "Go Back";
            
            mouseActive = updateMenu(&menuOptions, gameButtons, info, &gameState->longTermArena, moveSound);
            
            if(changeMenuKey) {
                playMenuSound(&gameState->longTermArena, submitSound, 0, AUDIO_BACKGROUND);
                switch (info->menuCursorAt) {
                    case 0: {
                        *info->running = false;
                    } break;
                    case 1: {
                        info->gameMode = info->lastMode;
                        info->menuCursorAt = 0;
                    } break;
                }
                info->lastMode = QUIT_MODE;
            }
            
        } break;
        case DIED_MODE:{
            menuOptions.options[menuOptions.count++] = "Really Quit?";
            menuOptions.options[menuOptions.count++] = "Go Back";
            
            mouseActive = updateMenu(&menuOptions, gameButtons, info, &gameState->longTermArena, moveSound);
            
            if(changeMenuKey) {
                playMenuSound(&gameState->longTermArena, submitSound, 0, AUDIO_BACKGROUND);
                switch (info->menuCursorAt) {
                    case 0: {
                        *info->running = false;
                    } break;
                    case 1: {
                        info->gameMode = info->lastMode;
                        info->menuCursorAt = 0;
                    } break;
                }
                info->lastMode = DIED_MODE;
            }
        } break;
        case SAVE_MODE:{
            menuOptions.options[menuOptions.count++] = "Save Progress";
            menuOptions.options[menuOptions.count++] = "Go Back";
            
            mouseActive = updateMenu(&menuOptions, gameButtons, info, &gameState->longTermArena, moveSound);
            
            if(changeMenuKey) {
                playMenuSound(&gameState->longTermArena, submitSound, 0, AUDIO_BACKGROUND);
                switch (info->menuCursorAt) {
                    case 0: {
                        char *dirName0 = concat(globalExeBasePath, "progress/");
                        char *dirName1 = platformGetUniqueDirName(dirName0);
                        bool createdDirectory = platformCreateDirectory(dirName1);
                        //NOTE: we can do this since we are using time stamps. If we change to overwritting exisiting files we will fail. 
                        assert(createdDirectory);
                        saveWorld(gameState, dirName1, "player_progress.txt");

                        free(dirName1);
                        free(dirName0);
                    } break;
                    case 1: {
                        info->gameMode = info->lastMode;
                        info->menuCursorAt = 1;
                        info->lastMode = SAVE_MODE;
                    } break;
                }
            }
        } break;
        case PAUSE_MODE:{
            menuOptions.options[menuOptions.count++] = "Resume";
            menuOptions.options[menuOptions.count++] = "Save";
            menuOptions.options[menuOptions.count++] = "Load";
            menuOptions.options[menuOptions.count++] = "Settings";
            menuOptions.options[menuOptions.count++] = "Quit";
            
            mouseActive = updateMenu(&menuOptions, gameButtons, info, &gameState->longTermArena, moveSound);
            
            if(changeMenuKey) {
                playMenuSound(&gameState->longTermArena, submitSound, 0, AUDIO_BACKGROUND);
                switch (info->menuCursorAt) {
                    case 0: {
                        info->gameMode = PLAY_MODE;
                    } break;
                    case 1: {
                        info->gameMode = SAVE_MODE;
                    } break;
                    case 2: {
                        info->gameMode = LOAD_MODE;
                    } break;
                    case 3: {
                        info->gameMode = SETTINGS_MODE;
                    } break;
                    case 4: {
                        info->gameMode = QUIT_MODE;
                    } break;
                }
                info->menuCursorAt = 0;
                info->lastMode = PAUSE_MODE;
            }
        } break;
        case SETTINGS_MODE:{
            unsigned int windowFlags = SDL_GetWindowFlags(info->windowHandle);
            
            bool isFullScreen = (windowFlags & SDL_WINDOW_FULLSCREEN);
            char *fullScreenOption = isFullScreen ? (char *)"Exit Full Screen" : (char *)"Full Screen";
            char *soundOption = globalSoundOn ? (char *)"Turn Off Sound" : (char *)"Turn On Sound";
            menuOptions.options[menuOptions.count++] = fullScreenOption;
            menuOptions.options[menuOptions.count++] = soundOption;
            menuOptions.options[menuOptions.count++] = "Go Back";
            
            mouseActive = updateMenu(&menuOptions, gameButtons, info, &gameState->longTermArena, moveSound);
            
            if(changeMenuKey) {
                playMenuSound(&gameState->longTermArena, submitSound, 0, AUDIO_BACKGROUND);
                switch (info->menuCursorAt) {
                    case 0: {
                        if(isFullScreen) {
                            // TODO(Oliver): Change this to handle resolution change. Have to query the current window size to adjust screen size. 
                            if(SDL_SetWindowFullscreen(info->windowHandle, false) < 0) {
                                printf("couldn't un-set to full screen\n");
                            }
                        } else {
                            if(SDL_SetWindowFullscreen(info->windowHandle, SDL_WINDOW_FULLSCREEN) < 0) {
                                printf("couldn't set to full screen\n");
                            }
                        }
                    } break;
                    case 1: {
                        globalSoundOn = !globalSoundOn;
                    } break;
                    case 2: {
                        info->gameMode = info->lastMode;
                        info->lastMode = SETTINGS_MODE;
                    } break;

                }
                info->menuCursorAt = 0;
            }
        } break;
        case MENU_MODE:{
            menuOptions.options[menuOptions.count++] = "Play";
            menuOptions.options[menuOptions.count++] = "Load";
            menuOptions.options[menuOptions.count++] = "Quit";
            
            mouseActive = updateMenu(&menuOptions, gameButtons, info, &gameState->longTermArena, moveSound);
            
            // NOTE(Oliver): Main Menu action options
            if(changeMenuKey) {
                playMenuSound(&gameState->longTermArena, submitSound, 0, AUDIO_BACKGROUND);
                switch (info->menuCursorAt) {
                    case 0: {
                        info->gameMode = PLAY_MODE;
                    } break;
                    case 1: {
                        info->gameMode = LOAD_MODE;
                    } break;
                    case 2: {
                        info->gameMode = QUIT_MODE;
                    } break;
                }
                info->lastMode = MENU_MODE;
                
                info->menuCursorAt = 0;
            }
        } break;
        case PLAY_MODE: {
            isPlayMode = true;
            setSoundType(AUDIO_FLAG_MAIN);
        } break;
    } 

    if(!isPlayMode) {
        renderMenu(backgroundTex->id, &menuOptions, info, thisSizeTimers, dt, screenRelativeSize, mouseP, mouseChangedPos);
        setSoundType(AUDIO_FLAG_MENU);
    }

    for(int i = 0; i < tempTitles.count; ++i) {
        char **titlePtr = getElementFromAlloc(&tempTitles, i, char *);
        free(*titlePtr);

        char **formatStringPtr = getElementFromAlloc(&tempStrings, i, char *);
        free(*formatStringPtr);
    }
    releaseInfiniteAlloc(&tempTitles);
    releaseInfiniteAlloc(&tempStrings);


    info->lastMouseP = mouseP;

    return isPlayMode;
}
