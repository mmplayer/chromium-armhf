// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBACCESSIBILITY_H_
#define WEBKIT_GLUE_WEBACCESSIBILITY_H_

#include <map>
#include <string>
#include <vector>

#include "base/string16.h"
#include "ui/gfx/rect.h"

namespace WebKit {
class WebAccessibilityObject;
}

namespace webkit_glue {

// A compact representation of the accessibility information for a
// single web object, in a form that can be serialized and sent from
// the renderer process to the browser process.
struct WebAccessibility {
 public:
  // An enumeration of accessibility roles.
  enum Role {
    ROLE_UNKNOWN = 0,

    // Used by Chromium to distinguish between the root of the tree
    // for this page, and a web area for a frame within this page.
    ROLE_ROOT_WEB_AREA,

    // These roles all directly correspond to WebKit accessibility roles,
    // keep these alphabetical.
    ROLE_ALERT,
    ROLE_ALERT_DIALOG,
    ROLE_ANNOTATION,
    ROLE_APPLICATION,
    ROLE_ARTICLE,
    ROLE_BROWSER,
    ROLE_BUSY_INDICATOR,
    ROLE_BUTTON,
    ROLE_CELL,
    ROLE_CHECKBOX,
    ROLE_COLOR_WELL,
    ROLE_COLUMN,
    ROLE_COLUMN_HEADER,
    ROLE_COMBO_BOX,
    ROLE_DEFINITION_LIST_DEFINITION,
    ROLE_DEFINITION_LIST_TERM,
    ROLE_DIALOG,
    ROLE_DIRECTORY,
    ROLE_DISCLOSURE_TRIANGLE,
    ROLE_DOCUMENT,
    ROLE_DRAWER,
    ROLE_EDITABLE_TEXT,
    ROLE_GRID,
    ROLE_GROUP,
    ROLE_GROW_AREA,
    ROLE_HEADING,
    ROLE_HELP_TAG,
    ROLE_IGNORED,
    ROLE_IMAGE,
    ROLE_IMAGE_MAP,
    ROLE_IMAGE_MAP_LINK,
    ROLE_INCREMENTOR,
    ROLE_LANDMARK_APPLICATION,
    ROLE_LANDMARK_BANNER,
    ROLE_LANDMARK_COMPLEMENTARY,
    ROLE_LANDMARK_CONTENTINFO,
    ROLE_LANDMARK_MAIN,
    ROLE_LANDMARK_NAVIGATION,
    ROLE_LANDMARK_SEARCH,
    ROLE_LINK,
    ROLE_LIST,
    ROLE_LISTBOX,
    ROLE_LISTBOX_OPTION,
    ROLE_LIST_ITEM,
    ROLE_LIST_MARKER,
    ROLE_LOG,
    ROLE_MARQUEE,
    ROLE_MATH,
    ROLE_MATTE,
    ROLE_MENU,
    ROLE_MENU_BAR,
    ROLE_MENU_ITEM,
    ROLE_MENU_BUTTON,
    ROLE_MENU_LIST_OPTION,
    ROLE_MENU_LIST_POPUP,
    ROLE_NOTE,
    ROLE_OUTLINE,
    ROLE_POPUP_BUTTON,
    ROLE_PROGRESS_INDICATOR,
    ROLE_RADIO_BUTTON,
    ROLE_RADIO_GROUP,
    ROLE_REGION,
    ROLE_ROW,
    ROLE_ROW_HEADER,
    ROLE_RULER,
    ROLE_RULER_MARKER,
    ROLE_SCROLLAREA,
    ROLE_SCROLLBAR,
    ROLE_SHEET,
    ROLE_SLIDER,
    ROLE_SLIDER_THUMB,
    ROLE_SPLITTER,
    ROLE_SPLIT_GROUP,
    ROLE_STATIC_TEXT,
    ROLE_STATUS,
    ROLE_SYSTEM_WIDE,
    ROLE_TAB,
    ROLE_TABLE,
    ROLE_TABLE_HEADER_CONTAINER,
    ROLE_TAB_GROUP,
    ROLE_TAB_LIST,
    ROLE_TAB_PANEL,
    ROLE_TEXTAREA,
    ROLE_TEXT_FIELD,
    ROLE_TIMER,
    ROLE_TOOLBAR,
    ROLE_TOOLTIP,
    ROLE_TREE,
    ROLE_TREE_GRID,
    ROLE_TREE_ITEM,
    ROLE_VALUE_INDICATOR,
    ROLE_WEBCORE_LINK,
    ROLE_WEB_AREA,
    ROLE_WINDOW,
    NUM_ROLES
  };

