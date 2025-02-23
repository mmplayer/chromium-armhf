// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
// Globals:
var RESULTS_PER_PAGE = 150;
var MAX_SEARCH_DEPTH_MONTHS = 18;

// Amount of time between pageviews that we consider a 'break' in browsing,
// measured in milliseconds.
var BROWSING_GAP_TIME = 15 * 60 * 1000;

function $(o) {return document.getElementById(o);}

function createElementWithClassName(type, className) {
  var elm = document.createElement(type);
  elm.className = className;
  return elm;
}

// Escapes a URI as appropriate for CSS.
function encodeURIForCSS(uri) {
  // CSS uris need to have '(' and ')' escaped.
  return uri.replace(/\(/g, "\\(").replace(/\)/g, "\\)");
}

// TODO(glen): Get rid of these global references, replace with a controller
//     or just make the classes own more of the page.
var historyModel;
var historyView;
var localStrings;
var pageState;
var deleteQueue = [];
var deleteInFlight = false;
var selectionAnchor = -1;
var id2checkbox = [];


///////////////////////////////////////////////////////////////////////////////
// Page:
/**
 * Class to hold all the information about an entry in our model.
 * @param {Object} result An object containing the page's data.
 * @param {boolean} continued Whether this page is on the same day as the
 *     page before it
 */
function Page(result, continued, model, id) {
  this.model_ = model;
  this.title_ = result.title;
  this.url_ = result.url;
  this.starred_ = result.starred;
  this.snippet_ = result.snippet || "";
  this.id_ = id;

  this.changed = false;

  this.isRendered = false;

  // All the date information is public so that owners can compare properties of
  // two items easily.

  // We get the time in seconds, but we want it in milliseconds.
  this.time = new Date(result.time * 1000);

  // See comment in BrowsingHistoryHandler::QueryComplete - we won't always
  // get all of these.
  this.dateRelativeDay = result.dateRelativeDay || "";
  this.dateTimeOfDay = result.dateTimeOfDay || "";
  this.dateShort = result.dateShort || "";

  // Whether this is the continuation of a previous day.
  this.continued = continued;
}

// Page, Public: --------------------------------------------------------------
/**
 * @return {DOMObject} Gets the DOM representation of the page
 *     for use in browse results.
 */
Page.prototype.getBrowseResultDOM = function() {
  var node = createElementWithClassName('div', 'entry');
  var time = createElementWithClassName('div', 'time');
  if (this.model_.getEditMode()) {
    var checkbox = document.createElement('input');
    checkbox.type = "checkbox";
    checkbox.name = this.id_;
    checkbox.time = this.time.toString();
    checkbox.addEventListener("click", checkboxClicked, false);
    id2checkbox[this.id_] = checkbox;
    time.appendChild(checkbox);
  }
  time.appendChild(document.createTextNode(this.dateTimeOfDay));
  node.appendChild(time);
  node.appendChild(this.getTitleDOM_());
  return node;
};

/**
 * @return {DOMObject} Gets the DOM representation of the page for
 *     use in search results.
 */
Page.prototype.getSearchResultDOM = function() {
  var row = createElementWithClassName('tr', 'entry');
  var datecell = createElementWithClassName('td', 'time');
  datecell.appendChild(document.createTextNode(this.dateShort));
  row.appendChild(datecell);
  if (this.model_.getEditMode()) {
    var checkbox = document.createElement('input');
    checkbox.type = "checkbox";
    checkbox.name = this.id_;
    checkbox.time = this.time.toString();
    checkbox.addEventListener("click", checkboxClicked, false);
    id2checkbox[this.id_] = checkbox;
    datecell.appendChild(checkbox);
  }

  var titleCell = document.createElement('td');
  titleCell.valign = 'top';
  titleCell.appendChild(this.getTitleDOM_());
  var snippet = createElementWithClassName('div', 'snippet');
  this.addHighlightedText_(snippet,
                           this.snippet_,
                           this.model_.getSearchText());
  titleCell.appendChild(snippet);
  row.appendChild(titleCell);

  return row;
};

