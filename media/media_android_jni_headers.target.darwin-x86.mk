# This file is generated by gyp; do not edit.

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := GYP
LOCAL_MODULE := media_media_android_jni_headers_gyp
LOCAL_MODULE_STEM := media_android_jni_headers
LOCAL_MODULE_SUFFIX := .stamp
LOCAL_MODULE_TARGET_ARCH := $(TARGET_$(GYP_VAR_PREFIX)ARCH)
gyp_intermediate_dir := $(call local-intermediates-dir,,$(GYP_VAR_PREFIX))
gyp_shared_intermediate_dir := $(call intermediates-dir-for,GYP,shared,,,$(GYP_VAR_PREFIX))

# Make sure our deps are built first.
GYP_TARGET_DEPENDENCIES := \
	$(call intermediates-dir-for,GYP,build_android_android_exports_gyp,,,$(GYP_VAR_PREFIX))/android_exports.stamp


### Generated for rule "media_media_gyp_media_android_jni_headers_target_generate_jni_headers":
# "{'inputs': ['../base/android/jni_generator/jni_generator.py', '../android_webview/build/jarjar-rules.txt'], 'process_outputs_as_sources': '1', 'extension': 'java', 'outputs': ['$(gyp_shared_intermediate_dir)/media/jni/%(INPUT_ROOT)s_jni.h'], 'rule_name': 'generate_jni_headers', 'rule_sources': ['base/android/java/src/org/chromium/media/AudioManagerAndroid.java', 'base/android/java/src/org/chromium/media/AudioRecordInput.java', 'base/android/java/src/org/chromium/media/MediaCodecBridge.java', 'base/android/java/src/org/chromium/media/MediaDrmBridge.java', 'base/android/java/src/org/chromium/media/MediaPlayerBridge.java', 'base/android/java/src/org/chromium/media/MediaPlayerListener.java', 'base/android/java/src/org/chromium/media/UsbMidiDeviceAndroid.java', 'base/android/java/src/org/chromium/media/UsbMidiDeviceFactoryAndroid.java', 'base/android/java/src/org/chromium/media/WebAudioMediaCodecBridge.java'], 'action': ['../base/android/jni_generator/jni_generator.py', '--input_file', '$(RULE_SOURCES)', '--output_dir', '$(gyp_shared_intermediate_dir)/media/jni', '--includes', 'base/android/jni_generator/jni_generator_helper.h', '--optimize_generation', '0', '--jarjar', '../android_webview/build/jarjar-rules.txt', '--ptr_type', 'long', '--native_exports'], 'message': 'Generating JNI bindings from $(RULE_SOURCES)'}":
$(gyp_shared_intermediate_dir)/media/jni/AudioManagerAndroid_jni.h: gyp_local_path := $(LOCAL_PATH)
$(gyp_shared_intermediate_dir)/media/jni/AudioManagerAndroid_jni.h: gyp_var_prefix := $(GYP_VAR_PREFIX)
$(gyp_shared_intermediate_dir)/media/jni/AudioManagerAndroid_jni.h: gyp_intermediate_dir := $(abspath $(gyp_intermediate_dir))
$(gyp_shared_intermediate_dir)/media/jni/AudioManagerAndroid_jni.h: gyp_shared_intermediate_dir := $(abspath $(gyp_shared_intermediate_dir))
$(gyp_shared_intermediate_dir)/media/jni/AudioManagerAndroid_jni.h: export PATH := $(subst $(ANDROID_BUILD_PATHS),,$(PATH))
$(gyp_shared_intermediate_dir)/media/jni/AudioManagerAndroid_jni.h: $(LOCAL_PATH)/media/base/android/java/src/org/chromium/media/AudioManagerAndroid.java $(LOCAL_PATH)/base/android/jni_generator/jni_generator.py $(LOCAL_PATH)/android_webview/build/jarjar-rules.txt $(GYP_TARGET_DEPENDENCIES)
	@mkdir -p $(gyp_shared_intermediate_dir)/media/jni; cd $(gyp_local_path)/media; ../base/android/jni_generator/jni_generator.py --input_file base/android/java/src/org/chromium/media/AudioManagerAndroid.java --output_dir "$(gyp_shared_intermediate_dir)/media/jni" --includes base/android/jni_generator/jni_generator_helper.h --optimize_generation 0 --jarjar ../android_webview/build/jarjar-rules.txt --ptr_type long --native_exports


