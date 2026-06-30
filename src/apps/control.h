/*
 * control.h  --  WuBuOS Control Panel API
 *
 * Cell 395: Win98-style settings panel with tabs.
 */
#ifndef WUBU_CONTROL_H
#define WUBU_CONTROL_H

/* Tab indices */
#define CTRL_DISPLAY      0
#define CTRL_THEME        1
#define CTRL_DESKTOP      2
#define CTRL_TASKBAR      3
#define CTRL_INPUT        4
#define CTRL_STARTUP      5
#define CTRL_CONTAINERS   6
#define CTRL_NETWORK      7
#define CTRL_ABOUT        8
#define CTRL_TAB_COUNT    9

/* API */
void control_open(void);
void control_init(void);
void control_shutdown(void);

#endif /* WUBU_CONTROL_H */