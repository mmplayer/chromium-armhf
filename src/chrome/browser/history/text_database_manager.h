// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_TEXT_DATABASE_MANAGER_H_
#define CHROME_BROWSER_HISTORY_TEXT_DATABASE_MANAGER_H_
#pragma once

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "base/task.h"
#include "base/memory/mru_cache.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/text_database.h"
#include "chrome/browser/history/query_parser.h"
#include "chrome/browser/history/url_database.h"

namespace history {

class HistoryPublisher;
class VisitDatabase;

// Manages a set of text databases representing different time periods. This
// will page them in and out as necessary, and will manage queries for times
// spanning multiple databases.
//
// It will also keep a list of partial changes, such as page adds and title and
// body sets, all of which come in at different times for a given page. When
// all data is received or enough time has elapsed since adding, the indexed
// data will be committed.
//
// This allows us to minimize inserts and modifications, which are slow for the
// full text database, since each page's information is added exactly once.
//
// Note: be careful to delete the relevant entries from this uncommitted list
// when clearing history or this information may get added to the database soon
// after the clear.
class TextDatabaseManager {
 public:
  // Tracks a set of changes (only deletes need to be supported now) to the
  // databases. This is opaque to the caller, but allows it to pass back a list
  // of all database that it has caused a change to.
  //
  // This is necessary for the feature where we optimize full text databases
  // which have changed as a result of the user deleting history via
  // OptimizeChangedDatabases. We want to do each affected database only once at
  // the end of the delete, but we don't want the caller to have to worry about
  // our internals.
  class ChangeSet {
   public:
    ChangeSet();
    ~ChangeSet();

   private:
    friend class TextDatabaseManager;

    typedef std::set<TextDatabase::DBIdent> DBSet;

    void Add(TextDatabase::DBIdent id) { changed_databases_.insert(id); }

    DBSet changed_databases_;
  };

  // You must call Init() to complete initialization.
  //
  // |dir| is the directory that will hold the full text database files (there
  // will be many files named by their date ranges).
  //
  // The visit database is a pointer owned by the caller for the main database
  // (of recent visits). The visit database will be updated to refer to the
  // added text database entries.
  TextDatabaseManager(const FilePath& dir,
                      URLDatabase* url_database,
                      VisitDatabase* visit_database);
  ~TextDatabaseManager();

  // Must call before using other functions. If it returns false, no other
  // functions should be called.
  bool Init(const HistoryPublisher* history_publisher);

  // Returns the directory that holds the full text database files.
  const FilePath& GetDir() { return dir_; }

  // Allows scoping updates. This also allows things to go faster since every
  // page add doesn't need to be committed to disk (slow). Note that files will
  // still get created during a transaction.
  void BeginTransaction();
  void CommitTransaction();

  // Sets specific information for the given page to be added to the database.
  // In normal operation, URLs will be added as the user visits them, the titles
  // and bodies will come in some time after that. These changes will be
  // automatically coalesced and added to the database some time in the future
  // using AddPageData().
  //
  // AddPageURL must be called for a given URL (+ its corresponding ID) before
  // either the title or body set. The visit ID specifies the visit that will
  // get updated to refer to the full text indexed information. The visit time
  // should be the time corresponding to that visit in the database.
  void AddPageURL(const GURL& url, URLID url_id, VisitID visit_id,
                  base::Time visit_time);
  void AddPageTitle(const GURL& url, const string16& title);
  void AddPageContents(const GURL& url, const string16& body);

  // Adds the given data to the appropriate database file, returning true on
  // success. The visit database row identified by |visit_id| will be updated
  // to refer to the full text index entry. If the visit ID is 0, the visit
  // database will not be updated.
  bool AddPageData(const GURL& url,
                   URLID url_id,
                   VisitID visit_id,
                   base::Time visit_time,
                   const string16& title,
                   const string16& body);

  // Deletes the instance of indexed data identified by the given time and URL.
  // Any changes will be tracked in the optional change set for use when calling
  // OptimizeChangedDatabases later. change_set can be NULL.
  void DeletePageData(base::Time time, const GURL& url,
                      ChangeSet* change_set);

  // The text database manager keeps a list of changes that are made to the
  // file AddPageURL/Title/Body that may not be committed to the database yet.
  // This function removes entries from this list happening between the given
  // time range. It is called when the user clears their history for a time
  // range, and we don't want any of our data to "leak." If restrict_urls is
  // not empty, only changes on those URLs are deleted.
  //
  // Either or both times my be is_null to be unbounded in that direction. When
  // non-null, the range is [begin, end).
  void DeleteFromUncommitted(const std::set<GURL>& restrict_urls,
                             base::Time begin, base::Time end);

  // Deletes all full text search data by removing the files from the disk.
  // This must be called OUTSIDE of a transaction since it actually deletes the
  // files rather than messing with the database.
  void DeleteAll();

  // Calls optimize on all the databases identified in a given change set (see
  // the definition of ChangeSet above for more). Optimizing means that old data
  // will be removed rather than marked unused.
  void OptimizeChangedDatabases(const ChangeSet& change_set);

  // Executes the given query. See QueryOptions for more info on input.
  //
  // The results are filled into |results|, and the first time considered for
  // the output is in |first_time_searched| (see QueryResults for more).
  //
  // This function will return more than one match per URL if there is more than
  // one entry for that URL in the database.
  void GetTextMatches(const string16& query,
                      const QueryOptions& options,
                      std::vector<TextDatabase::Match>* results,
                      base::Time* first_time_searched);

