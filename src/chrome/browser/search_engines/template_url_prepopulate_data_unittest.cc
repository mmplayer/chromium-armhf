// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/memory/scoped_vector.h"
#include "base/scoped_temp_dir.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/search_engines/search_engine_type.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test TemplateURLPrepopulateDataTest;

const int kCountryIds[] = {
    'A'<<8|'D', 'A'<<8|'E', 'A'<<8|'F', 'A'<<8|'G', 'A'<<8|'I',
    'A'<<8|'L', 'A'<<8|'M', 'A'<<8|'N', 'A'<<8|'O', 'A'<<8|'Q',
    'A'<<8|'R', 'A'<<8|'S', 'A'<<8|'T', 'A'<<8|'U', 'A'<<8|'W',
    'A'<<8|'X', 'A'<<8|'Z', 'B'<<8|'A', 'B'<<8|'B', 'B'<<8|'D',
    'B'<<8|'E', 'B'<<8|'F', 'B'<<8|'G', 'B'<<8|'H', 'B'<<8|'I',
    'B'<<8|'J', 'B'<<8|'M', 'B'<<8|'N', 'B'<<8|'O', 'B'<<8|'R',
    'B'<<8|'S', 'B'<<8|'T', 'B'<<8|'V', 'B'<<8|'W', 'B'<<8|'Y',
    'B'<<8|'Z', 'C'<<8|'A', 'C'<<8|'C', 'C'<<8|'D', 'C'<<8|'F',
    'C'<<8|'G', 'C'<<8|'H', 'C'<<8|'I', 'C'<<8|'K', 'C'<<8|'L',
    'C'<<8|'M', 'C'<<8|'N', 'C'<<8|'O', 'C'<<8|'R', 'C'<<8|'U',
    'C'<<8|'V', 'C'<<8|'X', 'C'<<8|'Y', 'C'<<8|'Z', 'D'<<8|'E',
    'D'<<8|'J', 'D'<<8|'K', 'D'<<8|'M', 'D'<<8|'O', 'D'<<8|'Z',
    'E'<<8|'C', 'E'<<8|'E', 'E'<<8|'G', 'E'<<8|'R', 'E'<<8|'S',
    'E'<<8|'T', 'F'<<8|'I', 'F'<<8|'J', 'F'<<8|'K', 'F'<<8|'M',
    'F'<<8|'O', 'F'<<8|'R', 'G'<<8|'A', 'G'<<8|'B', 'G'<<8|'D',
    'G'<<8|'E', 'G'<<8|'F', 'G'<<8|'G', 'G'<<8|'H', 'G'<<8|'I',
    'G'<<8|'L', 'G'<<8|'M', 'G'<<8|'N', 'G'<<8|'P', 'G'<<8|'Q',
    'G'<<8|'R', 'G'<<8|'S', 'G'<<8|'T', 'G'<<8|'U', 'G'<<8|'W',
    'G'<<8|'Y', 'H'<<8|'K', 'H'<<8|'M', 'H'<<8|'N', 'H'<<8|'R',
    'H'<<8|'T', 'H'<<8|'U', 'I'<<8|'D', 'I'<<8|'E', 'I'<<8|'L',
    'I'<<8|'M', 'I'<<8|'N', 'I'<<8|'O', 'I'<<8|'P', 'I'<<8|'Q',
    'I'<<8|'R', 'I'<<8|'S', 'I'<<8|'T', 'J'<<8|'E', 'J'<<8|'M',
    'J'<<8|'O', 'J'<<8|'P', 'K'<<8|'E', 'K'<<8|'G', 'K'<<8|'H',
    'K'<<8|'I', 'K'<<8|'M', 'K'<<8|'N', 'K'<<8|'P', 'K'<<8|'R',
    'K'<<8|'W', 'K'<<8|'Y', 'K'<<8|'Z', 'L'<<8|'A', 'L'<<8|'B',
    'L'<<8|'C', 'L'<<8|'I', 'L'<<8|'K', 'L'<<8|'R', 'L'<<8|'S',
    'L'<<8|'T', 'L'<<8|'U', 'L'<<8|'V', 'L'<<8|'Y', 'M'<<8|'A',
    'M'<<8|'C', 'M'<<8|'D', 'M'<<8|'E', 'M'<<8|'G', 'M'<<8|'H',
    'M'<<8|'K', 'M'<<8|'L', 'M'<<8|'M', 'M'<<8|'N', 'M'<<8|'O',
    'M'<<8|'P', 'M'<<8|'Q', 'M'<<8|'R', 'M'<<8|'S', 'M'<<8|'T',
    'M'<<8|'U', 'M'<<8|'V', 'M'<<8|'W', 'M'<<8|'X', 'M'<<8|'Y',
    'M'<<8|'Z', 'N'<<8|'A', 'N'<<8|'C', 'N'<<8|'E', 'N'<<8|'F',
    'N'<<8|'G', 'N'<<8|'I', 'N'<<8|'L', 'N'<<8|'O', 'N'<<8|'P',
    'N'<<8|'R', 'N'<<8|'U', 'N'<<8|'Z', 'O'<<8|'M', 'P'<<8|'A',
    'P'<<8|'E', 'P'<<8|'F', 'P'<<8|'G', 'P'<<8|'H', 'P'<<8|'K',
    'P'<<8|'L', 'P'<<8|'M', 'P'<<8|'N', 'P'<<8|'R', 'P'<<8|'S',
    'P'<<8|'T', 'P'<<8|'W', 'P'<<8|'Y', 'Q'<<8|'A', 'R'<<8|'E',
    'R'<<8|'O', 'R'<<8|'S', 'R'<<8|'U', 'R'<<8|'W', 'S'<<8|'A',
    'S'<<8|'B', 'S'<<8|'C', 'S'<<8|'D', 'S'<<8|'E', 'S'<<8|'G',
    'S'<<8|'H', 'S'<<8|'I', 'S'<<8|'J', 'S'<<8|'K', 'S'<<8|'L',
    'S'<<8|'M', 'S'<<8|'N', 'S'<<8|'O', 'S'<<8|'R', 'S'<<8|'T',
    'S'<<8|'V', 'S'<<8|'Y', 'S'<<8|'Z', 'T'<<8|'C', 'T'<<8|'D',
    'T'<<8|'F', 'T'<<8|'G', 'T'<<8|'H', 'T'<<8|'J', 'T'<<8|'K',
    'T'<<8|'L', 'T'<<8|'M', 'T'<<8|'N', 'T'<<8|'O', 'T'<<8|'R',
    'T'<<8|'T', 'T'<<8|'V', 'T'<<8|'W', 'T'<<8|'Z', 'U'<<8|'A',
    'U'<<8|'G', 'U'<<8|'M', 'U'<<8|'S', 'U'<<8|'Y', 'U'<<8|'Z',
    'V'<<8|'A', 'V'<<8|'C', 'V'<<8|'E', 'V'<<8|'G', 'V'<<8|'I',
    'V'<<8|'N', 'V'<<8|'U', 'W'<<8|'F', 'W'<<8|'S', 'Y'<<8|'E',
    'Y'<<8|'T', 'Z'<<8|'A', 'Z'<<8|'M', 'Z'<<8|'W', -1 };

