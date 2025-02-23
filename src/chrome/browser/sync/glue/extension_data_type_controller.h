// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_EXTENSION_DATA_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_GLUE_EXTENSION_DATA_TYPE_CONTROLLER_H_
#pragma once

#include <string>

#include "chrome/browser/sync/glue/generic_change_processor.h"
#include "chrome/browser/sync/glue/frontend_data_type_controller.h"

namespace browser_sync {

class ExtensionDataTypeController : public FrontendDataTypeController {
 public:
  ExtensionDataTypeController(
      ProfileSyncFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service);
  virtual ~ExtensionDataTypeController();

  // DataTypeController implementation.
  virtual syncable::ModelType type() const;

 protected:
  virtual GenericChangeProcessor* change_processor() const OVERRIDE;

 private:
  // DataTypeController implementations.
  virtual bool StartModels();
  virtual void CreateSyncComponents();
  virtual void RecordUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message);
  virtual void RecordAssociationTime(base::TimeDelta time);
  virtual void RecordStartFailure(StartResult result);

  scoped_ptr<GenericChangeProcessor> generic_change_processor_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSION_DATA_TYPE_CONTROLLER_H_
