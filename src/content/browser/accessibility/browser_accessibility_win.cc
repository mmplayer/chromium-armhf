// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_win.h"

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_manager_win.h"
#include "content/common/view_messages.h"
#include "net/base/escape.h"

using webkit_glue::WebAccessibility;

// The GUID for the ISimpleDOM service is not defined in the IDL files.
// This is taken directly from the Mozilla sources
// (accessible/src/msaa/nsAccessNodeWrap.cpp) and it's also documented at:
// http://developer.mozilla.org/en/Accessibility/AT-APIs/ImplementationFeatures/MSAA

const GUID GUID_ISimpleDOM = {
    0x0c539790, 0x12e4, 0x11cf,
    0xb6, 0x61, 0x00, 0xaa, 0x00, 0x4c, 0xd6, 0xd8};

// static
BrowserAccessibility* BrowserAccessibility::Create() {
  CComObject<BrowserAccessibilityWin>* instance;
  HRESULT hr = CComObject<BrowserAccessibilityWin>::CreateInstance(&instance);
  DCHECK(SUCCEEDED(hr));
  return instance->NewReference();
}

BrowserAccessibilityWin* BrowserAccessibility::toBrowserAccessibilityWin() {
  return static_cast<BrowserAccessibilityWin*>(this);
}

BrowserAccessibilityWin::BrowserAccessibilityWin()
    : ia_role_(0),
      ia_state_(0),
      ia2_role_(0),
      ia2_state_(0),
      first_time_(true) {
}

BrowserAccessibilityWin::~BrowserAccessibilityWin() {
}

//
// IAccessible methods.
//
// Conventions:
// * Always test for instance_active_ first and return E_FAIL if it's false.
// * Always check for invalid arguments first, even if they're unused.
// * Return S_FALSE if the only output is a string argument and it's empty.
//

