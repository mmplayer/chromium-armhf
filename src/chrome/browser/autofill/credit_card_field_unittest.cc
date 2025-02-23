// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/autofill_scanner.h"
#include "chrome/browser/autofill/credit_card_field.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/form_field.h"

class CreditCardFieldTest : public testing::Test {
 public:
  CreditCardFieldTest() {}

 protected:
  ScopedVector<const AutofillField> list_;
  scoped_ptr<CreditCardField> field_;
  FieldTypeMap field_type_map_;

  // Downcast for tests.
  static CreditCardField* Parse(AutofillScanner* scanner) {
    return static_cast<CreditCardField*>(CreditCardField::Parse(scanner));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CreditCardFieldTest);
};

TEST_F(CreditCardFieldTest, Empty) {
  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_EQ(static_cast<CreditCardField*>(NULL), field_.get());
}

TEST_F(CreditCardFieldTest, NonParse) {
  list_.push_back(new AutofillField);
  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_EQ(static_cast<CreditCardField*>(NULL), field_.get());
}

TEST_F(CreditCardFieldTest, ParseCreditCardNoNumber) {
  webkit_glue::FormField field;
  field.form_control_type = ASCIIToUTF16("text");

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("month1")));

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("year2")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_EQ(static_cast<CreditCardField*>(NULL), field_.get());
}

TEST_F(CreditCardFieldTest, ParseCreditCardNoDate) {
  webkit_glue::FormField field;
  field.form_control_type = ASCIIToUTF16("text");

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number1")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_EQ(static_cast<CreditCardField*>(NULL), field_.get());
}

TEST_F(CreditCardFieldTest, ParseMiniumCreditCard) {
  webkit_glue::FormField field;
  field.form_control_type = ASCIIToUTF16("text");

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number1")));

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("month2")));

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("year3")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("number1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER, field_type_map_[ASCIIToUTF16("number1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("month2")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, field_type_map_[ASCIIToUTF16("month2")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("year3")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
      field_type_map_[ASCIIToUTF16("year3")]);
}

TEST_F(CreditCardFieldTest, ParseFullCreditCard) {
  webkit_glue::FormField field;
  field.form_control_type = ASCIIToUTF16("text");

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number2")));

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("month3")));

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("year4")));

  field.label = ASCIIToUTF16("Verification");
  field.name = ASCIIToUTF16("verification");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("cvc5")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("number2")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER, field_type_map_[ASCIIToUTF16("number2")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("month3")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, field_type_map_[ASCIIToUTF16("month3")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("year4")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
      field_type_map_[ASCIIToUTF16("year4")]);
  // We don't store CVV.
  EXPECT_TRUE(
      field_type_map_.find(ASCIIToUTF16("cvc5")) == field_type_map_.end());
}

TEST_F(CreditCardFieldTest, ParseExpMonthYear) {
  webkit_glue::FormField field;
  field.form_control_type = ASCIIToUTF16("text");

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number2")));

  field.label = ASCIIToUTF16("ExpDate Month / Year");
  field.name = ASCIIToUTF16("ExpDate");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("month3")));

  field.label = ASCIIToUTF16("ExpDate Month / Year");
  field.name = ASCIIToUTF16("ExpDate");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("year4")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("number2")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER, field_type_map_[ASCIIToUTF16("number2")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("month3")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, field_type_map_[ASCIIToUTF16("month3")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("year4")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
      field_type_map_[ASCIIToUTF16("year4")]);
}

TEST_F(CreditCardFieldTest, ParseExpMonthYear2) {
  webkit_glue::FormField field;
  field.form_control_type = ASCIIToUTF16("text");

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number2")));

  field.label = ASCIIToUTF16("Expiration date Month / Year");
  field.name = ASCIIToUTF16("ExpDate");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("month3")));

  field.label = ASCIIToUTF16("Expiration date Month / Year");
  field.name = ASCIIToUTF16("ExpDate");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("year4")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("number2")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER, field_type_map_[ASCIIToUTF16("number2")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("month3")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, field_type_map_[ASCIIToUTF16("month3")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("year4")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
      field_type_map_[ASCIIToUTF16("year4")]);
}

TEST_F(CreditCardFieldTest, ParseExpField) {
  webkit_glue::FormField field;
  field.form_control_type = ASCIIToUTF16("text");

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number2")));

  field.label = ASCIIToUTF16("Expiration Date (MM/YYYY)");
  field.name = ASCIIToUTF16("cc_exp");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("exp3")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("number2")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER, field_type_map_[ASCIIToUTF16("number2")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("exp3")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
            field_type_map_[ASCIIToUTF16("exp3")]);
}

TEST_F(CreditCardFieldTest, ParseExpField2DigitYear) {
  webkit_glue::FormField field;
  field.form_control_type = ASCIIToUTF16("text");

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name1")));

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("number2")));

  field.label = ASCIIToUTF16("Expiration Date (MM/YY)");
  field.name = ASCIIToUTF16("cc_exp");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("exp3")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME, field_type_map_[ASCIIToUTF16("name1")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("number2")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NUMBER, field_type_map_[ASCIIToUTF16("number2")]);
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("exp3")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
            field_type_map_[ASCIIToUTF16("exp3")]);
}

TEST_F(CreditCardFieldTest, ParseCreditCardHolderNameWithCCFullName) {
  webkit_glue::FormField field;
  field.form_control_type = ASCIIToUTF16("text");

  field.label = ASCIIToUTF16("Name");
  field.name = ASCIIToUTF16("ccfullname");
  list_.push_back(new AutofillField(field, ASCIIToUTF16("name1")));

  AutofillScanner scanner(list_.get());
  field_.reset(Parse(&scanner));
  ASSERT_NE(static_cast<CreditCardField*>(NULL), field_.get());
  ASSERT_TRUE(field_->ClassifyField(&field_type_map_));
  ASSERT_TRUE(
      field_type_map_.find(ASCIIToUTF16("name1")) != field_type_map_.end());
  EXPECT_EQ(CREDIT_CARD_NAME, field_type_map_[ASCIIToUTF16("name1")]);
}