$(gyp_shared_intermediate_dir)/media/jni/AudioRecordInput_jni.h: gyp_local_path := $(LOCAL_PATH)
$(gyp_shared_intermediate_dir)/media/jni/AudioRecordInput_jni.h: gyp_var_prefix := $(GYP_VAR_PREFIX)
$(gyp_shared_intermediate_dir)/media/jni/AudioRecordInput_jni.h: gyp_intermediate_dir := $(abspath $(gyp_intermediate_dir))
$(gyp_shared_intermediate_dir)/media/jni/AudioRecordInput_jni.h: gyp_shared_intermediate_dir := $(abspath $(gyp_shared_intermediate_dir))
$(gyp_shared_intermediate_dir)/media/jni/AudioRecordInput_jni.h: export PATH := $(subst $(ANDROID_BUILD_PATHS),,$(PATH))
$(gyp_shared_intermediate_dir)/media/jni/AudioRecordInput_jni.h: $(LOCAL_PATH)/media/base/android/java/src/org/chromium/media/AudioRecordInput.java $(LOCAL_PATH)/base/android/jni_generator/jni_generator.py $(LOCAL_PATH)/android_webview/build/jarjar-rules.txt $(GYP_TARGET_DEPENDENCIES)
	@mkdir -p $(gyp_shared_intermediate_dir)/media/jni; cd $(gyp_local_path)/media; ../base/android/jni_generator/jni_generator.py --input_file base/android/java/src/org/chromium/media/AudioRecordInput.java --output_dir "$(gyp_shared_intermediate_dir)/media/jni" --includes base/android/jni_generator/jni_generator_helper.h --optimize_generation 0 --jarjar ../android_webview/build/jarjar-rules.txt --ptr_type long --native_exports


$(gyp_shared_intermediate_dir)/media/jni/MediaCodecBridge_jni.h: gyp_local_path := $(LOCAL_PATH)
$(gyp_shared_intermediate_dir)/media/jni/MediaCodecBridge_jni.h: gyp_var_prefix := $(GYP_VAR_PREFIX)
$(gyp_shared_intermediate_dir)/media/jni/MediaCodecBridge_jni.h: gyp_intermediate_dir := $(abspath $(gyp_intermediate_dir))
$(gyp_shared_intermediate_dir)/media/jni/MediaCodecBridge_jni.h: gyp_shared_intermediate_dir := $(abspath $(gyp_shared_intermediate_dir))
$(gyp_shared_intermediate_dir)/media/jni/MediaCodecBridge_jni.h: export PATH := $(subst $(ANDROID_BUILD_PATHS),,$(PATH))
$(gyp_shared_intermediate_dir)/media/jni/MediaCodecBridge_jni.h: $(LOCAL_PATH)/media/base/android/java/src/org/chromium/media/MediaCodecBridge.java $(LOCAL_PATH)/base/android/jni_generator/jni_generator.py $(LOCAL_PATH)/android_webview/build/jarjar-rules.txt $(GYP_TARGET_DEPENDENCIES)
	@mkdir -p $(gyp_shared_intermediate_dir)/media/jni; cd $(gyp_local_path)/media; ../base/android/jni_generator/jni_generator.py --input_file base/android/java/src/org/chromium/media/MediaCodecBridge.java --output_dir "$(gyp_shared_intermediate_dir)/media/jni" --includes base/android/jni_generator/jni_generator_helper.h --optimize_generation 0 --jarjar ../android_webview/build/jarjar-rules.txt --ptr_type long --native_exports


