#include "runtime/Runtime.hpp"
#include <jni.h>

namespace pl::mod { class ModContext; }

extern "C" __attribute__((visibility("default")))
bool PLMod_Load(JavaVM*, const PLModInfo* info) {
    return appleskin::Runtime::get().load(appleskin::legacyLocation(info));
}

extern "C" __attribute__((visibility("default")))
bool PLMod_Enable() { return appleskin::Runtime::get().enable(); }

extern "C" __attribute__((visibility("default")))
bool PLMod_Disable() { return appleskin::Runtime::get().disable(); }

extern "C" __attribute__((visibility("default")))
bool PLMod_Unload() { return appleskin::Runtime::get().unload(); }

namespace {
struct Registration {
    void* instance;
    bool (*load)(void*, pl::mod::ModContext&);
    bool (*enable)(void*, pl::mod::ModContext&);
    bool (*disable)(void*, pl::mod::ModContext&);
    bool (*unload)(void*, pl::mod::ModContext&);
};
bool cppLoad(void*, pl::mod::ModContext& context) {
    return appleskin::Runtime::get().load(appleskin::contextLocation(&context));
}
bool cppEnable(void*, pl::mod::ModContext&) { return PLMod_Enable(); }
bool cppDisable(void*, pl::mod::ModContext&) { return PLMod_Disable(); }
bool cppUnload(void*, pl::mod::ModContext&) { return PLMod_Unload(); }
int identity;
Registration registration{&identity, cppLoad, cppEnable, cppDisable, cppUnload};
}

extern "C" __attribute__((visibility("default")))
Registration* PLGetModRegistration() { return &registration; }
