float randomBetween(float a, float b) { // including a and b
    float result = ((float)rand() / (float)RAND_MAX);
    assert(result >= 0 && result <= 1.0f);
    result = lerp(a, result, b);
    return result;
}

typedef enum {
    PARTICLE_SYS_DEFAULT,
    PARTICLE_SYS_CIRCULAR,
} ParticleSystemType;

#define Particles_DrawGrid 0
struct particle
{
    V3 ddP;
    V3 dP;
    V3 P;
    V4 dColor;
    V4 Color;
    float lifeAt;
    
};

struct particle_cel
{
    float Density;
    V3 dP;
};

struct particle_system_settings {
    float LifeSpan;
    float MaxLifeSpan;
    float Loop;
    
    unsigned int BitmapCount;
    Texture *Bitmaps[32];
    unsigned int BitmapIndex;
    Rect2f VelBias;

    ParticleSystemType type;
    bool collidesWithFloor;
};

#define CEL_GRID_SIZE 32
#define DEFAULT_MAX_PARTICLE_COUNT 32

struct particle_system {
    particle_cel ParticleGrid[CEL_GRID_SIZE][CEL_GRID_SIZE];
    unsigned int NextParticle;
    unsigned int MaxParticleCount;
    int particleCount;

    particle Particles[DEFAULT_MAX_PARTICLE_COUNT];
    particle_system_settings Set;
    
    Timer creationTimer; 

    bool Active;
};


inline void pushParticleBitmap(particle_system_settings *Settings, Texture *Bitmap) {
    assert(Settings->BitmapCount < arrayCount(Settings->Bitmaps));
    Settings->Bitmaps[Settings->BitmapCount++] = Bitmap;
    
}

internal inline particle_system_settings InitParticlesSettings(ParticleSystemType type) {
    particle_system_settings Set = {};
    Set.MaxLifeSpan = Set.LifeSpan = 3.0f;
    Set.type = type;
    return Set;
}

internal inline void InitParticleSystem(particle_system *System, particle_system_settings *Set, unsigned int MaxParticleCount = DEFAULT_MAX_PARTICLE_COUNT) {
    memset(System, 0, sizeof(particle_system));
    System->Set = *Set;
    System->MaxParticleCount = DEFAULT_MAX_PARTICLE_COUNT;
    //System->Active = true;
    //System->Set.Loop = true;
    System->creationTimer = initTimer(1.0f/120.0f); 
}

inline void Reactivate(particle_system *System) {
    System->Set.LifeSpan = System->Set.MaxLifeSpan;
    System->particleCount = 0;
    System->Active = true;
}

