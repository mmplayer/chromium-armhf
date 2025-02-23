// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_loader.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/history/history_marshaling.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/instant/instant_loader_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/constrained_window_tab_helper_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper_delegate.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/navigation_details.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/provisional_load_details.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/common/notification_details.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"
#include "content/common/renderer_preferences.h"
#include "content/public/browser/notification_types.h"
#include "net/http/http_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"

namespace {

// Number of ms to delay before updating the omnibox bounds. This is only used
// when the bounds of the omnibox shrinks. If the bounds grows, we update
// immediately.
const int kUpdateBoundsDelayMS = 1000;

// If this status code is seen instant is disabled for the specified host.
const int kHostBlacklistStatusCode = 403;

enum PreviewUsageType {
  PREVIEW_CREATED,
  PREVIEW_DELETED,
  PREVIEW_LOADED,
  PREVIEW_SHOWN,
  PREVIEW_COMMITTED,
  PREVIEW_NUM_TYPES,
};

void AddPreviewUsageForHistogram(TemplateURLID template_url_id,
                                 PreviewUsageType usage) {
  DCHECK(0 <= usage && usage < PREVIEW_NUM_TYPES);
  // Only track the histogram for the instant loaders, for now.
  if (template_url_id)
    UMA_HISTOGRAM_ENUMERATION("Instant.Previews", usage, PREVIEW_NUM_TYPES);
}

}  // namespace

// static
const char* const InstantLoader::kInstantHeader = "X-Purpose";
// static
const char* const InstantLoader::kInstantHeaderValue = "instant";

// FrameLoadObserver is responsible for determining if the page supports
// instant after it has loaded.
class InstantLoader::FrameLoadObserver : public NotificationObserver {
 public:
  FrameLoadObserver(InstantLoader* loader,
                    TabContents* tab_contents,
                    const string16& text,
                    bool verbatim)
      : loader_(loader),
        tab_contents_(tab_contents),
        text_(text),
        verbatim_(verbatim),
        unique_id_(tab_contents_->controller().pending_entry()->unique_id()) {
    registrar_.Add(this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                   Source<TabContents>(tab_contents_));
  }

  // Sets the text to send to the page.
  void set_text(const string16& text) { text_ = text; }

  // Sets whether verbatim results are obtained rather than predictive.
  void set_verbatim(bool verbatim) { verbatim_ = verbatim; }

  // NotificationObserver:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  InstantLoader* loader_;

  // The TabContents we're listening for changes on.
  TabContents* tab_contents_;

  // Text to send down to the page.
  string16 text_;

  // Whether verbatim results are obtained.
  bool verbatim_;

  // unique_id of the NavigationEntry we're waiting on.
  const int unique_id_;

  // Registers and unregisters us for notifications.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(FrameLoadObserver);
};

