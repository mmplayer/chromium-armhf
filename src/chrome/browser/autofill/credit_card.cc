// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/credit_card.h"

#include <stddef.h>

#include <ostream>
#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "chrome/browser/autofill/autofill_regexes.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/common/guid.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "unicode/dtfmtsym.h"
#include "unicode/uloc.h"

namespace {

const char16 kCreditCardObfuscationSymbol = '*';

const AutofillFieldType kAutofillCreditCardTypes[] = {
  CREDIT_CARD_NAME,
  CREDIT_CARD_NUMBER,
  CREDIT_CARD_TYPE,
  CREDIT_CARD_EXP_MONTH,
  CREDIT_CARD_EXP_4_DIGIT_YEAR,
};

const int kAutofillCreditCardLength = arraysize(kAutofillCreditCardTypes);

// Returns a version of |number| that has any separator characters removed.
const string16 StripSeparators(const string16& number) {
  const char16 kSeparators[] = {'-', ' ', '\0'};
  string16 stripped;
  RemoveChars(number, kSeparators, &stripped);
  return stripped;
}

std::string GetCreditCardType(const string16& number) {
  // Don't check for a specific type if this is not a credit card number.
  if (!CreditCard::IsValidCreditCardNumber(number))
    return kGenericCard;

  // Credit card number specifications taken from:
  // http://en.wikipedia.org/wiki/Credit_card_numbers and
  // http://www.beachnet.com/~hstiles/cardtype.html
  // Card Type              Prefix(es)                      Length
  // ---------------------------------------------------------------
  // Visa                   4                               13,16
  // American Express       34,37                           15
  // Diners Club            300-305,2014,2149,36,           14,15
  // Discover Card          6011,65                         16
  // JCB                    3                               16
  // JCB                    2131,1800                       15
  // MasterCard             51-55                           16
  // Solo (debit card)      6334,6767                       16,18,19

  // We need at least 4 digits to work with.
  if (number.length() < 4)
    return kGenericCard;

  int first_four_digits = 0;
  if (!base::StringToInt(number.substr(0, 4), &first_four_digits))
    return kGenericCard;

  int first_three_digits = first_four_digits / 10;
  int first_two_digits = first_three_digits / 10;
  int first_digit = first_two_digits / 10;

  switch (number.length()) {
    case 13:
      if (first_digit == 4)
        return kVisaCard;

      break;
    case 14:
      if (first_three_digits >= 300 && first_three_digits <=305)
        return kDinersCard;

      if (first_digit == 36)
        return kDinersCard;

      break;
    case 15:
      if (first_two_digits == 34 || first_two_digits == 37)
        return kAmericanExpressCard;

      if (first_four_digits == 2131 || first_four_digits == 1800)
        return kJCBCard;

      if (first_four_digits == 2014 || first_four_digits == 2149)
        return kDinersCard;

      break;
    case 16:
      if (first_four_digits == 6011 || first_two_digits == 65)
        return kDiscoverCard;

      if (first_four_digits == 6334 || first_four_digits == 6767)
        return kSoloCard;

      if (first_two_digits >= 51 && first_two_digits <= 55)
        return kMasterCard;

      if (first_digit == 3)
        return kJCBCard;

      if (first_digit == 4)
        return kVisaCard;

      break;
    case 18:
    case 19:
      if (first_four_digits == 6334 || first_four_digits == 6767)
        return kSoloCard;

      break;
  }

  return kGenericCard;
}

bool ConvertYear(const string16& year, int* num) {
  // If the |year| is empty, clear the stored value.
  if (year.empty()) {
    *num = 0;
    return true;
  }

  // Try parsing the |year| as a number.
  if (base::StringToInt(year, num))
    return true;

  *num = 0;
  return false;
}

bool ConvertMonth(const string16& month, int* num) {
  // If the |month| is empty, clear the stored value.
  if (month.empty()) {
    *num = 0;
    return true;
  }

  // Try parsing the |month| as a number.
  if (base::StringToInt(month, num))
    return true;

  // Try parsing the |month| as a named month, e.g. "January" or "Jan".
  string16 lowercased_month = StringToLowerASCII(month);

  UErrorCode status = U_ZERO_ERROR;
  icu::Locale locale(AutofillCountry::ApplicationLocale().c_str());
  icu::DateFormatSymbols date_format_symbols(locale, status);
  DCHECK(status == U_ZERO_ERROR || status == U_USING_FALLBACK_WARNING ||
         status == U_USING_DEFAULT_WARNING);

  int32_t num_months;
  const icu::UnicodeString* months = date_format_symbols.getMonths(num_months);
  for (int32_t i = 0; i < num_months; ++i) {
    const string16 icu_month = string16(months[i].getBuffer(),
                                        months[i].length());
    if (lowercased_month == StringToLowerASCII(icu_month)) {
      *num = i + 1;  // Adjust from 0-indexed to 1-indexed.
      return true;
    }
  }

  months = date_format_symbols.getShortMonths(num_months);
  for (int32_t i = 0; i < num_months; ++i) {
    const string16 icu_month = string16(months[i].getBuffer(),
                                        months[i].length());
    if (lowercased_month == StringToLowerASCII(icu_month)) {
      *num = i + 1;  // Adjust from 0-indexed to 1-indexed.
      return true;
    }
  }

  *num = 0;
  return false;
}

}  // namespace

