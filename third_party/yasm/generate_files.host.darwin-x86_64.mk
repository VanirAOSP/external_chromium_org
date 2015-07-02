# This file is generated by gyp; do not edit.

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := GYP
LOCAL_MODULE := third_party_yasm_generate_files_$(TARGET_$(GYP_VAR_PREFIX)ARCH)_host_gyp
LOCAL_MODULE_STEM := generate_files
LOCAL_MODULE_SUFFIX := .stamp
LOCAL_IS_HOST_MODULE := true
LOCAL_MULTILIB := $(GYP_HOST_MULTILIB)
gyp_intermediate_dir := $(call local-intermediates-dir,,$(GYP_HOST_VAR_PREFIX))
gyp_shared_intermediate_dir := $(call intermediates-dir-for,GYP,shared,,,$(GYP_VAR_PREFIX))

# Make sure our deps are built first.
GYP_TARGET_DEPENDENCIES := \
	$(gyp_shared_intermediate_dir)/genperf \
	$(gyp_shared_intermediate_dir)/genversion

### Rules for action "generate_x86_insn":
$(gyp_shared_intermediate_dir)/third_party/yasm/x86insns.c: gyp_local_path := $(LOCAL_PATH)
$(gyp_shared_intermediate_dir)/third_party/yasm/x86insns.c: gyp_var_prefix := $(GYP_VAR_PREFIX)
$(gyp_shared_intermediate_dir)/third_party/yasm/x86insns.c: gyp_intermediate_dir := $(abspath $(gyp_intermediate_dir))
$(gyp_shared_intermediate_dir)/third_party/yasm/x86insns.c: gyp_shared_intermediate_dir := $(abspath $(gyp_shared_intermediate_dir))
$(gyp_shared_intermediate_dir)/third_party/yasm/x86insns.c: export PATH := $(subst $(ANDROID_BUILD_PATHS),,$(PATH))
$(gyp_shared_intermediate_dir)/third_party/yasm/x86insns.c: $(LOCAL_PATH)/third_party/yasm/source/patched-yasm/modules/arch/x86/gen_x86_insn.py $(GYP_TARGET_DEPENDENCIES)
	@echo "Gyp action: Running source/patched-yasm/modules/arch/x86/gen_x86_insn.py ($@)"
	$(hide)cd $(gyp_local_path)/third_party/yasm; mkdir -p $(gyp_shared_intermediate_dir)/third_party/yasm; python source/patched-yasm/modules/arch/x86/gen_x86_insn.py "$(gyp_shared_intermediate_dir)/third_party/yasm"

$(gyp_shared_intermediate_dir)/third_party/yasm/x86insn_gas.gperf: $(gyp_shared_intermediate_dir)/third_party/yasm/x86insns.c ;
$(gyp_shared_intermediate_dir)/third_party/yasm/x86insn_nasm.gperf: $(gyp_shared_intermediate_dir)/third_party/yasm/x86insns.c ;

### Rules for action "generate_version":
$(gyp_shared_intermediate_dir)/third_party/yasm/version.mac: gyp_local_path := $(LOCAL_PATH)
$(gyp_shared_intermediate_dir)/third_party/yasm/version.mac: gyp_var_prefix := $(GYP_VAR_PREFIX)
$(gyp_shared_intermediate_dir)/third_party/yasm/version.mac: gyp_intermediate_dir := $(abspath $(gyp_intermediate_dir))
$(gyp_shared_intermediate_dir)/third_party/yasm/version.mac: gyp_shared_intermediate_dir := $(abspath $(gyp_shared_intermediate_dir))
$(gyp_shared_intermediate_dir)/third_party/yasm/version.mac: export PATH := $(subst $(ANDROID_BUILD_PATHS),,$(PATH))
$(gyp_shared_intermediate_dir)/third_party/yasm/version.mac: $(gyp_shared_intermediate_dir)/genversion $(GYP_TARGET_DEPENDENCIES)
	@echo "Gyp action: Generating yasm version file: $(gyp_shared_intermediate_dir)/third_party/yasm/version.mac ($@)"
	$(hide)cd $(gyp_local_path)/third_party/yasm; mkdir -p $(gyp_shared_intermediate_dir)/third_party/yasm; "$(gyp_shared_intermediate_dir)/genversion" "$(gyp_shared_intermediate_dir)/third_party/yasm/version.mac"