// Page, private: -------------------------------------------------------------
/**
 * Add child text nodes to a node such that occurrences of the spcified text is
 * highligted.
 * @param {Node} node The node under which new text nodes will be made as
 *     children.
 * @param {string} content Text to be added beneath |node| as one or more
 *     text nodes.
 * @param {string} highlightText Occurences of this text inside |content| will
 *     be highlighted.
 */
Page.prototype.addHighlightedText_ = function(node, content, highlightText) {
  var i = 0;
  if (highlightText) {
    var re = new RegExp(Page.pregQuote_(highlightText), 'gim');
    var match;
    while (match = re.exec(content)) {
      if (match.index > i)
        node.appendChild(document.createTextNode(content.slice(i,
                                                               match.index)));
      i = re.lastIndex;
      // Mark the highlighted text in bold.
      var b = document.createElement('b');
      b.textContent = content.substring(match.index, i);
      node.appendChild(b);
    }
  }
  if (i < content.length)
    node.appendChild(document.createTextNode(content.slice(i)));
};

/**
 * @return {DOMObject} DOM representation for the title block.
 */
Page.prototype.getTitleDOM_ = function() {
  var node = document.createElement('div');
  node.className = 'title';
  var link = document.createElement('a');
  link.href = this.url_;

  link.style.backgroundImage =
      'url(chrome://favicon/' + encodeURIForCSS(this.url_) + ')';
  link.id = "id-" + this.id_;
  this.addHighlightedText_(link, this.title_, this.model_.getSearchText());

  node.appendChild(link);

  if (this.starred_) {
    node.className += ' starred';
    node.appendChild(createElementWithClassName('div', 'starred'));
  }

  return node;
};

// Page, private, static: -----------------------------------------------------

/**
 * Quote a string so it can be used in a regular expression.
 * @param {string} str The source string
 * @return {string} The escaped string
 */
Page.pregQuote_ = function(str) {
  return str.replace(/([\\\.\+\*\?\[\^\]\$\(\)\{\}\=\!\<\>\|\:])/g, "\\$1");
};

///////////////////////////////////////////////////////////////////////////////
// HistoryModel:
/**
 * Global container for history data. Future optimizations might include
 * allowing the creation of a HistoryModel for each search string, allowing
 * quick flips back and forth between results.
 *
 * The history model is based around pages, and only fetching the data to
 * fill the currently requested page. This is somewhat dependent on the view,
 * and so future work may wish to change history model to operate on
 * timeframe (day or week) based containers.
 */
function HistoryModel() {
  this.clearModel_();
  this.setEditMode(false);
  this.view_;
}

// HistoryModel, Public: ------------------------------------------------------
/**
 * Sets our current view that is called when the history model changes.
 * @param {HistoryView} view The view to set our current view to.
 */
HistoryModel.prototype.setView = function(view) {
  this.view_ = view;
};

/**
 * Start a new search - this will clear out our model.
 * @param {String} searchText The text to search for
 * @param {Number} opt_page The page to view - this is mostly used when setting
 *     up an initial view, use #requestPage otherwise.
 */
HistoryModel.prototype.setSearchText = function(searchText, opt_page) {
  this.clearModel_();
  this.searchText_ = searchText;
  this.requestedPage_ = opt_page ? opt_page : 0;
  this.getSearchResults_();
};

/**
 * Reload our model with the current parameters.
 */
HistoryModel.prototype.reload = function() {
  var search = this.searchText_;
  var page = this.requestedPage_;
  this.clearModel_();
  this.searchText_ = search;
  this.requestedPage_ = page;
  this.getSearchResults_();
};

/**
 * @return {String} The current search text.
 */
HistoryModel.prototype.getSearchText = function() {
  return this.searchText_;
};

/**
 * Tell the model that the view will want to see the current page. When
 * the data becomes available, the model will call the view back.
 * @page {Number} page The page we want to view.
 */
HistoryModel.prototype.requestPage = function(page) {
  this.requestedPage_ = page;
  this.changed = true;
  this.updateSearch_(false);
};

