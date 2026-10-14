#include <stdio.h>
int SudekiMpModArchiveImageTests(void);
int SudekiMpModManifestTests(void);
int SudekiMpModGroupsTests(void);
int SudekiMpModZipTests(void);
int SudekiMpModItemsTests(void);
int main(void) {
    if (SudekiMpModArchiveImageTests() || SudekiMpModManifestTests() || SudekiMpModGroupsTests() || SudekiMpModZipTests() || SudekiMpModItemsTests()) return 1;
    puts("SudekiMP.ModdingCoreTest: passed (synthetic fixtures only)");
    return 0;
}
