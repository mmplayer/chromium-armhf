// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_AUTOFILL_AUTOFILL_AGENT_H_
#define CHROME_RENDERER_AUTOFILL_AUTOFILL_AGENT_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/renderer/autofill/form_cache.h"
#include "chrome/renderer/page_click_listener.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAutofillClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"

namespace webkit_glue {
struct FormData;
struct FormDataPredictions;
struct FormField;
}

namespace WebKit {
class WebNode;
}

namespace autofill {

class PasswordAutofillManager;

// AutofillAgent deals with Autofill related communications between WebKit and
// the browser.  There is one AutofillAgent per RenderView.
// This code was originally part of RenderView.
// Note that Autofill encompasses:
// - single text field suggestions, that we usually refer to as Autocomplete,
// - password form fill, refered to as Password Autofill, and
// - entire form fill based on one field entry, referred to as Form Autofill.

class AutofillAgent : public content::RenderViewObserver,
                      public PageClickListener,
                      public WebKit::WebAutofillClient {
 public:
  // PasswordAutofillManager is guaranteed to outlive AutofillAgent.
  AutofillAgent(content::RenderView* render_view,
                PasswordAutofillManager* password_autofill_manager);
  virtual ~AutofillAgent();

  // Called when the translate helper has finished translating the page.  We
  // use this signal to re-scan the page for forms.
  void FrameTranslated(WebKit::WebFrame* frame);

 private:
  enum AutofillAction {
    AUTOFILL_NONE,     // No state set.
    AUTOFILL_FILL,     // Fill the Autofill form data.
    AUTOFILL_PREVIEW,  // Preview the Autofill form data.
  };

  // RenderView::Observer:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidFinishDocumentLoad(WebKit::WebFrame* frame) OVERRIDE;
  virtual void FrameDetached(WebKit::WebFrame* frame) OVERRIDE;
  virtual void FrameWillClose(WebKit::WebFrame* frame) OVERRIDE;
  virtual void WillSubmitForm(WebKit::WebFrame* frame,
                              const WebKit::WebFormElement& form) OVERRIDE;

  // PageClickListener:
  virtual bool InputElementClicked(const WebKit::WebInputElement& element,
                                   bool was_focused,
                                   bool is_focused) OVERRIDE;

  // WebKit::WebAutofillClient:
  virtual void didAcceptAutofillSuggestion(const WebKit::WebNode& node,
                                           const WebKit::WebString& value,
                                           const WebKit::WebString& label,
                                           int unique_id,
                                           unsigned index) OVERRIDE;
  virtual void didSelectAutofillSuggestion(const WebKit::WebNode& node,
                                           const WebKit::WebString& value,
                                           const WebKit::WebString& label,
                                           int unique_id) OVERRIDE;
  virtual void didClearAutofillSelection(const WebKit::WebNode& node) OVERRIDE;
  virtual void removeAutocompleteSuggestion(
      const WebKit::WebString& name,
      const WebKit::WebString& value) OVERRIDE;
  virtual void textFieldDidEndEditing(
      const WebKit::WebInputElement& element) OVERRIDE;
  virtual void textFieldDidChange(
      const WebKit::WebInputElement& element) OVERRIDE;
  virtual void textFieldDidReceiveKeyDown(
      const WebKit::WebInputElement& element,
      const WebKit::WebKeyboardEvent& event) OVERRIDE;

  void OnSuggestionsReturned(int query_id,
                             const std::vector<string16>& values,
                             const std::vector<string16>& labels,
                             const std::vector<string16>& icons,
                             const std::vector<int>& unique_ids);
  void OnFormDataFilled(int query_id, const webkit_glue::FormData& form);
  void OnFieldTypePredictionsAvailable(
      const std::vector<webkit_glue::FormDataPredictions>& forms);

  // Called in a posted task by textFieldDidChange() to work-around a WebKit bug
  // http://bugs.webkit.org/show_bug.cgi?id=16976
  void TextFieldDidChangeImpl(const WebKit::WebInputElement& element);

  // Shows the autofill suggestions for |element|.
  // This call is asynchronous and may or may not lead to the showing of a
  // suggestion popup (no popup is shown if there are no available suggestions).
  // |autofill_on_empty_values| specifies whether suggestions should be shown
  // when |element| contains no text.
  // |requires_caret_at_end| specifies whether suggestions should be shown when
  // the caret is not after the last character in |element|.
  // |display_warning_if_disabled| specifies whether a warning should be
  // displayed to the user if Autofill has suggestions available, but cannot
  // fill them because it is disabled (e.g. when trying to fill a credit card
  // form on a non-secure website).
  void ShowSuggestions(const WebKit::WebInputElement& element,
                       bool autofill_on_empty_values,
                       bool requires_caret_at_end,
                       bool display_warning_if_disabled);

  // Queries the browser for Autocomplete and Autofill suggestions for the given
  // |element|.
  void QueryAutofillSuggestions(const WebKit::WebInputElement& element,
                                bool display_warning_if_disabled);

  // Queries the AutofillManager for form data for the form containing |node|.
  // |value| is the current text in the field, and |unique_id| is the selected
  // profile's unique ID.  |action| specifies whether to Fill or Preview the
  // values returned from the AutofillManager.
  void FillAutofillFormData(const WebKit::WebNode& node,
                            int unique_id,
                            AutofillAction action);

  // Fills |form| and |field| with the FormData and FormField corresponding to
  // |node|. Returns true if the data was found; and false otherwise.
  bool FindFormAndFieldForNode(
      const WebKit::WebNode& node,
      webkit_glue::FormData* form,
      webkit_glue::FormField* field) WARN_UNUSED_RESULT;

  FormCache form_cache_;

  PasswordAutofillManager* password_autofill_manager_;  // WEAK reference.

  // The ID of the last request sent for form field Autofill.  Used to ignore
  // out of date responses.
  int autofill_query_id_;

  // The element corresponding to the last request sent for form field Autofill.
  WebKit::WebInputElement autofill_query_element_;

  // The action to take when receiving Autofill data from the AutofillManager.
  AutofillAction autofill_action_;

  // Should we display a warning if autofill is disabled?
  bool display_warning_if_disabled_;

  // Was the query node autofilled prior to previewing the form?
  bool was_query_node_autofilled_;

  // The menu index of the "Clear" menu item.
  int suggestions_clear_index_;

  // The menu index of the "Autofill options..." menu item.
  int suggestions_options_index_;

  // Have we already shown Autofill suggestions for the field the user is
  // currently editing?  Used to keep track of state for metrics logging.
  bool has_shown_autofill_popup_for_current_edit_;

  base::WeakPtrFactory<AutofillAgent> weak_ptr_factory_;

  friend class PasswordAutofillManagerTest;
  FRIEND_TEST_ALL_PREFIXES(ChromeRenderViewTest, SendForms);
  FRIEND_TEST_ALL_PREFIXES(ChromeRenderViewTest, FillFormElement);
  FRIEND_TEST_ALL_PREFIXES(PasswordAutofillManagerTest, WaitUsername);
  FRIEND_TEST_ALL_PREFIXES(PasswordAutofillManagerTest, SuggestionAccept);
  FRIEND_TEST_ALL_PREFIXES(PasswordAutofillManagerTest, SuggestionSelect);

  DISALLOW_COPY_AND_ASSIGN(AutofillAgent);
};

}  // namespace autofill

#endif  // CHROME_RENDERER_AUTOFILL_AUTOFILL_AGENT_H_
