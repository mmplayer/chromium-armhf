/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From pp_var.idl modified Tue Jul 12 15:26:30 2011. */

#ifndef PPAPI_C_PP_VAR_H_
#define PPAPI_C_PP_VAR_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

/**
 * @file
 * This file defines the API for handling the passing of data types between
 * your module and the page.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * The <code>PP_VarType</code> is an enumeration of the different types that
 * can be contained within a <code>PP_Var</code> structure.
 */
typedef enum {
  /**
   * An undefined value.
   */
  PP_VARTYPE_UNDEFINED,
  /**
   * A NULL value. This is similar to undefined, but JavaScript differentiates
   * the two so it is exposed here as well.
   */
  PP_VARTYPE_NULL,
  /**
   * A boolean value, use the <code>as_bool</code> member of the var.
   */
  PP_VARTYPE_BOOL,
  /**
   * A 32-bit integer value. Use the <code>as_int</code> member of the var.
   */
  PP_VARTYPE_INT32,
  /**
   * A double-precision floating point value. Use the <code>as_double</code>
   * member of the var.
   */
  PP_VARTYPE_DOUBLE,
  /**
   * The Var represents a string. The <code>as_id</code> field is used to
   * identify the string, which may be created and retrieved from the
   * <code>PPB_Var</code> interface.
   */
  PP_VARTYPE_STRING,
  /**
   * Represents a JavaScript object. This vartype is not currently usable
   * from modules, although it is used internally for some tasks.
   */
  PP_VARTYPE_OBJECT,
  /**
   * Arrays and dictionaries are not currently supported but will be added
   * in future revisions. These objects are reference counted so be sure
   * to properly AddRef/Release them as you would with strings to ensure your
   * module will continue to work with future versions of the API.
   */
  PP_VARTYPE_ARRAY,
  PP_VARTYPE_DICTIONARY
} PP_VarType;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_VarType, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
/**
 * The PP_VarValue union stores the data for any one of the types listed
 * in the PP_VarType enum.
 */
union PP_VarValue {
  /**
   * If <code>type</code> is <code>PP_VARTYPE_BOOL</code>,
   * <code>as_bool</code> represents the value of this <code>PP_Var</code> as
   * <code>PP_Bool</code>.
   */
  PP_Bool as_bool;
  /**
   * If <code>type</code> is <code>PP_VARTYPE_INT32</code>,
   * <code>as_int</code> represents the value of this <code>PP_Var</code> as
   * <code>int32_t</code>.
   */
  int32_t as_int;
  /**
   * If <code>type</code> is <code>PP_VARTYPE_DOUBLE</code>,
   * <code>as_double</code> represents the value of this <code>PP_Var</code>
   * as <code>double</code>.
   */
  double as_double;
  /**
   * If <code>type</code> is <code>PP_VARTYPE_STRING</code>,
   * <code>PP_VARTYPE_OBJECT</code>, <code>PP_VARTYPE_ARRAY</code>, or
   * <code>PP_VARTYPE_DICTIONARY</code>,
   * <code>as_id</code> represents the value of this <code>PP_Var</code> as
   * an opaque handle assigned by the browser. This handle is guaranteed
   * never to be 0, so a module can initialize this ID to 0 to indicate a
   * "NULL handle."
   */
  int64_t as_id;
};

/**
 * The <code>PP_VAR</code> struct is a variant data type and can contain any
 * value of one of the types named in the <code>PP_VarType</code> enum. This
 * structure is for passing data between native code which can be strongly
 * typed and the browser (JavaScript) which isn't strongly typed.
 *
 * JavaScript has a "number" type for holding a number, and does not
 * differentiate between floating point and integer numbers. The
 * JavaScript operations will try to optimize operations by using
 * integers when possible, but could end up with doubles. Therefore,
 * you can't assume a numeric <code>PP_Var</code> will be the type you expect.
 * Your code should be capable of handling either int32_t or double for numeric
 * PP_Vars sent from JavaScript.
 */
struct PP_Var {
  PP_VarType type;
  /**
   * The <code>padding</code> ensures <code>value</code> is aligned on an
   * 8-byte boundary relative to the start of the struct. Some compilers
   * align doubles on 8-byte boundaries for 32-bit x86, and some align on
   * 4-byte boundaries.
   */
  int32_t padding;
  /**
   * This <code>value</code> represents the contents of the PP_Var. Only one of
   * the fields of <code>value</code> is valid at a time based upon
   * <code>type</code>.
   */
  union PP_VarValue value;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_Var, 16);
/**
 * @}
 */

/**
 * @addtogroup Functions
 * @{
 */

/**
 * PP_MakeUndefined() is used to wrap an undefined value into a
 * <code>PP_Var</code> struct for passing to the browser.
 *
 * @return A <code>PP_Var</code> structure.
 */
PP_INLINE struct PP_Var PP_MakeUndefined() {
  struct PP_Var result = { PP_VARTYPE_UNDEFINED, 0, {PP_FALSE} };
  return result;
}

/**
 * PP_MakeNull() is used to wrap a null value into a
 * <code>PP_Var</code> struct for passing to the browser.
 *
 * @return A <code>PP_Var</code> structure,
 */
PP_INLINE struct PP_Var PP_MakeNull() {
  struct PP_Var result = { PP_VARTYPE_NULL, 0, {PP_FALSE} };
  return result;
}

/**
 * PP_MakeBool() is used to wrap a boolean value into a
 * <code>PP_Var</code> struct for passing to the browser.
 *
 * @param[in] value A <code>PP_Bool</code> enumeration to
 * wrap.
 *
 * @return A <code>PP_Var</code> structure.
 */
PP_INLINE struct PP_Var PP_MakeBool(PP_Bool value) {
  struct PP_Var result = { PP_VARTYPE_BOOL, 0, {PP_FALSE} };
  result.value.as_bool = value;
  return result;
}

/**
 * PP_MakeInt32() is used to wrap a 32 bit integer value
 * into a <code>PP_Var</code> struct for passing to the browser.
 *
 * @param[in] value An int32 to wrap.
 *
 * @return A <code>PP_Var</code> structure.
 */
PP_INLINE struct PP_Var PP_MakeInt32(int32_t value) {
  struct PP_Var result = { PP_VARTYPE_INT32, 0, {PP_FALSE} };
  result.value.as_int = value;
  return result;
}

/**
 * PP_MakeDouble() is used to wrap a double value into a
 * <code>PP_Var</code> struct for passing to the browser.
 *
 * @param[in] value A double to wrap.
 *
 * @return A <code>PP_Var</code> structure.
 */
PP_INLINE struct PP_Var PP_MakeDouble(double value) {
  struct PP_Var result = { PP_VARTYPE_DOUBLE, 0, {PP_FALSE} };
  result.value.as_double = value;
  return result;
}
/**
 * @}
 */

#endif  /* PPAPI_C_PP_VAR_H_ */

