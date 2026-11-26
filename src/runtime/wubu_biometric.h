/*
 * wubu_biometric.h -- sign-in biometrics as this machine actually exposes them (BioEnrollment parity).
 *
 * Windows shows "Fingerprint -- add a finger" or "no fingerprint reader". Both answers are facts about
 * the hardware and the enrolment service, so both must be MEASURED, never assumed:
 *   - the enrolment agent is a program on disk (fprintd-enroll / -list / -delete), found by path;
 *   - the reader is a device the kernel exposes (/sys/class/fingerprint*, /dev/fprint*, /dev/*finger*),
 *     with a seam for a platform that names it differently;
 *   - enrolled prints are files the service wrote under a user's or the system's fprint directory.
 *
 * The page that uses this must be able to say WHICH of those three is missing -- "biometrics
 * unavailable" with no reason is the answer that makes people think the laptop is broken.
 *
 * $WUBU_FPRINT_ROOT overrides the root the device/class/enrolment paths are resolved under (the gate
 * builds a fake machine). $WUBU_FPRINT_DEVICE names a device node directly, for a platform whose kernel
 * calls it something this module does not guess.
 *
 * C11.
 */
#ifndef WUBU_BIOMETRIC_H
#define WUBU_BIOMETRIC_H

#include <stdbool.h>
#include <stddef.h>

#define WUBU_BIO_PATH_MAX 256

typedef struct {
    bool agent_enroll;                 /* an fprintd-enroll program exists */
    bool agent_list;
    char enroll_path[WUBU_BIO_PATH_MAX];
    char list_path[WUBU_BIO_PATH_MAX];

    bool reader_present;
    char reader_path[WUBU_BIO_PATH_MAX];   /* what was found, so the page can name it */

    int  enrolled;                     /* prints on disk for this user (0 when none) */
    char enrolled_dir[WUBU_BIO_PATH_MAX];

    bool probed;                       /* the struct was filled (a zeroed struct is not a verdict) */
} WubuBiometric;

/* The root the paths resolve under: $WUBU_FPRINT_ROOT, else "/". */
const char *wubu_biometric_root(void);

/* Probe the machine. Always fills `out`; returns the number of the three facts that are PRESENT
 * (agent, reader, enrolment) so a caller can tell "nothing here" from "did not look". */
int wubu_biometric_probe(WubuBiometric *out);

/* One sentence for the page: what is missing, in the order that matters (reader first, because without
 * it an agent is useless). Empty when everything needed is present. */
void wubu_biometric_status(const WubuBiometric *b, char *out, size_t cap);

#endif