### Generated for rule "third_party_yasm_yasm_gyp_generate_files_host_generate_gperf":
# "{'inputs': ['$(gyp_shared_intermediate_dir)/genperf'], 'extension': 'gperf', 'process_outputs_as_sources': '0', 'outputs': ['$(gyp_shared_intermediate_dir)/third_party/yasm/%(INPUT_ROOT)s.c'], 'rule_name': 'generate_gperf', 'rule_sources': ['source/patched-yasm/modules/arch/x86/x86cpu.gperf', 'source/patched-yasm/modules/arch/x86/x86regtmod.gperf'], 'action': ['$(gyp_shared_intermediate_dir)/genperf', '$(RULE_SOURCES)', '$(gyp_shared_intermediate_dir)/third_party/yasm/%(INPUT_ROOT)s.c'], 'message': 'yasm genperf for $(RULE_SOURCES)'}":
$(gyp_shared_intermediate_dir)/third_party/yasm/x86cpu.c: gyp_local_path := $(LOCAL_PATH)
$(gyp_shared_intermediate_dir)/third_party/yasm/x86cpu.c: gyp_var_prefix := $(GYP_VAR_PREFIX)
$(gyp_shared_intermediate_dir)/third_party/yasm/x86cpu.c: gyp_intermediate_dir := $(abspath $(gyp_intermediate_dir))
$(gyp_shared_intermediate_dir)/third_party/yasm/x86cpu.c: gyp_shared_intermediate_dir := $(abspath $(gyp_shared_intermediate_dir))
$(gyp_shared_intermediate_dir)/third_party/yasm/x86cpu.c: export PATH := $(subst $(ANDROID_BUILD_PATHS),,$(PATH))
$(gyp_shared_intermediate_dir)/third_party/yasm/x86cpu.c: $(LOCAL_PATH)/third_party/yasm/source/patched-yasm/modules/arch/x86/x86cpu.gperf $(gyp_shared_intermediate_dir)/genperf $(GYP_TARGET_DEPENDENCIES)
	@mkdir -p $(gyp_shared_intermediate_dir)/third_party/yasm; cd $(gyp_local_path)/third_party/yasm; "$(gyp_shared_intermediate_dir)/genperf" source/patched-yasm/modules/arch/x86/x86cpu.gperf "$(gyp_shared_intermediate_dir)/third_party/yasm/x86cpu.c"


$(gyp_shared_intermediate_dir)/third_party/yasm/x86regtmod.c: gyp_local_path := $(LOCAL_PATH)
$(gyp_shared_intermediate_dir)/third_party/yasm/x86regtmod.c: gyp_var_prefix := $(GYP_VAR_PREFIX)
$(gyp_shared_intermediate_dir)/third_party/yasm/x86regtmod.c: gyp_intermediate_dir := $(abspath $(gyp_intermediate_dir))
$(gyp_shared_intermediate_dir)/third_party/yasm/x86regtmod.c: gyp_shared_intermediate_dir := $(abspath $(gyp_shared_intermediate_dir))
$(gyp_shared_intermediate_dir)/third_party/yasm/x86regtmod.c: export PATH := $(subst $(ANDROID_BUILD_PATHS),,$(PATH))
$(gyp_shared_intermediate_dir)/third_party/yasm/x86regtmod.c: $(LOCAL_PATH)/third_party/yasm/source/patched-yasm/modules/arch/x86/x86regtmod.gperf $(gyp_shared_intermediate_dir)/genperf $(GYP_TARGET_DEPENDENCIES)
	@mkdir -p $(gyp_shared_intermediate_dir)/third_party/yasm; cd $(gyp_local_path)/third_party/yasm; "$(gyp_shared_intermediate_dir)/genperf" source/patched-yasm/modules/arch/x86/x86regtmod.gperf "$(gyp_shared_intermediate_dir)/third_party/yasm/x86regtmod.c"



GYP_GENERATED_OUTPUTS := \
	$(gyp_shared_intermediate_dir)/third_party/yasm/x86insns.c \
	$(gyp_shared_intermediate_dir)/third_party/yasm/x86insn_gas.gperf \
	$(gyp_shared_intermediate_dir)/third_party/yasm/x86insn_nasm.gperf \
	$(gyp_shared_intermediate_dir)/third_party/yasm/version.mac \
	$(gyp_shared_intermediate_dir)/third_party/yasm/x86cpu.c \
	$(gyp_shared_intermediate_dir)/third_party/yasm/x86regtmod.c

# Make sure our deps and generated files are built first.
LOCAL_ADDITIONAL_DEPENDENCIES := $(GYP_TARGET_DEPENDENCIES) $(GYP_GENERATED_OUTPUTS)

