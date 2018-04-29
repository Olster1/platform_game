typedef enum {
    ANIM_IDLE,
    ANIM_LEFT,
    ANIM_RIGHT,
    ANIM_UP,
    ANIM_DOWN,
    ANIM_JUMP,
} AnimationState;

struct animation {
    Texture Frames[16];
    u32 FrameCount;
    char *Name;
    AnimationState state;
};

struct AnimationParent {
	int count;
    animation anim[16];
};

typedef struct animation_list_item animation_list_item;
typedef struct animation_list_item {
    Timer timer;
    u32 FrameIndex;
    
    animation *Animation;
    
    animation_list_item *Prev;
    animation_list_item *Next;
} animation_list_item;

