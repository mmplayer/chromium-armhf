// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Network drop-down implementation.
 */

cr.define('cr.ui', function() {
  /**
   * Creates a new container for the drop down menu items.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var DropDownContainer = cr.ui.define('div');

  DropDownContainer.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
      this.classList.add('dropdown-container');
      // Selected item in the menu list.
      this.selectedItem = null;
      // First item which could be selected.
      this.firstItem = null;
      this.setAttribute('role', 'menu');
      // Whether scroll has just happened.
      this.scrollJustHappened = false;
    },

    /**
     * Gets scroll action to be done for the item.
     * @param {!Object} item Menu item.
     * @return {integer} -1 for scroll up; 0 for no action; 1 for scroll down.
     */
    scrollAction: function(item) {
      var thisTop = this.scrollTop;
      var thisBottom = thisTop + this.offsetHeight;
      var itemTop = item.offsetTop;
      var itemBottom = itemTop + item.offsetHeight;
      if (itemTop <= thisTop) return -1;
      if (itemBottom >= thisBottom) return 1;
      return 0;
    },

    /**
     * Selects new item.
     * @param {!Object} selectedItem Item to be selected.
     * @param {boolean} mouseOver Is mouseover event triggered?
     */
    selectItem: function(selectedItem, mouseOver) {
      if (mouseOver && this.scrollJustHappened) {
        this.scrollJustHappened = false;
        return;
      }
      if (this.selectedItem)
        this.selectedItem.classList.remove('hover');
      selectedItem.classList.add('hover');
      this.selectedItem = selectedItem;
      if (!this.hidden) {
        this.previousSibling.setAttribute(
            'aria-activedescendant', selectedItem.id);
      }
      var action = this.scrollAction(selectedItem);
      if (action != 0) {
        selectedItem.scrollIntoView(action < 0);
        this.scrollJustHappened = true;
      }
    }
  };

  /**
   * Creates a new DropDown div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var DropDown = cr.ui.define('div');

  DropDown.ITEM_DIVIDER_ID = -2;

  DropDown.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
      this.appendChild(this.createOverlay_());
      this.appendChild(this.title_ = this.createTitle_());
      this.appendChild(new DropDownContainer());

      this.isShown = false;
      this.addEventListener('keydown', this.keyDownHandler_);

      this.title_.id = this.id + '-dropdown';
      this.title_.setAttribute('role', 'button');
      this.title_.setAttribute('aria-haspopup', 'true');
    },

    /**
     * Returns true if dropdown menu is shown.
     * @type {bool} Whether menu element is shown.
     */
    get isShown() {
      return !this.container.hidden;
    },

    /**
     * Sets dropdown menu visibility.
     * @param {bool} show New visibility state for dropdown menu.
     */
    set isShown(show) {
      this.firstElementChild.hidden = !show;
      this.container.hidden = !show;
      if (show) {
        this.container.selectItem(this.container.firstItem, false);
        this.title_.setAttribute('aria-pressed', 'true');
      } else {
        this.title_.setAttribute('aria-pressed', 'false');
        this.title_.removeAttribute('aria-activedescendant');
      }
    },

    /**
     * Returns title button.
     */
    get titleButton() {
      return this.children[1];
    },

    /**
     * Returns container of the menu items.
     */
    get container() {
      return this.lastElementChild;
    },

    /**
     * Sets title and icon.
     * @param {string} title Text on dropdown.
     * @param {string} icon Icon in dataURL format.
     */
    setTitle: function(title, icon) {
      this.titleButton.firstElementChild.src = icon;
      this.titleButton.lastElementChild.textContent = title;
    },

    /**
     * Sets dropdown items.
     * @param {Array} items Dropdown items array.
     */
    setItems: function(items) {
      this.container.innerHTML = '';
      this.container.firstItem = null;
      this.container.selectedItem = null;
      for (var i = 0; i < items.length; ++i) {
        var item = items[i];
        if ('sub' in item) {
          // Workaround for submenus, add items on top level.
          // TODO(altimofeev): support submenus.
          for (var j = 0; j < item.sub.length; ++j)
            this.createItem_(this.container, item.sub[j]);
          continue;
        }
        this.createItem_(this.container, item);
      }
      this.container.selectItem(this.container.firstItem, false);
    },

    /**
     * Id of the active drop-down element.
     * @private
     */
    activeElementId_: '',

    /**
     * Creates dropdown item element and adds into container.
     * @param {HTMLElement} container Container where item is added.
     * @param {!Object} item Item to be added.
     * @private
     */
    createItem_: function(container, item) {
      var itemContentElement;
      var className = 'dropdown-item';
      if (item.id == DropDown.ITEM_DIVIDER_ID) {
        className = 'dropdown-divider';
        itemContentElement = this.ownerDocument.createElement('hr');
      } else {
        var span = this.ownerDocument.createElement('span');
        itemContentElement = span;
        span.textContent = item.label;
        if ('bold' in item && item.bold)
          span.classList.add('bold');
        var image = this.ownerDocument.createElement('img');
        image.alt = '';
        if (item.icon)
          image.src = item.icon;
      }

      var itemElement = this.ownerDocument.createElement('div');
      itemElement.classList.add(className);
      itemElement.appendChild(itemContentElement);
      itemElement.iid = item.id;
      itemElement.controller = this;
      var enabled = 'enabled' in item && item.enabled;
      if (!enabled)
        itemElement.classList.add('disabled-item');

      if (item.id > 0) {
        var wrapperDiv = this.ownerDocument.createElement('div');
        wrapperDiv.setAttribute('role', 'menuitem');
        wrapperDiv.id = this.id + item.id;
        if (!enabled)
          wrapperDiv.setAttribute('aria-disabled', 'true');
        wrapperDiv.classList.add('dropdown-item-container');
        var imageDiv = this.ownerDocument.createElement('div');
        imageDiv.classList.add('dropdown-image');
        imageDiv.appendChild(image);
        wrapperDiv.appendChild(imageDiv);
        wrapperDiv.appendChild(itemElement);
        wrapperDiv.addEventListener('click', function f(e) {
          var item = this.lastElementChild;
          if (item.iid < -1 || item.classList.contains('disabled-item'))
            return;
          item.controller.isShown = false;
          if (item.iid >= 0)
            chrome.send('networkItemChosen', [item.iid]);
          this.parentNode.parentNode.titleButton.focus();
        });
        wrapperDiv.addEventListener('mouseover', function f(e) {
          this.parentNode.selectItem(this, true);
        });
        itemElement = wrapperDiv;
      }
      container.appendChild(itemElement);
      if (!container.firstItem && item.id >= 0) {
        container.firstItem = itemElement;
      }
    },

    /**
     * Creates dropdown overlay element, which catches outside clicks.
     * @type {HTMLElement}
     * @private
     */
    createOverlay_: function() {
      var overlay = this.ownerDocument.createElement('div');
      overlay.classList.add('dropdown-overlay');
      overlay.addEventListener('click', function() {
        this.parentNode.titleButton.focus();
        this.parentNode.isShown = false;
      });
      return overlay;
    },

    /**
     * Creates dropdown title element.
     * @type {HTMLElement}
     * @private
     */
    createTitle_: function() {
      var image = this.ownerDocument.createElement('img');
      image.alt = '';
      var text = this.ownerDocument.createElement('div');

      var el = this.ownerDocument.createElement('div');
      el.appendChild(image);
      el.appendChild(text);

      el.tabIndex = 0;
      el.classList.add('dropdown-title');
      el.iid = -1;
      el.controller = this;
      el.inFocus = false;
      el.opening = false;

      el.addEventListener('click', function f(e) {
        this.controller.isShown = !this.controller.isShown;
      });

      el.addEventListener('focus', function(e) {
        this.inFocus = true;
      });

      el.addEventListener('blur', function(e) {
        this.inFocus = false;
      });

      el.addEventListener('keydown', function f(e) {
        if (this.inFocus && !this.controller.isShown &&
            (e.keyCode == 13 || e.keyCode == 32)) {
          // Enter or space has been pressed.
          this.opening = true;
          this.controller.isShown = true;
        }
      });
      return el;
    },

    /**
     * Handles keydown event from the keyboard.
     * @private
     * @param {!Event} e Keydown event.
     */
    keyDownHandler_: function(e) {
      if (!this.isShown)
        return;
      var selected = this.container.selectedItem;
      switch (e.keyCode) {
        case 38: {  // Key up.
          do {
            selected = selected.previousSibling;
            if (!selected)
              selected = this.container.lastElementChild;
          } while (selected.iid < 0);
          this.container.selectItem(selected, false);
          break;
        }
        case 40: {  // Key down.
          do {
            selected = selected.nextSibling;
            if (!selected)
              selected = this.container.firstItem;
          } while (selected.iid < 0);
          this.container.selectItem(selected, false);
          break;
        }
        case 27: {  // Esc.
          this.isShown = false;
          break;
        }
        case 9: {  // Tab.
          this.isShown = false;
          break;
        }
        case 13: {  // Enter.
          var button = this.titleButton;
          if (!button.opening) {
            button.focus();
            this.isShown = false;
            var item =
                button.controller.container.selectedItem.lastElementChild;
            if (item.iid >= 0 && !item.classList.contains('disabled-item'))
              chrome.send('networkItemChosen', [item.iid]);
          } else {
            button.opening = false;
          }
          break;
        }
      }
    }
  };

  /**
   * Updates networks list with the new data.
   * @param {!Object} data Networks list.
   */
  DropDown.updateNetworks = function(data) {
    var elementId = DropDown.activeElementId_;
    $(elementId).setItems(data);
  };

  /**
   * Updates network title, which is shown by the drop-down.
   * @param {string} title Title to be displayed.
   * @param {!Object} icon Icon to be displayed.
   */
  DropDown.updateNetworkTitle = function(title, icon) {
    var elementId = DropDown.activeElementId_;
    $(elementId).setTitle(title, icon);
  };

  /**
   * Activates or deactivates network drop-down. Only one network drop-down
   * can be active at the same time. So activating new drop-down deactivates
   * the previous one. Deactivating not active drop-down does nothing.
   * @param {string} element_id Id of the element which is network drop-down.
   * @param {boolean} is_active Is drop-down active?
   */
  DropDown.setActive = function(elementId, isActive) {
    if (isActive) {
      DropDown.activeElementId_ = elementId;
      chrome.send('networkDropdownShow', [elementId]);
    } else {
      if (DropDown.activeElementId_ == elementId) {
        DropDown.activeElementId_ = '';
        chrome.send('networkDropdownHide', []);
      }
    }
  };

  /**
   * Refreshes network drop-down. Should be called on language change.
   */
  DropDown.refresh = function() {
    chrome.send('networkDropdownRefresh', []);
  };

  return {
    DropDown: DropDown
  };
});
