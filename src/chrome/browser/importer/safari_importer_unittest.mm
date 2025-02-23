// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/safari_importer.h"

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/importer/importer_bridge.h"
#include "chrome/common/chrome_paths.h"
#include "sql/connection.h"
#include "testing/platform_test.h"

// In order to test the Safari import functionality effectively, we store a
// simulated Library directory containing dummy data files in the same
// structure as ~/Library in the Chrome test data directory.
// This function returns the path to that directory.
FilePath GetTestSafariLibraryPath() {
    FilePath test_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &test_dir);

    // Our simulated ~/Library directory
    test_dir = test_dir.AppendASCII("safari_import");
    return test_dir;
}

class SafariImporterTest : public PlatformTest {
 public:
  SafariImporter* GetSafariImporter() {
    FilePath test_library_dir = GetTestSafariLibraryPath();
    CHECK(file_util::PathExists(test_library_dir))  <<
        "Missing test data directory";

    return new SafariImporter(test_library_dir);
  }
};

TEST_F(SafariImporterTest, HistoryImport) {
  scoped_refptr<SafariImporter> importer(GetSafariImporter());

  std::vector<history::URLRow> history_items;
  importer->ParseHistoryItems(&history_items);

  // Should be 2 history items.
  ASSERT_EQ(history_items.size(), 2U);

  history::URLRow& it1 = history_items[0];
  EXPECT_EQ(it1.url(), GURL("http://www.firsthistoryitem.com/"));
  EXPECT_EQ(it1.title(), UTF8ToUTF16("First History Item Title"));
  EXPECT_EQ(it1.visit_count(), 1);
  EXPECT_EQ(it1.hidden(), 0);
  EXPECT_EQ(it1.typed_count(), 0);
  EXPECT_EQ(it1.last_visit().ToDoubleT(),
      importer->HistoryTimeToEpochTime(@"270598264.4"));

  history::URLRow& it2 = history_items[1];
  std::string second_item_title("http://www.secondhistoryitem.com/");
  EXPECT_EQ(it2.url(), GURL(second_item_title));
  // The second item lacks a title so we expect the URL to be substituted.
  EXPECT_EQ(UTF16ToUTF8(it2.title()), second_item_title.c_str());
  EXPECT_EQ(it2.visit_count(), 55);
  EXPECT_EQ(it2.hidden(), 0);
  EXPECT_EQ(it2.typed_count(), 0);
  EXPECT_EQ(it2.last_visit().ToDoubleT(),
      importer->HistoryTimeToEpochTime(@"270598231.4"));
}

TEST_F(SafariImporterTest, BookmarkImport) {
  // Expected results
  const struct {
    bool in_toolbar;
    GURL url;
    // We store the path with levels of nesting delimited by forward slashes.
    string16 path;
    string16 title;
  } kImportedBookmarksData[] = {
    {
      true,
      GURL("http://www.apple.com/"),
      ASCIIToUTF16("Toolbar/"),
      ASCIIToUTF16("Apple")
    },
    {
      true,
      GURL("http://www.yahoo.com/"),
      ASCIIToUTF16("Toolbar/"),
      ASCIIToUTF16("Yahoo!")
    },
    {
      true,
      GURL("http://www.cnn.com/"),
      ASCIIToUTF16("Toolbar/News"),
      ASCIIToUTF16("CNN")
    },
    {
      true,
      GURL("http://www.nytimes.com/"),
      ASCIIToUTF16("Toolbar/News"),
      ASCIIToUTF16("The New York Times")
    },
    {
      false,
      GURL("http://www.reddit.com/"),
      ASCIIToUTF16(""),
      ASCIIToUTF16("reddit.com: what's new online!")
    },
    {
      false,
      GURL(),
      ASCIIToUTF16(""),
      ASCIIToUTF16("Empty Folder")
    },
    {
      false,
      GURL("http://www.webkit.org/blog/"),
      ASCIIToUTF16(""),
      ASCIIToUTF16("Surfin' Safari - The WebKit Blog")
    },
  };

  scoped_refptr<SafariImporter> importer(GetSafariImporter());
  std::vector<ProfileWriter::BookmarkEntry> bookmarks;
  importer->ParseBookmarks(ASCIIToUTF16("Toolbar"), &bookmarks);
  size_t num_bookmarks = bookmarks.size();
  ASSERT_EQ(ARRAYSIZE_UNSAFE(kImportedBookmarksData), num_bookmarks);

  for (size_t i = 0; i < num_bookmarks; ++i) {
    ProfileWriter::BookmarkEntry& entry = bookmarks[i];
    EXPECT_EQ(kImportedBookmarksData[i].in_toolbar, entry.in_toolbar);
    EXPECT_EQ(kImportedBookmarksData[i].url, entry.url);

    std::vector<string16> path;
    Tokenize(kImportedBookmarksData[i].path, ASCIIToUTF16("/"), &path);
    ASSERT_EQ(path.size(), entry.path.size());
    for (size_t j = 0; j < path.size(); ++j) {
      EXPECT_EQ(path[j], entry.path[j]);
    }

    EXPECT_EQ(kImportedBookmarksData[i].title, entry.title);
  }
}

