#ifndef DEFS_H_
#define DEFS_H_

//major and minor versions
#define Vmajor 0
#define Vminor 107
//#define Vbuild 1
//#define Vdevelopment

#include <fll_sdk_version.h> //this file is placed by hand in all SDKs

#ifdef _FLL_SDK_VERSION_50_
#define __TOUCH_ENABLED__
#endif

#ifdef _FLL_SDK_VERSION_32_
#define __MSK_ENABLED__  //defined or not defined
#endif

//#define METADATA_FILES_ONLY //defined or not defined

#define RELEASE_LOG 1 // 1/0. Do not change name, otherwise change in code as well
#define BUFFER_LOG 0 //buffer log in kB, put 0 to log directly to file. Make this bigger for profiling

#endif //DEFS_H_
