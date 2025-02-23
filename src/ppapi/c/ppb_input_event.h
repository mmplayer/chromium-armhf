/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_input_event.idl modified Wed Sep 21 12:32:06 2011. */

#ifndef PPAPI_C_PPB_INPUT_EVENT_H_
#define PPAPI_C_PPB_INPUT_EVENT_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/c/pp_var.h"

#define PPB_INPUT_EVENT_INTERFACE_1_0 "PPB_InputEvent;1.0"
#define PPB_INPUT_EVENT_INTERFACE PPB_INPUT_EVENT_INTERFACE_1_0

#define PPB_MOUSE_INPUT_EVENT_INTERFACE_1_0 "PPB_MouseInputEvent;1.0"
#define PPB_MOUSE_INPUT_EVENT_INTERFACE_1_1 "PPB_MouseInputEvent;1.1"
#define PPB_MOUSE_INPUT_EVENT_INTERFACE PPB_MOUSE_INPUT_EVENT_INTERFACE_1_1

#define PPB_WHEEL_INPUT_EVENT_INTERFACE_1_0 "PPB_WheelInputEvent;1.0"
#define PPB_WHEEL_INPUT_EVENT_INTERFACE PPB_WHEEL_INPUT_EVENT_INTERFACE_1_0

#define PPB_KEYBOARD_INPUT_EVENT_INTERFACE_1_0 "PPB_KeyboardInputEvent;1.0"
#define PPB_KEYBOARD_INPUT_EVENT_INTERFACE \
    PPB_KEYBOARD_INPUT_EVENT_INTERFACE_1_0

/**
 * @file
 * This file defines the Input Event interfaces.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * This enumeration contains the types of input events.
 */
typedef enum {
  PP_INPUTEVENT_TYPE_UNDEFINED = -1,
  /**
   * Notification that a mouse button was pressed.
   *
   * Register for this event using the PP_INPUTEVENT_CLASS_MOUSE class.
   */
  PP_INPUTEVENT_TYPE_MOUSEDOWN = 0,
  /**
   * Notification that a mouse button was released.
   *
   * Register for this event using the PP_INPUTEVENT_CLASS_MOUSE class.
   */
  PP_INPUTEVENT_TYPE_MOUSEUP = 1,
  /**
   * Notification that a mouse button was moved when it is over the instance
   * or dragged out of it.
   *
   * Register for this event using the PP_INPUTEVENT_CLASS_MOUSE class.
   */
  PP_INPUTEVENT_TYPE_MOUSEMOVE = 2,
  /**
   * Notification that the mouse entered the instance's bounds.
   *
   * Register for this event using the PP_INPUTEVENT_CLASS_MOUSE class.
   */
  PP_INPUTEVENT_TYPE_MOUSEENTER = 3,
  /**
   * Notification that a mouse left the instance's bounds.
   *
   * Register for this event using the PP_INPUTEVENT_CLASS_MOUSE class.
   */
  PP_INPUTEVENT_TYPE_MOUSELEAVE = 4,
  /**
   * Notification that the scroll wheel was used.
   *
   * Register for this event using the PP_INPUTEVENT_CLASS_WHEEL class.
   */
  PP_INPUTEVENT_TYPE_WHEEL = 5,
  /**
   * Notification that a key transitioned from "up" to "down".
   * TODO(brettw) differentiate from KEYDOWN.
   *
   * Register for this event using the PP_INPUTEVENT_CLASS_KEYBOARD class.
   */
  PP_INPUTEVENT_TYPE_RAWKEYDOWN = 6,
  /**
   * Notification that a key was pressed. This does not necessarily correspond
   * to a character depending on the key and language. Use the
   * PP_INPUTEVENT_TYPE_CHAR for character input.
   *
   * Register for this event using the PP_INPUTEVENT_CLASS_KEYBOARD class.
   */
  PP_INPUTEVENT_TYPE_KEYDOWN = 7,
  /**
   * Notification that a key was released.
   *
   * Register for this event using the PP_INPUTEVENT_CLASS_KEYBOARD class.
   */
  PP_INPUTEVENT_TYPE_KEYUP = 8,
  /**
   * Notification that a character was typed. Use this for text input. Key
   * down events may generate 0, 1, or more than one character event depending
   * on the key, locale, and operating system.
   *
   * Register for this event using the PP_INPUTEVENT_CLASS_KEYBOARD class.
   */
  PP_INPUTEVENT_TYPE_CHAR = 9,
  /**
   * TODO(brettw) when is this used?
   *
   * Register for this event using the PP_INPUTEVENT_CLASS_MOUSE class.
   */
  PP_INPUTEVENT_TYPE_CONTEXTMENU = 10,
  /**
   * Notification that an input method composition process has just started.
   *
   * Register for this event using the PP_INPUTEVENT_CLASS_IME class.
   */
  PP_INPUTEVENT_TYPE_IME_COMPOSITION_START = 11,
  /**
   * Notification that the input method composition string is updated.
   *
   * Register for this event using the PP_INPUTEVENT_CLASS_IME class.
   */
  PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE = 12,
  /**
   * Notification that an input method composition process has completed.
   *
   * Register for this event using the PP_INPUTEVENT_CLASS_IME class.
   */
  PP_INPUTEVENT_TYPE_IME_COMPOSITION_END = 13,
  /**
   * Notification that an input method committed a string.
   *
   * Register for this event using the PP_INPUTEVENT_CLASS_IME class.
   */
  PP_INPUTEVENT_TYPE_IME_TEXT = 14
} PP_InputEvent_Type;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_InputEvent_Type, 4);

