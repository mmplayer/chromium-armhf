// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_H_
#pragma once

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/time.h"
#include "chrome/browser/search_engines/search_engine_type.h"
#include "chrome/browser/search_engines/template_url_id.h"
#include "googleurl/src/gurl.h"

class PrefService;
class Profile;
class SearchTermsData;
class TemplateURL;
class WebDataService;
struct WDKeywordsResult;

// TemplateURL represents the relevant portions of the Open Search Description
// Document (http://www.opensearch.org/Specifications/OpenSearch).
// The main use case for TemplateURL is to use the TemplateURLRef returned by
// suggestions_url or url for keyword/suggestion expansion:
// . suggestions_url describes a URL that is ideal for as you type suggestions.
//   The returned results are in the mime type application/x-suggestions+json.
// . url describes a URL that may be used as a shortcut. Returned results are
//   are text/html.
// Before using either one, make sure it's non-NULL, and if you intend to use
// it to replace search terms, make sure SupportsReplacement returns true.
// To use either URL invoke the ReplaceSearchTerms method on the corresponding
// TemplateURLRef.
//
// For files parsed from the Web, be sure and invoke IsValid. IsValid returns
// true if the URL could be parsed.
//
// Both TemplateURL and TemplateURLRef have value semantics. This allows the
// UI to create a copy while the user modifies the values.
class TemplateURLRef {
 public:
  // Magic numbers to pass to ReplaceSearchTerms() for the |accepted_suggestion|
  // parameter.  Most callers aren't using Suggest capabilities and should just
  // pass NO_SUGGESTIONS_AVAILABLE.
  // NOTE: Because positive values are meaningful, make sure these are negative!
  enum AcceptedSuggestion {
    NO_SUGGESTION_CHOSEN = -1,
    NO_SUGGESTIONS_AVAILABLE = -2,
  };

  TemplateURLRef();

  TemplateURLRef(const std::string& url, int index_offset, int page_offset);

  ~TemplateURLRef();

  // Returns true if this URL supports replacement.
  bool SupportsReplacement() const;

  // Like SupportsReplacement but usable on threads other than the UI thread.
  bool SupportsReplacementUsingTermsData(
      const SearchTermsData& search_terms_data) const;

  // Returns a string that is the result of replacing the search terms in
  // the url with the specified value.
  //
  // If this TemplateURLRef does not support replacement (SupportsReplacement
  // returns false), an empty string is returned.
  //
  // The TemplateURL is used to determine the input encoding for the term.
  std::string ReplaceSearchTerms(
      const TemplateURL& host,
      const string16& terms,
      int accepted_suggestion,
      const string16& original_query_for_suggestion) const;

  // Just like ReplaceSearchTerms except that it takes a Profile that's used to
  // retrieve Instant field trial params. Most callers don't care about those
  // params, and so can use ReplaceSearchTerms instead.
  std::string ReplaceSearchTermsUsingProfile(
      Profile* profile,
      const TemplateURL& host,
      const string16& terms,
      int accepted_suggestion,
      const string16& original_query_for_suggestion) const;

  // Just like ReplaceSearchTerms except that it takes SearchTermsData to supply
  // the data for some search terms. Most of the time ReplaceSearchTerms should
  // be called.
  std::string ReplaceSearchTermsUsingTermsData(
      const TemplateURL& host,
      const string16& terms,
      int accepted_suggestion,
      const string16& original_query_for_suggestion,
      const SearchTermsData& search_terms_data) const;

  // Returns the raw URL. None of the parameters will have been replaced.
  const std::string& url() const { return url_; }

  // Returns the index number of the first search result.
  int index_offset() const { return index_offset_; }

  // Returns the page number of the first search results.
  int page_offset() const { return page_offset_; }

  // Returns true if the TemplateURLRef is valid. An invalid TemplateURLRef is
  // one that contains unknown terms, or invalid characters.
  bool IsValid() const;

  // Like IsValid but usable on threads other than the UI thread.
  bool IsValidUsingTermsData(const SearchTermsData& search_terms_data) const;

