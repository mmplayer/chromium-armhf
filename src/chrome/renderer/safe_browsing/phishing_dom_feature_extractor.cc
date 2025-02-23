// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/phishing_dom_feature_extractor.h"

#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/renderer/safe_browsing/feature_extractor_clock.h"
#include "chrome/renderer/safe_browsing/features.h"
#include "content/public/renderer/render_view.h"
#include "net/base/registry_controlled_domain.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNodeCollection.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

namespace safe_browsing {

// This time should be short enough that it doesn't noticeably disrupt the
// user's interaction with the page.
const int PhishingDOMFeatureExtractor::kMaxTimePerChunkMs = 20;

// Experimenting shows that we get a reasonable gain in performance by
// increasing this up to around 10, but there's not much benefit in
// increasing it past that.
const int PhishingDOMFeatureExtractor::kClockCheckGranularity = 10;

// This should be longer than we expect feature extraction to take on any
// actual phishing page.
const int PhishingDOMFeatureExtractor::kMaxTotalTimeMs = 500;

// Intermediate state used for computing features.  See features.h for
// descriptions of the DOM features that are computed.
struct PhishingDOMFeatureExtractor::PageFeatureState {
  // Link related features
  int external_links;
  base::hash_set<std::string> external_domains;
  int secure_links;
  int total_links;

  // Form related features
  int num_forms;
  int num_text_inputs;
  int num_pswd_inputs;
  int num_radio_inputs;
  int num_check_inputs;
  int action_other_domain;
  int total_actions;

  // Image related features
  int img_other_domain;
  int total_imgs;

  // How many script tags
  int num_script_tags;

  // The time at which we started feature extraction for the current page.
  base::TimeTicks start_time;

  // The number of iterations we've done for the current extraction.
  int num_iterations;

  explicit PageFeatureState(base::TimeTicks start_time_ticks)
      : external_links(0),
        secure_links(0),
        total_links(0),
        num_forms(0),
        num_text_inputs(0),
        num_pswd_inputs(0),
        num_radio_inputs(0),
        num_check_inputs(0),
        action_other_domain(0),
        total_actions(0),
        img_other_domain(0),
        total_imgs(0),
        num_script_tags(0),
        start_time(start_time_ticks),
        num_iterations(0) {}

  ~PageFeatureState() {}
};

// Per-frame state
struct PhishingDOMFeatureExtractor::FrameData {
  // This is our reference to document.all, which is an iterator over all
  // of the elements in the document.  It keeps track of our current position.
  WebKit::WebNodeCollection elements;
  // The domain of the document URL, stored here so that we don't need to
  // recompute it every time it's needed.
  std::string domain;
};

PhishingDOMFeatureExtractor::PhishingDOMFeatureExtractor(
    content::RenderView* render_view,
    FeatureExtractorClock* clock)
    : render_view_(render_view),
      clock_(clock),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  Clear();
}

PhishingDOMFeatureExtractor::~PhishingDOMFeatureExtractor() {
  // The RenderView should have called CancelPendingExtraction() before
  // we are destroyed.
  CheckNoPendingExtraction();
}

void PhishingDOMFeatureExtractor::ExtractFeatures(
    FeatureMap* features,
    DoneCallback* done_callback) {
  // The RenderView should have called CancelPendingExtraction() before
  // starting a new extraction, so DCHECK this.
  CheckNoPendingExtraction();
  // However, in an opt build, we will go ahead and clean up the pending
  // extraction so that we can start in a known state.
  CancelPendingExtraction();

  features_ = features;
  done_callback_.reset(done_callback);

  page_feature_state_.reset(new PageFeatureState(clock_->Now()));
  WebKit::WebView* web_view = render_view_->GetWebView();
  if (web_view && web_view->mainFrame()) {
    cur_document_ = web_view->mainFrame()->document();
  }

  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &PhishingDOMFeatureExtractor::ExtractFeaturesWithTimeout));
}

void PhishingDOMFeatureExtractor::CancelPendingExtraction() {
  // Cancel any pending callbacks, and clear our state.
  method_factory_.RevokeAll();
  Clear();
}

