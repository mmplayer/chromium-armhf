// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_MAC_H_
#define PRINTING_PRINTING_CONTEXT_MAC_H_

#include <string>

#include "base/memory/scoped_nsobject.h"
#include "printing/printing_context.h"
#include "printing/print_job_constants.h"

#ifdef __OBJC__
@class NSPrintInfo;
#else
class NSPrintInfo;
#endif  // __OBJC__

namespace printing {

class PRINTING_EXPORT PrintingContextMac : public PrintingContext {
 public:
  explicit PrintingContextMac(const std::string& app_locale);
  virtual ~PrintingContextMac();

  // PrintingContext implementation.
  virtual void AskUserForSettings(gfx::NativeView parent_view,
                                  int max_pages,
                                  bool has_selection,
                                  PrintSettingsCallback* callback);
  virtual Result UseDefaultSettings();
  virtual Result UpdatePrinterSettings(
      const base::DictionaryValue& job_settings,
      const PageRanges& ranges);
  virtual Result InitWithSettings(const PrintSettings& settings);
  virtual Result NewDocument(const string16& document_name);
  virtual Result NewPage();
  virtual Result PageDone();
  virtual Result DocumentDone();
  virtual void Cancel();
  virtual void ReleaseContext();
  virtual gfx::NativeDrawingContext context() const;

 private:
  // Initializes PrintSettings from |print_info_|. This must be called
  // after changes to |print_info_| in order for the changes to take effect in
  // printing.
  // This function ignores the page range information specified in the print
  // info object and use |ranges| instead.
  void InitPrintSettingsFromPrintInfo(const PageRanges& ranges);

  // Returns the set of page ranges constructed from |print_info_|.
  PageRanges GetPageRangesFromPrintInfo();

  // Updates |print_info_| to use the given printer.
  // Returns true if the printer was set else returns false.
  bool SetPrinter(const std::string& device_name);

  // Sets |copies| in PMPrintSettings.
  // Returns true if the number of copies is set.
  bool SetCopiesInPrintSettings(int copies);

  // Sets |collate| in PMPrintSettings.
  // Returns true if |collate| is set.
  bool SetCollateInPrintSettings(bool collate);

  // Sets orientation in native print info object.
  // Returns true if the orientation was set.
  bool SetOrientationIsLandscape(bool landscape);

  // Sets duplex mode in PMPrintSettings.
  // Returns true if duplex mode is set.
  bool SetDuplexModeInPrintSettings(DuplexMode mode);

  // Sets output color mode in PMPrintSettings.
  // Returns true if color mode is set.
  bool SetOutputColor(int color_mode);

  // The native print info object.
  scoped_nsobject<NSPrintInfo> print_info_;

  // The current page's context; only valid between NewPage and PageDone call
  // pairs.
  CGContext* context_;

  DISALLOW_COPY_AND_ASSIGN(PrintingContextMac);
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_MAC_H_