TEST_F(SafariImporterTest, FaviconImport) {
  scoped_refptr<SafariImporter> importer(GetSafariImporter());
  sql::Connection db;
  ASSERT_TRUE(importer->OpenDatabase(&db));

  SafariImporter::FaviconMap favicon_map;
  importer->ImportFaviconURLs(&db, &favicon_map);

  std::vector<history::ImportedFaviconUsage> favicons;
  importer->LoadFaviconData(&db, favicon_map, &favicons);

  size_t num_favicons = favicons.size();
  ASSERT_EQ(num_favicons, 2U);

  history::ImportedFaviconUsage &fav0 = favicons[0];
  EXPECT_EQ("http://s.ytimg.com/yt/favicon-vfl86270.ico",
            fav0.favicon_url.spec());
  EXPECT_GT(fav0.png_data.size(), 0U);
  EXPECT_EQ(fav0.urls.size(), 1U);
  EXPECT_TRUE(fav0.urls.find(GURL("http://www.youtube.com/"))
      != fav0.urls.end());

  history::ImportedFaviconUsage &fav1 = favicons[1];
  EXPECT_EQ("http://www.opensearch.org/favicon.ico",
            fav1.favicon_url.spec());
  EXPECT_GT(fav1.png_data.size(), 0U);
  EXPECT_EQ(fav1.urls.size(), 2U);
  EXPECT_TRUE(fav1.urls.find(GURL("http://www.opensearch.org/Home"))
      != fav1.urls.end());

  EXPECT_TRUE(fav1.urls.find(
      GURL("http://www.opensearch.org/Special:Search?search=lalala&go=Search"))
          != fav1.urls.end());
}

TEST_F(SafariImporterTest, CanImport) {
  uint16 items = importer::NONE;
  EXPECT_TRUE(SafariImporter::CanImport(GetTestSafariLibraryPath(), &items));
  EXPECT_EQ(items, importer::HISTORY | importer::FAVORITES);
  EXPECT_EQ(items & importer::COOKIES, importer::NONE);
  EXPECT_EQ(items & importer::PASSWORDS, importer::NONE);
  EXPECT_EQ(items & importer::SEARCH_ENGINES, importer::NONE);
  EXPECT_EQ(items & importer::HOME_PAGE, importer::NONE);

  // Check that we don't import anything from a bogus library directory.
  ScopedTempDir fake_library_dir;
  ASSERT_TRUE(fake_library_dir.CreateUniqueTempDir());
  EXPECT_FALSE(SafariImporter::CanImport(fake_library_dir.path(), &items));
}
