// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var BASE_KEYBOARD = {
  top: 0,
  left: 0,
  width: 1237,
  height: 514
};

var BASE_INSTRUCTIONS = {
  top: 194,
  left: 370,
  width: 498,
  height: 112
};

var MODIFIER_TO_CLASS = {
  'SHIFT': 'modifier-shift',
  'CTRL': 'modifier-ctrl',
  'ALT': 'modifier-alt'
};

var IDENTIFIER_TO_CLASS = {
  '2A': 'is-shift',
  '1D': 'is-ctrl',
  '38': 'is-alt'
};

var LABEL_TO_IDENTIFIER = {
  'search': 'E0 5B',
  'ctrl': '1D',
  'alt': '38',
  'caps lock': '3A',
  'disabled': 'DISABLED'
}

var keyboardOverlayId = 'en_US';
var identifierMap = {};

/**
 * Returns layouts data.
 */
function getLayouts() {
  return keyboardOverlayData['layouts'];
}

/**
 * Returns shortcut data.
 */
function getShortcutData() {
  return keyboardOverlayData['shortcut'];
}

/**
 * Returns the keyboard overlay ID.
 */
function getKeyboardOverlayId() {
  return keyboardOverlayId;
}

/**
 * Returns keyboard glyph data.
 */
function getKeyboardGlyphData() {
  return keyboardOverlayData['keyboardGlyph'][getKeyboardOverlayId()];
}

/**
 * Converts a single hex number to a character.
 */
function hex2char(hex) {
  if (!hex) {
    return '';
  }
  var result = '';
  var n = parseInt(hex, 16);
  if (n <= 0xFFFF) {
    result += String.fromCharCode(n);
  } else if (n <= 0x10FFFF) {
    n -= 0x10000;
    result += (String.fromCharCode(0xD800 | (n >> 10)) +
               String.fromCharCode(0xDC00 | (n & 0x3FF)));
  } else {
    console.error('hex2Char error: Code point out of range :' + hex);
  }
  return result;
}

/**
 * Returns a list of modifiers from the key event.
 */
function getModifiers(e) {
  if (!e) {
    return [];
  }
  var isKeyDown = (e.type == 'keydown');
  var keyCodeToModifier = {
    16: 'SHIFT',
    17: 'CTRL',
    18: 'ALT',
    91: 'ALT', // left ALT pressed with SHIFT
    92: 'ALT', // right ALT pressed with SHIFT
  };
  var modifierWithKeyCode = keyCodeToModifier[e.keyCode];
  var isPressed = {'SHIFT': e.shiftKey, 'CTRL': e.ctrlKey, 'ALT': e.altKey};
  // if e.keyCode is one of Shift, Ctrl and Alt, isPressed should
  // be changed because the key currently pressed
  // does not affect the values of e.shiftKey, e.ctrlKey and e.altKey
  if(modifierWithKeyCode){
    isPressed[modifierWithKeyCode] = isKeyDown;
  }
  // make the result array
  return ['SHIFT', 'CTRL', 'ALT'].filter(
      function(modifier) {
        return isPressed[modifier];
      }).sort();
}

/**
 * Returns an ID of the key.
 */
function keyId(identifier, i) {
  return identifier + '-key-' + i;
}

/**
 * Returns an ID of the text on the key.
 */
function keyTextId(identifier, i) {
  return identifier + '-key-text-' + i;
}

/**
 * Returns an ID of the shortcut text.
 */
function shortcutTextId(identifier, i) {
  return identifier + '-shortcut-text-' + i;
}

/**
 * Returns true if |list| contains |e|.
 */
function contains(list, e) {
  return list.indexOf(e) != -1;
}

/**
 * Returns a list of the class names corresponding to the identifier and
 * modifiers.
 */
function getKeyClasses(identifier, modifiers) {
  var classes = ['keyboard-overlay-key'];
  for (var i = 0; i < modifiers.length; ++i) {
    classes.push(MODIFIER_TO_CLASS[modifiers[i]]);
  }

  if ((identifier == '2A' && contains(modifiers, 'SHIFT')) ||
      (identifier == '1D' && contains(modifiers, 'CTRL')) ||
      (identifier == '38' && contains(modifiers, 'ALT'))) {
    classes.push('pressed');
    classes.push(IDENTIFIER_TO_CLASS[identifier]);
  }
  return classes;
}

/**
 * Returns true if a character is a ASCII character.
 */
function isAscii(c) {
  var charCode = c.charCodeAt(0);
  return 0x00 <= charCode && charCode <= 0x7F;
}

/**
 * Returns a remapped identiifer based on the preference.
 */
function remapIdentifier(identifier) {
  return identifierMap[identifier] || identifier;
}

/**
 * Returns a label of the key.
 */
