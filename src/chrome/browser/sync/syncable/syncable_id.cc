// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/syncable/syncable_id.h"

#include <iosfwd>

#include "base/string_util.h"
#include "base/values.h"

using std::ostream;
using std::string;

namespace syncable {

ostream& operator<<(ostream& out, const Id& id) {
  out << id.s_;
  return out;
}

StringValue* Id::ToValue() const {
  return Value::CreateStringValue(s_);
}

string Id::GetServerId() const {
  // Currently root is the string "0". We need to decide on a true value.
  // "" would be convenient here, as the IsRoot call would not be needed.
  if (IsRoot())
    return "0";
  return s_.substr(1);
}

Id Id::CreateFromServerId(const string& server_id) {
  Id id;
  if (server_id == "0")
    id.s_ = "r";
  else
    id.s_ = string("s") + server_id;
  return id;
}

Id Id::CreateFromClientString(const string& local_id) {
  Id id;
  if (local_id == "0")
    id.s_ = "r";
  else
    id.s_ = string("c") + local_id;
  return id;
}

Id Id::GetLexicographicSuccessor() const {
  // The successor of a string is given by appending the least
  // character in the alphabet.
  Id id = *this;
  id.s_.push_back(0);
  return id;
}

bool Id::ContainsStringCaseInsensitive(
    const std::string& lowercase_query) const {
  DCHECK_EQ(StringToLowerASCII(lowercase_query), lowercase_query);
  return StringToLowerASCII(s_).find(lowercase_query) != std::string::npos;
}

// static
Id Id::GetLeastIdForLexicographicComparison() {
  Id id;
  id.s_.clear();
  return id;
}

Id GetNullId() {
  return Id();  // Currently == root.
}

}  // namespace syncable
