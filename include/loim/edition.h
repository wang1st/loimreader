#ifndef LOIM_EDITION_H
#define LOIM_EDITION_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * LoimReader ships in two editions built from one source tree:
 *   - Community: free edition, without PDF export, printing and account
 *     sign-in.
 *   - Pro: full feature set.
 * The edition is selected at configure time with the LOIM_EDITION_PRO CMake
 * option.  Pro-only code paths are wrapped in edition guards so the community
 * export can strip them from the published source tree.  Use
 * loim_edition_is_pro() for runtime gates that must stay compilable in both
 * editions (for example the sign-in button prompt).
 */
bool loim_edition_is_pro(void);

#ifdef __cplusplus
}
#endif

#endif /* LOIM_EDITION_H */