/**
 * Receiver for history query.
 * @param {String} term The search term that the results are for.
 * @param {Array} results A list of results
 */
HistoryModel.prototype.addResults = function(info, results) {
  this.inFlight_ = false;
  if (info.term != this.searchText_) {
    // If our results aren't for our current search term, they're rubbish.
    return;
  }

  // Currently we assume we're getting things in date order. This needs to
  // be updated if that ever changes.
  if (results) {
    var lastURL, lastDay;
    var oldLength = this.pages_.length;
    if (oldLength) {
      var oldPage = this.pages_[oldLength - 1];
      lastURL = oldPage.url;
      lastDay = oldPage.dateRelativeDay;
    }

    for (var i = 0, thisResult; thisResult = results[i]; i++) {
      var thisURL = thisResult.url;
      var thisDay = thisResult.dateRelativeDay;

      // Remove adjacent duplicates.
      if (!lastURL || lastURL != thisURL) {
        // Figure out if this page is in the same day as the previous page,
        // this is used to determine how day headers should be drawn.
        this.pages_.push(new Page(thisResult, thisDay == lastDay, this,
            this.last_id_++));
        lastDay = thisDay;
        lastURL = thisURL;
      }
    }
    if (results.length)
      this.changed = true;
  }

  this.updateSearch_(info.finished);
};

/**
 * @return {Number} The number of pages in the model.
 */
HistoryModel.prototype.getSize = function() {
  return this.pages_.length;
};

/**
 * @return {boolean} Whether our history query has covered all of
 *     the user's history
 */
HistoryModel.prototype.isComplete = function() {
  return this.complete_;
};

/**
 * Get a list of pages between specified index positions.
 * @param {Number} start The start index
 * @param {Number} end The end index
 * @return {Array} A list of pages
 */
HistoryModel.prototype.getNumberedRange = function(start, end) {
  if (start >= this.getSize())
    return [];

  var end = end > this.getSize() ? this.getSize() : end;
  return this.pages_.slice(start, end);
};

/**
 * @return {boolean} Whether we are in edit mode where history items can be
 *    deleted
 */
HistoryModel.prototype.getEditMode = function() {
  return this.editMode_;
};

/**
 * @param {boolean} edit_mode Control whether we are in edit mode.
 */
HistoryModel.prototype.setEditMode = function(edit_mode) {
  this.editMode_ = edit_mode;
};

// HistoryModel, Private: -----------------------------------------------------
HistoryModel.prototype.clearModel_ = function() {
  this.inFlight_ = false; // Whether a query is inflight.
  this.searchText_ = '';
  this.searchDepth_ = 0;
  this.pages_ = []; // Date-sorted list of pages.
  this.last_id_ = 0;
  selectionAnchor = -1;
  id2checkbox = [];

  // The page that the view wants to see - we only fetch slightly past this
  // point. If the view requests a page that we don't have data for, we try
  // to fetch it and call back when we're done.
  this.requestedPage_ = 0;

  this.complete_ = false;

  if (this.view_) {
    this.view_.clear_();
  }
};

/**
 * Figure out if we need to do more searches to fill the currently requested
 * page. If we think we can fill the page, call the view and let it know
 * we're ready to show something.
 */
HistoryModel.prototype.updateSearch_ = function(finished) {
  if ((this.searchText_ && this.searchDepth_ >= MAX_SEARCH_DEPTH_MONTHS) ||
      finished) {
    // We have maxed out. There will be no more data.
    this.complete_ = true;
    this.view_.onModelReady();
    this.changed = false;
    chrome.send('setIsLoading', ['false']);
  } else {
    // If we can't fill the requested page, ask for more data unless a request
    // is still in-flight.
    if (!this.inFlight_) {
      if (!this.canFillPage_(this.requestedPage_))
        this.getSearchResults_(this.searchDepth_ + 1);
      else
        chrome.send('setIsLoading', ['false']);
    }

    // If we have any data for the requested page, show it.
    if (this.changed && this.haveDataForPage_(this.requestedPage_)) {
      this.view_.onModelReady();
      this.changed = false;
    }
  }
};

