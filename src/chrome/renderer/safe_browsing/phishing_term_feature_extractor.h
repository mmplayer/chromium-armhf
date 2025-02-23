// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PhishingTermFeatureExtractor handles computing term features from the text
// of a web page for the client-side phishing detection model.  To do this, it
// takes a list of terms that appear in the model, and scans through the page
// text looking for them.  Any terms that appear will cause a corresponding
// features::kPageTerm feature to be added to the FeatureMap.
//
// To make it harder for a phisher to enumerate all of the relevant terms in
// the model, the terms are provided as SHA-256 hashes, rather than plain text.
//
// There is one PhishingTermFeatureExtractor per RenderView.

#ifndef CHROME_RENDERER_SAFE_BROWSING_PHISHING_TERM_FEATURE_EXTRACTOR_H_
#define CHROME_RENDERER_SAFE_BROWSING_PHISHING_TERM_FEATURE_EXTRACTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_old.h"
#include "base/hash_tables.h"
#include "base/memory/mru_cache.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/string16.h"
#include "base/task.h"

namespace safe_browsing {
class FeatureExtractorClock;
class FeatureMap;

class PhishingTermFeatureExtractor {
 public:
  // Callback to be run when feature extraction finishes.  The callback
  // argument is true if extraction was successful, false otherwise.
  typedef Callback1<bool>::Type DoneCallback;

  // Creates a PhishingTermFeatureExtractor which will extract features for
  // all of the terms whose SHA-256 hashes are in |page_term_hashes|.  These
  // terms may be multi-word n-grams, with at most |max_words_per_term| words.
  //
  // |page_word_hashes| contains the murmur3 hashes for all of the individual
  // words that make up the terms.  Both sets of strings are UTF-8 encoded and
  // lowercased prior to hashing.  The caller owns both sets of strings, and
  // must ensure that they are valid until the PhishingTermFeatureExtractor is
  // destroyed.
  //
  // |clock| is used for timing feature extractor operations, and may be mocked
  // for testing.  The caller keeps ownership of the clock.
  PhishingTermFeatureExtractor(
      const base::hash_set<std::string>* page_term_hashes,
      const base::hash_set<uint32>* page_word_hashes,
      size_t max_words_per_term,
      uint32 murmurhash3_seed,
      FeatureExtractorClock* clock);
  ~PhishingTermFeatureExtractor();

  // Begins extracting features from |page_text| into the given FeatureMap.
  // |page_text| should contain the plain text of a web page, including any
  // subframes, as returned by RenderView::CaptureText().
  //
  // To avoid blocking the render thread for too long, the feature extractor
  // may run in several chunks of work, posting a task to the current
  // MessageLoop to continue processing.  Once feature extraction is complete,
  // |done_callback| is run on the current thread.
  // PhishingTermFeatureExtractor takes ownership of the callback.
  //
  // |page_text| and |features| are owned by the caller, and must not be
  // destroyed until either |done_callback| is run or
  // CancelPendingExtraction() is called.
  void ExtractFeatures(const string16* page_text,
                       FeatureMap* features,
                       DoneCallback* done_callback);

  // Cancels any pending feature extraction.  The DoneCallback will not be run.
  // Must be called if there is a feature extraction in progress when the page
  // is unloaded or the PhishingTermFeatureExtractor is destroyed.
  void CancelPendingExtraction();

 private:
  struct ExtractionState;

  // The maximum amount of wall time that we will spend on a single extraction
  // iteration before pausing to let other MessageLoop tasks run.
  static const int kMaxTimePerChunkMs;

  // The number of words that we will process before checking to see whether
  // kMaxTimePerChunkMs has elapsed.  Since checking the current time can be
  // slow, we don't do this on every word processed.
  static const int kClockCheckGranularity;

  // The maximum total amount of time that the feature extractor will run
  // before giving up on the current page.
  static const int kMaxTotalTimeMs;

  // The size of the cache that we use to determine if we can avoid lower
  // casing, hashing, and UTF conversion.
  static const int kMaxNegativeWordCacheSize;

  // Does the actual work of ExtractFeatures.  ExtractFeaturesWithTimeout runs
  // until a predefined maximum amount of time has elapsed, then posts a task
  // to the current MessageLoop to continue extraction.  When extraction
  // finishes, calls RunCallback().
  void ExtractFeaturesWithTimeout();

  // Handles a single word in the page text.
  void HandleWord(const base::StringPiece16& word);

  // Helper to verify that there is no pending feature extraction.  Dies in
  // debug builds if the state is not as expected.  This is a no-op in release
  // builds.
  void CheckNoPendingExtraction();

  // Runs |done_callback_| and then clears all internal state.
  void RunCallback(bool success);

  // Clears all internal feature extraction state.
  void Clear();

  // All of the term hashes that we are looking for in the page.
  const base::hash_set<std::string>* page_term_hashes_;

  // Murmur3 hashes of all the individual words in page_term_hashes_.  If
  // page_term_hashes_ included (hashed) "one" and "one two", page_word_hashes_
  // would contain (hashed) "one" and "two".  We do this so that we can have a
  // quick out in the common case that the current word we are processing
  // doesn't contain any part of one of our terms.
  const base::hash_set<uint32>* page_word_hashes_;

  // The maximum number of words in an n-gram.
  const size_t max_words_per_term_;

  // The seed for murmurhash3.
  const uint32 murmurhash3_seed_;

  // This cache is used to see if we need to check the word at all, as
  // converting to UTF8, lowercasing, and hashing are all relatively expensive
  // operations. Though this is called an MRU cache, it seems to behave like
  // an LRU cache (i.e. it evicts the oldest accesses first).
  typedef base::HashingMRUCache<base::StringPiece16, bool> WordCache;
  WordCache negative_word_cache_;

  // Non-owned pointer to our clock.
  FeatureExtractorClock* clock_;

  // The output parameters from the most recent call to ExtractFeatures().
  const string16* page_text_;  // The caller keeps ownership of this.
  FeatureMap* features_;  // The caller keeps ownership of this.
  scoped_ptr<DoneCallback> done_callback_;

  // Stores the current state of term extraction from |page_text_|.
  scoped_ptr<ExtractionState> state_;

  // Used to create ExtractFeaturesWithTimeout tasks.
  // These tasks are revoked if extraction is cancelled.
  ScopedRunnableMethodFactory<PhishingTermFeatureExtractor> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(PhishingTermFeatureExtractor);
};

}  // namespace safe_browsing

#endif  // CHROME_RENDERER_SAFE_BROWSING_PHISHING_TERM_FEATURE_EXTRACTOR_H_