$(gyp_shared_intermediate_dir)/media/jni/MediaDrmBridge_jni.h: gyp_local_path := $(LOCAL_PATH)
$(gyp_shared_intermediate_dir)/media/jni/MediaDrmBridge_jni.h: gyp_var_prefix := $(GYP_VAR_PREFIX)
$(gyp_shared_intermediate_dir)/media/jni/MediaDrmBridge_jni.h: gyp_intermediate_dir := $(abspath $(gyp_intermediate_dir))
$(gyp_shared_intermediate_dir)/media/jni/MediaDrmBridge_jni.h: gyp_shared_intermediate_dir := $(abspath $(gyp_shared_intermediate_dir))
$(gyp_shared_intermediate_dir)/media/jni/MediaDrmBridge_jni.h: export PATH := $(subst $(ANDROID_BUILD_PATHS),,$(PATH))
$(gyp_shared_intermediate_dir)/media/jni/MediaDrmBridge_jni.h: $(LOCAL_PATH)/media/base/android/java/src/org/chromium/media/MediaDrmBridge.java $(LOCAL_PATH)/base/android/jni_generator/jni_generator.py $(LOCAL_PATH)/android_webview/build/jarjar-rules.txt $(GYP_TARGET_DEPENDENCIES)
	@mkdir -p $(gyp_shared_intermediate_dir)/media/jni; cd $(gyp_local_path)/media; ../base/android/jni_generator/jni_generator.py --input_file base/android/java/src/org/chromium/media/MediaDrmBridge.java --output_dir "$(gyp_shared_intermediate_dir)/media/jni" --includes base/android/jni_generator/jni_generator_helper.h --optimize_generation 0 --jarjar ../android_webview/build/jarjar-rules.txt --ptr_type long --native_exports


$(gyp_shared_intermediate_dir)/media/jni/MediaPlayerBridge_jni.h: gyp_local_path := $(LOCAL_PATH)
$(gyp_shared_intermediate_dir)/media/jni/MediaPlayerBridge_jni.h: gyp_var_prefix := $(GYP_VAR_PREFIX)
$(gyp_shared_intermediate_dir)/media/jni/MediaPlayerBridge_jni.h: gyp_intermediate_dir := $(abspath $(gyp_intermediate_dir))
$(gyp_shared_intermediate_dir)/media/jni/MediaPlayerBridge_jni.h: gyp_shared_intermediate_dir := $(abspath $(gyp_shared_intermediate_dir))
$(gyp_shared_intermediate_dir)/media/jni/MediaPlayerBridge_jni.h: export PATH := $(subst $(ANDROID_BUILD_PATHS),,$(PATH))
$(gyp_shared_intermediate_dir)/media/jni/MediaPlayerBridge_jni.h: $(LOCAL_PATH)/media/base/android/java/src/org/chromium/media/MediaPlayerBridge.java $(LOCAL_PATH)/base/android/jni_generator/jni_generator.py $(LOCAL_PATH)/android_webview/build/jarjar-rules.txt $(GYP_TARGET_DEPENDENCIES)
	@mkdir -p $(gyp_shared_intermediate_dir)/media/jni; cd $(gyp_local_path)/media; ../base/android/jni_generator/jni_generator.py --input_file base/android/java/src/org/chromium/media/MediaPlayerBridge.java --output_dir "$(gyp_shared_intermediate_dir)/media/jni" --includes base/android/jni_generator/jni_generator_helper.h --optimize_generation 0 --jarjar ../android_webview/build/jarjar-rules.txt --ptr_type long --native_exports