/**
 * Get search results for a selected depth. Our history system is optimized
 * for queries that don't cross month boundaries, but an entire month's
 * worth of data is huge. When we're in browse mode (searchText is empty)
 * we request the data a day at a time. When we're searching, a month is
 * used.
 *
 * TODO: Fix this for when the user's clock goes across month boundaries.
 * @param {number} opt_day How many days back to do the search.
 */
HistoryModel.prototype.getSearchResults_ = function(depth) {
  this.searchDepth_ = depth || 0;

  if (this.searchText_ == "") {
    chrome.send('getHistory',
        [String(this.searchDepth_)]);
  } else {
    chrome.send('searchHistory',
        [this.searchText_, String(this.searchDepth_)]);
  }

  this.inFlight_ = true;
  chrome.send('setIsLoading', ['true']);
};

/**
 * Check to see if we have data for a given page.
 * @param {number} page The page number
 * @return {boolean} Whether we have any data for the given page.
 */
HistoryModel.prototype.haveDataForPage_ = function(page) {
  return (page * RESULTS_PER_PAGE < this.getSize());
};

/**
 * Check to see if we have data to fill a page.
 * @param {number} page The page number.
 * @return {boolean} Whether we have data to fill the page.
 */
HistoryModel.prototype.canFillPage_ = function(page) {
  return ((page + 1) * RESULTS_PER_PAGE <= this.getSize());
};

///////////////////////////////////////////////////////////////////////////////
// HistoryView:
/**
 * Functions and state for populating the page with HTML. This should one-day
 * contain the view and use event handlers, rather than pushing HTML out and
 * getting called externally.
 * @param {HistoryModel} model The model backing this view.
 */
function HistoryView(model) {
  this.summaryTd_ = $('results-summary');
  this.summaryTd_.textContent = localStrings.getString('loading');
  this.editButtonTd_ = $('edit-button');
  this.editingControlsDiv_ = $('editing-controls');
  this.resultDiv_ = $('results-display');
  this.pageDiv_ = $('results-pagination');
  this.model_ = model
  this.pageIndex_ = 0;
  this.lastDisplayed_ = [];

  this.model_.setView(this);

  this.currentPages_ = [];

  var self = this;
  window.onresize = function() {
    self.updateEntryAnchorWidth_();
  };
  self.updateEditControls_();

  this.boundUpdateRemoveButton_ = function(e) {
    return self.updateRemoveButton_(e);
  };
}

// HistoryView, public: -------------------------------------------------------
/**
 * Do a search and optionally view a certain page.
 * @param {string} term The string to search for.
 * @param {number} opt_page The page we wish to view, only use this for
 *     setting up initial views, as this triggers a search.
 */
HistoryView.prototype.setSearch = function(term, opt_page) {
  this.pageIndex_ = parseInt(opt_page || 0, 10);
  window.scrollTo(0, 0);
  this.model_.setSearchText(term, this.pageIndex_);
  this.updateEditControls_();
  pageState.setUIState(this.model_.getEditMode(), term, this.pageIndex_);
};

/**
 * Controls edit mode where history can be deleted.
 * @param {boolean} edit_mode Whether to enable edit mode.
 */
HistoryView.prototype.setEditMode = function(edit_mode) {
  this.model_.setEditMode(edit_mode);
  pageState.setUIState(this.model_.getEditMode(), this.model_.getSearchText(),
                       this.pageIndex_);
};

/**
 * Toggles the edit mode and triggers UI update.
 */
HistoryView.prototype.toggleEditMode = function() {
  var editMode = !this.model_.getEditMode();
  this.setEditMode(editMode);
  this.updateEditControls_();
};

/**
 * @return {boolean} Whether we are in edit mode where history items can be
 *    deleted
 */
HistoryView.prototype.getEditMode = function() {
  return this.model_.getEditMode();
};

/**
 * Reload the current view.
 */
HistoryView.prototype.reload = function() {
  this.model_.reload();
};