  // Returns a string representation of this TemplateURLRef suitable for
  // display. The display format is the same as the format used by Firefox.
  string16 DisplayURL() const;

  // Converts a string as returned by DisplayURL back into a string as
  // understood by TemplateURLRef.
  static std::string DisplayURLToURLRef(const string16& display_url);

  // If this TemplateURLRef is valid and contains one search term, this returns
  // the host/path of the URL, otherwise this returns an empty string.
  const std::string& GetHost() const;
  const std::string& GetPath() const;

  // If this TemplateURLRef is valid and contains one search term, this returns
  // the key of the search term, otherwise this returns an empty string.
  const std::string& GetSearchTermKey() const;

  // Converts the specified term in the encoding of the host TemplateURL to a
  // string16.
  string16 SearchTermToString16(const TemplateURL& host,
                                const std::string& term) const;

  // Returns true if this TemplateURLRef has a replacement term of
  // {google:baseURL} or {google:baseSuggestURL}.
  bool HasGoogleBaseURLs() const;

  // Returns true if both refs are NULL or have the same values.
  static bool SameUrlRefs(const TemplateURLRef* ref1,
                          const TemplateURLRef* ref2);

  // Collects metrics whether searches through Google are sent with RLZ string.
  void CollectRLZMetrics() const;

  // Sets whether this URL is pre-populated or not.
  void set_prepopulated(bool prepopulated) { prepopulated_ = prepopulated; }

 private:
  friend class SearchHostToURLsMapTest;
  friend class TemplateURL;
  friend class TemplateURLServiceTestUtil;
  friend class TemplateURLTest;
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, SetPrepopulatedAndParse);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseParameterKnown);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseParameterUnknown);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseURLEmpty);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseURLNoTemplateEnd);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseURLNoKnownParameters);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseURLTwoParameters);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseURLNestedParameter);

  // Enumeration of the known types.
  enum ReplacementType {
    ENCODING,
    GOOGLE_ACCEPTED_SUGGESTION,
    GOOGLE_BASE_URL,
    GOOGLE_BASE_SUGGEST_URL,
    GOOGLE_INSTANT_FIELD_TRIAL_GROUP,
    GOOGLE_ORIGINAL_QUERY_FOR_SUGGESTION,
    GOOGLE_RLZ,
    GOOGLE_SEARCH_FIELDTRIAL_GROUP,
    GOOGLE_UNESCAPED_SEARCH_TERMS,
    LANGUAGE,
    SEARCH_TERMS,
  };

  // Used to identify an element of the raw url that can be replaced.
  struct Replacement {
    Replacement(ReplacementType type, size_t index)
        : type(type), index(index) {}
    ReplacementType type;
    size_t index;
  };

  // The list of elements to replace.
  typedef std::vector<struct Replacement> Replacements;

  // TemplateURLRef internally caches values to make replacement quick. This
  // method invalidates any cached values.
  void InvalidateCachedValues() const;

  // Resets the url.
  void Set(const std::string& url, int index_offset, int page_offset);

  // Parses the parameter in url at the specified offset. start/end specify the
  // range of the parameter in the url, including the braces. If the parameter
  // is valid, url is updated to reflect the appropriate parameter. If
  // the parameter is one of the known parameters an element is added to
  // replacements indicating the type and range of the element. The original
  // parameter is erased from the url.
  //
  // If the parameter is not a known parameter, false is returned. If this is a
  // prepopulated URL, the parameter is erased, otherwise it is left alone.
  bool ParseParameter(size_t start,
                      size_t end,
                      std::string* url,
                      Replacements* replacements) const;

  // Parses the specified url, replacing parameters as necessary. If
  // successful, valid is set to true, and the parsed url is returned. For all
  // known parameters that are encountered an entry is added to replacements.
  // If there is an error parsing the url, valid is set to false, and an empty
  // string is returned.
  std::string ParseURL(const std::string& url,
                       Replacements* replacements,
                       bool* valid) const;

  // If the url has not yet been parsed, ParseURL is invoked.
  // NOTE: While this is const, it modifies parsed_, valid_, parsed_url_ and
  // search_offset_.
  void ParseIfNecessary() const;

  // Like ParseIfNecessary but usable on threads other than the UI thread.
  void ParseIfNecessaryUsingTermsData(
      const SearchTermsData& search_terms_data) const;

  // Extracts the query key and host from the url.
  void ParseHostAndSearchTermKey(
      const SearchTermsData& search_terms_data) const;

  // Used by tests to set the value for the Google base url. This takes
  // ownership of the given std::string.
  static void SetGoogleBaseURL(std::string* google_base_url);

  // The raw URL. Where as this contains all the terms (such as {searchTerms}),
  // parsed_url_ has them all stripped out.
  std::string url_;

  // indexOffset defined for the Url element.
  int index_offset_;

  // searchOffset defined for the Url element.
  int page_offset_;

  // Whether the URL has been parsed.
  mutable bool parsed_;

  // Whether the url was successfully parsed.
  mutable bool valid_;

  // The parsed URL. All terms have been stripped out of this with
  // replacements_ giving the index of the terms to replace.
  mutable std::string parsed_url_;

  // Do we support replacement?
  mutable bool supports_replacements_;

  // The replaceable parts of url (parsed_url_). These are ordered by index
  // into the string, and may be empty.
  mutable Replacements replacements_;

  // Host, path and key of the search term. These are only set if the url
  // contains one search term.
  mutable std::string host_;
  mutable std::string path_;
  mutable std::string search_term_key_;

  // Whether the contained URL is a pre-populated URL.
  bool prepopulated_;
};

