/*
 * wubu_ns_steamrt.h -- the /n/steamrt control subtree.
 */
#ifndef WUBU_NS_STEAMRT_H
#define WUBU_NS_STEAMRT_H

/* publish the /n/steamrt tree. */
int wubu_ns_publish_steamrt(void);

/* refresh /n/steamrt/manifest. */
int wubu_ns_steamrt_refresh_manifest(void);

/* `echo "<appid> <game_lib> <proton_dist>" > /n/steamrt/env`. */
int wubu_ns_steamrt_build_env(const char *args);

/* `echo "lib1,lib2,..." > /n/steamrt/verify`. */
int wubu_ns_steamrt_verify(const char *list);

#endif
