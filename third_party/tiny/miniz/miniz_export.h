/* miniz_export.h -- SHIM, not upstream.
 *
 * Upstream generates this from miniz_export.h.in via CMake; it is absent from
 * the repository (verified: 404 on both main and master). Without it
 * miniz.h fails at line 115 and the "single file library" cannot compile at
 * all -- which is exactly why a vendored file needs a build gate, not a hash.
 *
 * Empty default visibility is what CMake emits on a static build.
 */
#ifndef MINIZ_EXPORT
#define MINIZ_EXPORT
#endif
