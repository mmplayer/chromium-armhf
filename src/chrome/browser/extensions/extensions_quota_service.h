// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The ExtensionsQuotaService uses heuristics to limit abusive requests
// made by extensions.  In this model 'items' (e.g individual bookmarks) are
// represented by a 'Bucket' that holds state for that item for one single
// interval of time.  The interval of time is defined as 'how long we need to
// watch an item (for a particular heuristic) before making a decision about
// quota violations'.  A heuristic is two functions: one mapping input
// arguments to a unique Bucket (the BucketMapper), and another to determine
// if a new request involving such an item at a given time is a violation.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSIONS_QUOTA_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSIONS_QUOTA_SERVICE_H_
#pragma once

#include <list>
#include <map>
#include <string>

#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/values.h"

class ExtensionFunction;
class QuotaLimitHeuristic;
typedef std::list<QuotaLimitHeuristic*> QuotaLimitHeuristics;

class ExtensionsQuotaService {
 public:
  // Some concrete heuristics (declared below) that ExtensionFunctions can
  // use to help the service make decisions about quota violations.
  class TimedLimit;
  class SustainedLimit;

  ExtensionsQuotaService();
  ~ExtensionsQuotaService();

  // Decide whether the invocation of |function| with argument |args| by the
  // extension specified by |extension_id| results in a quota limit violation.
  // Returns true if the request is fine and can proceed, false if the request
  // should be throttled and an error returned to the extension.
  bool Assess(const std::string& extension_id, ExtensionFunction* function,
              const ListValue* args, const base::TimeTicks& event_time);
 private:
  friend class ExtensionTestQuotaResetFunction;
  typedef std::map<std::string, QuotaLimitHeuristics> FunctionHeuristicsMap;

  // Purge resets all accumulated data (except |violators_|) as if the service
  // was just created. Called periodically so we don't consume an unbounded
  // amount of memory while tracking quota.  Yes, this could mean an extension
  // gets away with murder if it is timed right, but the extensions we are
  // trying to limit are ones that consistently violate, so we'll converge
  // to the correct set.
  void Purge();
  void PurgeFunctionHeuristicsMap(FunctionHeuristicsMap* map);
  base::RepeatingTimer<ExtensionsQuotaService> purge_timer_;

  // Our quota tracking state for extensions that have invoked quota limited
  // functions.  Each extension is treated separately, so extension ids are the
  // key for the mapping.  As an extension invokes functions, the map keeps
  // track of which functions it has invoked and the heuristics for each one.
  // Each heuristic will be evaluated and ANDed together to get a final answer.
  std::map<std::string, FunctionHeuristicsMap> function_heuristics_;

  // For now, as soon as an extension violates quota, we don't allow it to
  // make any more requests to quota limited functions.  This provides a quick
  // lookup for these extensions that is only stored in memory.
  base::hash_set<std::string> violators_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsQuotaService);
};

// A QuotaLimitHeuristic is two things: 1, A heuristic to map extension
// function arguments to corresponding Buckets for each input arg, and 2) a
// heuristic for determining if a new event involving a particular item
// (represented by its Bucket) constitutes a quota violation.
class QuotaLimitHeuristic {
 public:
  // Parameters to configure the amount of tokens allotted to individual
  // Bucket objects (see Below) and how often they are replenished.
  struct Config {
    // The maximum number of tokens a bucket can contain, and is refilled to
    // every epoch.
    int64 refill_token_count;

    // Specifies how frequently the bucket is logically refilled with tokens.
    base::TimeDelta refill_interval;
  };

  // A Bucket is how the heuristic portrays an individual item (since quota
  // limits are per item) and all associated state for an item that needs to
  // carry through multiple calls to Apply.  It "holds" tokens, which are
  // debited and credited in response to new events involving the item being
  // being represented.  For convenience, instead of actually periodically
  // refilling buckets they are just 'Reset' on-demand (e.g. when new events
  // come in). So, a bucket has an expiration to denote it has becomes stale.
  class Bucket {
   public:
    Bucket() : num_tokens_(0) {}
    // Removes a token from this bucket, and returns true if the bucket had
    // any tokens in the first place.
    bool DeductToken() { return num_tokens_-- > 0; }