void PhishingDOMFeatureExtractor::ExtractFeaturesWithTimeout() {
  DCHECK(page_feature_state_.get());
  ++page_feature_state_->num_iterations;
  base::TimeTicks current_chunk_start_time = clock_->Now();

  if (cur_document_.isNull()) {
    // This will only happen if we weren't able to get the document for the
    // main frame.  We'll treat this as an extraction failure.
    RunCallback(false);
    return;
  }

  int num_elements = 0;
  for (; !cur_document_.isNull(); cur_document_ = GetNextDocument()) {
    WebKit::WebNode cur_node;
    if (cur_frame_data_.get()) {
      // We're resuming traversal of a frame, so just advance to the next node.
      cur_node = cur_frame_data_->elements.nextItem();
      // When we resume the traversal, the first call to nextItem() potentially
      // has to walk through the document again from the beginning, if it was
      // modified between our chunks of work.  Log how long this takes, so we
      // can tell if it's too slow.
      UMA_HISTOGRAM_TIMES("SBClientPhishing.DOMFeatureResumeTime",
                          clock_->Now() - current_chunk_start_time);
    } else {
      // We just moved to a new frame, so update our frame state
      // and advance to the first element.
      if (!ResetFrameData()) {
        // Nothing in this frame, move on to the next one.
        DLOG(WARNING) << "No content in frame, skipping";
        continue;
      }
      cur_node = cur_frame_data_->elements.firstItem();
    }

    for (; !cur_node.isNull();
         cur_node = cur_frame_data_->elements.nextItem()) {
      if (!cur_node.isElementNode()) {
        continue;
      }
      WebKit::WebElement element = cur_node.to<WebKit::WebElement>();
      if (element.hasTagName("a")) {
        HandleLink(element);
      } else if (element.hasTagName("form")) {
        HandleForm(element);
      } else if (element.hasTagName("img")) {
        HandleImage(element);
      } else if (element.hasTagName("input")) {
        HandleInput(element);
      } else if (element.hasTagName("script")) {
        HandleScript(element);
      }

      if (++num_elements >= kClockCheckGranularity) {
        num_elements = 0;
        base::TimeTicks now = clock_->Now();
        if (now - page_feature_state_->start_time >=
            base::TimeDelta::FromMilliseconds(kMaxTotalTimeMs)) {
          DLOG(ERROR) << "Feature extraction took too long, giving up";
          // We expect this to happen infrequently, so record when it does.
          UMA_HISTOGRAM_COUNTS("SBClientPhishing.DOMFeatureTimeout", 1);
          RunCallback(false);
          return;
        }
        base::TimeDelta chunk_elapsed = now - current_chunk_start_time;
        if (chunk_elapsed >=
            base::TimeDelta::FromMilliseconds(kMaxTimePerChunkMs)) {
          // The time limit for the current chunk is up, so post a task to
          // continue extraction.
          //
          // Record how much time we actually spent on the chunk. If this is
          // much higher than kMaxTimePerChunkMs, we may need to adjust the
          // clock granularity.
          UMA_HISTOGRAM_TIMES("SBClientPhishing.DOMFeatureChunkTime",
                              chunk_elapsed);
          MessageLoop::current()->PostTask(
              FROM_HERE,
              method_factory_.NewRunnableMethod(
                  &PhishingDOMFeatureExtractor::ExtractFeaturesWithTimeout));
          return;
        }
        // Otherwise, continue.
      }
    }

    // We're done with this frame, recalculate the FrameData when we
    // advance to the next frame.
    cur_frame_data_.reset();
  }

  InsertFeatures();
  RunCallback(true);
}

void PhishingDOMFeatureExtractor::HandleLink(
    const WebKit::WebElement& element) {
  // Count the number of times we link to a different host.
  if (!element.hasAttribute("href")) {
    DVLOG(1) << "Skipping anchor tag with no href";
    return;
  }

  // Retrieve the link and resolve the link in case it's relative.
  WebKit::WebURL full_url = element.document().completeURL(
      element.getAttribute("href"));

  std::string domain;
  bool is_external = IsExternalDomain(full_url, &domain);
  if (domain.empty()) {
    DVLOG(1) << "Could not extract domain from link: " << full_url;
    return;
  }

  if (is_external) {
    ++page_feature_state_->external_links;

    // Record each unique domain that we link to.
    page_feature_state_->external_domains.insert(domain);
  }

  // Check how many are https links.
  if (GURL(full_url).SchemeIs("https")) {
    ++page_feature_state_->secure_links;
  }

  ++page_feature_state_->total_links;
}

