typedef struct {
    GameButton buttons[BUTTON_COUNT];
    float xAxis;
    float yAxis;    
} GameController;

#define MAX_CONTROLLERS 4
SDL_GameController *ControllerHandles[MAX_CONTROLLERS];
GameController globalControllerButtons[MAX_CONTROLLERS];
static int globalAvailableControllers = 0;

void easyJoyStickOpenControllers() {
    int MaxJoysticks = SDL_NumJoysticks();
    int ControllerIndex = 0;
    for(int JoystickIndex = 0; JoystickIndex < MaxJoysticks; ++JoystickIndex)
    {
        if (!SDL_IsGameController(JoystickIndex)) {
            continue;
        }
        if (ControllerIndex >= MAX_CONTROLLERS)
        {
            break;
        }
        ControllerHandles[ControllerIndex] = SDL_GameControllerOpen(JoystickIndex);
        ControllerIndex++;
    }
}

void easyJoyStickCloseControllers() {
    for(int ControllerIndex = 0; ControllerIndex < MAX_CONTROLLERS; ++ControllerIndex) {
       if (ControllerHandles[ControllerIndex]) {
           SDL_GameControllerClose(ControllerHandles[ControllerIndex]);
       }
    }
}

int easyJoyStickPoll() { //returns controllers available
    globalAvailableControllers = 0;
    for (int ControllerIndex = 0;
         ControllerIndex < MAX_CONTROLLERS;
         ++ControllerIndex)
    {
        GameController *controller = globalControllerButtons + ControllerIndex;

        //clear the transition count
        for(int i = 0; i < BUTTON_COUNT; i++) {
            controller->buttons[i].transitionCount = 0;
        }

        if(ControllerHandles[ControllerIndex] != 0 && SDL_GameControllerGetAttached(ControllerHandles[ControllerIndex]))
        {
            
            bool Up = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_DPAD_UP);
            bool Down = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            bool Left = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_DPAD_LEFT);
            bool Right = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            bool Start = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_START);
            bool Back = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_BACK);
            bool LeftShoulder = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
            bool RightShoulder = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
            bool AButton = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_A);
            bool BButton = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_B);
            bool XButton = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_X);
            bool YButton = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_Y);

            short StickX = SDL_GameControllerGetAxis(ControllerHandles[ControllerIndex], SDL_CONTROLLER_AXIS_LEFTX);
            short StickY = SDL_GameControllerGetAxis(ControllerHandles[ControllerIndex], SDL_CONTROLLER_AXIS_LEFTY);

            float xAxis = inverse_lerp(-32768, StickX, 32767);
            float yAxis = inverse_lerp(-32768, StickY, 32767);

            sdlProcessGameKey(&controller->buttons[BUTTON_LEFT], Left, isDown(controller->buttons, BUTTON_LEFT) && Left);
            sdlProcessGameKey(&controller->buttons[BUTTON_RIGHT], Right, isDown(controller->buttons, BUTTON_RIGHT) && Right);
            sdlProcessGameKey(&controller->buttons[BUTTON_UP], Up, isDown(controller->buttons, BUTTON_UP) && Up);
            sdlProcessGameKey(&controller->buttons[BUTTON_DOWN], Down, isDown(controller->buttons, BUTTON_DOWN) && Down);
            sdlProcessGameKey(&controller->buttons[BUTTON_SPACE], AButton, isDown(controller->buttons, BUTTON_SPACE) && AButton);
            sdlProcessGameKey(&controller->buttons[BUTTON_SHIFT], LeftShoulder, isDown(controller->buttons, BUTTON_SHIFT) && LeftShoulder);
            sdlProcessGameKey(&controller->buttons[BUTTON_ENTER], Start, isDown(controller->buttons, BUTTON_ENTER) && Start);
            sdlProcessGameKey(&controller->buttons[BUTTON_BACKSPACE], BButton, isDown(controller->buttons, BUTTON_BACKSPACE) && BButton);
            sdlProcessGameKey(&controller->buttons[BUTTON_ESCAPE], Back, isDown(controller->buttons, BUTTON_ESCAPE) && Back);
            sdlProcessGameKey(&controller->buttons[BUTTON_LEFT_MOUSE], XButton, isDown(controller->buttons, BUTTON_LEFT_MOUSE) && XButton);
            sdlProcessGameKey(&controller->buttons[BUTTON_RIGHT_MOUSE], YButton, isDown(controller->buttons, BUTTON_RIGHT_MOUSE) && YButton);

            controller->xAxis = xAxis;
            controller->yAxis = yAxis;

            globalAvailableControllers++;
        }
        else
        {
            // TODO: This controller is note plugged in.
        }
    }
    return globalAvailableControllers;
}