// Verifies the set of prepopulate data doesn't contain entries with duplicate
// ids.
TEST_F(TemplateURLPrepopulateDataTest, UniqueIDs) {
  TestingProfile profile;
  for (size_t i = 0; i < arraysize(kCountryIds); ++i) {
    profile.GetPrefs()->SetInteger(prefs::kCountryIDAtInstall, kCountryIds[i]);
    ScopedVector<TemplateURL> urls;
    size_t default_index;
    TemplateURLPrepopulateData::GetPrepopulatedEngines(
        profile.GetPrefs(), &(urls.get()), &default_index);
    std::set<int> unique_ids;
    for (size_t turl_i = 0; turl_i < urls.size(); ++turl_i) {
      ASSERT_TRUE(unique_ids.find(urls[turl_i]->prepopulate_id()) ==
                  unique_ids.end());
      unique_ids.insert(urls[turl_i]->prepopulate_id());
    }
  }
}

// Verifies that default search providers from the preferences file
// override the built-in ones.
TEST_F(TemplateURLPrepopulateDataTest, ProvidersFromPrefs) {
  TestingPrefService prefs;
  TemplateURLPrepopulateData::RegisterUserPrefs(&prefs);
  prefs.SetUserPref(prefs::kSearchProviderOverridesVersion,
                    Value::CreateIntegerValue(1));
  ListValue* overrides = new ListValue;
  DictionaryValue* entry = new DictionaryValue;
  entry->SetString("name", "foo");
  entry->SetString("keyword", "fook");
  entry->SetString("search_url", "http://foo.com/s?q={searchTerms}");
  entry->SetString("favicon_url", "http://foi.com/favicon.ico");
  entry->SetString("suggest_url", "");
  entry->SetString("instant_url", "");
  entry->SetString("encoding", "UTF-8");
  entry->SetInteger("search_engine_type", 1);
  entry->SetInteger("logo_id", 0);
  entry->SetInteger("id", 1001);
  overrides->Append(entry);
  prefs.SetUserPref(prefs::kSearchProviderOverrides, overrides);

  int version = TemplateURLPrepopulateData::GetDataVersion(&prefs);
  EXPECT_EQ(1, version);

  ScopedVector<TemplateURL> t_urls;
  size_t default_index;
  TemplateURLPrepopulateData::GetPrepopulatedEngines(
      &prefs, &(t_urls.get()), &default_index);

  ASSERT_EQ(1u, t_urls.size());
  EXPECT_EQ(ASCIIToUTF16("foo"), t_urls[0]->short_name());
  EXPECT_EQ(ASCIIToUTF16("fook"), t_urls[0]->keyword());
  EXPECT_EQ("foo.com", t_urls[0]->url()->GetHost());
  EXPECT_EQ("foi.com", t_urls[0]->GetFaviconURL().host());
  EXPECT_EQ(1u, t_urls[0]->input_encodings().size());
  EXPECT_EQ(1001, t_urls[0]->prepopulate_id());
  EXPECT_EQ(SEARCH_ENGINE_GOOGLE, t_urls[0]->search_engine_type());
  EXPECT_EQ(0, t_urls[0]->logo_id());
}