$(gyp_shared_intermediate_dir)/media/jni/MediaPlayerListener_jni.h: gyp_local_path := $(LOCAL_PATH)
$(gyp_shared_intermediate_dir)/media/jni/MediaPlayerListener_jni.h: gyp_var_prefix := $(GYP_VAR_PREFIX)
$(gyp_shared_intermediate_dir)/media/jni/MediaPlayerListener_jni.h: gyp_intermediate_dir := $(abspath $(gyp_intermediate_dir))
$(gyp_shared_intermediate_dir)/media/jni/MediaPlayerListener_jni.h: gyp_shared_intermediate_dir := $(abspath $(gyp_shared_intermediate_dir))
$(gyp_shared_intermediate_dir)/media/jni/MediaPlayerListener_jni.h: export PATH := $(subst $(ANDROID_BUILD_PATHS),,$(PATH))
$(gyp_shared_intermediate_dir)/media/jni/MediaPlayerListener_jni.h: $(LOCAL_PATH)/media/base/android/java/src/org/chromium/media/MediaPlayerListener.java $(LOCAL_PATH)/base/android/jni_generator/jni_generator.py $(LOCAL_PATH)/android_webview/build/jarjar-rules.txt $(GYP_TARGET_DEPENDENCIES)
	@mkdir -p $(gyp_shared_intermediate_dir)/media/jni; cd $(gyp_local_path)/media; ../base/android/jni_generator/jni_generator.py --input_file base/android/java/src/org/chromium/media/MediaPlayerListener.java --output_dir "$(gyp_shared_intermediate_dir)/media/jni" --includes base/android/jni_generator/jni_generator_helper.h --optimize_generation 0 --jarjar ../android_webview/build/jarjar-rules.txt --ptr_type long --native_exports


$(gyp_shared_intermediate_dir)/media/jni/UsbMidiDeviceAndroid_jni.h: gyp_local_path := $(LOCAL_PATH)
$(gyp_shared_intermediate_dir)/media/jni/UsbMidiDeviceAndroid_jni.h: gyp_var_prefix := $(GYP_VAR_PREFIX)
$(gyp_shared_intermediate_dir)/media/jni/UsbMidiDeviceAndroid_jni.h: gyp_intermediate_dir := $(abspath $(gyp_intermediate_dir))
$(gyp_shared_intermediate_dir)/media/jni/UsbMidiDeviceAndroid_jni.h: gyp_shared_intermediate_dir := $(abspath $(gyp_shared_intermediate_dir))
$(gyp_shared_intermediate_dir)/media/jni/UsbMidiDeviceAndroid_jni.h: export PATH := $(subst $(ANDROID_BUILD_PATHS),,$(PATH))
$(gyp_shared_intermediate_dir)/media/jni/UsbMidiDeviceAndroid_jni.h: $(LOCAL_PATH)/media/base/android/java/src/org/chromium/media/UsbMidiDeviceAndroid.java $(LOCAL_PATH)/base/android/jni_generator/jni_generator.py $(LOCAL_PATH)/android_webview/build/jarjar-rules.txt $(GYP_TARGET_DEPENDENCIES)
	@mkdir -p $(gyp_shared_intermediate_dir)/media/jni; cd $(gyp_local_path)/media; ../base/android/jni_generator/jni_generator.py --input_file base/android/java/src/org/chromium/media/UsbMidiDeviceAndroid.java --output_dir "$(gyp_shared_intermediate_dir)/media/jni" --includes base/android/jni_generator/jni_generator_helper.h --optimize_generation 0 --jarjar ../android_webview/build/jarjar-rules.txt --ptr_type long --native_exports


