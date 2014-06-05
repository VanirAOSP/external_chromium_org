// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_log.h"

#include <string>

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/port.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "base/tracked_objects.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/metrics/proto/profiler_event.pb.h"
#include "chrome/common/metrics/proto/system_profile.pb.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/variations/metrics_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/process_type.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/fake_user_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chromeos/dbus/fake_bluetooth_adapter_client.h"
#include "chromeos/dbus/fake_bluetooth_device_client.h"
#include "chromeos/dbus/fake_bluetooth_input_client.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#endif  // OS_CHROMEOS

using base::TimeDelta;
using metrics::ProfilerEventProto;
using tracked_objects::ProcessDataSnapshot;
using tracked_objects::TaskSnapshot;

namespace {

const char kClientId[] = "bogus client ID";
const int64 kInstallDate = 1373051956;
const int64 kInstallDateExpected = 1373050800;  // Computed from kInstallDate.
const int64 kEnabledDate = 1373001211;
const int64 kEnabledDateExpected = 1373000400;  // Computed from kEnabledDate.
const int kSessionId = 127;
const int kScreenWidth = 1024;
const int kScreenHeight = 768;
const int kScreenCount = 3;
const float kScreenScaleFactor = 2;
const char kBrandForTesting[] = "brand_for_testing";
const chrome_variations::ActiveGroupId kFieldTrialIds[] = {
  {37, 43},
  {13, 47},
  {23, 17}
};
const chrome_variations::ActiveGroupId kSyntheticTrials[] = {
  {55, 15},
  {66, 16}
};

#if defined(ENABLE_PLUGINS)
content::WebPluginInfo CreateFakePluginInfo(
    const std::string& name,
    const base::FilePath::CharType* path,
    const std::string& version,
    bool is_pepper) {
  content::WebPluginInfo plugin(UTF8ToUTF16(name),
                                base::FilePath(path),
                                UTF8ToUTF16(version),
                                base::string16());
  if (is_pepper)
    plugin.type = content::WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS;
  else
    plugin.type = content::WebPluginInfo::PLUGIN_TYPE_NPAPI;
  return plugin;
}
#endif  // defined(ENABLE_PLUGINS)

class TestMetricsLog : public MetricsLog {
 public:
  TestMetricsLog(const std::string& client_id, int session_id)
      : MetricsLog(client_id, session_id),
        prefs_(&scoped_prefs_),
        brand_for_testing_(kBrandForTesting) {
    chrome::RegisterLocalState(scoped_prefs_.registry());
    InitPrefs();
  }
  // Creates a TestMetricsLog that will use |prefs| as the fake local state.
  // Useful for tests that need to re-use the local state prefs between logs.
  TestMetricsLog(const std::string& client_id,
                 int session_id,
                 TestingPrefServiceSimple* prefs)
      : MetricsLog(client_id, session_id),
        prefs_(prefs),
        brand_for_testing_(kBrandForTesting) {
    InitPrefs();
  }
  virtual ~TestMetricsLog() {}

  virtual PrefService* GetPrefService() OVERRIDE {
    return prefs_;
  }

  const metrics::ChromeUserMetricsExtension& uma_proto() const {
    return *MetricsLog::uma_proto();
  }

  const metrics::SystemProfileProto& system_profile() const {
    return uma_proto().system_profile();
  }

 private:
  void InitPrefs() {
    prefs_->SetInt64(prefs::kInstallDate, kInstallDate);
    prefs_->SetString(prefs::kMetricsClientIDTimestamp,
                     base::Int64ToString(kEnabledDate));
#if defined(OS_CHROMEOS)
    prefs_->SetInteger(prefs::kStabilityChildProcessCrashCount, 10);
    prefs_->SetInteger(prefs::kStabilityOtherUserCrashCount, 11);
    prefs_->SetInteger(prefs::kStabilityKernelCrashCount, 12);
    prefs_->SetInteger(prefs::kStabilitySystemUncleanShutdownCount, 13);
#endif  // OS_CHROMEOS
  }

