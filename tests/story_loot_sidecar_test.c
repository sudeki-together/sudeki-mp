#include "engine/story_loot_sidecar.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char root[MAX_PATH],directory[MAX_PATH],path[MAX_PATH],lock_path[MAX_PATH];
    assert(GetTempPathA(sizeof(root),root)>0);
    assert(snprintf(directory,sizeof(directory),"%sstory-loot-%lu-%lu",root,
        (unsigned long)GetCurrentProcessId(),(unsigned long)GetTickCount())<(int)sizeof(directory));
    assert(CreateDirectoryA(directory,NULL));
    assert(snprintf(path,sizeof(path),"%s/test.smploot",directory)<(int)sizeof(path));
    assert(snprintf(lock_path,sizeof(lock_path),"%s.lock",path)<(int)sizeof(lock_path));
    uint8_t identity[32]={1},other[32]={2};
    SudekiMpStoryLootState s,out,before;
    SudekiMpStoryLootReceipt receipt;
    assert(SudekiMpStoryLootInitialize(&s,identity,365));
    assert(SudekiMpStoryLootBeginVisit(&s,1)==SUDEKIMP_STORY_LOOT_APPLIED);
    memset(&out,0x44,sizeof(out)); before=out;
    assert(SudekiMpStoryLootReadFile(path,identity,&out)==SUDEKIMP_STORY_LOOT_FILE_MISSING);
    assert(!memcmp(&out,&before,sizeof(out)));
    assert(SudekiMpStoryLootWriteFile("must-not-touch-native.sav",&s)==SUDEKIMP_STORY_LOOT_FILE_INVALID);
    assert(SudekiMpStoryLootWriteFile("test.smploot:stream",&s)==SUDEKIMP_STORY_LOOT_FILE_INVALID);
    assert(SudekiMpStoryLootWriteFile(path,&s)==SUDEKIMP_STORY_LOOT_FILE_OK);
    assert(SudekiMpStoryLootReadFile(path,identity,&out)==SUDEKIMP_STORY_LOOT_FILE_OK);
    assert(!memcmp(&s,&out,sizeof(s)));
    before=out;
    assert(SudekiMpStoryLootReadFile(path,other,&out)==SUDEKIMP_STORY_LOOT_FILE_INVALID);
    assert(!memcmp(&out,&before,sizeof(out)));
    assert(SudekiMpStoryLootWriteFile(path,&s)==SUDEKIMP_STORY_LOOT_FILE_OK);
    s.money[0]=1; /* same revision must not change its meaning */
    assert(SudekiMpStoryLootWriteFile(path,&s)==SUDEKIMP_STORY_LOOT_FILE_ERROR);
    s=before;
    SudekiMpStoryLootEvent e={.visit=1,.operation=1,.source=20,.kind=SUDEKIMP_STORY_LOOT_WORLD_REWARD,
        .actor=SUDEKIMP_WALLET_CHARACTER_TAL,.amount=10};
    assert(SudekiMpStoryLootApplyConfirmed(&s,&e,&receipt)==SUDEKIMP_STORY_LOOT_APPLIED);
    HANDLE lock=CreateFileA(lock_path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    assert(lock!=INVALID_HANDLE_VALUE);
    assert(SudekiMpStoryLootWriteFile(path,&s)==SUDEKIMP_STORY_LOOT_FILE_ERROR);
    assert(GetFileAttributesA(lock_path)!=INVALID_FILE_ATTRIBUTES); /* foreign lock untouched */
    assert(CloseHandle(lock)); assert(DeleteFileA(lock_path));
    assert(SudekiMpStoryLootWriteFile(path,&s)==SUDEKIMP_STORY_LOOT_FILE_OK);
    assert(SudekiMpStoryLootWriteFile(path,&before)==SUDEKIMP_STORY_LOOT_FILE_ERROR); /* rollback */
    assert(SudekiMpStoryLootReadFile(path,identity,&out)==SUDEKIMP_STORY_LOOT_FILE_OK);
    assert(out.money[0]==10 && out.money[3]==10 && out.source_count==1);
    memcpy(s.save_identity,other,32); ++s.revision;
    assert(SudekiMpStoryLootWriteFile(path,&s)==SUDEKIMP_STORY_LOOT_FILE_ERROR);
    memcpy(s.save_identity,identity,32);
    HANDLE file=CreateFileA(path,GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    assert(file!=INVALID_HANDLE_VALUE);
    DWORD written; uint8_t bad=0;
    assert(WriteFile(file,&bad,1,&written,NULL) && written==1 && CloseHandle(file));
    before=out;
    assert(SudekiMpStoryLootReadFile(path,identity,&out)==SUDEKIMP_STORY_LOOT_FILE_INVALID);
    assert(!memcmp(&out,&before,sizeof(out)));
    assert(SudekiMpStoryLootWriteFile(path,&s)==SUDEKIMP_STORY_LOOT_FILE_ERROR); /* no corrupt-file replacement */
    assert(DeleteFileA(path)); assert(RemoveDirectoryA(directory));
    puts("story_loot_sidecar_test: PASS"); return 0;
}
