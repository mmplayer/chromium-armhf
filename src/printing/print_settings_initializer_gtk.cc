// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings_initializer_gtk.h"

#include <gtk/gtk.h>
#include <gtk/gtkprinter.h>

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "printing/print_settings.h"
#include "printing/units.h"

namespace printing {

// static
void PrintSettingsInitializerGtk::InitPrintSettings(
    GtkPrintSettings* settings,
    GtkPageSetup* page_setup,
    const PageRanges& new_ranges,
    bool print_selection_only,
    PrintSettings* print_settings) {
  DCHECK(settings);
  DCHECK(page_setup);
  DCHECK(print_settings);

  string16 name(UTF8ToUTF16(static_cast<const char*>(
      gtk_print_settings_get_printer(settings))));
  print_settings->set_printer_name(name);
  print_settings->set_device_name(print_settings->printer_name());
  print_settings->ranges = new_ranges;
  print_settings->selection_only = print_selection_only;

  gfx::Size physical_size_device_units;
  gfx::Rect printable_area_device_units;
  int dpi = gtk_print_settings_get_resolution(settings);
  if (dpi) {
    // Initialize page_setup_device_units_.
    physical_size_device_units.SetSize(
        gtk_page_setup_get_paper_width(page_setup, GTK_UNIT_INCH) * dpi,
        gtk_page_setup_get_paper_height(page_setup, GTK_UNIT_INCH) * dpi);
    printable_area_device_units.SetRect(
        gtk_page_setup_get_left_margin(page_setup, GTK_UNIT_INCH) * dpi,
        gtk_page_setup_get_top_margin(page_setup, GTK_UNIT_INCH) * dpi,
        gtk_page_setup_get_page_width(page_setup, GTK_UNIT_INCH) * dpi,
        gtk_page_setup_get_page_height(page_setup, GTK_UNIT_INCH) * dpi);
  } else {
    // Use default values if we cannot get valid values from the print dialog.
    dpi = kPixelsPerInch;
    double page_width_in_pixel = 8.5 * dpi;
    double page_height_in_pixel = 11.0 * dpi;
    physical_size_device_units.SetSize(
        static_cast<int>(page_width_in_pixel),
        static_cast<int>(page_height_in_pixel));
    printable_area_device_units.SetRect(
        static_cast<int>(kLeftMarginInInch * dpi),
        static_cast<int>(kTopMarginInInch * dpi),
        page_width_in_pixel - (kLeftMarginInInch + kRightMarginInInch) * dpi,
        page_height_in_pixel - (kTopMarginInInch + kBottomMarginInInch) * dpi);
  }

  print_settings->set_dpi(dpi);
  print_settings->SetPrinterPrintableArea(physical_size_device_units,
                                          printable_area_device_units,
                                          dpi);

  // Note: With the normal GTK print dialog, when the user selects the landscape
  // orientation, all that does is change the paper size. Which seems to be
  // enough to render the right output and send it to the printer.
  // The orientation value stays as portrait and does not actually affect
  // printing.
  // Thus this is only useful in print preview mode, where we manually set the
  // orientation and change the paper size ourselves.
  GtkPageOrientation orientation = gtk_print_settings_get_orientation(settings);
  print_settings->SetOrientation(orientation == GTK_PAGE_ORIENTATION_LANDSCAPE);
}

const double PrintSettingsInitializerGtk::kTopMarginInInch = 0.25;
const double PrintSettingsInitializerGtk::kBottomMarginInInch = 0.56;
const double PrintSettingsInitializerGtk::kLeftMarginInInch = 0.25;
const double PrintSettingsInitializerGtk::kRightMarginInInch = 0.25;

}  // namespace printing