  virtual void GetFieldTrialIds(
      std::vector<chrome_variations::ActiveGroupId>* field_trial_ids) const
      OVERRIDE {
    ASSERT_TRUE(field_trial_ids->empty());

    for (size_t i = 0; i < arraysize(kFieldTrialIds); ++i) {
      field_trial_ids->push_back(kFieldTrialIds[i]);
    }
  }

  virtual gfx::Size GetScreenSize() const OVERRIDE {
    return gfx::Size(kScreenWidth, kScreenHeight);
  }

  virtual float GetScreenDeviceScaleFactor() const OVERRIDE {
    return kScreenScaleFactor;
  }

  virtual int GetScreenCount() const OVERRIDE {
    return kScreenCount;
  }

  // Scoped PrefsService, which may not be used if |prefs_ != &scoped_prefs|.
  TestingPrefServiceSimple scoped_prefs_;
  // Weak pointer to the PrefsService used by this log.
  TestingPrefServiceSimple* prefs_;

  google_util::BrandForTesting brand_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(TestMetricsLog);
};

}  // namespace

class MetricsLogTest : public testing::Test {
 public:
  MetricsLogTest() : message_loop_(base::MessageLoop::TYPE_IO) {}

 protected:
  virtual void SetUp() OVERRIDE {
#if defined(OS_CHROMEOS)
    chromeos::FakeDBusThreadManager* fake_dbus_thread_manager =
        new chromeos::FakeDBusThreadManager;
    fake_dbus_thread_manager->SetBluetoothAdapterClient(
        scoped_ptr<chromeos::BluetoothAdapterClient>(
            new chromeos::FakeBluetoothAdapterClient));
    fake_dbus_thread_manager->SetBluetoothDeviceClient(
        scoped_ptr<chromeos::BluetoothDeviceClient>(
            new chromeos::FakeBluetoothDeviceClient));
    fake_dbus_thread_manager->SetBluetoothInputClient(
        scoped_ptr<chromeos::BluetoothInputClient>(
            new chromeos::FakeBluetoothInputClient));
    chromeos::DBusThreadManager::InitializeForTesting(fake_dbus_thread_manager);

    // Enable multi-profiles.
    CommandLine::ForCurrentProcess()->AppendSwitch(switches::kMultiProfiles);
#endif  // OS_CHROMEOS
  }

  virtual void TearDown() OVERRIDE {
    // Drain the blocking pool from PostTaskAndReply executed by
    // MetrticsLog.network_observer_.
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
    content::RunAllPendingInMessageLoop();

#if defined(OS_CHROMEOS)
    chromeos::DBusThreadManager::Shutdown();
#endif  // OS_CHROMEOS
  }

  // Check that the values in |system_values| correspond to the test data
  // defined at the top of this file.
  void CheckSystemProfile(const metrics::SystemProfileProto& system_profile) {
    EXPECT_EQ(kInstallDateExpected, system_profile.install_date());
    EXPECT_EQ(kEnabledDateExpected, system_profile.uma_enabled_date());

    ASSERT_EQ(arraysize(kFieldTrialIds) + arraysize(kSyntheticTrials),
              static_cast<size_t>(system_profile.field_trial_size()));
    for (size_t i = 0; i < arraysize(kFieldTrialIds); ++i) {
      const metrics::SystemProfileProto::FieldTrial& field_trial =
          system_profile.field_trial(i);
      EXPECT_EQ(kFieldTrialIds[i].name, field_trial.name_id());
      EXPECT_EQ(kFieldTrialIds[i].group, field_trial.group_id());
    }
    // Verify the right data is present for the synthetic trials.
    for (size_t i = 0; i < arraysize(kSyntheticTrials); ++i) {
      const metrics::SystemProfileProto::FieldTrial& field_trial =
          system_profile.field_trial(i + arraysize(kFieldTrialIds));
      EXPECT_EQ(kSyntheticTrials[i].name, field_trial.name_id());
      EXPECT_EQ(kSyntheticTrials[i].group, field_trial.group_id());
    }

    EXPECT_EQ(kBrandForTesting, system_profile.brand_code());

    const metrics::SystemProfileProto::Hardware& hardware =
        system_profile.hardware();
    EXPECT_EQ(kScreenWidth, hardware.primary_screen_width());
    EXPECT_EQ(kScreenHeight, hardware.primary_screen_height());
    EXPECT_EQ(kScreenScaleFactor, hardware.primary_screen_scale_factor());
    EXPECT_EQ(kScreenCount, hardware.screen_count());

    EXPECT_TRUE(hardware.has_cpu());
    EXPECT_TRUE(hardware.cpu().has_vendor_name());
    EXPECT_TRUE(hardware.cpu().has_signature());

    // TODO(isherman): Verify other data written into the protobuf as a result
    // of this call.
  }

