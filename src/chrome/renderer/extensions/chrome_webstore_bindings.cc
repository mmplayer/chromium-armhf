// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_webstore_bindings.h"

#include "base/lazy_instance.h"
#include "base/string_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/renderer/extensions/extension_helper.h"
#include "chrome/renderer/weak_v8_function_map.h"
#include "content/public/renderer/render_view.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNodeList.h"
#include "v8/include/v8.h"

using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebNode;
using WebKit::WebNodeList;

namespace {

const char kWebstoreV8ExtensionName[] = "v8/ChromeWebstore";

const char kWebstoreLinkRelation[] = "chrome-webstore-item";

const char kPreferredStoreLinkUrlNotAString[] =
    "The Chrome Web Store item link URL parameter must be a string.";
const char kSuccessCallbackNotAFunctionError[] =
    "The success callback parameter must be a function.";
const char kFailureCallbackNotAFunctionError[] =
    "The failure callback parameter must be a function.";
const char kNotInTopFrameError[] =
    "Chrome Web Store installations can only be started by the top frame.";
const char kNotUserGestureError[] =
    "Chrome Web Store installations can only be initated by a user gesture.";
const char kNoWebstoreItemLinkFoundError[] =
    "No Chrome Web Store item link found.";
const char kInvalidWebstoreItemUrlError[] =
    "Invalid Chrome Web Store item URL.";

// chrome.webstore.install() calls generate an install ID so that the install's
// callbacks may be fired when the browser notifies us of install completion
// (successful or not) via HandleInstallResponse.
int g_next_install_id = 0;

base::LazyInstance<WeakV8FunctionMap> g_success_callbacks(
    base::LINKER_INITIALIZED);
base::LazyInstance<WeakV8FunctionMap> g_failure_callbacks(
    base::LINKER_INITIALIZED);

} // anonymous namespace