HRESULT BrowserAccessibilityWin::accDoDefaultAction(VARIANT var_id) {
  if (!instance_active_)
    return E_FAIL;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  manager_->DoDefaultAction(*target);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::accHitTest(LONG x_left,
                                                 LONG y_top,
                                                 VARIANT* child) {
  if (!instance_active_)
    return E_FAIL;

  if (!child)
    return E_INVALIDARG;

  gfx::Point point(x_left, y_top);
  if (!GetBoundsRect().Contains(point)) {
    // Return S_FALSE and VT_EMPTY when the outside the object's boundaries.
    child->vt = VT_EMPTY;
    return S_FALSE;
  }

  BrowserAccessibility* result = BrowserAccessibilityForPoint(point);
  if (result == this) {
    // Point is within this object.
    child->vt = VT_I4;
    child->lVal = CHILDID_SELF;
  } else {
    child->vt = VT_DISPATCH;
    child->pdispVal = result->toBrowserAccessibilityWin()->NewReference();
  }
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::accLocation(LONG* x_left, LONG* y_top,
                                                  LONG* width, LONG* height,
                                                  VARIANT var_id) {
  if (!instance_active_)
    return E_FAIL;

  if (!x_left || !y_top || !width || !height)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  gfx::Rect bounds = target->GetBoundsRect();
  *x_left = bounds.x();
  *y_top  = bounds.y();
  *width  = bounds.width();
  *height = bounds.height();

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::accNavigate(
    LONG nav_dir, VARIANT start, VARIANT* end) {
  BrowserAccessibilityWin* target = GetTargetFromChildID(start);
  if (!target)
    return E_INVALIDARG;

  if ((nav_dir == NAVDIR_LASTCHILD || nav_dir == NAVDIR_FIRSTCHILD) &&
      start.lVal != CHILDID_SELF) {
    // MSAA states that navigating to first/last child can only be from self.
    return E_INVALIDARG;
  }

  BrowserAccessibility* result = NULL;
  switch (nav_dir) {
    case NAVDIR_DOWN:
    case NAVDIR_UP:
    case NAVDIR_LEFT:
    case NAVDIR_RIGHT:
      // These directions are not implemented, matching Mozilla and IE.
      return E_NOTIMPL;
    case NAVDIR_FIRSTCHILD:
      if (!target->children_.empty())
        result = target->children_.front();
      break;
    case NAVDIR_LASTCHILD:
      if (!target->children_.empty())
        result = target->children_.back();
      break;
    case NAVDIR_NEXT:
      result = target->GetNextSibling();
      break;
    case NAVDIR_PREVIOUS:
      result = target->GetPreviousSibling();
      break;
  }

  if (!result) {
    end->vt = VT_EMPTY;
    return S_FALSE;
  }

  end->vt = VT_DISPATCH;
  end->pdispVal = result->toBrowserAccessibilityWin()->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accChild(VARIANT var_child,
                                                   IDispatch** disp_child) {
  if (!instance_active_)
    return E_FAIL;

  if (!disp_child)
    return E_INVALIDARG;

  *disp_child = NULL;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_child);
  if (!target)
    return E_INVALIDARG;

  (*disp_child) = target->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accChildCount(LONG* child_count) {
  if (!instance_active_)
    return E_FAIL;

  if (!child_count)
    return E_INVALIDARG;

  *child_count = children_.size();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accDefaultAction(VARIANT var_id,
                                                           BSTR* def_action) {
  if (!instance_active_)
    return E_FAIL;

  if (!def_action)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  return target->GetStringAttributeAsBstr(
      WebAccessibility::ATTR_SHORTCUT, def_action);
}

STDMETHODIMP BrowserAccessibilityWin::get_accDescription(VARIANT var_id,
                                                         BSTR* desc) {
  if (!instance_active_)
    return E_FAIL;

  if (!desc)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  return target->GetStringAttributeAsBstr(
      WebAccessibility::ATTR_DESCRIPTION, desc);
}

STDMETHODIMP BrowserAccessibilityWin::get_accFocus(VARIANT* focus_child) {
  if (!instance_active_)
    return E_FAIL;

  if (!focus_child)
    return E_INVALIDARG;

  BrowserAccessibilityWin* focus = static_cast<BrowserAccessibilityWin*>(
      manager_->GetFocus(this));
  if (focus == this) {
    focus_child->vt = VT_I4;
    focus_child->lVal = CHILDID_SELF;
  } else if (focus == NULL) {
    focus_child->vt = VT_EMPTY;
  } else {
    focus_child->vt = VT_DISPATCH;
    focus_child->pdispVal = focus->NewReference();
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accHelp(VARIANT var_id, BSTR* help) {
  if (!instance_active_)
    return E_FAIL;

  if (!help)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  return target->GetStringAttributeAsBstr(WebAccessibility::ATTR_HELP, help);
}

STDMETHODIMP BrowserAccessibilityWin::get_accKeyboardShortcut(VARIANT var_id,
                                                              BSTR* acc_key) {
  if (!instance_active_)
    return E_FAIL;

  if (!acc_key)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  return target->GetStringAttributeAsBstr(
      WebAccessibility::ATTR_SHORTCUT, acc_key);
}

STDMETHODIMP BrowserAccessibilityWin::get_accName(VARIANT var_id, BSTR* name) {
  if (!instance_active_)
    return E_FAIL;

  if (!name)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  if (target->name_.empty())
    return S_FALSE;

  *name = SysAllocString(target->name_.c_str());

  DCHECK(*name);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accParent(IDispatch** disp_parent) {
  if (!instance_active_)
    return E_FAIL;

  if (!disp_parent)
    return E_INVALIDARG;

  IAccessible* parent = parent_->toBrowserAccessibilityWin();
  if (parent == NULL) {
    // This happens if we're the root of the tree;
    // return the IAccessible for the window.
    parent = manager_->toBrowserAccessibilityManagerWin()->
             GetParentWindowIAccessible();
  }

  parent->AddRef();
  *disp_parent = parent;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accRole(
    VARIANT var_id, VARIANT* role) {
  if (!instance_active_)
    return E_FAIL;

  if (!role)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  if (!target->role_name_.empty()) {
    role->vt = VT_BSTR;
    role->bstrVal = SysAllocString(target->role_name_.c_str());
  } else {
    role->vt = VT_I4;
    role->lVal = target->ia_role_;
  }
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accState(VARIANT var_id,
                                                   VARIANT* state) {
  if (!instance_active_)
    return E_FAIL;

  if (!state)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  state->vt = VT_I4;
  state->lVal = target->ia_state_;
  if (manager_->GetFocus(NULL) == this)
    state->lVal |= STATE_SYSTEM_FOCUSED;

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accValue(
    VARIANT var_id, BSTR* value) {
  if (!instance_active_)
    return E_FAIL;

  if (!value)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  *value = SysAllocString(target->value_.c_str());

  DCHECK(*value);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accHelpTopic(
    BSTR* help_file, VARIANT var_id, LONG* topic_id) {
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::get_accSelection(VARIANT* selected) {
  if (!instance_active_)
    return E_FAIL;

  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::accSelect(
    LONG flags_sel, VARIANT var_id) {
  if (!instance_active_)
    return E_FAIL;

  if (flags_sel & SELFLAG_TAKEFOCUS) {
    manager_->SetFocus(this, true);
    return S_OK;
  }

  return S_FALSE;
}

//
// IAccessible2 methods.
//

STDMETHODIMP BrowserAccessibilityWin::role(LONG* role) {
  if (!instance_active_)
    return E_FAIL;

  if (!role)
    return E_INVALIDARG;

  *role = ia2_role_;

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_attributes(BSTR* attributes) {
  if (!instance_active_)
    return E_FAIL;

  if (!attributes)
    return E_INVALIDARG;

  // The iaccessible2 attributes are a set of key-value pairs
  // separated by semicolons, with a colon between the key and the value.
  string16 str;
  for (unsigned int i = 0; i < ia2_attributes_.size(); ++i) {
    if (i != 0)
      str += L';';
    str += ia2_attributes_[i];
  }

  if (str.empty())
    return S_FALSE;

  *attributes = SysAllocString(str.c_str());
  DCHECK(*attributes);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_states(AccessibleStates* states) {
  if (!instance_active_)
    return E_FAIL;

  if (!states)
    return E_INVALIDARG;

  *states = ia2_state_;

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_uniqueID(LONG* unique_id) {
  if (!instance_active_)
    return E_FAIL;

  if (!unique_id)
    return E_INVALIDARG;

  *unique_id = child_id_;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_windowHandle(HWND* window_handle) {
  if (!instance_active_)
    return E_FAIL;

  if (!window_handle)
    return E_INVALIDARG;

  *window_handle = manager_->GetParentView();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_indexInParent(LONG* index_in_parent) {
  if (!instance_active_)
    return E_FAIL;

  if (!index_in_parent)
    return E_INVALIDARG;

  *index_in_parent = index_in_parent_;
  return S_OK;
}

//
// IAccessibleImage methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_description(BSTR* desc) {
  if (!instance_active_)
    return E_FAIL;

  if (!desc)
    return E_INVALIDARG;

  return GetStringAttributeAsBstr(WebAccessibility::ATTR_DESCRIPTION, desc);
}

STDMETHODIMP BrowserAccessibilityWin::get_imagePosition(
    enum IA2CoordinateType coordinate_type, LONG* x, LONG* y) {
  if (!instance_active_)
    return E_FAIL;

  if (!x || !y)
    return E_INVALIDARG;

  if (coordinate_type == IA2_COORDTYPE_SCREEN_RELATIVE) {
    HWND parent_hwnd = manager_->GetParentView();
    POINT top_left = {0, 0};
    ::ClientToScreen(parent_hwnd, &top_left);
    *x = location_.x() + top_left.x;
    *y = location_.y() + top_left.y;
  } else if (coordinate_type == IA2_COORDTYPE_PARENT_RELATIVE) {
    *x = location_.x();
    *y = location_.y();
    if (parent_) {
      *x -= parent_->location().x();
      *y -= parent_->location().y();
    }
  } else {
    return E_INVALIDARG;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_imageSize(LONG* height, LONG* width) {
  if (!instance_active_)
    return E_FAIL;

  if (!height || !width)
    return E_INVALIDARG;

  *height = location_.height();
  *width = location_.width();
  return S_OK;
}

//
// IAccessibleTable methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_accessibleAt(
    long row,
    long column,
    IUnknown** accessible) {
  if (!instance_active_)
    return E_FAIL;

  if (!accessible)
    return E_INVALIDARG;

  int columns;
  int rows;
  if (!GetIntAttribute(WebAccessibility::ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !GetIntAttribute(WebAccessibility::ATTR_TABLE_ROW_COUNT, &rows) ||
      columns <= 0 ||
      rows <= 0) {
    return S_FALSE;
  }

  if (row < 0 || row >= rows || column < 0 || column >= columns)
    return E_INVALIDARG;

  DCHECK_EQ(columns * rows, static_cast<int>(cell_ids_.size()));

  int cell_id = cell_ids_[row * columns + column];
  BrowserAccessibilityWin* cell = GetFromRendererID(cell_id);
  if (cell) {
    *accessible = static_cast<IAccessible*>(cell->NewReference());
    return S_OK;
  }

  *accessible = NULL;
  return E_INVALIDARG;
}

STDMETHODIMP BrowserAccessibilityWin::get_caption(IUnknown** accessible) {
  if (!instance_active_)
    return E_FAIL;

  if (!accessible)
    return E_INVALIDARG;

  // TODO(dmazzoni): implement
  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_childIndex(
    long row,
    long column,
    long* cell_index) {
  if (!instance_active_)
    return E_FAIL;

  if (!cell_index)
    return E_INVALIDARG;

  int columns;
  int rows;
  if (!GetIntAttribute(WebAccessibility::ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !GetIntAttribute(WebAccessibility::ATTR_TABLE_ROW_COUNT, &rows) ||
      columns <= 0 ||
      rows <= 0) {
    return S_FALSE;
  }

  if (row < 0 || row >= rows || column < 0 || column >= columns)
    return E_INVALIDARG;

  DCHECK_EQ(columns * rows, static_cast<int>(cell_ids_.size()));
  int cell_id = cell_ids_[row * columns + column];
  for (size_t i = 0; i < unique_cell_ids_.size(); ++i) {
    if (unique_cell_ids_[i] == cell_id) {
      *cell_index = (long)i;
      return S_OK;
    }
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_columnDescription(
    long column,
    BSTR* description) {
  if (!instance_active_)
    return E_FAIL;

  if (!description)
    return E_INVALIDARG;

  int columns;
  int rows;
  if (!GetIntAttribute(WebAccessibility::ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !GetIntAttribute(WebAccessibility::ATTR_TABLE_ROW_COUNT, &rows) ||
      columns <= 0 ||
      rows <= 0) {
    return S_FALSE;
  }

  if (column < 0 || column >= columns)
    return E_INVALIDARG;

  for (int i = 0; i < rows; ++i) {
    int cell_id = cell_ids_[i * columns + column];
    BrowserAccessibilityWin* cell = static_cast<BrowserAccessibilityWin*>(
        manager_->GetFromRendererID(cell_id));
    if (cell && cell->role_ == WebAccessibility::ROLE_COLUMN_HEADER) {
      if (cell->name_.size() > 0) {
        *description = SysAllocString(cell->name_.c_str());
        return S_OK;
      }

      return cell->GetStringAttributeAsBstr(
          WebAccessibility::ATTR_DESCRIPTION, description);
    }
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_columnExtentAt(
    long row,
    long column,
    long* n_columns_spanned) {
  if (!instance_active_)
    return E_FAIL;

  if (!n_columns_spanned)
    return E_INVALIDARG;

  int columns;
  int rows;
  if (!GetIntAttribute(WebAccessibility::ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !GetIntAttribute(WebAccessibility::ATTR_TABLE_ROW_COUNT, &rows) ||
      columns <= 0 ||
      rows <= 0) {
    return S_FALSE;
  }

  if (row < 0 || row >= rows || column < 0 || column >= columns)
    return E_INVALIDARG;

  int cell_id = cell_ids_[row * columns + column];
  BrowserAccessibilityWin* cell = static_cast<BrowserAccessibilityWin*>(
      manager_->GetFromRendererID(cell_id));
  int colspan;
  if (cell &&
      cell->GetIntAttribute(
          WebAccessibility::ATTR_TABLE_CELL_COLUMN_SPAN, &colspan) &&
      colspan >= 1) {
    *n_columns_spanned = colspan;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_columnHeader(
    IAccessibleTable** accessible_table,
    long* starting_row_index) {
  // TODO(dmazzoni): implement
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::get_columnIndex(
    long cell_index,
    long* column_index) {
  if (!instance_active_)
    return E_FAIL;

  if (!column_index)
    return E_INVALIDARG;

  int cell_id_count = static_cast<int>(unique_cell_ids_.size());
  if (cell_index < 0)
    return E_INVALIDARG;
  if (cell_index >= cell_id_count)
    return S_FALSE;

  int cell_id = unique_cell_ids_[cell_index];
  BrowserAccessibilityWin* cell =
      manager_->GetFromRendererID(cell_id)->toBrowserAccessibilityWin();
  int col_index;
  if (cell &&
      cell->GetIntAttribute(
          WebAccessibility::ATTR_TABLE_CELL_COLUMN_INDEX, &col_index)) {
    *column_index = col_index;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_nColumns(
    long* column_count) {
  if (!instance_active_)
    return E_FAIL;

  if (!column_count)
    return E_INVALIDARG;

  int columns;
  if (GetIntAttribute(WebAccessibility::ATTR_TABLE_COLUMN_COUNT, &columns)) {
    *column_count = columns;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_nRows(
    long* row_count) {
  if (!instance_active_)
    return E_FAIL;

  if (!row_count)
    return E_INVALIDARG;

  int rows;
  if (GetIntAttribute(WebAccessibility::ATTR_TABLE_ROW_COUNT, &rows)) {
    *row_count = rows;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_nSelectedChildren(
    long* cell_count) {
  if (!instance_active_)
    return E_FAIL;

  if (!cell_count)
    return E_INVALIDARG;

  // TODO(dmazzoni): add support for selected cells/rows/columns in tables.
  *cell_count = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_nSelectedColumns(
    long* column_count) {
  if (!instance_active_)
    return E_FAIL;

  if (!column_count)
    return E_INVALIDARG;

  *column_count = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_nSelectedRows(
    long* row_count) {
  if (!instance_active_)
    return E_FAIL;

  if (!row_count)
    return E_INVALIDARG;

  *row_count = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowDescription(
    long row,
    BSTR* description) {
  if (!instance_active_)
    return E_FAIL;

  if (!description)
    return E_INVALIDARG;

  int columns;
  int rows;
  if (!GetIntAttribute(WebAccessibility::ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !GetIntAttribute(WebAccessibility::ATTR_TABLE_ROW_COUNT, &rows) ||
      columns <= 0 ||
      rows <= 0) {
    return S_FALSE;
  }

  if (row < 0 || row >= rows)
    return E_INVALIDARG;

  for (int i = 0; i < columns; ++i) {
    int cell_id = cell_ids_[row * columns + i];
    BrowserAccessibilityWin* cell =
        manager_->GetFromRendererID(cell_id)->toBrowserAccessibilityWin();
    if (cell && cell->role_ == WebAccessibility::ROLE_ROW_HEADER) {
      if (cell->name_.size() > 0) {
        *description = SysAllocString(cell->name_.c_str());
        return S_OK;
      }

      return cell->GetStringAttributeAsBstr(
          WebAccessibility::ATTR_DESCRIPTION, description);
    }
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowExtentAt(
    long row,
    long column,
    long* n_rows_spanned) {
  if (!instance_active_)
    return E_FAIL;

  if (!n_rows_spanned)
    return E_INVALIDARG;

  int columns;
  int rows;
  if (!GetIntAttribute(WebAccessibility::ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !GetIntAttribute(WebAccessibility::ATTR_TABLE_ROW_COUNT, &rows) ||
      columns <= 0 ||
      rows <= 0) {
    return S_FALSE;
  }

  if (row < 0 || row >= rows || column < 0 || column >= columns)
    return E_INVALIDARG;

  int cell_id = cell_ids_[row * columns + column];
  BrowserAccessibilityWin* cell =
      manager_->GetFromRendererID(cell_id)->toBrowserAccessibilityWin();
  int rowspan;
  if (cell &&
      cell->GetIntAttribute(
          WebAccessibility::ATTR_TABLE_CELL_ROW_SPAN, &rowspan) &&
      rowspan >= 1) {
    *n_rows_spanned = rowspan;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowHeader(
    IAccessibleTable **accessible_table,
    long* starting_column_index) {
  // TODO(dmazzoni): implement
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowIndex(
    long cell_index,
    long* row_index) {
  if (!instance_active_)
    return E_FAIL;

  if (!row_index)
    return E_INVALIDARG;

  int cell_id_count = static_cast<int>(unique_cell_ids_.size());
  if (cell_index < 0)
    return E_INVALIDARG;
  if (cell_index >= cell_id_count)
    return S_FALSE;

  int cell_id = unique_cell_ids_[cell_index];
  BrowserAccessibilityWin* cell =
      manager_->GetFromRendererID(cell_id)->toBrowserAccessibilityWin();
  int cell_row_index;
  if (cell &&
      cell->GetIntAttribute(
          WebAccessibility::ATTR_TABLE_CELL_ROW_INDEX, &cell_row_index)) {
    *row_index = cell_row_index;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_selectedChildren(
    long max_children,
    long** children,
    long* n_children) {
  if (!instance_active_)
    return E_FAIL;

  if (!children || !n_children)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *n_children = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_selectedColumns(
    long max_columns,
    long** columns,
    long* n_columns) {
  if (!instance_active_)
    return E_FAIL;

  if (!columns || !n_columns)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *n_columns = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_selectedRows(
    long max_rows,
    long** rows,
    long* n_rows) {
  if (!instance_active_)
    return E_FAIL;

  if (!rows || !n_rows)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *n_rows = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_summary(
    IUnknown** accessible) {
  if (!instance_active_)
    return E_FAIL;

  if (!accessible)
    return E_INVALIDARG;

  // TODO(dmazzoni): implement
  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_isColumnSelected(
    long column,
    boolean* is_selected) {
  if (!instance_active_)
    return E_FAIL;

  if (!is_selected)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *is_selected = false;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_isRowSelected(
    long row,
    boolean* is_selected) {
  if (!instance_active_)
    return E_FAIL;

  if (!is_selected)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *is_selected = false;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_isSelected(
    long row,
    long column,
    boolean* is_selected) {
  if (!instance_active_)
    return E_FAIL;

  if (!is_selected)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *is_selected = false;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowColumnExtentsAtIndex(
    long index,
    long* row,
    long* column,
    long* row_extents,
    long* column_extents,
    boolean* is_selected) {
  if (!instance_active_)
    return E_FAIL;

  if (!row || !column || !row_extents || !column_extents || !is_selected)
    return E_INVALIDARG;

  int cell_id_count = static_cast<int>(unique_cell_ids_.size());
  if (index < 0)
    return E_INVALIDARG;
  if (index >= cell_id_count)
    return S_FALSE;

  int cell_id = unique_cell_ids_[index];
  BrowserAccessibilityWin* cell =
      manager_->GetFromRendererID(cell_id)->toBrowserAccessibilityWin();
  int rowspan;
  int colspan;
  if (cell &&
      cell->GetIntAttribute(
          WebAccessibility::ATTR_TABLE_CELL_ROW_SPAN, &rowspan) &&
      cell->GetIntAttribute(
          WebAccessibility::ATTR_TABLE_CELL_COLUMN_SPAN, &colspan) &&
      rowspan >= 1 &&
      colspan >= 1) {
    *row_extents = rowspan;
    *column_extents = colspan;
    return S_OK;
  }

  return S_FALSE;
}

//
// IAccessibleTable2 methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_cellAt(
    long row,
    long column,
    IUnknown** cell) {
  return get_accessibleAt(row, column, cell);
}

STDMETHODIMP BrowserAccessibilityWin::get_nSelectedCells(long* cell_count) {
  return get_nSelectedChildren(cell_count);
}

STDMETHODIMP BrowserAccessibilityWin::get_selectedCells(
    IUnknown*** cells,
    long* n_selected_cells) {
  if (!instance_active_)
    return E_FAIL;

  if (!cells || !n_selected_cells)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *n_selected_cells = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_selectedColumns(
    long** columns,
    long* n_columns) {
  if (!instance_active_)
    return E_FAIL;

  if (!columns || !n_columns)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *n_columns = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_selectedRows(
    long** rows,
    long* n_rows) {
  if (!instance_active_)
    return E_FAIL;

  if (!rows || !n_rows)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *n_rows = 0;
  return S_OK;
}


//
// IAccessibleTableCell methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_columnExtent(
    long* n_columns_spanned) {
  if (!instance_active_)
    return E_FAIL;

  if (!n_columns_spanned)
    return E_INVALIDARG;

  int colspan;
  if (GetIntAttribute(
          WebAccessibility::ATTR_TABLE_CELL_COLUMN_SPAN, &colspan) &&
      colspan >= 1) {
    *n_columns_spanned = colspan;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_columnHeaderCells(
    IUnknown*** cell_accessibles,
    long* n_column_header_cells) {
  if (!instance_active_)
    return E_FAIL;

  if (!cell_accessibles || !n_column_header_cells)
    return E_INVALIDARG;

  *n_column_header_cells = 0;

  int column;
  if (!GetIntAttribute(
          WebAccessibility::ATTR_TABLE_CELL_COLUMN_INDEX, &column)) {
    return S_FALSE;
  }

  BrowserAccessibility* table = parent();
  while (table && table->role() != WebAccessibility::ROLE_TABLE)
    table = table->parent();
  if (!table) {
    NOTREACHED();
    return S_FALSE;
  }

  int columns;
  int rows;
  if (!table->GetIntAttribute(
          WebAccessibility::ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !table->GetIntAttribute(
          WebAccessibility::ATTR_TABLE_ROW_COUNT, &rows)) {
    return S_FALSE;
  }
  if (columns <= 0 || rows <= 0 || column < 0 || column >= columns)
    return S_FALSE;

  for (int i = 0; i < rows; ++i) {
    int cell_id = table->cell_ids()[i * columns + column];
    BrowserAccessibilityWin* cell =
        manager_->GetFromRendererID(cell_id)->toBrowserAccessibilityWin();
    if (cell && cell->role_ == WebAccessibility::ROLE_COLUMN_HEADER)
      (*n_column_header_cells)++;
  }

  *cell_accessibles = static_cast<IUnknown**>(CoTaskMemAlloc(
      (*n_column_header_cells) * sizeof(cell_accessibles[0])));
  int index = 0;
  for (int i = 0; i < rows; ++i) {
    int cell_id = table->cell_ids()[i * columns + column];
    BrowserAccessibilityWin* cell =
        manager_->GetFromRendererID(cell_id)->toBrowserAccessibilityWin();
    if (cell && cell->role_ == WebAccessibility::ROLE_COLUMN_HEADER) {
      (*cell_accessibles)[index] =
          static_cast<IAccessible*>(cell->NewReference());
      ++index;
    }
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_columnIndex(
    long* column_index) {
  if (!instance_active_)
    return E_FAIL;

  if (!column_index)
    return E_INVALIDARG;

  int column;
  if (GetIntAttribute(
          WebAccessibility::ATTR_TABLE_CELL_COLUMN_INDEX, &column)) {
    *column_index = column;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowExtent(
    long* n_rows_spanned) {
  if (!instance_active_)
    return E_FAIL;

  if (!n_rows_spanned)
    return E_INVALIDARG;

  int rowspan;
  if (GetIntAttribute(
          WebAccessibility::ATTR_TABLE_CELL_ROW_SPAN, &rowspan) &&
      rowspan >= 1) {
    *n_rows_spanned = rowspan;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowHeaderCells(
    IUnknown*** cell_accessibles,
    long* n_row_header_cells) {
  if (!instance_active_)
    return E_FAIL;

  if (!cell_accessibles || !n_row_header_cells)
    return E_INVALIDARG;

  *n_row_header_cells = 0;

  int row;
  if (!GetIntAttribute(
          WebAccessibility::ATTR_TABLE_CELL_ROW_INDEX, &row)) {
    return S_FALSE;
  }

  BrowserAccessibility* table = parent();
  while (table && table->role() != WebAccessibility::ROLE_TABLE)
    table = table->parent();
  if (!table) {
    NOTREACHED();
    return S_FALSE;
  }

  int columns;
  int rows;
  if (!table->GetIntAttribute(
          WebAccessibility::ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !table->GetIntAttribute(
          WebAccessibility::ATTR_TABLE_ROW_COUNT, &rows)) {
    return S_FALSE;
  }
  if (columns <= 0 || rows <= 0 || row < 0 || row >= rows)
    return S_FALSE;

  for (int i = 0; i < columns; ++i) {
    int cell_id = table->cell_ids()[row * columns + i];
    BrowserAccessibilityWin* cell =
        manager_->GetFromRendererID(cell_id)->toBrowserAccessibilityWin();
    if (cell && cell->role_ == WebAccessibility::ROLE_ROW_HEADER)
      (*n_row_header_cells)++;
  }

  *cell_accessibles = static_cast<IUnknown**>(CoTaskMemAlloc(
      (*n_row_header_cells) * sizeof(cell_accessibles[0])));
  int index = 0;
  for (int i = 0; i < columns; ++i) {
    int cell_id = table->cell_ids()[row * columns + i];
    BrowserAccessibilityWin* cell =
        manager_->GetFromRendererID(cell_id)->toBrowserAccessibilityWin();
    if (cell && cell->role_ == WebAccessibility::ROLE_ROW_HEADER) {
      (*cell_accessibles)[index] =
          static_cast<IAccessible*>(cell->NewReference());
      ++index;
    }
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowIndex(
    long* row_index) {
  if (!instance_active_)
    return E_FAIL;

  if (!row_index)
    return E_INVALIDARG;

  int row;
  if (GetIntAttribute(WebAccessibility::ATTR_TABLE_CELL_ROW_INDEX, &row)) {
    *row_index = row;
    return S_OK;
  }
  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_isSelected(
    boolean* is_selected) {
  if (!instance_active_)
    return E_FAIL;

  if (!is_selected)
    return E_INVALIDARG;

  *is_selected = false;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowColumnExtents(
    long* row_index,
    long* column_index,
    long* row_extents,
    long* column_extents,
    boolean* is_selected) {
  if (!instance_active_)
    return E_FAIL;

  if (!row_index ||
      !column_index ||
      !row_extents ||
      !column_extents ||
      !is_selected) {
    return E_INVALIDARG;
  }

  int row;
  int column;
  int rowspan;
  int colspan;
  if (GetIntAttribute(WebAccessibility::ATTR_TABLE_CELL_ROW_INDEX, &row) &&
      GetIntAttribute(
          WebAccessibility::ATTR_TABLE_CELL_COLUMN_INDEX, &column) &&
      GetIntAttribute(
          WebAccessibility::ATTR_TABLE_CELL_ROW_SPAN, &rowspan) &&
      GetIntAttribute(
          WebAccessibility::ATTR_TABLE_CELL_COLUMN_SPAN, &colspan)) {
    *row_index = row;
    *column_index = column;
    *row_extents = rowspan;
    *column_extents = colspan;
    *is_selected = false;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_table(
    IUnknown** table) {
  if (!instance_active_)
    return E_FAIL;

  if (!table)
    return E_INVALIDARG;


  int row;
  int column;
  GetIntAttribute(WebAccessibility::ATTR_TABLE_CELL_ROW_INDEX, &row);
  GetIntAttribute(WebAccessibility::ATTR_TABLE_CELL_COLUMN_INDEX, &column);

  BrowserAccessibility* find_table = parent();
  while (find_table && find_table->role() != WebAccessibility::ROLE_TABLE)
    find_table = find_table->parent();
  if (!find_table) {
    NOTREACHED();
    return S_FALSE;
  }

  *table = static_cast<IAccessibleTable*>(
      find_table->toBrowserAccessibilityWin()->NewReference());

  return S_OK;
}

//
// IAccessibleText methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_nCharacters(LONG* n_characters) {
  if (!instance_active_)
    return E_FAIL;

  if (!n_characters)
    return E_INVALIDARG;

  if (role_ == WebAccessibility::ROLE_TEXT_FIELD ||
      role_ == WebAccessibility::ROLE_TEXTAREA) {
    *n_characters = value_.length();
  } else {
    *n_characters = name_.length();
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_caretOffset(LONG* offset) {
  if (!instance_active_)
    return E_FAIL;

  if (!offset)
    return E_INVALIDARG;

  if (role_ == WebAccessibility::ROLE_TEXT_FIELD ||
      role_ == WebAccessibility::ROLE_TEXTAREA) {
    int sel_start = 0;
    if (GetIntAttribute(WebAccessibility::ATTR_TEXT_SEL_START, &sel_start)) {
      *offset = sel_start;
    } else {
      *offset = 0;
    }
  } else {
    *offset = 0;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_nSelections(LONG* n_selections) {
  if (!instance_active_)
    return E_FAIL;

  if (!n_selections)
    return E_INVALIDARG;

  if (role_ == WebAccessibility::ROLE_TEXT_FIELD ||
      role_ == WebAccessibility::ROLE_TEXTAREA) {
    int sel_start = 0;
    int sel_end = 0;
    if (GetIntAttribute(WebAccessibility::ATTR_TEXT_SEL_START, &sel_start) &&
        GetIntAttribute(WebAccessibility::ATTR_TEXT_SEL_END, &sel_end) &&
        sel_start != sel_end) {
      *n_selections = 1;
    } else {
      *n_selections = 0;
    }
  } else {
    *n_selections = 0;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_selection(LONG selection_index,
                                                    LONG* start_offset,
                                                    LONG* end_offset) {
  if (!instance_active_)
    return E_FAIL;

  if (!start_offset || !end_offset || selection_index != 0)
    return E_INVALIDARG;

  if (role_ == WebAccessibility::ROLE_TEXT_FIELD ||
      role_ == WebAccessibility::ROLE_TEXTAREA) {
    int sel_start = 0;
    int sel_end = 0;
    if (GetIntAttribute(WebAccessibility::ATTR_TEXT_SEL_START, &sel_start) &&
        GetIntAttribute(WebAccessibility::ATTR_TEXT_SEL_END, &sel_end)) {
      *start_offset = sel_start;
      *end_offset = sel_end;
    } else {
      *start_offset = 0;
      *end_offset = 0;
    }
  } else {
    *start_offset = 0;
    *end_offset = 0;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_text(
    LONG start_offset, LONG end_offset, BSTR* text) {
  if (!instance_active_)
    return E_FAIL;

  if (!text)
    return E_INVALIDARG;

  const string16& text_str = TextForIAccessibleText();

  // Handle special text offsets.
  HandleSpecialTextOffset(text_str, &start_offset);
  HandleSpecialTextOffset(text_str, &end_offset);

  // The spec allows the arguments to be reversed.
  if (start_offset > end_offset) {
    LONG tmp = start_offset;
    start_offset = end_offset;
    end_offset = tmp;
  }

  // The spec does not allow the start or end offsets to be out or range;
  // we must return an error if so.
  LONG len = text_str.length();
  if (start_offset < 0)
    return E_INVALIDARG;
  if (end_offset > len)
    return E_INVALIDARG;

  string16 substr = text_str.substr(start_offset, end_offset - start_offset);
  if (substr.empty())
    return S_FALSE;

  *text = SysAllocString(substr.c_str());
  DCHECK(*text);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_textAtOffset(
    LONG offset,
    enum IA2TextBoundaryType boundary_type,
    LONG* start_offset, LONG* end_offset,
    BSTR* text) {
  if (!instance_active_)
    return E_FAIL;

  if (!start_offset || !end_offset || !text)
    return E_INVALIDARG;

  // The IAccessible2 spec says we don't have to implement the "sentence"
  // boundary type, we can just let the screenreader handle it.
  if (boundary_type == IA2_TEXT_BOUNDARY_SENTENCE) {
    *start_offset = 0;
    *end_offset = 0;
    *text = NULL;
    return S_FALSE;
  }

  const string16& text_str = TextForIAccessibleText();

  *start_offset = FindBoundary(text_str, boundary_type, offset, -1);
  *end_offset = FindBoundary(text_str, boundary_type, offset, 1);
  return get_text(*start_offset, *end_offset, text);
}

STDMETHODIMP BrowserAccessibilityWin::get_textBeforeOffset(
    LONG offset,
    enum IA2TextBoundaryType boundary_type,
    LONG* start_offset, LONG* end_offset,
    BSTR* text) {
  if (!instance_active_)
    return E_FAIL;

  if (!start_offset || !end_offset || !text)
    return E_INVALIDARG;

  // The IAccessible2 spec says we don't have to implement the "sentence"
  // boundary type, we can just let the screenreader handle it.
  if (boundary_type == IA2_TEXT_BOUNDARY_SENTENCE) {
    *start_offset = 0;
    *end_offset = 0;
    *text = NULL;
    return S_FALSE;
  }

  const string16& text_str = TextForIAccessibleText();

  *start_offset = FindBoundary(text_str, boundary_type, offset, -1);
  *end_offset = offset;
  return get_text(*start_offset, *end_offset, text);
}

STDMETHODIMP BrowserAccessibilityWin::get_textAfterOffset(
    LONG offset,
    enum IA2TextBoundaryType boundary_type,
    LONG* start_offset, LONG* end_offset,
    BSTR* text) {
  if (!instance_active_)
    return E_FAIL;

  if (!start_offset || !end_offset || !text)
    return E_INVALIDARG;

  // The IAccessible2 spec says we don't have to implement the "sentence"
  // boundary type, we can just let the screenreader handle it.
  if (boundary_type == IA2_TEXT_BOUNDARY_SENTENCE) {
    *start_offset = 0;
    *end_offset = 0;
    *text = NULL;
    return S_FALSE;
  }

  const string16& text_str = TextForIAccessibleText();

  *start_offset = offset;
  *end_offset = FindBoundary(text_str, boundary_type, offset, 1);
  return get_text(*start_offset, *end_offset, text);
}

STDMETHODIMP BrowserAccessibilityWin::get_newText(IA2TextSegment* new_text) {
  if (!instance_active_)
    return E_FAIL;

  if (!new_text)
    return E_INVALIDARG;

  string16 text = TextForIAccessibleText();

  new_text->text = SysAllocString(text.c_str());
  new_text->start = 0;
  new_text->end = static_cast<long>(text.size());
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_oldText(IA2TextSegment* old_text) {
  if (!instance_active_)
    return E_FAIL;

  if (!old_text)
    return E_INVALIDARG;

  old_text->text = SysAllocString(old_text_.c_str());
  old_text->start = 0;
  old_text->end = static_cast<long>(old_text_.size());
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_offsetAtPoint(
    LONG x, LONG y, enum IA2CoordinateType coord_type, LONG* offset) {
  if (!instance_active_)
    return E_FAIL;

  if (!offset)
    return E_INVALIDARG;

  // TODO(dmazzoni): implement this. We're returning S_OK for now so that
  // screen readers still return partially accurate results rather than
  // completely failing.
  *offset = 0;
  return S_OK;
}

//
// IAccessibleValue methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_currentValue(VARIANT* value) {
  if (!instance_active_)
    return E_FAIL;

  if (!value)
    return E_INVALIDARG;

  float float_val;
  if (GetFloatAttribute(WebAccessibility::ATTR_VALUE_FOR_RANGE, &float_val)) {
    value->vt = VT_R8;
    value->dblVal = float_val;
    return S_OK;
  }

  value->vt = VT_EMPTY;
  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_minimumValue(VARIANT* value) {
  if (!instance_active_)
    return E_FAIL;

  if (!value)
    return E_INVALIDARG;

  float float_val;
  if (GetFloatAttribute(WebAccessibility::ATTR_MIN_VALUE_FOR_RANGE,
                        &float_val)) {
    value->vt = VT_R8;
    value->dblVal = float_val;
    return S_OK;
  }

  value->vt = VT_EMPTY;
  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_maximumValue(VARIANT* value) {
  if (!instance_active_)
    return E_FAIL;

  if (!value)
    return E_INVALIDARG;

  float float_val;
  if (GetFloatAttribute(WebAccessibility::ATTR_MAX_VALUE_FOR_RANGE,
                        &float_val)) {
    value->vt = VT_R8;
    value->dblVal = float_val;
    return S_OK;
  }

  value->vt = VT_EMPTY;
  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::setCurrentValue(VARIANT new_value) {
  // TODO(dmazzoni): Implement this.
  return E_NOTIMPL;
}

//
// ISimpleDOMDocument methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_URL(BSTR* url) {
  if (!instance_active_)
    return E_FAIL;

  if (!url)
    return E_INVALIDARG;

  return GetStringAttributeAsBstr(WebAccessibility::ATTR_DOC_URL, url);
}

STDMETHODIMP BrowserAccessibilityWin::get_title(BSTR* title) {
  if (!instance_active_)
    return E_FAIL;

  if (!title)
    return E_INVALIDARG;

  return GetStringAttributeAsBstr(WebAccessibility::ATTR_DOC_TITLE, title);
}

STDMETHODIMP BrowserAccessibilityWin::get_mimeType(BSTR* mime_type) {
  if (!instance_active_)
    return E_FAIL;

  if (!mime_type)
    return E_INVALIDARG;

  return GetStringAttributeAsBstr(
      WebAccessibility::ATTR_DOC_MIMETYPE, mime_type);
}

STDMETHODIMP BrowserAccessibilityWin::get_docType(BSTR* doc_type) {
  if (!instance_active_)
    return E_FAIL;

  if (!doc_type)
    return E_INVALIDARG;

  return GetStringAttributeAsBstr(WebAccessibility::ATTR_DOC_DOCTYPE, doc_type);
}

//
// ISimpleDOMNode methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_nodeInfo(
    BSTR* node_name,
    short* name_space_id,
    BSTR* node_value,
    unsigned int* num_children,
    unsigned int* unique_id,
    unsigned short* node_type) {
  if (!instance_active_)
    return E_FAIL;

  if (!node_name || !name_space_id || !node_value || !num_children ||
      !unique_id || !node_type) {
    return E_INVALIDARG;
  }

  string16 tag;
  if (GetStringAttribute(WebAccessibility::ATTR_HTML_TAG, &tag))
    *node_name = SysAllocString(tag.c_str());
  else
    *node_name = NULL;

  *name_space_id = 0;
  *node_value = SysAllocString(value_.c_str());
  *num_children = children_.size();
  *unique_id = child_id_;

  if (ia_role_ == ROLE_SYSTEM_DOCUMENT) {
    *node_type = NODETYPE_DOCUMENT;
  } else if (ia_role_ == ROLE_SYSTEM_TEXT &&
             ((ia2_state_ & IA2_STATE_EDITABLE) == 0)) {
    *node_type = NODETYPE_TEXT;
  } else {
    *node_type = NODETYPE_ELEMENT;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_attributes(
    unsigned short max_attribs,
    BSTR* attrib_names,
    short* name_space_id,
    BSTR* attrib_values,
    unsigned short* num_attribs) {
  if (!instance_active_)
    return E_FAIL;

  if (!attrib_names || !name_space_id || !attrib_values || !num_attribs)
    return E_INVALIDARG;

  *num_attribs = max_attribs;
  if (*num_attribs > html_attributes_.size())
    *num_attribs = html_attributes_.size();

  for (unsigned short i = 0; i < *num_attribs; ++i) {
    attrib_names[i] = SysAllocString(html_attributes_[i].first.c_str());
    name_space_id[i] = 0;
    attrib_values[i] = SysAllocString(html_attributes_[i].second.c_str());
  }
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_attributesForNames(
    unsigned short num_attribs,
    BSTR* attrib_names,
    short* name_space_id,
    BSTR* attrib_values) {
  if (!instance_active_)
    return E_FAIL;

  if (!attrib_names || !name_space_id || !attrib_values)
    return E_INVALIDARG;

  for (unsigned short i = 0; i < num_attribs; ++i) {
    name_space_id[i] = 0;
    bool found = false;
    string16 name = (LPCWSTR)attrib_names[i];
    for (unsigned int j = 0;  j < html_attributes_.size(); ++j) {
      if (html_attributes_[j].first == name) {
        attrib_values[i] = SysAllocString(html_attributes_[j].second.c_str());
        found = true;
        break;
      }
    }
    if (!found) {
      attrib_values[i] = NULL;
    }
  }
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_computedStyle(
    unsigned short max_style_properties,
    boolean use_alternate_view,
    BSTR *style_properties,
    BSTR *style_values,
    unsigned short *num_style_properties)  {
  if (!instance_active_)
    return E_FAIL;

  if (!style_properties || !style_values)
    return E_INVALIDARG;

  // We only cache a single style property for now: DISPLAY

  string16 display;
  if (max_style_properties == 0 ||
      !GetStringAttribute(WebAccessibility::ATTR_DISPLAY, &display)) {
    *num_style_properties = 0;
    return S_OK;
  }

  *num_style_properties = 1;
  style_properties[0] = SysAllocString(L"display");
  style_values[0] = SysAllocString(display.c_str());

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_computedStyleForProperties(
    unsigned short num_style_properties,
    boolean use_alternate_view,
    BSTR* style_properties,
    BSTR* style_values) {
  if (!instance_active_)
    return E_FAIL;

  if (!style_properties || !style_values)
    return E_INVALIDARG;

  // We only cache a single style property for now: DISPLAY

  for (unsigned short i = 0; i < num_style_properties; ++i) {
    string16 name = (LPCWSTR)style_properties[i];
    StringToLowerASCII(&name);
    if (name == L"display") {
      string16 display;
      GetStringAttribute(WebAccessibility::ATTR_DISPLAY, &display);
      style_values[i] = SysAllocString(display.c_str());
    } else {
      style_values[i] = NULL;
    }
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::scrollTo(boolean placeTopLeft) {
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::get_parentNode(ISimpleDOMNode** node) {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  *node = parent_->toBrowserAccessibilityWin()->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_firstChild(ISimpleDOMNode** node)  {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (children_.size()) {
    *node = children_[0]->toBrowserAccessibilityWin()->NewReference();
    return S_OK;
  } else {
    *node = NULL;
    return S_FALSE;
  }
}

STDMETHODIMP BrowserAccessibilityWin::get_lastChild(ISimpleDOMNode** node) {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (children_.size()) {
    *node = children_[children_.size() - 1]->toBrowserAccessibilityWin()->
        NewReference();
    return S_OK;
  } else {
    *node = NULL;
    return S_FALSE;
  }
}

STDMETHODIMP BrowserAccessibilityWin::get_previousSibling(
    ISimpleDOMNode** node) {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (parent_ && index_in_parent_ > 0) {
    *node = parent_->children()[index_in_parent_ - 1]->
        toBrowserAccessibilityWin()->NewReference();
    return S_OK;
  } else {
    *node = NULL;
    return S_FALSE;
  }
}

STDMETHODIMP BrowserAccessibilityWin::get_nextSibling(ISimpleDOMNode** node) {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (parent_ &&
      index_in_parent_ >= 0 &&
      index_in_parent_ < static_cast<int>(parent_->children().size()) - 1) {
    *node = parent_->children()[index_in_parent_ + 1]->
        toBrowserAccessibilityWin()->NewReference();
    return S_OK;
  } else {
    *node = NULL;
    return S_FALSE;
  }
}

STDMETHODIMP BrowserAccessibilityWin::get_childAt(
    unsigned int child_index,
    ISimpleDOMNode** node) {
  if (!instance_active_)
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (child_index < children_.size()) {
    *node = children_[child_index]->toBrowserAccessibilityWin()->NewReference();
    return S_OK;
  } else {
    *node = NULL;
    return S_FALSE;
  }
}

//
// ISimpleDOMText methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_domText(BSTR* dom_text) {
  if (!instance_active_)
    return E_FAIL;

  if (!dom_text)
    return E_INVALIDARG;

  if (name_.empty())
    return S_FALSE;

  *dom_text = SysAllocString(name_.c_str());
  DCHECK(*dom_text);
  return S_OK;
}

//
// IServiceProvider methods.
//

STDMETHODIMP BrowserAccessibilityWin::QueryService(
    REFGUID guidService, REFIID riid, void** object) {
  if (!instance_active_)
    return E_FAIL;

  if (guidService == IID_IAccessible ||
      guidService == IID_IAccessible2 ||
      guidService == IID_IAccessibleImage ||
      guidService == IID_IAccessibleTable ||
      guidService == IID_IAccessibleTable2 ||
      guidService == IID_IAccessibleTableCell ||
      guidService == IID_IAccessibleText ||
      guidService == IID_IAccessibleValue ||
      guidService == IID_ISimpleDOMDocument ||
      guidService == IID_ISimpleDOMNode ||
      guidService == IID_ISimpleDOMText ||
      guidService == GUID_ISimpleDOM) {
    return QueryInterface(riid, object);
  }

  *object = NULL;
  return E_FAIL;
}

//
// CComObjectRootEx methods.
//

HRESULT WINAPI BrowserAccessibilityWin::InternalQueryInterface(
    void* this_ptr,
    const _ATL_INTMAP_ENTRY* entries,
    REFIID iid,
    void** object) {
  if (iid == IID_IAccessibleText) {
    if (ia_role_ != ROLE_SYSTEM_LINK && ia_role_ != ROLE_SYSTEM_TEXT) {
      *object = NULL;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_IAccessibleImage) {
    if (ia_role_ != ROLE_SYSTEM_GRAPHIC) {
      *object = NULL;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_IAccessibleTable || iid == IID_IAccessibleTable2) {
    if (ia_role_ != ROLE_SYSTEM_TABLE) {
      *object = NULL;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_IAccessibleTableCell) {
    if (ia_role_ != ROLE_SYSTEM_CELL) {
      *object = NULL;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_IAccessibleValue) {
    if (ia_role_ != ROLE_SYSTEM_PROGRESSBAR &&
        ia_role_ != ROLE_SYSTEM_SCROLLBAR &&
        ia_role_ != ROLE_SYSTEM_SLIDER) {
      *object = NULL;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_ISimpleDOMDocument) {
    if (ia_role_ != ROLE_SYSTEM_DOCUMENT) {
      *object = NULL;
      return E_NOINTERFACE;
    }
  }

  return CComObjectRootBase::InternalQueryInterface(
      this_ptr, entries, iid, object);
}

//
// Private methods.
//

// Initialize this object and mark it as active.
void BrowserAccessibilityWin::Initialize() {
  BrowserAccessibility::Initialize();

  InitRoleAndState();

  // Expose headings levels with the "level" attribute.
  if (role_ == WebAccessibility::ROLE_HEADING && role_name_.size() == 2 &&
          IsAsciiDigit(role_name_[1])) {
    ia2_attributes_.push_back(string16(L"level:") + role_name_.substr(1));
  }

  // Expose the "display" and "tag" attributes.
  StringAttributeToIA2(WebAccessibility::ATTR_DISPLAY, "display");
  StringAttributeToIA2(WebAccessibility::ATTR_HTML_TAG, "tag");
  StringAttributeToIA2(WebAccessibility::ATTR_ROLE, "xml-roles");

  // Expose "level" attribute for tree nodes.
  IntAttributeToIA2(WebAccessibility::ATTR_HIERARCHICAL_LEVEL, "level");

  // Expose live region attributes.
  StringAttributeToIA2(WebAccessibility::ATTR_LIVE_STATUS, "live");
  StringAttributeToIA2(WebAccessibility::ATTR_LIVE_RELEVANT, "relevant");
  BoolAttributeToIA2(WebAccessibility::ATTR_LIVE_ATOMIC, "atomic");
  BoolAttributeToIA2(WebAccessibility::ATTR_LIVE_BUSY, "busy");

  // Expose container live region attributes.
  StringAttributeToIA2(WebAccessibility::ATTR_CONTAINER_LIVE_STATUS,
                       "container-live");
  StringAttributeToIA2(WebAccessibility::ATTR_CONTAINER_LIVE_RELEVANT,
                       "container-relevant");
  BoolAttributeToIA2(WebAccessibility::ATTR_CONTAINER_LIVE_ATOMIC,
                     "container-atomic");
  BoolAttributeToIA2(WebAccessibility::ATTR_CONTAINER_LIVE_BUSY,
                     "container-busy");

  // Expose slider value.
  if (ia_role_ == ROLE_SYSTEM_PROGRESSBAR ||
      ia_role_ == ROLE_SYSTEM_SCROLLBAR ||
      ia_role_ == ROLE_SYSTEM_SLIDER) {
    float fval;
    if (value_.empty() &&
        GetFloatAttribute(WebAccessibility::ATTR_VALUE_FOR_RANGE, &fval)) {
      // TODO(dmazzoni): Use ICU to localize this?
      value_ = UTF8ToUTF16(base::DoubleToString(fval));
    }
    ia2_attributes_.push_back(L"valuetext:" + value_);
  }

  // Expose table cell index.
  if (ia_role_ == ROLE_SYSTEM_CELL) {
    BrowserAccessibility* table = parent();
    while (table && table->role() != WebAccessibility::ROLE_TABLE)
      table = table->parent();
    if (table) {
      const std::vector<int32>& unique_cell_ids = table->unique_cell_ids();
      int index = -1;
      for (size_t i = 0; i < unique_cell_ids.size(); ++i) {
        if (unique_cell_ids[i] == renderer_id_) {
          index = static_cast<int>(i);
          break;
        }
      }
      if (index >= 0) {
        ia2_attributes_.push_back(string16(L"table-cell-index:") +
                                  base::IntToString16(index));
      }
    } else {
      NOTREACHED();
    }
  }

  // If this is static text, put the text in the name rather than the value.
  if (role_ == WebAccessibility::ROLE_STATIC_TEXT && name_.empty())
    name_.swap(value_);

  // If this object doesn't have a name but it does have a description,
  // use the description as its name - because some screen readers only
  // announce the name.
  if (name_.empty())
    GetStringAttribute(WebAccessibility::ATTR_DESCRIPTION, &name_);

  // If this doesn't have a value and is linked then set its value to the url
  // attribute. This allows screen readers to read an empty link's destination.
  string16 url;
  if (value_.empty() && (ia_state_ & STATE_SYSTEM_LINKED))
    GetStringAttribute(WebAccessibility::ATTR_URL, &value_);
}

void BrowserAccessibilityWin::SendNodeUpdateEvents() {
  // Fire an event when an alert first appears.
  if (role_ == WebAccessibility::ROLE_ALERT && first_time_)
    manager_->NotifyAccessibilityEvent(ViewHostMsg_AccEvent::ALERT, this);

  // Fire events if text has changed.
  string16 text = TextForIAccessibleText();
  if (previous_text_ != text) {
    if (!previous_text_.empty() && !text.empty()) {
      manager_->NotifyAccessibilityEvent(
          ViewHostMsg_AccEvent::OBJECT_SHOW, this);
    }

    // TODO(dmazzoni): Look into HIDE events, too.

    old_text_ = previous_text_;
    previous_text_ = text;
  }

  first_time_ = false;
}

void BrowserAccessibilityWin::NativeAddReference() {
  AddRef();
}

void BrowserAccessibilityWin::NativeReleaseReference() {
  Release();
}

BrowserAccessibilityWin* BrowserAccessibilityWin::NewReference() {
  AddRef();
  return this;
}

BrowserAccessibilityWin* BrowserAccessibilityWin::GetTargetFromChildID(
    const VARIANT& var_id) {
  if (var_id.vt != VT_I4)
    return NULL;

  LONG child_id = var_id.lVal;
  if (child_id == CHILDID_SELF)
    return this;

  if (child_id >= 1 && child_id <= static_cast<LONG>(children_.size()))
    return children_[child_id - 1]->toBrowserAccessibilityWin();

  return manager_->GetFromChildID(child_id)->toBrowserAccessibilityWin();
}

HRESULT BrowserAccessibilityWin::GetStringAttributeAsBstr(
    WebAccessibility::StringAttribute attribute, BSTR* value_bstr) {
  string16 str;

  if (!GetStringAttribute(attribute, &str))
    return S_FALSE;

  if (str.empty())
    return S_FALSE;

  *value_bstr = SysAllocString(str.c_str());
  DCHECK(*value_bstr);

  return S_OK;
}

void BrowserAccessibilityWin::StringAttributeToIA2(
    WebAccessibility::StringAttribute attribute, const char* ia2_attr) {
  string16 value;
  if (GetStringAttribute(attribute, &value))
    ia2_attributes_.push_back(ASCIIToUTF16(ia2_attr) + L":" + value);
}

void BrowserAccessibilityWin::BoolAttributeToIA2(
    WebAccessibility::BoolAttribute attribute, const char* ia2_attr) {
  bool value;
  if (GetBoolAttribute(attribute, &value)) {
    ia2_attributes_.push_back((ASCIIToUTF16(ia2_attr) + L":") +
                              (value ? L"true" : L"false"));
  }
}

void BrowserAccessibilityWin::IntAttributeToIA2(
    WebAccessibility::IntAttribute attribute, const char* ia2_attr) {
  int value;
  if (GetIntAttribute(attribute, &value))
    ia2_attributes_.push_back(ASCIIToUTF16(ia2_attr) + L":" +
                              base::IntToString16(value));
}

string16 BrowserAccessibilityWin::Escape(const string16& str) {
  return net::EscapeQueryParamValueUTF8(str, false);
}

const string16& BrowserAccessibilityWin::TextForIAccessibleText() {
  if (IsEditableText()) {
    return value_;
  } else {
    return name_;
  }
}

void BrowserAccessibilityWin::HandleSpecialTextOffset(
    const string16& text, LONG* offset) {
  if (*offset == IA2_TEXT_OFFSET_LENGTH) {
    *offset = static_cast<LONG>(text.size());
  } else if (*offset == IA2_TEXT_OFFSET_CARET) {
    get_caretOffset(offset);
  }
}

LONG BrowserAccessibilityWin::FindBoundary(
    const string16& text,
    IA2TextBoundaryType boundary,
    LONG start_offset,
    LONG direction) {
  LONG text_size = static_cast<LONG>(text.size());
  DCHECK((start_offset >= 0 && start_offset <= text_size) ||
         start_offset == IA2_TEXT_OFFSET_LENGTH ||
         start_offset == IA2_TEXT_OFFSET_CARET);
  DCHECK(direction == 1 || direction == -1);

  HandleSpecialTextOffset(text, &start_offset);

  if (boundary == IA2_TEXT_BOUNDARY_CHAR) {
    if (direction == 1 && start_offset < text_size)
      return start_offset + 1;
    else
      return start_offset;
  } else if (boundary == IA2_TEXT_BOUNDARY_LINE) {
    if (direction == 1) {
      for (int j = 0; j < static_cast<int>(line_breaks_.size()); ++j) {
        if (line_breaks_[j] > start_offset)
          return line_breaks_[j];
      }
      return text_size;
    } else {
      for (int j = static_cast<int>(line_breaks_.size()) - 1; j >= 0; j--) {
        if (line_breaks_[j] <= start_offset)
          return line_breaks_[j];
      }
      return 0;
    }
  }

  LONG result = start_offset;
  for (;;) {
    LONG pos;
    if (direction == 1) {
      if (result >= text_size)
        return text_size;
      pos = result;
    } else {
      if (result <= 0)
        return 0;
      pos = result - 1;
    }

    switch (boundary) {
      case IA2_TEXT_BOUNDARY_CHAR:
      case IA2_TEXT_BOUNDARY_LINE:
        NOTREACHED();  // These are handled above.
        break;
      case IA2_TEXT_BOUNDARY_WORD:
        if (IsWhitespace(text[pos]))
          return result;
        break;
      case IA2_TEXT_BOUNDARY_PARAGRAPH:
        if (text[pos] == '\n')
          return result;
      case IA2_TEXT_BOUNDARY_SENTENCE:
        // Note that we don't actually have to implement sentence support;
        // currently IAccessibleText functions return S_FALSE so that
        // screenreaders will handle it on their own.
        if ((text[pos] == '.' || text[pos] == '!' || text[pos] == '?') &&
            (pos == text_size - 1 || IsWhitespace(text[pos + 1]))) {
          return result;
        }
      case IA2_TEXT_BOUNDARY_ALL:
      default:
        break;
    }

    if (direction > 0) {
      result++;
    } else if (direction < 0) {
      result--;
    } else {
      NOTREACHED();
      return result;
    }
  }
}

BrowserAccessibilityWin* BrowserAccessibilityWin::GetFromRendererID(
    int32 renderer_id) {
  return manager_->GetFromRendererID(renderer_id)->toBrowserAccessibilityWin();
}

void BrowserAccessibilityWin::InitRoleAndState() {
  ia_state_ = 0;
  ia2_state_ = IA2_STATE_OPAQUE;
  ia2_attributes_.clear();

  if ((state_ >> WebAccessibility::STATE_BUSY) & 1)
    ia_state_|= STATE_SYSTEM_BUSY;
  if ((state_ >> WebAccessibility::STATE_CHECKED) & 1)
    ia_state_ |= STATE_SYSTEM_CHECKED;
  if ((state_ >> WebAccessibility::STATE_COLLAPSED) & 1)
    ia_state_|= STATE_SYSTEM_COLLAPSED;
  if ((state_ >> WebAccessibility::STATE_EXPANDED) & 1)
    ia_state_|= STATE_SYSTEM_EXPANDED;
  if ((state_ >> WebAccessibility::STATE_FOCUSABLE) & 1)
    ia_state_|= STATE_SYSTEM_FOCUSABLE;
  if ((state_ >> WebAccessibility::STATE_HASPOPUP) & 1)
    ia_state_|= STATE_SYSTEM_HASPOPUP;
  if ((state_ >> WebAccessibility::STATE_HOTTRACKED) & 1)
    ia_state_|= STATE_SYSTEM_HOTTRACKED;
  if ((state_ >> WebAccessibility::STATE_INDETERMINATE) & 1)
    ia_state_|= STATE_SYSTEM_INDETERMINATE;
  if ((state_ >> WebAccessibility::STATE_INVISIBLE) & 1)
    ia_state_|= STATE_SYSTEM_INVISIBLE;
  if ((state_ >> WebAccessibility::STATE_LINKED) & 1)
    ia_state_|= STATE_SYSTEM_LINKED;
  if ((state_ >> WebAccessibility::STATE_MULTISELECTABLE) & 1)
    ia_state_|= STATE_SYSTEM_MULTISELECTABLE;
  // TODO(ctguil): Support STATE_SYSTEM_EXTSELECTABLE/accSelect.
  if ((state_ >> WebAccessibility::STATE_OFFSCREEN) & 1)
    ia_state_|= STATE_SYSTEM_OFFSCREEN;
  if ((state_ >> WebAccessibility::STATE_PRESSED) & 1)
    ia_state_|= STATE_SYSTEM_PRESSED;
  if ((state_ >> WebAccessibility::STATE_PROTECTED) & 1)
    ia_state_|= STATE_SYSTEM_PROTECTED;
  if ((state_ >> WebAccessibility::STATE_REQUIRED) & 1)
    ia2_state_|= IA2_STATE_REQUIRED;
  if ((state_ >> WebAccessibility::STATE_SELECTABLE) & 1)
    ia_state_|= STATE_SYSTEM_SELECTABLE;
  if ((state_ >> WebAccessibility::STATE_SELECTED) & 1)
    ia_state_|= STATE_SYSTEM_SELECTED;
  if ((state_ >> WebAccessibility::STATE_TRAVERSED) & 1)
    ia_state_|= STATE_SYSTEM_TRAVERSED;
  if ((state_ >> WebAccessibility::STATE_UNAVAILABLE) & 1)
    ia_state_|= STATE_SYSTEM_UNAVAILABLE;
  if ((state_ >> WebAccessibility::STATE_VERTICAL) & 1) {
    ia2_state_|= IA2_STATE_VERTICAL;
  } else {
    ia2_state_|= IA2_STATE_HORIZONTAL;
  }
  if ((state_ >> WebAccessibility::STATE_VISITED) & 1)
    ia_state_|= STATE_SYSTEM_TRAVERSED;

  // The meaning of the readonly state on Windows is very different from
  // the meaning of the readonly state in WebKit, so we ignore
  // WebAccessibility::STATE_READONLY, and instead just check the
  // aria readonly value, then there's additional logic below to set
  // the readonly state based on other criteria.
  bool aria_readonly = false;
  GetBoolAttribute(WebAccessibility::ATTR_ARIA_READONLY, &aria_readonly);
  if (aria_readonly)
    ia_state_|= STATE_SYSTEM_READONLY;

  string16 invalid;
  if (GetHtmlAttribute("aria-invalid", &invalid))
    ia2_state_|= IA2_STATE_INVALID_ENTRY;

  bool mixed = false;
  GetBoolAttribute(WebAccessibility::ATTR_BUTTON_MIXED, &mixed);
  if (mixed)
    ia_state_|= STATE_SYSTEM_MIXED;

  bool editable = false;
  GetBoolAttribute(WebAccessibility::ATTR_CAN_SET_VALUE, &editable);
  if (editable)
    ia2_state_ |= IA2_STATE_EDITABLE;

  string16 html_tag;
  GetStringAttribute(WebAccessibility::ATTR_HTML_TAG, &html_tag);
  ia_role_ = 0;
  ia2_role_ = 0;
  switch (role_) {
    case WebAccessibility::ROLE_ALERT:
      ia_role_ = ROLE_SYSTEM_ALERT;
      break;
    case WebAccessibility::ROLE_ALERT_DIALOG:
      ia_role_ = ROLE_SYSTEM_DIALOG;
      break;
    case WebAccessibility::ROLE_APPLICATION:
      ia_role_ = ROLE_SYSTEM_APPLICATION;
      break;
    case WebAccessibility::ROLE_ARTICLE:
      ia_role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_SECTION;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_BUSY_INDICATOR:
      ia_role_ = ROLE_SYSTEM_ANIMATION;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_BUTTON:
      ia_role_ = ROLE_SYSTEM_PUSHBUTTON;
      break;
    case WebAccessibility::ROLE_CELL:
      ia_role_ = ROLE_SYSTEM_CELL;
      break;
    case WebAccessibility::ROLE_CHECKBOX:
      ia_role_ = ROLE_SYSTEM_CHECKBUTTON;
      break;
    case WebAccessibility::ROLE_COLOR_WELL:
      ia_role_ = ROLE_SYSTEM_CLIENT;
      ia2_role_ = IA2_ROLE_COLOR_CHOOSER;
      break;
    case WebAccessibility::ROLE_COLUMN:
      ia_role_ = ROLE_SYSTEM_COLUMN;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_COLUMN_HEADER:
      ia_role_ = ROLE_SYSTEM_COLUMNHEADER;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_COMBO_BOX:
      ia_role_ = ROLE_SYSTEM_COMBOBOX;
      break;
    case WebAccessibility::ROLE_DEFINITION_LIST_DEFINITION:
      role_name_ = html_tag;
      ia2_role_ = IA2_ROLE_PARAGRAPH;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_DEFINITION_LIST_TERM:
      ia_role_ = ROLE_SYSTEM_LISTITEM;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_DIALOG:
      ia_role_ = ROLE_SYSTEM_DIALOG;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_DISCLOSURE_TRIANGLE:
      ia_role_ = ROLE_SYSTEM_OUTLINEBUTTON;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_DOCUMENT:
    case WebAccessibility::ROLE_ROOT_WEB_AREA:
    case WebAccessibility::ROLE_WEB_AREA:
      ia_role_ = ROLE_SYSTEM_DOCUMENT;
      ia_state_|= STATE_SYSTEM_READONLY;
      ia_state_|= STATE_SYSTEM_FOCUSABLE;
      break;
    case WebAccessibility::ROLE_EDITABLE_TEXT:
      ia_role_ = ROLE_SYSTEM_TEXT;
      ia2_state_ |= IA2_STATE_SINGLE_LINE;
      ia2_state_ |= IA2_STATE_EDITABLE;
      break;
    case WebAccessibility::ROLE_GRID:
      ia_role_ = ROLE_SYSTEM_TABLE;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_GROUP:
      if (html_tag == L"li") {
        ia_role_ = ROLE_SYSTEM_LISTITEM;
      } else if (html_tag == L"form") {
        role_name_ = html_tag;
        ia2_role_ = IA2_ROLE_FORM;
      } else if (html_tag == L"p") {
        role_name_ = html_tag;
        ia2_role_ = IA2_ROLE_PARAGRAPH;
      } else {
        if (html_tag.empty())
          role_name_ = L"div";
        else
          role_name_ = html_tag;
        ia2_role_ = IA2_ROLE_SECTION;
      }
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_GROW_AREA:
      ia_role_ = ROLE_SYSTEM_GRIP;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_HEADING:
      role_name_ = html_tag;
      ia2_role_ = IA2_ROLE_HEADING;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_IMAGE:
      ia_role_ = ROLE_SYSTEM_GRAPHIC;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_IMAGE_MAP:
      role_name_ = html_tag;
      ia2_role_ = IA2_ROLE_IMAGE_MAP;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_IMAGE_MAP_LINK:
      ia_role_ = ROLE_SYSTEM_LINK;
      ia_state_|= STATE_SYSTEM_LINKED;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_LANDMARK_APPLICATION:
    case WebAccessibility::ROLE_LANDMARK_BANNER:
    case WebAccessibility::ROLE_LANDMARK_COMPLEMENTARY:
    case WebAccessibility::ROLE_LANDMARK_CONTENTINFO:
    case WebAccessibility::ROLE_LANDMARK_MAIN:
    case WebAccessibility::ROLE_LANDMARK_NAVIGATION:
    case WebAccessibility::ROLE_LANDMARK_SEARCH:
      ia_role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_SECTION;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_LINK:
    case WebAccessibility::ROLE_WEBCORE_LINK:
      ia_role_ = ROLE_SYSTEM_LINK;
      ia_state_|= STATE_SYSTEM_LINKED;
      break;
    case WebAccessibility::ROLE_LIST:
      ia_role_ = ROLE_SYSTEM_LIST;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_LISTBOX:
      ia_role_ = ROLE_SYSTEM_LIST;
      break;
    case WebAccessibility::ROLE_LISTBOX_OPTION:
      ia_role_ = ROLE_SYSTEM_LISTITEM;
      break;
    case WebAccessibility::ROLE_LIST_ITEM:
      ia_role_ = ROLE_SYSTEM_LISTITEM;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_LIST_MARKER:
      ia_role_ = ROLE_SYSTEM_TEXT;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_MATH:
      ia_role_ = ROLE_SYSTEM_EQUATION;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_MENU:
    case WebAccessibility::ROLE_MENU_BUTTON:
      ia_role_ = ROLE_SYSTEM_MENUPOPUP;
      break;
    case WebAccessibility::ROLE_MENU_BAR:
      ia_role_ = ROLE_SYSTEM_MENUBAR;
      break;
    case WebAccessibility::ROLE_MENU_ITEM:
    case WebAccessibility::ROLE_MENU_LIST_OPTION:
      ia_role_ = ROLE_SYSTEM_MENUITEM;
      break;
    case WebAccessibility::ROLE_MENU_LIST_POPUP:
      ia_role_ = ROLE_SYSTEM_MENUPOPUP;
      break;
    case WebAccessibility::ROLE_NOTE:
      ia_role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_NOTE;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_OUTLINE:
      ia_role_ = ROLE_SYSTEM_OUTLINE;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_POPUP_BUTTON:
      if (html_tag == L"select") {
        ia_role_ = ROLE_SYSTEM_COMBOBOX;
      } else {
        ia_role_ = ROLE_SYSTEM_BUTTONMENU;
      }
      break;
    case WebAccessibility::ROLE_PROGRESS_INDICATOR:
      ia_role_ = ROLE_SYSTEM_PROGRESSBAR;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_RADIO_BUTTON:
      ia_role_ = ROLE_SYSTEM_RADIOBUTTON;
      break;
    case WebAccessibility::ROLE_RADIO_GROUP:
      ia_role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_SECTION;
      break;
    case WebAccessibility::ROLE_REGION:
      ia_role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_SECTION;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_ROW:
      ia_role_ = ROLE_SYSTEM_ROW;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_ROW_HEADER:
      ia_role_ = ROLE_SYSTEM_ROWHEADER;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_RULER:
      ia_role_ = ROLE_SYSTEM_CLIENT;
      ia2_role_ = IA2_ROLE_RULER;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_SCROLLAREA:
      ia_role_ = ROLE_SYSTEM_CLIENT;
      ia2_role_ = IA2_ROLE_SCROLL_PANE;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_SCROLLBAR:
      ia_role_ = ROLE_SYSTEM_SCROLLBAR;
      break;
    case WebAccessibility::ROLE_SLIDER:
      ia_role_ = ROLE_SYSTEM_SLIDER;
      break;
    case WebAccessibility::ROLE_SPLIT_GROUP:
      ia_role_ = ROLE_SYSTEM_CLIENT;
      ia2_role_ = IA2_ROLE_SPLIT_PANE;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_ANNOTATION:
    case WebAccessibility::ROLE_STATIC_TEXT:
      ia_role_ = ROLE_SYSTEM_TEXT;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_STATUS:
      ia_role_ = ROLE_SYSTEM_STATUSBAR;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_SPLITTER:
      ia_role_ = ROLE_SYSTEM_SEPARATOR;
      break;
    case WebAccessibility::ROLE_TAB:
      ia_role_ = ROLE_SYSTEM_PAGETAB;
      break;
    case WebAccessibility::ROLE_TABLE:
      ia_role_ = ROLE_SYSTEM_TABLE;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_TABLE_HEADER_CONTAINER:
      ia_role_ = ROLE_SYSTEM_GROUPING;
      ia2_role_ = IA2_ROLE_SECTION;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_TAB_GROUP:
    case WebAccessibility::ROLE_TAB_LIST:
    case WebAccessibility::ROLE_TAB_PANEL:
      ia_role_ = ROLE_SYSTEM_PAGETABLIST;
      break;
    case WebAccessibility::ROLE_TEXTAREA:
      ia_role_ = ROLE_SYSTEM_TEXT;
      ia2_state_ |= IA2_STATE_MULTI_LINE;
      ia2_state_ |= IA2_STATE_EDITABLE;
      ia2_state_ |= IA2_STATE_SELECTABLE_TEXT;
      break;
    case WebAccessibility::ROLE_TEXT_FIELD:
      ia_role_ = ROLE_SYSTEM_TEXT;
      ia2_state_ |= IA2_STATE_SINGLE_LINE;
      ia2_state_ |= IA2_STATE_EDITABLE;
      ia2_state_ |= IA2_STATE_SELECTABLE_TEXT;
      break;
    case WebAccessibility::ROLE_TIMER:
      ia_role_ = ROLE_SYSTEM_CLOCK;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_TOOLBAR:
      ia_role_ = ROLE_SYSTEM_TOOLBAR;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_TOOLTIP:
      ia_role_ = ROLE_SYSTEM_TOOLTIP;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_TREE:
      ia_role_ = ROLE_SYSTEM_OUTLINE;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_TREE_GRID:
      ia_role_ = ROLE_SYSTEM_OUTLINE;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_TREE_ITEM:
      ia_role_ = ROLE_SYSTEM_OUTLINEITEM;
      ia_state_|= STATE_SYSTEM_READONLY;
      break;
    case WebAccessibility::ROLE_WINDOW:
      ia_role_ = ROLE_SYSTEM_WINDOW;
      break;

    // TODO(dmazzoni): figure out the proper MSAA role for all of these.
    case WebAccessibility::ROLE_BROWSER:
    case WebAccessibility::ROLE_DIRECTORY:
    case WebAccessibility::ROLE_DRAWER:
    case WebAccessibility::ROLE_HELP_TAG:
    case WebAccessibility::ROLE_IGNORED:
    case WebAccessibility::ROLE_INCREMENTOR:
    case WebAccessibility::ROLE_LOG:
    case WebAccessibility::ROLE_MARQUEE:
    case WebAccessibility::ROLE_MATTE:
    case WebAccessibility::ROLE_RULER_MARKER:
    case WebAccessibility::ROLE_SHEET:
    case WebAccessibility::ROLE_SLIDER_THUMB:
    case WebAccessibility::ROLE_SYSTEM_WIDE:
    case WebAccessibility::ROLE_VALUE_INDICATOR:
    default:
      ia_role_ = ROLE_SYSTEM_CLIENT;
      break;
  }

  // The role should always be set.
  DCHECK(!role_name_.empty() || ia_role_);

  // If we didn't explicitly set the IAccessible2 role, make it the same
  // as the MSAA role.
  if (!ia2_role_)
    ia2_role_ = ia_role_;
}