/**
 * This enumeration contains event modifier constants. Each modifier is one
 * bit. Retrieve the modifiers from an input event using the GetEventModifiers
 * function on PPB_InputEvent.
 */
typedef enum {
  PP_INPUTEVENT_MODIFIER_SHIFTKEY = 1 << 0,
  PP_INPUTEVENT_MODIFIER_CONTROLKEY = 1 << 1,
  PP_INPUTEVENT_MODIFIER_ALTKEY = 1 << 2,
  PP_INPUTEVENT_MODIFIER_METAKEY = 1 << 3,
  PP_INPUTEVENT_MODIFIER_ISKEYPAD = 1 << 4,
  PP_INPUTEVENT_MODIFIER_ISAUTOREPEAT = 1 << 5,
  PP_INPUTEVENT_MODIFIER_LEFTBUTTONDOWN = 1 << 6,
  PP_INPUTEVENT_MODIFIER_MIDDLEBUTTONDOWN = 1 << 7,
  PP_INPUTEVENT_MODIFIER_RIGHTBUTTONDOWN = 1 << 8,
  PP_INPUTEVENT_MODIFIER_CAPSLOCKKEY = 1 << 9,
  PP_INPUTEVENT_MODIFIER_NUMLOCKKEY = 1 << 10
} PP_InputEvent_Modifier;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_InputEvent_Modifier, 4);

/**
 * This enumeration contains constants representing each mouse button. To get
 * the mouse button for a mouse down or up event, use GetMouseButton on
 * PPB_InputEvent.
 */
typedef enum {
  PP_INPUTEVENT_MOUSEBUTTON_NONE = -1,
  PP_INPUTEVENT_MOUSEBUTTON_LEFT = 0,
  PP_INPUTEVENT_MOUSEBUTTON_MIDDLE = 1,
  PP_INPUTEVENT_MOUSEBUTTON_RIGHT = 2
} PP_InputEvent_MouseButton;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_InputEvent_MouseButton, 4);

