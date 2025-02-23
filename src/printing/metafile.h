// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_METAFILE_H_
#define PRINTING_METAFILE_H_

#include "base/basictypes.h"
#include "build/build_config.h"
#include "printing/printing_export.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include "base/mac/scoped_cftyperef.h"
#endif

class FilePath;

namespace gfx {
class Point;
class Rect;
class Size;
}

class SkDevice;

#if defined(OS_CHROMEOS)
namespace base {
struct FileDescriptor;
}
#endif

namespace printing {

// This class creates a graphics context that renders into a data stream
// (usually PDF or EMF).
class PRINTING_EXPORT Metafile {
 public:
  virtual ~Metafile() {}

  // Initializes a fresh new metafile for rendering. Returns false on failure.
  // Note: It should only be called from within the renderer process to allocate
  // rendering resources.
  virtual bool Init() = 0;

  // Initializes the metafile with the data in |src_buffer|. Returns true
  // on success.
  // Note: It should only be called from within the browser process.
  virtual bool InitFromData(const void* src_buffer, uint32 src_buffer_size) = 0;

  // This method calls StartPage and then returns an appropriate
  // VectorPlatformDevice implementation bound to the context created by
  // StartPage or NULL on error.
  virtual SkDevice* StartPageForVectorCanvas(
      const gfx::Size& page_size,
      const gfx::Rect& content_area,
      const float& scale_factor) = 0;

  // Prepares a context for rendering a new page with the given |page_size|,
  // |content_area| and  a |scale_factor| to use for the drawing. The units are
  // in points (=1/72 in). Returns true on success.
  virtual bool StartPage(const gfx::Size& page_size,
                         const gfx::Rect& content_area,
                         const float& scale_factor) = 0;

  // Closes the current page and destroys the context used in rendering that
  // page. The results of current page will be appended into the underlying
  // data stream. Returns true on success.
  virtual bool FinishPage() = 0;

  // Closes the metafile. No further rendering is allowed (the current page
  // is implicitly closed).
  virtual bool FinishDocument() = 0;

  // Returns the size of the underlying data stream. Only valid after Close()
  // has been called.
  virtual uint32 GetDataSize() const = 0;

  // Copies the first |dst_buffer_size| bytes of the underlying data stream into
  // |dst_buffer|. This function should ONLY be called after Close() is invoked.
  // Returns true if the copy succeeds.
  virtual bool GetData(void* dst_buffer, uint32 dst_buffer_size) const = 0;

  // Saves the underlying data to the given file. This function should ONLY be
  // called after the metafile is closed. Returns true if writing succeeded.
  virtual bool SaveTo(const FilePath& file_path) const = 0;

  // Returns the bounds of the given page. Pages use a 1-based index.
  virtual gfx::Rect GetPageBounds(unsigned int page_number) const = 0;
  virtual unsigned int GetPageCount() const = 0;

  // Get the context for rendering to the PDF.
  virtual gfx::NativeDrawingContext context() const = 0;

#if defined(OS_WIN)
  // "Plays" the EMF buffer in a HDC. It is the same effect as calling the
  // original GDI function that were called when recording the EMF. |rect| is in
  // "logical units" and is optional. If |rect| is NULL, the natural EMF bounds
  // are used.
  // Note: Windows has been known to have stack buffer overflow in its GDI
  // functions, whether used directly or indirectly through precompiled EMF
  // data. We have to accept the risk here. Since it is used only for printing,
  // it requires user intervention.
  virtual bool Playback(gfx::NativeDrawingContext hdc,
                        const RECT* rect) const = 0;

  // The slow version of Playback(). It enumerates all the records and play them
  // back in the HDC. The trick is that it skip over the records known to have
  // issue with some printers. See Emf::Record::SafePlayback implementation for
  // details.
  virtual bool SafePlayback(gfx::NativeDrawingContext hdc) const = 0;

  virtual HENHMETAFILE emf() const = 0;
#elif defined(OS_MACOSX)
  // Renders the given page into |rect| in the given context.
  // Pages use a 1-based index. The rendering uses the following arguments
  // to determine scaling and translation factors.
  // |shrink_to_fit| specifies whether the output should be shrunk to fit the
  // supplied |rect| if the page size is larger than |rect| in any dimension.
  // If this is false, parts of the PDF page that lie outside the bounds will be
  // clipped.
  // |stretch_to_fit| specifies whether the output should be stretched to fit
  // the supplied bounds if the page size is smaller than |rect| in all
  // dimensions.
  // |center_horizontally| specifies whether the final image (after any scaling
  // is done) should be centered horizontally within the given |rect|.
  // |center_vertically| specifies whether the final image (after any scaling
  // is done) should be centered vertically within the given |rect|.
  // Note that all scaling preserves the original aspect ratio of the page.
  virtual bool RenderPage(unsigned int page_number,
                          gfx::NativeDrawingContext context,
                          const CGRect rect,
                          bool shrink_to_fit,
                          bool stretch_to_fit,
                          bool center_horizontally,
                          bool center_vertically) const = 0;
#elif defined(OS_CHROMEOS)
  // Saves the underlying data to the file associated with fd. This function
  // should ONLY be called after the metafile is closed.
  // Returns true if writing succeeded.
  virtual bool SaveToFD(const base::FileDescriptor& fd) const = 0;
#endif  // if defined(OS_CHROMEOS)
};

}  // namespace printing

#endif  // PRINTING_METAFILE_H_
