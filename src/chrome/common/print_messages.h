// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for printing.
// Multiply-included message file, hence no include guard.

#include <string>
#include <vector>

#include "base/values.h"
#include "base/shared_memory.h"
#include "ipc/ipc_message_macros.h"
#include "printing/page_size_margins.h"
#include "printing/print_job_constants.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

#ifndef CHROME_COMMON_PRINT_MESSAGES_H_
#define CHROME_COMMON_PRINT_MESSAGES_H_

struct PrintMsg_Print_Params {
  PrintMsg_Print_Params();
  ~PrintMsg_Print_Params();

  // Resets the members of the struct to 0.
  void Reset();

  gfx::Size page_size;
  gfx::Size printable_size;
  int margin_top;
  int margin_left;
  double dpi;
  double min_shrink;
  double max_shrink;
  int desired_dpi;
  int document_cookie;
  bool selection_only;
  bool supports_alpha_blend;
  std::string preview_ui_addr;
  int preview_request_id;
  bool is_first_request;
  bool display_header_footer;
  string16 date;
  string16 title;
  string16 url;
};

struct PrintMsg_PrintPages_Params {
  PrintMsg_PrintPages_Params();
  ~PrintMsg_PrintPages_Params();

  // Resets the members of the struct to 0.
  void Reset();

  PrintMsg_Print_Params params;
  std::vector<int> pages;
};

#endif  // CHROME_COMMON_PRINT_MESSAGES_H_

#define IPC_MESSAGE_START PrintMsgStart

IPC_ENUM_TRAITS(printing::MarginType)

// Parameters for a render request.
IPC_STRUCT_TRAITS_BEGIN(PrintMsg_Print_Params)
  // Physical size of the page, including non-printable margins,
  // in pixels according to dpi.
  IPC_STRUCT_TRAITS_MEMBER(page_size)

  // In pixels according to dpi_x and dpi_y.
  IPC_STRUCT_TRAITS_MEMBER(printable_size)

  // The y-offset of the printable area, in pixels according to dpi.
  IPC_STRUCT_TRAITS_MEMBER(margin_top)

  // The x-offset of the printable area, in pixels according to dpi.
  IPC_STRUCT_TRAITS_MEMBER(margin_left)

  // Specifies dots per inch.
  IPC_STRUCT_TRAITS_MEMBER(dpi)

  // Minimum shrink factor. See PrintSettings::min_shrink for more information.
  IPC_STRUCT_TRAITS_MEMBER(min_shrink)

  // Maximum shrink factor. See PrintSettings::max_shrink for more information.
  IPC_STRUCT_TRAITS_MEMBER(max_shrink)

  // Desired apparent dpi on paper.
  IPC_STRUCT_TRAITS_MEMBER(desired_dpi)

  // Cookie for the document to ensure correctness.
  IPC_STRUCT_TRAITS_MEMBER(document_cookie)

  // Should only print currently selected text.
  IPC_STRUCT_TRAITS_MEMBER(selection_only)

  // Does the printer support alpha blending?
  IPC_STRUCT_TRAITS_MEMBER(supports_alpha_blend)

  // *** Parameters below are used only for print preview. ***

  // The print preview ui associated with this request.
  IPC_STRUCT_TRAITS_MEMBER(preview_ui_addr)

  // The id of the preview request.
  IPC_STRUCT_TRAITS_MEMBER(preview_request_id)

  // True if this is the first preview request.
  IPC_STRUCT_TRAITS_MEMBER(is_first_request)

  // Specifies if the header and footer should be rendered.
  IPC_STRUCT_TRAITS_MEMBER(display_header_footer)

  // Date string to be printed as header if requested by the user.
  IPC_STRUCT_TRAITS_MEMBER(date)

  // Title string to be printed as header if requested by the user.
  IPC_STRUCT_TRAITS_MEMBER(title)

  // URL string to be printed as footer if requested by the user.
  IPC_STRUCT_TRAITS_MEMBER(url)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(PrintMsg_PrintPage_Params)
  // Parameters to render the page as a printed page. It must always be the same
  // value for all the document.
  IPC_STRUCT_MEMBER(PrintMsg_Print_Params, params)

  // The page number is the indicator of the square that should be rendered
  // according to the layout specified in PrintMsg_Print_Params.
  IPC_STRUCT_MEMBER(int, page_number)
