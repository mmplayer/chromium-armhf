// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/internal_auth.h"

#include <algorithm>
#include <deque>

#include "base/base64.h"
#include "base/lazy_instance.h"
#include "base/rand_util.h"
#include "base/synchronization/lock.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/threading/thread_checker.h"
#include "base/time.h"
#include "base/values.h"
#include "content/browser/browser_thread.h"
#include "crypto/hmac.h"

namespace {

typedef std::map<std::string, std::string> VarValueMap;

// Size of a tick in microseconds. This determines upper bound for average
// number of passports generated per time unit. This bound equals to
// (kMicrosecondsPerSecond / TickUs) calls per second.
const int64 kTickUs = 10000;

// Verification window size in ticks; that means any passport expires in
// (kVerificationWindowTicks * TickUs / kMicrosecondsPerSecond) seconds.
const int kVerificationWindowTicks = 2000;

// Generation window determines how well we are able to cope with bursts of
// GeneratePassport calls those exceed upper bound on average speed.
const int kGenerationWindowTicks = 20;

// Makes no sense to compare other way round.
COMPILE_ASSERT(kGenerationWindowTicks <= kVerificationWindowTicks,
    makes_no_sense_to_have_generation_window_larger_than_verification_one);
// We are not optimized for high value of kGenerationWindowTicks.
COMPILE_ASSERT(kGenerationWindowTicks < 30, too_large_generation_window);

// Regenerate key after this number of ticks.
const int kKeyRegenerationSoftTicks = 500000;
// Reject passports if key has not been regenerated in that number of ticks.
const int kKeyRegenerationHardTicks = kKeyRegenerationSoftTicks * 2;

// Limit for number of accepted var=value pairs. Feel free to bump this limit
// higher once needed.
const size_t kVarsLimit = 16;

// Limit for length of caller-supplied strings. Feel free to bump this limit
// higher once needed.
const size_t kStringLengthLimit = 512;

// Character used as a separator for construction of message to take HMAC of.
// It is critical to validate all caller-supplied data (used to construct
// message) to be clear of this separator because it could allow attacks.
const char kItemSeparator = '\n';

// Character used for var=value separation.
const char kVarValueSeparator = '=';

const size_t kKeySizeInBytes = 128 / 8;
const int kHMACSizeInBytes = 256 / 8;

// Length of base64 string required to encode given number of raw octets.
#define BASE64_PER_RAW(X) (X > 0 ? ((X - 1) / 3 + 1) * 4 : 0)

// Size of decimal string representing 64-bit tick.
const size_t kTickStringLength = 20;

// A passport consists of 2 parts: HMAC and tick.
const size_t kPassportSize =
    BASE64_PER_RAW(kHMACSizeInBytes) + kTickStringLength;

int64 GetCurrentTick() {
  int64 tick = base::Time::Now().ToInternalValue() / kTickUs;
  if (tick < kVerificationWindowTicks ||
      tick < kKeyRegenerationHardTicks ||
      tick > kint64max - kKeyRegenerationHardTicks) {
    return 0;
  }
  return tick;
}

bool IsDomainSane(const std::string& domain) {
  return !domain.empty() &&
      domain.size() <= kStringLengthLimit &&
      IsStringUTF8(domain) &&
      domain.find_first_of(kItemSeparator) == std::string::npos;
}

bool IsVarSane(const std::string& var) {
  static const char kAllowedChars[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789"
      "_";
  COMPILE_ASSERT(
      sizeof(kAllowedChars) == 26 + 26 + 10 + 1 + 1, some_mess_with_chars);
  // We must not allow kItemSeparator in anything used as an input to construct
  // message to sign.
  DCHECK(std::find(kAllowedChars, kAllowedChars + arraysize(kAllowedChars),
      kItemSeparator) == kAllowedChars + arraysize(kAllowedChars));
  DCHECK(std::find(kAllowedChars, kAllowedChars + arraysize(kAllowedChars),
      kVarValueSeparator) == kAllowedChars + arraysize(kAllowedChars));
  return !var.empty() &&
      var.size() <= kStringLengthLimit &&
      IsStringASCII(var) &&
      var.find_first_not_of(kAllowedChars) == std::string::npos &&
      !IsAsciiDigit(var[0]);
}

bool IsValueSane(const std::string& value) {
  return value.size() <= kStringLengthLimit &&
      IsStringUTF8(value) &&
      value.find_first_of(kItemSeparator) == std::string::npos;
}

bool IsVarValueMapSane(const VarValueMap& map) {
  if (map.size() > kVarsLimit)
    return false;
  for (VarValueMap::const_iterator it = map.begin(); it != map.end(); ++it) {
    const std::string& var = it->first;
    const std::string& value = it->second;
    if (!IsVarSane(var) || !IsValueSane(value))
      return false;
  }
  return true;
}

void ConvertVarValueMapToBlob(const VarValueMap& map, std::string* out) {
  out->clear();
  DCHECK(IsVarValueMapSane(map));
  for (VarValueMap::const_iterator it = map.begin(); it != map.end(); ++it)
    *out += it->first + kVarValueSeparator + it->second + kItemSeparator;
}

void CreatePassport(
    const std::string& domain,
    const VarValueMap& map,
    int64 tick,
    const crypto::HMAC* engine,
    std::string* out) {
  DCHECK(engine);
  DCHECK(out);
  DCHECK(IsDomainSane(domain));
  DCHECK(IsVarValueMapSane(map));

  out->clear();
  std::string result(kPassportSize, '0');

  std::string blob;
  blob = domain + kItemSeparator;
  std::string tmp;
  ConvertVarValueMapToBlob(map, &tmp);
  blob += tmp + kItemSeparator + base::Uint64ToString(tick);

  std::string hmac;
  unsigned char* hmac_data = reinterpret_cast<unsigned char*>(
      WriteInto(&hmac, kHMACSizeInBytes + 1));
  if (!engine->Sign(blob, hmac_data, kHMACSizeInBytes)) {
    NOTREACHED();
    return;
  }
  std::string hmac_base64;
  if (!base::Base64Encode(hmac, &hmac_base64)) {
    NOTREACHED();
    return;
  }
  if (hmac_base64.size() != BASE64_PER_RAW(kHMACSizeInBytes)) {
    NOTREACHED();
    return;
  }
  DCHECK(hmac_base64.size() < result.size());
  std::copy(hmac_base64.begin(), hmac_base64.end(), result.begin());

  std::string tick_decimal = base::Uint64ToString(tick);
  DCHECK(tick_decimal.size() <= kTickStringLength);
  std::copy(
      tick_decimal.begin(),
      tick_decimal.end(),
      result.begin() + kPassportSize - tick_decimal.size());

  out->swap(result);
}

}  // namespace

namespace browser {

class InternalAuthVerificationService {
 public:
  InternalAuthVerificationService()
      : key_change_tick_(0),
        dark_tick_(0) {
  }

