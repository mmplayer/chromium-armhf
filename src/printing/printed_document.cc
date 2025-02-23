// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/i18n/time_formatting.h"
#include "printing/page_number.h"
#include "printing/printed_pages_source.h"
#include "printing/printed_page.h"
#include "printing/units.h"
#include "skia/ext/platform_device.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/font.h"

namespace {

struct PrintDebugDumpPath {
  PrintDebugDumpPath()
    : enabled(false) {
  }

  bool enabled;
  FilePath debug_dump_path;
};

static base::LazyInstance<PrintDebugDumpPath> g_debug_dump_info(
    base::LINKER_INITIALIZED);

}  // namespace

namespace printing {

PrintedDocument::PrintedDocument(const PrintSettings& settings,
                                 PrintedPagesSource* source,
                                 int cookie)
    : mutable_(source),
      immutable_(settings, source, cookie) {

  // Records the expected page count if a range is setup.
  if (!settings.ranges.empty()) {
    // If there is a range, set the number of page
    for (unsigned i = 0; i < settings.ranges.size(); ++i) {
      const PageRange& range = settings.ranges[i];
      mutable_.expected_page_count_ += range.to - range.from + 1;
    }
  }
}

PrintedDocument::~PrintedDocument() {
}

void PrintedDocument::SetPage(int page_number,
                              Metafile* metafile,
                              double shrink,
                              const gfx::Size& paper_size,
                              const gfx::Rect& page_rect) {
  // Notice the page_number + 1, the reason is that this is the value that will
  // be shown. Users dislike 0-based counting.
  scoped_refptr<PrintedPage> page(
      new PrintedPage(page_number + 1,
                      metafile,
                      paper_size,
                      page_rect));
  {
    base::AutoLock lock(lock_);
    mutable_.pages_[page_number] = page;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
    if (page_number < mutable_.first_page)
      mutable_.first_page = page_number;
#endif

    if (mutable_.shrink_factor == 0) {
      mutable_.shrink_factor = shrink;
    } else {
      DCHECK_EQ(mutable_.shrink_factor, shrink);
    }
  }
  DebugDump(*page);
}

bool PrintedDocument::GetPage(int page_number,
                              scoped_refptr<PrintedPage>* page) {
  base::AutoLock lock(lock_);
  PrintedPages::const_iterator itr = mutable_.pages_.find(page_number);
  if (itr != mutable_.pages_.end()) {
    if (itr->second.get()) {
      *page = itr->second;
      return true;
    }
  }
  return false;
}

bool PrintedDocument::IsComplete() const {
  base::AutoLock lock(lock_);
  if (!mutable_.page_count_)
    return false;
  PageNumber page(immutable_.settings_, mutable_.page_count_);
  if (page == PageNumber::npos())
    return false;

  for (; page != PageNumber::npos(); ++page) {
#if defined(OS_WIN) || defined(OS_MACOSX)
    const bool metafile_must_be_valid = true;
#elif defined(OS_POSIX)
    const bool metafile_must_be_valid = (page.ToInt() == mutable_.first_page);
#endif
    PrintedPages::const_iterator itr = mutable_.pages_.find(page.ToInt());
    if (itr == mutable_.pages_.end() || !itr->second.get())
      return false;
    if (metafile_must_be_valid && !itr->second->metafile())
      return false;
  }
  return true;
}

void PrintedDocument::DisconnectSource() {
  base::AutoLock lock(lock_);
  mutable_.source_ = NULL;
}

uint32 PrintedDocument::MemoryUsage() const {
  std::vector< scoped_refptr<PrintedPage> > pages_copy;
  {
    base::AutoLock lock(lock_);
    pages_copy.reserve(mutable_.pages_.size());
    PrintedPages::const_iterator end = mutable_.pages_.end();
    for (PrintedPages::const_iterator itr = mutable_.pages_.begin();
         itr != end; ++itr) {
      if (itr->second.get()) {
        pages_copy.push_back(itr->second);
      }
    }
  }
  uint32 total = 0;
  for (size_t i = 0; i < pages_copy.size(); ++i) {
    total += pages_copy[i]->metafile()->GetDataSize();
  }
  return total;
}

void PrintedDocument::set_page_count(int max_page) {
  base::AutoLock lock(lock_);
  DCHECK_EQ(0, mutable_.page_count_);
  mutable_.page_count_ = max_page;
  if (immutable_.settings_.ranges.empty()) {
    mutable_.expected_page_count_ = max_page;
  } else {
    // If there is a range, don't bother since expected_page_count_ is already
    // initialized.
    DCHECK_NE(mutable_.expected_page_count_, 0);
  }
}

int PrintedDocument::page_count() const {
  base::AutoLock lock(lock_);
  return mutable_.page_count_;
}

int PrintedDocument::expected_page_count() const {
  base::AutoLock lock(lock_);
  return mutable_.expected_page_count_;
}

void PrintedDocument::DebugDump(const PrintedPage& page) {
  if (!g_debug_dump_info.Get().enabled)
    return;

  string16 filename;
  filename += name();
  filename += ASCIIToUTF16("_");
  filename += ASCIIToUTF16(StringPrintf("%02d", page.page_number()));
#if defined(OS_WIN)
  filename += ASCIIToUTF16("_.emf");
  page.metafile()->SaveTo(
      g_debug_dump_info.Get().debug_dump_path.Append(filename));
#else  // OS_WIN
  filename += ASCIIToUTF16("_.pdf");
  page.metafile()->SaveTo(
      g_debug_dump_info.Get().debug_dump_path.Append(UTF16ToUTF8(filename)));
#endif  // OS_WIN
}

void PrintedDocument::set_debug_dump_path(const FilePath& debug_dump_path) {
  g_debug_dump_info.Get().enabled = !debug_dump_path.empty();
  g_debug_dump_info.Get().debug_dump_path = debug_dump_path;
}

const FilePath& PrintedDocument::debug_dump_path() {
  return g_debug_dump_info.Get().debug_dump_path;
}

PrintedDocument::Mutable::Mutable(PrintedPagesSource* source)
    : source_(source),
      expected_page_count_(0),
      page_count_(0),
      shrink_factor(0) {
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  first_page = INT_MAX;
#endif
}

PrintedDocument::Mutable::~Mutable() {
}

PrintedDocument::Immutable::Immutable(const PrintSettings& settings,
                                      PrintedPagesSource* source,
                                      int cookie)
    : settings_(settings),
      source_message_loop_(MessageLoop::current()),
      name_(source->RenderSourceName()),
      cookie_(cookie) {
}

PrintedDocument::Immutable::~Immutable() {
}

}  // namespace printing
