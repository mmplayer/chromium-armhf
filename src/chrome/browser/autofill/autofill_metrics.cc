// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_metrics.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/form_structure.h"
#include "webkit/glue/form_data.h"

namespace {

// Server experiments we support.
enum ServerExperiment {
  NO_EXPERIMENT = 0,
  UNKNOWN_EXPERIMENT,
  ACCEPTANCE_RATIO_06,
  ACCEPTANCE_RATIO_1,
  ACCEPTANCE_RATIO_2,
  ACCEPTANCE_RATIO_4,
  ACCEPTANCE_RATIO_05_WINNER_LEAD_RATIO_15,
  ACCEPTANCE_RATIO_05_WINNER_LEAD_RATIO_25,
  ACCEPTANCE_RATIO_05_WINNER_LEAD_RATIO_15_MIN_FORM_SCORE_5,
  TOOLBAR_DATA_ONLY,
  ACCEPTANCE_RATIO_04_WINNER_LEAD_RATIO_3_MIN_FORM_SCORE_4,
  NO_SERVER_RESPONSE,
  PROBABILITY_PICKER_05,
  NUM_SERVER_EXPERIMENTS
};

enum FieldTypeGroupForMetrics {
  AMBIGUOUS = 0,
  NAME,
  COMPANY,
  ADDRESS_LINE_1,
  ADDRESS_LINE_2,
  ADDRESS_CITY,
  ADDRESS_STATE,
  ADDRESS_ZIP,
  ADDRESS_COUNTRY,
  PHONE,
  FAX,  // Deprecated.
  EMAIL,
  CREDIT_CARD_NAME,
  CREDIT_CARD_NUMBER,
  CREDIT_CARD_DATE,
  NUM_FIELD_TYPE_GROUPS_FOR_METRICS
};

// First, translates |field_type| to the corresponding logical |group| from
// |FieldTypeGroupForMetrics|.  Then, interpolates this with the given |metric|,
// which should be in the range [0, |num_possible_metrics|).
// Returns the interpolated index.
//
// The interpolation maps the pair (|group|, |metric|) to a single index, so
// that all the indicies for a given group are adjacent.  In particular, with
// the groups {AMBIGUOUS, NAME, ...} combining with the metrics {UNKNOWN, MATCH,
// MISMATCH}, we create this set of mapped indices:
// {
//   AMBIGUOUS+UNKNOWN,
//   AMBIGUOUS+MATCH,
//   AMBIGUOUS+MISMATCH,
//   NAME+UNKNOWN,
//   NAME+MATCH,
//   NAME+MISMATCH,
//   ...
// }.
//
// Clients must ensure that |field_type| is one of the types Chrome supports
// natively, e.g. |field_type| must not be a billng address.
int GetFieldTypeGroupMetric(const AutofillFieldType field_type,
                            const int metric,
                            const int num_possible_metrics) {
  DCHECK(metric < num_possible_metrics);

  FieldTypeGroupForMetrics group;
  switch (AutofillType(field_type).group()) {
    case AutofillType::NO_GROUP:
      group = AMBIGUOUS;
      break;

    case AutofillType::NAME:
      group = NAME;
      break;

    case AutofillType::COMPANY:
      group = COMPANY;
      break;

    case AutofillType::ADDRESS_HOME:
      switch (field_type) {
        case ADDRESS_HOME_LINE1:
          group = ADDRESS_LINE_1;
          break;
        case ADDRESS_HOME_LINE2:
          group = ADDRESS_LINE_2;
          break;
        case ADDRESS_HOME_CITY:
          group = ADDRESS_CITY;
          break;
        case ADDRESS_HOME_STATE:
          group = ADDRESS_STATE;
          break;
        case ADDRESS_HOME_ZIP:
          group = ADDRESS_ZIP;
          break;
        case ADDRESS_HOME_COUNTRY:
          group = ADDRESS_COUNTRY;
          break;
        default:
          NOTREACHED();
          group = AMBIGUOUS;
      }
      break;

    case AutofillType::EMAIL:
      group = EMAIL;
      break;

    case AutofillType::PHONE:
      group = PHONE;
      break;

    case AutofillType::CREDIT_CARD:
      switch (field_type) {
        case ::CREDIT_CARD_NAME:
          group = CREDIT_CARD_NAME;
          break;
        case ::CREDIT_CARD_NUMBER:
          group = CREDIT_CARD_NUMBER;
          break;
        default:
          group = CREDIT_CARD_DATE;
      }
      break;

    default:
      NOTREACHED();
      group = AMBIGUOUS;
  }

  // Interpolate the |metric| with the |group|, so that all metrics for a given
  // |group| are adjacent.
  return (group * num_possible_metrics) + metric;
}

// A version of the UMA_HISTOGRAM_ENUMERATION macro that allows the |name|
// to vary over the program's runtime.
void LogUMAHistogramEnumeration(const std::string& name,
                                int sample,
                                int boundary_value) {
  // We can't use the UMA_HISTOGRAM_ENUMERATION macro here because the histogram
  // name can vary over the duration of the program.
  // Note that this leaks memory; that is expected behavior.
  base::Histogram* counter =
      base::LinearHistogram::FactoryGet(
          name,
          1,
          boundary_value,
          boundary_value + 1,
          base::Histogram::kUmaTargetedHistogramFlag);
  counter->Add(sample);
}

// Logs a type quality metric.  The primary histogram name is constructed based
// on |base_name| and |experiment_id|.  The field-specific histogram name also
// factors in the |field_type|.  Logs a sample of |metric|, which should be in
// the range [0, |num_possible_metrics|).
void LogTypeQualityMetric(const std::string& base_name,
                          const int metric,
                          const int num_possible_metrics,
                          const AutofillFieldType field_type,
                          const std::string& experiment_id) {
  DCHECK(metric < num_possible_metrics);

  std::string histogram_name = base_name;
  if (!experiment_id.empty())
    histogram_name += "_" + experiment_id;
  LogUMAHistogramEnumeration(histogram_name, metric, num_possible_metrics);

  std::string sub_histogram_name = base_name + ".ByFieldType";
  if (!experiment_id.empty())
    sub_histogram_name += "_" + experiment_id;
  const int field_type_group_metric =
      GetFieldTypeGroupMetric(field_type, metric, num_possible_metrics);
  const int num_field_type_group_metrics =
      num_possible_metrics * NUM_FIELD_TYPE_GROUPS_FOR_METRICS;
  LogUMAHistogramEnumeration(sub_histogram_name,
                             field_type_group_metric,
                             num_field_type_group_metrics);
}

void LogServerExperimentId(const std::string& histogram_name,
                           const std::string& experiment_id) {
  ServerExperiment metric = UNKNOWN_EXPERIMENT;

  const std::string default_experiment_name =
      FormStructure(webkit_glue::FormData()).server_experiment_id();
  if (experiment_id.empty())
    metric = NO_EXPERIMENT;
  else if (experiment_id == "ar06")
    metric = ACCEPTANCE_RATIO_06;
  else if (experiment_id == "ar1")
    metric = ACCEPTANCE_RATIO_1;
  else if (experiment_id == "ar2")
    metric = ACCEPTANCE_RATIO_2;
  else if (experiment_id == "ar4")
    metric = ACCEPTANCE_RATIO_4;
  else if (experiment_id == "ar05wlr15")
    metric = ACCEPTANCE_RATIO_05_WINNER_LEAD_RATIO_15;
  else if (experiment_id == "ar05wlr25")
    metric = ACCEPTANCE_RATIO_05_WINNER_LEAD_RATIO_25;
  else if (experiment_id == "ar05wr15fs5")
    metric = ACCEPTANCE_RATIO_05_WINNER_LEAD_RATIO_15_MIN_FORM_SCORE_5;
  else if (experiment_id == "tbar1")
    metric = TOOLBAR_DATA_ONLY;
  else if (experiment_id == "ar04wr3fs4")
    metric = ACCEPTANCE_RATIO_04_WINNER_LEAD_RATIO_3_MIN_FORM_SCORE_4;
  else if (experiment_id == default_experiment_name)
    metric = NO_SERVER_RESPONSE;
  else if (experiment_id == "fp05")
    metric = PROBABILITY_PICKER_05;

  DCHECK(metric < NUM_SERVER_EXPERIMENTS);
  LogUMAHistogramEnumeration(histogram_name, metric, NUM_SERVER_EXPERIMENTS);
}

}  // namespace