void InstantLoader::FrameLoadObserver::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME: {
      int page_id = *(Details<int>(details).ptr());
      NavigationEntry* active_entry =
          tab_contents_->controller().GetActiveEntry();
      if (!active_entry || active_entry->page_id() != page_id ||
          active_entry->unique_id() != unique_id_) {
        return;
      }
      loader_->SendBoundsToPage(true);
      // TODO: support real cursor position.
      int text_length = static_cast<int>(text_.size());
      RenderViewHost* host = tab_contents_->render_view_host();
      host->Send(new ChromeViewMsg_DetermineIfPageSupportsInstant(
          host->routing_id(), text_, verbatim_, text_length, text_length));
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

// TabContentsDelegateImpl -----------------------------------------------------

class InstantLoader::TabContentsDelegateImpl
    : public TabContentsDelegate,
      public TabContentsWrapperDelegate,
      public ConstrainedWindowTabHelperDelegate,
      public NotificationObserver,
      public TabContentsObserver {
 public:
  explicit TabContentsDelegateImpl(InstantLoader* loader);

  // Invoked prior to loading a new URL.
  void PrepareForNewLoad();

  // Invoked when the preview paints. Invokes PreviewPainted on the loader.
  void PreviewPainted();

  bool is_mouse_down_from_activate() const {
    return is_mouse_down_from_activate_;
  }

  void set_user_typed_before_load() { user_typed_before_load_ = true; }

  // Sets the last URL that will be added to history when CommitHistory is
  // invoked and removes all but the first navigation.
  void SetLastHistoryURLAndPrune(const GURL& url);

  // Commits the currently buffered history.
  void CommitHistory(bool supports_instant);

  void RegisterForPaintNotifications(RenderWidgetHost* render_widget_host);

  void UnregisterForPaintNotifications();

  // NotificationObserver:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // TabContentsDelegate:
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) OVERRIDE;
  virtual void AddNavigationHeaders(const GURL& url,
                                    std::string* headers) OVERRIDE;
  virtual bool ShouldSuppressDialogs() OVERRIDE;
  virtual void BeforeUnloadFired(TabContents* tab,
                                 bool proceed,
                                 bool* proceed_to_fire_unload) OVERRIDE;
  virtual void SetFocusToLocationBar(bool select_all) OVERRIDE;
  virtual bool ShouldFocusPageAfterCrash() OVERRIDE;
  virtual void LostCapture() OVERRIDE;
  // If the user drags, we won't get a mouse up (at least on Linux). Commit the
  // instant result when the drag ends, so that during the drag the page won't
  // move around.
  virtual void DragEnded() OVERRIDE;
  virtual bool CanDownload(TabContents* source, int request_id) OVERRIDE;
  virtual void HandleMouseUp() OVERRIDE;
  virtual void HandleMouseActivate() OVERRIDE;
  virtual bool OnGoToEntryOffset(int offset) OVERRIDE;
  virtual bool ShouldAddNavigationToHistory(
      const history::HistoryAddPageArgs& add_page_args,
      content::NavigationType navigation_type) OVERRIDE;

  // TabContentsWrapperDelegate:
  virtual void SwapTabContents(TabContentsWrapper* old_tc,
                               TabContentsWrapper* new_tc) OVERRIDE;

  // ConstrainedWindowTabHelperDelegate:
  virtual void WillShowConstrainedWindow(TabContentsWrapper* source) OVERRIDE;
  virtual bool ShouldFocusConstrainedWindow() OVERRIDE;

  // TabContentsObserver:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  typedef std::vector<scoped_refptr<history::HistoryAddPageArgs> >
      AddPageVector;

  // Message from renderer indicating the page has suggestions.
  void OnSetSuggestions(
      int32 page_id,
      const std::vector<std::string>& suggestions,
      InstantCompleteBehavior behavior);

  // Messages from the renderer when we've determined whether the page supports
  // instant.
  void OnInstantSupportDetermined(int32 page_id, bool result);

  void CommitFromMouseReleaseIfNecessary();

  InstantLoader* loader_;

  NotificationRegistrar registrar_;

  // If we are registered for paint notifications on a RenderWidgetHost this
  // will contain a pointer to it.
  RenderWidgetHost* registered_render_widget_host_;

  // Used to cache data that needs to be added to history. Normally entries are
  // added to history as the user types, but for instant we only want to add the
  // items to history if the user commits instant. So, we cache them here and if
  // committed then add the items to history.
  AddPageVector add_page_vector_;

  // Are we we waiting for a NavigationType of NEW_PAGE? If we're waiting for
  // NEW_PAGE navigation we don't add history items to add_page_vector_.
  bool waiting_for_new_page_;

  // True if the mouse is down from an activate.
  bool is_mouse_down_from_activate_;

  // True if the user typed in the search box before the page loaded.
  bool user_typed_before_load_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsDelegateImpl);
};

InstantLoader::TabContentsDelegateImpl::TabContentsDelegateImpl(
    InstantLoader* loader)
    : TabContentsObserver(loader->preview_contents()->tab_contents()),
      loader_(loader),
      registered_render_widget_host_(NULL),
      waiting_for_new_page_(true),
      is_mouse_down_from_activate_(false),
      user_typed_before_load_(false) {
  DCHECK(loader->preview_contents());
  registrar_.Add(this, content::NOTIFICATION_INTERSTITIAL_ATTACHED,
      Source<TabContents>(loader->preview_contents()->tab_contents()));
  registrar_.Add(this, content::NOTIFICATION_FAIL_PROVISIONAL_LOAD_WITH_ERROR,
      Source<NavigationController>(&loader->preview_contents()->controller()));
}

void InstantLoader::TabContentsDelegateImpl::PrepareForNewLoad() {
  user_typed_before_load_ = false;
  waiting_for_new_page_ = true;
  add_page_vector_.clear();
  UnregisterForPaintNotifications();
}

void InstantLoader::TabContentsDelegateImpl::PreviewPainted() {
  loader_->PreviewPainted();
}

void InstantLoader::TabContentsDelegateImpl::SetLastHistoryURLAndPrune(
    const GURL& url) {
  if (add_page_vector_.empty())
    return;

  history::HistoryAddPageArgs* args = add_page_vector_.front().get();
  args->url = url;
  args->redirects.clear();
  args->redirects.push_back(url);

  // Prune all but the first entry.
  add_page_vector_.erase(add_page_vector_.begin() + 1,
                         add_page_vector_.end());
}

