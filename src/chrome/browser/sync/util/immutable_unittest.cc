// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/immutable.h"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <list>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

// Helper class that keeps track of the token passed in at
// construction and how many times that token is copied.
class TokenCore : public base::RefCounted<TokenCore> {
 public:
  explicit TokenCore(const char* token) : token_(token), copy_count_(0) {}

  const char* GetToken() const { return token_; }

  void RecordCopy() { ++copy_count_; }

  int GetCopyCount() const { return copy_count_; }

 private:
  friend class base::RefCounted<TokenCore>;

  ~TokenCore() {}

  const char* const token_;
  int copy_count_;
};

enum SwapBehavior {
  USE_DEFAULT_SWAP,
  USE_FAST_SWAP_VIA_ADL,
  USE_FAST_SWAP_VIA_SPECIALIZATION
};

const char kEmptyToken[] = "<empty token>";

// Base class for various token classes, differing in swap behavior.
template <SwapBehavior>
class TokenBase {
 public:
  TokenBase() : core_(new TokenCore(kEmptyToken)) {}

  explicit TokenBase(const char* token) : core_(new TokenCore(token)) {}

  TokenBase(const TokenBase& other) : core_(other.core_) {
    core_->RecordCopy();
  }

  TokenBase& operator=(const TokenBase& other) {
    core_ = other.core_;
    core_->RecordCopy();
    return *this;
  }

  const char* GetToken() const {
    return core_->GetToken();
  }

  int GetCopyCount() const {
    return core_->GetCopyCount();
  }

  // For associative containers.
  bool operator<(const TokenBase& other) const {
    return std::string(GetToken()) < std::string(other.GetToken());
  }

  // STL-style swap.
  void swap(TokenBase& other) {
    using std::swap;
    swap(other.core_, core_);
  }

  // Google-style swap.
  void Swap(TokenBase* other) {
    using std::swap;
    swap(other->core_, core_);
  }

 private:
  scoped_refptr<TokenCore> core_;
};

typedef TokenBase<USE_DEFAULT_SWAP> Token;
typedef TokenBase<USE_FAST_SWAP_VIA_ADL> ADLToken;
typedef TokenBase<USE_FAST_SWAP_VIA_SPECIALIZATION> SpecializationToken;

void swap(ADLToken& t1, ADLToken& t2) {
  t1.Swap(&t2);
}

}  // namespace browser_sync

// Allowed by the standard (17.4.3.1/1).
namespace std {

template <>
void swap(browser_sync::SpecializationToken& t1,
          browser_sync::SpecializationToken& t2) {
  t1.Swap(&t2);
}

}  // namespace

namespace browser_sync {
namespace {

class ImmutableTest : public ::testing::Test {};

TEST_F(ImmutableTest, Int) {
  int x = 5;
  Immutable<int> ix(&x);
  EXPECT_EQ(5, ix.Get());
  EXPECT_EQ(0, x);
}

TEST_F(ImmutableTest, IntCopy) {
  int x = 5;
  Immutable<int> ix = Immutable<int>(&x);
  EXPECT_EQ(5, ix.Get());
  EXPECT_EQ(0, x);
}

TEST_F(ImmutableTest, IntAssign) {
  int x = 5;
  Immutable<int> ix;
  EXPECT_EQ(0, ix.Get());
  ix = Immutable<int>(&x);
  EXPECT_EQ(5, ix.Get());
  EXPECT_EQ(0, x);
}

TEST_F(ImmutableTest, IntMakeImmutable) {
  int x = 5;
  Immutable<int> ix = MakeImmutable(&x);
  EXPECT_EQ(5, ix.Get());
  EXPECT_EQ(0, x);
}

template <typename T, typename ImmutableT>
void RunTokenTest(const char* token, bool expect_copies) {
  SCOPED_TRACE(token);
  T t(token);
  EXPECT_EQ(token, t.GetToken());
  EXPECT_EQ(0, t.GetCopyCount());

  ImmutableT immutable_t(&t);
  EXPECT_EQ(token, immutable_t.Get().GetToken());
  EXPECT_EQ(kEmptyToken, t.GetToken());
  EXPECT_EQ(expect_copies, immutable_t.Get().GetCopyCount() > 0);
  EXPECT_EQ(expect_copies, t.GetCopyCount() > 0);
}

TEST_F(ImmutableTest, Token) {
  RunTokenTest<Token, Immutable<Token> >("Token", true /* expect_copies */);
}

TEST_F(ImmutableTest, TokenSwapMemFnByRef) {
  RunTokenTest<Token, Immutable<Token, HasSwapMemFnByRef<Token> > >(
      "TokenSwapMemFnByRef", false /* expect_copies */);
}

TEST_F(ImmutableTest, TokenSwapMemFnByPtr) {
  RunTokenTest<Token, Immutable<Token, HasSwapMemFnByPtr<Token> > >(
      "TokenSwapMemFnByPtr", false /* expect_copies */);
}

TEST_F(ImmutableTest, ADLToken) {
  RunTokenTest<ADLToken, Immutable<ADLToken> >(
      "ADLToken", false /* expect_copies */);
}

TEST_F(ImmutableTest, SpecializationToken) {
  RunTokenTest<SpecializationToken, Immutable<SpecializationToken> >(
      "SpecializationToken", false /* expect_copies */);
}

template <typename C, typename ImmutableC>
void RunTokenContainerTest(const char* token) {
  SCOPED_TRACE(token);
  const Token tokens[] = { Token(), Token(token) };
  const size_t token_count = arraysize(tokens);
  C c(tokens, tokens + token_count);
  const int copy_count = c.begin()->GetCopyCount();
  EXPECT_GT(copy_count, 0);
  for (typename C::const_iterator it = c.begin(); it != c.end(); ++it) {
    EXPECT_EQ(copy_count, it->GetCopyCount());
  }

  // Make sure that making the container immutable doesn't incur any
  // copies of the tokens.
  ImmutableC immutable_c(&c);
  EXPECT_TRUE(c.empty());
  ASSERT_EQ(token_count, immutable_c.Get().size());
  int i = 0;
  for (typename C::const_iterator it = c.begin(); it != c.end(); ++it) {
    EXPECT_EQ(tokens[i].GetToken(), it->GetToken());
    EXPECT_EQ(copy_count, it->GetCopyCount());
    ++i;
  }
}

TEST_F(ImmutableTest, Vector) {
  RunTokenContainerTest<std::vector<Token>, Immutable<std::vector<Token> > >(
      "Vector");
}

TEST_F(ImmutableTest, VectorSwapMemFnByRef) {
  RunTokenContainerTest<
    std::vector<Token>,
    Immutable<std::vector<Token>, HasSwapMemFnByRef<std::vector<Token> > > >(
        "VectorSwapMemFnByRef");
}

TEST_F(ImmutableTest, Deque) {
  RunTokenContainerTest<std::deque<Token>, Immutable<std::deque<Token> > >(
      "Deque");
}

TEST_F(ImmutableTest, List) {
  RunTokenContainerTest<std::list<Token>, Immutable<std::list<Token> > >(
      "List");
}

TEST_F(ImmutableTest, Set) {
  RunTokenContainerTest<std::set<Token>, Immutable<std::set<Token> > >(
      "Set");
}

}  // namespace
}  // namespace browser_sync
