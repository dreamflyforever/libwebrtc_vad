LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := fvad
LOCAL_SRC_FILES :=  ../src/fvad.c ../src/signal_processing/division_operations.c ../src/signal_processing/energy.c ../src/signal_processing/get_scaling_square.c ../src/signal_processing/resample_48khz.c ../src/signal_processing/resample_by_2_internal.c ../src/signal_processing/resample_fractional.c ../src/signal_processing/spl_inl.c ../src/vad/vad_core.c ../src/vad/vad_filterbank.c ../src/vad/vad_gmm.c ../src/vad/vad_sp.c
include $(BUILD_SHARED_LIBRARY)