void InstantLoader::TabContentsDelegateImpl::CommitHistory(
    bool supports_instant) {
  TabContentsWrapper* tab = loader_->preview_contents();
  if (tab->profile()->IsOffTheRecord())
    return;

  for (size_t i = 0; i < add_page_vector_.size(); ++i) {
    tab->history_tab_helper()->UpdateHistoryForNavigation(
      add_page_vector_[i].get());
  }

  NavigationEntry* active_entry =
      tab->tab_contents()->controller().GetActiveEntry();
  if (!active_entry) {
    // It appears to be possible to get here with no active entry. This seems
    // to be possible with an auth dialog, but I can't narrow down the
    // circumstances. If you hit this, file a bug with the steps you did and
    // assign it to me (sky).
    NOTREACHED();
    return;
  }
  tab->history_tab_helper()->UpdateHistoryPageTitle(*active_entry);

  FaviconService* favicon_service =
      tab->profile()->GetFaviconService(Profile::EXPLICIT_ACCESS);

  if (favicon_service && active_entry->favicon().is_valid() &&
      !active_entry->favicon().bitmap().empty()) {
    std::vector<unsigned char> image_data;
    gfx::PNGCodec::EncodeBGRASkBitmap(active_entry->favicon().bitmap(), false,
                                      &image_data);
    favicon_service->SetFavicon(active_entry->url(),
                                active_entry->favicon().url(),
                                image_data,
                                history::FAVICON);
    if (supports_instant && !add_page_vector_.empty()) {
      // If we're using the instant API, then we've tweaked the url that is
      // going to be added to history. We need to also set the favicon for the
      // url we're adding to history (see comment in ReleasePreviewContents
      // for details).
      favicon_service->SetFavicon(add_page_vector_.back()->url,
                                  active_entry->favicon().url(),
                                  image_data,
                                  history::FAVICON);
    }
  }
}

void InstantLoader::TabContentsDelegateImpl::RegisterForPaintNotifications(
    RenderWidgetHost* render_widget_host) {
  DCHECK(registered_render_widget_host_ == NULL);
  registered_render_widget_host_ = render_widget_host;
  Source<RenderWidgetHost> source =
      Source<RenderWidgetHost>(registered_render_widget_host_);
  registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_HOST_DID_PAINT,
                 source);
  registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
                 source);
}

void InstantLoader::TabContentsDelegateImpl::UnregisterForPaintNotifications() {
  if (registered_render_widget_host_) {
    Source<RenderWidgetHost> source =
        Source<RenderWidgetHost>(registered_render_widget_host_);
    registrar_.Remove(this, content::NOTIFICATION_RENDER_WIDGET_HOST_DID_PAINT,
                      source);
    registrar_.Remove(this, content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
                      source);
    registered_render_widget_host_ = NULL;
  }
}

void InstantLoader::TabContentsDelegateImpl::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_FAIL_PROVISIONAL_LOAD_WITH_ERROR:
      if (Details<ProvisionalLoadDetails>(details)->url() == loader_->url_) {
        // This typically happens with downloads (which are disabled with
        // instant active). To ensure the download happens when the user presses
        // enter we set needs_reload_ to true, which triggers a reload.
        loader_->needs_reload_ = true;
      }
      break;
    case content::NOTIFICATION_RENDER_WIDGET_HOST_DID_PAINT:
      UnregisterForPaintNotifications();
      PreviewPainted();
      break;
    case content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED:
      UnregisterForPaintNotifications();
      break;
    case content::NOTIFICATION_INTERSTITIAL_ATTACHED:
      PreviewPainted();
      break;
    default:
      NOTREACHED() << "Got a notification we didn't register for.";
  }
}

void InstantLoader::TabContentsDelegateImpl::NavigationStateChanged(
    const TabContents* source,
    unsigned changed_flags) {
  if (!loader_->ready() && !registered_render_widget_host_ &&
      source->controller().entry_count()) {
    // The load has been committed. Install an observer that waits for the
    // first paint then makes the preview active. We wait for the load to be
    // committed before waiting on paint as there is always an initial paint
    // when a new renderer is created from the resize so that if we showed the
    // preview after the first paint we would end up with a white rect.
    RenderWidgetHostView *rwhv = source->GetRenderWidgetHostView();
    if (rwhv)
      RegisterForPaintNotifications(rwhv->GetRenderWidgetHost());
  } else if (source->is_crashed()) {
    PreviewPainted();
  }
}

void InstantLoader::TabContentsDelegateImpl::AddNavigationHeaders(
    const GURL& url,
    std::string* headers) {
  net::HttpUtil::AppendHeaderIfMissing(kInstantHeader, kInstantHeaderValue,
                                       headers);
}

bool InstantLoader::TabContentsDelegateImpl::ShouldSuppressDialogs() {
  // Any message shown during instant cancels instant, so we suppress them.
  return true;
}

void InstantLoader::TabContentsDelegateImpl::BeforeUnloadFired(
    TabContents* tab,
    bool proceed,
    bool* proceed_to_fire_unload) {
}