  // An alphabetical enumeration of accessibility states.
  // A state bitmask is formed by shifting 1 to the left by each state,
  // for example:
  //   int mask = (1 << STATE_CHECKED) | (1 << STATE_FOCUSED);
  enum State {
    STATE_BUSY,
    STATE_CHECKED,
    STATE_COLLAPSED,
    STATE_EXPANDED,
    STATE_FOCUSABLE,
    STATE_FOCUSED,
    STATE_HASPOPUP,
    STATE_HOTTRACKED,
    STATE_INDETERMINATE,
    STATE_INVISIBLE,
    STATE_LINKED,
    STATE_MULTISELECTABLE,
    STATE_OFFSCREEN,
    STATE_PRESSED,
    STATE_PROTECTED,
    STATE_READONLY,
    STATE_REQUIRED,
    STATE_SELECTABLE,
    STATE_SELECTED,
    STATE_TRAVERSED,
    STATE_UNAVAILABLE,
    STATE_VERTICAL,
    STATE_VISITED,
    NUM_STATES
  };

  COMPILE_ASSERT(NUM_STATES <= 31, state_enum_not_too_large);

  // Additional optional attributes that can be optionally attached to
  // a node.
  enum StringAttribute {
    // Document attributes.
    ATTR_DOC_URL,
    ATTR_DOC_TITLE,
    ATTR_DOC_MIMETYPE,
    ATTR_DOC_DOCTYPE,

    // Attributes that could apply to any node.
    ATTR_ACCESS_KEY,
    ATTR_ACTION,
    ATTR_CONTAINER_LIVE_RELEVANT,
    ATTR_CONTAINER_LIVE_STATUS,
    ATTR_DESCRIPTION,
    ATTR_DISPLAY,
    ATTR_HELP,
    ATTR_HTML_TAG,
    ATTR_LIVE_RELEVANT,
    ATTR_LIVE_STATUS,
    ATTR_ROLE,
    ATTR_SHORTCUT,
    ATTR_URL,
  };

  enum IntAttribute {
    // Document attributes.
    ATTR_DOC_SCROLLX,
    ATTR_DOC_SCROLLY,

    // Editable text attributes.
    ATTR_TEXT_SEL_START,
    ATTR_TEXT_SEL_END,

    // Table attributes.
    ATTR_TABLE_ROW_COUNT,
    ATTR_TABLE_COLUMN_COUNT,

    // Table cell attributes.
    ATTR_TABLE_CELL_COLUMN_INDEX,
    ATTR_TABLE_CELL_COLUMN_SPAN,
    ATTR_TABLE_CELL_ROW_INDEX,
    ATTR_TABLE_CELL_ROW_SPAN,

    // Tree control attributes.
    ATTR_HIERARCHICAL_LEVEL,
  };

  enum FloatAttribute {
    // Document attributes.
    ATTR_DOC_LOADING_PROGRESS,

    // Range attributes.
    ATTR_VALUE_FOR_RANGE,
    ATTR_MIN_VALUE_FOR_RANGE,
    ATTR_MAX_VALUE_FOR_RANGE,
  };

  enum BoolAttribute {
    // Document attributes.
    ATTR_DOC_LOADED,

    // True if a checkbox or radio button is in the "mixed" state.
    ATTR_BUTTON_MIXED,

    // Live region attributes.
    ATTR_CONTAINER_LIVE_ATOMIC,
    ATTR_CONTAINER_LIVE_BUSY,
    ATTR_LIVE_ATOMIC,
    ATTR_LIVE_BUSY,

    // ARIA readonly flag.
    ATTR_ARIA_READONLY,

    // Writeable attributes
    ATTR_CAN_SET_VALUE,
  };

  // Empty constructor, for serialization.
  WebAccessibility();

  // Construct from a WebAccessibilityObject. Recursively creates child
  // nodes as needed to complete the tree.
  WebAccessibility(const WebKit::WebAccessibilityObject& src,
                   bool include_children);

  ~WebAccessibility();

#ifndef NDEBUG
  std::string DebugString(bool recursive,
                          int render_routing_id,
                          int notification_type) const;
#endif

 private:
  // Initialize an already-created struct, same as the constructor above.
  void Init(const WebKit::WebAccessibilityObject& src,
            bool include_children);

  // Returns true if |ancestor| is the first unignored parent of |child|,
  // which means that when walking up the parent chain from |child|,
  // |ancestor| is the *first* ancestor that isn't marked as
  // accessibilityIsIgnored().
  bool IsParentUnignoredOf(const WebKit::WebAccessibilityObject& ancestor,
                           const WebKit::WebAccessibilityObject& child);

 public:
  // This is a simple serializable struct. All member variables should be
  // copyable.
  int32 id;
  string16 name;
  string16 value;
  Role role;
  uint32 state;
  gfx::Rect location;
  std::map<StringAttribute, string16> string_attributes;
  std::map<IntAttribute, int32> int_attributes;
  std::map<FloatAttribute, float> float_attributes;
  std::map<BoolAttribute, bool> bool_attributes;
  std::vector<WebAccessibility> children;
  std::vector<int32> indirect_child_ids;
  std::vector<std::pair<string16, string16> > html_attributes;
  std::vector<int32> line_breaks;

  // For a table, the cell ids in row-major order, with duplicate entries
  // when there's a rowspan or colspan, and with -1 for missing cells.
  // There are always exactly rows * columns entries.
  std::vector<int32> cell_ids;

  // For a table, the unique cell ids in row-major order of their first
  // occurrence.
  std::vector<int32> unique_cell_ids;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBACCESSIBILITY_H_