IPC_STRUCT_END()

IPC_STRUCT_TRAITS_BEGIN(printing::PageSizeMargins)
  IPC_STRUCT_TRAITS_MEMBER(content_width)
  IPC_STRUCT_TRAITS_MEMBER(content_height)
  IPC_STRUCT_TRAITS_MEMBER(margin_left)
  IPC_STRUCT_TRAITS_MEMBER(margin_right)
  IPC_STRUCT_TRAITS_MEMBER(margin_top)
  IPC_STRUCT_TRAITS_MEMBER(margin_bottom)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(PrintMsg_PrintPages_Params)
  // Parameters to render the page as a printed page. It must always be the same
  // value for all the document.
  IPC_STRUCT_TRAITS_MEMBER(params)

  // If empty, this means a request to render all the printed pages.
  IPC_STRUCT_TRAITS_MEMBER(pages)
IPC_STRUCT_TRAITS_END()

// Parameters to describe a rendered document.
IPC_STRUCT_BEGIN(PrintHostMsg_DidPreviewDocument_Params)
  // True when we can reuse existing preview data. |metafile_data_handle| and
  // |data_size| should not be used when this is true.
  IPC_STRUCT_MEMBER(bool, reuse_existing_data)

  // A shared memory handle to metafile data.
  IPC_STRUCT_MEMBER(base::SharedMemoryHandle, metafile_data_handle)

  // Size of metafile data.
  IPC_STRUCT_MEMBER(uint32, data_size)

  // Cookie for the document to ensure correctness.
  IPC_STRUCT_MEMBER(int, document_cookie)

  // Store the expected pages count.
  IPC_STRUCT_MEMBER(int, expected_pages_count)

  // Whether the preview can be modified.
  IPC_STRUCT_MEMBER(bool, modifiable)

  // The id of the preview request.
  IPC_STRUCT_MEMBER(int, preview_request_id)
IPC_STRUCT_END()

// Parameters to describe a rendered preview page.
IPC_STRUCT_BEGIN(PrintHostMsg_DidPreviewPage_Params)
  // A shared memory handle to metafile data for a draft document of the page.
  IPC_STRUCT_MEMBER(base::SharedMemoryHandle, metafile_data_handle)

  // Size of metafile data.
  IPC_STRUCT_MEMBER(uint32, data_size)

  // |page_number| is zero-based and can be |printing::INVALID_PAGE_INDEX| if it
  // is just a check.
  IPC_STRUCT_MEMBER(int, page_number)

  // The id of the preview request.
  IPC_STRUCT_MEMBER(int, preview_request_id)
IPC_STRUCT_END()

// Parameters sent along with the page count.
IPC_STRUCT_BEGIN(PrintHostMsg_DidGetPreviewPageCount_Params)
  // Cookie for the document to ensure correctness.
  IPC_STRUCT_MEMBER(int, document_cookie)

  // Total page count.
  IPC_STRUCT_MEMBER(int, page_count)

  // Indicates whether the previewed document is modifiable.
  IPC_STRUCT_MEMBER(bool, is_modifiable)

  // The id of the preview request.
  IPC_STRUCT_MEMBER(int, preview_request_id)

  // Indicates whether the existing preview data needs to be cleared or not.
  IPC_STRUCT_MEMBER(bool, clear_preview_data)
IPC_STRUCT_END()