/**
 * Switch to a specified page.
 * @param {number} page The page we wish to view.
 */
HistoryView.prototype.setPage = function(page) {
  this.clear_();
  this.pageIndex_ = parseInt(page, 10);
  window.scrollTo(0, 0);
  this.model_.requestPage(page);
  pageState.setUIState(this.model_.getEditMode(), this.model_.getSearchText(),
      this.pageIndex_);
};

/**
 * @return {number} The page number being viewed.
 */
HistoryView.prototype.getPage = function() {
  return this.pageIndex_;
};

/**
 * Callback for the history model to let it know that it has data ready for us
 * to view.
 */
HistoryView.prototype.onModelReady = function() {
  this.displayResults_();
};

// HistoryView, private: ------------------------------------------------------
/**
 * Clear the results in the view.  Since we add results piecemeal, we need
 * to clear them out when we switch to a new page or reload.
 */
HistoryView.prototype.clear_ = function() {
  this.resultDiv_.textContent = '';

  var pages = this.currentPages_;
  for (var i = 0; i < pages.length; i++) {
    pages[i].isRendered = false;
  }
  this.currentPages_ = [];
};

HistoryView.prototype.setPageRendered_ = function(page) {
  page.isRendered = true;
  this.currentPages_.push(page);
};

/**
 * Update the page with results.
 */
HistoryView.prototype.displayResults_ = function() {
  var results = this.model_.getNumberedRange(
      this.pageIndex_ * RESULTS_PER_PAGE,
      this.pageIndex_ * RESULTS_PER_PAGE + RESULTS_PER_PAGE);

  if (this.model_.getSearchText()) {
    var resultTable = createElementWithClassName('table', 'results');
    resultTable.cellSpacing = 0;
    resultTable.cellPadding = 0;
    resultTable.border = 0;

    for (var i = 0, page; page = results[i]; i++) {
      if (!page.isRendered) {
        resultTable.appendChild(page.getSearchResultDOM());
        this.setPageRendered_(page);
      }
    }
    this.resultDiv_.appendChild(resultTable);
  } else {
    var lastTime = Math.infinity;
    for (var i = 0, page; page = results[i]; i++) {
      if (page.isRendered) {
        continue;
      }
      // Break across day boundaries and insert gaps for browsing pauses.
      var thisTime = page.time.getTime();

      if ((i == 0 && page.continued) || !page.continued) {
        var day = createElementWithClassName('div', 'day');
        day.appendChild(document.createTextNode(page.dateRelativeDay));

        if (i == 0 && page.continued) {
          day.appendChild(document.createTextNode(' ' +
              localStrings.getString('cont')));
        }

        this.resultDiv_.appendChild(day);
      } else if (lastTime - thisTime > BROWSING_GAP_TIME) {
        this.resultDiv_.appendChild(createElementWithClassName('div', 'gap'));
      }
      lastTime = thisTime;

      // Add entry.
      this.resultDiv_.appendChild(page.getBrowseResultDOM());
      this.setPageRendered_(page);
    }
  }

  this.displaySummaryBar_();
  this.displayNavBar_();
  this.updateEntryAnchorWidth_();
};

/**
 * Update the summary bar with descriptive text.
 */
HistoryView.prototype.displaySummaryBar_ = function() {
  var searchText = this.model_.getSearchText();
  if (searchText != '') {
    this.summaryTd_.textContent = localStrings.getStringF('searchresultsfor',
        searchText);
  } else {
    this.summaryTd_.textContent = localStrings.getString('history');
  }
};

/**
 * Update the widgets related to edit mode.
 */