  bool VerifyPassport(
      const std::string& passport,
      const std::string& domain,
      const VarValueMap& map) {
    int64 current_tick = GetCurrentTick();
    int64 tick = PreVerifyPassport(passport, domain, current_tick);
    if (tick == 0)
      return false;
    if (!IsVarValueMapSane(map))
      return false;
    std::string reference_passport;
    CreatePassport(domain, map, tick, engine_.get(), &reference_passport);
    if (passport != reference_passport) {
      // Consider old key.
      if (key_change_tick_ + get_verification_window_ticks() < tick) {
        return false;
      }
      if (old_key_.empty() || old_engine_ == NULL)
        return false;
      CreatePassport(domain, map, tick, old_engine_.get(), &reference_passport);
      if (passport != reference_passport)
        return false;
    }

    // Record used tick to prevent reuse.
    std::deque<int64>::iterator it = std::lower_bound(
        used_ticks_.begin(), used_ticks_.end(), tick);
    DCHECK(it == used_ticks_.end() || *it != tick);
    used_ticks_.insert(it, tick);

    // Consider pruning |used_ticks_|.
    if (used_ticks_.size() > 50) {
      dark_tick_ = std::max(dark_tick_,
          current_tick - get_verification_window_ticks());
      used_ticks_.erase(
          used_ticks_.begin(),
          std::lower_bound(used_ticks_.begin(), used_ticks_.end(),
                           dark_tick_ + 1));
    }
    return true;
  }

