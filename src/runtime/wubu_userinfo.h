/*
 * wubu_userinfo.h -- who is signed in, as the machine defines it (AccountsControl parity).
 *
 * Windows shows an account page: name, picture, type, and what you can change. WuBuOS had account
 * RECORDS (src/runtime/wubu_account.c -- user + password verification) and nothing that could answer
 * "who am I" without a shell. Two rules, same as the firmware and print surfaces:
 *   - every value is read from the machine (passwd, the home directory, an executable's presence);
 *   - when something is not available it says so, instead of drawing a button that cannot work.
 *
 * $WUBU_PASSWD_ROOT overrides the passwd tree so the gate runs against a fake /etc/passwd.
 *
 * C11.
 */
#ifndef WUBU_USERINFO_H
#define WUBU_USERINFO_H

#include <stdbool.h>
#include <stddef.h>

#define WUBU_USER_PATH_MAX 256

typedef struct {
    bool found;                          /* the passwd tree named this uid */
    int  uid;
    int  gid;
    char login[64];
    char gecos[128];                     /* display name, when the system set one */
    char home[WUBU_USER_PATH_MAX];
    char shell[128];
    bool has_face;                       /* ~/.face (the picture every desktop looks for) */
    bool can_change_password;            /* a passwd binary exists here */
    char passwd_tool[WUBU_USER_PATH_MAX];
} WubuUserInfo;

/* The passwd tree in use: $WUBU_PASSWD_ROOT, else "/" (so /etc/passwd). */
const char *wubu_userinfo_passwd_root(void);

/* Read the passwd record for uid. Returns 0 when found, -1 when the tree has no such user. */
int wubu_userinfo_read(int uid, WubuUserInfo *out);

/* The name to show: gecos when present, else login, else "". Static buffer. */
const char *wubu_userinfo_display(const WubuUserInfo *u);

/* "Administrator" when uid == 0, else "Standard user". */
const char *wubu_userinfo_type(const WubuUserInfo *u);

#endif
