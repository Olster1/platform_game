/*
TODO: Merge the other file stuff into here
Have defines for platform i.e. not named sdl 
*/

typedef struct {
    bool valid;
    size_t fileSize;
    unsigned char *memory;
} FileContents;

size_t getFileSize(FILE *handle) {
    size_t result = 0;
    if(fseek(handle, 0, SEEK_END) == 0) {
        result = ftell(handle);
    }
    
    fseek(handle, 0, SEEK_SET);
    return result;
}

char *getFileExtension(char *fileName) {
    char *result = fileName;
    
    bool hasDot = false;
    while(*fileName) {
        if(*fileName == '.') { 
            result = fileName + 1;
            hasDot = true;
        }
        fileName++;
    }
    
    if(!hasDot) {
        result = 0;
    }
    
    return result;
}

char *getFileLastPortion(char *at) {
    char *recent = at;
    while(*at) {
        if(*at == '/' && at[1] != '\0') { 
            recent = (at + 1); //plus 1 to pass the slash
        }
        at++;
    }
    
    int length = (int)(at - recent) + 1; //for null termination
    char *result = (char *)calloc(length, 1);
    
    memcpy(result, recent, length);
    
    return result;
}

typedef struct {
    char *names[32]; //max 32 files
    int count;
} FileNameOfType;

bool isInCharList(char *ext, char **exts, int count) {
    bool result = false;
    for(int i = 0; i < count; i++) {
        if(cmpStrNull(ext, exts[i])) {
            result = true;
            break;
        }
    }
    return result;
}

typedef enum {
    DIR_FIND_FILE_TYPE,
    DIR_DELETE_FILE_TYPE,
    DIR_FIND_DIR_TYPE,
} DirTypeOperation;

//TODO: can we make this into a multiple purpose function for deleting files etc.??
FileNameOfType getDirectoryFilesOfType_(char *dirName, char **exts, int count, DirTypeOperation opType) { 
    FileNameOfType fileNames = {};
    #ifdef __APPLE__
        DIR *directory = opendir(dirName);
        if(directory) {

            struct dirent *dp = 0;

               do {
                   dp = readdir(directory);
                   if (dp) {
                        char *fileName = concat(dirName, dp->d_name);
                        char *ext = getFileExtension(fileName);
                        switch(opType) {
                            case DIR_FIND_FILE_TYPE: {
                                if(isInCharList(ext, exts, count)) {
                                    assert(fileNames.count < arrayCount(fileNames.names));
                                    fileNames.names[fileNames.count++] = fileName;
                                }
                            } break;
                            case DIR_DELETE_FILE_TYPE: {
                                if(isInCharList(ext, exts, count)) {
                                    remove(fileName);
                                    free(fileName);
                                }
                            } break;
                            case DIR_FIND_DIR_TYPE: {
                                if(!ext) { //is folder
                                    assert(fileNames.count < arrayCount(fileNames.names));
                                    fileNames.names[fileNames.count++] = fileName;
                                }
                            } break;
                        }
                   }
               } while (dp);
            closedir(directory);
        }
    #else 
        assert(!"not implemented");
    #endif

    return fileNames;
}

#define getDirectoryFilesOfType(dirName, exts, count) getDirectoryFilesOfType_(dirName, exts, count, DIR_FIND_FILE_TYPE)
#define deleteAllFilesOfType(dirName, exts, count) getDirectoryFilesOfType_(dirName, exts, count, DIR_DELETE_FILE_TYPE)
#define getDirectoryFolders(dirName) getDirectoryFilesOfType_(dirName, 0, 0, DIR_FIND_DIR_TYPE)

void platformDeleteFile(char *fileName) {
    if(remove(fileName) != 0) {
        assert(!"couldn't delete file");
    }
}

#include <errno.h>
#include <sys/stat.h> //for mkdir S_IRWXU
bool platformCreateDirectory(char *fileName) {

    printf("%s\n", fileName);
    bool result = false;
    DIR* dir = opendir(fileName);
    if (dir) {
        closedir(dir);
        printf("%s %s\n", "exists", fileName);
    } else if (ENOENT == errno) {
        printf("%s\n", "platformCreateDirectory dir");
        if(mkdir(fileName, S_IRWXU) == -1) {
            assert(!"couldn't create directory");
        }
        result = true;
    } else {
        assert(!"something went wrong");
    }
    return result;

}

typedef struct {
    void *Data;
    bool HasErrors;
}  game_file_handle;

