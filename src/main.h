typedef struct {
    Arena longTermArena;
    Arena scratchMemoryArena;
    
    Array_Dynamic commons;
    Array_Dynamic entities;
    Array_Dynamic collisionEnts;
    Array_Dynamic doorEnts;
    Array_Dynamic platformEnts;
    Array_Dynamic noteEnts;
    Array_Dynamic noteParentEnts;
    Array_Dynamic npcEntities;
    Array_Dynamic particleSystems;
    Array_Dynamic events;
    Array_Dynamic lights;

    V2 mouseOffset;
    V2 interactStartingMouseP;

    animation_list_item *AnimationItemFreeList;

    AnimationParent KnightAnimations;
    AnimationParent ManAnimations;

    Array_Dynamic undoBuffer;

    int ID;
    int eventID;

    Entity *player;
    Entity_Commons *camera;
    
} GameState;