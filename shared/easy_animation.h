// struct animation {
//     Texture Frames[16];
//     u32 FrameCount;
//     char *Name;
// };

// typedef struct animation_list_item animation_list_item;
// typedef struct animation_list_item {
//     Timer timer;
//     u32 FrameIndex;
    
//     animation *Animation;
    
//     animation_list_item *Prev;
//     animation_list_item *Next;
// } animation_list_item;

// typedef enum {
//     ANIM_IDLE,
//     ANIM_LEFT,
//     ANIM_RIGHT,
//     ANIM_UP,
//     ANIM_DOWN,
//     ANIM_JUMP,
// } AnimationState;

static void InitAnimation(animation *Animation, char **FileNames, u32 FileNameCount, float DirectionValue, char *Name, AnimationState state) {
    Animation->Name = Name;
    Animation->state = state;
    
    for(u32 i = 0; i < FileNameCount; ++i) {
        Texture *Bitmap = Animation->Frames + Animation->FrameCount++; 
        *Bitmap = loadImage(FileNames[i]);
    }
}

static animation *FindAnimationWithState(animation *Animations, u32 AnimationsCount, AnimationState state) {
    animation *Result = 0;
    forN(AnimationsCount) {
        animation *Anim = Animations + AnimationsCountIndex;
        if(Anim->state == state) {
            Result = Anim;
            break;
        }
    }
    
    return Result;
}

static animation *FindAnimation(animation *Animations, u32 AnimationsCount, char *Name) {
    animation *Result = 0;
    forN(AnimationsCount) {
        animation *Anim = Animations + AnimationsCountIndex;
        if(cmpStrNull(Anim->Name, Name)) {
            Result = Anim;
            break;
        }
    }
    
    return Result;
}

static float ANIMATION_PERIOD = 0.08f;
static void AddAnimationToList(GameState *gameState, Arena *arena, Entity_Commons *Ent, animation *Animation) {
    
    animation_list_item *Item = 0;
    if(gameState->AnimationItemFreeList) {
        Item = gameState->AnimationItemFreeList;
        gameState->AnimationItemFreeList = gameState->AnimationItemFreeList->Next;
    } else {
        Item = pushStruct(arena, animation_list_item);
    }
    
    assert(Item);
    
    Item->timer = initTimer(ANIMATION_PERIOD);
    
    Item->FrameIndex = 0;
    
    Item->Animation = Animation;
    
    //Add animation to end of list;
    Item->Next = &Ent->AnimationListSentintel;
    Item->Prev = Ent->AnimationListSentintel.Prev;
    
    Ent->AnimationListSentintel.Prev->Next = Item;
    Ent->AnimationListSentintel.Prev = Item;
    
}

static void UpdateAnimation(GameState *State, Entity_Commons *Ent, float dt, animation *NextAnimation) {
    animation_list_item *Item = Ent->AnimationListSentintel.Next;
    assert(Item != &Ent->AnimationListSentintel);
    
    if(updateTimer(&Item->timer, dt).finished) {
        Item->FrameIndex++;
        
        //if(Item->FrameIndex >= Item->Animation->FrameCount) 
        if(NextAnimation != Item->Animation || Item->FrameIndex >= Item->Animation->FrameCount)
        { 
            //finished animation
            Item->FrameIndex = 0;
            
            if(NextAnimation) {
                //Remove from linked list
                Ent->AnimationListSentintel.Next = Item->Next;
                Item->Next->Prev = &Ent->AnimationListSentintel;
                
                //Add to free list
                Item->Next = State->AnimationItemFreeList;
                State->AnimationItemFreeList = Item;
                
                //Add new animation
                AddAnimationToList(State, &State->longTermArena, Ent, NextAnimation);
            } 
        }
        
    }
}

inline static bool IsEmpty(animation_list_item *Sentinel) {
    bool Result = Sentinel->Next == Sentinel;
    return Result;
}

inline static Texture *GetBitmap(animation_list_item *Item) {
    assert(Item->Animation);
    assert(Item->FrameIndex < Item->Animation->FrameCount);
    Texture *Result = &Item->Animation->Frames[Item->FrameIndex];
    return Result;
}

inline static float GetDirectionInRadians(V2 dp) {
    float DirectionValue = 0;
    if(dp.x != 0 || dp.y != 0) {
        //V2 EntityVelocity = normalizeV2(dp);
        DirectionValue = ATan2_0toTau(dp.y, dp.x);
    }
    return DirectionValue;
}

inline static Texture *getCurrentTexture(animation_list_item *AnimationListSentintel) {
    Texture *CurrentBitmap = GetBitmap(AnimationListSentintel->Next);
    return CurrentBitmap;
}