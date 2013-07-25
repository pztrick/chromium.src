// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "content/renderer/pepper/mock_plugin_delegate.h"
#include "content/renderer/pepper/pepper_device_enumeration_host_helper.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_message_utils.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/proxy/resource_message_test_sink.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class TestPluginDelegate : public webkit::ppapi::MockPluginDelegate {
 public:
  TestPluginDelegate() : last_used_id_(0) {
  }

  virtual ~TestPluginDelegate() {
    CHECK(callbacks_.empty());
  }

  virtual int EnumerateDevices(
      PP_DeviceType_Dev /* type */,
      const EnumerateDevicesCallback& callback) OVERRIDE {
    last_used_id_++;
    callbacks_[last_used_id_] = callback;
    return last_used_id_;
  }

  virtual void StopEnumerateDevices(int request_id) OVERRIDE {
    std::map<int, EnumerateDevicesCallback>::iterator iter =
        callbacks_.find(request_id);
    CHECK(iter != callbacks_.end());
    callbacks_.erase(iter);
  }

  // Returns false if |request_id| is not found.
  bool SimulateEnumerateResult(
      int request_id,
      bool succeeded,
      const std::vector<ppapi::DeviceRefData>& devices) {
    std::map<int, EnumerateDevicesCallback>::iterator iter =
        callbacks_.find(request_id);
    if (iter == callbacks_.end())
      return false;

    iter->second.Run(request_id, succeeded, devices);
    return true;
  }

  size_t GetRegisteredCallbackCount() const { return callbacks_.size(); }

  int last_used_id() const { return last_used_id_; }

 private:
  std::map<int, EnumerateDevicesCallback> callbacks_;
  int last_used_id_;

  DISALLOW_COPY_AND_ASSIGN(TestPluginDelegate);
};

class TestResourceHost : public ppapi::host::ResourceHost,
                         public PepperDeviceEnumerationHostHelper::Delegate {
 public:
  TestResourceHost(ppapi::host::PpapiHost* host,
                   PP_Instance instance,
                   PP_Resource resource,
                   webkit::ppapi::PluginDelegate* delegate)
      : ResourceHost(host, instance, resource),
        delegate_(delegate) {
  }

  virtual ~TestResourceHost() {}

  virtual webkit::ppapi::PluginDelegate* GetPluginDelegate() OVERRIDE {
    return delegate_;
  }

 private:
  webkit::ppapi::PluginDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(TestResourceHost);
};

class PepperDeviceEnumerationHostHelperTest : public testing::Test {
 protected:
  PepperDeviceEnumerationHostHelperTest()
      : ppapi_host_(&sink_, ppapi::PpapiPermissions()),
        resource_host_(&ppapi_host_, 12345, 67890, &delegate_),
        device_enumeration_(&resource_host_, &resource_host_,
                            PP_DEVICETYPE_DEV_AUDIOCAPTURE) {
  }

  virtual ~PepperDeviceEnumerationHostHelperTest() {}

  void SimulateMonitorDeviceChangeReceived(uint32_t callback_id) {
    PpapiHostMsg_DeviceEnumeration_MonitorDeviceChange msg(callback_id);
    ppapi::proxy::ResourceMessageCallParams call_params(
        resource_host_.pp_resource(), 123);
    ppapi::host::HostMessageContext context(call_params);
    int32_t result = PP_ERROR_FAILED;
    ASSERT_TRUE(device_enumeration_.HandleResourceMessage(
        msg, &context, &result));
    EXPECT_EQ(PP_OK, result);
  }

  void CheckNotifyDeviceChangeMessage(
      uint32_t callback_id,
      const std::vector<ppapi::DeviceRefData>& expected) {
    ppapi::proxy::ResourceMessageReplyParams reply_params;
    IPC::Message reply_msg;
    ASSERT_TRUE(sink_.GetFirstResourceReplyMatching(
        PpapiPluginMsg_DeviceEnumeration_NotifyDeviceChange::ID,
        &reply_params, &reply_msg));
    sink_.ClearMessages();

    EXPECT_EQ(PP_OK, reply_params.result());

    uint32_t reply_callback_id = 0;
    std::vector<ppapi::DeviceRefData> reply_data;
    ASSERT_TRUE(ppapi::UnpackMessage<
        PpapiPluginMsg_DeviceEnumeration_NotifyDeviceChange>(
            reply_msg, &reply_callback_id, &reply_data));
    EXPECT_EQ(callback_id, reply_callback_id);
    EXPECT_EQ(expected, reply_data);
  }

  TestPluginDelegate delegate_;
  ppapi::proxy::ResourceMessageTestSink sink_;
  ppapi::host::PpapiHost ppapi_host_;
  TestResourceHost resource_host_;
  PepperDeviceEnumerationHostHelper device_enumeration_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PepperDeviceEnumerationHostHelperTest);
};

}  // namespace

