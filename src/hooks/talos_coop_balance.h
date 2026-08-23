#ifndef SUDEKIMP_TALOS_COOP_BALANCE_H
#define SUDEKIMP_TALOS_COOP_BALANCE_H

#include <windows.h>

BOOL SudekiMpTalosCoopBalanceConfigure(
    BOOL enabled,
    BOOL coop_profile,
    unsigned int health_scale,
    unsigned int stagger_limit,
    unsigned int stagger_window_seconds
);
void SudekiMpTalosCoopBalanceService(void);
void SudekiMpTalosCoopBalanceReset(void);

#endif