 private:
  // This is necessary because eventually some tests call base::RepeatingTimer
  // functions and a message loop is required for that.
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(MetricsLogTest);
};

TEST_F(MetricsLogTest, RecordEnvironment) {
  TestMetricsLog log(kClientId, kSessionId);

  std::vector<content::WebPluginInfo> plugins;
  GoogleUpdateMetrics google_update_metrics;
  std::vector<chrome_variations::ActiveGroupId> synthetic_trials;
  // Add two synthetic trials.
  synthetic_trials.push_back(kSyntheticTrials[0]);
  synthetic_trials.push_back(kSyntheticTrials[1]);

  log.RecordEnvironment(plugins, google_update_metrics, synthetic_trials);
  // Check that the system profile on the log has the correct values set.
  CheckSystemProfile(log.system_profile());

  // Check that the system profile has also been written to prefs.
  PrefService* local_state = log.GetPrefService();
  const std::string base64_system_profile =
      local_state->GetString(prefs::kStabilitySavedSystemProfile);
  EXPECT_FALSE(base64_system_profile.empty());
  std::string serialied_system_profile;
  EXPECT_TRUE(base::Base64Decode(base64_system_profile,
                                 &serialied_system_profile));
  SystemProfileProto decoded_system_profile;
  EXPECT_TRUE(decoded_system_profile.ParseFromString(serialied_system_profile));
  CheckSystemProfile(decoded_system_profile);
}

TEST_F(MetricsLogTest, LoadSavedEnvironmentFromPrefs) {
  const char* kSystemProfilePref = prefs::kStabilitySavedSystemProfile;
  const char* kSystemProfileHashPref = prefs::kStabilitySavedSystemProfileHash;

  TestingPrefServiceSimple prefs;
  chrome::RegisterLocalState(prefs.registry());

  // The pref value is empty, so loading it from prefs should fail.
  {
    TestMetricsLog log(kClientId, kSessionId, &prefs);
    EXPECT_FALSE(log.LoadSavedEnvironmentFromPrefs());
  }

  // Do a RecordEnvironment() call and check whether the pref is recorded.
  {
    TestMetricsLog log(kClientId, kSessionId, &prefs);
    log.RecordEnvironment(std::vector<content::WebPluginInfo>(),
                          GoogleUpdateMetrics(),
                          std::vector<chrome_variations::ActiveGroupId>());
    EXPECT_FALSE(prefs.GetString(kSystemProfilePref).empty());
    EXPECT_FALSE(prefs.GetString(kSystemProfileHashPref).empty());
  }

  {
    TestMetricsLog log(kClientId, kSessionId, &prefs);
    EXPECT_TRUE(log.LoadSavedEnvironmentFromPrefs());
    // Check some values in the system profile.
    EXPECT_EQ(kInstallDateExpected, log.system_profile().install_date());
    EXPECT_EQ(kEnabledDateExpected, log.system_profile().uma_enabled_date());
    // Ensure that the call cleared the prefs.
    EXPECT_TRUE(prefs.GetString(kSystemProfilePref).empty());
    EXPECT_TRUE(prefs.GetString(kSystemProfileHashPref).empty());
  }

  // Ensure that a non-matching hash results in the pref being invalid.
  {
    TestMetricsLog log(kClientId, kSessionId, &prefs);
    // Call RecordEnvironment() to record the pref again.
    log.RecordEnvironment(std::vector<content::WebPluginInfo>(),
                          GoogleUpdateMetrics(),
                          std::vector<chrome_variations::ActiveGroupId>());
  }

  {
    // Set the hash to a bad value.
    prefs.SetString(kSystemProfileHashPref, "deadbeef");
    TestMetricsLog log(kClientId, kSessionId, &prefs);
    EXPECT_FALSE(log.LoadSavedEnvironmentFromPrefs());
    // Ensure that the prefs are cleared, even if the call failed.
    EXPECT_TRUE(prefs.GetString(kSystemProfilePref).empty());
    EXPECT_TRUE(prefs.GetString(kSystemProfileHashPref).empty());
  }
}