TEST_F(PepperDeviceEnumerationHostHelperTest, EnumerateDevices) {
  PpapiHostMsg_DeviceEnumeration_EnumerateDevices msg;
  ppapi::proxy::ResourceMessageCallParams call_params(
      resource_host_.pp_resource(), 123);
  ppapi::host::HostMessageContext context(call_params);
  int32_t result = PP_ERROR_FAILED;
  ASSERT_TRUE(device_enumeration_.HandleResourceMessage(msg, &context,
                                                        &result));
  EXPECT_EQ(PP_OK_COMPLETIONPENDING, result);

  EXPECT_EQ(1U, delegate_.GetRegisteredCallbackCount());
  int request_id = delegate_.last_used_id();

  std::vector<ppapi::DeviceRefData> data;
  ppapi::DeviceRefData data_item;
  data_item.type = PP_DEVICETYPE_DEV_AUDIOCAPTURE;
  data_item.name = "name_1";
  data_item.id = "id_1";
  data.push_back(data_item);
  data_item.type = PP_DEVICETYPE_DEV_VIDEOCAPTURE;
  data_item.name = "name_2";
  data_item.id = "id_2";
  data.push_back(data_item);
  ASSERT_TRUE(delegate_.SimulateEnumerateResult(request_id, true, data));

  // StopEnumerateDevices() should have been called since the EnumerateDevices
  // message is not a persistent request.
  EXPECT_EQ(0U, delegate_.GetRegisteredCallbackCount());

  // A reply message should have been sent to the test sink.
  ppapi::proxy::ResourceMessageReplyParams reply_params;
  IPC::Message reply_msg;
  ASSERT_TRUE(sink_.GetFirstResourceReplyMatching(
      PpapiPluginMsg_DeviceEnumeration_EnumerateDevicesReply::ID,
      &reply_params, &reply_msg));

  EXPECT_EQ(call_params.sequence(), reply_params.sequence());
  EXPECT_EQ(PP_OK, reply_params.result());

  std::vector<ppapi::DeviceRefData> reply_data;
  ASSERT_TRUE(ppapi::UnpackMessage<
      PpapiPluginMsg_DeviceEnumeration_EnumerateDevicesReply>(
          reply_msg, &reply_data));
  EXPECT_EQ(data, reply_data);
}

TEST_F(PepperDeviceEnumerationHostHelperTest, MonitorDeviceChange) {
  uint32_t callback_id = 456;
  SimulateMonitorDeviceChangeReceived(callback_id);

  EXPECT_EQ(1U, delegate_.GetRegisteredCallbackCount());
  int request_id = delegate_.last_used_id();

  std::vector<ppapi::DeviceRefData> data;
  ASSERT_TRUE(delegate_.SimulateEnumerateResult(request_id, true, data));

  // StopEnumerateDevices() shouldn't be called because the MonitorDeviceChange
  // message is a persistent request.
  EXPECT_EQ(1U, delegate_.GetRegisteredCallbackCount());

  CheckNotifyDeviceChangeMessage(callback_id, data);

  ppapi::DeviceRefData data_item;
  data_item.type = PP_DEVICETYPE_DEV_AUDIOCAPTURE;
  data_item.name = "name_1";
  data_item.id = "id_1";
  data.push_back(data_item);
  data_item.type = PP_DEVICETYPE_DEV_VIDEOCAPTURE;
  data_item.name = "name_2";
  data_item.id = "id_2";
  data.push_back(data_item);
  ASSERT_TRUE(delegate_.SimulateEnumerateResult(request_id, true, data));
  EXPECT_EQ(1U, delegate_.GetRegisteredCallbackCount());

  CheckNotifyDeviceChangeMessage(callback_id, data);

  uint32_t callback_id2 = 789;
  SimulateMonitorDeviceChangeReceived(callback_id2);

  // StopEnumerateDevice() should have been called for the previous request.
  EXPECT_EQ(1U, delegate_.GetRegisteredCallbackCount());
  int request_id2 = delegate_.last_used_id();

  data_item.type = PP_DEVICETYPE_DEV_AUDIOCAPTURE;
  data_item.name = "name_3";
  data_item.id = "id_3";
  data.push_back(data_item);
  ASSERT_TRUE(delegate_.SimulateEnumerateResult(request_id2, true, data));

  CheckNotifyDeviceChangeMessage(callback_id2, data);

  PpapiHostMsg_DeviceEnumeration_StopMonitoringDeviceChange msg;
  ppapi::proxy::ResourceMessageCallParams call_params(
      resource_host_.pp_resource(), 123);
  ppapi::host::HostMessageContext context(call_params);
  int32_t result = PP_ERROR_FAILED;
  ASSERT_TRUE(device_enumeration_.HandleResourceMessage(
      msg, &context, &result));
  EXPECT_EQ(PP_OK, result);

  EXPECT_EQ(0U, delegate_.GetRegisteredCallbackCount());
}

}  // namespace content
