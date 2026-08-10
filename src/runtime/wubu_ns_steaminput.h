/*
 * wubu_ns_steaminput.h -- the /n/steaminput control subtree.
 */
#ifndef WUBU_NS_STEAMINPUT_H
#define WUBU_NS_STEAMINPUT_H

/* publish the /n/steaminput tree. */
int wubu_ns_publish_steaminput(void);

/* `echo <64 hex bytes> > /n/steaminput/report` — parse a Deck
 * controller report. Returns the events, -1 on error. */
int wubu_ns_steaminput_report(const char *hex);

/* refresh /n/steaminput/battery from the module state. */
int wubu_ns_steaminput_refresh_battery(void);

#endif