TEST_F(MetricsLogTest, InitialLogStabilityMetrics) {
  TestMetricsLog log(kClientId, kSessionId);
  log.RecordEnvironment(std::vector<content::WebPluginInfo>(),
                        GoogleUpdateMetrics(),
                        std::vector<chrome_variations::ActiveGroupId>());
  log.RecordStabilityMetrics(base::TimeDelta(), MetricsLog::INITIAL_LOG);
  const metrics::SystemProfileProto_Stability& stability =
      log.system_profile().stability();
  // Required metrics:
  EXPECT_TRUE(stability.has_launch_count());
  EXPECT_TRUE(stability.has_crash_count());
  // Initial log metrics:
  EXPECT_TRUE(stability.has_incomplete_shutdown_count());
  EXPECT_TRUE(stability.has_breakpad_registration_success_count());
  EXPECT_TRUE(stability.has_breakpad_registration_failure_count());
  EXPECT_TRUE(stability.has_debugger_present_count());
  EXPECT_TRUE(stability.has_debugger_not_present_count());
}

TEST_F(MetricsLogTest, OngoingLogStabilityMetrics) {
  TestMetricsLog log(kClientId, kSessionId);
  log.RecordEnvironment(std::vector<content::WebPluginInfo>(),
                        GoogleUpdateMetrics(),
                        std::vector<chrome_variations::ActiveGroupId>());
  log.RecordStabilityMetrics(base::TimeDelta(), MetricsLog::ONGOING_LOG);
  const metrics::SystemProfileProto_Stability& stability =
      log.system_profile().stability();
  // Required metrics:
  EXPECT_TRUE(stability.has_launch_count());
  EXPECT_TRUE(stability.has_crash_count());
  // Initial log metrics:
  EXPECT_FALSE(stability.has_incomplete_shutdown_count());
  EXPECT_FALSE(stability.has_breakpad_registration_success_count());
  EXPECT_FALSE(stability.has_breakpad_registration_failure_count());
  EXPECT_FALSE(stability.has_debugger_present_count());
  EXPECT_FALSE(stability.has_debugger_not_present_count());
}

