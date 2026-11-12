/*
 * wubu_agent.h -- the OS-side AGENT IDENTITY (the body's half of the
 * ~WuBu~ federation contract).
 *
 * The federation (WuBuFederation/agent-protocol.json, schema
 * wubu-agent-contract-v1) admits agents as `instrument` (IN) or `witness`
 * (WI) with a sponsor and an AID. The AID is:
 *
 *      AID-<CLASS>-<YY>-<NNNN>-<C>
 *
 * where C is the last digit of a mod-11 sum over YYNNNN with weights
 * 7,6,5,4,3,2, and a remainder of 10 renders as `X` (the same arithmetic the
 * WID uses, REG-2).
 *
 * This module owns that arithmetic, the local card, its persistence, its
 * SHA-256 seal, and the /n/agent control subtree -- so the OS can say who it
 * is acting for (the transparency edict: every action names a sponsor).
 *
 * C11, minimal includes, self-contained.
 */
#ifndef WUBU_AGENT_H
#define WUBU_AGENT_H

#include <stdbool.h>
#include <stddef.h>

#define WUBU_AGENT_AID_MAX       24
#define WUBU_AGENT_CLASS_MAX     4    /* "IN" | "WI" | "DO" */
#define WUBU_AGENT_SPONSOR_MAX   32
#define WUBU_AGENT_FIELD_MAX     128
#define WUBU_AGENT_CAPS_MAX      256
#define WUBU_AGENT_NOTE_MAX      256

typedef struct {
    char aid[WUBU_AGENT_AID_MAX];
    char class_code[WUBU_AGENT_CLASS_MAX];
    int  yy;                                  /* the two-digit year      */
    int  seq;                                 /* the four-digit sequence */
    char sponsor[WUBU_AGENT_SPONSOR_MAX];     /* a citizen WID or a lodge */
    char model[WUBU_AGENT_FIELD_MAX];
    char endpoint[WUBU_AGENT_FIELD_MAX];
    char capabilities[WUBU_AGENT_CAPS_MAX];   /* comma-separated         */
    char note[WUBU_AGENT_NOTE_MAX];
} WubuAgentCard;

/* -- the AID arithmetic ------------------------------------------------ */

/* The check character for (yy, seq): '0'..'9', or 'X' for a remainder of 10. */
int wubu_agent_check_digit(int yy, int seq);

/* Render "AID-<cls>-YY-NNNN-C". Returns 0 on success, -1 if the class is
 * unknown or the fields are out of range. */
int wubu_agent_format_aid(const char *cls, int yy, int seq, char *out, size_t cap);

/* Validate an AID against the mod-11 rule. Returns 1 if valid, 0 if not. */
int wubu_agent_aid_valid(const char *aid);

/* -- enrolment + persistence ------------------------------------------- */

/* Fill in the card's AID from its class/yy/seq and validate the rest.
 * Returns 0 on success, -1 if the class or sponsor is missing. */
int wubu_agent_enrol(WubuAgentCard *card);

/* Load/save the canonical card line ("AID|CLASS|YY|SEQ|SPONSOR|MODEL|...").
 * Returns 0 on success, -1 if the file is missing or malformed. */
int  wubu_agent_load(const char *path);
int  wubu_agent_save(const char *path);

const WubuAgentCard *wubu_agent_card(void);
bool                  wubu_agent_is_enroled(void);

/* -- Seal + namespace -------------------------------------------------- */

/* SHA-256 of the canonical card, as 64 lowercase hex chars + NUL.
 * The seal is what a filing or an audit row commits to. */
int wubu_agent_seal(char *hex_out, size_t cap);

/* Publish /n/agent/{aid,class,sponsor,model,endpoint,capabilities,note,
 * seal,card}. Returns 0 on success. */
int wubu_ns_publish_agent(void);

#endif /* WUBU_AGENT_H */
