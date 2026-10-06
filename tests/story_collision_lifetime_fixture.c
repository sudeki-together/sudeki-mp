/* Synthetic native events for query+real-journal integration. Never linked in
 * the DLL; no production API for manufacturing allocation observations. */
#include "../src/hooks/lan_story_collision_lifetime.c"
void TestStoryCollisionBorn(void *source) { observed(source,TRUE); }
void TestStoryCollisionDeleted(void *source) { observed(source,FALSE); }