#if defined(ENABLE_PLUGINS)
TEST_F(MetricsLogTest, Plugins) {
  TestMetricsLog log(kClientId, kSessionId);

  std::vector<content::WebPluginInfo> plugins;
  plugins.push_back(CreateFakePluginInfo("p1", FILE_PATH_LITERAL("p1.plugin"),
                                         "1.5", true));
  plugins.push_back(CreateFakePluginInfo("p2", FILE_PATH_LITERAL("p2.plugin"),
                                         "2.0", false));
  log.RecordEnvironment(plugins, GoogleUpdateMetrics(),
                        std::vector<chrome_variations::ActiveGroupId>());

  const metrics::SystemProfileProto& system_profile = log.system_profile();
  ASSERT_EQ(2, system_profile.plugin_size());
  EXPECT_EQ("p1", system_profile.plugin(0).name());
  EXPECT_EQ("p1.plugin", system_profile.plugin(0).filename());
  EXPECT_EQ("1.5", system_profile.plugin(0).version());
  EXPECT_TRUE(system_profile.plugin(0).is_pepper());
  EXPECT_EQ("p2", system_profile.plugin(1).name());
  EXPECT_EQ("p2.plugin", system_profile.plugin(1).filename());
  EXPECT_EQ("2.0", system_profile.plugin(1).version());
  EXPECT_FALSE(system_profile.plugin(1).is_pepper());

  // Now set some plugin stability stats for p2 and verify they're recorded.
  scoped_ptr<base::DictionaryValue> plugin_dict(new DictionaryValue);
  plugin_dict->SetString(prefs::kStabilityPluginName, "p2");
  plugin_dict->SetInteger(prefs::kStabilityPluginLaunches, 1);
  plugin_dict->SetInteger(prefs::kStabilityPluginCrashes, 2);
  plugin_dict->SetInteger(prefs::kStabilityPluginInstances, 3);
  plugin_dict->SetInteger(prefs::kStabilityPluginLoadingErrors, 4);
  {
    ListPrefUpdate update(log.GetPrefService(), prefs::kStabilityPluginStats);
    update.Get()->Append(plugin_dict.release());
  }

  log.RecordStabilityMetrics(base::TimeDelta(), MetricsLog::ONGOING_LOG);
  const metrics::SystemProfileProto_Stability& stability =
      log.system_profile().stability();
  ASSERT_EQ(1, stability.plugin_stability_size());
  EXPECT_EQ("p2", stability.plugin_stability(0).plugin().name());
  EXPECT_EQ("p2.plugin", stability.plugin_stability(0).plugin().filename());
  EXPECT_EQ("2.0", stability.plugin_stability(0).plugin().version());
  EXPECT_FALSE(stability.plugin_stability(0).plugin().is_pepper());
  EXPECT_EQ(1, stability.plugin_stability(0).launch_count());
  EXPECT_EQ(2, stability.plugin_stability(0).crash_count());
  EXPECT_EQ(3, stability.plugin_stability(0).instance_count());
  EXPECT_EQ(4, stability.plugin_stability(0).loading_error_count());
}
#endif  // defined(ENABLE_PLUGINS)

