// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "chrome/browser/importer/safari_importer.h"

#include <map>
#include <vector>

#include "base/file_util.h"
#include "base/mac/mac_util.h"
#include "base/memory/scoped_nsobject.h"
#include "base/string16.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/importer/importer_bridge.h"
#include "chrome/browser/importer/importer_util.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "net/base/data_url.h"
#include "sql/statement.h"

namespace {

// A function like this is used by other importers in order to filter out
// URLS we don't want to import.
// For now it's pretty basic, but I've split it out so it's easy to slot
// in necessary logic for filtering URLS, should we need it.
bool CanImportSafariURL(const GURL& url) {
  // The URL is not valid.
  if (!url.is_valid())
    return false;

  return true;
}

}  // namespace

SafariImporter::SafariImporter(const FilePath& library_dir)
    : library_dir_(library_dir) {
}

SafariImporter::~SafariImporter() {
}

// static
bool SafariImporter::CanImport(const FilePath& library_dir,
                               uint16* services_supported) {
  DCHECK(services_supported);
  *services_supported = importer::NONE;

  // Import features are toggled by the following:
  // bookmarks import: existence of ~/Library/Safari/Bookmarks.plist file.
  // history import: existence of ~/Library/Safari/History.plist file.
  FilePath safari_dir = library_dir.Append("Safari");
  FilePath bookmarks_path = safari_dir.Append("Bookmarks.plist");
  FilePath history_path = safari_dir.Append("History.plist");

  if (file_util::PathExists(bookmarks_path))
    *services_supported |= importer::FAVORITES;
  if (file_util::PathExists(history_path))
    *services_supported |= importer::HISTORY;

  return *services_supported != importer::NONE;
}

void SafariImporter::StartImport(const importer::SourceProfile& source_profile,
                                 uint16 items,
                                 ImporterBridge* bridge) {
  bridge_ = bridge;
  // The order here is important!
  bridge_->NotifyStarted();

  // In keeping with import on other platforms (and for other browsers), we
  // don't import the home page (since it may lead to a useless homepage); see
  // crbug.com/25603.
  if ((items & importer::HISTORY) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::HISTORY);
    ImportHistory();
    bridge_->NotifyItemEnded(importer::HISTORY);
  }
  if ((items & importer::FAVORITES) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::FAVORITES);
    ImportBookmarks();
    bridge_->NotifyItemEnded(importer::FAVORITES);
  }
  if ((items & importer::PASSWORDS) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::PASSWORDS);
    ImportPasswords();
    bridge_->NotifyItemEnded(importer::PASSWORDS);
  }
  bridge_->NotifyEnded();
}

void SafariImporter::ImportBookmarks() {
  string16 toolbar_name =
      bridge_->GetLocalizedString(IDS_BOOKMARK_BAR_FOLDER_NAME);
  std::vector<ProfileWriter::BookmarkEntry> bookmarks;
  ParseBookmarks(toolbar_name, &bookmarks);

  // Write bookmarks into profile.
  if (!bookmarks.empty() && !cancelled()) {
    const string16& first_folder_name =
        bridge_->GetLocalizedString(IDS_BOOKMARK_GROUP_FROM_SAFARI);
    bridge_->AddBookmarks(bookmarks, first_folder_name);
  }

  // Import favicons.
  sql::Connection db;
  if (!OpenDatabase(&db))
    return;

  FaviconMap favicon_map;
  ImportFaviconURLs(&db, &favicon_map);
  // Write favicons into profile.
  if (!favicon_map.empty() && !cancelled()) {
    std::vector<history::ImportedFaviconUsage> favicons;
    LoadFaviconData(&db, favicon_map, &favicons);
    bridge_->SetFavicons(favicons);
  }
}

bool SafariImporter::OpenDatabase(sql::Connection* db) {
  // Construct ~/Library/Safari/WebIcons.db path.
  NSString* library_dir = [NSString
      stringWithUTF8String:library_dir_.value().c_str()];
  NSString* safari_dir = [library_dir
      stringByAppendingPathComponent:@"Safari"];
  NSString* favicons_db_path = [safari_dir
      stringByAppendingPathComponent:@"WebpageIcons.db"];

  const char* db_path = [favicons_db_path fileSystemRepresentation];
  return db->Open(FilePath(db_path));
}

void SafariImporter::ImportFaviconURLs(sql::Connection* db,
                                       FaviconMap* favicon_map) {
  const char* query = "SELECT iconID, url FROM PageURL;";
  sql::Statement s(db->GetUniqueStatement(query));
  if (!s)
    return;

  while (s.Step() && !cancelled()) {
    int64 icon_id = s.ColumnInt64(0);
    GURL url = GURL(s.ColumnString(1));
    (*favicon_map)[icon_id].insert(url);
  }
}

