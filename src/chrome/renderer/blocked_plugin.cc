// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/blocked_plugin.h"

#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/plugin_uma.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMenuItemInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRegularExpression.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCaseSensitivity.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/plugins/npapi/plugin_group.h"
#include "webkit/plugins/npapi/webview_plugin.h"

using WebKit::WebContextMenuData;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebMenuItemInfo;
using WebKit::WebNode;
using WebKit::WebPlugin;
using WebKit::WebPluginContainer;
using WebKit::WebPluginParams;
using WebKit::WebPoint;
using WebKit::WebRegularExpression;
using WebKit::WebString;
using WebKit::WebURLRequest;
using WebKit::WebVector;
using content::RenderThread;

static const char* const kBlockedPluginDataURL = "chrome://blockedplugindata/";
// TODO(cevans) - move these to a shared header file so that there are no
// collisions in the longer term. Currently, blocked_plugin.cc is the only
// user of custom menu commands (extension menu items have their own range).
static const unsigned kMenuActionLoad = 1;
static const unsigned kMenuActionRemove = 2;

static const BlockedPlugin* g_last_active_menu;

BlockedPlugin::BlockedPlugin(content::RenderView* render_view,
                             WebFrame* frame,
                             const webkit::WebPluginInfo& info,
                             const WebPluginParams& params,
                             const WebPreferences& preferences,
                             int template_id,
                             const string16& name,
                             const string16& message,
                             bool is_blocked_for_prerendering,
                             bool allow_loading)
    : content::RenderViewObserver(render_view),
      frame_(frame),
      plugin_info_(info),
      plugin_params_(params),
      name_(name),
      is_blocked_for_prerendering_(is_blocked_for_prerendering),
      hidden_(false),
      allow_loading_(allow_loading) {
  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(template_id));

  DCHECK(!template_html.empty()) << "unable to load template. ID: "
                                 << template_id;

  DictionaryValue values;
  values.SetString("message", message);
  values.SetString("name", name_);
  values.SetString("hide", l10n_util::GetStringUTF8(IDS_PLUGIN_HIDE));

  // "t" is the id of the templates root node.
  std::string html_data = jstemplate_builder::GetI18nTemplateHtml(
      template_html, &values);

  plugin_ = webkit::npapi::WebViewPlugin::Create(this,
                                                 preferences,
                                                 html_data,
                                                 GURL(kBlockedPluginDataURL));
}

BlockedPlugin::~BlockedPlugin() {
}

void BlockedPlugin::BindWebFrame(WebFrame* frame) {
  BindToJavascript(frame, "plugin");
  BindMethod("load", &BlockedPlugin::LoadCallback);
  BindMethod("hide", &BlockedPlugin::HideCallback);
  BindMethod("openURL", &BlockedPlugin::OpenUrlCallback);
}

void BlockedPlugin::WillDestroyPlugin() {
  delete this;
}

void BlockedPlugin::ShowContextMenu(const WebKit::WebMouseEvent& event) {
  WebContextMenuData menu_data;

  size_t num_items = name_.empty() ? 2u : 4u;
  WebVector<WebMenuItemInfo> custom_items(num_items);

  size_t i = 0;
  if (!name_.empty()) {
    WebMenuItemInfo name_item;
    name_item.label = name_;
    name_item.hasTextDirectionOverride = false;
    name_item.textDirection =  WebKit::WebTextDirectionDefault;
    custom_items[i++] = name_item;

    WebMenuItemInfo separator_item;
    separator_item.type = WebMenuItemInfo::Separator;
    custom_items[i++] = separator_item;
  }

  WebMenuItemInfo run_item;
  run_item.action = kMenuActionLoad;
  // Disable this menu item if the plugin is blocked by policy.
  run_item.enabled = allow_loading_;
  run_item.label = WebString::fromUTF8(
      l10n_util::GetStringUTF8(IDS_CONTENT_CONTEXT_PLUGIN_RUN).c_str());
  run_item.hasTextDirectionOverride = false;
  run_item.textDirection =  WebKit::WebTextDirectionDefault;
  custom_items[i++] = run_item;

  WebMenuItemInfo hide_item;
  hide_item.action = kMenuActionRemove;
  hide_item.enabled = true;
  hide_item.label = WebString::fromUTF8(
      l10n_util::GetStringUTF8(IDS_CONTENT_CONTEXT_PLUGIN_HIDE).c_str());
  hide_item.hasTextDirectionOverride = false;
  hide_item.textDirection =  WebKit::WebTextDirectionDefault;
  custom_items[i++] = hide_item;

  menu_data.customItems.swap(custom_items);
  menu_data.mousePosition = WebPoint(event.windowX, event.windowY);
  render_view()->ShowContextMenu(NULL, menu_data);
  g_last_active_menu = this;
}

bool BlockedPlugin::OnMessageReceived(const IPC::Message& message) {
  // We don't swallow ViewMsg_CustomContextMenuAction because we listen for all
  // custom menu IDs, and not just our own. We don't swallow
  // ViewMsg_LoadBlockedPlugins because multiple blocked plugins have an
  // interest in it.
  IPC_BEGIN_MESSAGE_MAP(BlockedPlugin, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_LoadBlockedPlugins, OnLoadBlockedPlugins)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetIsPrerendering, OnSetIsPrerendering)
  IPC_END_MESSAGE_MAP()

  return false;
}

