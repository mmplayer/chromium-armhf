// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/v8_unit_test.h"

#include "base/string_piece.h"
#include "base/stringprintf.h"

V8UnitTest::V8UnitTest() {}

V8UnitTest::~V8UnitTest() {}

void V8UnitTest::SetUp() {
  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();
  global->Set(v8::String::New("log"),
              v8::FunctionTemplate::New(&V8UnitTest::Log));
  context_ = v8::Context::New(NULL, global);
}

void V8UnitTest::SetGlobalStringVar(const std::string& var_name,
                                    const std::string& value) {
  v8::Context::Scope context_scope(context_);
  context_->Global()->Set(v8::String::New(var_name.c_str(), var_name.length()),
                          v8::String::New(value.c_str(), value.length()));
}

void V8UnitTest::ExecuteScriptInContext(const base::StringPiece& script_source,
                                        const base::StringPiece& script_name) {
  v8::Context::Scope context_scope(context_);
  v8::HandleScope handle_scope;
  v8::Handle<v8::String> source = v8::String::New(script_source.data(),
                                                  script_source.size());
  v8::Handle<v8::String> name = v8::String::New(script_name.data(),
                                                script_name.size());

  v8::TryCatch try_catch;
  v8::Handle<v8::Script> script = v8::Script::Compile(source, name);
  // Ensure the script compiled without errors.
  if (script.IsEmpty())
    FAIL() << ExceptionToString(try_catch);

  v8::Handle<v8::Value> result = script->Run();
  // Ensure the script ran without errors.
  if (result.IsEmpty())
    FAIL() << ExceptionToString(try_catch);
}

std::string V8UnitTest::ExceptionToString(const v8::TryCatch& try_catch) {
  std::string str;
  v8::HandleScope handle_scope;
  v8::String::Utf8Value exception(try_catch.Exception());
  v8::Local<v8::Message> message(try_catch.Message());
  if (message.IsEmpty()) {
    str.append(base::StringPrintf("%s\n", *exception));
  } else {
    v8::String::Utf8Value filename(message->GetScriptResourceName());
    int linenum = message->GetLineNumber();
    int colnum = message->GetStartColumn();
    str.append(base::StringPrintf(
        "%s:%i:%i %s\n", *filename, linenum, colnum, *exception));
    v8::String::Utf8Value sourceline(message->GetSourceLine());
    str.append(base::StringPrintf("%s\n", *sourceline));
  }
  return str;
}

void V8UnitTest::TestFunction(const std::string& function_name) {
  v8::Context::Scope context_scope(context_);
  v8::HandleScope handle_scope;

  v8::Handle<v8::Value> functionProperty =
      context_->Global()->Get(v8::String::New(function_name.c_str()));
  ASSERT_FALSE(functionProperty.IsEmpty());
  ASSERT_TRUE(functionProperty->IsFunction());
  v8::Handle<v8::Function> function =
      v8::Handle<v8::Function>::Cast(functionProperty);

  v8::TryCatch try_catch;
  v8::Handle<v8::Value> result = function->Call(context_->Global(), 0, NULL);
  // The test fails if an exception was thrown.
  if (result.IsEmpty())
    FAIL() << ExceptionToString(try_catch);
}

// static
v8::Handle<v8::Value> V8UnitTest::Log(const v8::Arguments& args) {
  std::string message;
  bool first = true;
  for (int i = 0; i < args.Length(); i++) {
    v8::HandleScope handle_scope;
    if (first) {
      first = false;
    } else {
      message += " ";
    }
    v8::String::Utf8Value str(args[i]);
    message += *str;
  }
  std::cout << message << "\n";
  return v8::Undefined();
}