// Test that we properly write profiler data to the log.
TEST_F(MetricsLogTest, RecordProfilerData) {
  TestMetricsLog log(kClientId, kSessionId);
  EXPECT_EQ(0, log.uma_proto().profiler_event_size());

  {
    ProcessDataSnapshot process_data;
    process_data.process_id = 177;
    process_data.tasks.push_back(TaskSnapshot());
    process_data.tasks.back().birth.location.file_name = "file";
    process_data.tasks.back().birth.location.function_name = "function";
    process_data.tasks.back().birth.location.line_number = 1337;
    process_data.tasks.back().birth.thread_name = "birth_thread";
    process_data.tasks.back().death_data.count = 37;
    process_data.tasks.back().death_data.run_duration_sum = 31;
    process_data.tasks.back().death_data.run_duration_max = 17;
    process_data.tasks.back().death_data.run_duration_sample = 13;
    process_data.tasks.back().death_data.queue_duration_sum = 8;
    process_data.tasks.back().death_data.queue_duration_max = 5;
    process_data.tasks.back().death_data.queue_duration_sample = 3;
    process_data.tasks.back().death_thread_name = "Still_Alive";
    process_data.tasks.push_back(TaskSnapshot());
    process_data.tasks.back().birth.location.file_name = "file2";
    process_data.tasks.back().birth.location.function_name = "function2";
    process_data.tasks.back().birth.location.line_number = 1773;
    process_data.tasks.back().birth.thread_name = "birth_thread2";
    process_data.tasks.back().death_data.count = 19;
    process_data.tasks.back().death_data.run_duration_sum = 23;
    process_data.tasks.back().death_data.run_duration_max = 11;
    process_data.tasks.back().death_data.run_duration_sample = 7;
    process_data.tasks.back().death_data.queue_duration_sum = 0;
    process_data.tasks.back().death_data.queue_duration_max = 0;
    process_data.tasks.back().death_data.queue_duration_sample = 0;
    process_data.tasks.back().death_thread_name = "death_thread";

    log.RecordProfilerData(process_data, content::PROCESS_TYPE_BROWSER);
    ASSERT_EQ(1, log.uma_proto().profiler_event_size());
    EXPECT_EQ(ProfilerEventProto::STARTUP_PROFILE,
              log.uma_proto().profiler_event(0).profile_type());
    EXPECT_EQ(ProfilerEventProto::WALL_CLOCK_TIME,
              log.uma_proto().profiler_event(0).time_source());

    ASSERT_EQ(2, log.uma_proto().profiler_event(0).tracked_object_size());

    const ProfilerEventProto::TrackedObject* tracked_object =
        &log.uma_proto().profiler_event(0).tracked_object(0);
    EXPECT_EQ(GG_UINT64_C(10123486280357988687),
              tracked_object->source_file_name_hash());
    EXPECT_EQ(GG_UINT64_C(13962325592283560029),
              tracked_object->source_function_name_hash());
    EXPECT_EQ(1337, tracked_object->source_line_number());
    EXPECT_EQ(GG_UINT64_C(3400908935414830400),
              tracked_object->birth_thread_name_hash());
    EXPECT_EQ(37, tracked_object->exec_count());
    EXPECT_EQ(31, tracked_object->exec_time_total());
    EXPECT_EQ(13, tracked_object->exec_time_sampled());
    EXPECT_EQ(8, tracked_object->queue_time_total());
    EXPECT_EQ(3, tracked_object->queue_time_sampled());
    EXPECT_EQ(GG_UINT64_C(10151977472163283085),
              tracked_object->exec_thread_name_hash());
    EXPECT_EQ(177U, tracked_object->process_id());
    EXPECT_EQ(ProfilerEventProto::TrackedObject::BROWSER,
              tracked_object->process_type());

    tracked_object = &log.uma_proto().profiler_event(0).tracked_object(1);
    EXPECT_EQ(GG_UINT64_C(2025659946535236365),
              tracked_object->source_file_name_hash());
    EXPECT_EQ(GG_UINT64_C(55232426147951219),
              tracked_object->source_function_name_hash());
    EXPECT_EQ(1773, tracked_object->source_line_number());
    EXPECT_EQ(GG_UINT64_C(15727396632046120663),
              tracked_object->birth_thread_name_hash());
    EXPECT_EQ(19, tracked_object->exec_count());
    EXPECT_EQ(23, tracked_object->exec_time_total());
    EXPECT_EQ(7, tracked_object->exec_time_sampled());
    EXPECT_EQ(0, tracked_object->queue_time_total());
    EXPECT_EQ(0, tracked_object->queue_time_sampled());
    EXPECT_EQ(GG_UINT64_C(14275151213201158253),
              tracked_object->exec_thread_name_hash());
    EXPECT_EQ(177U, tracked_object->process_id());
    EXPECT_EQ(ProfilerEventProto::TrackedObject::BROWSER,
              tracked_object->process_type());
  }

  {
    ProcessDataSnapshot process_data;
    process_data.process_id = 1177;
    process_data.tasks.push_back(TaskSnapshot());
    process_data.tasks.back().birth.location.file_name = "file3";
    process_data.tasks.back().birth.location.function_name = "function3";
    process_data.tasks.back().birth.location.line_number = 7331;
    process_data.tasks.back().birth.thread_name = "birth_thread3";
    process_data.tasks.back().death_data.count = 137;
    process_data.tasks.back().death_data.run_duration_sum = 131;
    process_data.tasks.back().death_data.run_duration_max = 117;
    process_data.tasks.back().death_data.run_duration_sample = 113;
    process_data.tasks.back().death_data.queue_duration_sum = 108;
    process_data.tasks.back().death_data.queue_duration_max = 105;
    process_data.tasks.back().death_data.queue_duration_sample = 103;
    process_data.tasks.back().death_thread_name = "death_thread3";

    log.RecordProfilerData(process_data, content::PROCESS_TYPE_RENDERER);
    ASSERT_EQ(1, log.uma_proto().profiler_event_size());
    EXPECT_EQ(ProfilerEventProto::STARTUP_PROFILE,
              log.uma_proto().profiler_event(0).profile_type());
    EXPECT_EQ(ProfilerEventProto::WALL_CLOCK_TIME,
              log.uma_proto().profiler_event(0).time_source());
    ASSERT_EQ(3, log.uma_proto().profiler_event(0).tracked_object_size());

    const ProfilerEventProto::TrackedObject* tracked_object =
        &log.uma_proto().profiler_event(0).tracked_object(2);
    EXPECT_EQ(GG_UINT64_C(2686523203278102732),
              tracked_object->source_file_name_hash());
    EXPECT_EQ(GG_UINT64_C(5081672290546182009),
              tracked_object->source_function_name_hash());
    EXPECT_EQ(7331, tracked_object->source_line_number());
    EXPECT_EQ(GG_UINT64_C(8768512930949373716),
              tracked_object->birth_thread_name_hash());
    EXPECT_EQ(137, tracked_object->exec_count());
    EXPECT_EQ(131, tracked_object->exec_time_total());
    EXPECT_EQ(113, tracked_object->exec_time_sampled());
    EXPECT_EQ(108, tracked_object->queue_time_total());
    EXPECT_EQ(103, tracked_object->queue_time_sampled());
    EXPECT_EQ(GG_UINT64_C(7246674144371406371),
              tracked_object->exec_thread_name_hash());
    EXPECT_EQ(1177U, tracked_object->process_id());
    EXPECT_EQ(ProfilerEventProto::TrackedObject::RENDERER,
              tracked_object->process_type());
  }
}

