// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('ntp4', function() {
  'use strict';

  // We can't pass the currently dragging tile via dataTransfer because of
  // http://crbug.com/31037
  var currentlyDraggingTile = null;
  function getCurrentlyDraggingTile() {
    return currentlyDraggingTile;
  }
  function setCurrentlyDraggingTile(tile) {
    currentlyDraggingTile = tile;
    if (tile)
      ntp4.enterRearrangeMode();
    else
      ntp4.leaveRearrangeMode();
  }

  /**
   * Changes the current dropEffect of a drag. This modifies the native cursor
   * and serves as an indicator of what we should do at the end of the drag as
   * well as give indication to the user if a drop would succeed if they let go.
   * @param {DataTransfer} dataTransfer A dataTransfer object from a drag event.
   * @param {string} effect A drop effect to change to (i.e. copy, move, none).
   */
  function setCurrentDropEffect(dataTransfer, effect) {
    dataTransfer.dropEffect = effect;
    if (currentlyDraggingTile)
      currentlyDraggingTile.lastDropEffect = dataTransfer.dropEffect;
  }

  /**
   * Creates a new Tile object. Tiles wrap content on a TilePage, providing
   * some styling and drag functionality.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function Tile(contents) {
    var tile = cr.doc.createElement('div');
    tile.__proto__ = Tile.prototype;
    tile.initialize(contents);

    return tile;
  }

  Tile.prototype = {
    __proto__: HTMLDivElement.prototype,

    initialize: function(contents) {
      // 'real' as opposed to doppleganger.
      this.className = 'tile real';
      this.appendChild(contents);
      contents.tile = this;

      this.addEventListener('dragstart', this.onDragStart_);
      this.addEventListener('drag', this.onDragMove_);
      this.addEventListener('dragend', this.onDragEnd_);

      this.firstChild.addEventListener(
          'webkitAnimationEnd', this.onContentsAnimationEnd_.bind(this));

      this.eventTracker = new EventTracker();
    },

    get index() {
      return Array.prototype.indexOf.call(this.parentNode.children, this);
    },

    get tilePage() {
      return findAncestorByClass(this, 'tile-page');
    },

    /**
     * Position the tile at |x, y|, and store this as the grid location, i.e.
     * where the tile 'belongs' when it's not being dragged.
     * @param {number} x The x coordinate, in pixels.
     * @param {number} y The y coordinate, in pixels.
     */
    setGridPosition: function(x, y) {
      this.gridX = x;
      this.gridY = y;
      this.moveTo(x, y);
    },

    /**
     * Position the tile at |x, y|.
     * @param {number} x The x coordinate, in pixels.
     * @param {number} y The y coordinate, in pixels.
     */
    moveTo: function(x, y) {
      // left overrides right in LTR, and right takes precedence in RTL.
      this.style.left = x + 'px';
      this.style.right = x + 'px';
      this.style.top = y + 'px';
    },

    /**
     * The handler for dragstart events fired on |this|.
     * @param {Event} e The event for the drag.
     * @private
     */
    onDragStart_: function(e) {
      // The user may start dragging again during a previous drag's finishing
      // animation.
      if (this.classList.contains('dragging'))
        this.finalizeDrag_();

      setCurrentlyDraggingTile(this);

      e.dataTransfer.effectAllowed = 'copyMove';
      this.firstChild.setDragData(e.dataTransfer);

      // The drag clone is the node we use as a representation during the drag.
      // It's attached to the top level document element so that it floats above
      // image masks.
      this.dragClone = this.cloneNode(true);
      this.dragClone.style.right = '';
      this.dragClone.classList.add('drag-representation');
      $('card-slider-frame').appendChild(this.dragClone);
      this.eventTracker.add(this.dragClone, 'webkitTransitionEnd',
                            this.onDragCloneTransitionEnd_.bind(this));

      this.classList.add('dragging');
      // offsetLeft is mirrored in RTL. Un-mirror it.
      var offsetLeft = ntp4.isRTL() ?
          this.parentNode.clientWidth - this.offsetLeft :
          this.offsetLeft;
      this.dragOffsetX = e.x - offsetLeft - this.parentNode.offsetLeft;
      this.dragOffsetY = e.y - this.offsetTop -
          // Unlike offsetTop, this value takes scroll position into account.
          this.parentNode.getBoundingClientRect().top;

      this.onDragMove_(e);
    },

    /**
     * The handler for drag events fired on |this|.
     * @param {Event} e The event for the drag.
     * @private
     */
    onDragMove_: function(e) {
      if (e.view != window || (e.x == 0 && e.y == 0)) {
        this.dragClone.hidden = true;
        return;
      }

      this.dragClone.hidden = false;
      this.dragClone.style.left = (e.x - this.dragOffsetX) + 'px';
      this.dragClone.style.top = (e.y - this.dragOffsetY) + 'px';
    },

    /**
     * The handler for dragend events fired on |this|.
     * @param {Event} e The event for the drag.
     * @private
     */
    onDragEnd_: function(e) {
      this.dragClone.hidden = false;
      this.dragClone.classList.add('placing');

      setCurrentlyDraggingTile(null);

      // tilePage will be null if we've already been removed.
      var tilePage = this.tilePage;
      if (tilePage)
        tilePage.positionTile_(this.index);

      // Take an appropriate action with the drag clone.
      if (this.landedOnTrash) {
        this.dragClone.classList.add('deleting');
      } else if (tilePage) {
        // TODO(dbeam): Until we fix dropEffect to the correct behavior it will
        // differ on windows - crbug.com/39399.  That's why we use the custom
        // this.lastDropEffect instead of e.dataTransfer.dropEffect.
        if (tilePage.selected && this.lastDropEffect != 'copy') {
          // The drag clone can still be hidden from the last drag move event.
          this.dragClone.hidden = false;
          // The tile's contents may have moved following the respositioning;
          // adjust for that.
          var contentDiffX = this.dragClone.firstChild.offsetLeft -
              this.firstChild.offsetLeft;
          var contentDiffY = this.dragClone.firstChild.offsetTop -
              this.firstChild.offsetTop;
          this.dragClone.style.left = (this.gridX + this.parentNode.offsetLeft -
              contentDiffX) + 'px';
          this.dragClone.style.top =
              (this.gridY + this.parentNode.getBoundingClientRect().top -
              contentDiffY) + 'px';
        } else if (this.dragClone.hidden) {
          this.finalizeDrag_();
        } else {
          // The CSS3 transitions spec intentionally leaves it up to individual
          // user agents to determine when styles should be applied. On some
          // platforms (at the moment, Windows), when you apply both classes
          // immediately a transition may not occur correctly. That's why we're
          // using a setTimeout here to queue adding the class until the
          // previous class (currently: .placing) sets up a transition.
          // http://dev.w3.org/csswg/css3-transitions/#starting
          window.setTimeout(function() {
            this.dragClone.classList.add('dropped-on-other-page');
          }.bind(this), 0);
        }
      }

      delete this.lastDropEffect;
      this.landedOnTrash = false;
    },

    /**
     * Creates a clone of this node offset by the coordinates. Used for the
     * dragging effect where a tile appears to float off one side of the grid
     * and re-appear on the other.
     * @param {number} x x-axis offset, in pixels.
     * @param {number} y y-axis offset, in pixels.
     */
    showDoppleganger: function(x, y) {
      // We always have to clear the previous doppleganger to make sure we get
      // style updates for the contents of this tile.
      this.clearDoppleganger();

      var clone = this.cloneNode(true);
      clone.classList.remove('real');
      clone.classList.add('doppleganger');
      var clonelets = clone.querySelectorAll('.real');
      for (var i = 0; i < clonelets.length; i++) {
        clonelets[i].classList.remove('real');
      }

      this.appendChild(clone);
      this.doppleganger_ = clone;

      if (ntp4.isRTL())
        x *= -1;

      this.doppleganger_.style.WebkitTransform = 'translate(' + x + 'px, ' +
                                                                y + 'px)';
    },

    /**
     * Destroys the current doppleganger.
     */
    clearDoppleganger: function() {
      if (this.doppleganger_) {
        this.removeChild(this.doppleganger_);
        this.doppleganger_ = null;
      }
    },

    /**
     * Returns status of doppleganger.
     * @return {boolean} True if there is a doppleganger showing for |this|.
     */
    hasDoppleganger: function() {
      return !!this.doppleganger_;
    },

    /**
     * Cleans up after the drag is over. This is either called when the
     * drag representation finishes animating to the final position, or when
     * the next drag starts (if the user starts a 2nd drag very quickly).
     * @private
     */
    finalizeDrag_: function() {
      assert(this.classList.contains('dragging'));

      var clone = this.dragClone;
      this.dragClone = null;

      clone.parentNode.removeChild(clone);
      this.eventTracker.remove(clone, 'webkitTransitionEnd');
      this.classList.remove('dragging');
      if (this.firstChild.finalizeDrag)
        this.firstChild.finalizeDrag();
    },

    /**
     * Called when the drag representation node is done migrating to its final
     * resting spot.
     * @param {Event} e The transition end event.
     */
    onDragCloneTransitionEnd_: function(e) {
      if (this.classList.contains('dragging') &&
          (e.propertyName == 'left' || e.propertyName == 'top' ||
           e.propertyName == '-webkit-transform')) {
        this.finalizeDrag_();
      }
    },

    /**
     * Called when an app is removed from Chrome. Animates its disappearance.
     */
    doRemove: function() {
      this.firstChild.classList.add('removing-tile-contents');
    },

    /**
     * Callback for the webkitAnimationEnd event on the tile's contents.
     * @param {Event} e The event object.
     */
    onContentsAnimationEnd_: function(e) {
      if (this.firstChild.classList.contains('new-tile-contents'))
        this.firstChild.classList.remove('new-tile-contents');
      if (this.firstChild.classList.contains('removing-tile-contents'))
        this.tilePage.removeTile(this);
    },
  };

  /**
   * Gives the proportion of the row width that is devoted to a single icon.
   * @param {number} rowTileCount The number of tiles in a row.
   * @param {number} tileSpacingFraction The proportion of the tile width which
   *     will be used as spacing between tiles.
   * @return {number} The ratio between icon width and row width.
   */
  function tileWidthFraction(rowTileCount, tileSpacingFraction) {
    return rowTileCount + (rowTileCount - 1) * tileSpacingFraction;
  }

  /**
   * Calculates an assortment of tile-related values for a grid with the
   * given dimensions.
   * @param {number} width The pixel width of the grid.
   * @param {number} numRowTiles The number of tiles in a row.
   * @param {number} tileSpacingFraction The proportion of the tile width which
   *     will be used as spacing between tiles.
   * @return {Object} A mapping of pixel values.
   */
  function tileValuesForGrid(width, numRowTiles, tileSpacingFraction) {
    var tileWidth = width / tileWidthFraction(numRowTiles, tileSpacingFraction);
    var offsetX = tileWidth * (1 + tileSpacingFraction);
    var interTileSpacing = offsetX - tileWidth;

    return {
      tileWidth: tileWidth,
      offsetX: offsetX,
      interTileSpacing: interTileSpacing,
    };
  }

  // The smallest amount of horizontal blank space to display on the sides when
  // displaying a wide arrangement. There is an additional 26px of margin from
  // the tile page padding.
  var MIN_WIDE_MARGIN = 18;

  /**
   * Creates a new TilePage object. This object contains tiles and controls
   * their layout.
   * @param {Object} gridValues Pixel values that define the size and layout
   *     of the tile grid.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function TilePage(gridValues) {
    var el = cr.doc.createElement('div');
    el.gridValues_ = gridValues;
    el.__proto__ = TilePage.prototype;
    el.initialize();

    return el;
  }

  /**
   * Takes a collection of grid layout pixel values and updates them with
   * additional tiling values that are calculated from TilePage constants.
   * @param {Object} grid The grid layout pixel values to update.
   */
  TilePage.initGridValues = function(grid) {
    // The amount of space we need to display a narrow grid (all narrow grids
    // are this size).
    grid.narrowWidth =
        grid.minTileWidth * tileWidthFraction(grid.minColCount,
                                              grid.tileSpacingFraction);
    // The minimum amount of space we need to display a wide grid.
    grid.minWideWidth =
        grid.minTileWidth * tileWidthFraction(grid.maxColCount,
                                              grid.tileSpacingFraction);
    // The largest we will ever display a wide grid.
    grid.maxWideWidth =
        grid.maxTileWidth * tileWidthFraction(grid.maxColCount,
                                              grid.tileSpacingFraction);
    // Tile-related pixel values for the narrow display.
    grid.narrowTileValues = tileValuesForGrid(grid.narrowWidth,
                                              grid.minColCount,
                                              grid.tileSpacingFraction);
    // Tile-related pixel values for the minimum narrow display.
    grid.wideTileValues = tileValuesForGrid(grid.minWideWidth,
                                            grid.maxColCount,
                                            grid.tileSpacingFraction);
  };

  TilePage.prototype = {
    __proto__: HTMLDivElement.prototype,

    initialize: function() {
      this.className = 'tile-page';

      // Div that acts as a custom scrollbar. The scrollbar has to live
      // outside the content div so it doesn't flicker when scrolling (due to
      // repainting after the scroll, then repainting again when moved in the
      // onScroll handler). |scrollbar_| is only aesthetic, and it only
      // represents the thumb. Actual events are still handled by the invisible
      // native scrollbars. This div gives us more flexibility with the visuals.
      this.scrollbar_ = this.ownerDocument.createElement('div');
      this.scrollbar_.className = 'tile-page-scrollbar';
      this.scrollbar_.hidden = true;
      this.appendChild(this.scrollbar_);

      // This contains everything but the scrollbar.
      this.content_ = this.ownerDocument.createElement('div');
      this.content_.className = 'tile-page-content';
      this.appendChild(this.content_);

      // Div that sets the vertical position of the tile grid.
      this.topMargin_ = this.ownerDocument.createElement('div');
      this.topMargin_.className = 'top-margin';
      this.content_.appendChild(this.topMargin_);

      // Div that holds the tiles.
      this.tileGrid_ = this.ownerDocument.createElement('div');
      this.tileGrid_.className = 'tile-grid';
      this.tileGrid_.style.minWidth = this.gridValues_.narrowWidth + 'px';
      this.content_.appendChild(this.tileGrid_);

      // Ordered list of our tiles.
      this.tileElements_ = this.tileGrid_.getElementsByClassName('tile real');
      // Ordered list of the elements which want to accept keyboard focus. These
      // elements will not be a part of the normal tab order; the tile grid
      // initially gets focused and then these elements can be focused via the
      // arrow keys.
      this.focusableElements_ =
          this.tileGrid_.getElementsByClassName('focusable');

      // These are properties used in updateTopMargin.
      this.animatedTopMarginPx_ = 0;
      this.topMarginPx_ = 0;

      this.eventTracker = new EventTracker();
      this.eventTracker.add(window, 'resize', this.onResize_.bind(this));

      this.addEventListener('DOMNodeInsertedIntoDocument',
                            this.onNodeInsertedIntoDocument_);

      this.addEventListener('mousewheel', this.onMouseWheel_);
      this.content_.addEventListener('scroll', this.onScroll_.bind(this));

      this.dragWrapper_ = new DragWrapper(this.tileGrid_, this);

      this.addEventListener('cardselected', this.handleCardSelection_);
      this.addEventListener('carddeselected', this.handleCardDeselection_);
      this.addEventListener('focus', this.handleFocus_);
      this.addEventListener('keydown', this.handleKeyDown_);
      this.addEventListener('mousedown', this.handleMouseDown_);

      this.focusElementIndex_ = -1;
    },

    get tiles() {
      return this.tileElements_;
    },

    get tileCount() {
      return this.tileElements_.length;
    },

    get selected() {
      return Array.prototype.indexOf.call(this.parentNode.children, this) ==
          ntp4.getCardSlider().currentCard;
    },

    /**
     * The size of the margin (unused space) on the sides of the tile grid, in
     * pixels.
     * @type {number}
     */
    get sideMargin() {
      return this.layoutValues_.leftMargin;
    },

    /**
     * Returns the width of the scrollbar, in pixels, if it is active, or 0
     * otherwise.
     * @type {number}
     */
    get scrollbarWidth() {
      return this.scrollbar_.hidden ? 0 : 13;
    },

    /**
     * Returns any extra padding to insert to the bottom of a tile page.  By
     * default there is none, but subclasses can override.
     * @type {number}
     */
    get extraBottomPadding() {
      return 0;
    },

    /**
     * Cleans up resources that are no longer needed after this TilePage
     * instance is removed from the DOM.
     */
    tearDown: function() {
      this.eventTracker.removeAll();
    },

    /**
     * Appends a tile to the end of the tile grid.
     * @param {HTMLElement} tileElement The contents of the tile.
     * @param {?boolean} animate If true, the append will be animated.
     * @protected
     */
    appendTile: function(tileElement, animate) {
      this.addTileAt(tileElement, this.tileElements_.length, animate);
    },

    /**
     * Adds the given element to the tile grid.
     * @param {Node} tileElement The tile object/node to insert.
     * @param {number} index The location in the tile grid to insert it at.
     * @param {?boolean} animate If true, the tile in question will be animated
     *     (other tiles, if they must reposition, do not animate).
     * @protected
     */
    addTileAt: function(tileElement, index, animate) {
      this.classList.remove('animating-tile-page');
      if (animate)
        tileElement.classList.add('new-tile-contents');
      var wrapperDiv = new Tile(tileElement);
      if (index == this.tileElements_.length) {
        this.tileGrid_.appendChild(wrapperDiv);
      } else {
        this.tileGrid_.insertBefore(wrapperDiv,
                                    this.tileElements_[index]);
      }
      this.calculateLayoutValues_();
      this.heightChanged_();

      this.positionTile_(index);
    },

    /**
     * Removes the given tile and animates the respositioning of the other
     * tiles.
     * @param {HTMLElement} tile The tile to remove from |tileGrid_|.
     * @param {?boolean} animate If true, tiles will animate.
     */
    removeTile: function(tile, animate) {
      if (animate)
        this.classList.add('animating-tile-page');
      var index = tile.index;
      tile.parentNode.removeChild(tile);
      this.calculateLayoutValues_();
      for (var i = index; i < this.tileElements_.length; i++) {
        this.positionTile_(i);
      }
    },

    /**
     * Removes all tiles from the page.
     */
    removeAllTiles: function() {
      this.tileGrid_.innerHTML = '';
    },

    /**
     * Called when the page is selected (in the card selector).
     * @param {Event} e A custom cardselected event.
     * @private
     */
    handleCardSelection_: function(e) {
      this.tabIndex = 1;

      // When we are selected, we re-calculate the layout values. (See comment
      // in doDrop.)
      this.calculateLayoutValues_();
    },

    /**
     * Called when the page loses selection (in the card selector).
     * @param {Event} e A custom carddeselected event.
     * @private
     */
    handleCardDeselection_: function(e) {
      this.tabIndex = -1;
      if (this.currentFocusElement_)
        this.currentFocusElement_.tabIndex = -1;
    },

    /**
     * When we get focus, pass it on to the focus element.
     * @param {Event} e The focus event.
     * @private
     */
    handleFocus_: function(e) {
      if (this.focusableElements_.length == 0)
        return;

      this.updateFocusElement_();
    },

    /**
     * Since we are doing custom focus handling, we have to manually
     * set focusability on click (as well as keyboard nav above).
     * @param {Event} e The focus event.
     * @private
     */
    handleMouseDown_: function(e) {
      var focusable = findAncestorByClass(e.target, 'focusable');
      if (focusable) {
        this.focusElementIndex_ =
            Array.prototype.indexOf.call(this.focusableElements_,
                                         focusable);
        this.updateFocusElement_();
      } else {
        // This prevents the tile page from getting focus when the user clicks
        // inside the grid but outside of any tile.
        e.preventDefault();
      }
    },

    /**
     * Handle arrow key focus nav.
     * @param {Event} e The focus event.
     * @private
     */
    handleKeyDown_: function(e) {
      // We only handle up, down, left, right without control keys.
      if (e.metaKey || e.shiftKey || e.altKey || e.ctrlKey)
        return;

      // Wrap the given index to |this.focusableElements_|.
      var wrap = function(idx) {
        return (idx + this.focusableElements_.length) %
            this.focusableElements_.length;
      }.bind(this);

      switch (e.keyIdentifier) {
        case 'Right':
        case 'Left':
          var direction = e.keyIdentifier == 'Right' ? 1 : -1;
          this.focusElementIndex_ = wrap(this.focusElementIndex_ + direction);
          break;
        case 'Up':
        case 'Down':
          // Look through all focusable elements. Find the first one that is
          // in the same column.
          var direction = e.keyIdentifier == 'Up' ? -1 : 1;
          var currentIndex =
              Array.prototype.indexOf.call(this.focusableElements_,
                                           this.currentFocusElement_);
          var newFocusIdx = wrap(currentIndex + direction)
          var tile = this.currentFocusElement_.parentNode;
          for (;; newFocusIdx = wrap(newFocusIdx + direction)) {
            var newTile = this.focusableElements_[newFocusIdx].parentNode;
            var rowTiles = this.layoutValues_.numRowTiles;
            if ((newTile.index - tile.index) % rowTiles == 0)
              break;
          }

          this.focusElementIndex_ = newFocusIdx;
          break;

        default:
          return;
      }

      this.updateFocusElement_();

      e.preventDefault();
      e.stopPropagation();
    },

    /**
     * Focuses the element for |this.focusElementIndex_|. Makes the current
     * focus element, if any, no longer eligible for focus.
     * @private
     */
    updateFocusElement_: function() {
      this.focusElementIndex_ = Math.min(this.focusableElements_.length - 1,
                                         this.focusElementIndex_);
      this.focusElementIndex_ = Math.max(0, this.focusElementIndex_);

      var newFocusElement = this.focusableElements_[this.focusElementIndex_];
      var lastFocusElement = this.currentFocusElement_;
      if (lastFocusElement && lastFocusElement != newFocusElement)
        lastFocusElement.tabIndex = -1;

      newFocusElement.tabIndex = 1;
      newFocusElement.focus();
      this.tabIndex = -1;
    },

    /**
     * The current focus element is that element which is eligible for focus.
     * @type {HTMLElement} The node.
     * @private
     */
    get currentFocusElement_() {
      return this.querySelector('.focusable[tabindex="1"]');
    },

    /**
     * Makes some calculations for tile layout. These change depending on
     * height, width, and the number of tiles.
     * TODO(estade): optimize calls to this function. Do nothing if the page is
     * hidden, but call before being shown.
     * @private
     */
    calculateLayoutValues_: function() {
      var grid = this.gridValues_;
      var availableSpace = this.tileGrid_.clientWidth - 2 * MIN_WIDE_MARGIN;
      var wide = availableSpace >= grid.minWideWidth;
      var numRowTiles = wide ? grid.maxColCount : grid.minColCount;

      var effectiveGridWidth = wide ?
          Math.min(Math.max(availableSpace, grid.minWideWidth),
                   grid.maxWideWidth) :
          grid.narrowWidth;
      var realTileValues = tileValuesForGrid(effectiveGridWidth, numRowTiles,
                                             grid.tileSpacingFraction);

      // leftMargin centers the grid within the avaiable space.
      var minMargin = wide ? MIN_WIDE_MARGIN : 0;
      var leftMargin =
          Math.max(minMargin,
                   (this.tileGrid_.clientWidth - effectiveGridWidth) / 2);

      var rowHeight = this.heightForWidth(realTileValues.tileWidth) +
          realTileValues.interTileSpacing;

      this.layoutValues_ = {
        numRowTiles: numRowTiles,
        leftMargin: leftMargin,
        colWidth: realTileValues.offsetX,
        rowHeight: rowHeight,
        tileWidth: realTileValues.tileWidth,
        wide: wide,
      };

      // We need to update the top margin as well.
      this.updateTopMargin_();

      this.firePageLayoutEvent_();
    },

    /**
     * Dispatches the custom pagelayout event.
     * @private
     */
    firePageLayoutEvent_: function() {
      cr.dispatchSimpleEvent(this, 'pagelayout', true, true);
    },

    /**
     * Calculates the x/y coordinates for an element and moves it there.
     * @param {number} index The index of the element to be positioned.
     * @param {number} indexOffset If provided, this is added to |index| when
     *     positioning the tile. The effect is that the tile will be positioned
     *     in a non-default location.
     * @private
     */
    positionTile_: function(index, indexOffset) {
      var grid = this.gridValues_;
      var layout = this.layoutValues_;

      indexOffset = typeof indexOffset != 'undefined' ? indexOffset : 0;
      // Add the offset _after_ the modulus division. We might want to show the
      // tile off the side of the grid.
      var col = index % layout.numRowTiles + indexOffset;
      var row = Math.floor(index / layout.numRowTiles);
      // Calculate the final on-screen position for the tile.
      var realX = col * layout.colWidth + layout.leftMargin;
      var realY = row * layout.rowHeight;

      // Calculate the portion of the tile's position that should be animated.
      var animatedTileValues = layout.wide ?
          grid.wideTileValues : grid.narrowTileValues;
      // Animate the difference between three-wide and six-wide.
      var animatedLeftMargin = layout.wide ?
          0 : (grid.minWideWidth - MIN_WIDE_MARGIN - grid.narrowWidth) / 2;
      var animatedX = col * animatedTileValues.offsetX + animatedLeftMargin;
      var animatedY = row * (this.heightForWidth(animatedTileValues.tileWidth) +
                             animatedTileValues.interTileSpacing);

      var tile = this.tileElements_[index];
      tile.setGridPosition(animatedX, animatedY);
      tile.firstChild.setBounds(layout.tileWidth,
                                realX - animatedX,
                                realY - animatedY);

      // This code calculates whether the tile needs to show a clone of itself
      // wrapped around the other side of the tile grid.
      var offTheRight = col == layout.numRowTiles ||
          (col == layout.numRowTiles - 1 && tile.hasDoppleganger());
      var offTheLeft = col == -1 || (col == 0 && tile.hasDoppleganger());
      if (this.isCurrentDragTarget && (offTheRight || offTheLeft)) {
        var sign = offTheRight ? 1 : -1;
        tile.showDoppleganger(-layout.numRowTiles * layout.colWidth * sign,
                              layout.rowHeight * sign);
      } else {
        tile.clearDoppleganger();
      }

      if (index == this.tileElements_.length - 1) {
        this.tileGrid_.style.height = (realY + layout.rowHeight) + 'px';
        this.queueUpdateScrollbars_();
      }
    },

    /**
     * Gets the index of the tile that should occupy coordinate (x, y). Note
     * that this function doesn't care where the tiles actually are, and will
     * return an index even for the space between two tiles. This function is
     * effectively the inverse of |positionTile_|.
     * @param {number} x The x coordinate, in pixels, relative to the left of
     *     |this|.
     * @param {number} y The y coordinate, in pixels, relative to the top of
     *     |this|.
     * @private
     */
    getWouldBeIndexForPoint_: function(x, y) {
      var grid = this.gridValues_;
      var layout = this.layoutValues_;

      var gridClientRect = this.tileGrid_.getBoundingClientRect();
      var col = Math.floor((x - gridClientRect.left - layout.leftMargin) /
                           layout.colWidth);
      if (col < 0 || col >= layout.numRowTiles)
        return -1;

      if (ntp4.isRTL())
        col = layout.numRowTiles - 1 - col;

      var row = Math.floor((y - gridClientRect.top) / layout.rowHeight);
      return row * layout.numRowTiles + col;
    },

    /**
     * Window resize event handler. Window resizes may trigger re-layouts.
     * @param {Object} e The resize event.
     */
    onResize_: function(e) {
      if (this.lastWidth_ == this.clientWidth &&
          this.lastHeight_ == this.clientHeight) {
        return;
      }

      this.calculateLayoutValues_();

      this.lastWidth_ = this.clientWidth;
      this.lastHeight_ = this.clientHeight;
      this.classList.add('animating-tile-page');
      this.heightChanged_();

      this.repositionTiles_();
    },

    /**
     * The tile grid has an image mask which fades at the edges. We only show
     * the mask when there is an active drag; it obscures doppleganger tiles
     * as they enter or exit the grid.
     * @private
     */
    updateMask_: function() {
      if (!this.isCurrentDragTarget) {
        this.tileGrid_.style.WebkitMaskBoxImage = '';
        return;
      }

      var leftMargin = this.layoutValues_.leftMargin;
      // The fade distance is the space between tiles.
      var fadeDistance = (this.gridValues_.tileSpacingFraction *
          this.layoutValues_.tileWidth);
      fadeDistance = Math.min(leftMargin, fadeDistance);
      // On Skia we don't use any fade because it works very poorly. See
      // http://crbug.com/99373
      if (!cr.isMac)
        fadeDistance = 1;
      var gradient =
          '-webkit-linear-gradient(left,' +
              'transparent, ' +
              'transparent ' + (leftMargin - fadeDistance) + 'px, ' +
              'black ' + leftMargin + 'px, ' +
              'black ' + (this.tileGrid_.clientWidth - leftMargin) + 'px, ' +
              'transparent ' + (this.tileGrid_.clientWidth - leftMargin +
                                fadeDistance) + 'px, ' +
              'transparent)';
      this.tileGrid_.style.WebkitMaskBoxImage = gradient;
    },

    updateTopMargin_: function() {
      var layout = this.layoutValues_;

      // The top margin is set so that the vertical midpoint of the grid will
      // be 1/3 down the page.
      var numTiles = this.tileCount +
          (this.isCurrentDragTarget && !this.withinPageDrag_ ? 1 : 0);
      var numRows = Math.ceil(numTiles / layout.numRowTiles);
      var usedHeight = layout.rowHeight * numRows;
      // 100 matches the top padding of tile-page.
      var newMargin = document.documentElement.clientHeight / 3 -
          usedHeight / 2 - 100 - this.content_.offsetTop;
      newMargin = Math.max(newMargin, 0);

      // |newMargin| is the final margin we actually want to show. However,
      // part of that should be animated and part should not (for the same
      // reason as with leftMargin). The approach is to consider differences
      // when the layout changes from wide to narrow or vice versa as
      // 'animatable'. These differences accumulate in animatedTopMarginPx_,
      // while topMarginPx_ caches the real (total) margin. Either of these
      // calculations may come out to be negative, so we use margins as the
      // css property.

      if (typeof this.topMarginIsForWide_ == 'undefined')
        this.topMarginIsForWide_ = layout.wide;
      if (this.topMarginIsForWide_ != layout.wide) {
        this.animatedTopMarginPx_ += newMargin - this.topMarginPx_;
        this.topMargin_.style.marginBottom =
            this.animatedTopMarginPx_ + 'px';
      }

      this.topMarginIsForWide_ = layout.wide;
      this.topMarginPx_ = newMargin;
      this.topMargin_.style.marginTop =
          (this.topMarginPx_ - this.animatedTopMarginPx_) + 'px';
    },

    /**
     * Handles final setup that can only happen after |this| is inserted into
     * the page.
     * @private
     */
    onNodeInsertedIntoDocument_: function(e) {
      this.calculateLayoutValues_();
      this.heightChanged_();
    },

    /**
     * Called when the height of |this| has changed: update the size of
     * tileGrid.
     * @private
     */
    heightChanged_: function() {
      // The tile grid will expand to the bottom footer, or enough to hold all
      // the tiles, whichever is greater. It would be nicer if tilePage were
      // a flex box, and the tile grid could be box-flex: 1, but this exposes a
      // bug where repositioning tiles will cause the scroll position to reset.
      this.tileGrid_.style.minHeight = (this.clientHeight -
          this.tileGrid_.offsetTop - this.content_.offsetTop -
          this.extraBottomPadding) + 'px';
    },

    /**
     * Scrolls the page in response to a mousewheel event.
     * @param {Event} e The mousewheel event.
     */
    handleMouseWheel: function(e) {
      this.content_.scrollTop -= e.wheelDeltaY / 3;
    },

    /**
     * Handles mouse wheels on |this|. We handle this explicitly because we want
     * a consistent experience whether the user scrolls on the page or on the
     * page switcher (handleMouseWheel provides a common conversion factor
     * between wheel delta and scroll delta).
     * @param {Event} e The mousewheel event.
     * @private
     */
    onMouseWheel_: function(e) {
      if (e.wheelDeltaY == 0)
        return;

      this.handleMouseWheel(e);
      e.preventDefault();
      e.stopPropagation();
    },

    /**
     * Handler for the 'scroll' event on |content_|.
     * @param {Event} e The scroll event.
     * @private
     */
    onScroll_: function(e) {
      this.queueUpdateScrollbars_();
    },

    /**
     * ID of scrollbar update timer. If 0, there's no scrollbar re-calc queued.
     * @private
     */
    scrollbarUpdate_: 0,

    /**
     * Queues an update on the custom scrollbar. Used for two reasons: first,
     * coalescing of multiple updates, and second, because action like
     * repositioning a tile can require a delay before they affect values
     * like clientHeight.
     * @private
     */
    queueUpdateScrollbars_: function() {
      if (this.scrollbarUpdate_)
        return;

      this.scrollbarUpdate_ = window.setTimeout(
          this.doUpdateScrollbars_.bind(this), 0);
    },

    /**
     * Does the work of calculating the visibility, height and position of the
     * scrollbar thumb (there is no track or buttons).
     * @private
     */
    doUpdateScrollbars_: function() {
      this.scrollbarUpdate_ = 0;

      var content = this.content_;

      // Adjust scroll-height to account for possible header-bar.
      var adjustedScrollHeight = content.scrollHeight - content.offsetTop;

      if (adjustedScrollHeight <= content.clientHeight) {
        this.scrollbar_.hidden = true;
        return;
      } else {
        this.scrollbar_.hidden = false;
      }

      var thumbTop = content.offsetTop +
          content.scrollTop / adjustedScrollHeight * content.clientHeight;
      var thumbHeight = content.clientHeight / adjustedScrollHeight *
          this.clientHeight;

      this.scrollbar_.style.top = thumbTop + 'px';
      this.scrollbar_.style.height = thumbHeight + 'px';
      this.firePageLayoutEvent_();
    },

    /**
     * Get the height for a tile of a certain width. Override this function to
     * get non-square tiles.
     * @param {number} width The pixel width of a tile.
     * @return {number} The height for |width|.
     */
    heightForWidth: function(width) {
      return width;
    },

    /** Dragging **/

    get isCurrentDragTarget() {
      return this.dragWrapper_.isCurrentDragTarget;
    },

    /**
     * Thunk for dragleave events fired on |tileGrid_|.
     * @param {Event} e A MouseEvent for the drag.
     */
    doDragLeave: function(e) {
      this.cleanupDrag();
    },

    /**
     * Performs all actions necessary when a drag enters the tile page.
     * @param {Event} e A mouseover event for the drag enter.
     */
    doDragEnter: function(e) {

      // Applies the mask so doppleganger tiles disappear into the fog.
      this.updateMask_();

      this.classList.add('animating-tile-page');
      this.withinPageDrag_ = this.contains(currentlyDraggingTile);
      this.dragItemIndex_ = this.withinPageDrag_ ?
          currentlyDraggingTile.index : this.tileElements_.length;
      this.currentDropIndex_ = this.dragItemIndex_;

      // The new tile may change the number of rows, hence the top margin
      // will change.
      if (!this.withinPageDrag_)
        this.updateTopMargin_();

      this.doDragOver(e);
    },

    /**
     * Performs all actions necessary when the user moves the cursor during
     * a drag over the tile page.
     * @param {Event} e A mouseover event for the drag over.
     */
    doDragOver: function(e) {
      e.preventDefault();

      this.setDropEffect(e.dataTransfer);
      var newDragIndex = this.getWouldBeIndexForPoint_(e.pageX, e.pageY);
      if (newDragIndex < 0 || newDragIndex >= this.tileElements_.length)
        newDragIndex = this.dragItemIndex_;
      this.updateDropIndicator_(newDragIndex);
    },

    /**
     * Performs all actions necessary when the user completes a drop.
     * @param {Event} e A mouseover event for the drag drop.
     */
    doDrop: function(e) {
      e.stopPropagation();

      var index = this.currentDropIndex_;
      // Only change data if this was not a 'null drag'.
      if (!((index == this.dragItemIndex_) && this.withinPageDrag_)) {
        var adjustedIndex = this.currentDropIndex_ +
            (index > this.dragItemIndex_ ? 1 : 0);
        if (this.withinPageDrag_) {
          this.tileGrid_.insertBefore(
              currentlyDraggingTile,
              this.tileElements_[adjustedIndex]);
          this.tileMoved(currentlyDraggingTile, this.dragItemIndex_);
        } else {
          var originalPage = currentlyDraggingTile ?
              currentlyDraggingTile.tilePage : null;
          this.addDragData(e.dataTransfer, adjustedIndex);
          if (originalPage)
            originalPage.cleanupDrag();
        }

        // Dropping the icon may cause topMargin to change, but changing it
        // now would cause everything to move (annoying), so we leave it
        // alone. The top margin will be re-calculated next time the window is
        // resized or the page is selected.
      }

      this.classList.remove('animating-tile-page');
      this.cleanupDrag();
    },

    /**
     * Appends the currently dragged tile to the end of the page. Called
     * from outside the page, e.g. when dropping on a nav dot.
     */
    appendDraggingTile: function() {
      var originalPage = currentlyDraggingTile.tilePage;
      if (originalPage == this)
        return;

      this.addDragData(null, this.tileElements_.length);
      originalPage.cleanupDrag();
    },

    /**
     * Makes sure all the tiles are in the right place after a drag is over.
     */
    cleanupDrag: function() {
      this.repositionTiles_(currentlyDraggingTile);
      // Remove the drag mask.
      this.updateMask_();
    },

    /**
     * Reposition all the tiles (possibly ignoring one).
     * @param {Node} ignoreNode An optional node to ignore.
     * @private
     */
    repositionTiles_: function(ignoreNode) {
      for (var i = 0; i < this.tileElements_.length; i++) {
        if (!ignoreNode || ignoreNode !== this.tileElements_[i])
          this.positionTile_(i);
      }
    },

    /**
     * Updates the visual indicator for the drop location for the active drag.
     * @param {Event} e A MouseEvent for the drag.
     * @private
     */
    updateDropIndicator_: function(newDragIndex) {
      var oldDragIndex = this.currentDropIndex_;
      if (newDragIndex == oldDragIndex)
        return;

      var repositionStart = Math.min(newDragIndex, oldDragIndex);
      var repositionEnd = Math.max(newDragIndex, oldDragIndex);

      for (var i = repositionStart; i <= repositionEnd; i++) {
        if (i == this.dragItemIndex_)
          continue;
        else if (i > this.dragItemIndex_)
          var adjustment = i <= newDragIndex ? -1 : 0;
        else
          var adjustment = i >= newDragIndex ? 1 : 0;

        this.positionTile_(i, adjustment);
      }
      this.currentDropIndex_ = newDragIndex;
    },

    /**
     * Checks if a page can accept a drag with the given data.
     * @param {Event} e The drag event if the drag object. Implementations will
     *     likely want to check |e.dataTransfer|.
     * @return {boolean} True if this page can handle the drag.
     */
    shouldAcceptDrag: function(e) {
      return false;
    },

    /**
     * Called to accept a drag drop. Will not be called for in-page drops.
     * @param {Object} dataTransfer The data transfer object that holds the drop
     *     data. This should only be used if currentlyDraggingTile is null.
     * @param {number} index The tile index at which the drop occurred.
     */
    addDragData: function(dataTransfer, index) {
      assert(false);
    },

    /**
     * Called when a tile has been moved (via dragging). Override this to make
     * backend updates.
     * @param {Node} draggedTile The tile that was dropped.
     * @param {number} prevIndex The previous index of the tile.
     */
    tileMoved: function(draggedTile, prevIndex) {
    },

    /**
     * Sets the drop effect on |dataTransfer| to the desired value (e.g.
     * 'copy').
     * @param {Object} dataTransfer The drag event dataTransfer object.
     */
    setDropEffect: function(dataTransfer) {
      assert(false);
    },
  };

  return {
    getCurrentlyDraggingTile: getCurrentlyDraggingTile,
    setCurrentDropEffect: setCurrentDropEffect,
    TilePage: TilePage,
  };
});