// Parameters to describe a rendered page.
IPC_STRUCT_BEGIN(PrintHostMsg_DidPrintPage_Params)
  // A shared memory handle to the EMF data. This data can be quite large so a
  // memory map needs to be used.
  IPC_STRUCT_MEMBER(base::SharedMemoryHandle, metafile_data_handle)

  // Size of the metafile data.
  IPC_STRUCT_MEMBER(uint32, data_size)

  // Cookie for the document to ensure correctness.
  IPC_STRUCT_MEMBER(int, document_cookie)

  // Page number.
  IPC_STRUCT_MEMBER(int, page_number)

  // Shrink factor used to render this page.
  IPC_STRUCT_MEMBER(double, actual_shrink)

  // The size of the page the page author specified.
  IPC_STRUCT_MEMBER(gfx::Size, page_size)

  // The printable area the page author specified.
  IPC_STRUCT_MEMBER(gfx::Rect, content_area)
IPC_STRUCT_END()

// Parameters for the IPC message ViewHostMsg_ScriptedPrint
IPC_STRUCT_BEGIN(PrintHostMsg_ScriptedPrint_Params)
  IPC_STRUCT_MEMBER(int, routing_id)
  IPC_STRUCT_MEMBER(gfx::NativeViewId, host_window_id)
  IPC_STRUCT_MEMBER(int, cookie)
  IPC_STRUCT_MEMBER(int, expected_pages_count)
  IPC_STRUCT_MEMBER(bool, has_selection)
  IPC_STRUCT_MEMBER(printing::MarginType, margin_type)
IPC_STRUCT_END()


// Messages sent from the browser to the renderer.

// Tells the render view to initiate print preview for the entire document.
IPC_MESSAGE_ROUTED0(PrintMsg_InitiatePrintPreview)

// Tells the render view to initiate printing or print preview for a particular
// node, depending on which mode the render view is in.
IPC_MESSAGE_ROUTED0(PrintMsg_PrintNodeUnderContextMenu)

// Tells the renderer to print the print preview tab's PDF plugin without
// showing the print dialog. (This is the final step in the print preview
// workflow.)
IPC_MESSAGE_ROUTED1(PrintMsg_PrintForPrintPreview,
                    DictionaryValue /* settings */)

// Tells the render view to switch the CSS to print media type, renders every
// requested pages and switch back the CSS to display media type.
IPC_MESSAGE_ROUTED0(PrintMsg_PrintPages)

// Tells the render view that printing is done so it can clean up.
IPC_MESSAGE_ROUTED1(PrintMsg_PrintingDone,
                    bool /* success */)

// Tells the render view that preview printing request has been cancelled.
IPC_MESSAGE_ROUTED0(PrintMsg_PreviewPrintingRequestCancelled)

// Tells the render view to switch the CSS to print media type, renders every
// requested pages for print preview using the given |settings|. This gets
// called multiple times as the user updates settings.
IPC_MESSAGE_ROUTED1(PrintMsg_PrintPreview,
                    DictionaryValue /* settings */)

// Like PrintMsg_PrintPages, but using the print preview document's frame/node.
IPC_MESSAGE_ROUTED0(PrintMsg_PrintForSystemDialog)

// Tells a renderer to stop blocking script initiated printing.
IPC_MESSAGE_ROUTED0(PrintMsg_ResetScriptedPrintCount)

// Messages sent from the renderer to the browser.

#if defined(OS_WIN)
// Duplicates a shared memory handle from the renderer to the browser. Then
// the renderer can flush the handle.
IPC_SYNC_MESSAGE_ROUTED1_1(PrintHostMsg_DuplicateSection,
                           base::SharedMemoryHandle /* renderer handle */,
                           base::SharedMemoryHandle /* browser handle */)
#endif

// Tells the browser that the renderer is done calculating the number of
// rendered pages according to the specified settings.
IPC_MESSAGE_ROUTED2(PrintHostMsg_DidGetPrintedPagesCount,
                    int /* rendered document cookie */,
                    int /* number of rendered pages */)

// Sends the document cookie of the current printer query to the browser.
IPC_MESSAGE_ROUTED1(PrintHostMsg_DidGetDocumentCookie,
                    int /* rendered document cookie */)

// Tells the browser that the print dialog has been shown.
IPC_MESSAGE_ROUTED0(PrintHostMsg_DidShowPrintDialog)

