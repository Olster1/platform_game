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
} MenuInfo;

void updateMenu(MenuOptions *menuOptions, GameButton *gameButtons, MenuInfo *info) {
    if(wasPressed(gameButtons, BUTTON_DOWN)) {
        info->menuCursorAt++;
        if(info->menuCursorAt >= menuOptions->count) {
            info->menuCursorAt = 0;
        }
    } 
    
    if(wasPressed(gameButtons, BUTTON_UP)) {
        info->menuCursorAt--;
        if(info->menuCursorAt < 0) {
            info->menuCursorAt = menuOptions->count - 1;
        }
    }
}

void renderMenu(GLuint backgroundTexId, MenuOptions *menuOptions, MenuInfo *info) {
    
    char *titleAt = menuOptions->options[info->menuCursorAt];
    
    //openGlTextureCentreDim(backgroundTexId, v2(0.5f*bufferWidth, 0.5f*bufferHeight), v2(bufferWidth, bufferHeight), v4(1, 1, 1, 1), 0, mat4());
    float yIncrement = bufferHeight / (menuOptions->count + 1);
    Rect2f menuMargin = rect2f(0, 0, bufferWidth, bufferHeight);
    
    float xAt_ = (bufferWidth / 2);
    float yAt = yIncrement;
    for(int menuIndex = 0;
        menuIndex < menuOptions->count;
        ++menuIndex) {
        V4 menuItemColor = COLOR_BLUE;
        
        if(menuIndex == info->menuCursorAt) {
            menuItemColor = COLOR_RED;
        }
        
        char *title = menuOptions->options[menuIndex];
        float xAt = xAt_ - (getBounds(title, menuMargin, info->font).x / 2);
        outputText(info->font, xAt, yAt, bufferWidth, bufferHeight, title, menuMargin, menuItemColor, 1, true);
        yAt += yIncrement;
    }
}

bool drawMenu(MenuInfo *info, GameButton *gameButtons, Texture *backgroundTex) {
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

    switch(info->gameMode) {
        case LOAD_MODE:{
            menuOptions.options[menuOptions.count++] = "Go Back";
            
            updateMenu(&menuOptions, gameButtons, info);
            
            if(wasPressed(gameButtons, BUTTON_ENTER)) {
                switch (info->menuCursorAt) {
                    case 0: {
                        info->gameMode = info->lastMode;
                        info->menuCursorAt = 0;
                    } break;
                }
                info->lastMode = LOAD_MODE;
            }
        } break;
        case QUIT_MODE:{
            menuOptions.options[menuOptions.count++] = "Really Quit?";
            menuOptions.options[menuOptions.count++] = "Go Back";
            
            updateMenu(&menuOptions, gameButtons, info);
            
            if(wasPressed(gameButtons, BUTTON_ENTER)) {
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
            
            updateMenu(&menuOptions, gameButtons, info);
            
            if(wasPressed(gameButtons, BUTTON_ENTER)) {
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
            
            updateMenu(&menuOptions, gameButtons, info);
            
            if(wasPressed(gameButtons, BUTTON_ENTER)) {
                switch (info->menuCursorAt) {
                    case 0: {
                        //savePlayerData(table, "playerData.txt", currentGameState);
                        //saveProgress(gameState, char *dir, char *fileName);
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
            
            updateMenu(&menuOptions, gameButtons, info);
            
            if(wasPressed(gameButtons, BUTTON_ENTER)) {
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
            menuOptions.options[menuOptions.count++] = fullScreenOption;
            menuOptions.options[menuOptions.count++] = "Go Back";
            
            updateMenu(&menuOptions, gameButtons, info);
            
            if(wasPressed(gameButtons, BUTTON_ENTER)) {
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
            
            updateMenu(&menuOptions, gameButtons, info);
            
            // NOTE(Oliver): Main Menu action options
            if(wasPressed(gameButtons, BUTTON_ENTER)) {
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
        } break;
    } 

    if(!isPlayMode) {
        renderMenu(backgroundTex->id, &menuOptions, info);
    }

    return isPlayMode;
}