void SafariImporter::LoadFaviconData(
    sql::Connection* db,
    const FaviconMap& favicon_map,
    std::vector<history::ImportedFaviconUsage>* favicons) {
  const char* query = "SELECT i.url, d.data "
                      "FROM IconInfo i JOIN IconData d "
                      "ON i.iconID = d.iconID "
                      "WHERE i.iconID = ?;";
  sql::Statement s(db->GetUniqueStatement(query));
  if (!s)
    return;

  for (FaviconMap::const_iterator i = favicon_map.begin();
       i != favicon_map.end(); ++i) {
    s.Reset();
    s.BindInt64(0, i->first);
    if (s.Step()) {
      history::ImportedFaviconUsage usage;

      usage.favicon_url = GURL(s.ColumnString(0));
      if (!usage.favicon_url.is_valid())
        continue;  // Don't bother importing favicons with invalid URLs.

      std::vector<unsigned char> data;
      s.ColumnBlobAsVector(1, &data);
      if (data.empty())
        continue;  // Data definitely invalid.

      if (!importer::ReencodeFavicon(&data[0], data.size(), &usage.png_data))
        continue;  // Unable to decode.

      usage.urls = i->second;
      favicons->push_back(usage);
    }
  }
}

void SafariImporter::RecursiveReadBookmarksFolder(
    NSDictionary* bookmark_folder,
    const std::vector<string16>& parent_path_elements,
    bool is_in_toolbar,
    const string16& toolbar_name,
    std::vector<ProfileWriter::BookmarkEntry>* out_bookmarks) {
  DCHECK(bookmark_folder);

  NSString* type = [bookmark_folder objectForKey:@"WebBookmarkType"];
  NSString* title = [bookmark_folder objectForKey:@"Title"];

  // Are we the dictionary that contains all other bookmarks?
  // We need to know this so we don't add it to the path.
  bool is_top_level_bookmarks_container = [bookmark_folder
      objectForKey:@"WebBookmarkFileVersion"] != nil;

  // We're expecting a list of bookmarks here, if that isn't what we got, fail.
  if (!is_top_level_bookmarks_container) {
    // Top level containers sometimes don't have title attributes.
    if (![type isEqualToString:@"WebBookmarkTypeList"] || !title) {
      NOTREACHED() << "Type=("
                   << (type ? base::SysNSStringToUTF8(type) : "Null type")
                   << ") Title=("
                   << (title ? base::SysNSStringToUTF8(title) : "Null title")
                   << ")";
      return;
    }
  }

  NSArray* elements = [bookmark_folder objectForKey:@"Children"];
  if (!elements && (!parent_path_elements.empty() || !is_in_toolbar)) {
    // This is an empty folder, so add it explicitly; but don't add the toolbar
    // folder if it is empty.  Note that all non-empty folders are added
    // implicitly when their children are added.
    ProfileWriter::BookmarkEntry entry;
    // Safari doesn't specify a creation time for the folder.
    entry.creation_time = base::Time::Now();
    entry.title = base::SysNSStringToUTF16(title);
    entry.path = parent_path_elements;
    entry.in_toolbar = is_in_toolbar;
    entry.is_folder = true;

    out_bookmarks->push_back(entry);
    return;
  }

  std::vector<string16> path_elements(parent_path_elements);
  // Create a folder for the toolbar, but not for the bookmarks menu.
  if (path_elements.empty() && [title isEqualToString:@"BookmarksBar"]) {
    is_in_toolbar = true;
    path_elements.push_back(toolbar_name);
  } else if (!is_top_level_bookmarks_container &&
             !(path_elements.empty() &&
               [title isEqualToString:@"BookmarksMenu"])) {
    if (title)
      path_elements.push_back(base::SysNSStringToUTF16(title));
  }

  // Iterate over individual bookmarks.
  for (NSDictionary* bookmark in elements) {
    NSString* type = [bookmark objectForKey:@"WebBookmarkType"];
    if (!type)
      continue;

    // If this is a folder, recurse.
    if ([type isEqualToString:@"WebBookmarkTypeList"]) {
      RecursiveReadBookmarksFolder(bookmark,
                                   path_elements,
                                   is_in_toolbar,
                                   toolbar_name,
                                   out_bookmarks);
    }

    // If we didn't see a bookmark folder, then we're expecting a bookmark
    // item.  If that's not what we got then ignore it.
    if (![type isEqualToString:@"WebBookmarkTypeLeaf"])
      continue;

    NSString* url = [bookmark objectForKey:@"URLString"];
    NSString* title = [[bookmark objectForKey:@"URIDictionary"]
        objectForKey:@"title"];

    if (!url || !title)
      continue;

    // Output Bookmark.
    ProfileWriter::BookmarkEntry entry;
    // Safari doesn't specify a creation time for the bookmark.
    entry.creation_time = base::Time::Now();
    entry.title = base::SysNSStringToUTF16(title);
    entry.url = GURL(base::SysNSStringToUTF8(url));
    entry.path = path_elements;
    entry.in_toolbar = is_in_toolbar;

    out_bookmarks->push_back(entry);
  }
}

