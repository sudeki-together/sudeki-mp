/* Event latch only; no windows, hooks, input injection or native controller. */
#include "../src/hooks/lan_story_input.c"
#include <assert.h>
#include <stdio.h>
int main(void) {
    NativeEvent attack={.action=0x2c,.value=1};
    sample_melee(&attack,TRUE,10); assert(take_melee(10)==1 && !take_melee(10));
    sample_melee(&attack,TRUE,11); assert(!take_melee(11)); /* repeat, not another press */
    clear_sample(); sample_melee(&attack,TRUE,12); assert(!take_melee(12));
    attack.value=0; sample_melee(&attack,FALSE,13);
    attack.value=1; sample_melee(&attack,FALSE,14);
    sample_melee(&attack,TRUE,15); assert(!take_melee(15)); /* focus/lease regain */
    attack.value=0; sample_melee(&attack,TRUE,16);
    for(unsigned i=0;i<6;++i) {
        attack.action=0x2c+i%3; attack.value=0; sample_melee(&attack,TRUE,20+i);
        attack.value=1; sample_melee(&attack,TRUE,20+i);
    }
    assert(melee_count==4); /* bounded, ordered, overflow doesn't invent input */
    assert(take_melee(30)==1 && take_melee(30)==2 && take_melee(30)==3 && take_melee(30)==1);
    assert(!take_melee(30));
    attack.action=0x2c; attack.value=0; sample_melee(&attack,TRUE,40);
    attack.value=1; sample_melee(&attack,TRUE,40); assert(!take_melee(291));
    attack.value=0; sample_melee(&attack,TRUE,UINT32_MAX-10u);
    attack.value=1; sample_melee(&attack,TRUE,UINT32_MAX-10u);
    assert(take_melee(4)==1); /* unsigned clock wrap */
    attack.value=0; sample_melee(&attack,TRUE,50);
    attack.value=1; sample_melee(&attack,TRUE,50); clear_sample(); assert(!take_melee(50));
    NativeEvent e={.action=0x19,.value=1};
    sample_quick(&e,TRUE); assert(quick_pending && quick_down);
    quick_pending=FALSE; sample_quick(&e,TRUE); assert(!quick_pending);
    clear_sample(); sample_quick(&e,TRUE); assert(!quick_pending); /* new lease, held key */
    e.value=0; sample_quick(&e,FALSE); assert(!quick_down);
    e.value=1; sample_quick(&e,FALSE); assert(!quick_pending && quick_down);
    sample_quick(&e,TRUE); assert(!quick_pending); /* regain focus cannot open Q */
    e.value=0; sample_quick(&e,TRUE); e.value=1; sample_quick(&e,TRUE); assert(quick_pending);
    e.action=0x28; e.value=0; sample_quick(&e,TRUE); assert(quick_down); /* other input is not Q release */
    clear_sample(); assert(!quick_pending && quick_down);
    native_thread=GetCurrentThreadId(); sample_controller=(void *)1; sample_actor=(void *)2; sample_transaction=3;
    sample_x=sample_z=look_x=look_y=1; SudekiMpLanStoryInputMuteMovement();
    assert(!sample_x && !sample_z && !look_x && !look_y);
    assert(sample_controller==(void *)1 && sample_actor==(void *)2 && sample_transaction==3);
    attack.value=0; sample_melee(&attack,TRUE,60);
    attack.value=1; sample_melee(&attack,TRUE,60); SudekiMpLanStoryInputMuteMovement();
    assert(!take_melee(60)); sample_melee(&attack,TRUE,61); assert(!take_melee(61));
    puts("story Q/melee edge, queue, expiry, rebind and menu-mute tests passed (synthetic)"); return 0;
}