typedef enum {
  /**
   * Request mouse input events.
   *
   * Normally you will request mouse events by calling RequestInputEvents().
   * The only use case for filtered events (via RequestFilteringInputEvents())
   * is for instances that have irregular outlines and you want to perform hit
   * testing, which is very uncommon. Requesting non-filtered mouse events will
   * lead to higher performance.
   */
  PP_INPUTEVENT_CLASS_MOUSE = 1 << 0,
  /**
   * Requests keyboard events. Often you will want to request filtered mode
   * (via RequestFilteringInputEvents) for keyboard events so you can pass on
   * events (by returning false) that you don't handle. For example, if you
   * don't request filtered mode and the user pressed "Page Down" when your
   * instance has focus, the page won't scroll which will be a poor experience.
   *
   * A small number of tab and window management commands like Alt-F4 are never
   * sent to the page. You can not request these keyboard commands since it
   * would allow pages to trap users on a page.
   */
  PP_INPUTEVENT_CLASS_KEYBOARD = 1 << 1,
  /**
   * Identifies scroll wheel input event. Wheel events must be requested in
   * filtering mode via RequestFilteringInputEvents(). This is because many
   * wheel commands should be forwarded to the page.
   *
   * Most instances will not need this event. Consuming wheel events by
   * returning true from your filtered event handler will prevent the user from
   * scrolling the page when the mouse is over the instance which can be very
   * annoying.
   *
   * If you handle wheel events (for example, you have a document viewer which
   * the user can scroll), the recommended behavior is to return false only if
   * the wheel event actually causes your document to scroll. When the user
   * reaches the end of the document, return false to indicating that the event
   * was not handled. This will then forward the event to the containing page
   * for scrolling, producing the nested scrolling behavior users expect from
   * frames in a page.
   */
  PP_INPUTEVENT_CLASS_WHEEL = 1 << 2,
  /**
   * Identifies touch input events.
   *
   * Request touch events only if you intend to handle them. If the browser
   * knows you do not need to handle touch events, it can handle them at a
   * higher level and achieve higher performance.
   */
  PP_INPUTEVENT_CLASS_TOUCH = 1 << 3,
  /**
   * Identifies IME composition input events.
   *
   * Request this input event class if you allow on-the-spot IME input.
   */
  PP_INPUTEVENT_CLASS_IME = 1 << 4
} PP_InputEvent_Class;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_InputEvent_Class, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_InputEvent</code> interface contains pointers to several
 * functions related to generic input events on the browser.
 */