function getKeyLabel(keyData, modifiers) {
  if (!keyData) {
    return '';
  }
  if (keyData.label) {
    return keyData.label;
  }
  var keyLabel = '';
  for (var j = 1; j <= 9; j++) {
    var pos =  keyData['p' + j];
    if (!pos) {
      continue;
    }
    keyLabel = hex2char(pos);
    if (!keyLabel) {
      continue;
     }
    if (isAscii(keyLabel) &&
        getShortcutData()[getAction(keyLabel, modifiers)]) {
      break;
    }
  }
  return keyLabel;
}

/**
 * Returns a normalized string used for a key of shortcutData.
 *
 * Examples:
 *   keycode: 'd', modifiers: ['CTRL', 'SHIFT'] => 'd<>CTRL<>SHIFT'
 *   keycode: 'alt', modifiers: ['ALT', 'SHIFT'] => 'ALT<>SHIFT'
 */
function getAction(keycode, modifiers) {
  const SEPARATOR = '<>';
  if (keycode.toUpperCase() in MODIFIER_TO_CLASS) {
    keycode = keycode.toUpperCase();
    if (keycode in modifiers) {
      return modifiers.join(SEPARATOR);
    } else {
      var action = [keycode].concat(modifiers)
      action.sort();
      return action.join(SEPARATOR);
    }
  }
  return [keycode].concat(modifiers).join(SEPARATOR);
}

/**
 * Returns a text which displayed on a key.
 */
function getKeyTextValue(keyData) {
  if (keyData.label) {
    // Do not show text on the space key.
    if (keyData.label == 'space') {
      return '';
    }
    return keyData.label;
  }

  var chars = [];
  for (var j = 1; j <= 9; ++j) {
    var pos = keyData['p' + j];
    if (pos && pos.length > 0) {
      chars.push(hex2char(pos));
    }
  }
  return chars.join(' ');
}

/**
 * Updates the whole keyboard.
 */
function update(modifiers) {
  var instructions = document.getElementById('instructions');
  if (modifiers.length == 0) {
    instructions.style.visibility = 'visible';
  } else {
    instructions.style.visibility = 'hidden';
  }

  var keyboardGlyphData = getKeyboardGlyphData();
  var shortcutData = getShortcutData();
  var layout = getLayouts()[keyboardGlyphData.layoutName];
  for (var i = 0; i < layout.length; ++i) {
    var identifier = remapIdentifier(layout[i][0]);
    var keyData = keyboardGlyphData.keys[identifier];
    var classes = getKeyClasses(identifier, modifiers, keyData);
    var keyLabel = getKeyLabel(keyData, modifiers);
    var shortcutId = shortcutData[getAction(keyLabel, modifiers)];
    if (modifiers.length == 1 && modifiers[0] == 'SHIFT' &&
        identifier == '2A') {
      // Currently there is no way to identify whether the left shift or the
      // right shift is preesed from the key event, so I assume the left shift
      // key is pressed here and do not show keyboard shortcut description for
      // 'Shift - Shift' (Toggle caps lock) on the left shift key, the
      // identifier of which is '2A'.
      // TODO(mazda): Remove this workaround (http://crosbug.com/18047)
      shortcutId = null;
    }
    if (shortcutId) {
      classes.push('is-shortcut');
    }

    var key = document.getElementById(keyId(identifier, i));
    key.className = classes.join(' ');

    if (!keyData) {
      continue;
    }

    var keyText = document.getElementById(keyTextId(identifier, i));
    var keyTextValue = getKeyTextValue(keyData);
    if (keyTextValue) {
       keyText.style.visibility = 'visible';
    } else {
       keyText.style.visibility = 'hidden';
    }
    keyText.textContent = keyTextValue;

    var shortcutText = document.getElementById(shortcutTextId(identifier, i));
    if (shortcutId) {
      shortcutText.style.visibility = 'visible';
      shortcutText.textContent = templateData[shortcutId];
    } else {
      shortcutText.style.visibility = 'hidden';
    }

    if (keyData.format) {
      var format = keyData.format;
      if (format == 'left' || format == 'right') {
        shortcutText.style.textAlign = format;
        keyText.style.textAlign = format;
      }
    }
  }
}

/**
 * A callback function for onkeydown and onkeyup events.
 */
function handleKeyEvent(e){
  var modifiers = getModifiers(e);
  if (!getKeyboardOverlayId()) {
    return;
  }
  update(modifiers);
  KeyboardOverlayAccessibilityHelper.maybeSpeakAllShortcuts(modifiers);
}

/**
 * Initializes the layout of the keys.
 */
