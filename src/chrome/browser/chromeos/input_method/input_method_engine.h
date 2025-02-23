// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_ENGINE_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_ENGINE_H_
#pragma once

#include <string>
#include <vector>

namespace chromeos {

namespace input_method {
struct KeyEventHandle;
}  // namespace input_method

extern const char* kExtensionImePrefix;

// InputMethodEngine is used to translate from the Chrome IME API to the native
// API.
class InputMethodEngine {
 public:
  struct KeyboardEvent {
    KeyboardEvent();
    virtual ~KeyboardEvent();

    std::string type;
    std::string key;
    std::string key_code;
    bool alt_key;
    bool ctrl_key;
    bool shift_key;
  };

  enum {
    MENU_ITEM_MODIFIED_LABEL        = 0x0001,
    MENU_ITEM_MODIFIED_STYLE        = 0x0002,
    MENU_ITEM_MODIFIED_VISIBLE      = 0x0004,
    MENU_ITEM_MODIFIED_ENABLED      = 0x0008,
    MENU_ITEM_MODIFIED_CHECKED      = 0x0010,
    MENU_ITEM_MODIFIED_ICON         = 0x0020,
    MENU_ITEM_MODIFIED_SHORTCUT_KEY = 0x0040,
  };

  struct MenuItem {
    MenuItem();
    virtual ~MenuItem();

    std::string id;
    std::string label;
    int style;
    bool visible;
    bool enabled;
    bool checked;
    std::string icon;
    KeyboardEvent shortcut_key;

    unsigned int modified;
    std::vector<MenuItem> children;
  };

  struct InputContext {
    int id;
    std::string type;
  };

  struct Candidate {
    Candidate();
    virtual ~Candidate();

    std::string value;
    int id;
    std::string label;
    std::string annotation;
    std::vector<Candidate> candidates;
  };

  struct SegmentInfo {
    int start;
    int end;
    int style;
  };

  class Observer {
   public:
    virtual ~Observer();

    // Called when the IME becomes the active IME.
    virtual void OnActivate(const std::string& engine_id) = 0;

    // Called when the IME is no longer active.
    virtual void OnDeactivated(const std::string& engine_id) = 0;

    // Called when a text field gains focus, and will be sending key events.
    virtual void OnFocus(const InputContext& context) = 0;

    // Called when a text field loses focus, and will no longer generate events.
    virtual void OnBlur(int context_id) = 0;

    // Called when an InputContext's properties change while it is focused.
    virtual void OnInputContextUpdate(const InputContext& context) = 0;

    // Called when the user pressed a key with a text field focused.
    virtual void OnKeyEvent(const std::string& engine_id,
                            const KeyboardEvent& event,
                            input_method::KeyEventHandle* key_data) = 0;

    // Called when the user clicks on an item in the candidate list.
    virtual void OnCandidateClicked(const std::string& engine_id,
                                    int candidate_id,
                                    int button) = 0;

    // Called when a menu item for this IME is interacted with.
    virtual void OnMenuItemActivated(const std::string& engine_id,
                                     const std::string& menu_id) = 0;
  };

  enum {
    MENU_ITEM_STYLE_NONE,
    MENU_ITEM_STYLE_CHECK,
    MENU_ITEM_STYLE_RADIO,
    MENU_ITEM_STYLE_SEPARATOR,
  };

  enum {
    MOUSE_BUTTON_LEFT,
    MOUSE_BUTTON_RIGHT,
    MOUSE_BUTTON_MIDDLE,
  };

  enum {
    SEGMENT_STYLE_UNDERLINE,
    SEGMENT_STYLE_DOUBLE_UNDERLINE,
  };

  virtual ~InputMethodEngine() {}

  // Set the current composition and associated properties.
  virtual bool SetComposition(int context_id,
                              const char* text,
                              int selection_start,
                              int selection_end,
                              const std::vector<SegmentInfo>& segments,
                              std::string* error) = 0;

  // Clear the current composition.
  virtual bool ClearComposition(int context_id, std::string* error) = 0;

  // Commit the specified text to the specified context.  Fails if the context
  // is not focused.
  virtual bool CommitText(int context_id, const char* text,
                          std::string* error) = 0;

  // Show or hide the candidate window.
  virtual bool SetCandidateWindowVisible(bool visible, std::string* error) = 0;

  // Show or hide the cursor in the candidate window.
  virtual void SetCandidateWindowCursorVisible(bool visible) = 0;

  // Set the orientation of the candidate window.
  virtual void SetCandidateWindowVertical(bool vertical) = 0;

  // Set the number of candidates displayed in the candidate window.
  virtual void SetCandidateWindowPageSize(int size) = 0;

  // Set the text that appears as a label in the candidate window.
  virtual void SetCandidateWindowAuxText(const char* text) = 0;

  // Show or hide the extra text in the candidate window.
  virtual void SetCandidateWindowAuxTextVisible(bool visible) = 0;

  // Set the list of entries displayed in the candidate window.
  virtual bool SetCandidates(int context_id,
                             const std::vector<Candidate>& candidates,
                             std::string* error) = 0;

  // Set the position of the cursor in the candidate window.
  virtual bool SetCursorPosition(int context_id, int candidate_id,
                                 std::string* error) = 0;

  // Set the list of items that appears in the language menu when this IME is
  // active.
  virtual bool SetMenuItems(const std::vector<MenuItem>& items) = 0;

  // Update the state of the menu items.
  virtual bool UpdateMenuItems(const std::vector<MenuItem>& items) = 0;

  // Returns true if this IME is active, false if not.
  virtual bool IsActive() const = 0;

  // Inform the engine that a key event has been processed.
  virtual void KeyEventDone(input_method::KeyEventHandle* key_data,
                            bool handled) = 0;

  // Create an IME engine.
  static InputMethodEngine* CreateEngine(
      InputMethodEngine::Observer* observer,
      const char* engine_name,
      const char* extension_id,
      const char* engine_id,
      const char* description,
      const char* language,
      const std::vector<std::string>& layouts,
      KeyboardEvent& shortcut_key,
      std::string* error);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_ENGINE_H_
