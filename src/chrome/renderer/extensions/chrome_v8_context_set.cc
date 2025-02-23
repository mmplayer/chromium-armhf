// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_v8_context_set.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/tracked_objects.h"
#include "base/values.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
#include "content/public/renderer/render_view.h"
#include "v8/include/v8.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using content::RenderThread;
using content::V8ValueConverter;

namespace {

// Returns true if the extension running in the given |render_view| has
// sufficient permissions to access the data.
//
// TODO(aa): This looks super suspicious. Is it correct? Can we use something
// else already in the system? Should it be moved elsewhere?
  bool HasSufficientPermissions(content::RenderView* render_view,
                                const GURL& event_url) {
  // During unit tests, we might be invoked without a v8 context. In these
  // cases, we only allow empty event_urls and short-circuit before retrieving
  // the render view from the current context.
  if (!event_url.is_valid())
    return true;

  WebKit::WebDocument document =
      render_view->GetWebView()->mainFrame()->document();
  return GURL(document.url()).SchemeIs(chrome::kExtensionScheme) &&
       document.securityOrigin().canRequest(event_url);
}

}

ChromeV8ContextSet::ChromeV8ContextSet() {
}
ChromeV8ContextSet::~ChromeV8ContextSet() {
}

int ChromeV8ContextSet::size() const {
  return static_cast<int>(contexts_.size());
}

void ChromeV8ContextSet::Add(ChromeV8Context* context) {
#ifndef NDEBUG
  // It's OK to insert the same context twice, but we should only ever have one
  // ChromeV8Context per v8::Context.
  for (ContextSet::iterator iter = contexts_.begin(); iter != contexts_.end();
       ++iter) {
    ChromeV8Context* candidate = *iter;
    if (candidate != context)
      DCHECK(candidate->v8_context() != context->v8_context());
  }
#endif
  contexts_.insert(context);
}

void ChromeV8ContextSet::Remove(ChromeV8Context* context) {
  if (contexts_.erase(context)) {
    context->clear_web_frame();
    MessageLoop::current()->DeleteSoon(FROM_HERE, context);
  }
}

ChromeV8ContextSet::ContextSet ChromeV8ContextSet::GetAll()
    const {
  return contexts_;
}

ChromeV8Context* ChromeV8ContextSet::GetCurrent() const {
  if (!v8::Context::InContext())
    return NULL;
  else
    return GetByV8Context(v8::Context::GetCurrent());
}

ChromeV8Context* ChromeV8ContextSet::GetByV8Context(
    v8::Handle<v8::Context> v8_context) const {
  for (ContextSet::const_iterator iter = contexts_.begin();
       iter != contexts_.end(); ++iter) {
    if ((*iter)->v8_context() == v8_context)
      return *iter;
  }

  return NULL;
}

void ChromeV8ContextSet::DispatchChromeHiddenMethod(
    const std::string& extension_id,
    const std::string& method_name,
    const base::ListValue& arguments,
    content::RenderView* render_view,
    const GURL& event_url) const {
  v8::HandleScope handle_scope;

  // We copy the context list, because calling into javascript may modify it
  // out from under us.
  ContextSet contexts = GetAll();

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  for (ContextSet::iterator it = contexts.begin(); it != contexts.end();
       ++it) {
    if ((*it)->v8_context().IsEmpty())
      continue;

    if (!extension_id.empty() && extension_id != (*it)->extension_id())
      continue;

    content::RenderView* context_render_view = (*it)->GetRenderView();
    if (!context_render_view)
      continue;

    if (render_view && render_view != context_render_view)
      continue;

    if (!HasSufficientPermissions(context_render_view, event_url))
      continue;

    v8::Local<v8::Context> context(*((*it)->v8_context()));
    std::vector<v8::Handle<v8::Value> > v8_arguments;
    for (size_t i = 0; i < arguments.GetSize(); ++i) {
      base::Value* item = NULL;
      CHECK(arguments.Get(i, &item));
      v8_arguments.push_back(converter->ToV8Value(item, context));
    }

    v8::Handle<v8::Value> retval;
    (*it)->CallChromeHiddenMethod(
        method_name, v8_arguments.size(), &v8_arguments[0], &retval);
    // In debug, the js will validate the event parameters and return a
    // string if a validation error has occured.
    // TODO(rafaelw): Consider only doing this check if function_name ==
    // "Event.dispatchJSON".
#ifndef NDEBUG
    if (!retval.IsEmpty() && !retval->IsUndefined()) {
      std::string error = *v8::String::AsciiValue(retval);
      DCHECK(false) << error;
    }
#endif
  }
}
