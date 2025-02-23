// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"

#include "base/float_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"

namespace {

const wchar_t kNullString[] = L"null";
const wchar_t kTrueString[] = L"true";
const wchar_t kFalseString[] = L"false";

const int kStackLimit = 100;

// A helper method for ParseNumberToken.  It reads an int from the end of
// token.  The method returns false if there is no valid integer at the end of
// the token.
bool ReadInt(base::JSONReader::Token& token, bool can_have_leading_zeros) {
  wchar_t first = token.NextChar();
  int len = 0;

  // Read in more digits.
  wchar_t c = first;
  while ('\0' != c && IsAsciiDigit(c)) {
    ++token.length;
    ++len;
    c = token.NextChar();
  }
  // We need at least 1 digit.
  if (len == 0)
    return false;

  if (!can_have_leading_zeros && len > 1 && '0' == first)
    return false;

  return true;
}

// A helper method for ParseStringToken.  It reads |digits| hex digits from the
// token. If the sequence if digits is not valid (contains other characters),
// the method returns false.
bool ReadHexDigits(base::JSONReader::Token& token, int digits) {
  for (int i = 1; i <= digits; ++i) {
    wchar_t c = *(token.begin + token.length + i);
    if ('\0' == c)
      return false;
    if (!IsHexDigit(c))
      return false;
  }

  token.length += digits;
  return true;
}

}  // namespace

