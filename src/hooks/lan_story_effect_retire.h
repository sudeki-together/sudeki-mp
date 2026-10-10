/* Private implementation included by lan_story_spawn_trace.c. Observe the
 * animation destructor's existing effect release, including delayed resource
 * destruction. Do not patch 531DF0: other adapters validate its entire body. */
enum { EFFECT_OWNER_RETIRE_CALL=0xe1870u, EFFECT_RETIRE=0x131df0u };
static SudekiMpRelativeCallHook effect_retire_hook;
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
    if(exact) SudekiMpLogFormat("story_spawn_trace event=owner_effect_retire accounted=%u\r\n",accounted);
    retire_active=previous; c.active=FALSE; --retire_callbacks; SetLastError(c.error);
}
static BOOL effect_retire_install(uint8_t *b) {
    if(!effect_retire_code(b)) return FALSE;
    effect_retire_original=(void (__stdcall *)(void *))(b+EFFECT_RETIRE);
    return SudekiMpInstallRelativeCallHook(&effect_retire_hook,b+EFFECT_OWNER_RETIRE_CALL,
        b+EFFECT_RETIRE,(void *)effect_owner_retire);
}
static BOOL effect_retire_uninstall(void) {
    if(retire_callbacks) {SetLastError(ERROR_BUSY);return FALSE;}
    return SudekiMpRestoreRelativeCallHook(&effect_retire_hook);
}