// Describes the relevant portions of a single OSD document.
class TemplateURL {
 public:
  // Describes a single image reference. Each TemplateURL may have
  // any number (including 0) of ImageRefs.
  //
  // If a TemplateURL has no images, the favicon for the generated URL
  // should be used.
  struct ImageRef {
    ImageRef(const std::string& type, int width, int height)
        : type(type), width(width), height(height) {
    }

    ImageRef(const std::string& type, int width, int height, const GURL& url)
      : type(type), width(width), height(height), url(url) {
    }

    // Mime type for the image.
    // ICO image will have the format: image/x-icon or image/vnd.microsoft.icon
    std::string type;

    // Size of the image
    int width;
    int height;

    // URL of the image.
    GURL url;
  };

  // Generates a favicon URL from the specified url.
  static GURL GenerateFaviconURL(const GURL& url);

  // Returns true if |turl| is non-null and has a search URL that supports
  // replacement.
  static bool SupportsReplacement(const TemplateURL* turl);

  // Like SupportsReplacement but usable on threads other than the UI thread.
  static bool SupportsReplacementUsingTermsData(
      const TemplateURL* turl,
      const SearchTermsData& search_terms_data);

  TemplateURL();
  ~TemplateURL();

  // A short description of the template. This is the name we show to the user
  // in various places that use keywords. For example, the location bar shows
  // this when the user selects the keyword.
  void set_short_name(const string16& short_name) {
    short_name_ = short_name;
  }
  string16 short_name() const { return short_name_; }

  // An accessor for the short_name, but adjusted so it can be appropriately
  // displayed even if it is LTR and the UI is RTL.
  string16 AdjustedShortNameForLocaleDirection() const;

  // A description of the template; this may be empty.
  void set_description(const string16& description) {
    description_ = description;
  }
  string16 description() const { return description_; }

  // URL providing JSON results. This is typically used to provide suggestions
  // as your type. If NULL, this url does not support suggestions.
  // Be sure and check the resulting TemplateURLRef for SupportsReplacement
  // before using.
  void SetSuggestionsURL(const std::string& suggestions_url,
                         int index_offset,
                         int page_offset);
  const TemplateURLRef* suggestions_url() const {
    return suggestions_url_.url().empty() ? NULL : &suggestions_url_;
  }

