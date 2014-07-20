// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function_test_utils.h"

#include <string>

#include "base/file_path.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestFunctionDispatcherDelegate
    : public ExtensionFunctionDispatcher::Delegate {
 public:
  explicit TestFunctionDispatcherDelegate(Browser* browser) :
      browser_(browser) {}
  virtual ~TestFunctionDispatcherDelegate() {}

 private:
  virtual Browser* GetBrowser() OVERRIDE {
    return browser_;
  }

  virtual gfx::NativeView GetNativeViewOfHost() OVERRIDE {
    return NULL;
  }

  virtual TabContents* GetAssociatedTabContents() const OVERRIDE {
    return NULL;
  }

  Browser* browser_;
};

}  // namespace

namespace extension_function_test_utils {

base::Value* ParseJSON(const std::string& data) {
  const bool kAllowTrailingComma = false;
  return base::JSONReader::Read(data, kAllowTrailingComma);
}

base::ListValue* ParseList(const std::string& data) {
  scoped_ptr<base::Value> result(ParseJSON(data));
  if (result.get() && result->IsType(base::Value::TYPE_LIST))
    return static_cast<base::ListValue*>(result.release());
  else
    return NULL;
}

base::DictionaryValue* ParseDictionary(
    const std::string& data) {
  scoped_ptr<base::Value> result(ParseJSON(data));
  if (result.get() && result->IsType(base::Value::TYPE_DICTIONARY))
    return static_cast<base::DictionaryValue*>(result.release());
  else
    return NULL;
}

bool GetBoolean(base::DictionaryValue* val, const std::string& key) {
  bool result = false;
  if (!val->GetBoolean(key, &result))
      ADD_FAILURE() << key << " does not exist or is not a boolean.";
  return result;
}

int GetInteger(base::DictionaryValue* val, const std::string& key) {
  int result = 0;
  if (!val->GetInteger(key, &result))
    ADD_FAILURE() << key << " does not exist or is not an integer.";
  return result;
}

std::string GetString(base::DictionaryValue* val, const std::string& key) {
  std::string result;
  if (!val->GetString(key, &result))
    ADD_FAILURE() << key << " does not exist or is not a string.";
  return result;
}

base::DictionaryValue* ToDictionary(base::Value* val) {
  if (!val || !val->IsType(base::Value::TYPE_DICTIONARY))
    ADD_FAILURE() << "value is null or not a dictionary.";
  return static_cast<base::DictionaryValue*>(val);
}

scoped_refptr<Extension> CreateEmptyExtension() {
  std::string error;
  const FilePath test_extension_path;
  scoped_ptr<base::DictionaryValue> test_extension_value(
      ParseDictionary("{\"name\": \"Test\", \"version\": \"1.0\"}"));
  scoped_refptr<Extension> extension(Extension::Create(
      test_extension_path,
      Extension::INTERNAL,
      *test_extension_value.get(),
      Extension::NO_FLAGS,
      &error));
  if (!error.empty())
    ADD_FAILURE() << "Could not parse test extension " << error;
  return extension;
}

std::string RunFunctionAndReturnError(UIThreadExtensionFunction* function,
                                      const std::string& args,
                                      Browser* browser) {
  return RunFunctionAndReturnError(function, args, browser, NONE);
}
std::string RunFunctionAndReturnError(UIThreadExtensionFunction* function,
                                      const std::string& args,
                                      Browser* browser,
                                      RunFunctionFlags flags) {
  scoped_refptr<ExtensionFunction> function_owner(function);
  RunFunction(function, args, browser, flags);
  if (function->GetResultValue())
    ADD_FAILURE() << function->GetResult();
  return function->GetError();
}

base::Value* RunFunctionAndReturnResult(UIThreadExtensionFunction* function,
                                        const std::string& args,
                                        Browser* browser) {
  return RunFunctionAndReturnResult(function, args, browser, NONE);
}
base::Value* RunFunctionAndReturnResult(UIThreadExtensionFunction* function,
                                        const std::string& args,
                                        Browser* browser,
                                        RunFunctionFlags flags) {
  scoped_refptr<ExtensionFunction> function_owner(function);
  RunFunction(function, args, browser, flags);
  if (!function->GetError().empty())
    ADD_FAILURE() << "Unexpected error: " << function->GetError();
  if (!function->GetResultValue())
    ADD_FAILURE() << "No result value found";
  return function->GetResultValue()->DeepCopy();
}

void RunFunction(UIThreadExtensionFunction* function,
                 const std::string& args,
                 Browser* browser,
                 RunFunctionFlags flags) {
  scoped_ptr<base::ListValue> parsed_args(ParseList(args));
  if (!parsed_args.get()) {
    ADD_FAILURE() << "Could not parse extension function arguments: " << args;
  }
  function->SetArgs(parsed_args.get());

  TestFunctionDispatcherDelegate dispatcher_delegate(browser);
  ExtensionFunctionDispatcher dispatcher(
      browser->profile(), &dispatcher_delegate);
  function->set_dispatcher(dispatcher.AsWeakPtr());

  function->set_profile(browser->profile());
  function->set_include_incognito(flags & INCLUDE_INCOGNITO);
  function->Run();
}

} // namespace extension_function_test_utils
