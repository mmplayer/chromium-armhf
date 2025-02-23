// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#include "base/logging.h"
#include "printing/page_number.h"
#include "printing/printed_pages_source.h"
#include "printing/printed_page.h"
#include "printing/units.h"
#include "skia/ext/platform_device.h"

namespace {

void SimpleModifyWorldTransform(HDC context,
                                int offset_x,
                                int offset_y,
                                double shrink_factor) {
  XFORM xform = { 0 };
  xform.eDx = static_cast<float>(offset_x);
  xform.eDy = static_cast<float>(offset_y);
  xform.eM11 = xform.eM22 = static_cast<float>(1. / shrink_factor);
  BOOL res = ModifyWorldTransform(context, &xform, MWT_LEFTMULTIPLY);
  DCHECK_NE(res, 0);
}

void DrawRect(HDC context, gfx::Rect rect) {
  Rectangle(context, rect.x(), rect.y(), rect.right(), rect.bottom());
}

}  // namespace

namespace printing {

void PrintedDocument::RenderPrintedPage(
    const PrintedPage& page, gfx::NativeDrawingContext context) const {
#ifndef NDEBUG
  {
    // Make sure the page is from our list.
    base::AutoLock lock(lock_);
    DCHECK(&page == mutable_.pages_.find(page.page_number() - 1)->second.get());
  }
#endif

  DCHECK(context);

  const printing::PageSetup& page_setup(
      immutable_.settings_.page_setup_device_units());
  gfx::Rect content_area;
  page.GetCenteredPageContentRect(page_setup.physical_size(), &content_area);

  // Save the state to make sure the context this function call does not modify
  // the device context.
  int saved_state = SaveDC(context);
  DCHECK_NE(saved_state, 0);
  skia::InitializeDC(context);
  {
    // Save the state (again) to apply the necessary world transformation.
    int saved_state = SaveDC(context);
    DCHECK_NE(saved_state, 0);

    // Setup the matrix to translate and scale to the right place. Take in
    // account the actual shrinking factor.
    // Note that the printing output is relative to printable area of the page.
    // That is 0,0 is offset by PHYSICALOFFSETX/Y from the page.
    SimpleModifyWorldTransform(
        context,
        content_area.x() - page_setup.printable_area().x(),
        content_area.y() - page_setup.printable_area().y(),
        mutable_.shrink_factor);

    if (!page.metafile()->SafePlayback(context)) {
      NOTREACHED();
    }

    BOOL res = RestoreDC(context, saved_state);
    DCHECK_NE(res, 0);
  }

  int res = RestoreDC(context, saved_state);
  DCHECK_NE(res, 0);
}

}  // namespace printing
