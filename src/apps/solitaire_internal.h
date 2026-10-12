/*
 * solitaire_internal.h  --  private definitions for solitaire.c
 */
#ifndef WUBU_SOLITAIRE_INTERNAL_H
#define WUBU_SOLITAIRE_INTERNAL_H

#include "solitaire.h"

struct SolState {
    uint8_t tab[SOL_NPILES][SOL_MAXRUN];   /* tableau card ids */
    uint8_t tab_up[SOL_NPILES][SOL_MAXRUN];/* face-up flag */
    int     tab_count[SOL_NPILES];
    uint8_t stock[24];
    int     stock_count;
    uint8_t waste[24];
    int     waste_count;
    int     found[SOL_NFOUND];             /* top foundation card id, -1 empty */
    int     moves;
    int     won;
    /* selection */
    int     sel_pile;   /* -1 none, SOL_WASTE, or 0..6 */
    int     sel_index;
    int     selected;
};

#endif /* WUBU_SOLITAIRE_INTERNAL_H */