void InstantLoader::TabContentsDelegateImpl::SetFocusToLocationBar(
    bool select_all) {
}

bool InstantLoader::TabContentsDelegateImpl::ShouldFocusPageAfterCrash() {
  return false;
}

void InstantLoader::TabContentsDelegateImpl::LostCapture() {
  CommitFromMouseReleaseIfNecessary();
}

void InstantLoader::TabContentsDelegateImpl::DragEnded() {
  CommitFromMouseReleaseIfNecessary();
}

bool InstantLoader::TabContentsDelegateImpl::CanDownload(TabContents* source,
                                                         int request_id) {
  // Downloads are disabled.
  return false;
}

void InstantLoader::TabContentsDelegateImpl::HandleMouseUp() {
  CommitFromMouseReleaseIfNecessary();
}

void InstantLoader::TabContentsDelegateImpl::HandleMouseActivate() {
  is_mouse_down_from_activate_ = true;
}

bool InstantLoader::TabContentsDelegateImpl::OnGoToEntryOffset(int offset) {
  return false;
}

bool InstantLoader::TabContentsDelegateImpl::ShouldAddNavigationToHistory(
    const history::HistoryAddPageArgs& add_page_args,
    content::NavigationType navigation_type) {
  if (waiting_for_new_page_ &&
      navigation_type == content::NAVIGATION_TYPE_NEW_PAGE) {
    waiting_for_new_page_ = false;
  }

  if (!waiting_for_new_page_) {
    add_page_vector_.push_back(
        scoped_refptr<history::HistoryAddPageArgs>(add_page_args.Clone()));
  }
  return false;
}

// If this is being called, something is swapping in to our preview_contents_
// before we've added it to the tab strip.
void InstantLoader::TabContentsDelegateImpl::SwapTabContents(
    TabContentsWrapper* old_tc,
    TabContentsWrapper* new_tc) {
  loader_->ReplacePreviewContents(old_tc, new_tc);
}

bool InstantLoader::TabContentsDelegateImpl::ShouldFocusConstrainedWindow() {
  // Return false so that constrained windows are not initially focused. If
  // we did otherwise the preview would prematurely get committed when focus
  // goes to the constrained window.
  return false;
}

void InstantLoader::TabContentsDelegateImpl::WillShowConstrainedWindow(
    TabContentsWrapper* source) {
  if (!loader_->ready()) {
    // A constrained window shown for an auth may not paint. Show the preview
    // contents.
    UnregisterForPaintNotifications();
    loader_->ShowPreview();
  }
}

bool InstantLoader::TabContentsDelegateImpl::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TabContentsDelegateImpl, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SetSuggestions, OnSetSuggestions)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_InstantSupportDetermined,
                        OnInstantSupportDetermined)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void InstantLoader::TabContentsDelegateImpl::OnSetSuggestions(
    int32 page_id,
    const std::vector<std::string>& suggestions,
    InstantCompleteBehavior behavior) {
  TabContentsWrapper* source = loader_->preview_contents();
  if (!source->controller().GetActiveEntry() ||
      page_id != source->controller().GetActiveEntry()->page_id())
    return;

  if (suggestions.empty())
    loader_->SetCompleteSuggestedText(string16(), behavior);
  else
    loader_->SetCompleteSuggestedText(UTF8ToUTF16(suggestions[0]), behavior);
}

void InstantLoader::TabContentsDelegateImpl::OnInstantSupportDetermined(
    int32 page_id,
    bool result) {
  TabContents* source = loader_->preview_contents()->tab_contents();
  if (!source->controller().GetActiveEntry() ||
      page_id != source->controller().GetActiveEntry()->page_id())
    return;

  Details<const bool> details(&result);
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_INSTANT_SUPPORT_DETERMINED,
      NotificationService::AllSources(),
      details);

  if (result)
    loader_->PageFinishedLoading();
  else
    loader_->PageDoesntSupportInstant(user_typed_before_load_);

  AddPreviewUsageForHistogram(loader_->template_url_id(), PREVIEW_LOADED);
}

void InstantLoader::TabContentsDelegateImpl
    ::CommitFromMouseReleaseIfNecessary() {
  bool was_down = is_mouse_down_from_activate_;
  is_mouse_down_from_activate_ = false;
  if (was_down && loader_->ShouldCommitInstantOnMouseUp())
    loader_->CommitInstantLoader();
}

// InstantLoader ---------------------------------------------------------------

InstantLoader::InstantLoader(InstantLoaderDelegate* delegate, TemplateURLID id)
    : delegate_(delegate),
      template_url_id_(id),
      ready_(false),
      http_status_ok_(true),
      last_transition_type_(content::PAGE_TRANSITION_LINK),
      verbatim_(false),
      needs_reload_(false) {
}