struct PPB_InputEvent {
  /**
   * RequestInputEvent() requests that input events corresponding to the given
   * input events are delivered to the instance.
   *
   * It's recommended that you use RequestFilteringInputEvents() for keyboard
   * events instead of this function so that you don't interfere with normal
   * browser accelerators.
   *
   * By default, no input events are delivered. Call this function with the
   * classes of events you are interested in to have them be delivered to
   * the instance. Calling this function will override any previous setting for
   * each specified class of input events (for example, if you previously
   * called RequestFilteringInputEvents(), this function will set those events
   * to non-filtering mode).
   *
   * Input events may have high overhead, so you should only request input
   * events that your plugin will actually handle. For example, the browser may
   * do optimizations for scroll or touch events that can be processed
   * substantially faster if it knows there are no non-default receivers for
   * that message. Requesting that such messages be delivered, even if they are
   * processed very quickly, may have a noticeable effect on the performance of
   * the page.
   *
   * When requesting input events through this function, the events will be
   * delivered and <i>not</i> bubbled to the page. This means that even if you
   * aren't interested in the message, no other parts of the page will get
   * a crack at the message.
   *
   * <strong>Example:</strong>
   * <code>
   *   RequestInputEvents(instance, PP_INPUTEVENT_CLASS_MOUSE);
   *   RequestFilteringInputEvents(instance,
   *       PP_INPUTEVENT_CLASS_WHEEL | PP_INPUTEVENT_CLASS_KEYBOARD);
   * </code>
   *
   * @param instance The <code>PP_Instance</code> of the instance requesting
   * the given events.
   *
   * @param event_classes A combination of flags from
   * <code>PP_InputEvent_Class</code> that identifies the classes of events the
   * instance is requesting. The flags are combined by logically ORing their
   * values.
   *
   * @return <code>PP_OK</code> if the operation succeeded,
   * <code>PP_ERROR_BADARGUMENT</code> if instance is invalid, or
   * <code>PP_ERROR_NOTSUPPORTED</code> if one of the event class bits were
   * illegal. In the case of an invalid bit, all valid bits will be applied
   * and only the illegal bits will be ignored. The most common cause of a
   * <code>PP_ERROR_NOTSUPPORTED</code> return value is requesting keyboard
   * events, these must use RequestFilteringInputEvents().
   */
  int32_t (*RequestInputEvents)(PP_Instance instance, uint32_t event_classes);
  /**
   * RequestFilteringInputEvents() requests that input events corresponding to
   * the given input events are delivered to the instance for filtering.
   *
   * By default, no input events are delivered. In most cases you would
   * register to receive events by calling RequestInputEvents(). In some cases,
   * however, you may wish to filter events such that they can be bubbled up
   * to the DOM. In this case, register for those classes of events using
   * this function instead of RequestInputEvents().
   *
   * Filtering input events requires significantly more overhead than just
   * delivering them to the instance. As such, you should only request
   * filtering in those cases where it's absolutely necessary. The reason is
   * that it requires the browser to stop and block for the instance to handle
   * the input event, rather than sending the input event asynchronously. This
   * can have significant overhead.
   *
   * <strong>Example:</strong>
   * <code>
   *   RequestInputEvents(instance, PP_INPUTEVENT_CLASS_MOUSE);
   *   RequestFilteringInputEvents(instance,
   *       PP_INPUTEVENT_CLASS_WHEEL | PP_INPUTEVENT_CLASS_KEYBOARD);
   * </code>
   *
   * @return <code>PP_OK</code> if the operation succeeded,
   * <code>PP_ERROR_BADARGUMENT</code> if instance is invalid, or
   * <code>PP_ERROR_NOTSUPPORTED</code> if one of the event class bits were
   * illegal. In the case of an invalid bit, all valid bits will be applied
   * and only the illegal bits will be ignored.
   */
  int32_t (*RequestFilteringInputEvents)(PP_Instance instance,
                                         uint32_t event_classes);
  /**
   * ClearInputEventRequest() requests that input events corresponding to the
   * given input classes no longer be delivered to the instance.
   *
   * By default, no input events are delivered. If you have previously
   * requested input events via RequestInputEvents() or
   * RequestFilteringInputEvents(), this function will unregister handling
   * for the given instance. This will allow greater browser performance for
   * those events.
   *
   * Note that you may still get some input events after clearing the flag if
   * they were dispatched before the request was cleared. For example, if
   * there are 3 mouse move events waiting to be delivered, and you clear the
   * mouse event class during the processing of the first one, you'll still
   * receive the next two. You just won't get more events generated.
   *
   * @param instance The <code>PP_Instance</code> of the instance requesting
   * to no longer receive the given events.
   *
   * @param event_classes A combination of flags from
   * <code>PP_InputEvent_Class</code> that identify the classes of events the
   * instance is no longer interested in.
   */
  void (*ClearInputEventRequest)(PP_Instance instance, uint32_t event_classes);
  /**
   * IsInputEvent() returns true if the given resource is a valid input event
   * resource.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to a generic
   * resource.
   *
   * @return <code>PP_TRUE</code> if the given resource is a valid input event
   * resource.
   */
  PP_Bool (*IsInputEvent)(PP_Resource resource);
  /**
   * GetType() returns the type of input event for the given input event
   * resource.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to an input
   * event.
   *
   * @return A <code>PP_InputEvent_Type</code> if its a valid input event or
   * <code>PP_INPUTEVENT_TYPE_UNDEFINED</code> if the resource is invalid.
   */
  PP_InputEvent_Type (*GetType)(PP_Resource event);
  /**
   * GetTimeStamp() Returns the time that the event was generated. This will be
   *  before the current time since processing and dispatching the event has
   * some overhead. Use this value to compare the times the user generated two
   * events without being sensitive to variable processing time.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to the event.
   *
   * @return The return value is in time ticks, which is a monotonically
   * increasing clock not related to the wall clock time. It will not change
   * if the user changes their clock or daylight savings time starts, so can
   * be reliably used to compare events. This means, however, that you can't
   * correlate event times to a particular time of day on the system clock.
   */
  PP_TimeTicks (*GetTimeStamp)(PP_Resource event);
  /**
   * GetModifiers() returns a bitfield indicating which modifiers were down
   * at the time of the event. This is a combination of the flags in the
   * <code>PP_InputEvent_Modifier</code> enum.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to an input
   * event.
   *
   * @return The modifiers associated with the event, or 0 if the given
   * resource is not a valid event resource.
   */
  uint32_t (*GetModifiers)(PP_Resource event);
};

