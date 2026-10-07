#include <stdio.h>
int SudekiMpModArchiveImageTests(void);
int SudekiMpModManifestTests(void);
int SudekiMpModGroupsTests(void);
int main(void) {
    if (SudekiMpModArchiveImageTests() || SudekiMpModManifestTests() || SudekiMpModGroupsTests()) return 1;
    puts("SudekiMP.ModdingCoreTest: passed (synthetic fixtures only)");
    return 0;
}