TEST_F(TemplateURLPrepopulateDataTest, SearchEngineFromOrigin) {
  UIThreadSearchTermsData search_terms_data;
  std::set<GURL> unique_engines;

  { // Scoping for the profile.
    TestingProfile profile;
    for (size_t i = 0; i < arraysize(kCountryIds); ++i) {
      profile.GetPrefs()->SetInteger(prefs::kCountryIDAtInstall,
                                     kCountryIds[i]);
      ScopedVector<TemplateURL> urls;
      size_t default_index;
      TemplateURLPrepopulateData::GetPrepopulatedEngines(
          profile.GetPrefs(), &(urls.get()), &default_index);
      for (size_t turl_i = 0; turl_i < urls.size(); ++turl_i) {
        GURL engine_url(urls[turl_i]->url()->url());
        if (!engine_url.is_valid()) {
          engine_url = TemplateURLService::GenerateSearchURLUsingTermsData(
              urls[turl_i], search_terms_data);
        }
        GURL origin = engine_url.GetOrigin();
        unique_engines.insert(origin);
      }
    }
  }

  TestingProfile profile;
  for (std::set<GURL>::iterator it = unique_engines.begin();
       it != unique_engines.end(); ++it) {
    scoped_ptr<TemplateURL> found_url(
        TemplateURLPrepopulateData::GetEngineForOrigin(profile.GetPrefs(),
                                                       *it));
    EXPECT_EQ(
        TemplateURLService::GenerateSearchURLUsingTermsData(
            found_url.get(), search_terms_data).GetOrigin(),
        it->GetOrigin());
  }

  GURL not_a_search_engine("http://example.com/");
  EXPECT_EQ(NULL, TemplateURLPrepopulateData::GetEngineForOrigin(
      profile.GetPrefs(),
      not_a_search_engine));
}

TEST_F(TemplateURLPrepopulateDataTest, GetSearchEngineLogo) {
  GURL bad_engine("http://example.com/");
  EXPECT_EQ(kNoSearchEngineLogo,
            TemplateURLPrepopulateData::GetSearchEngineLogo(bad_engine));
  GURL engine_with_logo("http://www.ask.com/");
  EXPECT_EQ(IDR_SEARCH_ENGINE_LOGO_ASK,
            TemplateURLPrepopulateData::GetSearchEngineLogo(engine_with_logo));
  GURL engine_no_logo("http://araby.com/");
  EXPECT_EQ(kNoSearchEngineLogo,
            TemplateURLPrepopulateData::GetSearchEngineLogo(engine_no_logo));

}
