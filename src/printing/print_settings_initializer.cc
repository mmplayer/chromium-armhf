// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings_initializer.h"

#include <algorithm>
#include <cmath>

#include "base/i18n/time_formatting.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"
#include "printing/units.h"
#include "ui/base/text/text_elider.h"

using base::DictionaryValue;
using base::Time;
using printing::ConvertPointsToPixelDouble;
using printing::ConvertUnitDouble;
using printing::GetHeaderFooterSegmentWidth;

namespace printing {

void PrintSettingsInitializer::InitHeaderFooterStrings(
      const DictionaryValue& job_settings,
      PrintSettings* print_settings) {
  if (!job_settings.GetBoolean(printing::kSettingHeaderFooterEnabled,
                               &print_settings->display_header_footer)) {
    NOTREACHED();
  }
  if (!print_settings->display_header_footer)
    return;

  string16 date = base::TimeFormatShortDateNumeric(Time::Now());
  string16 title;
  std::string url;
  if (!job_settings.GetString(printing::kSettingHeaderFooterTitle, &title) ||
      !job_settings.GetString(printing::kSettingHeaderFooterURL, &url)) {
    NOTREACHED();
  }

  gfx::Font font(UTF8ToUTF16(printing::kSettingHeaderFooterFontName),
                 ceil(ConvertPointsToPixelDouble(
                     printing::kSettingHeaderFooterFontSize)));
  double segment_width = GetHeaderFooterSegmentWidth(ConvertUnitDouble(
      print_settings->page_setup_device_units().physical_size().width(),
      print_settings->device_units_per_inch(),
      printing::kPixelsPerInch));
  date = ui::ElideText(date, font, segment_width, false);
  print_settings->date = date;

  // Calculate the available title width. If the date string is not long
  // enough, increase the available space for the title.
  // Assumes there is no header text to RIGHT of title.
  double date_width = font.GetStringWidth(date);
  double max_title_width = std::min(2 * segment_width,
                                    2 * (segment_width - date_width) +
                                        segment_width);
  print_settings->title = ui::ElideText(title, font, max_title_width, false);

  double max_url_width = 2 * segment_width;
  GURL gurl(url);
  print_settings->url = ui::ElideUrl(gurl, font, max_url_width, std::string());
}

}  // namespace printing

