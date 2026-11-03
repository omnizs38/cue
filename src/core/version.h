// The one place the version lives. res/cue.rc, the installer and the update check read it.
#pragma once
#include "build_identity.h"
#define CUE_VERSION_MAJOR 1
#define CUE_VERSION_MINOR 2
#define CUE_VERSION_PATCH 1
#define CUE_VERSION "1.2.1"
#define CUE_VERSION_SHORT "1.2.1"
// The version whose news the home shows: a fix release (1.2.1) does not bring them back.
#define CUE_NEWS_VERSION "1.2"
// Releases are published on GitHub. The update check reads the latest release of this repo.
#define CUE_UPDATE_REPO "tinypeep/cue"
// The project's contact address, shown in About (mailto: link, copied when no mail app answers).
#define CUE_CONTACT_EMAIL "tinypeepstudio@gmail.com"
