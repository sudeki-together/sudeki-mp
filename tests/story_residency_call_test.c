#include "hooks/story_residency_call.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Containment fixture: registry/task deltas are accepted only across the
 * supplied native operation. Outside changes must fail the next preflight. */
typedef struct Fixture {
    SudekiMpStoryResidencyCallState scope;
    unsigned registry, baseline_registry, tasks, baseline_tasks;
    unsigned world, enrolled_world, thread, owner_thread;
    unsigned calls, adoptions, postchecks;
    int entered, remove, adoption_failure, post_failure, drift_without_call;
    int change_world, nested_stage, native_failed;
} Fixture;
static int before(void *context);
static int operation(void *context);
static int adopt(void *context);
static int after(void *context);
static int run(Fixture *f) {
    return SudekiMpStoryResidencyCallRun(&f->scope,f,before,operation,adopt,after);
}
static void reenter(Fixture *f,int stage) {
    if(f->nested_stage!=stage) return;
    unsigned calls=f->calls,adoptions=f->adoptions;
    assert(!run(f));
    assert(f->calls==calls && f->adoptions==adoptions && f->scope.active);
}
static int exact(Fixture *f) {
    return f->world==f->enrolled_world && f->thread==f->owner_thread &&
        f->registry==f->baseline_registry && f->tasks==f->baseline_tasks;
}
static int before(void *context) {
    Fixture *f=context; reenter(f,1); return exact(f);
}
static int operation(void *context) {
    Fixture *f=context; ++f->calls; reenter(f,2);
    if(f->entered || f->drift_without_call) {
        if(f->remove) --f->registry;
        else ++f->registry;
        ++f->tasks;
    }
    if(f->change_world) ++f->world;
    /* Native failure may leave an allocation: entry, not success, is reported. */
    if(f->native_failed) assert(f->entered);
    return f->entered;
}
static int adopt(void *context) {
    Fixture *f=context; ++f->adoptions; reenter(f,3);
    if(f->adoption_failure || f->world!=f->enrolled_world) return 0;
    f->baseline_registry=f->registry; f->baseline_tasks=f->tasks; return 1;
}
static int after(void *context) {
    Fixture *f=context; ++f->postchecks; reenter(f,4);
    return exact(f) && !f->post_failure;
}
static Fixture fixture(void) {
    Fixture f; memset(&f,0,sizeof(f));
    f.registry=f.baseline_registry=80;
    f.tasks=f.baseline_tasks=100;
    f.world=f.enrolled_world=3;
    f.thread=f.owner_thread=7;
    return f;
}
int main(void) {
    Fixture f=fixture();
    /* Old bug: tracked groups renewed a 30-second window for the whole fight. */
    for(unsigned tick=0;tick<15000;++tick) assert(run(&f));
    assert(!f.adoptions && !f.scope.active && !f.scope.failed);
    ++f.registry; /* unrelated entity appears between calls */
    assert(!run(&f) && f.calls==15000 && !f.adoptions);

    f=fixture(); f.entered=1;
    assert(run(&f) && f.adoptions==1 && exact(&f) && !f.scope.active);
    f.remove=1;
    assert(run(&f) && f.adoptions==2 && f.registry==80);
    ++f.tasks; /* a previous real spawn does not permit later task creation */
    assert(!run(&f) && f.calls==2 && f.adoptions==2);

    f=fixture(); f.native_failed=f.entered=1;
    assert(run(&f) && f.adoptions==1 && exact(&f));
    f.entered=f.native_failed=0;
    assert(run(&f) && f.adoptions==1); /* refusal/no-op grants no adoption */
    f.drift_without_call=1;
    assert(!run(&f) && f.adoptions==1 && f.scope.failed && !f.scope.active);
    f.drift_without_call=0; f.baseline_registry=f.registry; f.baseline_tasks=f.tasks;
    assert(!run(&f) && f.calls==3); /* no retry can reopen quarantined scope */

    for(int stage=1;stage<=4;++stage) {
        f=fixture(); f.entered=1; f.nested_stage=stage;
        assert(run(&f) && f.calls==1 && f.adoptions==1 && f.postchecks==1);
    }
    f=fixture(); ++f.thread;
    assert(!run(&f) && !f.calls && !f.adoptions && !f.scope.active);
    f=fixture(); ++f.world;
    assert(!run(&f) && !f.calls && !f.adoptions);
    f=fixture(); f.change_world=f.entered=1;
    assert(!run(&f) && f.scope.failed && f.postchecks==1);
    f=fixture(); f.adoption_failure=f.entered=1;
    assert(!run(&f) && f.scope.failed && f.postchecks==1);
    f=fixture(); f.post_failure=f.entered=1;
    assert(!run(&f) && f.scope.failed && f.postchecks==1 && !f.scope.active);
    f=fixture();
    assert(!SudekiMpStoryResidencyCallRun(&f.scope,&f,before,NULL,adopt,after));
    assert(!f.calls && !f.scope.active);
    puts("StoryResidencyCallTest: PASS");
    return 0;
}