InstantLoader::~InstantLoader() {
  registrar_.RemoveAll();

  // Delete the TabContents before the delegate as the TabContents holds a
  // reference to the delegate.
  if (preview_contents())
    AddPreviewUsageForHistogram(template_url_id_, PREVIEW_DELETED);
  preview_contents_.reset();
}

bool InstantLoader::Update(TabContentsWrapper* tab_contents,
                           const TemplateURL* template_url,
                           const GURL& url,
                           content::PageTransition transition_type,
                           const string16& user_text,
                           bool verbatim,
                           string16* suggested_text) {
  DCHECK(!url.is_empty() && url.is_valid());

  // Strip leading ?.
  string16 new_user_text =
      !user_text.empty() && (UTF16ToWide(user_text)[0] == L'?') ?
      user_text.substr(1) : user_text;

  // We should preserve the transition type regardless of whether we're already
  // showing the url.
  last_transition_type_ = transition_type;

  // If state hasn't changed, reuse the last suggestion. There are two cases:
  // 1. If no template url (not using instant API), then we only care if the url
  //    changes.
  // 2. Template url (using instant API) then the important part is if the
  //    user_text changes.
  //    We have to be careful in checking user_text as in some situations
  //    InstantController passes in an empty string (when it knows the user_text
  //    won't matter).
  if ((!template_url_id_ && url_ == url) ||
      (template_url_id_ &&
       (new_user_text.empty() || user_text_ == new_user_text))) {
    suggested_text->assign(last_suggestion_);
    // Track the url even if we're not going to update. This is important as
    // when we get the suggest text we set user_text_ to the new suggest text,
    // but yet the url is much different.
    url_ = url;
    return false;
  }

  url_ = url;
  user_text_ = new_user_text;
  verbatim_ = verbatim;
  last_suggestion_.clear();
  needs_reload_ = false;

  bool created_preview_contents = preview_contents_.get() == NULL;
  if (created_preview_contents)
    CreatePreviewContents(tab_contents);

  if (template_url) {
    DCHECK(template_url_id_ == template_url->id());
    if (!created_preview_contents) {
      if (is_determining_if_page_supports_instant()) {
        // The page hasn't loaded yet. We'll send the script down when it does.
        frame_load_observer_->set_text(user_text_);
        frame_load_observer_->set_verbatim(verbatim);
        preview_tab_contents_delegate_->set_user_typed_before_load();
        return true;
      }
      // TODO: support real cursor position.
      int text_length = static_cast<int>(user_text_.size());
      RenderViewHost* host = preview_contents_->render_view_host();
      host->Send(new ChromeViewMsg_SearchBoxChange(
          host->routing_id(), user_text_, verbatim, text_length, text_length));

      string16 complete_suggested_text_lower = base::i18n::ToLower(
          complete_suggested_text_);
      string16 user_text_lower = base::i18n::ToLower(user_text_);
      if (!verbatim &&
          complete_suggested_text_lower.size() > user_text_lower.size() &&
          !complete_suggested_text_lower.compare(0, user_text_lower.size(),
                                                 user_text_lower)) {
        *suggested_text = last_suggestion_ =
            complete_suggested_text_.substr(user_text_.size());
      }
    } else {
      LoadInstantURL(tab_contents, template_url, transition_type, user_text_,
                     verbatim);
    }
  } else {
    DCHECK(template_url_id_ == 0);
    preview_tab_contents_delegate_->PrepareForNewLoad();
    frame_load_observer_.reset(NULL);
    preview_contents_->controller().LoadURL(url_, GURL(), transition_type,
                                            std::string());
  }
  return true;
}

void InstantLoader::SetOmniboxBounds(const gfx::Rect& bounds) {
  if (omnibox_bounds_ == bounds)
    return;

  // Don't update the page while the mouse is down. http://crbug.com/71952
  if (IsMouseDownFromActivate())
    return;

  omnibox_bounds_ = bounds;
  if (preview_contents_.get() && is_showing_instant() &&
      !is_determining_if_page_supports_instant()) {
    // Updating the bounds is rather expensive, and because of the async nature
    // of the omnibox the bounds can dance around a bit. Delay the update in
    // hopes of things settling down. To avoid hiding results we grow
    // immediately, but delay shrinking.
    update_bounds_timer_.Stop();
    if (omnibox_bounds_.height() > last_omnibox_bounds_.height()) {
      SendBoundsToPage(false);
    } else {
      update_bounds_timer_.Start(FROM_HERE,
          base::TimeDelta::FromMilliseconds(kUpdateBoundsDelayMS),
          this, &InstantLoader::ProcessBoundsChange);
    }
  }
}

bool InstantLoader::IsMouseDownFromActivate() {
  return preview_tab_contents_delegate_.get() &&
      preview_tab_contents_delegate_->is_mouse_down_from_activate();
}