namespace base {

const char* JSONReader::kBadRootElementType =
    "Root value must be an array or object.";
const char* JSONReader::kInvalidEscape =
    "Invalid escape sequence.";
const char* JSONReader::kSyntaxError =
    "Syntax error.";
const char* JSONReader::kTrailingComma =
    "Trailing comma not allowed.";
const char* JSONReader::kTooMuchNesting =
    "Too much nesting.";
const char* JSONReader::kUnexpectedDataAfterRoot =
    "Unexpected data after root element.";
const char* JSONReader::kUnsupportedEncoding =
    "Unsupported encoding. JSON must be UTF-8.";
const char* JSONReader::kUnquotedDictionaryKey =
    "Dictionary keys must be quoted.";

JSONReader::JSONReader()
    : start_pos_(NULL),
      json_pos_(NULL),
      stack_depth_(0),
      allow_trailing_comma_(false),
      error_code_(JSON_NO_ERROR),
      error_line_(0),
      error_col_(0) {}

// static
Value* JSONReader::Read(const std::string& json,
                        bool allow_trailing_comma) {
  return ReadAndReturnError(json, allow_trailing_comma, NULL, NULL);
}

// static
Value* JSONReader::ReadAndReturnError(const std::string& json,
                                      bool allow_trailing_comma,
                                      int* error_code_out,
                                      std::string* error_msg_out) {
  JSONReader reader = JSONReader();
  Value* root = reader.JsonToValue(json, true, allow_trailing_comma);
  if (root)
    return root;

  if (error_code_out)
    *error_code_out = reader.error_code();
  if (error_msg_out)
    *error_msg_out = reader.GetErrorMessage();

  return NULL;
}

// static
std::string JSONReader::ErrorCodeToString(JsonParseError error_code) {
  switch (error_code) {
    case JSON_NO_ERROR:
      return std::string();
    case JSON_BAD_ROOT_ELEMENT_TYPE:
      return kBadRootElementType;
    case JSON_INVALID_ESCAPE:
      return kInvalidEscape;
    case JSON_SYNTAX_ERROR:
      return kSyntaxError;
    case JSON_TRAILING_COMMA:
      return kTrailingComma;
    case JSON_TOO_MUCH_NESTING:
      return kTooMuchNesting;
    case JSON_UNEXPECTED_DATA_AFTER_ROOT:
      return kUnexpectedDataAfterRoot;
    case JSON_UNSUPPORTED_ENCODING:
      return kUnsupportedEncoding;
    case JSON_UNQUOTED_DICTIONARY_KEY:
      return kUnquotedDictionaryKey;
    default:
      NOTREACHED();
      return std::string();
  }
}

std::string JSONReader::GetErrorMessage() const {
  return FormatErrorMessage(error_line_, error_col_,
                            ErrorCodeToString(error_code_));
}

Value* JSONReader::JsonToValue(const std::string& json, bool check_root,
                               bool allow_trailing_comma) {
  // The input must be in UTF-8.
  if (!IsStringUTF8(json.c_str())) {
    error_code_ = JSON_UNSUPPORTED_ENCODING;
    return NULL;
  }

  // The conversion from UTF8 to wstring removes null bytes for us
  // (a good thing).
  std::wstring json_wide(UTF8ToWide(json));
  start_pos_ = json_wide.c_str();

  // When the input JSON string starts with a UTF-8 Byte-Order-Mark
  // (0xEF, 0xBB, 0xBF), the UTF8ToWide() function converts it to a Unicode
  // BOM (U+FEFF). To avoid the JSONReader::BuildValue() function from
  // mis-treating a Unicode BOM as an invalid character and returning NULL,
  // skip a converted Unicode BOM if it exists.
  if (!json_wide.empty() && start_pos_[0] == 0xFEFF) {
    ++start_pos_;
  }

  json_pos_ = start_pos_;
  allow_trailing_comma_ = allow_trailing_comma;
  stack_depth_ = 0;
  error_code_ = JSON_NO_ERROR;

  scoped_ptr<Value> root(BuildValue(check_root));
  if (root.get()) {
    if (ParseToken().type == Token::END_OF_INPUT) {
      return root.release();
    } else {
      SetErrorCode(JSON_UNEXPECTED_DATA_AFTER_ROOT, json_pos_);
    }
  }

  // Default to calling errors "syntax errors".
  if (error_code_ == 0)
    SetErrorCode(JSON_SYNTAX_ERROR, json_pos_);

  return NULL;
}

// static
std::string JSONReader::FormatErrorMessage(int line, int column,
                                           const std::string& description) {
  if (line || column) {
    return base::StringPrintf(
        "Line: %i, column: %i, %s", line, column, description.c_str());
  }
  return description;
}

Value* JSONReader::BuildValue(bool is_root) {
  ++stack_depth_;
  if (stack_depth_ > kStackLimit) {
    SetErrorCode(JSON_TOO_MUCH_NESTING, json_pos_);
    return NULL;
  }

  Token token = ParseToken();
  // The root token must be an array or an object.
  if (is_root && token.type != Token::OBJECT_BEGIN &&
      token.type != Token::ARRAY_BEGIN) {
    SetErrorCode(JSON_BAD_ROOT_ELEMENT_TYPE, json_pos_);
    return NULL;
  }

  scoped_ptr<Value> node;

  switch (token.type) {
    case Token::END_OF_INPUT:
    case Token::INVALID_TOKEN:
      return NULL;

    case Token::NULL_TOKEN:
      node.reset(Value::CreateNullValue());
      break;

    case Token::BOOL_TRUE:
      node.reset(Value::CreateBooleanValue(true));
      break;

    case Token::BOOL_FALSE:
      node.reset(Value::CreateBooleanValue(false));
      break;

    case Token::NUMBER:
      node.reset(DecodeNumber(token));
      if (!node.get())
        return NULL;
      break;

    case Token::STRING:
      node.reset(DecodeString(token));
      if (!node.get())
        return NULL;
      break;

    case Token::ARRAY_BEGIN:
      {
        json_pos_ += token.length;
        token = ParseToken();

        node.reset(new ListValue());
        while (token.type != Token::ARRAY_END) {
          Value* array_node = BuildValue(false);
          if (!array_node)
            return NULL;
          static_cast<ListValue*>(node.get())->Append(array_node);

          // After a list value, we expect a comma or the end of the list.
          token = ParseToken();
          if (token.type == Token::LIST_SEPARATOR) {
            json_pos_ += token.length;
            token = ParseToken();
            // Trailing commas are invalid according to the JSON RFC, but some
            // consumers need the parsing leniency, so handle accordingly.
            if (token.type == Token::ARRAY_END) {
              if (!allow_trailing_comma_) {
                SetErrorCode(JSON_TRAILING_COMMA, json_pos_);
                return NULL;
              }
              // Trailing comma OK, stop parsing the Array.
              break;
            }
          } else if (token.type != Token::ARRAY_END) {
            // Unexpected value after list value.  Bail out.
            return NULL;
          }
        }
        if (token.type != Token::ARRAY_END) {
          return NULL;
        }
        break;
      }

    case Token::OBJECT_BEGIN:
      {
        json_pos_ += token.length;
        token = ParseToken();

        node.reset(new DictionaryValue);
        while (token.type != Token::OBJECT_END) {
          if (token.type != Token::STRING) {
            SetErrorCode(JSON_UNQUOTED_DICTIONARY_KEY, json_pos_);
            return NULL;
          }
          scoped_ptr<Value> dict_key_value(DecodeString(token));
          if (!dict_key_value.get())
            return NULL;

          // Convert the key into a wstring.
          std::string dict_key;
          bool success = dict_key_value->GetAsString(&dict_key);
          DCHECK(success);

          json_pos_ += token.length;
          token = ParseToken();
          if (token.type != Token::OBJECT_PAIR_SEPARATOR)
            return NULL;

          json_pos_ += token.length;
          token = ParseToken();
          Value* dict_value = BuildValue(false);
          if (!dict_value)
            return NULL;
          static_cast<DictionaryValue*>(node.get())->SetWithoutPathExpansion(
              dict_key, dict_value);

          // After a key/value pair, we expect a comma or the end of the
          // object.
          token = ParseToken();
          if (token.type == Token::LIST_SEPARATOR) {
            json_pos_ += token.length;
            token = ParseToken();
            // Trailing commas are invalid according to the JSON RFC, but some
            // consumers need the parsing leniency, so handle accordingly.
            if (token.type == Token::OBJECT_END) {
              if (!allow_trailing_comma_) {
                SetErrorCode(JSON_TRAILING_COMMA, json_pos_);
                return NULL;
              }
              // Trailing comma OK, stop parsing the Object.
              break;
            }
          } else if (token.type != Token::OBJECT_END) {
            // Unexpected value after last object value.  Bail out.
            return NULL;
          }
        }
        if (token.type != Token::OBJECT_END)
          return NULL;

        break;
      }

    default:
      // We got a token that's not a value.
      return NULL;
  }
  json_pos_ += token.length;

  --stack_depth_;
  return node.release();
}

JSONReader::Token JSONReader::ParseNumberToken() {
  // We just grab the number here.  We validate the size in DecodeNumber.
  // According   to RFC4627, a valid number is: [minus] int [frac] [exp]
  Token token(Token::NUMBER, json_pos_, 0);
  wchar_t c = *json_pos_;
  if ('-' == c) {
    ++token.length;
    c = token.NextChar();
  }

  if (!ReadInt(token, false))
    return Token::CreateInvalidToken();

  // Optional fraction part
  c = token.NextChar();
  if ('.' == c) {
    ++token.length;
    if (!ReadInt(token, true))
      return Token::CreateInvalidToken();
    c = token.NextChar();
  }

  // Optional exponent part
  if ('e' == c || 'E' == c) {
    ++token.length;
    c = token.NextChar();
    if ('-' == c || '+' == c) {
      ++token.length;
      c = token.NextChar();
    }
    if (!ReadInt(token, true))
      return Token::CreateInvalidToken();
  }

  return token;
}

Value* JSONReader::DecodeNumber(const Token& token) {
  const std::wstring num_string(token.begin, token.length);

  int num_int;
  if (StringToInt(WideToUTF8(num_string), &num_int))
    return Value::CreateIntegerValue(num_int);

  double num_double;
  if (StringToDouble(WideToUTF8(num_string), &num_double) &&
      base::IsFinite(num_double))
    return Value::CreateDoubleValue(num_double);

  return NULL;
}

JSONReader::Token JSONReader::ParseStringToken() {
  Token token(Token::STRING, json_pos_, 1);
  wchar_t c = token.NextChar();
  while ('\0' != c) {
    if ('\\' == c) {
      ++token.length;
      c = token.NextChar();
      // Make sure the escaped char is valid.
      switch (c) {
        case 'x':
          if (!ReadHexDigits(token, 2)) {
            SetErrorCode(JSON_INVALID_ESCAPE, json_pos_ + token.length);
            return Token::CreateInvalidToken();
          }
          break;
        case 'u':
          if (!ReadHexDigits(token, 4)) {
            SetErrorCode(JSON_INVALID_ESCAPE, json_pos_ + token.length);
            return Token::CreateInvalidToken();
          }
          break;
        case '\\':
        case '/':
        case 'b':
        case 'f':
        case 'n':
        case 'r':
        case 't':
        case 'v':
        case '"':
          break;
        default:
          SetErrorCode(JSON_INVALID_ESCAPE, json_pos_ + token.length);
          return Token::CreateInvalidToken();
      }
    } else if ('"' == c) {
      ++token.length;
      return token;
    }
    ++token.length;
    c = token.NextChar();
  }
  return Token::CreateInvalidToken();
}

Value* JSONReader::DecodeString(const Token& token) {
  std::wstring decoded_str;
  decoded_str.reserve(token.length - 2);

  for (int i = 1; i < token.length - 1; ++i) {
    wchar_t c = *(token.begin + i);
    if ('\\' == c) {
      ++i;
      c = *(token.begin + i);
      switch (c) {
        case '"':
        case '/':
        case '\\':
          decoded_str.push_back(c);
          break;
        case 'b':
          decoded_str.push_back('\b');
          break;
        case 'f':
          decoded_str.push_back('\f');
          break;
        case 'n':
          decoded_str.push_back('\n');
          break;
        case 'r':
          decoded_str.push_back('\r');
          break;
        case 't':
          decoded_str.push_back('\t');
          break;
        case 'v':
          decoded_str.push_back('\v');
          break;

        case 'x':
          decoded_str.push_back((HexDigitToInt(*(token.begin + i + 1)) << 4) +
                                HexDigitToInt(*(token.begin + i + 2)));
          i += 2;
          break;
        case 'u':
          decoded_str.push_back((HexDigitToInt(*(token.begin + i + 1)) << 12 ) +
                                (HexDigitToInt(*(token.begin + i + 2)) << 8) +
                                (HexDigitToInt(*(token.begin + i + 3)) << 4) +
                                HexDigitToInt(*(token.begin + i + 4)));
          i += 4;
          break;

        default:
          // We should only have valid strings at this point.  If not,
          // ParseStringToken didn't do it's job.
          NOTREACHED();
          return NULL;
      }
    } else {
      // Not escaped
      decoded_str.push_back(c);
    }
  }
  return Value::CreateStringValue(WideToUTF16Hack(decoded_str));
}

JSONReader::Token JSONReader::ParseToken() {
  EatWhitespaceAndComments();

  Token token(Token::INVALID_TOKEN, 0, 0);
  switch (*json_pos_) {
    case '\0':
      token.type = Token::END_OF_INPUT;
      break;

    case 'n':
      if (NextStringMatch(kNullString, arraysize(kNullString) - 1))
        token = Token(Token::NULL_TOKEN, json_pos_, 4);
      break;

    case 't':
      if (NextStringMatch(kTrueString, arraysize(kTrueString) - 1))
        token = Token(Token::BOOL_TRUE, json_pos_, 4);
      break;

    case 'f':
      if (NextStringMatch(kFalseString, arraysize(kFalseString) - 1))
        token = Token(Token::BOOL_FALSE, json_pos_, 5);
      break;

    case '[':
      token = Token(Token::ARRAY_BEGIN, json_pos_, 1);
      break;

    case ']':
      token = Token(Token::ARRAY_END, json_pos_, 1);
      break;

    case ',':
      token = Token(Token::LIST_SEPARATOR, json_pos_, 1);
      break;

    case '{':
      token = Token(Token::OBJECT_BEGIN, json_pos_, 1);
      break;

    case '}':
      token = Token(Token::OBJECT_END, json_pos_, 1);
      break;

    case ':':
      token = Token(Token::OBJECT_PAIR_SEPARATOR, json_pos_, 1);
      break;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '-':
      token = ParseNumberToken();
      break;

    case '"':
      token = ParseStringToken();
      break;
  }
  return token;
}

void JSONReader::EatWhitespaceAndComments() {
  while ('\0' != *json_pos_) {
    switch (*json_pos_) {
      case ' ':
      case '\n':
      case '\r':
      case '\t':
        ++json_pos_;
        break;
      case '/':
        // TODO(tc): This isn't in the RFC so it should be a parser flag.
        if (!EatComment())
          return;
        break;
      default:
        // Not a whitespace char, just exit.
        return;
    }
  }
}

bool JSONReader::EatComment() {
  if ('/' != *json_pos_)
    return false;

  wchar_t next_char = *(json_pos_ + 1);
  if ('/' == next_char) {
    // Line comment, read until \n or \r
    json_pos_ += 2;
    while ('\0' != *json_pos_) {
      switch (*json_pos_) {
        case '\n':
        case '\r':
          ++json_pos_;
          return true;
        default:
          ++json_pos_;
      }
    }
  } else if ('*' == next_char) {
    // Block comment, read until */
    json_pos_ += 2;
    while ('\0' != *json_pos_) {
      if ('*' == *json_pos_ && '/' == *(json_pos_ + 1)) {
        json_pos_ += 2;
        return true;
      }
      ++json_pos_;
    }
  } else {
    return false;
  }
  return true;
}

bool JSONReader::NextStringMatch(const wchar_t* str, size_t length) {
  return wcsncmp(json_pos_, str, length) == 0;
}

void JSONReader::SetErrorCode(JsonParseError error,
                              const wchar_t* error_pos) {
  int line_number = 1;
  int column_number = 1;

  // Figure out the line and column the error occured at.
  for (const wchar_t* pos = start_pos_; pos != error_pos; ++pos) {
    if (*pos == '\0') {
      NOTREACHED();
      return;
    }

    if (*pos == '\n') {
      ++line_number;
      column_number = 1;
    } else {
      ++column_number;
    }
  }

  error_line_ = line_number;
  error_col_ = column_number;
  error_code_ = error;
}

}  // namespace base