$(gyp_shared_intermediate_dir)/media/jni/UsbMidiDeviceFactoryAndroid_jni.h: gyp_local_path := $(LOCAL_PATH)
$(gyp_shared_intermediate_dir)/media/jni/UsbMidiDeviceFactoryAndroid_jni.h: gyp_var_prefix := $(GYP_VAR_PREFIX)
$(gyp_shared_intermediate_dir)/media/jni/UsbMidiDeviceFactoryAndroid_jni.h: gyp_intermediate_dir := $(abspath $(gyp_intermediate_dir))
$(gyp_shared_intermediate_dir)/media/jni/UsbMidiDeviceFactoryAndroid_jni.h: gyp_shared_intermediate_dir := $(abspath $(gyp_shared_intermediate_dir))
$(gyp_shared_intermediate_dir)/media/jni/UsbMidiDeviceFactoryAndroid_jni.h: export PATH := $(subst $(ANDROID_BUILD_PATHS),,$(PATH))
$(gyp_shared_intermediate_dir)/media/jni/UsbMidiDeviceFactoryAndroid_jni.h: $(LOCAL_PATH)/media/base/android/java/src/org/chromium/media/UsbMidiDeviceFactoryAndroid.java $(LOCAL_PATH)/base/android/jni_generator/jni_generator.py $(LOCAL_PATH)/android_webview/build/jarjar-rules.txt $(GYP_TARGET_DEPENDENCIES)
	@mkdir -p $(gyp_shared_intermediate_dir)/media/jni; cd $(gyp_local_path)/media; ../base/android/jni_generator/jni_generator.py --input_file base/android/java/src/org/chromium/media/UsbMidiDeviceFactoryAndroid.java --output_dir "$(gyp_shared_intermediate_dir)/media/jni" --includes base/android/jni_generator/jni_generator_helper.h --optimize_generation 0 --jarjar ../android_webview/build/jarjar-rules.txt --ptr_type long --native_exports


$(gyp_shared_intermediate_dir)/media/jni/WebAudioMediaCodecBridge_jni.h: gyp_local_path := $(LOCAL_PATH)
$(gyp_shared_intermediate_dir)/media/jni/WebAudioMediaCodecBridge_jni.h: gyp_var_prefix := $(GYP_VAR_PREFIX)
$(gyp_shared_intermediate_dir)/media/jni/WebAudioMediaCodecBridge_jni.h: gyp_intermediate_dir := $(abspath $(gyp_intermediate_dir))
$(gyp_shared_intermediate_dir)/media/jni/WebAudioMediaCodecBridge_jni.h: gyp_shared_intermediate_dir := $(abspath $(gyp_shared_intermediate_dir))
$(gyp_shared_intermediate_dir)/media/jni/WebAudioMediaCodecBridge_jni.h: export PATH := $(subst $(ANDROID_BUILD_PATHS),,$(PATH))
$(gyp_shared_intermediate_dir)/media/jni/WebAudioMediaCodecBridge_jni.h: $(LOCAL_PATH)/media/base/android/java/src/org/chromium/media/WebAudioMediaCodecBridge.java $(LOCAL_PATH)/base/android/jni_generator/jni_generator.py $(LOCAL_PATH)/android_webview/build/jarjar-rules.txt $(GYP_TARGET_DEPENDENCIES)
	@mkdir -p $(gyp_shared_intermediate_dir)/media/jni; cd $(gyp_local_path)/media; ../base/android/jni_generator/jni_generator.py --input_file base/android/java/src/org/chromium/media/WebAudioMediaCodecBridge.java --output_dir "$(gyp_shared_intermediate_dir)/media/jni" --includes base/android/jni_generator/jni_generator_helper.h --optimize_generation 0 --jarjar ../android_webview/build/jarjar-rules.txt --ptr_type long --native_exports



GYP_GENERATED_OUTPUTS := \
	$(gyp_shared_intermediate_dir)/media/jni/AudioManagerAndroid_jni.h \
	$(gyp_shared_intermediate_dir)/media/jni/AudioRecordInput_jni.h \
	$(gyp_shared_intermediate_dir)/media/jni/MediaCodecBridge_jni.h \
	$(gyp_shared_intermediate_dir)/media/jni/MediaDrmBridge_jni.h \
	$(gyp_shared_intermediate_dir)/media/jni/MediaPlayerBridge_jni.h \
	$(gyp_shared_intermediate_dir)/media/jni/MediaPlayerListener_jni.h \
	$(gyp_shared_intermediate_dir)/media/jni/UsbMidiDeviceAndroid_jni.h \
	$(gyp_shared_intermediate_dir)/media/jni/UsbMidiDeviceFactoryAndroid_jni.h \
	$(gyp_shared_intermediate_dir)/media/jni/WebAudioMediaCodecBridge_jni.h