TabContentsWrapper* InstantLoader::ReleasePreviewContents(
    InstantCommitType type) {
  if (!preview_contents_.get())
    return NULL;

  // FrameLoadObserver is only used for instant results, and instant results are
  // only committed if active (when the FrameLoadObserver isn't installed).
  DCHECK(type == INSTANT_COMMIT_DESTROY || !frame_load_observer_.get());

  if (type != INSTANT_COMMIT_DESTROY && is_showing_instant()) {
    RenderViewHost* host = preview_contents_->render_view_host();
    if (type == INSTANT_COMMIT_FOCUS_LOST) {
      host->Send(new ChromeViewMsg_SearchBoxCancel(host->routing_id()));
    } else {
      host->Send(new ChromeViewMsg_SearchBoxSubmit(
          host->routing_id(), user_text_,
          type == INSTANT_COMMIT_PRESSED_ENTER));
    }
  }
  omnibox_bounds_ = gfx::Rect();
  last_omnibox_bounds_ = gfx::Rect();
  GURL url;
  url.Swap(&url_);
  user_text_.clear();
  complete_suggested_text_.clear();
  if (preview_contents_.get()) {
    if (type != INSTANT_COMMIT_DESTROY) {
      if (template_url_id_) {
        // The URL used during instant is mostly gibberish, and not something
        // we'll parse and match as a past search. Set it to something we can
        // parse.
        preview_tab_contents_delegate_->SetLastHistoryURLAndPrune(url);
      }
      preview_tab_contents_delegate_->CommitHistory(template_url_id_ != 0);
    }
    if (preview_contents_->tab_contents()->GetRenderWidgetHostView()) {
#if defined(OS_MACOSX)
      preview_contents_->tab_contents()->GetRenderWidgetHostView()->
          SetTakesFocusOnlyOnMouseDown(false);
      registrar_.Remove(
          this,
          content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
          Source<NavigationController>(&preview_contents_->controller()));
#endif
    }
    preview_contents_->tab_contents()->set_delegate(NULL);
    ready_ = false;
  }
  update_bounds_timer_.Stop();
  AddPreviewUsageForHistogram(template_url_id_,
      type == INSTANT_COMMIT_DESTROY ? PREVIEW_DELETED : PREVIEW_COMMITTED);
  return preview_contents_.release();
}

bool InstantLoader::ShouldCommitInstantOnMouseUp() {
  return delegate_->ShouldCommitInstantOnMouseUp();
}

void InstantLoader::CommitInstantLoader() {
  delegate_->CommitInstantLoader(this);
}

void InstantLoader::MaybeLoadInstantURL(TabContentsWrapper* tab_contents,
                                        const TemplateURL* template_url) {
  DCHECK(template_url_id_ == template_url->id());

  // If we already have a |preview_contents_|, future search queries will be
  // issued into it (see the "if (!created_preview_contents)" block in |Update|
  // above), so there is no need to load the |template_url|'s instant URL.
  if (preview_contents_.get())
    return;

  CreatePreviewContents(tab_contents);
  LoadInstantURL(tab_contents, template_url, content::PAGE_TRANSITION_GENERATED,
                 string16(), true);
}

bool InstantLoader::IsNavigationPending() const {
  return preview_contents_.get() &&
      preview_contents_->controller().pending_entry();
}

void InstantLoader::Observe(int type,
                            const NotificationSource& source,
                            const NotificationDetails& details) {
#if defined(OS_MACOSX)
  if (type == content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED) {
    if (preview_contents_->tab_contents()->GetRenderWidgetHostView()) {
      preview_contents_->tab_contents()->GetRenderWidgetHostView()->
          SetTakesFocusOnlyOnMouseDown(true);
    }
    return;
  }
#endif
  if (type == content::NOTIFICATION_NAV_ENTRY_COMMITTED) {
    content::LoadCommittedDetails* load_details =
        Details<content::LoadCommittedDetails>(details).ptr();
    if (load_details->is_main_frame) {
      if (load_details->http_status_code == kHostBlacklistStatusCode) {
        delegate_->AddToBlacklist(this, load_details->entry->url());
      } else {
        SetHTTPStatusOK(load_details->http_status_code == 200);
      }
    }
    return;
  }

  NOTREACHED() << "Got a notification we didn't register for.";
}