/**
 * The <code>PPB_MouseInputEvent</code> interface contains pointers to several
 * functions related to mouse input events.
 */
struct PPB_MouseInputEvent {
  /**
   * Create() creates a mouse input event with the given parameters. Normally
   * you will get a mouse event passed through the
   * <code>HandleInputEvent</code> and will not need to create them, but some
   * applications may want to create their own for internal use. The type must
   * be one of the mouse event types.
   *
   * @param[in] instance The instance for which this event occurred.
   *
   * @param[in] type A <code>PP_InputEvent_Type</code> identifying the type of
   * input event.
   *
   * @param[in] time_stamp A <code>PP_TimeTicks</code> indicating the time
   * when the event occurred.
   *
   * @param[in] modifiers A bit field combination of the
   * <code>PP_InputEvent_Modifier</code> flags.
   *
   * @param[in] mouse_button The button that changed for mouse down or up
   * events. This value will be <code>PP_EVENT_MOUSEBUTTON_NONE</code> for
   * mouse move, enter, and leave events.
   *
   * @param[in] mouse_position A <code>Point</code> containing the x and y
   * position of the mouse when the event occurred.
   *
   * @param[in] mouse_movement The change in position of the mouse.
   *
   * @return A <code>PP_Resource</code> containing the new mouse input event.
   */
  PP_Resource (*Create)(PP_Instance instance,
                        PP_InputEvent_Type type,
                        PP_TimeTicks time_stamp,
                        uint32_t modifiers,
                        PP_InputEvent_MouseButton mouse_button,
                        const struct PP_Point* mouse_position,
                        int32_t click_count,
                        const struct PP_Point* mouse_movement);
  /**
   * IsMouseInputEvent() determines if a resource is a mouse event.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to an event.
   *
   * @return <code>PP_TRUE</code> if the given resource is a valid mouse input
   * event, otherwise <code>PP_FALSE</code>.
   */
  PP_Bool (*IsMouseInputEvent)(PP_Resource resource);
  /**
   * GetButton() returns the mouse button that generated a mouse down or up
   * event.
   *
   * @param[in] mouse_event A <code>PP_Resource</code> corresponding to a
   * mouse event.
   *
   * @return The mouse button associated with mouse down and up events. This
   * value will be <code>PP_EVENT_MOUSEBUTTON_NONE</code> for mouse move,
   * enter, and leave events, and for all non-mouse events.
   */
  PP_InputEvent_MouseButton (*GetButton)(PP_Resource mouse_event);
  /**
   * GetPosition() returns the pixel location of a mouse input event. When
   * the mouse is locked, it returns the last known mouse position just as
   * mouse lock was entered.
   *
   * @param[in] mouse_event A <code>PP_Resource</code> corresponding to a
   * mouse event.
   *
   * @return The point associated with the mouse event, relative to the upper-
   * left of the instance receiving the event. These values can be negative for
   * mouse drags. The return value will be (0, 0) for non-mouse events.
   */
  struct PP_Point (*GetPosition)(PP_Resource mouse_event);
  /**
   * TODO(brettw) figure out exactly what this means.
   */
  int32_t (*GetClickCount)(PP_Resource mouse_event);
  /**
   * Returns the change in position of the mouse. When the mouse is locked,
   * although the mouse position doesn't actually change, this function
   * still provides movement information, which indicates what the change in
   * position would be had the mouse not been locked.
   *
   * @param[in] mouse_event A <code>PP_Resource</code> corresponding to a
   * mouse event.
   *
   * @return The change in position of the mouse, relative to the previous
   * position.
   *
   * TODO(yzshen): This feature hasn't been supported yet. The returned value is
   * always (0, 0) for system-generated mouse events (which are passed through
   * the <code>HandleInputEvent</code>).
   */
  struct PP_Point (*GetMovement)(PP_Resource mouse_event);
};