CreditCard::CreditCard(const std::string& guid)
    : type_(kGenericCard),
      expiration_month_(0),
      expiration_year_(0),
      guid_(guid) {
}

CreditCard::CreditCard()
    : type_(kGenericCard),
      expiration_month_(0),
      expiration_year_(0),
      guid_(guid::GenerateGUID()) {
}

CreditCard::CreditCard(const CreditCard& credit_card) : FormGroup() {
  operator=(credit_card);
}

CreditCard::~CreditCard() {}

void CreditCard::GetSupportedTypes(FieldTypeSet* supported_types) const {
  supported_types->insert(CREDIT_CARD_NAME);
  supported_types->insert(CREDIT_CARD_NUMBER);
  supported_types->insert(CREDIT_CARD_EXP_MONTH);
  supported_types->insert(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  supported_types->insert(CREDIT_CARD_EXP_4_DIGIT_YEAR);
  supported_types->insert(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR);
  supported_types->insert(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);
}

string16 CreditCard::GetInfo(AutofillFieldType type) const {
  switch (type) {
    case CREDIT_CARD_NAME:
      return name_on_card_;

    case CREDIT_CARD_EXP_MONTH:
      return ExpirationMonthAsString();

    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
      return Expiration2DigitYearAsString();

    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return Expiration4DigitYearAsString();

    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR: {
      string16 month = ExpirationMonthAsString();
      string16 year = Expiration2DigitYearAsString();
      if (!month.empty() && !year.empty())
        return month + ASCIIToUTF16("/") + year;
      return string16();
    }

    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR: {
      string16 month = ExpirationMonthAsString();
      string16 year = Expiration4DigitYearAsString();
      if (!month.empty() && !year.empty())
        return month + ASCIIToUTF16("/") + year;
      return string16();
    }

    case CREDIT_CARD_TYPE:
      // We don't handle this case.
      return string16();

    case CREDIT_CARD_NUMBER:
      return number_;

    case CREDIT_CARD_VERIFICATION_CODE:
      NOTREACHED();
      return string16();

    default:
      // ComputeDataPresentForArray will hit this repeatedly.
      return string16();
  }
}

void CreditCard::SetInfo(AutofillFieldType type, const string16& value) {
  switch (type) {
    case CREDIT_CARD_NAME:
      name_on_card_ = value;
      break;

    case CREDIT_CARD_EXP_MONTH:
      SetExpirationMonthFromString(value);
      break;

    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
      // This is a read-only attribute.
      break;

    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      SetExpirationYearFromString(value);
      break;

    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
      // This is a read-only attribute.
      break;

    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      // This is a read-only attribute.
      break;

    case CREDIT_CARD_TYPE:
      // We determine the type based on the number.
      break;

    case CREDIT_CARD_NUMBER: {
      // Don't change the real value if the input is an obfuscated string.
      if (value.size() > 0 && value[0] != kCreditCardObfuscationSymbol)
        SetNumber(value);
      break;
    }

    case CREDIT_CARD_VERIFICATION_CODE:
      NOTREACHED();
      break;

    default:
      NOTREACHED() << "Attempting to set unknown info-type " << type;
      break;
  }
}

string16 CreditCard::GetCanonicalizedInfo(AutofillFieldType type) const {
  if (type == CREDIT_CARD_NUMBER)
    return StripSeparators(number_);

  return GetInfo(type);
}

bool CreditCard::SetCanonicalizedInfo(AutofillFieldType type,
                                      const string16& value) {
  if (type == CREDIT_CARD_NUMBER)
    SetInfo(type, StripSeparators(value));
  else
    SetInfo(type, value);

  return true;
}

void CreditCard::GetMatchingTypes(const string16& text,
                                  FieldTypeSet* matching_types) const {
  FormGroup::GetMatchingTypes(text, matching_types);

  string16 card_number = GetCanonicalizedInfo(CREDIT_CARD_NUMBER);
  if (!card_number.empty() && StripSeparators(text) == card_number)
    matching_types->insert(CREDIT_CARD_NUMBER);

  int month;
  if (ConvertMonth(text, &month) && month != 0 && month == expiration_month_)
    matching_types->insert(CREDIT_CARD_EXP_MONTH);
}

const string16 CreditCard::Label() const {
  string16 label;
  if (number().empty())
    return name_on_card_;  // No CC number, return name only.

  string16 obfuscated_cc_number = ObfuscatedNumber();
  if (!expiration_month_ || !expiration_year_)
    return obfuscated_cc_number;  // No expiration date set.

  // TODO(georgey): Internationalize date.
  string16 formatted_date(ExpirationMonthAsString());
  formatted_date.append(ASCIIToUTF16("/"));
  formatted_date.append(Expiration4DigitYearAsString());

  label = l10n_util::GetStringFUTF16(IDS_CREDIT_CARD_NUMBER_PREVIEW_FORMAT,
                                     obfuscated_cc_number,
                                     formatted_date);
  return label;
}

void CreditCard::SetInfoForMonthInputType(const string16& value) {
  // Check if |text| is "yyyy-mm" format first, and check normal month format.
  if (!autofill::MatchesPattern(value, UTF8ToUTF16("^[0-9]{4}-[0-9]{1,2}$")))
    return;

  std::vector<string16> year_month;
  base::SplitString(value, L'-', &year_month);
  DCHECK_EQ((int)year_month.size(), 2);
  int num = 0;
  bool converted = false;
  converted = base::StringToInt(year_month[0], &num);
  DCHECK(converted);
  SetExpirationYear(num);
  converted = base::StringToInt(year_month[1], &num);
  DCHECK(converted);
  SetExpirationMonth(num);
}

string16 CreditCard::ObfuscatedNumber() const {
  // If the number is shorter than four digits, there's no need to obfuscate it.
  if (number_.size() < 4)
    return number_;

  string16 number = StripSeparators(number_);
  string16 result(number.size() - 4, kCreditCardObfuscationSymbol);
  result.append(LastFourDigits());

  return result;
}

string16 CreditCard::LastFourDigits() const {
  static const size_t kNumLastDigits = 4;

  string16 number = StripSeparators(number_);
  if (number.size() < kNumLastDigits)
    return string16();

  return number.substr(number.size() - kNumLastDigits, kNumLastDigits);
}

void CreditCard::operator=(const CreditCard& credit_card) {
  if (this == &credit_card)
    return;

  number_ = credit_card.number_;
  name_on_card_ = credit_card.name_on_card_;
  type_ = credit_card.type_;
  expiration_month_ = credit_card.expiration_month_;
  expiration_year_ = credit_card.expiration_year_;
  guid_ = credit_card.guid_;
}

bool CreditCard::UpdateFromImportedCard(const CreditCard& imported_card) {
  if (this->GetCanonicalizedInfo(CREDIT_CARD_NUMBER) !=
          imported_card.GetCanonicalizedInfo(CREDIT_CARD_NUMBER)) {
    return false;
  }

  // Note that the card number is intentionally not updated, so as to preserve
  // any formatting (i.e. separator characters).  Since the card number is not
  // updated, there is no reason to update the card type, either.
  if (!imported_card.name_on_card_.empty())
    name_on_card_ = imported_card.name_on_card_;

  // The expiration date for |imported_card| should always be set.
  DCHECK(imported_card.expiration_month_ && imported_card.expiration_year_);
  expiration_month_ = imported_card.expiration_month_;
  expiration_year_ = imported_card.expiration_year_;

  return true;
}

int CreditCard::Compare(const CreditCard& credit_card) const {
  // The following CreditCard field types are the only types we store in the
  // WebDB so far, so we're only concerned with matching these types in the
  // credit card.
  const AutofillFieldType types[] = { CREDIT_CARD_NAME,
                                      CREDIT_CARD_NUMBER,
                                      CREDIT_CARD_EXP_MONTH,
                                      CREDIT_CARD_EXP_4_DIGIT_YEAR };
  for (size_t index = 0; index < arraysize(types); ++index) {
    int comparison = GetInfo(types[index]).compare(
        credit_card.GetInfo(types[index]));
    if (comparison != 0)
      return comparison;
  }

  return 0;
}

int CreditCard::CompareMulti(const CreditCard& credit_card) const {
  return Compare(credit_card);
}

bool CreditCard::operator==(const CreditCard& credit_card) const {
  if (guid_ != credit_card.guid_)
    return false;

  return Compare(credit_card) == 0;
}

bool CreditCard::operator!=(const CreditCard& credit_card) const {
  return !operator==(credit_card);
}

// static
bool CreditCard::IsValidCreditCardNumber(const string16& text) {
  string16 number = StripSeparators(text);

  // Credit card numbers are at most 19 digits in length [1]. 12 digits seems to
  // be a fairly safe lower-bound [2].
  // [1] http://www.merriampark.com/anatomycc.htm
  // [2] http://en.wikipedia.org/wiki/Bank_card_number
  const size_t kMinCreditCardDigits = 12;
  const size_t kMaxCreditCardDigits = 19;
  if (number.size() < kMinCreditCardDigits ||
      number.size() > kMaxCreditCardDigits)
    return false;

  // Use the Luhn formula [3] to validate the number.
  // [3] http://en.wikipedia.org/wiki/Luhn_algorithm
  int sum = 0;
  bool odd = false;
  string16::reverse_iterator iter;
  for (iter = number.rbegin(); iter != number.rend(); ++iter) {
    if (!IsAsciiDigit(*iter))
      return false;

    int digit = *iter - '0';
    if (odd) {
      digit *= 2;
      sum += digit / 10 + digit % 10;
    } else {
      sum += digit;
    }
    odd = !odd;
  }

  return (sum % 10) == 0;
}

bool CreditCard::IsEmpty() const {
  FieldTypeSet types;
  GetNonEmptyTypes(&types);
  return types.empty();
}

bool CreditCard::IsComplete() const {
  return
      IsValidCreditCardNumber(number_) &&
      expiration_month_ != 0 &&
      expiration_year_ != 0;
}

string16 CreditCard::ExpirationMonthAsString() const {
  if (expiration_month_ == 0)
    return string16();

  string16 month = base::IntToString16(expiration_month_);
  if (expiration_month_ >= 10)
    return month;

  string16 zero = ASCIIToUTF16("0");
  zero.append(month);
  return zero;
}

string16 CreditCard::Expiration4DigitYearAsString() const {
  if (expiration_year_ == 0)
    return string16();

  return base::IntToString16(Expiration4DigitYear());
}

string16 CreditCard::Expiration2DigitYearAsString() const {
  if (expiration_year_ == 0)
    return string16();

  return base::IntToString16(Expiration2DigitYear());
}

void CreditCard::SetExpirationMonthFromString(const string16& text) {
  int month;
  if (!ConvertMonth(text, &month))
    return;

  SetExpirationMonth(month);
}

void CreditCard::SetExpirationYearFromString(const string16& text) {
  int year;
  if (!ConvertYear(text, &year))
    return;

  SetExpirationYear(year);
}

void CreditCard::SetNumber(const string16& number) {
  number_ = number;
  type_ = GetCreditCardType(StripSeparators(number_));
}

void CreditCard::SetExpirationMonth(int expiration_month) {
  if (expiration_month < 0 || expiration_month > 12)
    return;

  expiration_month_ = expiration_month;
}

void CreditCard::SetExpirationYear(int expiration_year) {
  if (expiration_year != 0 &&
      (expiration_year < 2006 || expiration_year > 10000)) {
    return;
  }

  expiration_year_ = expiration_year;
}

// So we can compare CreditCards with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const CreditCard& credit_card) {
  return os
      << UTF16ToUTF8(credit_card.Label())
      << " "
      << credit_card.guid()
      << " "
      << UTF16ToUTF8(credit_card.GetInfo(CREDIT_CARD_NAME))
      << " "
      << UTF16ToUTF8(credit_card.GetInfo(CREDIT_CARD_TYPE))
      << " "
      << UTF16ToUTF8(credit_card.GetInfo(CREDIT_CARD_NUMBER))
      << " "
      << UTF16ToUTF8(credit_card.GetInfo(CREDIT_CARD_EXP_MONTH))
      << " "
      << UTF16ToUTF8(credit_card.GetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));
}

// These values must match the values in WebKitPlatformSupportImpl in
// webkit/glue. We send these strings to WebKit, which then asks
// WebKitPlatformSupportImpl to load the image data.
const char* const kAmericanExpressCard = "americanExpressCC";
const char* const kDinersCard = "dinersCC";
const char* const kDiscoverCard = "discoverCC";
const char* const kGenericCard = "genericCC";
const char* const kJCBCard = "jcbCC";
const char* const kMasterCard = "masterCardCC";
const char* const kSoloCard = "soloCC";
const char* const kVisaCard = "visaCC";