void InstantLoader::SetCompleteSuggestedText(
    const string16& complete_suggested_text,
    InstantCompleteBehavior behavior) {
  if (!is_showing_instant()) {
    // We're not trying to use the instant API with this page. Ignore it.
    return;
  }

  ShowPreview();

  if (complete_suggested_text == complete_suggested_text_)
    return;

  if (verbatim_) {
    // Don't show suggest results for verbatim queries.
    return;
  }

  string16 user_text_lower = base::i18n::ToLower(user_text_);
  string16 complete_suggested_text_lower = base::i18n::ToLower(
      complete_suggested_text);
  last_suggestion_.clear();
  if (user_text_lower.compare(0, user_text_lower.size(),
                              complete_suggested_text_lower,
                              0, user_text_lower.size())) {
    // The user text no longer contains the suggested text, ignore it.
    complete_suggested_text_.clear();
    delegate_->SetSuggestedTextFor(this, string16(), behavior);
    return;
  }

  complete_suggested_text_ = complete_suggested_text;
  if (behavior == INSTANT_COMPLETE_NOW) {
    // We are effectively showing complete_suggested_text_ now. Update
    // user_text_ so we don't notify the page again if Update happens to be
    // invoked (which is more than likely if this callback completes before the
    // omnibox is done).
    string16 suggestion = complete_suggested_text_.substr(user_text_.size());
    user_text_ = complete_suggested_text_;
    delegate_->SetSuggestedTextFor(this, suggestion, behavior);
  } else {
    DCHECK((behavior == INSTANT_COMPLETE_DELAYED) ||
           (behavior == INSTANT_COMPLETE_NEVER));
    last_suggestion_ = complete_suggested_text_.substr(user_text_.size());
    delegate_->SetSuggestedTextFor(this, last_suggestion_, behavior);
  }
}

void InstantLoader::PreviewPainted() {
  // If instant is supported then we wait for the first suggest result before
  // showing the page.
  if (!template_url_id_)
    ShowPreview();
}

void InstantLoader::SetHTTPStatusOK(bool is_ok) {
  if (is_ok == http_status_ok_)
    return;

  http_status_ok_ = is_ok;
  if (ready_)
    delegate_->InstantStatusChanged(this);
}

void InstantLoader::ShowPreview() {
  if (!ready_) {
    ready_ = true;
    delegate_->InstantStatusChanged(this);
    AddPreviewUsageForHistogram(template_url_id_, PREVIEW_SHOWN);
  }
}

void InstantLoader::PageFinishedLoading() {
  frame_load_observer_.reset();

  // Send the bounds of the omnibox down now.
  SendBoundsToPage(false);

  // Wait for the user input before showing, this way the page should be up to
  // date by the time we show it.
}

// TODO(tonyg): This method only fires when the omnibox bounds change. It also
// needs to fire when the preview bounds change (e.g. open/close info bar).
gfx::Rect InstantLoader::GetOmniboxBoundsInTermsOfPreview() {
  gfx::Rect preview_bounds(delegate_->GetInstantBounds());
  gfx::Rect intersection(omnibox_bounds_.Intersect(preview_bounds));

  // Translate into window's coordinates.
  if (!intersection.IsEmpty()) {
    intersection.Offset(-preview_bounds.origin().x(),
                        -preview_bounds.origin().y());
  }

  // In the current Chrome UI, these must always be true so they sanity check
  // the above operations. In a future UI, these may be removed or adjusted.
  // There is no point in sanity-checking |intersection.y()| because the omnibox
  // can be placed anywhere vertically relative to the preview (for example, in
  // Mac fullscreen mode, the omnibox is entirely enclosed by the preview
  // bounds).
  DCHECK_LE(0, intersection.x());
  DCHECK_LE(0, intersection.width());
  DCHECK_LE(0, intersection.height());

  return intersection;
}

void InstantLoader::PageDoesntSupportInstant(bool needs_reload) {
  frame_load_observer_.reset(NULL);

  delegate_->InstantLoaderDoesntSupportInstant(this);
}

void InstantLoader::ProcessBoundsChange() {
  SendBoundsToPage(false);
}

void InstantLoader::SendBoundsToPage(bool force_if_waiting) {
  if (last_omnibox_bounds_ == omnibox_bounds_)
    return;

  if (preview_contents_.get() && is_showing_instant() &&
      (force_if_waiting || !is_determining_if_page_supports_instant())) {
    last_omnibox_bounds_ = omnibox_bounds_;
    RenderViewHost* host = preview_contents_->render_view_host();
    host->Send(new ChromeViewMsg_SearchBoxResize(
        host->routing_id(), GetOmniboxBoundsInTermsOfPreview()));
  }
}

void InstantLoader::ReplacePreviewContents(TabContentsWrapper* old_tc,
                                           TabContentsWrapper* new_tc) {
  DCHECK(old_tc == preview_contents_);
  // We release here without deleting so that the caller still has reponsibility
  // for deleting the TabContentsWrapper.
  ignore_result(preview_contents_.release());
  preview_contents_.reset(new_tc);

  // Make sure the new preview contents acts like the old one.
  SetupPreviewContents(old_tc);

  // Cleanup the old preview contents.
  old_tc->constrained_window_tab_helper()->set_delegate(NULL);
  old_tc->tab_contents()->set_delegate(NULL);
  old_tc->set_delegate(NULL);

#if defined(OS_MACOSX)
  registrar_.Remove(this,
                    content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
                    Source<NavigationController>(&old_tc->controller()));
#endif
  registrar_.Remove(this,
                 content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 Source<NavigationController>(&old_tc->controller()));

  // We prerendered so we should be ready to show. If we're ready, swap in
  // immediately, otherwise show the preview as normal.
  if (ready_)
    delegate_->SwappedTabContents(this);
  else
    ShowPreview();
}

