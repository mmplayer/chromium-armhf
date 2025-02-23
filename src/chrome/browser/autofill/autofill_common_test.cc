// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_common_test.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "webkit/glue/form_field.h"

namespace autofill_test {

void CreateTestFormField(const char* label,
                         const char* name,
                         const char* value,
                         const char* type,
                         webkit_glue::FormField* field) {
  field->label = ASCIIToUTF16(label);
  field->name = ASCIIToUTF16(name);
  field->value = ASCIIToUTF16(value);
  field->form_control_type = ASCIIToUTF16(type);
}

inline void check_and_set(
    FormGroup* profile, AutofillFieldType type, const char* value) {
  if (value)
    profile->SetInfo(type, UTF8ToUTF16(value));
}

void SetProfileInfo(AutofillProfile* profile,
    const char* first_name, const char* middle_name,
    const char* last_name, const char* email, const char* company,
    const char* address1, const char* address2, const char* city,
    const char* state, const char* zipcode, const char* country,
    const char* phone) {
  check_and_set(profile, NAME_FIRST, first_name);
  check_and_set(profile, NAME_MIDDLE, middle_name);
  check_and_set(profile, NAME_LAST, last_name);
  check_and_set(profile, EMAIL_ADDRESS, email);
  check_and_set(profile, COMPANY_NAME, company);
  check_and_set(profile, ADDRESS_HOME_LINE1, address1);
  check_and_set(profile, ADDRESS_HOME_LINE2, address2);
  check_and_set(profile, ADDRESS_HOME_CITY, city);
  check_and_set(profile, ADDRESS_HOME_STATE, state);
  check_and_set(profile, ADDRESS_HOME_ZIP, zipcode);
  check_and_set(profile, ADDRESS_HOME_COUNTRY, country);
  check_and_set(profile, PHONE_HOME_WHOLE_NUMBER, phone);
}

void SetProfileInfoWithGuid(AutofillProfile* profile,
    const char* guid, const char* first_name, const char* middle_name,
    const char* last_name, const char* email, const char* company,
    const char* address1, const char* address2, const char* city,
    const char* state, const char* zipcode, const char* country,
    const char* phone) {
  if (guid)
    profile->set_guid(guid);
  SetProfileInfo(profile, first_name, middle_name, last_name, email,
                 company, address1, address2, city, state, zipcode, country,
                 phone);
}

void SetCreditCardInfo(CreditCard* credit_card,
    const char* name_on_card, const char* card_number,
    const char* expiration_month, const char* expiration_year) {
  check_and_set(credit_card, CREDIT_CARD_NAME, name_on_card);
  check_and_set(credit_card, CREDIT_CARD_NUMBER, card_number);
  check_and_set(credit_card, CREDIT_CARD_EXP_MONTH, expiration_month);
  check_and_set(credit_card, CREDIT_CARD_EXP_4_DIGIT_YEAR, expiration_year);
}

void DisableSystemServices(Profile* profile) {
  // Use a mock Keychain rather than the OS one to store credit card data.
#if defined(OS_MACOSX)
  Encryptor::UseMockKeychain(true);
#endif

  // Disable auxiliary profiles for unit testing.  These reach out to system
  // services on the Mac.
  if (profile) {
    profile->GetPrefs()->SetBoolean(prefs::kAutofillAuxiliaryProfilesEnabled,
                                    false);
  }
}

}  // namespace autofill_test