size_t sdl_GetFileSize(SDL_RWops *FileHandle) {
    long Result = SDL_RWseek(FileHandle, 0, RW_SEEK_END);
    if(Result < 0) {
        assert(!"Seek Error");
    }
    if(SDL_RWseek(FileHandle, 0, RW_SEEK_SET) < 0) {
        assert(!"Seek Error");
    }
    return (size_t)Result;
}

game_file_handle platformBeginFileRead(char *FileName)
{
    game_file_handle Result = {};
    
    SDL_RWops* FileHandle = SDL_RWFromFile(FileName, "r+");
    
    if(FileHandle)
    {
        Result.Data = FileHandle;
    }
    else
    {
        Result.HasErrors = true;
        printf("%s\n", SDL_GetError());
    }
    
    return Result;
}

game_file_handle platformBeginFileWrite(char *FileName)
{
    game_file_handle Result = {};
    
    SDL_RWops* FileHandle = SDL_RWFromFile(FileName, "w+");
    
    if(FileHandle)
    {
        Result.Data = FileHandle;
    }
    else
    {
        const char* Error = SDL_GetError();
        Result.HasErrors = true;
    }
    
    return Result;
}

void platformEndFile(game_file_handle Handle)
{
    SDL_RWops*  FileHandle = (SDL_RWops* )Handle.Data;
    if(FileHandle) {
        SDL_RWclose(FileHandle);
    }
}

void platformWriteFile(game_file_handle *Handle, void *Memory, size_t Size, int Offset)
{
    Handle->HasErrors = false;
    SDL_RWops *FileHandle = (SDL_RWops *)Handle->Data;
    if(!Handle->HasErrors)
    {
        Handle->HasErrors = true; 
        
        if(FileHandle)
        {
            
            if(SDL_RWseek(FileHandle, Offset, RW_SEEK_SET) >= 0)
            {
                if(SDL_RWwrite(FileHandle, Memory, 1, Size) == Size)
                {
                    Handle->HasErrors = false;
                }
                else
                {
                    assert(!"write file did not succeed");
                }
            }
        }
    }    
}

FileContents platformReadFile(game_file_handle Handle, void *Memory, size_t Size, int Offset)
{
    FileContents Result = {};
    
    SDL_RWops* FileHandle = (SDL_RWops*)Handle.Data;
    if(!Handle.HasErrors)
    {
        if(FileHandle)
        {
            if(SDL_RWseek(FileHandle, Offset, RW_SEEK_SET) >= 0)
            {
                if(SDL_RWread(FileHandle, Memory, 1, Size) == Size)
                {
                    Result.memory = (unsigned char *)Memory;
                    Result.fileSize = Size;
                    Result.valid = true;
                }
                else
                {
                    assert(!"Read file did not succeed");
                    Result.valid = false;
                }
            }
        }
    }    
    return Result;
    
}
size_t platformFileSize(char *FileName)
{
    size_t Result = 0;
    
    SDL_RWops* FileHandle = SDL_RWFromFile(FileName, "rb");
    
    if(FileHandle)
    {
        Result = sdl_GetFileSize(FileHandle);
        SDL_RWclose(FileHandle);
    }
    
    return Result;
}

FileContents platformReadEntireFile(char *FileName, bool nullTerminate) {
    FileContents Result = {};
    
    SDL_RWops* FileHandle = SDL_RWFromFile(FileName, "r+");
    
    if(FileHandle)
    {
        size_t allocSize = Result.fileSize = sdl_GetFileSize(FileHandle);

        if(nullTerminate) { allocSize += 1; }

        Result.memory = (unsigned char *)calloc(allocSize, 1);
        size_t ReturnSize = SDL_RWread(FileHandle, Result.memory, 1, Result.fileSize);
        if(ReturnSize == Result.fileSize)
        {
            if(nullTerminate) {
                Result.memory[Result.fileSize] = '\0'; // put at the end of the file
                Result.fileSize += 1;
            }
            Result.valid = true;
            //NOTE(Oliver): Successfully read
        } else {
            assert(!"Couldn't read file");
            Result.valid = false;
            free(Result.memory);
        }
    } else {
        Result.valid = false;
        const char *Error = SDL_GetError();
        assert(!"Couldn't open file");
        
        printf("%s\n", Error);
    }
    
    SDL_RWclose(FileHandle);
    
    return Result;
}

static inline FileContents getFileContentsNullTerminate(char *fileName) {
    FileContents result = platformReadEntireFile(fileName, true);
    return result;
}

static inline FileContents getFileContents(char *fileName) {
    FileContents result = platformReadEntireFile(fileName, false);
    return result;
}