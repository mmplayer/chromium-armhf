// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/form_data_predictions.h"

namespace webkit_glue {

FormDataPredictions::FormDataPredictions() {
}

FormDataPredictions::FormDataPredictions(const FormDataPredictions& other)
    : data(other.data),
      signature(other.signature),
      experiment_id(other.experiment_id),
      fields(other.fields) {
}

FormDataPredictions::~FormDataPredictions() {
}

}  // namespace webkit_glue