# Make sure our deps and generated files are built first.
LOCAL_ADDITIONAL_DEPENDENCIES := $(GYP_TARGET_DEPENDENCIES) $(GYP_GENERATED_OUTPUTS)

LOCAL_GENERATED_SOURCES := \
	$(gyp_shared_intermediate_dir)/media/jni/AudioManagerAndroid_jni.h \
	$(gyp_shared_intermediate_dir)/media/jni/AudioRecordInput_jni.h \
	$(gyp_shared_intermediate_dir)/media/jni/MediaCodecBridge_jni.h \
	$(gyp_shared_intermediate_dir)/media/jni/MediaDrmBridge_jni.h \
	$(gyp_shared_intermediate_dir)/media/jni/MediaPlayerBridge_jni.h \
	$(gyp_shared_intermediate_dir)/media/jni/MediaPlayerListener_jni.h \
	$(gyp_shared_intermediate_dir)/media/jni/UsbMidiDeviceAndroid_jni.h \
	$(gyp_shared_intermediate_dir)/media/jni/UsbMidiDeviceFactoryAndroid_jni.h \
	$(gyp_shared_intermediate_dir)/media/jni/WebAudioMediaCodecBridge_jni.h

GYP_COPIED_SOURCE_ORIGIN_DIRS :=

LOCAL_SRC_FILES :=


# Flags passed to both C and C++ files.
MY_CFLAGS_Debug := \
	--param=ssp-buffer-size=4 \
	-Werror \
	-fno-strict-aliasing \
	-Wall \
	-Wno-unused-parameter \
	-Wno-missing-field-initializers \
	-fvisibility=hidden \
	-pipe \
	-fPIC \
	-Wno-unused-local-typedefs \
	-msse2 \
	-mfpmath=sse \
	-mmmx \
	-m32 \
	-ffunction-sections \
	-funwind-tables \
	-g \
	-fno-short-enums \
	-finline-limit=64 \
	-Wa,--noexecstack \
	-U_FORTIFY_SOURCE \
	-Wno-extra \
	-Wno-ignored-qualifiers \
	-Wno-type-limits \
	-Wno-unused-but-set-variable \
	-fno-stack-protector \
	-Os \
	-g \
	-gdwarf-4 \
	-fdata-sections \
	-ffunction-sections \
	-fomit-frame-pointer \
	-funwind-tables