void InstantLoader::SetupPreviewContents(TabContentsWrapper* tab_contents) {
  preview_contents_->set_delegate(preview_tab_contents_delegate_.get());
  preview_contents_->tab_contents()->set_delegate(
      preview_tab_contents_delegate_.get());
  preview_contents_->blocked_content_tab_helper()->SetAllContentsBlocked(true);
  preview_contents_->constrained_window_tab_helper()->set_delegate(
      preview_tab_contents_delegate_.get());

  // Propagate the max page id. That way if we end up merging the two
  // NavigationControllers (which happens if we commit) none of the page ids
  // will overlap.
  int32 max_page_id = tab_contents->tab_contents()->GetMaxPageID();
  if (max_page_id != -1)
    preview_contents_->controller().set_max_restored_page_id(max_page_id + 1);

#if defined(OS_MACOSX)
  // If |preview_contents_| does not currently have a RWHV, we will call
  // SetTakesFocusOnlyOnMouseDown() as a result of the
  // RENDER_VIEW_HOST_CHANGED notification.
  if (preview_contents_->tab_contents()->GetRenderWidgetHostView()) {
    preview_contents_->tab_contents()->GetRenderWidgetHostView()->
        SetTakesFocusOnlyOnMouseDown(true);
  }
  registrar_.Add(
      this,
      content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
      Source<NavigationController>(&preview_contents_->controller()));
#endif

  registrar_.Add(
      this,
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      Source<NavigationController>(&preview_contents_->controller()));

  gfx::Rect tab_bounds;
  tab_contents->view()->GetContainerBounds(&tab_bounds);
  preview_contents_->view()->SizeContents(tab_bounds.size());
}

void InstantLoader::CreatePreviewContents(TabContentsWrapper* tab_contents) {
  TabContents* new_contents =
      new TabContents(
          tab_contents->profile(), NULL, MSG_ROUTING_NONE, NULL, NULL);
  preview_contents_.reset(new TabContentsWrapper(new_contents));
  AddPreviewUsageForHistogram(template_url_id_, PREVIEW_CREATED);
  preview_tab_contents_delegate_.reset(new TabContentsDelegateImpl(this));
  SetupPreviewContents(tab_contents);

  preview_contents_->tab_contents()->ShowContents();
}

void InstantLoader::LoadInstantURL(TabContentsWrapper* tab_contents,
                                   const TemplateURL* template_url,
                                   content::PageTransition transition_type,
                                   const string16& user_text,
                                   bool verbatim) {
  preview_tab_contents_delegate_->PrepareForNewLoad();

  // Load the instant URL. We don't reflect the url we load in url() as
  // callers expect that we're loading the URL they tell us to.
  //
  // This uses an empty string for the replacement text as the url doesn't
  // really have the search params, but we need to use the replace
  // functionality so that embeded tags (like {google:baseURL}) are escaped
  // correctly.
  // TODO(sky): having to use a replaceable url is a bit of a hack here.
  GURL instant_url(template_url->instant_url()->ReplaceSearchTermsUsingProfile(
      tab_contents->profile(), *template_url, string16(), -1, string16()));
  CommandLine* cl = CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kInstantURL))
    instant_url = GURL(cl->GetSwitchValueASCII(switches::kInstantURL));
  preview_contents_->controller().LoadURL(instant_url, GURL(), transition_type,
                                          std::string());
  RenderViewHost* host = preview_contents_->render_view_host();
  preview_contents_->tab_contents()->HideContents();

  // If user_text is empty, this must be a preload of the search homepage. In
  // that case, send down a SearchBoxResize message, which will switch the page
  // to "search results" UI. This avoids flicker when the page is shown with
  // results. In addition, we don't want the page accidentally causing the
  // preloaded page to be displayed yet (by calling setSuggestions), so don't
  // send a SearchBoxChange message.
  if (user_text.empty()) {
    host->Send(new ChromeViewMsg_SearchBoxResize(
        host->routing_id(), GetOmniboxBoundsInTermsOfPreview()));
  } else {
    host->Send(new ChromeViewMsg_SearchBoxChange(
        host->routing_id(), user_text, verbatim, 0, 0));
  }

  frame_load_observer_.reset(new FrameLoadObserver(
      this, preview_contents()->tab_contents(), user_text, verbatim));
}