AutofillMetrics::AutofillMetrics() {
}

AutofillMetrics::~AutofillMetrics() {
}

void AutofillMetrics::LogCreditCardInfoBarMetric(InfoBarMetric metric) const {
  DCHECK(metric < NUM_INFO_BAR_METRICS);

  UMA_HISTOGRAM_ENUMERATION("Autofill.CreditCardInfoBar", metric,
                            NUM_INFO_BAR_METRICS);
}

void AutofillMetrics::LogHeuristicTypePrediction(
    FieldTypeQualityMetric metric,
    AutofillFieldType field_type,
    const std::string& experiment_id) const {
  LogTypeQualityMetric("Autofill.Quality.HeuristicType",
                       metric, NUM_FIELD_TYPE_QUALITY_METRICS,
                       field_type, experiment_id);
}

void AutofillMetrics::LogOverallTypePrediction(
    FieldTypeQualityMetric metric,
    AutofillFieldType field_type,
    const std::string& experiment_id) const {
  LogTypeQualityMetric("Autofill.Quality.PredictedType",
                       metric, NUM_FIELD_TYPE_QUALITY_METRICS,
                       field_type, experiment_id);
}

void AutofillMetrics::LogServerTypePrediction(
    FieldTypeQualityMetric metric,
    AutofillFieldType field_type,
    const std::string& experiment_id) const {
  LogTypeQualityMetric("Autofill.Quality.ServerType",
                       metric, NUM_FIELD_TYPE_QUALITY_METRICS,
                       field_type, experiment_id);
}