  // Parameterized URL for providing the results. This may be NULL.
  // Be sure and check the resulting TemplateURLRef for SupportsReplacement
  // before using.
  void SetURL(const std::string& url, int index_offset, int page_offset);
  // Returns the TemplateURLRef that may be used for search results. This
  // returns NULL if a url element was not specified.
  const TemplateURLRef* url() const {
    return url_.url().empty() ? NULL : &url_;
  }

  // Parameterized URL for instant results. This may be NULL.  Be sure and check
  // the resulting TemplateURLRef for SupportsReplacement before using. See
  // TemplateURLRef for a description of |index_offset| and |page_offset|.
  void SetInstantURL(const std::string& url, int index_offset, int page_offset);
  // Returns the TemplateURLRef that may be used for search results. This
  // returns NULL if a url element was not specified.
  const TemplateURLRef* instant_url() const {
    return instant_url_.url().empty() ? NULL : &instant_url_;
  }

  // URL to the OSD file this came from. May be empty.
  void set_originating_url(const GURL& url) {
    originating_url_ = url;
  }
  const GURL& originating_url() const { return originating_url_; }

  // The shortcut for this template url. May be empty.
  void set_keyword(const string16& keyword);
  string16 keyword() const;

  // Whether to autogenerate a keyword from the url() in GetKeyword().  Most
  // consumers should not need this.
  // NOTE: Calling set_keyword() turns this back off.  Manual and automatic
  // keywords are mutually exclusive.
  void set_autogenerate_keyword(bool autogenerate_keyword) {
    autogenerate_keyword_ = autogenerate_keyword;
    if (autogenerate_keyword_) {
      keyword_.clear();
      keyword_generated_ = false;
    }
  }
  bool autogenerate_keyword() const {
    return autogenerate_keyword_;
  }

  // Ensures that the keyword is generated.  Most consumers should not need this
  // because it is done automatically.  Use this method on the UI thread, so
  // the keyword may be accessed on another thread.
  void EnsureKeyword() const;

  // Whether this keyword is shown in the default list of search providers. This
  // is just a property and does not indicate whether this TemplateURL has
  // a TemplateURLRef that supports replacement. Use ShowInDefaultList to
  // test both.
  // The default value is false.
  void set_show_in_default_list(bool show_in_default_list) {
    show_in_default_list_ = show_in_default_list;
  }
  bool show_in_default_list() const { return show_in_default_list_; }

  // Returns true if show_in_default_list() is true and this TemplateURL has a
  // TemplateURLRef that supports replacement.
  bool ShowInDefaultList() const;

  // Whether it's safe for auto-modification code (the autogenerator and the
  // code that imports data from other browsers) to replace the TemplateURL.
  // This should be set to false for any keyword the user edits, or any keyword
  // that the user clearly manually edited in the past, like a bookmark keyword
  // from another browser.
  void set_safe_for_autoreplace(bool safe_for_autoreplace) {
    safe_for_autoreplace_ = safe_for_autoreplace;
  }
  bool safe_for_autoreplace() const { return safe_for_autoreplace_; }

  // Images for this URL. May be empty.
  void add_image_ref(const ImageRef& ref) { image_refs_.push_back(ref); }
  const std::vector<ImageRef>& image_refs() const { return image_refs_; }

  // Convenience methods for getting/setting an ImageRef that points to a
  // favicon. A TemplateURL need not have an ImageRef for a favicon. In such
  // a situation GetFaviconURL returns an invalid url.
  //
  // If url is empty and there is an image ref for a favicon, it is removed.
  void SetFaviconURL(const GURL& url);
  GURL GetFaviconURL() const;

  // Set of languages supported. This may be empty.
  void add_language(const string16& language) {
    languages_.push_back(language);
  }
  std::vector<string16> languages() const { return languages_; }

  // Date this keyword was created.
  //
  // NOTE: this may be 0, which indicates the keyword was created before we
  // started tracking creation time.
  void set_date_created(base::Time time) { date_created_ = time; }
  base::Time date_created() const { return date_created_; }

  // The last time this keyword was modified by a user, since creation.
  //
  // NOTE: Like date_created above, this may be 0.
  void set_last_modified(base::Time time) { last_modified_ = time; }
  base::Time last_modified() const { return last_modified_; }

