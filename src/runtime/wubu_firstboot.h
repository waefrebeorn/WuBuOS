/*
 * wubu_firstboot.h -- deferred provisioning: consume the configurator's plan.
 *
 * Omarchy defers all personal setup to first boot: the install writes no
 * user, and the machine's very first power-on applies language / keyboard /
 * username / password on the same frame that opened the install. This module
 * is that apply step for WuBuOS.
 *
 * wubu_firstboot_provision() turns a filled wubu_install_plan into the
 * provisioning manifest under a target root (chroot-style, so the same code
 * runs against a restore volume or a target install): a passwd line, the
 * hostname, keymap, language and timezone. The manifest carries the plan's
 * OWN salted hash -- the plaintext password was discarded at the wizard and
 * never wrote anything here. The OS supervisor at first boot applies these
 * manifest files to /etc.
 *
 * wubu_firstboot_verify() re-derives the hash from a supplied password and the
 * plan's salt and compares -- the one-way-but-verifiable contract.
 *
 * Pure C11 + stdio; links wubu_install.c (for wubu_password_hash).
 */
#ifndef WUBU_FIRSTBOOT_H
#define WUBU_FIRSTBOOT_H

#include "wubu_install.h"

/* Apply `plan` as the first-boot provisioning manifest under `root` (a
 * directory that must exist and be writable). Writes, all flat under root:
 *   wubu_passwd    "user:<salted hash>:/bin/sh"
 *   wubu_hostname  the machine name (derived from username until the wizard
 *                  asks it explicitly)
 *   wubu_keymap    the keyboard layout
 *   wubu_language  the language
 *   wubu_timezone  the timezone
 * Returns 0 on success, -1 on a null argument, an empty username, no hash, a
 * malformed hash, or any file-write failure. */
int wubu_firstboot_provision(const wubu_install_plan *plan, const char *root);

/* Re-derive the hash of `password` with the plan's salt and compare to the
 * plan's stored hash. Returns 1 = matches, 0 = does not, -1 if the plan has no
 * storeable hash. The plaintext never needs to be persisted to verify. */
int wubu_firstboot_verify(const char *password, const wubu_install_plan *plan);

#endif /* WUBU_FIRSTBOOT_H */