void BlockedPlugin::ContextMenuAction(unsigned id) {
  if (g_last_active_menu != this)
    return;
  if (id == kMenuActionLoad) {
    RenderThread::Get()->RecordUserMetrics("Plugin_Load_Menu");
    LoadPlugin();
  } else if (id == kMenuActionRemove) {
    RenderThread::Get()->RecordUserMetrics("Plugin_Hide_Menu");
    HidePlugin();
  }
}

void BlockedPlugin::OnLoadBlockedPlugins() {
  RenderThread::Get()->RecordUserMetrics("Plugin_Load_UI");
  LoadPlugin();
}

void BlockedPlugin::OnSetIsPrerendering(bool is_prerendering) {
  // Prerendering can only be enabled prior to a RenderView's first navigation,
  // so no BlockedPlugin should see the notification that enables prerendering.
  DCHECK(!is_prerendering);
  if (is_blocked_for_prerendering_ && !is_prerendering)
    LoadPlugin();
}

void BlockedPlugin::LoadPlugin() {
  CHECK(plugin_);
  // This is not strictly necessary but is an important defense in case the
  // event propagation changes between "close" vs. "click-to-play".
  if (hidden_)
    return;
  if (!allow_loading_)
    return;
  WebPluginContainer* container = plugin_->container();
  WebPlugin* new_plugin =
      render_view()->CreatePlugin(frame_, plugin_info_, plugin_params_);
  if (new_plugin && new_plugin->initialize(container)) {
    plugin_->RestoreTitleText();
    container->setPlugin(new_plugin);
    container->invalidate();
    container->reportGeometry();
    plugin_->ReplayReceivedData(new_plugin);
    plugin_->destroy();
  } else {
    MissingPluginReporter::GetInstance()->ReportPluginMissing(
        plugin_params_.mimeType.utf8(),
        plugin_params_.url);
  }
}

void BlockedPlugin::LoadCallback(const CppArgumentList& args,
                                 CppVariant* result) {
  RenderThread::Get()->RecordUserMetrics("Plugin_Load_Click");
  LoadPlugin();
}

void BlockedPlugin::HideCallback(const CppArgumentList& args,
                                 CppVariant* result) {
  RenderThread::Get()->RecordUserMetrics("Plugin_Hide_Click");
  HidePlugin();
}

void BlockedPlugin::OpenUrlCallback(const CppArgumentList& args,
                                    CppVariant* result) {
  if (args.size() < 1) {
    NOTREACHED();
    return;
  }
  if (!args[0].isString()) {
    NOTREACHED();
    return;
  }

  GURL url(args[0].ToString());
  WebURLRequest request;
  request.initialize();
  request.setURL(url);
  render_view()->LoadURLExternally(
      frame_, request, WebKit::WebNavigationPolicyNewForegroundTab);
}

void BlockedPlugin::HidePlugin() {
  CHECK(plugin_);
  hidden_ = true;
  WebPluginContainer* container = plugin_->container();
  WebElement element = container->element();
  element.setAttribute("style", "display: none;");
  // If we have a width and height, search for a parent (often <div>) with the
  // same dimensions. If we find such a parent, hide that as well.
  // This makes much more uncovered page content usable (including clickable)
  // as opposed to merely visible.
  // TODO(cevans) -- it's a foul heurisitc but we're going to tolerate it for
  // now for these reasons:
  // 1) Makes the user experience better.
  // 2) Foulness is encapsulated within this single function.
  // 3) Confidence in no fasle positives.
  // 4) Seems to have a good / low false negative rate at this time.
  if (element.hasAttribute("width") && element.hasAttribute("height")) {
    std::string width_str("width:[\\s]*");
    width_str += element.getAttribute("width").utf8().data();
    if (EndsWith(width_str, "px", false)) {
      width_str = width_str.substr(0, width_str.length() - 2);
    }
    TrimWhitespace(width_str, TRIM_TRAILING, &width_str);
    width_str += "[\\s]*px";
    WebRegularExpression width_regex(WebString::fromUTF8(width_str.c_str()),
                                     WebKit::WebTextCaseSensitive);
    std::string height_str("height:[\\s]*");
    height_str += element.getAttribute("height").utf8().data();
    if (EndsWith(height_str, "px", false)) {
      height_str = height_str.substr(0, height_str.length() - 2);
    }
    TrimWhitespace(height_str, TRIM_TRAILING, &height_str);
    height_str += "[\\s]*px";
    WebRegularExpression height_regex(WebString::fromUTF8(height_str.c_str()),
                                      WebKit::WebTextCaseSensitive);
    WebNode parent = element;
    while (!parent.parentNode().isNull()) {
      parent = parent.parentNode();
      if (!parent.isElementNode())
        continue;
      element = parent.toConst<WebElement>();
      if (element.hasAttribute("style")) {
        WebString style_str = element.getAttribute("style");
        if (width_regex.match(style_str) >= 0 &&
            height_regex.match(style_str) >= 0)
          element.setAttribute("style", "display: none;");
      }
    }
  }
}
