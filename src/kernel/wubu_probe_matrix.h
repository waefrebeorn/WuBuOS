/*
 * wubu_probe_matrix.h -- shared matrix-state for the probe matrix builder.
 *
 * Split out of wubu_probe.c (the 917-line wubu_probe_build_matrix() lived
 * there as a monolith).  The builder populates g_matrix; wubu_probe_publish()
 * and the accessor read it.  Both files include this header.
 */
#ifndef WUBU_PROBE_MATRIX_H
#define WUBU_PROBE_MATRIX_H

/* The human-readable machine matrix, NUL-terminated.
 * Written by wubu_probe_build_matrix(), read by wubu_probe_publish()
 * and wubu_probe_matrix(). */
extern char g_matrix[];
#define WUBU_MATRIX_SIZE 16384

/* W2: build the matrix string (populates g_matrix). */
void wubu_probe_build_matrix(void);

/* W5: accessor for the built matrix string. */
const char *wubu_probe_matrix(void);

#endif /* WUBU_PROBE_MATRIX_H */
