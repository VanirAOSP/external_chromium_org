// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/sync/glue/fake_data_type_controller.h"
#include "chrome/browser/sync/glue/model_association_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
namespace browser_sync {
class MockModelAssociationResultProcessor :
    public ModelAssociationResultProcessor {
 public:
  MockModelAssociationResultProcessor() {}
  ~MockModelAssociationResultProcessor() {}
  MOCK_METHOD2(OnSingleDataTypeAssociationDone,
               void(syncer::ModelType type,
                    const syncer::DataTypeAssociationStats& association_stats));
  MOCK_METHOD1(OnModelAssociationDone, void(
      const DataTypeManager::ConfigureResult& result));
};

FakeDataTypeController* GetController(
    const DataTypeController::TypeMap& controllers,
    syncer::ModelType model_type) {
  DataTypeController::TypeMap::const_iterator it =
      controllers.find(model_type);
  if (it == controllers.end()) {
    return NULL;
  }
  return (FakeDataTypeController*)(it->second.get());
}

ACTION_P(VerifyResult, expected_result) {
  EXPECT_EQ(arg0.status, expected_result.status);
  EXPECT_TRUE(arg0.requested_types.Equals(expected_result.requested_types));
  EXPECT_EQ(arg0.failed_data_types.size(),
            expected_result.failed_data_types.size());

  if (arg0.failed_data_types.size() ==
          expected_result.failed_data_types.size()) {
    std::map<syncer::ModelType, syncer::SyncError>::const_iterator it1, it2;
    for (it1 = arg0.failed_data_types.begin(),
         it2 = expected_result.failed_data_types.begin();
         it1 != arg0.failed_data_types.end();
         ++it1, ++it2) {
      EXPECT_EQ((*it1).first, (*it2).first);
    }
  }

  EXPECT_TRUE(arg0.unfinished_data_types.Equals(
      expected_result.unfinished_data_types));
}

class SyncModelAssociationManagerTest : public testing::Test {
 public:
  SyncModelAssociationManagerTest() :
      ui_thread_(content::BrowserThread::UI, &ui_loop_) {
  }