LOCAL_GENERATED_SOURCES :=

GYP_COPIED_SOURCE_ORIGIN_DIRS :=

LOCAL_SRC_FILES :=


# Flags passed to both C and C++ files.
MY_CFLAGS_Debug := \
	-fstack-protector \
	--param=ssp-buffer-size=4 \
	-pthread \
	-fno-strict-aliasing \
	-Wno-unused-parameter \
	-Wno-missing-field-initializers \
	-fvisibility=hidden \
	-pipe \
	-fPIC \
	-Wno-format \
	-Wheader-hygiene \
	-Wno-char-subscripts \
	-Wno-unneeded-internal-declaration \
	-Wno-covered-switch-default \
	-Wstring-conversion \
	-Wno-c++11-narrowing \
	-Wno-deprecated-register \
	-Wno-unused-local-typedef \
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
	'-DDYNAMIC_ANNOTATIONS_ENABLED=1' \
	'-DWTF_USE_DYNAMIC_ANNOTATIONS=1' \
	'-D_DEBUG'


# Include paths placed before CFLAGS/CPPFLAGS
LOCAL_C_INCLUDES_Debug := \
	$(gyp_shared_intermediate_dir)


# Flags passed to only C++ (and not C) files.
LOCAL_CPPFLAGS_Debug := \
	-fno-exceptions \
	-fno-rtti \
	-fno-threadsafe-statics \
	-fvisibility-inlines-hidden \
	-Wno-deprecated \
	-std=gnu++11


# Flags passed to both C and C++ files.
MY_CFLAGS_Release := \
	-fstack-protector \
	--param=ssp-buffer-size=4 \
	-pthread \
	-fno-strict-aliasing \
	-Wno-unused-parameter \
	-Wno-missing-field-initializers \
	-fvisibility=hidden \
	-pipe \
	-fPIC \
	-Wno-format \
	-Wheader-hygiene \
	-Wno-char-subscripts \
	-Wno-unneeded-internal-declaration \
	-Wno-covered-switch-default \
	-Wstring-conversion \
	-Wno-c++11-narrowing \
	-Wno-deprecated-register \
	-Wno-unused-local-typedef \
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
	'-DNDEBUG' \
	'-DNVALGRIND' \
	'-DDYNAMIC_ANNOTATIONS_ENABLED=0'


# Include paths placed before CFLAGS/CPPFLAGS
LOCAL_C_INCLUDES_Release := \
	$(gyp_shared_intermediate_dir)


# Flags passed to only C++ (and not C) files.
LOCAL_CPPFLAGS_Release := \
	-fno-exceptions \
	-fno-rtti \
	-fno-threadsafe-statics \
	-fvisibility-inlines-hidden \
	-Wno-deprecated \
	-std=gnu++11


LOCAL_CFLAGS := $(MY_CFLAGS_$(GYP_CONFIGURATION)) $(MY_DEFS_$(GYP_CONFIGURATION))
# Undefine ANDROID for host modules
LOCAL_CFLAGS += -UANDROID
LOCAL_C_INCLUDES := $(GYP_COPIED_SOURCE_ORIGIN_DIRS) $(LOCAL_C_INCLUDES_$(GYP_CONFIGURATION))
LOCAL_CPPFLAGS := $(LOCAL_CPPFLAGS_$(GYP_CONFIGURATION))
LOCAL_ASFLAGS := $(LOCAL_CFLAGS)
### Rules for final target.
### Set directly by aosp_build_settings.
LOCAL_CLANG := true

# Add target alias to "gyp_all_modules" target.
.PHONY: gyp_all_modules
gyp_all_modules: third_party_yasm_generate_files_$(TARGET_$(GYP_VAR_PREFIX)ARCH)_host_gyp

# Alias gyp target name.
.PHONY: generate_files
generate_files: third_party_yasm_generate_files_$(TARGET_$(GYP_VAR_PREFIX)ARCH)_host_gyp

LOCAL_MODULE_PATH := $(PRODUCT_OUT)/gyp_stamp
LOCAL_UNINSTALLABLE_MODULE := true
LOCAL_2ND_ARCH_VAR_PREFIX := $(GYP_HOST_VAR_PREFIX)

include $(BUILD_SYSTEM)/base_rules.mk

$(LOCAL_BUILT_MODULE): $(LOCAL_ADDITIONAL_DEPENDENCIES)
	$(hide) echo "Gyp timestamp: $@"
	$(hide) mkdir -p $(dir $@)
	$(hide) touch $@

LOCAL_2ND_ARCH_VAR_PREFIX :=