// Sends back to the browser the rendered "printed page" that was requested by
// a ViewMsg_PrintPage message or from scripted printing. The memory handle in
// this message is already valid in the browser process.
IPC_MESSAGE_ROUTED1(PrintHostMsg_DidPrintPage,
                    PrintHostMsg_DidPrintPage_Params /* page content */)

// The renderer wants to know the default print settings.
IPC_SYNC_MESSAGE_ROUTED0_1(PrintHostMsg_GetDefaultPrintSettings,
                           PrintMsg_Print_Params /* default_settings */)

// The renderer wants to update the current print settings with new
// |job_settings|.
IPC_SYNC_MESSAGE_ROUTED2_1(PrintHostMsg_UpdatePrintSettings,
                           int /* document_cookie */,
                           DictionaryValue /* job_settings */,
                           PrintMsg_PrintPages_Params /* current_settings */)

// It's the renderer that controls the printing process when it is generated
// by javascript. This step is about showing UI to the user to select the
// final print settings. The output parameter is the same as
// ViewMsg_PrintPages which is executed implicitly.
IPC_SYNC_MESSAGE_ROUTED1_1(PrintHostMsg_ScriptedPrint,
                           PrintHostMsg_ScriptedPrint_Params,
                           PrintMsg_PrintPages_Params
                               /* settings chosen by the user*/)

#if defined(USE_X11)
// Asks the browser to create a temporary file for the renderer to fill
// in resulting NativeMetafile in printing.
IPC_SYNC_MESSAGE_CONTROL0_2(PrintHostMsg_AllocateTempFileForPrinting,
                            base::FileDescriptor /* temp file fd */,
                            int /* fd in browser*/)
IPC_MESSAGE_CONTROL1(PrintHostMsg_TempFileForPrintingWritten,
                     int /* fd in browser */)
#endif

// Asks the browser to do print preview.
IPC_MESSAGE_ROUTED0(PrintHostMsg_RequestPrintPreview)

// Notify the browser the number of pages in the print preview document.
IPC_MESSAGE_ROUTED1(PrintHostMsg_DidGetPreviewPageCount,
                    PrintHostMsg_DidGetPreviewPageCount_Params /* params */)

// Notify the browser of the default page layout according to the currently
// selected printer and page size.
IPC_MESSAGE_ROUTED1(PrintHostMsg_DidGetDefaultPageLayout,
                    printing::PageSizeMargins /* page layout in points */)

// Notify the browser a print preview page has been rendered.
IPC_MESSAGE_ROUTED1(PrintHostMsg_DidPreviewPage,
                    PrintHostMsg_DidPreviewPage_Params /* params */)

// Asks the browser whether the print preview has been cancelled.
IPC_SYNC_MESSAGE_ROUTED2_1(PrintHostMsg_CheckForCancel,
                           std::string /* print preview ui address */,
                           int /* request id */,
                           bool /* print preview cancelled */)

// Sends back to the browser the complete rendered document (non-draft mode,
// used for printing) that was requested by a PrintMsg_PrintPreview message.
// The memory handle in this message is already valid in the browser process.
IPC_MESSAGE_ROUTED1(PrintHostMsg_MetafileReadyForPrinting,
                    PrintHostMsg_DidPreviewDocument_Params /* params */)

// Tell the browser printing failed.
IPC_MESSAGE_ROUTED1(PrintHostMsg_PrintingFailed,
                    int /* document cookie */)

// Tell the browser print preview failed.
IPC_MESSAGE_ROUTED1(PrintHostMsg_PrintPreviewFailed,
                    int /* document cookie */)

// Tell the browser print preview was cancelled.
IPC_MESSAGE_ROUTED1(PrintHostMsg_PrintPreviewCancelled,
                    int /* document cookie */)

// Tell the browser print preview found the selected printer has invalid
// settings (which typically caused by disconnected network printer or printer
// driver is bogus).
IPC_MESSAGE_ROUTED1(PrintHostMsg_PrintPreviewInvalidPrinterSettings,
                    int /* document cookie */)
