// Copyright (C) 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: Fredrik Roubert

#include "regexp_cache.h"

#include <cstddef>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "regexp_adapter.h"

using std::string;

namespace i18n {
namespace phonenumbers {

using base::AutoLock;

RegExpCache::RegExpCache(size_t min_items)
#ifdef USE_TR1_UNORDERED_MAP
    : cache_impl_(new CacheImpl(min_items)) {}
#else  // USE_TR1_UNORDERED_MAP
    : cache_impl_(new CacheImpl()) {}
#endif  // USE_TR1_UNORDERED_MAP

RegExpCache::~RegExpCache() {
  AutoLock l(lock_);
  for (CacheImpl::const_iterator
       it = cache_impl_->begin(); it != cache_impl_->end(); ++it) {
    delete it->second;
  }
}

const RegExp& RegExpCache::GetRegExp(const string& pattern) {
  AutoLock l(lock_);
  CacheImpl::const_iterator it = cache_impl_->find(pattern);
  if (it != cache_impl_->end()) return *it->second;

  const RegExp* regexp = RegExp::Create(pattern);
  cache_impl_->insert(make_pair(pattern, regexp));
  return *regexp;
}

}  // namespace phonenumbers
}  // namespace i18n
