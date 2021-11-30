APP_ABI := armeabi-v7a arm64-v8a x86_64
#APP_STL := gnustl_static
APP_PLATFORM :=android-23
APP_CPP_FEATURES += exceptions rtti
ifdef APP_DEBUG
   ifeq ($(APP_DEBUG),true)
      CFLAGS+=  -O0 -g
      LOCAL_CFLAGS+=    -D_DEBUG
      APP_OPTIM := debug
   else
      CFLAGS+=  -O2 -g
      LOCAL_CFLAGS+=    -DNDEBUG
      APP_OPTIM := release
  endif
endif
