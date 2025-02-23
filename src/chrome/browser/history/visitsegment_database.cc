// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/visitsegment_database.h"

#include <math.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/page_usage_data.h"
#include "sql/statement.h"

// The following tables are used to store url segment information.
//
// segments
//   id                 Primary key
//   name               A unique string to represent that segment. (URL derived)
//   url_id             ID of the url currently used to represent this segment.
//   pres_index         index used to store a fixed presentation position.
//
// segment_usage
//   id                 Primary key
//   segment_id         Corresponding segment id
//   time_slot          time stamp identifying for what day this entry is about
//   visit_count        Number of visit in the segment
//

namespace history {

VisitSegmentDatabase::VisitSegmentDatabase() {
}

VisitSegmentDatabase::~VisitSegmentDatabase() {
}

bool VisitSegmentDatabase::InitSegmentTables() {
  // Segments table.
  if (!GetDB().DoesTableExist("segments")) {
    if (!GetDB().Execute("CREATE TABLE segments ("
        "id INTEGER PRIMARY KEY,"
        "name VARCHAR,"
        "url_id INTEGER NON NULL,"
        "pres_index INTEGER DEFAULT -1 NOT NULL)")) {
      NOTREACHED();
      return false;
    }

    if (!GetDB().Execute("CREATE INDEX segments_name ON segments(name)")) {
      NOTREACHED();
      return false;
    }
  }

  // This was added later, so we need to try to create it even if the table
  // already exists.
  GetDB().Execute("CREATE INDEX segments_url_id ON segments(url_id)");

  // Segment usage table.
  if (!GetDB().DoesTableExist("segment_usage")) {
    if (!GetDB().Execute("CREATE TABLE segment_usage ("
        "id INTEGER PRIMARY KEY,"
        "segment_id INTEGER NOT NULL,"
        "time_slot INTEGER NOT NULL,"
        "visit_count INTEGER DEFAULT 0 NOT NULL)")) {
      NOTREACHED();
      return false;
    }
    if (!GetDB().Execute(
        "CREATE INDEX segment_usage_time_slot_segment_id ON "
        "segment_usage(time_slot, segment_id)")) {
      NOTREACHED();
      return false;
    }
  }

  // Added in a later version, so we always need to try to creat this index.
  GetDB().Execute("CREATE INDEX segments_usage_seg_id "
                  "ON segment_usage(segment_id)");

  // Presentation index table.
  //
  // Important note:
  // Right now, this table is only used to store the presentation index.
  // If you need to add more columns, keep in mind that rows are currently
  // deleted when the presentation index is changed to -1.
  // See SetPagePresentationIndex() in this file
  if (!GetDB().DoesTableExist("presentation")) {
    if (!GetDB().Execute("CREATE TABLE presentation("
        "url_id INTEGER PRIMARY KEY,"
        "pres_index INTEGER NOT NULL)"))
      return false;
  }
  return true;
}

bool VisitSegmentDatabase::DropSegmentTables() {
  // Dropping the tables will implicitly delete the indices.
  return GetDB().Execute("DROP TABLE segments") &&
         GetDB().Execute("DROP TABLE segment_usage");
}

// Note: the segment name is derived from the URL but is not a URL. It is
// a string that can be easily recreated from various URLS. Maybe this should
// be an MD5 to limit the length.
//
// static
std::string VisitSegmentDatabase::ComputeSegmentName(const GURL& url) {
  // TODO(brettw) this should probably use the registry controlled
  // domains service.
  GURL::Replacements r;
  const char kWWWDot[] = "www.";
  const int kWWWDotLen = arraysize(kWWWDot) - 1;

  std::string host = url.host();
  const char* host_c = host.c_str();
  // Remove www. to avoid some dups.
  if (static_cast<int>(host.size()) > kWWWDotLen &&
      LowerCaseEqualsASCII(host_c, host_c + kWWWDotLen, kWWWDot)) {
    r.SetHost(host.c_str(),
              url_parse::Component(kWWWDotLen,
                  static_cast<int>(host.size()) - kWWWDotLen));
  }
  // Remove other stuff we don't want.
  r.ClearUsername();
  r.ClearPassword();
  r.ClearQuery();
  r.ClearRef();
  r.ClearPort();

  return url.ReplaceComponents(r).spec();
}

SegmentID VisitSegmentDatabase::GetSegmentNamed(
    const std::string& segment_name) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT id FROM segments WHERE name = ?"));
  if (!statement)
    return 0;

