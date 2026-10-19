/* Event latch only; no windows, hooks, input injection or native controller. */
#include "../src/hooks/lan_story_input.c"
#include <assert.h>
#include <stdio.h>
void SudekiMpLogFormat(const char *format,...) { (void)format; }
int main(void) {
    native_thread=GetCurrentThreadId(); focused=TRUE;
    NativeEvent m={.action=0x18,.value=1};
    sample_menu(&m,FALSE); assert(menu_pending_raw && !menu_pending);
    SudekiMpLanStoryInputClear(); assert(SudekiMpLanStoryInputTakeMainMenuRaw());
    assert(!SudekiMpLanStoryInputTakeMainMenuRaw());
    sample_menu(&m,FALSE); assert(!SudekiMpLanStoryInputTakeMainMenuRaw());
    m.value=0; sample_menu(&m,FALSE); m.value=1; sample_menu(&m,FALSE);
    clear_sample(); assert(SudekiMpLanStoryInputTakeMainMenuRaw());
    focused=FALSE; m.value=0; sample_menu(&m,FALSE); m.value=1; sample_menu(&m,FALSE);
    assert(!SudekiMpLanStoryInputTakeMainMenuRaw());

    focused=TRUE; menu_pending_raw=TRUE; input_window=(HWND)1;
    CWPSTRUCT focus_event={.hwnd=(HWND)1,.message=WM_KILLFOCUS};
    (void)focus_messages(0,0,(LPARAM)&focus_event); assert(!menu_pending_raw && !focused);
    focus_event.message=WM_SETFOCUS; (void)focus_messages(0,0,(LPARAM)&focus_event);
    assert(focused && !SudekiMpLanStoryInputTakeMainMenuRaw()); input_window=NULL;
    NativeEvent attack={.action=0x2c,.value=1};
    sample_melee(&attack,TRUE,10); assert(take_melee()==1 && take_melee()==2 && take_melee()==2);
    sample_melee(&attack,TRUE,11); assert(take_melee()==2); /* repeat remains held */
    attack.value=0;sample_melee(&attack,TRUE,12);assert(take_melee()==3 && !take_melee());
    attack.value=1;sample_melee(&attack,TRUE,13);
    attack.action=0x2d;sample_melee(&attack,TRUE,13);
    assert(take_melee()==5 && take_melee()==10); /* grouped weak+strong */
    attack.action=0x2c;attack.value=0;sample_melee(&attack,TRUE,14);
    assert(take_melee()==11 && take_melee()==8); /* one release while other held */
    attack.action=0x2d;attack.value=0;sample_melee(&attack,TRUE,14);assert(take_melee()==12);
    attack.action=0x2e;attack.value=1;sample_melee(&attack,TRUE,15);assert(take_melee()==16);
    uint32_t generation=melee_generation;clear_sample();assert(melee_generation!=generation && !take_melee());
    sample_melee(&attack,TRUE,16);assert(!take_melee()); /* held across rebind */
    attack.value=0;sample_melee(&attack,FALSE,17);
    attack.value=1;sample_melee(&attack,FALSE,18);sample_melee(&attack,TRUE,19);assert(!take_melee());
    attack.value=0;sample_melee(&attack,TRUE,20);attack.value=1;sample_melee(&attack,TRUE,21);
    assert(take_melee()==16 && take_melee()==32);
    clear_sample();
    melee_down=0; ranged_press=0;ranged_held=FALSE;
    attack.action=0x2c;attack.value=1;sample_melee(&attack,TRUE,70);
    assert(ranged_held && ranged_press==1);
    sample_melee(&attack,TRUE,71);assert(ranged_press==1 && ranged_held);
    clear_sample();assert(!ranged_held && ranged_press==1);
    sample_melee(&attack,TRUE,72);assert(!ranged_held && ranged_press==1);
    attack.value=0;sample_melee(&attack,FALSE,73);
    attack.value=1;sample_melee(&attack,FALSE,74);
    sample_melee(&attack,TRUE,75);assert(!ranged_held && ranged_press==1);
    attack.value=0;sample_melee(&attack,TRUE,76);
    attack.value=1;sample_melee(&attack,TRUE,77);assert(ranged_held && ranged_press==2);
    attack.value=0;sample_melee(&attack,TRUE,78);assert(!ranged_held && ranged_press==2);
    NativeEvent guard={.action=0x31,.value=1};
    sample_block(&guard,TRUE);assert(block_down && block_held);
    sample_block(&guard,TRUE);assert(block_held); /* held state persists */
    clear_sample();assert(block_down && !block_held);
    sample_block(&guard,TRUE);assert(!block_held); /* rebind cannot replay hold */
    guard.value=0;sample_block(&guard,FALSE);assert(!block_down && !block_held);
    guard.value=1;sample_block(&guard,FALSE);sample_block(&guard,TRUE);assert(!block_held);
    guard.value=0;sample_block(&guard,TRUE);guard.value=1;sample_block(&guard,TRUE);assert(block_held);
    native_thread=GetCurrentThreadId();SudekiMpLanStoryInputMuteMovement();assert(block_down && !block_held);
    sample_block(&guard,TRUE);assert(!block_held);
    guard.value=0;sample_block(&guard,TRUE);guard.value=1;sample_block(&guard,TRUE);assert(block_held);
    guard.action=0x2f;guard.value=0;sample_block(&guard,TRUE);assert(block_held); /* weapon-next is not block */
    guard.action=0x31;sample_block(&guard,FALSE);assert(!block_held && !block_down);
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
    assert(!take_melee()); sample_melee(&attack,TRUE,61); assert(!take_melee());
    sample_x=sample_z=1; look_x=0.5f;look_y=-0.25f;
    look_x_at=11;look_y_at=12;look_sample_at=13;
    ranged_held=block_held=TRUE;
    SudekiMpLanStoryInputMuteForCamera();
    assert(!sample_x && !sample_z && !take_melee() && !ranged_held && !block_held);
    assert(look_x==0.5f && look_y==-0.25f && look_x_at==11 && look_y_at==12 && look_sample_at==13);
    assert(sample_controller==(void *)1 && sample_actor==(void *)2 && sample_transaction==3);
    float x=1,y=1;focused=FALSE;
    assert(SudekiMpLanStoryInputLookAxes(&x,&y) && !x && !y);
    focused=TRUE;look_x_at=look_y_at=GetTickCount()-101u;
    assert(SudekiMpLanStoryInputLookAxes(&x,&y) && !x && !y);
    SudekiMpLanStoryInputMuteMovement();
    assert(!look_x && !look_y && !look_x_at && !look_y_at && !look_sample_at);
    /* Away orbit owns only a proved own-body look binding. Gameplay clear
     * keeps its current axes and receipt stamps; disabling discards them. */
    away_controller=(void *)1; away_actor=(void *)2;
    look_x=.5f; look_y=-.5f; look_x_at=10; look_y_at=11;
    sample_x=sample_z=1; clear_sample();
    assert(!sample_x && !sample_z && !sample_transaction && look_x==.5f && look_y==-.5f);
    assert(look_x_at==10 && look_y_at==11);
    away_controller=away_actor=NULL; clear_sample(); assert(!look_x && !look_y && !look_x_at && !look_y_at);
    puts("PASS named CameraGapInput preserves original look timestamps only; gameplay discarded, focus loss and stale axes remain zero, menu mute still clears look");
    puts("story Q/melee grouped states, native cadence, rebind and menu-mute tests passed (synthetic)"); return 0;
}