void PhishingDOMFeatureExtractor::HandleForm(
    const WebKit::WebElement& element) {
  // Increment the number of forms on this page.
  ++page_feature_state_->num_forms;

  // Record whether the action points to a different domain.
  if (!element.hasAttribute("action")) {
    return;
  }

  WebKit::WebURL full_url = element.document().completeURL(
      element.getAttribute("action"));

  std::string domain;
  bool is_external = IsExternalDomain(full_url, &domain);
  if (domain.empty()) {
    DVLOG(1) << "Could not extract domain from form action: " << full_url;
    return;
  }

  if (is_external) {
    ++page_feature_state_->action_other_domain;
  }
  ++page_feature_state_->total_actions;
}

void PhishingDOMFeatureExtractor::HandleImage(
    const WebKit::WebElement& element) {
  if (!element.hasAttribute("src")) {
    DVLOG(1) << "Skipping img tag with no src";
  }

  // Record whether the image points to a different domain.
  WebKit::WebURL full_url = element.document().completeURL(
      element.getAttribute("src"));
  std::string domain;
  bool is_external = IsExternalDomain(full_url, &domain);
  if (domain.empty()) {
    DVLOG(1) << "Could not extract domain from image src: " << full_url;
    return;
  }

  if (is_external) {
    ++page_feature_state_->img_other_domain;
  }
  ++page_feature_state_->total_imgs;
}

void PhishingDOMFeatureExtractor::HandleInput(
    const WebKit::WebElement& element) {
  // The HTML spec says that if the type is unspecified, it defaults to text.
  // In addition, any unrecognized type will be treated as a text input.
  //
  // Note that we use the attribute value rather than
  // WebFormControlElement::formControlType() for consistency with the
  // way the phishing classification model is created.
  std::string type = element.getAttribute("type").utf8();
  StringToLowerASCII(&type);
  if (type == "password") {
    ++page_feature_state_->num_pswd_inputs;
  } else if (type == "radio") {
    ++page_feature_state_->num_radio_inputs;
  } else if (type == "checkbox") {
    ++page_feature_state_->num_check_inputs;
  } else if (type != "submit" && type != "reset" && type != "file" &&
             type != "hidden" && type != "image" && type != "button") {
    // Note that there are a number of new input types in HTML5 that are not
    // handled above.  For now, we will consider these as text inputs since
    // they could be used to capture user input.
    ++page_feature_state_->num_text_inputs;
  }
}

void PhishingDOMFeatureExtractor::HandleScript(
    const WebKit::WebElement& element) {
  ++page_feature_state_->num_script_tags;
}

void PhishingDOMFeatureExtractor::CheckNoPendingExtraction() {
  DCHECK(!done_callback_.get());
  DCHECK(!cur_frame_data_.get());
  DCHECK(cur_document_.isNull());
  if (done_callback_.get() || cur_frame_data_.get() ||
      !cur_document_.isNull()) {
    LOG(ERROR) << "Extraction in progress, missing call to "
               << "CancelPendingExtraction";
  }
}

void PhishingDOMFeatureExtractor::RunCallback(bool success) {
  // Record some timing stats that we can use to evaluate feature extraction
  // performance.  These include both successful and failed extractions.
  DCHECK(page_feature_state_.get());
  UMA_HISTOGRAM_COUNTS("SBClientPhishing.DOMFeatureIterations",
                       page_feature_state_->num_iterations);
  UMA_HISTOGRAM_TIMES("SBClientPhishing.DOMFeatureTotalTime",
                      clock_->Now() - page_feature_state_->start_time);

  DCHECK(done_callback_.get());
  done_callback_->Run(success);
  Clear();
}

void PhishingDOMFeatureExtractor::Clear() {
  features_ = NULL;
  done_callback_.reset(NULL);
  cur_frame_data_.reset(NULL);
  cur_document_.reset();
}

bool PhishingDOMFeatureExtractor::ResetFrameData() {
  DCHECK(!cur_document_.isNull());
  DCHECK(!cur_frame_data_.get());

  cur_frame_data_.reset(new FrameData());
  cur_frame_data_->elements = cur_document_.all();
  cur_frame_data_->domain =
      net::RegistryControlledDomainService::GetDomainAndRegistry(
          cur_document_.url());
  return true;
}

