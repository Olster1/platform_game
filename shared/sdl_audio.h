
#define SDL_AUDIO_CALLBACK(name) void name(void *udata, unsigned char *stream, int len)
typedef SDL_AUDIO_CALLBACK(sdl_audio_callback);

#define AUDIO_MONO 1
#define AUDIO_STEREO 2

typedef struct {
    unsigned int size;
    unsigned char *data;
    char *fileName;
} WavFile;

typedef struct WavFilePtr WavFilePtr;
typedef struct WavFilePtr {
    WavFile *file;
    
    char *name;
    
    WavFilePtr *next;
} WavFilePtr;


typedef struct PlayingSound {
    WavFile *wavFile;
    unsigned int bytesAt;

    bool active;
    
    PlayingSound *nextSound;
    
    struct PlayingSound *next;
} PlayingSound;

static PlayingSound *playingSoundsFreeList;
static PlayingSound *playingSounds;

WavFilePtr *sounds[4096];

int getSoundHashKey_(char *at, int maxSize) {
    int hashKey = 0;
    while(*at) {
        //Make the hash look up different prime numbers. 
        hashKey += (*at)*19;
        at++;
    }
    hashKey %= maxSize;
    return hashKey;
}

WavFile *easyAudio_findSound(char *fileName) {
    int hashKey = getSoundHashKey_(fileName, arrayCount(sounds));
    
    WavFilePtr *file = sounds[hashKey];
    WavFile *result = 0;
    
    bool found = false;
    
    while(!found) {
        if(!file) {
            found = true; 
        } else {
            assert(file->file);
            assert(file->name);
            if(cmpStrNull(fileName, file->name)) {
                result = file->file;
                found = true;
            } else {
                file = file->next;
            }
        }
    }
    return result;
}

char *sdlAudiolastFilePortion_(char *at) {
    // TODO(Oliver): Make this more robust
    char *recent = at;
    while(*at) {
        if(*at == '/' && at[1] != '\0') { 
            recent = (at + 1); //plus 1 to pass the slash
        }
        at++;
    }
    
    int length = (int)(at - recent);
    char *result = (char *)calloc(length, 1);
    
    memcpy(result, recent, length);
    
    return result;
}

void addSound_(WavFile *sound) {
    char *truncName = sdlAudiolastFilePortion_(sound->fileName);
    int hashKey = getSoundHashKey_(truncName, arrayCount(sounds));
    
    WavFilePtr **filePtr = sounds + hashKey;
    
    bool found = false; 
    while(!found) {
        WavFilePtr *file = *filePtr;
        if(!file) {
            file = (WavFilePtr *)calloc(sizeof(WavFilePtr), 1);
            file->file = sound;
            file->name = truncName;
            *filePtr = file;
            found = true;
        } else {
            filePtr = &file->next;
        }
    }
    assert(found);
    
}

PlayingSound *playSound(Arena *arena, WavFile *wavFile, PlayingSound *nextSoundToPlay) {
    PlayingSound *result = playingSoundsFreeList;
    if(result) {
        playingSoundsFreeList = result->next;
    } else {
        result = pushStruct(arena, PlayingSound);
    }
    assert(result);
    
    //add to playing sounds. 
    result->next = playingSounds;
    playingSounds = result;
    
    result->active = true;
    result->nextSound = nextSoundToPlay;
    result->bytesAt = 0;
    result->wavFile = wavFile;
    return result;
}

//This call is for setting a sound up but not playing it. 
PlayingSound *pushSound(Arena *arena, WavFile *wavFile, PlayingSound *nextSoundToPlay) {
    PlayingSound *result = playSound(arena, wavFile, nextSoundToPlay);
    result->active = false;
    return result;
}

void loadWavFile(WavFile *result, char *fileName, SDL_AudioSpec *audioSpec) {
    int desiredChannels = audioSpec->channels;

    SDL_AudioSpec* outputSpec = SDL_LoadWAV(fileName, audioSpec, &result->data, &result->size);
    result->fileName = fileName;

    addSound_(result);

    ///NOTE: Upsample to Stereo if mono sound
    if(outputSpec->channels != desiredChannels) {
        assert(outputSpec->channels == AUDIO_MONO);
        assert(audioSpec->channels != desiredChannels);
        audioSpec->channels = desiredChannels;

        
        unsigned int newSize = 2 * result->size;
        //assign double the data 
        unsigned char *newData = (unsigned char *)calloc(sizeof(unsigned char)*newSize, 1); 
        //TODO :SIMD this 
        s16 *samples = (s16 *)result->data;
        s16 *newSamples = (s16 *)newData;
        int sampleCount = result->size/sizeof(s16);
        for(int i = 0; i < sampleCount; ++i) {
            s16 value = samples[i];
            int sampleAt = 2*i;
            newSamples[sampleAt] = value;
            newSamples[sampleAt + 1] = value;
        }
        result->size = newSize;
        SDL_FreeWAV(result->data);
        result->data = newData;
    }
    /////////////
    
    if(outputSpec) {
        assert(audioSpec->freq == outputSpec->freq);
        assert(audioSpec->format = outputSpec->format);
        assert(audioSpec->channels == outputSpec->channels);   
        assert(audioSpec->samples == outputSpec->samples);
    } else {
        fprintf(stderr, "Couldn't open wav file: %s\n", SDL_GetError());
        assert(!"couldn't open file");
    }
}

#define initAudioSpec(audioSpec, frequency) initAudioSpec_(audioSpec, frequency, audioCallback)

void initAudioSpec_(SDL_AudioSpec *audioSpec, int frequency, sdl_audio_callback *callback) {
    /* Set the audio format */
    audioSpec->freq = frequency;
    audioSpec->format = AUDIO_S16;
    audioSpec->channels = AUDIO_STEREO;
    audioSpec->samples = 4096; 
    audioSpec->callback = callback;
    audioSpec->userdata = NULL;
}

bool initAudio(SDL_AudioSpec *audioSpec) {
    bool successful = true;
    /* Open the audio device, forcing the desired format */
    if ( SDL_OpenAudio(audioSpec, NULL) < 0 ) {
        fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
        successful = false;
    }

    SDL_PauseAudio(0); //play audio
    return successful;
}


SDL_AUDIO_CALLBACK(audioCallback) {
    SDL_memset(stream, 0, len);
    for(PlayingSound **soundPrt = &playingSounds;
        *soundPrt; 
        ) {
        bool advancePtr = true;
        PlayingSound *sound = *soundPrt;
        if(sound->active) {
            unsigned char *samples = sound->wavFile->data + sound->bytesAt;
            int remainingBytes = sound->wavFile->size - sound->bytesAt;
            
            assert(remainingBytes >= 0);
            
            unsigned int bytesToWrite = (remainingBytes < len) ? remainingBytes: len;
            
            SDL_MixAudio(stream, samples, bytesToWrite, SDL_MIX_MAXVOLUME);
            
            sound->bytesAt += bytesToWrite;
            
            if(sound->bytesAt >= sound->wavFile->size) {
                assert(sound->bytesAt == sound->wavFile->size);
                if(sound->nextSound) {
                    //TODO: Allow the remaining bytes to loop back round and finish the full duration 
                    sound->active = false;
                    sound->bytesAt = 0;
                    sound->nextSound->active = true;
                } else {
                    //remove from linked list
                    advancePtr = false;
                    *soundPrt = sound->next;
                    sound->next = playingSoundsFreeList;
                    playingSoundsFreeList = sound;
                }
            }
        }
        
        if(advancePtr) {
            soundPrt = &((*soundPrt)->next);
        }
    }
}