  void ChangeKey(const std::string& key) {
    old_key_.swap(key_);
    key_.clear();
    old_engine_.swap(engine_);
    engine_.reset(NULL);

    if (key.size() != kKeySizeInBytes)
      return;
    scoped_ptr<crypto::HMAC> new_engine(
        new crypto::HMAC(crypto::HMAC::SHA256));
    if (!new_engine->Init(key))
      return;
    engine_.swap(new_engine);
    key_ = key;
    key_change_tick_ = GetCurrentTick();
  }

 private:
  static int get_verification_window_ticks() {
    return InternalAuthVerification::get_verification_window_ticks();
  }

  // Returns tick bound to given passport on success or zero on failure.
  int64 PreVerifyPassport(
    const std::string& passport,
    const std::string& domain,
    int64 current_tick) {
    if (passport.size() != kPassportSize ||
        !IsStringASCII(passport) ||
        !IsDomainSane(domain) ||
        current_tick <= dark_tick_ ||
        current_tick > key_change_tick_  + kKeyRegenerationHardTicks ||
        key_.empty() ||
        engine_ == NULL) {
      return 0;
    }

    // Passport consists of 2 parts: first hmac and then tick.
    std::string tick_decimal =
        passport.substr(BASE64_PER_RAW(kHMACSizeInBytes));
    DCHECK(tick_decimal.size() == kTickStringLength);
    int64 tick = 0;
    if (!base::StringToInt64(tick_decimal, &tick) ||
        tick <= dark_tick_ ||
        tick > key_change_tick_ + kKeyRegenerationHardTicks ||
        tick < current_tick - get_verification_window_ticks() ||
        std::binary_search(used_ticks_.begin(), used_ticks_.end(), tick)) {
      return 0;
    }
    return tick;
  }

  // Current key.
  std::string key_;

  // We keep previous key in order to be able to verify passports during
  // regeneration time.  Keys are regenerated on a regular basis.
  std::string old_key_;

  // Corresponding HMAC engines.
  scoped_ptr<crypto::HMAC> engine_;
  scoped_ptr<crypto::HMAC> old_engine_;

  // Tick at a time of recent key regeneration.
  int64 key_change_tick_;

  // Keeps track of ticks of successfully verified passports to prevent their
  // reuse. Size of this container is kept reasonably low by purging outdated
  // ticks.
  std::deque<int64> used_ticks_;

  // Some ticks before |dark_tick_| were purged from |used_ticks_| container.
  // That means that we must not trust any tick less than or equal to dark tick.
  int64 dark_tick_;

  DISALLOW_COPY_AND_ASSIGN(InternalAuthVerificationService);
};

}  // namespace browser

namespace {

static base::LazyInstance<browser::InternalAuthVerificationService>
    g_verification_service(base::LINKER_INITIALIZED);
static base::LazyInstance<base::Lock> g_verification_service_lock(
    base::LINKER_INITIALIZED);

}  // namespace

namespace browser {

class InternalAuthGenerationService : public base::ThreadChecker {
 public:
  InternalAuthGenerationService() : key_regeneration_tick_(0) {
    GenerateNewKey();
  }

  void GenerateNewKey() {
    DCHECK(CalledOnValidThread());
    scoped_ptr<crypto::HMAC> new_engine(new crypto::HMAC(crypto::HMAC::SHA256));
    std::string key = base::RandBytesAsString(kKeySizeInBytes);
    if (!new_engine->Init(key))
      return;
    engine_.swap(new_engine);
    key_regeneration_tick_ = GetCurrentTick();
    g_verification_service.Get().ChangeKey(key);
    std::fill(key.begin(), key.end(), 0);
  }

