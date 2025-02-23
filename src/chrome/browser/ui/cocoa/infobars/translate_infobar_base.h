// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TRANSLATE_INFOBAR_BASE_H_
#define CHROME_BROWSER_UI_COCOA_TRANSLATE_INFOBAR_BASE_H_
#pragma once

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"
#import "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/translate/languages_menu_model.h"
#include "chrome/browser/translate/options_menu_model.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_controller.h"
#include "chrome/common/translate_errors.h"

class TranslateInfoBarMenuModel;

#pragma mark TranslateInfoBarUtilities helper functions.
namespace TranslateInfoBarUtilities {

// Move the |toMove| view |spacing| pixels before/after the |anchor| view.
// |after| signifies the side of |anchor| on which to place |toMove|.
void MoveControl(NSView* anchor, NSView* toMove, int spacing, bool after);

// Vertically center |toMove| in its container.
void VerticallyCenterView(NSView *toMove);
// Check that the control |before| is ordered visually before the |after|
// control.
// Also, check that there is space between them.
bool VerifyControlOrderAndSpacing(id before, id after);

// Creates a label control in the style we need for the translate infobar's
// labels within |bounds|.
NSTextField* CreateLabel(NSRect bounds);

// Adds an item with the specified properties to |menu|.
void AddMenuItem(NSMenu *menu, id target, SEL selector, NSString* title,
    int tag, bool enabled, bool checked);

}  // namespace

// The base class for the three translate infobars.  This class does all of the
// heavy UI lifting, while deferring to the subclass to tell it what views
// should be shown and where.  Subclasses need to implement:
// - (void)layout;
// - (void)loadLabelText;
// - (void)visibleControls;
// - (bool)verifyLayout; // For testing.
@interface TranslateInfoBarControllerBase : InfoBarController<NSMenuDelegate> {
 @protected
  scoped_nsobject<NSTextField> label1_;
  scoped_nsobject<NSTextField> label2_;
  scoped_nsobject<NSTextField> label3_;
  scoped_nsobject<NSPopUpButton> fromLanguagePopUp_;
  scoped_nsobject<NSPopUpButton> toLanguagePopUp_;
  scoped_nsobject<NSPopUpButton> optionsPopUp_;
  scoped_nsobject<NSButton> showOriginalButton_;
  // This is the button used in the translate message infobar.  It can either be
  // a "Try Again" button, or a "Show Original" button in the case that the
  // page was translated from an unknown language.
  scoped_nsobject<NSButton> translateMessageButton_;

  // In the current locale, are the "from" and "to" language popup menu
  // flipped from what they'd appear in English.
  bool swappedLanguagePlaceholders_;

  // Space between controls in pixels - read from the NIB.
  CGFloat spaceBetweenControls_;

  scoped_ptr<LanguagesMenuModel> originalLanguageMenuModel_;
  scoped_ptr<LanguagesMenuModel> targetLanguageMenuModel_;
  scoped_ptr<OptionsMenuModel> optionsMenuModel_;
}

// Returns the delegate as a TranslateInfoBarDelegate.  The return
// value can be NULL if the infobar is closing.
- (TranslateInfoBarDelegate*)delegate;

// Called when the "Show Original" button is pressed.
- (IBAction)showOriginal:(id)sender;

@end

@interface TranslateInfoBarControllerBase (ProtectedAPI)

// Resizes or hides the options button based on how much space is available
// so that it doesn't overlap other buttons.
// lastView is the rightmost view, the first one that the options button
// would overlap with.
- (void)adjustOptionsButtonSizeAndVisibilityForView:(NSView*)lastView;

// Move all the currently visible views into the correct place for the
// current mode.
// Must be implemented by the subclass.
- (void)layout;

// Loads the text for the 3 labels.  There is only one message, but since
// it has controls separating parts of it, it is separated into 3 separate
// labels.
// Must be implemented by the subclass.
- (void)loadLabelText;

// Returns the controls that are visible in the subclasses infobar.  The
// default implementation returns an empty array. The controls should
// be returned in the order they are displayed, otherwise the layout test
// will fail.
// Must be implemented by the subclass.
- (NSArray*)visibleControls;

// Shows the array of controls provided by the subclass.
- (void)showVisibleControls:(NSArray*)visibleControls;

// Hides the OK and Cancel buttons.
- (void)removeOkCancelButtons;

// Called when the source or target language selection changes in a menu.
// |newLanguageIdx| is the index of the newly selected item in the appropriate
// menu.
- (void)sourceLanguageModified:(NSInteger)newLanguageIdx;
- (void)targetLanguageModified:(NSInteger)newLanguageIdx;

// Called when an item in one of the toolbar's language or options
// menus is selected.
- (void)languageMenuChanged:(id)item;
- (void)optionsMenuChanged:(id)item;

// Teardown and rebuild the options menu.  When the infobar is small, the
// options menu is shrunk to just a drop down arrow, so the title needs
// to be empty.
- (void)rebuildOptionsMenu:(BOOL)hideTitle;

// Whether or not this infobar should show the options popup.
- (BOOL)shouldShowOptionsPopUp;

@end // TranslateInfoBarControllerBase (ProtectedAPI)

#pragma mark TestingAPI

@interface TranslateInfoBarControllerBase (TestingAPI)

// All the controls used in any of the translate states.
// This is used for verifying layout and for setting the
// correct styles on each button.
- (NSArray*)allControls;

// Verifies that the layout of the infobar is correct.
// Must be implmented by the subclass.
- (bool)verifyLayout;

// Returns the underlying options menu.
- (NSMenu*)optionsMenu;

// Returns |translateMessageButton_|, see declaration of member
// variable for a full description.
- (NSButton*)translateMessageButton;

@end // TranslateInfoBarControllerBase (TestingAPI)


#endif // CHROME_BROWSER_UI_COCOA_TRANSLATE_INFOBAR_BASE_H_