  statement.BindString(0, segment_name);
  if (statement.Step())
    return statement.ColumnInt64(0);
  return 0;
}

bool VisitSegmentDatabase::UpdateSegmentRepresentationURL(SegmentID segment_id,
                                                          URLID url_id) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE segments SET url_id = ? WHERE id = ?"));
  if (!statement)
    return false;

  statement.BindInt64(0, url_id);
  statement.BindInt64(1, segment_id);
  return statement.Run();
}

URLID VisitSegmentDatabase::GetSegmentRepresentationURL(SegmentID segment_id) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT url_id FROM segments WHERE id = ?"));
  if (!statement)
    return 0;

  statement.BindInt64(0, segment_id);
  if (statement.Step())
    return statement.ColumnInt64(0);
  return 0;
}

SegmentID VisitSegmentDatabase::CreateSegment(URLID url_id,
                                              const std::string& segment_name) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO segments (name, url_id) VALUES (?,?)"));
  if (!statement)
    return false;

  statement.BindString(0, segment_name);
  statement.BindInt64(1, url_id);
  if (statement.Run())
    return GetDB().GetLastInsertRowId();
  return false;
}

bool VisitSegmentDatabase::IncreaseSegmentVisitCount(SegmentID segment_id,
                                                     base::Time ts,
                                                     int amount) {
  base::Time t = ts.LocalMidnight();

  sql::Statement select(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT id, visit_count FROM segment_usage "
      "WHERE time_slot = ? AND segment_id = ?"));
  if (!select)
    return false;

  select.BindInt64(0, t.ToInternalValue());
  select.BindInt64(1, segment_id);
  if (select.Step()) {
    sql::Statement update(GetDB().GetCachedStatement(SQL_FROM_HERE,
        "UPDATE segment_usage SET visit_count = ? WHERE id = ?"));
    if (!update)
      return false;

    update.BindInt64(0, select.ColumnInt64(1) + static_cast<int64>(amount));
    update.BindInt64(1, select.ColumnInt64(0));
    return update.Run();

  } else {
    sql::Statement insert(GetDB().GetCachedStatement(SQL_FROM_HERE,
        "INSERT INTO segment_usage "
        "(segment_id, time_slot, visit_count) VALUES (?, ?, ?)"));
    if (!insert)
      return false;

    insert.BindInt64(0, segment_id);
    insert.BindInt64(1, t.ToInternalValue());
    insert.BindInt64(2, static_cast<int64>(amount));
    return insert.Run();
  }
}