class ExtensionImpl : public v8::Extension {
 public:
  ExtensionImpl() :
    v8::Extension(
        kWebstoreV8ExtensionName,
        "var chrome = chrome || {};"
        "if (!chrome.webstore) {"
        "  chrome.webstore = new function() {"
        "    native function Install(preferredStoreUrl, onSuccess, onFailure);"
        "    this.install = Install;"
        "  };"
        "}") {
  }

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("Install"))) {
      return v8::FunctionTemplate::New(Install);
    } else {
      return v8::Handle<v8::FunctionTemplate>();
    }
  }

  static v8::Handle<v8::Value> Install(const v8::Arguments& args) {
    WebFrame* frame = WebFrame::frameForCurrentContext();
    if (!frame || !frame->view())
      return v8::Undefined();

    content::RenderView* render_view =
        content::RenderView::FromWebView(frame->view());
    if (!render_view)
      return v8::Undefined();

    std::string preferred_store_link_url;
    if (args.Length() >= 1 && !args[0]->IsUndefined()) {
      if (args[0]->IsString()) {
        preferred_store_link_url = std::string(*v8::String::Utf8Value(args[0]));
      } else {
        v8::ThrowException(v8::String::New(kPreferredStoreLinkUrlNotAString));
        return v8::Undefined();
      }
    }

    std::string webstore_item_id;
    std::string error;
    if (!GetWebstoreItemIdFromFrame(
        frame, preferred_store_link_url, &webstore_item_id, &error)) {
      v8::ThrowException(v8::String::New(error.c_str()));
      return v8::Undefined();
    }

    int install_id = g_next_install_id++;
    if (args.Length() >= 2 && !args[1]->IsUndefined()) {
      if (args[1]->IsFunction()) {
        g_success_callbacks.Get().Add(install_id, v8::Function::Cast(*args[1]));
      } else {
        v8::ThrowException(v8::String::New(kSuccessCallbackNotAFunctionError));
        return v8::Undefined();
      }
    }
    if (args.Length() >= 3 && !args[2]->IsUndefined()) {
      if (args[2]->IsFunction()) {
        g_failure_callbacks.Get().Add(install_id, v8::Function::Cast(*args[2]));
      } else {
        v8::ThrowException(v8::String::New(kFailureCallbackNotAFunctionError));
        return v8::Undefined();
      }
    }

    ExtensionHelper* helper = ExtensionHelper::Get(render_view);
    helper->InlineWebstoreInstall(
        install_id, webstore_item_id, frame->document().url());
    return v8::Undefined();
  }

 private:
  // Extracts a Web Store item ID from a <link rel="chrome-webstore-item"
  // href="https://chrome.google.com/webstore/detail/id"> node found in the
  // frame. On success, true will be returned and the |webstore_item_id|
  // parameter will be populated with the ID. On failure, false will be returned
  // and |error| will be populated with the error.
  static bool GetWebstoreItemIdFromFrame(
      WebFrame* frame, const std::string& preferred_store_link_url,
      std::string* webstore_item_id, std::string* error) {
    if (frame != frame->top()) {
      *error = kNotInTopFrameError;
      return false;
    }

    if (!frame->isProcessingUserGesture()) {
      *error = kNotUserGestureError;
      return false;
    }

    WebDocument document = frame->document();
    if (document.isNull()) {
      *error = kNoWebstoreItemLinkFoundError;
      return false;
    }

    WebElement head = document.head();
    if (head.isNull()) {
      *error = kNoWebstoreItemLinkFoundError;
      return false;
    }

    GURL webstore_base_url =
        GURL(extension_urls::GetWebstoreItemDetailURLPrefix());
    WebNodeList children = head.childNodes();
    for (unsigned i = 0; i < children.length(); ++i) {
      WebNode child = children.item(i);
      if (!child.isElementNode())
        continue;
      WebElement elem = child.to<WebElement>();

      if (!elem.hasTagName("link") || !elem.hasAttribute("rel") ||
          !elem.hasAttribute("href"))
        continue;

      std::string rel = elem.getAttribute("rel").utf8();
      if (!LowerCaseEqualsASCII(rel, kWebstoreLinkRelation))
        continue;

      std::string webstore_url_string(elem.getAttribute("href").utf8());

      if (!preferred_store_link_url.empty() &&
          preferred_store_link_url != webstore_url_string) {
        continue;
      }

      GURL webstore_url = GURL(webstore_url_string);
      if (!webstore_url.is_valid()) {
        *error = kInvalidWebstoreItemUrlError;
        return false;
      }

      if (webstore_url.scheme() != webstore_base_url.scheme() ||
          webstore_url.host() != webstore_base_url.host() ||
          !StartsWithASCII(
              webstore_url.path(), webstore_base_url.path(), true)) {
        *error = kInvalidWebstoreItemUrlError;
        return false;
      }

      std::string candidate_webstore_item_id = webstore_url.path().substr(
          webstore_base_url.path().length());
      if (!::Extension::IdIsValid(candidate_webstore_item_id)) {
        *error = kInvalidWebstoreItemUrlError;
        return false;
      }

      std::string reconstructed_webstore_item_url_string =
          extension_urls::GetWebstoreItemDetailURLPrefix() +
              candidate_webstore_item_id;
      if (reconstructed_webstore_item_url_string != webstore_url_string) {
        *error = kInvalidWebstoreItemUrlError;
        return false;
      }

      *webstore_item_id = candidate_webstore_item_id;
      return true;
    }

    *error = kNoWebstoreItemLinkFoundError;
    return false;
  }
};

// static
v8::Extension* ChromeWebstoreExtension::Get() {
  return new ExtensionImpl();
}

// static
void ChromeWebstoreExtension::HandleInstallResponse(int install_id,
                                                    bool success,
                                                    const std::string& error) {
  WeakV8FunctionMap& callback_map =
      success ? g_success_callbacks.Get() : g_failure_callbacks.Get();

  v8::Persistent<v8::Function> function = callback_map.Remove(install_id);
  if (function.IsEmpty())
    return;

  v8::HandleScope handle_scope;
  v8::Context::Scope context_scope(function->CreationContext());
  v8::Handle<v8::Value> argv[1];
  argv[0] = v8::String::New(error.c_str());
  function->Call(v8::Object::New(), arraysize(argv), argv);
}
