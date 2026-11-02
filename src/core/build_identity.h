#pragma once
// Release automation opts in explicitly. Local/fork builds identify themselves
// separately; historical copyright notices remain in LICENSE/COPYRIGHT.md.
#ifndef CUE_OFFICIAL_BUILD
#define CUE_OFFICIAL_BUILD 0
#endif
#ifndef CUE_DISTRIBUTOR
#if CUE_OFFICIAL_BUILD
#define CUE_DISTRIBUTOR ".tinypeep"
#else
#define CUE_DISTRIBUTOR "Community build"
#endif
#endif
#if CUE_OFFICIAL_BUILD
#define CUE_PRODUCT_NAME "Cue"
#else
#define CUE_PRODUCT_NAME "Cue (unofficial)"
#endif