 protected:
  base::MessageLoopForUI ui_loop_;
  content::TestBrowserThread ui_thread_;
  MockModelAssociationResultProcessor result_processor_;
  DataTypeController::TypeMap controllers_;
};

// Start a type and make sure ModelAssociationManager callst the |Start|
// method and calls the callback when it is done.
TEST_F(SyncModelAssociationManagerTest, SimpleModelStart) {
  controllers_[syncer::BOOKMARKS] =
      new FakeDataTypeController(syncer::BOOKMARKS);
  controllers_[syncer::APPS] =
      new FakeDataTypeController(syncer::APPS);
  ModelAssociationManager model_association_manager(&controllers_,
                                                    &result_processor_);
  syncer::ModelTypeSet types(syncer::BOOKMARKS, syncer::APPS);
  DataTypeManager::ConfigureResult expected_result(
      DataTypeManager::OK,
      types,
      std::map<syncer::ModelType, syncer::SyncError>(),
      syncer::ModelTypeSet(),
      syncer::ModelTypeSet());
  EXPECT_CALL(result_processor_, OnModelAssociationDone(_)).
              WillOnce(VerifyResult(expected_result));

  EXPECT_EQ(GetController(controllers_, syncer::BOOKMARKS)->state(),
            DataTypeController::NOT_RUNNING);
  EXPECT_EQ(GetController(controllers_, syncer::APPS)->state(),
            DataTypeController::NOT_RUNNING);

  // Initialize() kicks off model loading.
  model_association_manager.Initialize(types);

  EXPECT_EQ(GetController(controllers_, syncer::BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  EXPECT_EQ(GetController(controllers_, syncer::APPS)->state(),
            DataTypeController::MODEL_LOADED);

  model_association_manager.StartAssociationAsync(types);

  EXPECT_EQ(GetController(controllers_, syncer::BOOKMARKS)->state(),
            DataTypeController::ASSOCIATING);
  EXPECT_EQ(GetController(controllers_, syncer::APPS)->state(),
            DataTypeController::ASSOCIATING);
  GetController(controllers_, syncer::BOOKMARKS)->FinishStart(
      DataTypeController::OK);
  GetController(controllers_, syncer::APPS)->FinishStart(
      DataTypeController::OK);
}

// Start a type and call stop before it finishes associating.
TEST_F(SyncModelAssociationManagerTest, StopModelBeforeFinish) {
  controllers_[syncer::BOOKMARKS] =
      new FakeDataTypeController(syncer::BOOKMARKS);
  ModelAssociationManager model_association_manager(
      &controllers_,
      &result_processor_);

  syncer::ModelTypeSet types;
  types.Put(syncer::BOOKMARKS);

  std::map<syncer::ModelType, syncer::SyncError> errors;
  syncer::SyncError error(FROM_HERE,
                          syncer::SyncError::DATATYPE_ERROR,
                          "Failed",
                          syncer::BOOKMARKS);
  errors[syncer::BOOKMARKS] = error;

  DataTypeManager::ConfigureResult expected_result(
      DataTypeManager::ABORTED,
      types,
      errors,
      syncer::ModelTypeSet(syncer::BOOKMARKS),
      syncer::ModelTypeSet());

  EXPECT_CALL(result_processor_, OnModelAssociationDone(_)).
              WillOnce(VerifyResult(expected_result));

  model_association_manager.Initialize(types);
  model_association_manager.StartAssociationAsync(types);

  EXPECT_EQ(GetController(controllers_, syncer::BOOKMARKS)->state(),
            DataTypeController::ASSOCIATING);
  model_association_manager.Stop();
  EXPECT_EQ(GetController(controllers_, syncer::BOOKMARKS)->state(),
            DataTypeController::NOT_RUNNING);
}

// Start a type, let it finish and then call stop.
TEST_F(SyncModelAssociationManagerTest, StopAfterFinish) {
  controllers_[syncer::BOOKMARKS] =
      new FakeDataTypeController(syncer::BOOKMARKS);
  ModelAssociationManager model_association_manager(
      &controllers_,
      &result_processor_);
  syncer::ModelTypeSet types;
  types.Put(syncer::BOOKMARKS);
  DataTypeManager::ConfigureResult expected_result(
      DataTypeManager::OK,
      types,
      std::map<syncer::ModelType, syncer::SyncError>(),
      syncer::ModelTypeSet(),
      syncer::ModelTypeSet());
  EXPECT_CALL(result_processor_, OnModelAssociationDone(_)).
              WillOnce(VerifyResult(expected_result));

  model_association_manager.Initialize(types);
  model_association_manager.StartAssociationAsync(types);

  EXPECT_EQ(GetController(controllers_, syncer::BOOKMARKS)->state(),
            DataTypeController::ASSOCIATING);
  GetController(controllers_, syncer::BOOKMARKS)->FinishStart(
      DataTypeController::OK);

  model_association_manager.Stop();
  EXPECT_EQ(GetController(controllers_, syncer::BOOKMARKS)->state(),
            DataTypeController::NOT_RUNNING);
}

// Make a type fail model association and verify correctness.
TEST_F(SyncModelAssociationManagerTest, TypeFailModelAssociation) {
  controllers_[syncer::BOOKMARKS] =
      new FakeDataTypeController(syncer::BOOKMARKS);
  ModelAssociationManager model_association_manager(
      &controllers_,
      &result_processor_);
  syncer::ModelTypeSet types;
  types.Put(syncer::BOOKMARKS);
  std::map<syncer::ModelType, syncer::SyncError> errors;
  syncer::SyncError error(FROM_HERE,
                          syncer::SyncError::DATATYPE_ERROR,
                          "Failed",
                          syncer::BOOKMARKS);
  errors[syncer::BOOKMARKS] = error;
  DataTypeManager::ConfigureResult expected_result(
      DataTypeManager::PARTIAL_SUCCESS,
      types,
      errors,
      syncer::ModelTypeSet(),
      syncer::ModelTypeSet());
  EXPECT_CALL(result_processor_, OnModelAssociationDone(_)).
              WillOnce(VerifyResult(expected_result));

  model_association_manager.Initialize(types);
  model_association_manager.StartAssociationAsync(types);

  EXPECT_EQ(GetController(controllers_, syncer::BOOKMARKS)->state(),
            DataTypeController::ASSOCIATING);
  GetController(controllers_, syncer::BOOKMARKS)->FinishStart(
      DataTypeController::ASSOCIATION_FAILED);
  EXPECT_EQ(GetController(controllers_, syncer::BOOKMARKS)->state(),
            DataTypeController::NOT_RUNNING);
}

// Ensure configuring stops when a type returns a unrecoverable error.
TEST_F(SyncModelAssociationManagerTest, TypeReturnUnrecoverableError) {
  controllers_[syncer::BOOKMARKS] =
      new FakeDataTypeController(syncer::BOOKMARKS);
  ModelAssociationManager model_association_manager(
      &controllers_,
      &result_processor_);
  syncer::ModelTypeSet types;
  types.Put(syncer::BOOKMARKS);
  std::map<syncer::ModelType, syncer::SyncError> errors;
  syncer::SyncError error(FROM_HERE,
                          syncer::SyncError::DATATYPE_ERROR,
                          "Failed",
                          syncer::BOOKMARKS);
  errors[syncer::BOOKMARKS] = error;
  DataTypeManager::ConfigureResult expected_result(
      DataTypeManager::UNRECOVERABLE_ERROR,
      types,
      errors,
      syncer::ModelTypeSet(),
      syncer::ModelTypeSet());
  EXPECT_CALL(result_processor_, OnModelAssociationDone(_)).
              WillOnce(VerifyResult(expected_result));

  model_association_manager.Initialize(types);

  model_association_manager.StartAssociationAsync(types);

  EXPECT_EQ(GetController(controllers_, syncer::BOOKMARKS)->state(),
            DataTypeController::ASSOCIATING);
  GetController(controllers_, syncer::BOOKMARKS)->FinishStart(
      DataTypeController::UNRECOVERABLE_ERROR);
}

TEST_F(SyncModelAssociationManagerTest, SlowTypeAsFailedType) {
  controllers_[syncer::BOOKMARKS] =
      new FakeDataTypeController(syncer::BOOKMARKS);
  controllers_[syncer::APPS] =
      new FakeDataTypeController(syncer::APPS);
  GetController(controllers_, syncer::BOOKMARKS)->SetDelayModelLoad();
  ModelAssociationManager model_association_manager(&controllers_,
                                                    &result_processor_);
  syncer::ModelTypeSet types;
  types.Put(syncer::BOOKMARKS);
  types.Put(syncer::APPS);

  std::map<syncer::ModelType, syncer::SyncError> errors;
  syncer::SyncError error(FROM_HERE,
                          syncer::SyncError::DATATYPE_ERROR,
                          "Association timed out.",
                          syncer::BOOKMARKS);
  errors[syncer::BOOKMARKS] = error;

  syncer::ModelTypeSet expected_types_unfinished;
  expected_types_unfinished.Put(syncer::BOOKMARKS);
  DataTypeManager::ConfigureResult expected_result_partially_done(
      DataTypeManager::PARTIAL_SUCCESS,
      types,
      errors,
      expected_types_unfinished,
      syncer::ModelTypeSet());

  EXPECT_CALL(result_processor_, OnModelAssociationDone(_)).
              WillOnce(VerifyResult(expected_result_partially_done));

  model_association_manager.Initialize(types);
  model_association_manager.StartAssociationAsync(types);
  GetController(controllers_, syncer::APPS)->FinishStart(
      DataTypeController::OK);

  model_association_manager.GetTimerForTesting()->user_task().Run();

  EXPECT_EQ(DataTypeController::NOT_RUNNING,
            GetController(controllers_, syncer::BOOKMARKS)->state());
}

TEST_F(SyncModelAssociationManagerTest, StartMultipleTimes) {
  controllers_[syncer::BOOKMARKS] =
      new FakeDataTypeController(syncer::BOOKMARKS);
  controllers_[syncer::APPS] =
      new FakeDataTypeController(syncer::APPS);
  ModelAssociationManager model_association_manager(&controllers_,
                                                    &result_processor_);
  syncer::ModelTypeSet types;
  types.Put(syncer::BOOKMARKS);
  types.Put(syncer::APPS);

  DataTypeManager::ConfigureResult result_1st(
      DataTypeManager::OK,
      syncer::ModelTypeSet(syncer::BOOKMARKS),
      std::map<syncer::ModelType, syncer::SyncError>(),
      syncer::ModelTypeSet(),
      syncer::ModelTypeSet());
  DataTypeManager::ConfigureResult result_2nd(
      DataTypeManager::OK,
      syncer::ModelTypeSet(syncer::APPS),
      std::map<syncer::ModelType, syncer::SyncError>(),
      syncer::ModelTypeSet(),
      syncer::ModelTypeSet());
  EXPECT_CALL(result_processor_, OnModelAssociationDone(_)).
      Times(2).
      WillOnce(VerifyResult(result_1st)).
      WillOnce(VerifyResult(result_2nd));

  model_association_manager.Initialize(types);

  // Start BOOKMARKS first.
  model_association_manager.StartAssociationAsync(
      syncer::ModelTypeSet(syncer::BOOKMARKS));
  EXPECT_EQ(GetController(controllers_, syncer::BOOKMARKS)->state(),
            DataTypeController::ASSOCIATING);
  EXPECT_EQ(GetController(controllers_, syncer::APPS)->state(),
            DataTypeController::MODEL_LOADED);

  // Finish BOOKMARKS association.
  GetController(controllers_, syncer::BOOKMARKS)->FinishStart(
      DataTypeController::OK);
  EXPECT_EQ(GetController(controllers_, syncer::BOOKMARKS)->state(),
            DataTypeController::RUNNING);
  EXPECT_EQ(GetController(controllers_, syncer::APPS)->state(),
            DataTypeController::MODEL_LOADED);

  // Start APPS next.
  model_association_manager.StartAssociationAsync(
      syncer::ModelTypeSet(syncer::APPS));
  EXPECT_EQ(GetController(controllers_, syncer::APPS)->state(),
            DataTypeController::ASSOCIATING);
  GetController(controllers_, syncer::APPS)->FinishStart(
      DataTypeController::OK);
  EXPECT_EQ(GetController(controllers_, syncer::APPS)->state(),
            DataTypeController::RUNNING);
}

// Test that model that failed to load between initialization and association
// is reported and stopped properly.
TEST_F(SyncModelAssociationManagerTest, ModelLoadFailBeforeAssociationStart) {
  controllers_[syncer::BOOKMARKS] =
      new FakeDataTypeController(syncer::BOOKMARKS);
  GetController(controllers_, syncer::BOOKMARKS)->SetModelLoadError(
      syncer::SyncError(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                        "", syncer::BOOKMARKS));
  ModelAssociationManager model_association_manager(
      &controllers_,
      &result_processor_);
  syncer::ModelTypeSet types;
  types.Put(syncer::BOOKMARKS);
  std::map<syncer::ModelType, syncer::SyncError> errors;
  syncer::SyncError error(FROM_HERE,
                          syncer::SyncError::DATATYPE_ERROR,
                          "Failed",
                          syncer::BOOKMARKS);
  errors[syncer::BOOKMARKS] = error;
  DataTypeManager::ConfigureResult expected_result(
      DataTypeManager::PARTIAL_SUCCESS,
      types,
      errors,
      syncer::ModelTypeSet(),
      syncer::ModelTypeSet());
  EXPECT_CALL(result_processor_, OnModelAssociationDone(_)).
              WillOnce(VerifyResult(expected_result));

  model_association_manager.Initialize(types);
  EXPECT_EQ(DataTypeController::DISABLED,
            GetController(controllers_, syncer::BOOKMARKS)->state());
  model_association_manager.StartAssociationAsync(types);
  EXPECT_EQ(DataTypeController::NOT_RUNNING,
            GetController(controllers_, syncer::BOOKMARKS)->state());
}

}  // namespace browser_sync