WebKit::WebDocument PhishingDOMFeatureExtractor::GetNextDocument() {
  DCHECK(!cur_document_.isNull());
  WebKit::WebFrame* frame = cur_document_.frame();
  // Advance to the next frame that contains a document, with no wrapping.
  if (frame) {
    while ((frame = frame->traverseNext(false))) {
      if (!frame->document().isNull()) {
        return frame->document();
      }
    }
  } else {
    // Keep track of how often frame traversal got "stuck" due to the
    // current subdocument getting removed from the frame tree.
    UMA_HISTOGRAM_COUNTS("SBClientPhishing.DOMFeatureFrameRemoved", 1);
  }
  return WebKit::WebDocument();
}

bool PhishingDOMFeatureExtractor::IsExternalDomain(const GURL& url,
                                                   std::string* domain) const {
  DCHECK(domain);
  DCHECK(cur_frame_data_.get());

  if (cur_frame_data_->domain.empty()) {
    return false;
  }

  // TODO(bryner): Ensure that the url encoding is consistent with the features
  // in the model.
  if (url.HostIsIPAddress()) {
    domain->assign(url.host());
  } else {
    domain->assign(net::RegistryControlledDomainService::GetDomainAndRegistry(
        url));
  }

  return !domain->empty() && *domain != cur_frame_data_->domain;
}

void PhishingDOMFeatureExtractor::InsertFeatures() {
  DCHECK(page_feature_state_.get());

  if (page_feature_state_->total_links > 0) {
    // Add a feature for the fraction of times the page links to an external
    // domain vs. an internal domain.
    double link_freq = static_cast<double>(
        page_feature_state_->external_links) /
        page_feature_state_->total_links;
    features_->AddRealFeature(features::kPageExternalLinksFreq, link_freq);

    // Add a feature for each unique domain that we're linking to
    for (base::hash_set<std::string>::iterator it =
             page_feature_state_->external_domains.begin();
         it != page_feature_state_->external_domains.end(); ++it) {
      features_->AddBooleanFeature(features::kPageLinkDomain + *it);
    }

    // Fraction of links that use https.
    double secure_freq = static_cast<double>(
        page_feature_state_->secure_links) / page_feature_state_->total_links;
    features_->AddRealFeature(features::kPageSecureLinksFreq, secure_freq);
  }

  // Record whether forms appear and whether various form elements appear.
  if (page_feature_state_->num_forms > 0) {
    features_->AddBooleanFeature(features::kPageHasForms);
  }
  if (page_feature_state_->num_text_inputs > 0) {
    features_->AddBooleanFeature(features::kPageHasTextInputs);
  }
  if (page_feature_state_->num_pswd_inputs > 0) {
    features_->AddBooleanFeature(features::kPageHasPswdInputs);
  }
  if (page_feature_state_->num_radio_inputs > 0) {
    features_->AddBooleanFeature(features::kPageHasRadioInputs);
  }
  if (page_feature_state_->num_check_inputs > 0) {
    features_->AddBooleanFeature(features::kPageHasCheckInputs);
  }

  // Record fraction of form actions that point to a different domain.
  if (page_feature_state_->total_actions > 0) {
    double action_freq = static_cast<double>(
        page_feature_state_->action_other_domain) /
        page_feature_state_->total_actions;
    features_->AddRealFeature(features::kPageActionOtherDomainFreq,
                              action_freq);
  }

  // Record how many image src attributes point to a different domain.
  if (page_feature_state_->total_imgs > 0) {
    double img_freq = static_cast<double>(
        page_feature_state_->img_other_domain) /
        page_feature_state_->total_imgs;
    features_->AddRealFeature(features::kPageImgOtherDomainFreq, img_freq);
  }

  // Record number of script tags (discretized for numerical stability.)
  if (page_feature_state_->num_script_tags > 1) {
    features_->AddBooleanFeature(features::kPageNumScriptTagsGTOne);
    if (page_feature_state_->num_script_tags > 6) {
      features_->AddBooleanFeature(features::kPageNumScriptTagsGTSix);
    }
  }
}

}  // namespace safe_browsing