HistoryView.prototype.updateEditControls_ = function() {
  // Display a button (looking like a link) to enable/disable edit mode.
  var oldButton = this.editButtonTd_.firstChild;
  var editMode = this.model_.getEditMode();
  var button = createElementWithClassName('button', 'edit-button');
  button.onclick = toggleEditMode;
  button.textContent = localStrings.getString(editMode ?
                                              'doneediting' : 'edithistory');
  this.editButtonTd_.replaceChild(button, oldButton);

  this.editingControlsDiv_.textContent = '';

  if (editMode) {
    // Button to delete the selected items.
    button = document.createElement('button');
    button.onclick = removeItems;
    button.textContent = localStrings.getString('removeselected');
    button.disabled = true;
    this.editingControlsDiv_.appendChild(button);
    this.removeButton_ = button;

    // Button that opens up the clear browsing data dialog.
    button = document.createElement('button');
    button.onclick = openClearBrowsingData;
    button.textContent = localStrings.getString('clearallhistory');
    this.editingControlsDiv_.appendChild(button);

    // Listen for clicks in the page to sync the disabled state.
    document.addEventListener('click', this.boundUpdateRemoveButton_);
  } else {
    this.removeButton_ = null;
    document.removeEventListener('click', this.boundUpdateRemoveButton_);
  }
};

/**
 * Updates the disabled state of the remove button when in editing mode.
 * @param {!Event} e The click event object.
 * @private
 */
HistoryView.prototype.updateRemoveButton_ = function(e) {
  if (e.target.tagName != 'INPUT')
    return;

  var anyChecked = document.querySelector('.entry input:checked') != null;
  if (this.removeButton_)
    this.removeButton_.disabled = !anyChecked;
};

/**
 * Update the pagination tools.
 */
HistoryView.prototype.displayNavBar_ = function() {
  this.pageDiv_.textContent = '';

  if (this.pageIndex_ > 0) {
    this.pageDiv_.appendChild(
        this.createPageNav_(0, localStrings.getString('newest')));
    this.pageDiv_.appendChild(
        this.createPageNav_(this.pageIndex_ - 1,
                            localStrings.getString('newer')));
  }

  // TODO(feldstein): this causes the navbar to not show up when your first
  // page has the exact amount of results as RESULTS_PER_PAGE.
  if (this.model_.getSize() > (this.pageIndex_ + 1) * RESULTS_PER_PAGE) {
    this.pageDiv_.appendChild(
        this.createPageNav_(this.pageIndex_ + 1,
                            localStrings.getString('older')));
  }
};

/**
 * Make a DOM object representation of a page navigation link.
 * @param {number} page The page index the navigation element should link to
 * @param {string} name The text content of the link
 * @return {HTMLAnchorElement} the pagination link
 */
HistoryView.prototype.createPageNav_ = function(page, name) {
  anchor = document.createElement('a');
  anchor.className = 'page-navigation';
  anchor.textContent = name;
  var hashString = PageState.getHashString(this.model_.getEditMode(),
                                           this.model_.getSearchText(), page);
  var link = 'chrome://history/' + (hashString ? '#' + hashString : '');
  anchor.href = link;
  anchor.onclick = function() {
    setPage(page);
    return false;
  };
  return anchor;
};

/**
 * Updates the CSS rule for the entry anchor.
 * @private
 */
HistoryView.prototype.updateEntryAnchorWidth_ = function() {
  // We need to have at least on .title div to be able to calculate the
  // desired width of the anchor.
  var titleElement = document.querySelector('.entry .title');
  if (!titleElement)
    return;

  // Create new CSS rules and add them last to the last stylesheet.
  if (!this.entryAnchorRule_) {
     var styleSheets = document.styleSheets;
     var styleSheet = styleSheets[styleSheets.length - 1];
     var rules = styleSheet.cssRules;
     var createRule = function(selector) {
       styleSheet.insertRule(selector + '{}', rules.length);
       return rules[rules.length - 1];
     };
     this.entryAnchorRule_ = createRule('.entry .title > a');
     // The following rule needs to be more specific to have higher priority.
     this.entryAnchorStarredRule_ = createRule('.entry .title.starred > a');
   }

   var anchorMaxWith = titleElement.offsetWidth;
   this.entryAnchorRule_.style.maxWidth = anchorMaxWith + 'px';
   // Adjust by the width of star plus its margin.
   this.entryAnchorStarredRule_.style.maxWidth = anchorMaxWith - 23 + 'px';
};