    // Returns true if this bucket has tokens to deduct.
    bool has_tokens() const { return num_tokens_ > 0; }

    // Reset this bucket to specification (from internal configuration), to be
    // valid from |start| until the first refill interval elapses and it needs
    // to be reset again.
    void Reset(const Config& config, const base::TimeTicks& start);

    // The time at which the token count and next expiration should be reset,
    // via a call to Reset.
    const base::TimeTicks& expiration() { return expiration_; }
   private:
    base::TimeTicks expiration_;
    int64 num_tokens_;
    DISALLOW_COPY_AND_ASSIGN(Bucket);
  };
  typedef std::list<Bucket*> BucketList;

  // A generic error message for quota violating requests.
  static const char kGenericOverQuotaError[];

  // A helper interface to retrieve the bucket corresponding to |args| from
  // the set of buckets (which is typically stored in the BucketMapper itself)
  // for this QuotaLimitHeuristic.
  class BucketMapper {
   public:
    virtual ~BucketMapper() {}
    // In most cases, this should simply extract item IDs from the arguments
    // (e.g for bookmark operations involving an existing item). If a problem
    // occurs while parsing |args|, the function aborts - buckets may be non-
    // empty). The expectation is that invalid args and associated errors are
    // handled by the ExtensionFunction itself so we don't concern ourselves.
    virtual void GetBucketsForArgs(const ListValue* args,
                                   BucketList* buckets) = 0;
  };

  // Ownership of |mapper| is given to the new QuotaLimitHeuristic.
  QuotaLimitHeuristic(const Config& config, BucketMapper* map);
  virtual ~QuotaLimitHeuristic();

  // Determines if sufficient quota exists (according to the Apply
  // implementation of a derived class) to perform an operation with |args|,
  // based on the history of similar operations with similar arguments (which
  // is retrieved using the BucketMapper).
  bool ApplyToArgs(const ListValue* args, const base::TimeTicks& event_time);

 protected:
  const Config& config() { return config_; }

  // Determine if the new event occurring at |event_time| involving |bucket|
  // constitutes a quota violation according to this heuristic.
  virtual bool Apply(Bucket* bucket, const base::TimeTicks& event_time) = 0;

 private:
  friend class QuotaLimitHeuristicTest;

  const Config config_;

  // The mapper used in Map.  Cannot be NULL.
  scoped_ptr<BucketMapper> bucket_mapper_;

  DISALLOW_COPY_AND_ASSIGN(QuotaLimitHeuristic);
};

// A simple per-item heuristic to limit the number of events that can occur in
// a given period of time; e.g "no more than 100 events in an hour".
class ExtensionsQuotaService::TimedLimit : public QuotaLimitHeuristic {
 public:
  TimedLimit(const Config& config, BucketMapper* map)
      : QuotaLimitHeuristic(config, map) {}
  virtual bool Apply(Bucket* bucket, const base::TimeTicks& event_time);
};

// A per-item heuristic to limit the number of events that can occur in a
// period of time over a sustained longer interval. E.g "no more than two
// events per minute, sustained over 10 minutes".
class ExtensionsQuotaService::SustainedLimit : public QuotaLimitHeuristic {
 public:
  SustainedLimit(const base::TimeDelta& sustain,
                 const Config& config,
                 BucketMapper* map);
  virtual bool Apply(Bucket* bucket, const base::TimeTicks& event_time);
 private:
  // Specifies how long exhaustion of buckets is allowed to continue before
  // denying requests.
  const int64 repeat_exhaustion_allowance_;
  int64 num_available_repeat_exhaustions_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSIONS_QUOTA_SERVICE_H_