function initLayout() {
  // Add data for the caps lock key
  var keys = getKeyboardGlyphData().keys;
  if (!('3A' in keys)) {
    keys['3A'] = {label: 'caps lock', format: 'left'};
  }
  // Add data for the special key representing a disabled key
  keys['DISABLED'] = {label: 'disabled', format: 'left'};

  var layout = getLayouts()[getKeyboardGlyphData().layoutName];
  var keyboard = document.body;
  var minX = window.innerWidth;
  var maxX = 0;
  var minY = window.innerHeight;
  var maxY = 0;
  var multiplier = 1.38 * window.innerWidth / BASE_KEYBOARD.width;
  var keyMargin = 7;
  var offsetX = 10;
  var offsetY = 7;
  for (var i = 0; i < layout.length; i++) {
    var array = layout[i];
    var identifier = remapIdentifier(array[0]);
    var x = Math.round((array[1] + offsetX) * multiplier);
    var y = Math.round((array[2] + offsetY) * multiplier);
    var w = Math.round((array[3] - keyMargin) * multiplier);
    var h = Math.round((array[4] - keyMargin) * multiplier);

    var key = document.createElement('div');
    key.id = keyId(identifier, i);
    key.className = 'keyboard-overlay-key';
    key.style.left = x + 'px';
    key.style.top = y + 'px';
    key.style.width = w + 'px';
    key.style.height = h + 'px';

    var keyText = document.createElement('div');
    keyText.id = keyTextId(identifier, i);
    keyText.className = 'keyboard-overlay-key-text';
    keyText.style.visibility = 'hidden';
    key.appendChild(keyText);

    var shortcutText = document.createElement('div');
    shortcutText.id = shortcutTextId(identifier, i);
    shortcutText.className = 'keyboard-overlay-shortcut-text';
    shortcutText.style.visilibity = 'hidden';
    key.appendChild(shortcutText);
    keyboard.appendChild(key);

    minX = Math.min(minX, x);
    maxX = Math.max(maxX, x + w);
    minY = Math.min(minY, y);
    maxY = Math.max(maxY, y + h);
  }

  var width = maxX - minX + 1;
  var height = maxY - minY + 1;
  keyboard.style.width = (width + 2 * (minX + 1)) + 'px';
  keyboard.style.height = (height + 2 * (minY + 1)) + 'px';

  var instructions = document.createElement('div');
  instructions.id = 'instructions';
  instructions.className = 'keyboard-overlay-instructions';
  instructions.style.left = ((BASE_INSTRUCTIONS.left - BASE_KEYBOARD.left) *
                             width / BASE_KEYBOARD.width + minX) + 'px';
  instructions.style.top = ((BASE_INSTRUCTIONS.top - BASE_KEYBOARD.top) *
                            height / BASE_KEYBOARD.height + minY) + 'px';
  instructions.style.width = (width * BASE_INSTRUCTIONS.width /
                              BASE_KEYBOARD.width) + 'px';
  instructions.style.height = (height * BASE_INSTRUCTIONS.height /
                               BASE_KEYBOARD.height) + 'px';

  var instructionsText = document.createElement('div');
  instructionsText.id = 'instructions-text';
  instructionsText.className = 'keyboard-overlay-instructions-text';
  instructionsText.innerHTML = templateData.keyboardOverlayInstructions;
  instructions.appendChild(instructionsText);
  var instructionsHideText = document.createElement('div');
  instructionsHideText.id = 'instructions-hide-text';
  instructionsHideText.className = 'keyboard-overlay-instructions-hide-text';
  instructionsHideText.innerHTML = templateData.keyboardOverlayInstructionsHide;
  instructions.appendChild(instructionsHideText);
  keyboard.appendChild(instructions);
}

/**
 * A callback function for the onload event of the body element.
 */
function init() {
  document.addEventListener('keydown', handleKeyEvent);
  document.addEventListener('keyup', handleKeyEvent);
  chrome.send('getLabelMap');
}

/**
 * Initializes the global map for remapping identifiers of modifier keys based
 * on the preference.
 * Called after sending the 'getLabelMap' message.
 */
function initIdentifierMap(remap) {
  for (var key in remap) {
    var val = remap[key];
    if ((key in LABEL_TO_IDENTIFIER) &&
        (val in LABEL_TO_IDENTIFIER)) {
      identifierMap[LABEL_TO_IDENTIFIER[key]] =
          LABEL_TO_IDENTIFIER[val];
    } else {
      console.error('Invalid label map element: ' + key + ', ' + val);
    }
  }
  chrome.send('getInputMethodId');
}

/**
 * Initializes the global keyboad overlay ID and the layout of keys.
 * Called after sending the 'getInputMethodId' message.
 */
function initKeyboardOverlayId(inputMethodId) {
  // Libcros returns an empty string when it cannot find the keyboard overlay ID
  // corresponding to the current input method.
  // In such a case, fallback to the default ID (en_US).
  var inputMethodIdToOverlayId = keyboardOverlayData['inputMethodIdToOverlayId']
  if (inputMethodId) {
    keyboardOverlayId = inputMethodIdToOverlayId[inputMethodId];
  }
  if (!keyboardOverlayId) {
    console.error('No keyboard overlay ID for ' + inputMethodId);
    keyboardOverlayId = 'en_US';
  }
  while(document.body.firstChild) {
    document.body.removeChild(document.body.firstChild);
  }
  initLayout();
  update([]);
}

document.addEventListener('DOMContentLoaded', init);