struct PPB_MouseInputEvent_1_0 {
  PP_Resource (*Create)(PP_Instance instance,
                        PP_InputEvent_Type type,
                        PP_TimeTicks time_stamp,
                        uint32_t modifiers,
                        PP_InputEvent_MouseButton mouse_button,
                        const struct PP_Point* mouse_position,
                        int32_t click_count);
  PP_Bool (*IsMouseInputEvent)(PP_Resource resource);
  PP_InputEvent_MouseButton (*GetButton)(PP_Resource mouse_event);
  struct PP_Point (*GetPosition)(PP_Resource mouse_event);
  int32_t (*GetClickCount)(PP_Resource mouse_event);
};

/**
 * The <code>PPB_WheelIputEvent</code> interface contains pointers to several
 * functions related to wheel input events.
 */
struct PPB_WheelInputEvent {
  /**
   * Create() creates a wheel input event with the given parameters. Normally
   * you will get a wheel event passed through the
   * <code>HandleInputEvent</code> and will not need to create them, but some
   * applications may want to create their own for internal use.
   *
   * @param[in] instance The instance for which this event occurred.
   *
   * @param[in] time_stamp A <code>PP_TimeTicks</code> indicating the time
   * when the event occurred.
   *
   * @param[in] modifiers A bit field combination of the
   * <code>PP_InputEvent_Modifier</code> flags.
   *
   * @param[in] wheel_delta The scroll wheel's horizontal and vertical scroll
   * amounts.
   *
   * @param[in] wheel_ticks The number of "clicks" of the scroll wheel that
   * have produced the event.
   *
   * @param[in] scroll_by_page When true, the user is requesting to scroll
   * by pages. When false, the user is requesting to scroll by lines.
   *
   * @return A <code>PP_Resource</code> containing the new wheel input event.
   */
  PP_Resource (*Create)(PP_Instance instance,
                        PP_TimeTicks time_stamp,
                        uint32_t modifiers,
                        const struct PP_FloatPoint* wheel_delta,
                        const struct PP_FloatPoint* wheel_ticks,
                        PP_Bool scroll_by_page);
  /**
   * IsWheelInputEvent() determines if a resource is a wheel event.
   *
   * @param[in] wheel_event A <code>PP_Resource</code> corresponding to an
   * event.
   *
   * @return <code>PP_TRUE</code> if the given resource is a valid wheel input
   * event.
   */
  PP_Bool (*IsWheelInputEvent)(PP_Resource resource);
  /**
   * GetDelta() returns the amount vertically and horizontally the user has
   * requested to scroll by with their mouse wheel. A scroll down or to the
   * right (where the content moves up or left) is represented as positive
   * values, and a scroll up or to the left (where the content moves down or
   * right) is represented as negative values.
   *
   * This amount is system dependent and will take into account the user's
   * preferred scroll sensitivity and potentially also nonlinear acceleration
   * based on the speed of the scrolling.
   *
   * Devices will be of varying resolution. Some mice with large detents will
   * only generate integer scroll amounts. But fractional values are also
   * possible, for example, on some trackpads and newer mice that don't have
   * "clicks".
   *
   * @param[in] wheel_event A <code>PP_Resource</code> corresponding to a wheel
   * event.
   *
   * @return The vertical and horizontal scroll values. The units are either in
   * pixels (when scroll_by_page is false) or pages (when scroll_by_page is
   * true). For example, y = -3 means scroll up 3 pixels when scroll_by_page
   * is false, and scroll up 3 pages when scroll_by_page is true.
   */
  struct PP_FloatPoint (*GetDelta)(PP_Resource wheel_event);
  /**
   * GetTicks() returns the number of "clicks" of the scroll wheel
   * that have produced the event. The value may have system-specific
   * acceleration applied to it, depending on the device. The positive and
   * negative meanings are the same as for GetDelta().
   *
   * If you are scrolling, you probably want to use the delta values.  These
   * tick events can be useful if you aren't doing actual scrolling and don't
   * want or pixel values. An example may be cycling between different items in
   * a game.
   *
   * @param[in] wheel_event A <code>PP_Resource</code> corresponding to a wheel
   * event.
   *
   * @return The number of "clicks" of the scroll wheel. You may receive
   * fractional values for the wheel ticks if the mouse wheel is high
   * resolution or doesn't have "clicks". If your program wants discrete
   * events (as in the "picking items" example) you should accumulate
   * fractional click values from multiple messages until the total value
   * reaches positive or negative one. This should represent a similar amount
   * of scrolling as for a mouse that has a discrete mouse wheel.
   */
  struct PP_FloatPoint (*GetTicks)(PP_Resource wheel_event);
  /**
   * GetScrollByPage() indicates if the scroll delta x/y indicates pages or
   * lines to scroll by.
   *
   * @param[in] wheel_event A <code>PP_Resource</code> corresponding to a wheel
   * event.
   *
   * @return <code>PP_TRUE</code> if the event is a wheel event and the user is
   * scrolling by pages. <code>PP_FALSE</code> if not or if the resource is not
   * a wheel event.
   */
  PP_Bool (*GetScrollByPage)(PP_Resource wheel_event);
};

