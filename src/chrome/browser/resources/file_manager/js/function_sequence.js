// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @class FunctionSequence to invoke steps in sequence
 *
 * @param {Array} steps                array of functions to invoke in sequence
 * @param {Object} logger              logger
 * @param {Function} [callback]        callback to invoke on success
 * @param {Function} [failureCallback] callback to invoke on failure
 */
function FunctionSequence(name, steps, logger, callback, failureCallback) {
  // Private variables hidden in closure
  this.currentStepIdx_ = -1;
  this.failed_ = false;
  this.steps_ = steps;
  this.callback_ = callback;
  this.failureCallback_ = failureCallback;
  this.logger = logger;
  this.name = name;

  this.onError = this.onError_.bind(this);
  this.finish = this.finish_.bind(this);
  this.nextStep = this.nextStep_.bind(this);
  this.apply = this.apply_.bind(this);
}

/**
 * Sets new callback
 *
 * @param {Function} callback new callback to call on succeed
 */
FunctionSequence.prototype.setCallback = function(callback) {
    this.callback_ = callback;
};

/**
 * Sets new error callback
 *
 * @param {Function} failureCallback new callback to call on failure
 */
FunctionSequence.prototype.setFailureCallback = function(failureCallback) {
    this.failureCallback_ = failureCallback;
};


/**
 * Error handling function, which traces current error step, stops sequence
 * advancing and fires error callback.
 *
 * @param err error message
 */
FunctionSequence.prototype.onError_ = function(err) {
  this.logger.vlog('Failed step: ' + this.steps_[this.currentStepIdx_].name
      + ': '
      + err);
  if (!this.failed_) {
    this.failed_ = true;
    this.failureCallback_(err);
  }
};

/**
 * Finishes sequence processing and jumps to the last step.
 * This method should not be used externally. In external
 * cases should be used finish function, which is defined in closure and thus
 * has access to internal variables of functionsequence.
 */
FunctionSequence.prototype.finish_ = function() {
  if (!this.failed_ && this.currentStepIdx_ < this.steps_.length) {
    this.currentStepIdx_ = this.steps_.length;
    this.callback_();
  }
};

/**
 * Advances to next step.
 * This method should not be used externally. In external
 * cases should be used nextStep function, which is defined in closure and thus
 * has access to internal variables of functionsequence.
 */
FunctionSequence.prototype.nextStep_ = function(var_args) {
  if (this.failed_) {
    return;
  }

  if (++this.currentStepIdx_ >= this.steps_.length) {
    this.logger.vlog('Sequence ended');
    this.callback_.apply(this, arguments);
  } else {
    this.logger.vlog('Attempting to start step ['
                      + this.steps_[this.currentStepIdx_].name
                      + ']');
    this.steps_[this.currentStepIdx_].apply(this, arguments);
  }
};

/**
 * This function should be called only once on start, so start sequence pipeline
 */
FunctionSequence.prototype.start = function(var_args) {
  if (this.started) {
    throw new Error('"Start" method of FunctionSequence was called twice');
  }

  this.logger.log("Starting sequence with " + arguments.length + " arguments");

  this.started = true;
  this.nextStep.apply(this, arguments);
};

/**
 * Add Function object mimics to FunctionSequence
 */
FunctionSequence.prototype.apply_ = function(obj, args) {
  this.start.apply(this, args);
};

