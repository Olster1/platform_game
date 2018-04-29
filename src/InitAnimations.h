        addAsset_((char *)"knight animation", &gameState->KnightAnimations);

        char *BaseName = (char *)"knight/knight iso char_";
        //char *BaseName = "knight iso char_";
        
        
#define FILE_EXTENSION (char *)".png"
#define CREATE_NAME(Append) CREATE_NAME_(#Append)
#define CREATE_NAME_(Append) concat(concat(globalExeBasePath, (char *)BaseName), concat((char *)Append, FILE_EXTENSION))
        
        {
            char *FileNames[] = {
                CREATE_NAME(idle_0),
                CREATE_NAME(idle_1),
                CREATE_NAME(idle_2),
                CREATE_NAME(idle_3),
            };
            InitAnimation(&gameState->KnightAnimations.anim[gameState->KnightAnimations.count++], FileNames, arrayCount(FileNames), 0.75f*TAU32, (char *)"Knight_Idle", ANIM_IDLE);
        }
        
        {
            char *FileNames[] = {
                CREATE_NAME(run down_0),
                CREATE_NAME(run down_1),
                CREATE_NAME(run down_2),
                CREATE_NAME(run down_3),
                CREATE_NAME(run down_4)
            };
            InitAnimation(&gameState->KnightAnimations.anim[gameState->KnightAnimations.count++], FileNames, arrayCount(FileNames), 0.75f*TAU32, (char *)"Knight_Run_Down", ANIM_DOWN);
        }
        {
            char *FileNames[] = {
                CREATE_NAME(run up_0),
                CREATE_NAME(run up_1),
                CREATE_NAME(run up_2),
                CREATE_NAME(run up_3),
                CREATE_NAME(run up_4),
            };
            
            InitAnimation(&gameState->KnightAnimations.anim[gameState->KnightAnimations.count++], FileNames, arrayCount(FileNames), 0.25f*TAU32, (char *)"Knight_Run_Up", ANIM_UP);
        }
        {
            char *FileNames[] = {
                CREATE_NAME(run left_0),
                CREATE_NAME(run left_1),
                CREATE_NAME(run left_2),
                CREATE_NAME(run left_3),
                CREATE_NAME(run left_4),
                CREATE_NAME(run left_5),
            };
            
            InitAnimation(&gameState->KnightAnimations.anim[gameState->KnightAnimations.count++], FileNames, arrayCount(FileNames), 0.5f*TAU32, (char *)"Knight_Run_Left", ANIM_LEFT);
        }
        {
            char *FileNames[] = {
                CREATE_NAME(run right_0),
                CREATE_NAME(run right_1),
                CREATE_NAME(run right_2),
                CREATE_NAME(run right_3),
                CREATE_NAME(run right_4),
                CREATE_NAME(run right_5),
            };
            
            InitAnimation(&gameState->KnightAnimations.anim[gameState->KnightAnimations.count++], FileNames, arrayCount(FileNames), 0*TAU32, (char *)"Knight_Run_Right", ANIM_RIGHT);
        }
        addAsset_((char *)"chubby man animation", &gameState->ManAnimations);
        BaseName = (char *)"chubby_iso_character/idle_";
        {
            char *FileNames[] = {
                CREATE_NAME(idle_0),
                CREATE_NAME(idle_1),
                CREATE_NAME(idle_2),
                CREATE_NAME(idle_3),
            };
            InitAnimation(&gameState->ManAnimations.anim[gameState->ManAnimations.count++], FileNames, arrayCount(FileNames), 0.75f*TAU32, (char *)"Man_Idle", ANIM_IDLE);
        }
        
        {
            char *FileNames[] = {
                CREATE_NAME(walk forward_0),
                CREATE_NAME(walk forward_1),
                CREATE_NAME(walk forward_2),
                CREATE_NAME(walk forward_3)
            };
            
            InitAnimation(&gameState->ManAnimations.anim[gameState->ManAnimations.count++], FileNames, arrayCount(FileNames), 0.75f*TAU32, (char *)"Man_Run_Down", ANIM_DOWN);
        }
        {
            char *FileNames[] = {
                CREATE_NAME(walk up_0),
                CREATE_NAME(walk up_1),
                CREATE_NAME(walk up_2)
            };
            
            InitAnimation(&gameState->ManAnimations.anim[gameState->ManAnimations.count++], FileNames, arrayCount(FileNames), 0.25f*TAU32, (char *)"Man_Run_Up", ANIM_UP);
        }
        {
            char *FileNames[] = {
                CREATE_NAME(walk left_0),
                CREATE_NAME(walk left_1),
                CREATE_NAME(walk left_2),
                CREATE_NAME(walk left_3)
            };
            
            InitAnimation(&gameState->ManAnimations.anim[gameState->ManAnimations.count++], FileNames, arrayCount(FileNames), 0.5f*TAU32, (char *)"Man_Run_Left", ANIM_LEFT);
        }
        {
            char *FileNames[] = {
                CREATE_NAME(walk right_0),
                CREATE_NAME(walk right_1),
                CREATE_NAME(walk right_2),
                CREATE_NAME(walk right_3)
            };
            
            InitAnimation(&gameState->ManAnimations.anim[gameState->ManAnimations.count++], FileNames, arrayCount(FileNames), 0*TAU32, (char *)"Man_Run_Right", ANIM_RIGHT);
        }