/**
 * The <code>PPB_KeyboardInputEvent</code> interface contains pointers to
 * several functions related to keyboard input events.
 */
struct PPB_KeyboardInputEvent {
  /**
   * Creates a keyboard input event with the given parameters. Normally you
   * will get a keyboard event passed through the HandleInputEvent and will not
   * need to create them, but some applications may want to create their own
   * for internal use. The type must be one of the keyboard event types.
   *
   * @param[in] instance The instance for which this event occurred.
   *
   * @param[in] type A <code>PP_InputEvent_Type</code> identifying the type of
   * input event.
   *
   * @param[in] time_stamp A <code>PP_TimeTicks</code> indicating the time
   * when the event occurred.
   *
   * @param[in]  modifiers A bit field combination of the
   * <code>PP_InputEvent_Modifier</code> flags.
   *
   * @param[in] key_code This value reflects the DOM KeyboardEvent
   * <code>keyCode</code> field. Chrome populates this with the Windows-style
   * Virtual Key code of the key.
   *
   * @param[in] character_text This value represents the typed character as a
   * UTF-8 string.
   *
   * @return A <code>PP_Resource</code> containing the new keyboard input
   * event.
   */
  PP_Resource (*Create)(PP_Instance instance,
                        PP_InputEvent_Type type,
                        PP_TimeTicks time_stamp,
                        uint32_t modifiers,
                        uint32_t key_code,
                        struct PP_Var character_text);
  /**
   * IsKeyboardInputEvent() determines if a resource is a keyboard event.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to an event.
   *
   * @return <code>PP_TRUE</code> if the given resource is a valid input event.
   */
  PP_Bool (*IsKeyboardInputEvent)(PP_Resource resource);
  /**
   * GetKeyCode() returns the DOM keyCode field for the keyboard event.
   * Chrome populates this with the Windows-style Virtual Key code of the key.
   *
   * @param[in] key_event A <code>PP_Resource</code> corresponding to a
   * keyboard event.
   *
   * @return The DOM keyCode field for the keyboard event.
   */
  uint32_t (*GetKeyCode)(PP_Resource key_event);
  /**
   * GetCharacterText() returns the typed character as a UTF-8 string for the
   * given character event.
   *
   * @param[in] character_event A <code>PP_Resource</code> corresponding to a
   * keyboard event.
   *
   * @return A string var representing a single typed character for character
   * input events. For non-character input events the return value will be an
   * undefined var.
   */
  struct PP_Var (*GetCharacterText)(PP_Resource character_event);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_INPUT_EVENT_H_ */