void AutofillMetrics::LogQualityMetric(QualityMetric metric,
                                       const std::string& experiment_id) const {
  DCHECK(metric < NUM_QUALITY_METRICS);

  std::string histogram_name = "Autofill.Quality";
  if (!experiment_id.empty())
    histogram_name += "_" + experiment_id;

  LogUMAHistogramEnumeration(histogram_name, metric, NUM_QUALITY_METRICS);
}

void AutofillMetrics::LogServerQueryMetric(ServerQueryMetric metric) const {
  DCHECK(metric < NUM_SERVER_QUERY_METRICS);

  UMA_HISTOGRAM_ENUMERATION("Autofill.ServerQueryResponse", metric,
                            NUM_SERVER_QUERY_METRICS);
}

void AutofillMetrics::LogUserHappinessMetric(UserHappinessMetric metric) const {
  DCHECK(metric < NUM_USER_HAPPINESS_METRICS);

  UMA_HISTOGRAM_ENUMERATION("Autofill.UserHappiness", metric,
                            NUM_USER_HAPPINESS_METRICS);
}

void AutofillMetrics::LogFormFillDurationFromLoadWithAutofill(
    const base::TimeDelta& duration) const {
  UMA_HISTOGRAM_CUSTOM_TIMES("Autofill.FillDuration.FromLoad.WithAutofill",
                             duration,
                             base::TimeDelta::FromMilliseconds(100),
                             base::TimeDelta::FromMinutes(10),
                             50);
}

void AutofillMetrics::LogFormFillDurationFromLoadWithoutAutofill(
    const base::TimeDelta& duration) const {
  UMA_HISTOGRAM_CUSTOM_TIMES("Autofill.FillDuration.FromLoad.WithoutAutofill",
                             duration,
                             base::TimeDelta::FromMilliseconds(100),
                             base::TimeDelta::FromMinutes(10),
                             50);
}

void AutofillMetrics::LogFormFillDurationFromInteractionWithAutofill(
    const base::TimeDelta& duration) const {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Autofill.FillDuration.FromInteraction.WithAutofill",
      duration,
      base::TimeDelta::FromMilliseconds(100),
      base::TimeDelta::FromMinutes(10),
      50);
}

void AutofillMetrics::LogFormFillDurationFromInteractionWithoutAutofill(
    const base::TimeDelta& duration) const {
  UMA_HISTOGRAM_CUSTOM_TIMES(
       "Autofill.FillDuration.FromInteraction.WithoutAutofill",
       duration,
       base::TimeDelta::FromMilliseconds(100),
       base::TimeDelta::FromMinutes(10),
       50);
}

void AutofillMetrics::LogIsAutofillEnabledAtStartup(bool enabled) const {
  UMA_HISTOGRAM_BOOLEAN("Autofill.IsEnabled.Startup", enabled);
}

void AutofillMetrics::LogIsAutofillEnabledAtPageLoad(bool enabled) const {
  UMA_HISTOGRAM_BOOLEAN("Autofill.IsEnabled.PageLoad", enabled);
}

void AutofillMetrics::LogStoredProfileCount(size_t num_profiles) const {
  UMA_HISTOGRAM_COUNTS("Autofill.StoredProfileCount", num_profiles);
}

void AutofillMetrics::LogAddressSuggestionsCount(size_t num_suggestions) const {
  UMA_HISTOGRAM_COUNTS("Autofill.AddressSuggestionsCount", num_suggestions);
}

void AutofillMetrics::LogServerExperimentIdForQuery(
    const std::string& experiment_id) const {
  LogServerExperimentId("Autofill.ServerExperimentId.Query", experiment_id);
}

void AutofillMetrics::LogServerExperimentIdForUpload(
    const std::string& experiment_id) const {
  LogServerExperimentId("Autofill.ServerExperimentId.Upload", experiment_id);
}
