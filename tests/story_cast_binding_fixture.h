/* Included by StoryCastAdmissionTest. Exercise the real binding/rollback
 * coordinator with inert native dependencies, including a busy second caster.
 * This is lifecycle proof, not proof that retail hooks install. */
static unsigned binding_attempts,binding_creates,binding_destroys,binding_resets;
static unsigned binding_failure_at;
static DWORD binding_error;
static BOOL binding_restore=TRUE,binding_bank_failure;
uint8_t SudekiMpInvokeSkillUse(void *s,void *edx,int slot,SudekiMpSkillUseFunction original) {
    (void)s;(void)edx;(void)slot;(void)original;assert(!"Binding must not cast a skill");return 0;
}
BOOL SudekiMpLanStoryObserverRosterStillExact(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r) { return identity && w && r; }
BOOL SudekiMpLanPartyControlEnableCastInputIsolation(const SudekiMpControlUpdateDispatchWitness *w) {
    assert(w && !bound); ++binding_attempts; return TRUE;
}
BOOL SudekiMpLanStoryTaskHostExact(HMODULE m) { return m==image; }
BOOL SudekiMpLanStoryTaskHostAttach(HMODULE m,SudekiMpLanCastCreatedObserver c,SudekiMpLanCastStepAdapter s) {
    (void)m;(void)c;(void)s;return TRUE;
}
BOOL SudekiMpLanStoryTaskHostDetach(SudekiMpLanCastCreatedObserver c,SudekiMpLanCastStepAdapter s) {
    (void)c;(void)s;return TRUE;
}
BOOL SudekiMpInstallLanCastContextWithTaskHost(HMODULE m,SudekiMpLanCastOwnerWitness w,
    const SudekiMpLanCastTaskHost *h) { assert(m==image && w==root_owner && h);return TRUE; }
BOOL SudekiMpUninstallLanCastContext(void) { return TRUE; }
BOOL SudekiMpInitializeSpiritInstanceAbi(HMODULE m,SudekiMpSpiritInstanceIdleWitness w) {
    assert(m==image && w==all_idle && w());return TRUE;
}
BOOL SudekiMpResetSpiritInstanceAbi(void) { ++binding_resets;return binding_restore; }
BOOL SudekiMpInstallSpiritInstanceUpdates(void) { return TRUE; }
BOOL SudekiMpEnableSpiritInstanceNamedCameraBanking(void) {
    if(binding_bank_failure) { if(binding_error)SetLastError(binding_error);return FALSE; }
    return TRUE;
}
BOOL SudekiMpCreateSpiritInstance(SudekiMpSpiritInstance *s) {
    ++binding_creates;
    assert(all_idle() && !s->generation);
    if(binding_creates==binding_failure_at) { SetLastError(binding_error);return FALSE; }
    unsigned i; /* Resolve sparse fixture explicitly. */
    for(i=0;i<4;++i)if(s==&caster[i].instance)break;
    assert(i<4);s->generation=i+1;s->manager=skills[i];s->camera=actors[i];return TRUE;
}
BOOL SudekiMpDestroySpiritInstance(SudekiMpSpiritInstance *s) {
    assert(all_idle());if(!binding_restore)return FALSE;
    ++binding_destroys;memset(s,0,sizeof(*s));return TRUE;
}
BOOL SudekiMpBindSpiritInstanceCaster(const SudekiMpSpiritInstance *s,void *a,uint8_t type,
    uint64_t life,SudekiMpSpiritCasterWitness w) {
    assert(s->generation && ix(a)>=0 && type && life==lifetime && w==retained_actor);return TRUE;
}
#define BIND_INSTANCE_OK(name) BOOL name(const SudekiMpSpiritInstance *s) { assert(s->generation);return TRUE; }
BIND_INSTANCE_OK(SudekiMpEnableSpiritInstanceRemoteUi)
BIND_INSTANCE_OK(SudekiMpEnableSpiritInstanceRemoteSkillUi)
BIND_INSTANCE_OK(SudekiMpEnableSpiritInstanceNamedCameras)
BIND_INSTANCE_OK(SudekiMpEnableSpiritInstanceRemoteCameraSelection)
BIND_INSTANCE_OK(SudekiMpScheduleSpiritInstanceManager)
#undef BIND_INSTANCE_OK
BOOL SudekiMpEnableSpiritInstanceRemoteSkillInput(const SudekiMpSpiritInstance *s,void *a,
    SudekiMpSpiritCasterWitness w) { assert(s->generation && a && w);return TRUE; }