 private:
  // These tests call ExpireRecentChangesForTime to force expiration.
  FRIEND_TEST_ALL_PREFIXES(TextDatabaseManagerTest, InsertPartial);
  FRIEND_TEST_ALL_PREFIXES(TextDatabaseManagerTest, PartialComplete);
  FRIEND_TEST_ALL_PREFIXES(ExpireHistoryTest, DeleteURLAndFavicon);
  FRIEND_TEST_ALL_PREFIXES(ExpireHistoryTest, FlushRecentURLsUnstarred);
  FRIEND_TEST_ALL_PREFIXES(ExpireHistoryTest,
                           FlushRecentURLsUnstarredRestricted);

  // Stores "recent stuff" that has happened with the page, since the page
  // visit, title, and body all come in at different times.
  class PageInfo {
   public:
    PageInfo(URLID url_id, VisitID visit_id, base::Time visit_time);
    ~PageInfo();

    // Getters.
    URLID url_id() const { return url_id_; }
    VisitID visit_id() const { return visit_id_; }
    base::Time visit_time() const { return visit_time_; }
    const string16& title() const { return title_; }
    const string16& body() const { return body_; }

    // Setters, we can only update the title and body.
    void set_title(const string16& ttl);
    void set_body(const string16& bdy);

    // Returns true if both the title or body of the entry has been set. Since
    // both the title and body setters will "fix" empty strings to be a space,
    // these indicate if the setter was ever called.
    bool has_title() const { return !title_.empty(); }
    bool has_body() { return !body_.empty(); }

    // Returns true if this entry was added too long ago and we should give up
    // waiting for more data. The current time is passed in as an argument so we
    // can check many without re-querying the timer.
    bool Expired(base::TimeTicks now) const;

   private:
    URLID url_id_;
    VisitID visit_id_;

    // Time of the visit of the URL. This will be the value stored in the URL
    // and visit tables for the entry.
    base::Time visit_time_;

    // When this page entry was created. We have a cap on the maximum time that
    // an entry will be in the queue before being flushed to the database.
    base::TimeTicks added_time_;

    // Will be the string " " when they are set to distinguish set and unset.
    string16 title_;
    string16 body_;
  };

  // Converts the given time to a database identifier or vice-versa.
  static TextDatabase::DBIdent TimeToID(base::Time time);
  static base::Time IDToTime(TextDatabase::DBIdent id);

  // Returns a text database for the given identifier or time. This file will
  // be created if it doesn't exist and |for_writing| is set. On error,
  // including the case where the file doesn't exist and |for_writing|
  // is false, it will return NULL.
  //
  // When |for_writing| is set, a transaction on the database will be opened
  // if there is a transaction open on this manager.
  //
  // The pointer will be tracked in the cache. The caller should not store it
  // or delete it since it will get automatically deleted as necessary.
  TextDatabase* GetDB(TextDatabase::DBIdent id, bool for_writing);
  TextDatabase* GetDBForTime(base::Time time, bool for_writing);

  // Populates the present_databases_ list based on which files are on disk.
  // When the list is already initialized, this will do nothing, so you can
  // call it whenever you want to ensure the present_databases_ set is filled.
  void InitDBList();

  // Schedules a call to ExpireRecentChanges in the future.
  void ScheduleFlushOldChanges();

  // Checks the recent_changes_ list and commits partial data that has been
  // around too long.
  void FlushOldChanges();

  // Given "now," this will expire old things from the recent_changes_ list.
  // This is used as the backend for FlushOldChanges and is called directly
  // by the unit tests with fake times.
  void FlushOldChangesForTime(base::TimeTicks now);

  // Directory holding our index files.
  const FilePath dir_;

  // Non-owning pointers to the recent history databases for URLs and visits.
  URLDatabase* url_database_;
  VisitDatabase* visit_database_;

  // Lists recent additions that we have not yet filled out with the title and
  // body. Sorted by time, we will flush them when they are complete or have
  // been in the queue too long without modification.
  //
  // We kind of abuse the MRUCache because we never move things around in it
  // using Get. Instead, we keep them in the order they were inserted, since
  // this is the metric we use to measure age. The MRUCache gives us an ordered
  // list with fast lookup by URL.
  typedef base::MRUCache<GURL, PageInfo> RecentChangeList;
  RecentChangeList recent_changes_;

  // Nesting levels of transactions. Since sqlite only allows one open
  // transaction, we simulate nested transactions by mapping the outermost one
  // to a real transaction. Since this object never needs to do ROLLBACK, losing
  // the ability for all transactions to rollback is inconsequential.
  int transaction_nesting_;

  // The cache owns the TextDatabase pointers, they will be automagically
  // deleted when the cache entry is removed or expired.
  typedef base::OwningMRUCache<TextDatabase::DBIdent, TextDatabase*> DBCache;
  DBCache db_cache_;

  // Tells us about the existence of database files on disk. All existing
  // databases will be in here, and non-existent ones will not, so we don't
  // have to check the disk every time.
  //
  // This set is populated LAZILY by InitDBList(), you should call that function
  // before accessing the list.
  //
  // Note that iterators will work on the keys in-order. Normally, reverse
  // iterators will be used to iterate the keys in reverse-order.
  typedef std::set<TextDatabase::DBIdent> DBIdentSet;
  DBIdentSet present_databases_;
  bool present_databases_loaded_;  // Set by InitDBList when populated.

  // Lists all databases with open transactions. These will have to be closed
  // when the transaction is committed.
  DBIdentSet open_transactions_;

  QueryParser query_parser_;

  // Generates tasks for our periodic checking of expired "recent changes".
  base::WeakPtrFactory<TextDatabaseManager> weak_factory_;

  // This object is created and managed by the history backend. We maintain an
  // opaque pointer to the object for our use.
  // This can be NULL if there are no indexers registered to receive indexing
  // data from us.
  const HistoryPublisher* history_publisher_;

  DISALLOW_COPY_AND_ASSIGN(TextDatabaseManager);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_TEXT_DATABASE_MANAGER_H_
