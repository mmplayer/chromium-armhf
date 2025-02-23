// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PhishingDOMFeatureExtractor handles computing DOM-based features for the
// client-side phishing detection model.  These include the presence of various
// types of elements, ratios of external and secure links, and tokens for
// external domains linked to.

#ifndef CHROME_RENDERER_SAFE_BROWSING_PHISHING_DOM_FEATURE_EXTRACTOR_H_
#define CHROME_RENDERER_SAFE_BROWSING_PHISHING_DOM_FEATURE_EXTRACTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_old.h"
#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"

class GURL;

namespace WebKit {
class WebElement;
}

namespace content {
class RenderView;
}

namespace safe_browsing {
class FeatureExtractorClock;
class FeatureMap;

class PhishingDOMFeatureExtractor {
 public:
  // Callback to be run when feature extraction finishes.  The callback
  // argument is true if extraction was successful, false otherwise.
  typedef Callback1<bool>::Type DoneCallback;

  // Creates a PhishingDOMFeatureExtractor for the specified RenderView.
  // The PhishingDOMFeatureExtrator should be destroyed prior to destroying
  // the RenderView.  |clock| is used for timing feature extractor operations,
  // and may be mocked for testing.  The caller maintains ownership of the
  // clock.
  PhishingDOMFeatureExtractor(content::RenderView* render_view,
                              FeatureExtractorClock* clock);
  ~PhishingDOMFeatureExtractor();

  // Begins extracting features into the given FeatureMap for the page
  // currently loaded in this object's RenderView.  To avoid blocking the
  // render thread for too long, the feature extractor may run in several
  // chunks of work, posting a task to the current MessageLoop to continue
  // processing.  Once feature extraction is complete, |done_callback|
  // is run on the current thread.  PhishingDOMFeatureExtractor takes
  // ownership of the callback.
  void ExtractFeatures(FeatureMap* features, DoneCallback* done_callback);

  // Cancels any pending feature extraction.  The DoneCallback will not be run.
  // Must be called if there is a feature extraction in progress when the page
  // is unloaded or the PhishingDOMFeatureExtractor is destroyed.
  void CancelPendingExtraction();

 private:
  struct FrameData;
  struct PageFeatureState;

  // The maximum amount of wall time that we will spend on a single extraction
  // iteration before pausing to let other MessageLoop tasks run.
  static const int kMaxTimePerChunkMs;

  // The number of elements that we will process before checking to see whether
  // kMaxTimePerChunkMs has elapsed.  Since checking the current time can be
  // slow, we don't do this on every element processed.
  static const int kClockCheckGranularity;

  // The maximum total amount of time that the feature extractor will run
  // before giving up on the current page.
  static const int kMaxTotalTimeMs;

  // Does the actual work of ExtractFeatures.  ExtractFeaturesWithTimeout runs
  // until a predefined maximum amount of time has elapsed, then posts a task
  // to the current MessageLoop to continue extraction.  When extraction
  // finishes, calls RunCallback().
  void ExtractFeaturesWithTimeout();

  // Handlers for the various HTML elements that we compute features for.
  // Since some of the features (such as ratios) cannot be computed until
  // feature extraction is finished, these handlers do not add to the feature
  // map directly.  Instead, they update the values in the PageFeatureState.
  void HandleLink(const WebKit::WebElement& element);
  void HandleForm(const WebKit::WebElement& element);
  void HandleImage(const WebKit::WebElement& element);
  void HandleInput(const WebKit::WebElement& element);
  void HandleScript(const WebKit::WebElement& element);

  // Helper to verify that there is no pending feature extraction.  Dies in
  // debug builds if the state is not as expected.  This is a no-op in release
  // builds.
  void CheckNoPendingExtraction();

  // Runs |done_callback_| and then clears all internal state.
  void RunCallback(bool success);

  // Clears all internal feature extraction state.
  void Clear();

  // Called after advancing |cur_document_| to update the state in
  // |cur_frame_data_|.  Returns true if the state was updated successfully.
  bool ResetFrameData();

  // Returns the next document in frame-traversal order from cur_document_.
  // If there are no more documents, returns a null WebDocument.
  WebKit::WebDocument GetNextDocument();

  // Given a URL, checks whether the domain is different from the domain of
  // the current frame's URL.  If so, stores the domain in |domain| and returns
  // true, otherwise returns false.
  bool IsExternalDomain(const GURL& url, std::string* domain) const;

  // Called once all frames have been processed to compute features from the
  // PageFeatureState and add them to |features_|.  See features.h for a
  // description of which features are computed.
  void InsertFeatures();

  // Non-owned pointer to the view that we will extract features from.
  content::RenderView* render_view_;

  // Non-owned pointer to our clock.
  FeatureExtractorClock* clock_;

  // The output parameters from the most recent call to ExtractFeatures().
  FeatureMap* features_;  // The caller keeps ownership of this.
  scoped_ptr<DoneCallback> done_callback_;

  // The current (sub-)document that we are processing.  May be a null document
  // (isNull()) if we are not currently extracting features.
  WebKit::WebDocument cur_document_;

  // Stores extra state for |cur_document_| that will be persisted until we
  // advance to the next frame.
  scoped_ptr<FrameData> cur_frame_data_;

  // Stores the intermediate data used to create features.  This data is
  // accumulated across all frames in the RenderView.
  scoped_ptr<PageFeatureState> page_feature_state_;

  // Used to create ExtractFeaturesWithTimeout tasks.
  // These tasks are revoked if extraction is cancelled.
  ScopedRunnableMethodFactory<PhishingDOMFeatureExtractor> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(PhishingDOMFeatureExtractor);
};

}  // namespace safe_browsing

#endif  // CHROME_RENDERER_SAFE_BROWSING_PHISHING_DOM_FEATURE_EXTRACTOR_H_