internal inline void drawAndUpdateParticleSystem(particle_system *System, float dt, V3 Origin, V3 Acceleration, V3 camPos, Matrix4 metresToPixels) {
    if(System->Active) {
        float particleLifeSpan = 0;
        float GridScale = 0.4f;
        float Inv_GridScale = 1.0f / GridScale;
        
        V3 CelGridOrigin = Origin;
    
        int particlesToCreate = 0;        

        if(System->Set.type == PARTICLE_SYS_DEFAULT) {        
            particleLifeSpan = System->creationTimer.period*System->MaxParticleCount;
            Timer *timer = &System->creationTimer;
            timer->value += dt;        
            if(timer->value >= timer->period) {
                particlesToCreate = (int)(timer->value / timer->period);
                assert(particlesToCreate > 0);
                timer->value = 0;
            }

            //create new particles for the frame
            for(int ParticleIndex = 0;
                ParticleIndex < particlesToCreate;
                ++ParticleIndex)
            {
                particle *Particle = System->Particles + System->NextParticle++;
                Particle->Color = v4(1, 1, 1, 1.0f);
                
                //NOTE(oliver): Paricles start with motion 
                
                Particle->P = v3(randomBetween(-0.01f, 0.01f),
                                 0,
                                 0);
                Particle->dP = v3(randomBetween(System->Set.VelBias.min.x, System->Set.VelBias.max.x),
                                  randomBetween(System->Set.VelBias.min.y, System->Set.VelBias.max.y),
                                  0);
                Particle->ddP = Acceleration;
                Particle->lifeAt = 0;
                
                if(System->particleCount < System->NextParticle) {
                    System->particleCount = System->NextParticle;
                }
                Particle->dColor = v4_scale(0.002f, v4(1, 1, 1, 1));
                if(System->NextParticle == System->MaxParticleCount) {
                    System->NextParticle = 0;
                }
            }
        } else if(System->Set.type == PARTICLE_SYS_CIRCULAR) {
            particleLifeSpan = System->Set.MaxLifeSpan; //seting up the particle system
            if(System->particleCount == 0) {
                float dTheta = (float)TAU32 / (float)System->MaxParticleCount;
                
                float theta = 0;
                for(int i = 0; i < System->MaxParticleCount; ++i) {
                    V2 dp = v2_scale(1, v2(cos(theta), sin(theta)));
                    theta += dTheta;
                    particle *Particle = System->Particles + i;
                    Particle->Color = v4(1, 1, 1, 1);
                    Particle->P = v3(0, 0, 0);
                    Particle->dP = v2ToV3(dp, 0);
                    Particle->ddP = Acceleration;
                    Particle->dColor = v4_scale(0.002f, v4(1, 1, 1, 1));
                    Particle->lifeAt = 0;
                }
                System->particleCount = System->MaxParticleCount;
            }
        }
         
        memset(&System->ParticleGrid, 0, sizeof(System->ParticleGrid));
        
        float halfGridWidth = 0.5f*GridScale*CEL_GRID_SIZE;
        float halfGridHeight = 0.5f*GridScale*CEL_GRID_SIZE;
        for(unsigned int ParticleIndex = 0;
            ParticleIndex < System->particleCount;
            ++ParticleIndex)
        {
            particle *Particle = System->Particles + ParticleIndex;
            
            V3 P = v3_scale(Inv_GridScale, Particle->P);
            
            int CelX = (int)(P.x + halfGridWidth);
            int CelY = (int)(P.y + halfGridHeight);
            
            if(CelX >= CEL_GRID_SIZE){ CelX = CEL_GRID_SIZE - 1;}
            if(CelY >= CEL_GRID_SIZE){ CelY = CEL_GRID_SIZE - 1;}
            
            particle_cel *Cel = &System->ParticleGrid[CelY][CelX];
            
            float ParticleDensity = Particle->Color.w;
            Cel->Density += ParticleDensity;
            Cel->dP = v3_plus(v3_scale(ParticleDensity, Particle->dP), Cel->dP);
        }
        
#if Particles_DrawGrid
        {
            //NOTE(oliver): To draw Eulerian grid
            
            for(unsigned int Y = 0;
                Y < CEL_GRID_SIZE;
                ++Y)
            {
                for(unsigned int X = 0;
                    X < CEL_GRID_SIZE;
                    ++X)
                {
                    particle_cel *Cel = &System->ParticleGrid[Y][X];
                    
                    V3 P = CelGridOrigin + v3(GridScale*(float)X, GridScale*(float)Y, 0);
                    
                    float Density = 0.1f*Cel->Density;
                    V4 Color = v4(Density, Density, Density, 1);
                    Rect2f Rect = rect2MinDim(P.XY, v2(GridScale, GridScale));
                    //PushRectOutline(RenderGroup, Rect, 1,Color);
                }
            }
        }
#endif
        for(unsigned int ParticleIndex = 0;
            ParticleIndex < System->particleCount;
            ++ParticleIndex)
        {
            particle *Particle = System->Particles + ParticleIndex;
            
            V3 P = v3_scale(Inv_GridScale, Particle->P);
            
            int CelX = (int)(P.x);
            int CelY = (int)(P.y);
            
            if(CelX >= CEL_GRID_SIZE - 1){ CelX = CEL_GRID_SIZE - 2;}
            if(CelY >= CEL_GRID_SIZE - 1){ CelY = CEL_GRID_SIZE - 2;}
            
            if(CelX < 1){ CelX = 1;}
            if(CelY < 1){ CelY = 1;}
            
            particle_cel *CelCenter = &System->ParticleGrid[CelY][CelX];
            particle_cel *CelLeft = &System->ParticleGrid[CelY][CelX - 1];
            particle_cel *CelRight = &System->ParticleGrid[CelY][CelX + 1];
            particle_cel *CelUp = &System->ParticleGrid[CelY + 1][CelX];
            particle_cel *CelDown = &System->ParticleGrid[CelY - 1][CelX];
            
            
            V3 VacumDisplacement = Acceleration;//V3(0, 0, 0);
            float DisplacmentCoeff = 0.6f;
            if(System->Set.type != PARTICLE_SYS_CIRCULAR) { //don't have vacuum displacement dor circular particle effects
                VacumDisplacement = v3_plus(VacumDisplacement, v3_scale((CelCenter->Density - CelRight->Density), v3(1, 0, 0)));
                VacumDisplacement = v3_plus(VacumDisplacement, v3_scale((CelCenter->Density - CelLeft->Density), v3(-1, 0, 0)));
                VacumDisplacement = v3_plus(VacumDisplacement, v3_scale((CelCenter->Density - CelUp->Density), v3(0, 1, 0)));
                VacumDisplacement = v3_plus(VacumDisplacement, v3_scale((CelCenter->Density - CelDown->Density), v3(0, -1, 0)));
            }

            V3 ddP = v3_plus(Particle->ddP, v3_scale(DisplacmentCoeff, VacumDisplacement));
            
            //NOTE(oliver): Move particle
            Particle->P = v3_plus(v3_plus(v3_scale(0.5f*sqr(dt), ddP),  v3_scale(dt, Particle->dP)),  Particle->P);
            Particle->dP = v3_plus(v3_scale(dt, ddP), Particle->dP);
            
            //NOTE(oliver): Collision with ground
            if(System->Set.collidesWithFloor) {
                if(Particle->P.y < 0.0f)
                {
                    float CoefficentOfResitution = 0.5f;
                    Particle->P.y = 0.0f;
                    Particle->dP.y = -Particle->dP.y*CoefficentOfResitution;
                    Particle->dP.x *= 0.8f;
                }
            }
            
            //NOTE(oliver): Color update
            Particle->Color = v4_plus(v4_scale(dt, Particle->dColor), Particle->Color);
            V4 Color = Particle->Color;
            
            float t = Particle->lifeAt/particleLifeSpan;
            assert(t >= 0 && t <= 1);
            float alphaValue = clamp(0, smoothStep00(0, t, 1), 1);

            Color.w = alphaValue;
            
            particle_system_settings *Set = &System->Set;
            
            Texture *Bitmap = Set->Bitmaps[Set->BitmapIndex++];
            
            if(Set->BitmapIndex >= Set->BitmapCount) {
                Set->BitmapIndex = 0;
            }
            
            Particle->lifeAt += dt;
            RenderInfo renderInfo = calculateRenderInfo(v3_plus(Particle->P, Origin), v3(0.8f, 0.8f, 0), camPos, metresToPixels);
            openGlTextureCentreDim(Bitmap->id, renderInfo.pos, renderInfo.dim.xy, Color, 0, mat4(), 1, projectionMatrixToScreen(bufferWidth, bufferHeight));
        }
        System->Set.LifeSpan -= dt;
        if(System->Set.LifeSpan <= 0.0f) {
            if(System->Set.Loop) {
                Reactivate(System);
            } else {
                System->Active = false;
            }
            
        }
    }
}