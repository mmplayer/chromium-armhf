// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_H_
#pragma once

#include <set>
#include <vector>

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task.h"
#include "chrome/browser/safe_browsing/safe_browsing_store.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace base {
  class Time;
}

namespace safe_browsing {
class PrefixSet;
}

class BloomFilter;
class GURL;
class MessageLoop;
class SafeBrowsingDatabase;

// Factory for creating SafeBrowsingDatabase. Tests implement this factory
// to create fake Databases for testing.
class SafeBrowsingDatabaseFactory {
 public:
  SafeBrowsingDatabaseFactory() { }
  virtual ~SafeBrowsingDatabaseFactory() { }
  virtual SafeBrowsingDatabase* CreateSafeBrowsingDatabase(
      bool enable_download_protection,
      bool enable_client_side_whitelist,
      bool enable_download_whitelist) = 0;
 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingDatabaseFactory);
};


// Encapsulates on-disk databases that for safebrowsing. There are
// four databases: browse, download, download whitelist and
// client-side detection (csd) whitelist databases. The browse database contains
// information about phishing and malware urls. The download database contains
// URLs for bad binaries (e.g: those containing virus) and hash of
// these downloaded contents. The download whitelist contains whitelisted
// download hosting sites as well as whitelisted binary signing certificates
// etc.  The csd whitelist database contains URLs that will never be considered
// as phishing by the client-side phishing detection. These on-disk databases
// are shared among all profiles, as it doesn't contain user-specific data. This
// object is not thread-safe, i.e. all its methods should be used on the same
// thread that it was created on.
class SafeBrowsingDatabase {
 public:
  // Factory method for obtaining a SafeBrowsingDatabase implementation.
  // It is not thread safe.
  // |enable_download_protection| is used to control the download database
  // feature.
  // |enable_client_side_whitelist| is used to control the csd whitelist
  // database feature.
  // |enable_download_whitelist| is used to control the download whitelist
  // database feature.
  static SafeBrowsingDatabase* Create(bool enable_download_protection,
                                      bool enable_client_side_whitelist,
                                      bool enable_download_whitelist);

  // Makes the passed |factory| the factory used to instantiate
  // a SafeBrowsingDatabase. This is used for tests.
  static void RegisterFactory(SafeBrowsingDatabaseFactory* factory) {
    factory_ = factory;
  }

  virtual ~SafeBrowsingDatabase();

  // Initializes the database with the given filename.
  virtual void Init(const FilePath& filename) = 0;

  // Deletes the current database and creates a new one.
  virtual bool ResetDatabase() = 0;

  // Returns false if |url| is not in the browse database.  If it
  // returns true, then either |matching_list| is the name of the matching
  // list, or |prefix_hits| and |full_hits| contains the matching hash
  // prefixes.  This function is safe to call from threads other than
  // the creation thread.
  virtual bool ContainsBrowseUrl(const GURL& url,
                                 std::string* matching_list,
                                 std::vector<SBPrefix>* prefix_hits,
                                 std::vector<SBFullHashResult>* full_hits,
                                 base::Time last_update) = 0;

  // Returns false if none of |urls| are in Download database. If it returns
  // true, |prefix_hits| should contain the prefixes for the URLs that were in
  // the database.  This function could ONLY be accessed from creation thread.
  virtual bool ContainsDownloadUrl(const std::vector<GURL>& urls,
                                   std::vector<SBPrefix>* prefix_hits) = 0;

  // Returns false if |prefix| is not in Download database.
  // This function could ONLY be accessed from creation thread.
  virtual bool ContainsDownloadHashPrefix(const SBPrefix& prefix) = 0;

  // Returns false if |url| is not on the client-side phishing detection
  // whitelist.  Otherwise, this function returns true.  Note: the whitelist
  // only contains full-length hashes so we don't return any prefix hit.
  // This function should only be called from the IO thread.
  virtual bool ContainsCsdWhitelistedUrl(const GURL& url) = 0;