  // True if this TemplateURL was automatically created by the administrator via
  // group policy.
  void set_created_by_policy(bool created_by_policy) {
     created_by_policy_ = created_by_policy;
  }
  bool created_by_policy() const { return created_by_policy_; }

  // Number of times this keyword has been explicitly used to load a URL.  We
  // don't increment this for uses as the "default search engine" since that's
  // not really "explicit" usage and incrementing would result in pinning the
  // user's default search engine(s) to the top of the list of searches on the
  // New Tab page, de-emphasizing the omnibox as "where you go to search".
  void set_usage_count(int count) { usage_count_ = count; }
  int usage_count() const { return usage_count_; }

  // The list of supported encodings for the search terms. This may be empty,
  // which indicates the terms should be encoded with UTF-8.
  void set_input_encodings(const std::vector<std::string>& encodings) {
    input_encodings_ = encodings;
  }
  void add_input_encoding(const std::string& encoding) {
    input_encodings_.push_back(encoding);
  }
  const std::vector<std::string>& input_encodings() const {
    return input_encodings_;
  }

  void set_search_engine_type(SearchEngineType search_engine_type) {
    search_engine_type_ = search_engine_type;
  }
  SearchEngineType search_engine_type() const {
    return search_engine_type_;
  }

  void set_logo_id(int logo_id) { logo_id_ = logo_id; }
  int logo_id() const { return logo_id_; }

  // Returns the unique identifier of this TemplateURL. The unique ID is set
  // by the TemplateURLService when the TemplateURL is added to it.
  TemplateURLID id() const { return id_; }

  // If this TemplateURL comes from prepopulated data the prepopulate_id is > 0.
  // SetPrepopulateId also sets any TemplateURLRef's prepopulated flag to true
  // if |id| > 0 and false otherwise.
  void SetPrepopulateId(int id);
  int prepopulate_id() const { return prepopulate_id_; }

  std::string GetExtensionId() const;
  bool IsExtensionKeyword() const;

  std::string sync_guid() const { return sync_guid_; }
  void set_sync_guid(const std::string& guid) { sync_guid_ = guid; }

 private:
  friend void MergeEnginesFromPrepopulateData(
      PrefService* prefs,
      WebDataService* service,
      std::vector<TemplateURL*>* template_urls,
      const TemplateURL** default_search_provider);
  friend class KeywordTable;
  friend class KeywordTableTest;
  friend class SearchHostToURLsMap;
  friend class TemplateURLService;

  // Invalidates cached values on this object and its child TemplateURLRefs.
  void InvalidateCachedValues() const;

  // Sets all TemplateURLRefs prepopulated flags.
  void SetTemplateURLRefsPrepopulated(bool prepopulated);

  // Unique identifier, used when archived to the database.
  void set_id(TemplateURLID id) { id_ = id; }

  string16 short_name_;
  string16 description_;
  TemplateURLRef suggestions_url_;
  TemplateURLRef url_;
  TemplateURLRef instant_url_;
  GURL originating_url_;
  mutable string16 keyword_;
  bool autogenerate_keyword_;  // If this is set, |keyword_| holds the cached
                               // generated keyword if available.
  mutable bool keyword_generated_;  // True if the keyword was generated. This
                                    // is used to avoid multiple attempts if
                                    // generating a keyword failed.
  bool show_in_default_list_;
  bool safe_for_autoreplace_;
  std::vector<ImageRef> image_refs_;
  std::vector<string16> languages_;
  // List of supported input encodings.
  std::vector<std::string> input_encodings_;
  TemplateURLID id_;
  base::Time date_created_;
  base::Time last_modified_;
  bool created_by_policy_;
  int usage_count_;
  SearchEngineType search_engine_type_;
  int logo_id_;
  int prepopulate_id_;
  // The primary unique identifier for Sync. This is only set on TemplateURLs
  // that have been associated with Sync.
  std::string sync_guid_;

  // TODO(sky): Add date last parsed OSD file.
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_H_