///////////////////////////////////////////////////////////////////////////////
// State object:
/**
 * An 'AJAX-history' implementation.
 * @param {HistoryModel} model The model we're representing
 * @param {HistoryView} view The view we're representing
 */
function PageState(model, view) {
  // Enforce a singleton.
  if (PageState.instance) {
    return PageState.instance;
  }

  this.model = model;
  this.view = view;

  if (typeof this.checker_ != 'undefined' && this.checker_) {
    clearInterval(this.checker_);
  }

  // TODO(glen): Replace this with a bound method so we don't need
  //     public model and view.
  this.checker_ = setInterval((function(state_obj) {
    var hashData = state_obj.getHashData();

    if (hashData.q != state_obj.model.getSearchText(term)) {
      state_obj.view.setSearch(hashData.q, parseInt(hashData.p, 10));
    } else if (parseInt(hashData.p, 10) != state_obj.view.getPage()) {
      state_obj.view.setPage(hashData.p);
    }
  }), 50, this);
}

PageState.instance = null;

/**
 * @return {Object} An object containing parameters from our window hash.
 */
PageState.prototype.getHashData = function() {
  var result = {
    e : 0,
    q : '',
    p : 0
  };

  if (!window.location.hash) {
    return result;
  }

  var hashSplit = window.location.hash.substr(1).split('&');
  for (var i = 0; i < hashSplit.length; i++) {
    var pair = hashSplit[i].split('=');
    if (pair.length > 1) {
      result[pair[0]] = decodeURIComponent(pair[1].replace(/\+/g, ' '));
    }
  }

  return result;
};

/**
 * Set the hash to a specified state, this will create an entry in the
 * session history so the back button cycles through hash states, which
 * are then picked up by our listener.
 * @param {string} term The current search string.
 * @param {string} page The page currently being viewed.
 */
PageState.prototype.setUIState = function(editMode, term, page) {
  // Make sure the form looks pretty.
  document.forms[0].term.value = term;
  var currentHash = this.getHashData();
  if (Boolean(currentHash.e) != editMode || currentHash.q != term ||
      currentHash.p != page) {
    window.location.hash = PageState.getHashString(editMode, term, page);
  }
};

/**
 * Static method to get the hash string for a specified state
 * @param {string} term The current search string.
 * @param {string} page The page currently being viewed.
 * @return {string} The string to be used in a hash.
 */
PageState.getHashString = function(editMode, term, page) {
  var newHash = [];
  if (editMode) {
    newHash.push('e=1');
  }
  if (term) {
    newHash.push('q=' + encodeURIComponent(term));
  }
  if (page != undefined) {
    newHash.push('p=' + page);
  }

  return newHash.join('&');
};

///////////////////////////////////////////////////////////////////////////////
// Document Functions:
/**
 * Window onload handler, sets up the page.
 */
function load() {
  $('term').focus();

  localStrings = new LocalStrings();
  historyModel = new HistoryModel();
  historyView = new HistoryView(historyModel);
  pageState = new PageState(historyModel, historyView);

  // Create default view.
  var hashData = pageState.getHashData();
  if (Boolean(hashData.e)) {
    historyView.toggleEditMode();
  }
  historyView.setSearch(hashData.q, hashData.p);

  // Add handlers to HTML elements.
  $('history-section').onclick = function () {
    setSearch('');
    return false;
  };
  $('search-form').onsubmit = function () {
    setSearch(this.term.value);
    return false;
  };
}

/**
 * TODO(glen): Get rid of this function.
 * Set the history view to a specified page.
 * @param {String} term The string to search for
 */
function setSearch(term) {
  if (historyView) {
    historyView.setSearch(term);
  }
}

/**
 * TODO(glen): Get rid of this function.
 * Set the history view to a specified page.
 * @param {number} page The page to set the view to.
 */
function setPage(page) {
  if (historyView) {
    historyView.setPage(page);
  }
}

/**
 * TODO(glen): Get rid of this function.
 * Toggles edit mode.
 */