  // The download whitelist is used for two purposes: a white-domain list of
  // sites that are considered to host only harmless binaries as well as a
  // whitelist of arbitrary strings such as hashed certificate authorities that
  // are considered to be trusted.  The two methods below let you lookup
  // the whitelist either for a URL or an arbitrary string.  These methods will
  // return false if no match is found and true otherwise.
  // This function could ONLY be accessed from the IO thread.
  virtual bool ContainsDownloadWhitelistedUrl(const GURL& url) = 0;
  virtual bool ContainsDownloadWhitelistedString(const std::string& str) = 0;

  // A database transaction should look like:
  //
  // std::vector<SBListChunkRanges> lists;
  // if (db.UpdateStarted(&lists)) {
  //   // Do something with |lists|.
  //
  //   // Process add/sub commands.
  //   db.InsertChunks(list_name, chunks);
  //
  //   // Process adddel/subdel commands.
  //   db.DeleteChunks(chunks_deletes);
  //
  //   // If passed true, processes the collected chunk info and
  //   // rebuilds the bloom filter.  If passed false, rolls everything
  //   // back.
  //   db.UpdateFinished(success);
  // }
  //
  // If UpdateStarted() returns true, the caller MUST eventually call
  // UpdateFinished().  If it returns false, the caller MUST NOT call
  // the other functions.
  virtual bool UpdateStarted(std::vector<SBListChunkRanges>* lists) = 0;
  virtual void InsertChunks(const std::string& list_name,
                            const SBChunkList& chunks) = 0;
  virtual void DeleteChunks(
      const std::vector<SBChunkDelete>& chunk_deletes) = 0;
  virtual void UpdateFinished(bool update_succeeded) = 0;

  // Store the results of a GetHash response. In the case of empty results, we
  // cache the prefixes until the next update so that we don't have to issue
  // further GetHash requests we know will be empty.
  virtual void CacheHashResults(
      const std::vector<SBPrefix>& prefixes,
      const std::vector<SBFullHashResult>& full_hits) = 0;

  // The name of the bloom-filter file for the given database file.
  static FilePath BloomFilterForFilename(const FilePath& db_filename);

  // Filename for malware and phishing URL database.
  static FilePath BrowseDBFilename(const FilePath& db_base_filename);

  // Filename for download URL and download binary hash database.
  static FilePath DownloadDBFilename(const FilePath& db_base_filename);

  // Filename for client-side phishing detection whitelist databsae.
  static FilePath CsdWhitelistDBFilename(
      const FilePath& csd_whitelist_base_filename);

  // Filename for download whitelist databsae.
  static FilePath DownloadWhitelistDBFilename(
      const FilePath& download_whitelist_base_filename);

  // Enumerate failures for histogramming purposes.  DO NOT CHANGE THE
  // ORDERING OF THESE VALUES.
  enum FailureType {
    FAILURE_DATABASE_CORRUPT,
    FAILURE_DATABASE_CORRUPT_HANDLER,
    FAILURE_BROWSE_DATABASE_UPDATE_BEGIN,
    FAILURE_BROWSE_DATABASE_UPDATE_FINISH,
    FAILURE_DATABASE_FILTER_MISSING,
    FAILURE_DATABASE_FILTER_READ,
    FAILURE_DATABASE_FILTER_WRITE,
    FAILURE_DATABASE_FILTER_DELETE,
    FAILURE_DATABASE_STORE_MISSING,
    FAILURE_DATABASE_STORE_DELETE,
    FAILURE_DOWNLOAD_DATABASE_UPDATE_BEGIN,
    FAILURE_DOWNLOAD_DATABASE_UPDATE_FINISH,
    FAILURE_WHITELIST_DATABASE_UPDATE_BEGIN,
    FAILURE_WHITELIST_DATABASE_UPDATE_FINISH,
    // Memory space for histograms is determined by the max.  ALWAYS
    // ADD NEW VALUES BEFORE THIS ONE.
    FAILURE_DATABASE_MAX
  };

  static void RecordFailure(FailureType failure_type);

