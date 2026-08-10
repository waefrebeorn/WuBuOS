/*
 * wubu_ns_session.h -- the /n/session control subtree.
 */
#ifndef WUBU_NS_SESSION_H
#define WUBU_NS_SESSION_H

/* publish the /n/session tree. */
int wubu_ns_publish_session(void);

/* `echo game > /n/session/current` — switch the session. */
int wubu_ns_session_set(const char *name);

#endif