BOOL SudekiMpConfigureSpiritInstanceSkillTiming(const SudekiMpSpiritInstance *s,BOOL replica) {
    assert(s->generation && !replica);return TRUE;
}
#define BIND_OK(name) BOOL name(void) { return TRUE; }
BIND_OK(SudekiMpInstallSpiritInstanceNamedCameraUpdates)
BIND_OK(SudekiMpEnableSpiritInstanceCastGates)
BIND_OK(SudekiMpEnableSpiritInstanceLighting)
BIND_OK(SudekiMpEnableSpiritInstanceSkillTargeting)
BIND_OK(SudekiMpEnableSpiritInstanceSharedSsp)
BIND_OK(SudekiMpUninstallCastMotionBlur)
BIND_OK(SudekiMpUninstallQuickSkillInputTrace)
BIND_OK(SudekiMpUninstallLanArenaSpiritAudioTrace)
BIND_OK(SudekiMpLanArenaSpiritVisualHostReset)
BIND_OK(SudekiMpLanPartyEffectLifetimeReset)
BIND_OK(SudekiMpLanPartyProjectileLifetimeUninstall)
#undef BIND_OK
BOOL SudekiMpInstallCastMotionBlur(HMODULE m,SudekiMpCastMotionBlurScope s) { assert(m==image && s);return TRUE; }
BOOL SudekiMpInstallInlineHook(SudekiMpInlineHook *h,uint8_t *p,const uint8_t *e,size_t n,const void *r) {
    assert(p && e && n && r);h->installed=TRUE;return TRUE;
}
BOOL SudekiMpRestoreInlineHook(SudekiMpInlineHook *h) { h->installed=FALSE;return TRUE; }
BOOL SudekiMpInstallRelativeCallHook(SudekiMpRelativeCallHook *h,uint8_t *p,const void *e,const void *r) {
    assert(p && e && r);h->installed=TRUE;return TRUE;
}
BOOL SudekiMpRestoreRelativeCallHook(SudekiMpRelativeCallHook *h) { h->installed=FALSE;return TRUE; }
BOOL SudekiMpQuickSkillSpiritRoutingReady(void) { return FALSE; }
BOOL SudekiMpInstallQuickSkillInputTrace(HMODULE m,BOOL a,BOOL b) { assert(m==image && !a && !b);return TRUE; }
BOOL SudekiMpSetSpiritActivationRouting(SudekiMpSpiritRoutingIdleWitness w,
    SudekiMpSpiritRoutingEnter e,SudekiMpSpiritRoutingLeave l) { assert((!w==!e) && (!w==!l));return TRUE; }
BOOL SudekiMpSetSkillActivationRouting(SudekiMpSkillRoutingIdleWitness w,
    SudekiMpSkillRoutingEnter e,SudekiMpSkillRoutingLeave l) { assert((!w==!e) && (!w==!l));return TRUE; }