void SafariImporter::ParseBookmarks(
    const string16& toolbar_name,
    std::vector<ProfileWriter::BookmarkEntry>* bookmarks) {
  DCHECK(bookmarks);

  // Construct ~/Library/Safari/Bookmarks.plist path
  NSString* library_dir = [NSString
      stringWithUTF8String:library_dir_.value().c_str()];
  NSString* safari_dir = [library_dir
      stringByAppendingPathComponent:@"Safari"];
  NSString* bookmarks_plist = [safari_dir
    stringByAppendingPathComponent:@"Bookmarks.plist"];

  // Load the plist file.
  NSDictionary* bookmarks_dict = [NSDictionary
      dictionaryWithContentsOfFile:bookmarks_plist];
  if (!bookmarks_dict)
    return;

  // Recursively read in bookmarks.
  std::vector<string16> parent_path_elements;
  RecursiveReadBookmarksFolder(bookmarks_dict, parent_path_elements, false,
                               toolbar_name, bookmarks);
}

void SafariImporter::ImportPasswords() {
  // Safari stores it's passwords in the Keychain, same as us so we don't need
  // to import them.
  // Note: that we don't automatically pick them up, there is some logic around
  // the user needing to explicitly input his username in a page and blurring
  // the field before we pick it up, but the details of that are beyond the
  // scope of this comment.
}

void SafariImporter::ImportHistory() {
  std::vector<history::URLRow> rows;
  ParseHistoryItems(&rows);

  if (!rows.empty() && !cancelled()) {
    bridge_->SetHistoryItems(rows, history::SOURCE_SAFARI_IMPORTED);
  }
}

double SafariImporter::HistoryTimeToEpochTime(NSString* history_time) {
  DCHECK(history_time);
  // Add Difference between Unix epoch and CFAbsoluteTime epoch in seconds.
  // Unix epoch is 1970-01-01 00:00:00.0 UTC,
  // CF epoch is 2001-01-01 00:00:00.0 UTC.
  return CFStringGetDoubleValue(base::mac::NSToCFCast(history_time)) +
      kCFAbsoluteTimeIntervalSince1970;
}

void SafariImporter::ParseHistoryItems(
    std::vector<history::URLRow>* history_items) {
  DCHECK(history_items);

  // Construct ~/Library/Safari/History.plist path
  NSString* library_dir = [NSString
      stringWithUTF8String:library_dir_.value().c_str()];
  NSString* safari_dir = [library_dir
      stringByAppendingPathComponent:@"Safari"];
  NSString* history_plist = [safari_dir
      stringByAppendingPathComponent:@"History.plist"];

  // Load the plist file.
  NSDictionary* history_dict = [NSDictionary
      dictionaryWithContentsOfFile:history_plist];
  if (!history_dict)
    return;

  NSArray* safari_history_items = [history_dict
      objectForKey:@"WebHistoryDates"];

  for (NSDictionary* history_item in safari_history_items) {
    NSString* url_ns = [history_item objectForKey:@""];
    if (!url_ns)
      continue;

    GURL url(base::SysNSStringToUTF8(url_ns));

    if (!CanImportSafariURL(url))
      continue;

    history::URLRow row(url);
    NSString* title_ns = [history_item objectForKey:@"title"];

    // Sometimes items don't have a title, in which case we just substitue
    // the url.
    if (!title_ns)
      title_ns = url_ns;

    row.set_title(base::SysNSStringToUTF16(title_ns));
    int visit_count = [[history_item objectForKey:@"visitCount"]
                          intValue];
    row.set_visit_count(visit_count);
    // Include imported URLs in autocompletion - don't hide them.
    row.set_hidden(0);
    // Item was never typed before in the omnibox.
    row.set_typed_count(0);

    NSString* last_visit_str = [history_item objectForKey:@"lastVisitedDate"];
    // The last visit time should always be in the history item, but if not
    /// just continue without this item.
    DCHECK(last_visit_str);
    if (!last_visit_str)
      continue;

    // Convert Safari's last visit time to Unix Epoch time.
    double seconds_since_unix_epoch = HistoryTimeToEpochTime(last_visit_str);
    row.set_last_visit(base::Time::FromDoubleT(seconds_since_unix_epoch));

    history_items->push_back(row);
  }
}