function toggleEditMode() {
  if (historyView) {
    historyView.toggleEditMode();
    historyView.reload();
  }
}

/**
 * Delete the next item in our deletion queue.
 */
function deleteNextInQueue() {
  if (!deleteInFlight && deleteQueue.length) {
    deleteInFlight = true;
    chrome.send('removeURLsOnOneDay',
                [String(deleteQueue[0])].concat(deleteQueue[1]));
  }
}

/**
 * Open the clear browsing data dialog.
 */
function openClearBrowsingData() {
  chrome.send('clearBrowsingData', []);
  return false;
}

/**
 * Collect IDs from checked checkboxes and send to Chrome for deletion.
 */
function removeItems() {
  var checkboxes = document.getElementsByTagName('input');
  var ids = [];
  var disabledItems = [];
  var queue = [];
  var date = new Date();
  for (var i = 0; i < checkboxes.length; i++) {
    if (checkboxes[i].type == 'checkbox' && checkboxes[i].checked &&
        !checkboxes[i].disabled) {
      var cbDate = new Date(checkboxes[i].time);
      if (date.getFullYear() != cbDate.getFullYear() ||
          date.getMonth() != cbDate.getMonth() ||
          date.getDate() != cbDate.getDate()) {
        if (ids.length > 0) {
          queue.push(date.valueOf() / 1000);
          queue.push(ids);
        }
        ids = [];
        date = cbDate;
      }
      var link = $('id-' + checkboxes[i].name);
      checkboxes[i].disabled = true;
      link.style.textDecoration = 'line-through';
      disabledItems.push(checkboxes[i]);
      ids.push(link.href);
    }
  }
  if (ids.length > 0) {
    queue.push(date.valueOf() / 1000);
    queue.push(ids);
  }
  if (queue.length > 0) {
    if (confirm(localStrings.getString('deletewarning'))) {
      deleteQueue = deleteQueue.concat(queue);
      deleteNextInQueue();
    } else {
      // If the remove is cancelled, return the checkboxes to their
      // enabled, non-line-through state.
      for (var i = 0; i < disabledItems.length; i++) {
        var link = $('id-' + disabledItems[i].name);
        disabledItems[i].disabled = false;
        link.style.textDecoration = '';
      }
    }
  }
  return false;
}

/**
 * Toggle state of checkbox and handle Shift modifier.
 */
function checkboxClicked(event) {
  if (event.shiftKey && (selectionAnchor != -1)) {
    var checked = this.checked;
    // Set all checkboxes from the anchor up to the clicked checkbox to the
    // state of the clicked one.
    var begin = Math.min(this.name, selectionAnchor);
    var end = Math.max(this.name, selectionAnchor);
    for (var i = begin; i <= end; i++) {
      id2checkbox[i].checked = checked;
    }
  }
  selectionAnchor = this.name;
  this.focus();
}

///////////////////////////////////////////////////////////////////////////////
// Chrome callbacks:
/**
 * Our history system calls this function with results from searches.
 */
function historyResult(info, results) {
  historyModel.addResults(info, results);
}

/**
 * Our history system calls this function when a deletion has finished.
 */
function deleteComplete() {
  window.console.log('Delete complete');
  deleteInFlight = false;
  if (deleteQueue.length > 2) {
    deleteQueue = deleteQueue.slice(2);
    deleteNextInQueue();
  } else {
    deleteQueue = [];
  }
}

/**
 * Our history system calls this function if a delete is not ready (e.g.
 * another delete is in-progress).
 */
function deleteFailed() {
  window.console.log('Delete failed');
  // The deletion failed - try again later.
  deleteInFlight = false;
  setTimeout(deleteNextInQueue, 500);
}

/**
 * We're called when something is deleted (either by us or by someone
 * else).
 */
function historyDeleted() {
  window.console.log('History deleted');
  var anyChecked = document.querySelector('.entry input:checked') != null;
  if (!(historyView.getEditMode() && anyChecked))
    historyView.reload();
}

document.addEventListener('DOMContentLoaded', load);