  // Returns zero on failure.
  int64 GetUnusedTick(const std::string& domain) {
    DCHECK(CalledOnValidThread());
    if (engine_ == NULL) {
      NOTREACHED();
      return 0;
    }
    if (!IsDomainSane(domain))
      return 0;

    int64 current_tick = GetCurrentTick();
    if (!used_ticks_.empty() && used_ticks_.back() > current_tick)
      current_tick = used_ticks_.back();
    for (bool first_iteration = true;; first_iteration = false) {
      if (current_tick < key_regeneration_tick_ + kKeyRegenerationHardTicks)
        break;
      if (!first_iteration)
        return 0;
      GenerateNewKey();
    }

    // Forget outdated ticks if any.
    used_ticks_.erase(
        used_ticks_.begin(),
        std::lower_bound(used_ticks_.begin(), used_ticks_.end(),
                         current_tick - kGenerationWindowTicks + 1));
    DCHECK(used_ticks_.size() <= kGenerationWindowTicks + 0u);
    if (used_ticks_.size() >= kGenerationWindowTicks + 0u) {
      // Average speed of GeneratePassport calls exceeds limit.
      return 0;
    }
    for (int64 tick = current_tick;
        tick > current_tick - kGenerationWindowTicks;
        --tick) {
      int idx = static_cast<int>(used_ticks_.size()) -
          static_cast<int>(current_tick - tick + 1);
      if (idx < 0 || used_ticks_[idx] != tick) {
        DCHECK(used_ticks_.end() ==
            std::find(used_ticks_.begin(), used_ticks_.end(), tick));
        return tick;
      }
    }
    NOTREACHED();
    return 0;
  }

  std::string GeneratePassport(
      const std::string& domain, const VarValueMap& map, int64 tick) {
    DCHECK(CalledOnValidThread());
    if (tick == 0) {
      tick = GetUnusedTick(domain);
      if (tick == 0)
        return std::string();
    }
    if (!IsVarValueMapSane(map))
      return std::string();

    std::string result;
    CreatePassport(domain, map, tick, engine_.get(), &result);
    used_ticks_.insert(
        std::lower_bound(used_ticks_.begin(), used_ticks_.end(), tick), tick);
    return result;
  }

 private:
  static int get_verification_window_ticks() {
    return InternalAuthVerification::get_verification_window_ticks();
  }

  scoped_ptr<crypto::HMAC> engine_;
  int64 key_regeneration_tick_;
  std::deque<int64> used_ticks_;

  DISALLOW_COPY_AND_ASSIGN(InternalAuthGenerationService);
};

}  // namespace browser

namespace {

static base::LazyInstance<browser::InternalAuthGenerationService>
    g_generation_service(base::LINKER_INITIALIZED);

}  // namespace

namespace browser {

// static
bool InternalAuthVerification::VerifyPassport(
    const std::string& passport,
    const std::string& domain,
    const VarValueMap& var_value_map) {
  base::AutoLock alk(g_verification_service_lock.Get());
  return g_verification_service.Get().VerifyPassport(
      passport, domain, var_value_map);
}

// static
void InternalAuthVerification::ChangeKey(const std::string& key) {
  base::AutoLock alk(g_verification_service_lock.Get());
  g_verification_service.Get().ChangeKey(key);
};

// static
int InternalAuthVerification::get_verification_window_ticks() {
  int candidate = kVerificationWindowTicks;
  if (verification_window_seconds_ > 0)
    candidate = verification_window_seconds_ *
        base::Time::kMicrosecondsPerSecond / kTickUs;
  return std::max(1, std::min(candidate, kVerificationWindowTicks));
}

int InternalAuthVerification::verification_window_seconds_ = 0;

// static
std::string InternalAuthGeneration::GeneratePassport(
    const std::string& domain, const VarValueMap& var_value_map) {
  return g_generation_service.Get().GeneratePassport(domain, var_value_map, 0);
}

// static
void InternalAuthGeneration::GenerateNewKey() {
  g_generation_service.Get().GenerateNewKey();
}

}  // namespace browser