 private:
  // The factory used to instantiate a SafeBrowsingDatabase object.
  // Useful for tests, so they can provide their own implementation of
  // SafeBrowsingDatabase.
  static SafeBrowsingDatabaseFactory* factory_;
};

class SafeBrowsingDatabaseNew : public SafeBrowsingDatabase {
 public:
  // Create a database with a browse, download, download whitelist and
  // csd whitelist store objects. Takes ownership of all the store objects.
  // When |download_store| is NULL, the database will ignore any operations
  // related download (url hashes and binary hashes).  The same is true for
  // the |csd_whitelist_store| and |download_whitelist_store|.
  SafeBrowsingDatabaseNew(SafeBrowsingStore* browse_store,
                          SafeBrowsingStore* download_store,
                          SafeBrowsingStore* csd_whitelist_store,
                          SafeBrowsingStore* download_whitelist_store);

  // Create a database with a browse store. This is a legacy interface that
  // useds Sqlite.
  SafeBrowsingDatabaseNew();

  virtual ~SafeBrowsingDatabaseNew();

  // Implement SafeBrowsingDatabase interface.
  virtual void Init(const FilePath& filename);
  virtual bool ResetDatabase();
  virtual bool ContainsBrowseUrl(const GURL& url,
                                 std::string* matching_list,
                                 std::vector<SBPrefix>* prefix_hits,
                                 std::vector<SBFullHashResult>* full_hits,
                                 base::Time last_update);
  virtual bool ContainsDownloadUrl(const std::vector<GURL>& urls,
                                   std::vector<SBPrefix>* prefix_hits);
  virtual bool ContainsDownloadHashPrefix(const SBPrefix& prefix);
  virtual bool ContainsCsdWhitelistedUrl(const GURL& url);
  virtual bool ContainsDownloadWhitelistedUrl(const GURL& url);
  virtual bool ContainsDownloadWhitelistedString(const std::string& str);
  virtual bool UpdateStarted(std::vector<SBListChunkRanges>* lists);
  virtual void InsertChunks(const std::string& list_name,
                            const SBChunkList& chunks);
  virtual void DeleteChunks(const std::vector<SBChunkDelete>& chunk_deletes);
  virtual void UpdateFinished(bool update_succeeded);
  virtual void CacheHashResults(const std::vector<SBPrefix>& prefixes,
                                const std::vector<SBFullHashResult>& full_hits);

 private:
  friend class SafeBrowsingDatabaseTest;
  FRIEND_TEST(SafeBrowsingDatabaseTest, HashCaching);

  // A SafeBrowsing whitelist contains a list of whitelisted full-hashes (stored
  // in a sorted vector) as well as a boolean flag indicating whether all
  // lookups in the whitelist should be considered matches for safety.
  typedef std::pair<std::vector<SBFullHash>, bool> SBWhitelist;

  // Returns true if the whitelist is disabled or if any of the given hashes
  // matches the whitelist.
  bool ContainsWhitelistedHashes(const SBWhitelist& whitelist,
                                 const std::vector<SBFullHash>& hashes);

  // Return the browse_store_, download_store_, download_whitelist_store or
  // csd_whitelist_store_ based on list_id.
  SafeBrowsingStore* GetStore(int list_id);

    // Deletes the files on disk.
  bool Delete();

  // Load the bloom filter off disk, or generates one if it doesn't exist.
  void LoadBloomFilter();

  // Writes the current bloom filter to disk.
  void WriteBloomFilter();

  // Loads the given full-length hashes to the given whitelist.  If the number
  // of hashes is too large or if the kill switch URL is on the whitelist
  // we will whitelist everything.
  void LoadWhitelist(const std::vector<SBAddFullHash>& full_hashes,
                     SBWhitelist* whitelist);

  // Call this method if an error occured with the given whitelist.  This will
  // result in all lookups to the whitelist to return true.
  void WhitelistEverything(SBWhitelist* whitelist);