BOOL SudekiMpInstallLanArenaSpiritAudioTrace(HMODULE m,SudekiMpLanArenaSpiritActiveWitness w,void *c) {
    assert(m==image && w==audio_witness && !c);return TRUE;
}
BOOL SudekiMpLanPartyProjectileLifetimeInstall(HMODULE m) { assert(m==image);return TRUE; }
BOOL SudekiMpLanArenaSpiritVisualHostInitialize(HMODULE m,SudekiMpLanArenaSpiritVisualHostWitness w,void *c) {
    assert(m==image && w==visual_owner && !c);return TRUE;
}
BOOL SudekiMpLanPartySpiritVisualHostSetShieldWitness(HMODULE m,SudekiMpLanPartyShieldHostWitness w) {
    assert(m==image && w==shield_owner);return TRUE;
}
BOOL SudekiMpLanPartyEffectLifetimeInitialize(HMODULE m,SudekiMpLanPartyEffectWitness w,BOOL (*d)(void)) {
    assert(m==image && w==effect_owner && d==shutdown_drained);return TRUE;
}
BOOL SudekiMpLanPartySpiritVisualHostCapture(uint64_t life,BOOL idle,SudekiMpStoryVfx *out) {
    assert(bound && life==lifetime && idle);memset(out,0,sizeof(*out));return TRUE;
}
BOOL SudekiMpLanStoryShotsAppendVisuals(const SudekiMpLanStoryNativeRoster *r,SudekiMpStoryVfx *out) {
    assert(r==&owners && out);return TRUE;
}
BOOL SudekiMpStoryVfxValid(const SudekiMpStoryVfx *v) { return v && v->stream; }
static void cast_binding_lifecycle(void) {
    HMODULE fixture_image=image;
    SudekiMpControlUpdateDispatchWitness w={.dispatch_serial=5,.service_post_original_exact=TRUE};
    for(unsigned mode=0;mode<5;++mode) {
        image=fixture_image;reset();
        SudekiMpLanStoryNativeRoster r=owners;
        memset(caster,0,sizeof(caster));memset(&owners,0,sizeof(owners));
        bound=initialized=context_owned=menu_owned=skill_routed=spirit_routed=FALSE;
        effects_owned=missiles_owned=visuals_owned=audio_owned=bind_deferred=FALSE;
        binding_attempts=binding_creates=binding_destroys=binding_resets=0;
        binding_restore=mode!=2;binding_bank_failure=mode>=3;
        binding_failure_at=2;binding_error=mode==1?ERROR_INVALID_DATA:ERROR_BUSY;
        if(mode==3)binding_error=0; /* Failure without an error must not inherit stale BUSY. */
        SetLastError(ERROR_BUSY);
        BOOL retry=SudekiMpLanStoryCastTryBind(&w,&r);
        assert(!bound && binding_attempts==1);
        if(mode==0 || mode==4) {
            assert(retry && bind_deferred && !fault && !stopping && !SudekiMpLanStoryCastRetains());
            assert(binding_destroys==(mode==0?1u:0u) && binding_resets==1);
            assert(SudekiMpLanStoryCastTryBind(&w,&r) && binding_attempts==1); /* Backoff. */
            bind_deferred_at=GetTickCount()-1000u;
            identity=FALSE;assert(!SudekiMpLanStoryCastTryBind(&w,&r) && binding_attempts==1);identity=TRUE;
            front[0x8d]=1;assert(SudekiMpLanStoryCastTryBind(&w,&r) && binding_attempts==1);front[0x8d]=0;
            binding_failure_at=0;binding_bank_failure=FALSE;
            assert(SudekiMpLanStoryCastTryBind(&w,&r) && SudekiMpLanStoryCastReady());
            SudekiMpStoryVfx v;
            assert(SudekiMpLanStoryCastCaptureVisuals(&v) && v.stream==lifetime && audio_owned);
            assert(binding_attempts==2 && !bind_deferred);
        } else {
            assert(!retry && !bind_deferred && stopping);
            assert((mode==2)==(SudekiMpLanStoryCastRetains()!=FALSE));
            assert(!SudekiMpLanStoryCastTryBind(&w,&r) && binding_attempts==1);
        }
        /* Test fixture releases its injected restoration failure. Production
         * never clears a retained dependency to pretend teardown succeeded. */
        binding_restore=TRUE;
        assert(SudekiMpLanStoryCastUninstall() && !SudekiMpLanStoryCastRetains());
    }
    image=fixture_image;reset();
    puts("PASS binding busy rollback/retry restores casts and VFX; backoff, fresh preflight, invalid/unknown and retained failures stay closed");
}
