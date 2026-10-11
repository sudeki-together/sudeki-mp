/* Private implementation included by lan_story_spawn_trace.c. Observe the
 * animation destructor's existing effect release, including delayed resource
 * destruction. Do not patch 531DF0: other adapters validate its entire body. */
enum { EFFECT_OWNER_RETIRE_CALL=0xe1870u, EFFECT_RETIRE=0x131df0u };
static SudekiMpRelativeCallHook effect_retire_hook,effect_animation_retire_hook;
static SudekiMpInlineHook effect_finished_retire_hook;
static void (__stdcall *effect_retire_original)(void *);
typedef struct EffectRetireCall {
    uint8_t *effect;
    DWORD thread,error;
    BOOL entered,active;
} EffectRetireCall;
static EffectRetireCall *retire_active;
static BOOL effect_retire_code(uint8_t *b) {
    return readable(b+0xe1840u,0x42u) && readable(b+EFFECT_RETIRE,9u) &&
        !memcmp(b+0xe1840u,"\x53\x56\x8b\xf1\x57\x8b\xbe\x00\x01\x00\x00",11u) &&
        b[0xe186fu]==0x57 &&
        !memcmp(b+0xe1875u,"\x80\xa7\xe0\x03\x00\x00\xfe\x89\x9e\x00\x01\x00\x00",13u) &&
        !memcmp(b+EFFECT_RETIRE,"\x55\x8b\xec\x83\xe4\xf8\x8b\x4d\x08",9u);
}
static BOOL effect_retire_witness(void *context) {
    EffectRetireCall *c=context;
    /* Native retirement can invalidate effect storage. Only this active stack
     * and installed callsite are used by the post-call witness. */
    int32_t displacement;
    if(c!=retire_active || !c->active || !retire_callbacks ||
        c->thread!=GetCurrentThreadId() || !effect_retire_hook.installed ||
        !readable(effect_retire_hook.instruction,5u) || *effect_retire_hook.instruction!=0xe8) return FALSE;
    memcpy(&displacement,effect_retire_hook.instruction+1u,4u);
    return displacement==effect_retire_hook.replacement_displacement;
}
static BOOL effect_retire_operation(void *context) {
    EffectRetireCall *c=context; c->entered=TRUE;
    SetLastError(c->error); effect_retire_original(c->effect); c->error=GetLastError();
    return TRUE;
}
static void __stdcall effect_owner_retire(void *raw) {
    EffectRetireCall c={.effect=raw,.thread=GetCurrentThreadId(),.error=GetLastError(),.active=TRUE};
    ++retire_callbacks;
    uint32_t sigil=sigil_retiring(raw);
    EffectRetireCall *previous=retire_active;
    BOOL exact=!previous && client_role && in_registry(raw) && fx_exact(raw);
    BOOL accounted=FALSE;
    if(exact) {
        retire_active=&c;
        accounted=SudekiMpLanStoryClientEffectRetireCall(effect_retire_witness,&c,raw,effect_retire_operation);
    }
    /* An enclosing residency call already accounts nested cleanup. A refused
     * call still executes native work exactly once; never manufacture cancel. */
    if(!c.entered) (void)effect_retire_operation(&c);
    sigil_retired(sigil);
    if(exact) SudekiMpLogFormat("story_spawn_trace event=owner_effect_retire accounted=%u\r\n",accounted);
    retire_active=previous; c.active=FALSE; --retire_callbacks; SetLastError(c.error);
}
/* Both sites already enter stdcall 531DF0(effect). The update's tail JMP
 * has restored its stack/registers. Its generated inline trampoline is never
 * executed (it contains a relative branch); call the proved target directly. */
static void __stdcall effect_observed_retire(void *raw) {
    DWORD error=GetLastError(); ++retire_callbacks;
    uint32_t serial=sigil_retiring(raw);
    SetLastError(error); effect_retire_original(raw); error=GetLastError();
    sigil_retired(serial); --retire_callbacks; SetLastError(error);
}
static BOOL effect_end_sites_code(uint8_t *b) {
    int32_t call,jump;
    if(!readable(b+0xe2161u,20u) || !readable(b+RVA_FX_RETIRE_TAIL-4u,9u) ||
        b[0xe2161u]!=0x57 || b[0xe2162u]!=0xe8 ||
        memcmp(b+0xe2167u,"\x80\xa7\xe0\x03\x00\x00\xfe\xc7\x03\x00\x00\x00\x00",13u) ||
        memcmp(b+RVA_FX_RETIRE_TAIL-4u,"\x89\x44\x24\x04\xe9",5u)) return FALSE;
    memcpy(&call,b+0xe2163u,4u); memcpy(&jump,b+RVA_FX_RETIRE_TAIL+1u,4u);
    return b+0xe2167u+call==b+EFFECT_RETIRE && b+RVA_FX_RETIRE_TAIL+5u+jump==b+EFFECT_RETIRE;
}
static BOOL effect_retire_install(uint8_t *b) {
    if(!effect_retire_code(b) || !effect_end_sites_code(b)) return FALSE;
    effect_retire_original=(void (__stdcall *)(void *))(b+EFFECT_RETIRE);
    if(!SudekiMpInstallRelativeCallHook(&effect_retire_hook,b+EFFECT_OWNER_RETIRE_CALL,
        b+EFFECT_RETIRE,(void *)effect_owner_retire)) return FALSE;
    if(!SudekiMpInstallRelativeCallHook(&effect_animation_retire_hook,b+0xe2162u,
        b+EFFECT_RETIRE,(void *)effect_observed_retire)) goto fail;
    uint8_t jump[5]={0xe9}; int32_t displacement=(int32_t)(EFFECT_RETIRE-RVA_FX_RETIRE_TAIL-5u);
    memcpy(jump+1u,&displacement,4u);
    if(!SudekiMpInstallInlineHook(&effect_finished_retire_hook,b+RVA_FX_RETIRE_TAIL,jump,sizeof(jump),
        (void *)effect_observed_retire)) goto fail;
    return TRUE;
fail:
    (void)SudekiMpRestoreRelativeCallHook(&effect_animation_retire_hook);
    (void)SudekiMpRestoreRelativeCallHook(&effect_retire_hook);
    return FALSE;
}
static BOOL effect_retire_uninstall(void) {
    if(retire_callbacks) {SetLastError(ERROR_BUSY);return FALSE;}
    BOOL ok=SudekiMpRestoreInlineHook(&effect_finished_retire_hook);
    if(!SudekiMpRestoreRelativeCallHook(&effect_animation_retire_hook)) ok=FALSE;
    if(!SudekiMpRestoreRelativeCallHook(&effect_retire_hook)) ok=FALSE;
    return ok;
}