MY_DEFS_Debug := \
	'-DV8_DEPRECATION_WARNINGS' \
	'-D_FILE_OFFSET_BITS=64' \
	'-DNO_TCMALLOC' \
	'-DDISABLE_NACL' \
	'-DCHROMIUM_BUILD' \
	'-DUSE_LIBJPEG_TURBO=1' \
	'-DENABLE_WEBRTC=1' \
	'-DUSE_PROPRIETARY_CODECS' \
	'-DENABLE_BROWSER_CDMS' \
	'-DENABLE_CONFIGURATION_POLICY' \
	'-DDISCARDABLE_MEMORY_ALWAYS_SUPPORTED_NATIVELY' \
	'-DSYSTEM_NATIVELY_SIGNALS_MEMORY_PRESSURE' \
	'-DENABLE_EGLIMAGE=1' \
	'-DCLD_VERSION=1' \
	'-DENABLE_PRINTING=1' \
	'-DENABLE_MANAGED_USERS=1' \
	'-DDATA_REDUCTION_FALLBACK_HOST="http://compress.googlezip.net:80/"' \
	'-DDATA_REDUCTION_DEV_HOST="https://proxy-dev.googlezip.net:443/"' \
	'-DDATA_REDUCTION_DEV_FALLBACK_HOST="http://proxy-dev.googlezip.net:80/"' \
	'-DSPDY_PROXY_AUTH_ORIGIN="https://proxy.googlezip.net:443/"' \
	'-DDATA_REDUCTION_PROXY_PROBE_URL="http://check.googlezip.net/connect"' \
	'-DDATA_REDUCTION_PROXY_WARMUP_URL="http://www.gstatic.com/generate_204"' \
	'-DVIDEO_HOLE=1' \
	'-DENABLE_LOAD_COMPLETION_HACKS=1' \
	'-DUSE_OPENSSL=1' \
	'-DUSE_OPENSSL_CERTS=1' \
	'-D__STDC_CONSTANT_MACROS' \
	'-D__STDC_FORMAT_MACROS' \
	'-DANDROID' \
	'-D__GNU_SOURCE=1' \
	'-DUSE_STLPORT=1' \
	'-D_STLP_USE_PTR_SPECIALIZATIONS=1' \
	'-DCHROME_BUILD_ID=""' \
	'-DDYNAMIC_ANNOTATIONS_ENABLED=1' \
	'-DWTF_USE_DYNAMIC_ANNOTATIONS=1' \
	'-D_DEBUG'


# Include paths placed before CFLAGS/CPPFLAGS
LOCAL_C_INCLUDES_Debug := \
	$(gyp_shared_intermediate_dir) \
	$(PWD)/frameworks/wilhelm/include \
	$(PWD)/bionic \
	$(PWD)/external/stlport/stlport


# Flags passed to only C++ (and not C) files.
LOCAL_CPPFLAGS_Debug := \
	-fno-exceptions \
	-fno-rtti \
	-fno-threadsafe-statics \
	-fvisibility-inlines-hidden \
	-Wsign-compare \
	-std=gnu++11 \
	-Wno-narrowing \
	-Wno-literal-suffix \
	-Wno-non-virtual-dtor \
	-Wno-sign-promo


# Flags passed to both C and C++ files.
MY_CFLAGS_Release := \
	--param=ssp-buffer-size=4 \
	-Werror \
	-fno-strict-aliasing \
	-Wall \
	-Wno-unused-parameter \
	-Wno-missing-field-initializers \
	-fvisibility=hidden \
	-pipe \
	-fPIC \
	-Wno-unused-local-typedefs \
	-msse2 \
	-mfpmath=sse \
	-mmmx \
	-m32 \
	-ffunction-sections \
	-funwind-tables \
	-g \
	-fno-short-enums \
	-finline-limit=64 \
	-Wa,--noexecstack \
	-U_FORTIFY_SOURCE \
	-Wno-extra \
	-Wno-ignored-qualifiers \
	-Wno-type-limits \
	-Wno-unused-but-set-variable \
	-fno-stack-protector \
	-Os \
	-fno-ident \
	-fdata-sections \
	-ffunction-sections \
	-fomit-frame-pointer \
	-funwind-tables