#if defined(OS_CHROMEOS)
TEST_F(MetricsLogTest, MultiProfileUserCount) {
  std::string user1("user1@example.com");
  std::string user2("user2@example.com");
  std::string user3("user3@example.com");

  // |scoped_enabler| takes over the lifetime of |user_manager|.
  chromeos::FakeUserManager* user_manager = new chromeos::FakeUserManager();
  chromeos::ScopedUserManagerEnabler scoped_enabler(user_manager);
  user_manager->AddKioskAppUser(user1);
  user_manager->AddKioskAppUser(user2);
  user_manager->AddKioskAppUser(user3);

  user_manager->LoginUser(user1);
  user_manager->LoginUser(user3);

  TestMetricsLog log(kClientId, kSessionId);
  std::vector<content::WebPluginInfo> plugins;
  GoogleUpdateMetrics google_update_metrics;
  std::vector<chrome_variations::ActiveGroupId> synthetic_trials;
  log.RecordEnvironment(plugins, google_update_metrics, synthetic_trials);
  EXPECT_EQ(2u, log.system_profile().multi_profile_user_count());
}

TEST_F(MetricsLogTest, MultiProfileCountInvalidated) {
  std::string user1("user1@example.com");
  std::string user2("user2@example.com");
  std::string user3("user3@example.com");

  // |scoped_enabler| takes over the lifetime of |user_manager|.
  chromeos::FakeUserManager* user_manager = new chromeos::FakeUserManager();
  chromeos::ScopedUserManagerEnabler scoped_enabler(user_manager);
  user_manager->AddKioskAppUser(user1);
  user_manager->AddKioskAppUser(user2);
  user_manager->AddKioskAppUser(user3);

  user_manager->LoginUser(user1);

  TestMetricsLog log(kClientId, kSessionId);
  EXPECT_EQ(1u, log.system_profile().multi_profile_user_count());

  user_manager->LoginUser(user2);
  std::vector<chrome_variations::ActiveGroupId> synthetic_trials;
  log.RecordEnvironment(std::vector<content::WebPluginInfo>(),
                        GoogleUpdateMetrics(), synthetic_trials);
  EXPECT_EQ(0u, log.system_profile().multi_profile_user_count());
}
#endif  // OS_CHROMEOS
