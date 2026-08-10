/*
 * wubu_ns_ec.h -- the /n/ec control subtree (the handheld EC).
 */
#ifndef WUBU_NS_EC_H
#define WUBU_NS_EC_H

/* publish the /n/ec tree (after wubu_ns_bridge_create + EC probe). */
int wubu_ns_publish_ec(void);

/* `echo 30 > /n/ec/pwm` — set the manual fan duty. */
int wubu_ns_ec_set_pwm(int percent);

/* `echo 2 > /n/ec/mode` — set the fan mode (1 manual / 2 auto). */
int wubu_ns_ec_set_mode(int mode);

/* refresh /n/ec/fan + /n/ec/temp. */
int wubu_ns_ec_refresh(void);

#endif