MY_DEFS_Release := \
	'-DV8_DEPRECATION_WARNINGS' \
	'-D_FILE_OFFSET_BITS=64' \
	'-DNO_TCMALLOC' \
	'-DDISABLE_NACL' \
	'-DCHROMIUM_BUILD' \
	'-DUSE_LIBJPEG_TURBO=1' \
	'-DENABLE_WEBRTC=1' \
	'-DUSE_PROPRIETARY_CODECS' \
	'-DENABLE_BROWSER_CDMS' \
	'-DENABLE_CONFIGURATION_POLICY' \
	'-DDISCARDABLE_MEMORY_ALWAYS_SUPPORTED_NATIVELY' \
	'-DSYSTEM_NATIVELY_SIGNALS_MEMORY_PRESSURE' \
	'-DENABLE_EGLIMAGE=1' \
	'-DCLD_VERSION=1' \
	'-DENABLE_PRINTING=1' \
	'-DENABLE_MANAGED_USERS=1' \
	'-DDATA_REDUCTION_FALLBACK_HOST="http://compress.googlezip.net:80/"' \
	'-DDATA_REDUCTION_DEV_HOST="https://proxy-dev.googlezip.net:443/"' \
	'-DDATA_REDUCTION_DEV_FALLBACK_HOST="http://proxy-dev.googlezip.net:80/"' \
	'-DSPDY_PROXY_AUTH_ORIGIN="https://proxy.googlezip.net:443/"' \
	'-DDATA_REDUCTION_PROXY_PROBE_URL="http://check.googlezip.net/connect"' \
	'-DDATA_REDUCTION_PROXY_WARMUP_URL="http://www.gstatic.com/generate_204"' \
	'-DVIDEO_HOLE=1' \
	'-DENABLE_LOAD_COMPLETION_HACKS=1' \
	'-DUSE_OPENSSL=1' \
	'-DUSE_OPENSSL_CERTS=1' \
	'-D__STDC_CONSTANT_MACROS' \
	'-D__STDC_FORMAT_MACROS' \
	'-DANDROID' \
	'-D__GNU_SOURCE=1' \
	'-DUSE_STLPORT=1' \
	'-D_STLP_USE_PTR_SPECIALIZATIONS=1' \
	'-DCHROME_BUILD_ID=""' \
	'-DNDEBUG' \
	'-DNVALGRIND' \
	'-DDYNAMIC_ANNOTATIONS_ENABLED=0' \
	'-D_FORTIFY_SOURCE=2'


# Include paths placed before CFLAGS/CPPFLAGS
LOCAL_C_INCLUDES_Release := \
	$(gyp_shared_intermediate_dir) \
	$(PWD)/frameworks/wilhelm/include \
	$(PWD)/bionic \
	$(PWD)/external/stlport/stlport


# Flags passed to only C++ (and not C) files.
LOCAL_CPPFLAGS_Release := \
	-fno-exceptions \
	-fno-rtti \
	-fno-threadsafe-statics \
	-fvisibility-inlines-hidden \
	-Wsign-compare \
	-std=gnu++11 \
	-Wno-narrowing \
	-Wno-literal-suffix \
	-Wno-non-virtual-dtor \
	-Wno-sign-promo


LOCAL_CFLAGS := $(MY_CFLAGS_$(GYP_CONFIGURATION)) $(MY_DEFS_$(GYP_CONFIGURATION))
LOCAL_C_INCLUDES := $(GYP_COPIED_SOURCE_ORIGIN_DIRS) $(LOCAL_C_INCLUDES_$(GYP_CONFIGURATION))
LOCAL_CPPFLAGS := $(LOCAL_CPPFLAGS_$(GYP_CONFIGURATION))
LOCAL_ASFLAGS := $(LOCAL_CFLAGS)
### Rules for final target.
# Add target alias to "gyp_all_modules" target.
.PHONY: gyp_all_modules
gyp_all_modules: media_media_android_jni_headers_gyp

# Alias gyp target name.
.PHONY: media_android_jni_headers
media_android_jni_headers: media_media_android_jni_headers_gyp

LOCAL_MODULE_PATH := $(PRODUCT_OUT)/gyp_stamp
LOCAL_UNINSTALLABLE_MODULE := true
LOCAL_2ND_ARCH_VAR_PREFIX := $(GYP_VAR_PREFIX)

include $(BUILD_SYSTEM)/base_rules.mk

$(LOCAL_BUILT_MODULE): $(LOCAL_ADDITIONAL_DEPENDENCIES)
	$(hide) echo "Gyp timestamp: $@"
	$(hide) mkdir -p $(dir $@)
	$(hide) touch $@

LOCAL_2ND_ARCH_VAR_PREFIX :=