void VisitSegmentDatabase::QuerySegmentUsage(
    base::Time from_time,
    int max_result_count,
    std::vector<PageUsageData*>* results) {
  // This function gathers the highest-ranked segments in two queries.
  // The first gathers scores for all segments.
  // The second gathers segment data (url, title, etc.) for the highest-ranked
  // segments.
  // TODO(evanm): this disregards the "presentation index", which was what was
  // used to lock results into position.  But the rest of our code currently
  // does as well.

  // Gather all the segment scores.
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT segment_id, time_slot, visit_count "
      "FROM segment_usage WHERE time_slot >= ? "
      "ORDER BY segment_id"));
  if (!statement) {
    NOTREACHED() << GetDB().GetErrorMessage();
    return;
  }

  base::Time ts = from_time.LocalMidnight();
  statement.BindInt64(0, ts.ToInternalValue());

  base::Time now = base::Time::Now();
  SegmentID last_segment_id = 0;
  PageUsageData* pud = NULL;
  float score = 0;
  while (statement.Step()) {
    SegmentID segment_id = statement.ColumnInt64(0);
    if (segment_id != last_segment_id) {
      if (pud) {
        pud->SetScore(score);
        results->push_back(pud);
      }

      pud = new PageUsageData(segment_id);
      score = 0;
      last_segment_id = segment_id;
    }

    base::Time timeslot =
        base::Time::FromInternalValue(statement.ColumnInt64(1));
    int visit_count = statement.ColumnInt(2);
    int days_ago = (now - timeslot).InDays();

    // Score for this day in isolation.
    float day_visits_score = 1.0f + log(static_cast<float>(visit_count));
    // Recent visits count more than historical ones, so we multiply in a boost
    // related to how long ago this day was.
    // This boost is a curve that smoothly goes through these values:
    // Today gets 3x, a week ago 2x, three weeks ago 1.5x, falling off to 1x
    // at the limit of how far we reach into the past.
    float recency_boost = 1.0f + (2.0f * (1.0f / (1.0f + days_ago/7.0f)));
    score += recency_boost * day_visits_score;
  }

  if (pud) {
    pud->SetScore(score);
    results->push_back(pud);
  }

  // Limit to the top kResultCount results.
  sort(results->begin(), results->end(), PageUsageData::Predicate);
  if (static_cast<int>(results->size()) > max_result_count) {
    STLDeleteContainerPointers(results->begin() + max_result_count,
                               results->end());
    results->resize(max_result_count);
  }

  // Now fetch the details about the entries we care about.
  sql::Statement statement2(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT urls.url, urls.title FROM urls "
      "JOIN segments ON segments.url_id = urls.id "
      "WHERE segments.id = ?"));
  if (!statement2) {
    NOTREACHED() << GetDB().GetErrorMessage();
    return;
  }
  for (size_t i = 0; i < results->size(); ++i) {
    PageUsageData* pud = (*results)[i];
    statement2.BindInt64(0, pud->GetID());
    if (statement2.Step()) {
      pud->SetURL(GURL(statement2.ColumnString(0)));
      pud->SetTitle(UTF8ToUTF16(statement2.ColumnString(1)));
    }
    statement2.Reset();
  }
}

void VisitSegmentDatabase::DeleteSegmentData(base::Time older_than) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM segment_usage WHERE time_slot < ?"));
  if (!statement)
    return;

  statement.BindInt64(0, older_than.LocalMidnight().ToInternalValue());
  if (!statement.Run())
    NOTREACHED();
}

void VisitSegmentDatabase::SetSegmentPresentationIndex(SegmentID segment_id,
                                                       int index) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE segments SET pres_index = ? WHERE id = ?"));
  if (!statement)
    return;

  statement.BindInt(0, index);
  statement.BindInt64(1, segment_id);
  if (!statement.Run())
    NOTREACHED();
  else
    DCHECK_EQ(1, GetDB().GetLastChangeCount());
}

bool VisitSegmentDatabase::DeleteSegmentForURL(URLID url_id) {
  sql::Statement select(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT id FROM segments WHERE url_id = ?"));
  if (!select)
    return false;

  sql::Statement delete_seg(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM segments WHERE id = ?"));
  if (!delete_seg)
    return false;

  sql::Statement delete_usage(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM segment_usage WHERE segment_id = ?"));
  if (!delete_usage)
    return false;

  bool r = true;
  select.BindInt64(0, url_id);
  // In theory there could not be more than one segment using that URL but we
  // loop anyway to cleanup any inconsistency.
  while (select.Step()) {
    SegmentID segment_id = select.ColumnInt64(0);

    delete_usage.BindInt64(0, segment_id);
    if (!delete_usage.Run()) {
      NOTREACHED();
      r = false;
    }

    delete_seg.BindInt64(0, segment_id);
    if (!delete_seg.Run()) {
      NOTREACHED();
      r = false;
    }
    delete_usage.Reset();
    delete_seg.Reset();
  }
  return r;
}

}  // namespace history