  // Helpers for handling database corruption.
  // |OnHandleCorruptDatabase()| runs |ResetDatabase()| and sets
  // |corruption_detected_|, |HandleCorruptDatabase()| posts
  // |OnHandleCorruptDatabase()| to the current thread, to be run
  // after the current task completes.
  // TODO(shess): Wire things up to entirely abort the update
  // transaction when this happens.
  void HandleCorruptDatabase();
  void OnHandleCorruptDatabase();

  // Helpers for InsertChunks().
  void InsertAdd(int chunk, SBPrefix host, const SBEntry* entry, int list_id);
  void InsertAddChunks(int list_id, const SBChunkList& chunks);
  void InsertSub(int chunk, SBPrefix host, const SBEntry* entry, int list_id);
  void InsertSubChunks(int list_id, const SBChunkList& chunks);

  void UpdateDownloadStore();
  void UpdateBrowseStore();
  void UpdateWhitelistStore(const FilePath& store_filename,
                            SafeBrowsingStore* store,
                            SBWhitelist* whitelist);

  // Helper function to compare addprefixes in download_store_ with |prefixes|.
  // The |list_bit| indicates which list (download url or download hash)
  // to compare.
  // Returns true if there is a match, |*prefix_hits| will contain the actual
  // matching prefixes.
  bool MatchDownloadAddPrefixes(int list_bit,
                                const std::vector<SBPrefix>& prefixes,
                                std::vector<SBPrefix>* prefix_hits);

  // Used to verify that various calls are made from the thread the
  // object was created on.
  MessageLoop* creation_loop_;

  // Lock for protecting access to variables that may be used on the
  // IO thread.  This includes |browse_bloom_filter_|, |full_browse_hashes_|,
  // |pending_browse_hashes_|, |prefix_miss_cache_|, |csd_whitelist_|, and
  // |csd_whitelist_all_urls_|.
  base::Lock lookup_lock_;

  // Underlying persistent store for chunk data.
  // For browsing related (phishing and malware URLs) chunks and prefixes.
  FilePath browse_filename_;
  scoped_ptr<SafeBrowsingStore> browse_store_;

  // For download related (download URL and binary hash) chunks and prefixes.
  FilePath download_filename_;
  scoped_ptr<SafeBrowsingStore> download_store_;

  // For the client-side phishing detection whitelist chunks and full-length
  // hashes.  This list only contains 256 bit hashes.
  FilePath csd_whitelist_filename_;
  scoped_ptr<SafeBrowsingStore> csd_whitelist_store_;

  // For the download whitelist chunks and full-length hashes.  This list only
  // contains 256 bit hashes.
  FilePath download_whitelist_filename_;
  scoped_ptr<SafeBrowsingStore> download_whitelist_store_;

  SBWhitelist csd_whitelist_;
  SBWhitelist download_whitelist_;

  // Bloom filter generated from the add-prefixes in |browse_store_|.
  // Only browse_store_ requires the BloomFilter for fast query.
  FilePath bloom_filter_filename_;
  scoped_refptr<BloomFilter> browse_bloom_filter_;

  // Cached browse store related full-hash items, ordered by prefix for
  // efficient scanning.
  // |full_browse_hashes_| are items from |browse_store_|,
  // |pending_browse_hashes_| are items from |CacheHashResults()|, which
  // will be pushed to the store on the next update.
  std::vector<SBAddFullHash> full_browse_hashes_;
  std::vector<SBAddFullHash> pending_browse_hashes_;

  // Cache of prefixes that returned empty results (no full hash
  // match) to |CacheHashResults()|.  Cached to prevent asking for
  // them every time.  Cleared on next update.
  std::set<SBPrefix> prefix_miss_cache_;

  // Used to schedule resetting the database because of corruption.
  base::WeakPtrFactory<SafeBrowsingDatabaseNew> reset_factory_;

  // Set if corruption is detected during the course of an update.
  // Causes the update functions to fail with no side effects, until
  // the next call to |UpdateStarted()|.
  bool corruption_detected_;

  // Set to true if any chunks are added or deleted during an update.
  // Used to optimize away database update.
  bool change_detected_;

  // Used to check if a prefix was in the database.
  scoped_ptr<safe_browsing::PrefixSet> prefix_set_;
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_H_
