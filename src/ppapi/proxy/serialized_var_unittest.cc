// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppapi_proxy_test.h"

#include "ppapi/proxy/serialized_var.h"

namespace ppapi {
namespace proxy {

namespace {

PP_Var MakeStringVar(int64_t string_id) {
  PP_Var ret;
  ret.type = PP_VARTYPE_STRING;
  ret.value.as_id = string_id;
  return ret;
}

PP_Var MakeObjectVar(int64_t object_id) {
  PP_Var ret;
  ret.type = PP_VARTYPE_OBJECT;
  ret.value.as_id = object_id;
  return ret;
}

class SerializedVarTest : public PluginProxyTest {
 public:
  SerializedVarTest() {}
};

}  // namespace

// Tests output arguments in the plugin. This is when the host calls into the
// plugin and the plugin returns something via an out param, like an exception.
TEST_F(SerializedVarTest, PluginSerializedVarOutParam) {
  PP_Var host_object = MakeObjectVar(0x31337);

  // Start tracking this object in the plugin.
  PP_Var plugin_object = var_tracker().ReceiveObjectPassRef(
      host_object, plugin_dispatcher());
  EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_object));

  {
    SerializedVar sv;
    {
      // The "OutParam" does its work in its destructor, it will write the
      // information to the SerializedVar we passed in the constructor.
      SerializedVarOutParam out_param(&sv);
      *out_param.OutParam(plugin_dispatcher()) = plugin_object;
    }

    // The object should have transformed the plugin object back to the host
    // object ID. Nothing in the var tracker should have changed yet, and no
    // messages should have been sent.
    SerializedVarTestReader reader(sv);
    EXPECT_EQ(host_object.value.as_id, reader.GetIncompleteVar().value.as_id);
    EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_object));
    EXPECT_EQ(0u, sink().message_count());
  }

  // The out param should have done an "end send pass ref" on the plugin
  // var serialization rules, which should have in turn released the reference
  // in the var tracker. Since we only had one reference, this should have sent
  // a release to the browser.
  EXPECT_EQ(-1, var_tracker().GetRefCountForObject(plugin_object));
  EXPECT_EQ(1u, sink().message_count());

  // We don't bother validating that message since it's nontrivial and the
  // PluginVarTracker test has cases that cover that this message is correct.
}

// Tests the case that the plugin receives the same var twice as an input
// parameter (not passing ownership).
TEST_F(SerializedVarTest, PluginReceiveInput) {
  PP_Var host_object = MakeObjectVar(0x31337);

  PP_Var plugin_object;
  {
    // Receive the first param, we should be tracking it with no refcount, and
    // no messages sent.
    SerializedVarTestConstructor input1(host_object);
    SerializedVarReceiveInput receive_input(input1);
    plugin_object = receive_input.Get(plugin_dispatcher());
    EXPECT_EQ(0, var_tracker().GetRefCountForObject(plugin_object));
    EXPECT_EQ(0u, sink().message_count());

    // Receive the second param, it should be resolved to the same plugin
    // object and there should still be no refcount.
    SerializedVarTestConstructor input2(host_object);
    SerializedVarReceiveInput receive_input2(input2);
    PP_Var plugin_object2 = receive_input2.Get(plugin_dispatcher());
    EXPECT_EQ(plugin_object.value.as_id, plugin_object2.value.as_id);
    EXPECT_EQ(0, var_tracker().GetRefCountForObject(plugin_object));
    EXPECT_EQ(0u, sink().message_count());

    // Take a reference to the object, as if the plugin was using it, and then
    // release it, we should still be tracking the object since the
    // ReceiveInputs keep the "track_with_no_reference_count" alive until
    // they're destroyed.
    var_tracker().AddRefVar(plugin_object);
    EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_object));
    var_tracker().ReleaseVar(plugin_object);
    EXPECT_EQ(0, var_tracker().GetRefCountForObject(plugin_object));
    EXPECT_EQ(2u, sink().message_count());
  }

  // Since we didn't keep any refs to the objects, it should have freed the
  // object.
  EXPECT_EQ(-1, var_tracker().GetRefCountForObject(plugin_object));
}

// Tests the plugin receiving a var as a return value from the browser
// two different times (passing ownership).
TEST_F(SerializedVarTest, PluginReceiveReturn) {
  PP_Var host_object = MakeObjectVar(0x31337);

  PP_Var plugin_object;
  {
    // Receive the first param, we should be tracking it with a refcount of 1.
    SerializedVarTestConstructor input1(host_object);
    ReceiveSerializedVarReturnValue receive_input(input1);
    plugin_object = receive_input.Return(plugin_dispatcher());
    EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_object));
    EXPECT_EQ(0u, sink().message_count());

    // Receive the second param, it should be resolved to the same plugin
    // object and there should be a plugin refcount of 2. There should have
    // been an IPC message sent that released the duplicated ref in the browser
    // (so both of our refs are represented by one in the browser).
    SerializedVarTestConstructor input2(host_object);
    ReceiveSerializedVarReturnValue receive_input2(input2);
    PP_Var plugin_object2 = receive_input2.Return(plugin_dispatcher());
    EXPECT_EQ(plugin_object.value.as_id, plugin_object2.value.as_id);
    EXPECT_EQ(2, var_tracker().GetRefCountForObject(plugin_object));
    EXPECT_EQ(1u, sink().message_count());
  }

  // The ReceiveSerializedVarReturnValue destructor shouldn't have affected
  // the refcount or sent any messages.
  EXPECT_EQ(2, var_tracker().GetRefCountForObject(plugin_object));
  EXPECT_EQ(1u, sink().message_count());

  // Manually release one refcount, it shouldn't have sent any more messages.
  var_tracker().ReleaseVar(plugin_object);
  EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_object));
  EXPECT_EQ(1u, sink().message_count());

  // Manually release the last refcount, it should have freed it and sent a
  // release message to the browser.
  var_tracker().ReleaseVar(plugin_object);
  EXPECT_EQ(-1, var_tracker().GetRefCountForObject(plugin_object));
  EXPECT_EQ(2u, sink().message_count());
}

// Returns a value from the browser to the plugin, then return that one ref
// back to the browser.
TEST_F(SerializedVarTest, PluginReturnValue) {
  PP_Var host_object = MakeObjectVar(0x31337);

  PP_Var plugin_object;
  {
    // Receive the param in the plugin.
    SerializedVarTestConstructor input1(host_object);
    ReceiveSerializedVarReturnValue receive_input(input1);
    plugin_object = receive_input.Return(plugin_dispatcher());
    EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_object));
    EXPECT_EQ(0u, sink().message_count());
  }

  {
    // Now return to the browser.
    SerializedVar output;
    SerializedVarReturnValue return_output(&output);
    return_output.Return(plugin_dispatcher(), plugin_object);

    // The ref in the plugin should be alive until the ReturnValue goes out of
    // scope, since the release needs to be after the browser processes the
    // message.
    EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_object));
  }

  // When the ReturnValue object goes out of scope, it should have sent a
  // release message to the browser.
  EXPECT_EQ(-1, var_tracker().GetRefCountForObject(plugin_object));
  EXPECT_EQ(1u, sink().message_count());
}

}  // namespace proxy
}  // namespace ppapi
