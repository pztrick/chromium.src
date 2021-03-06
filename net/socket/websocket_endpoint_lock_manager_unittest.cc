// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/websocket_endpoint_lock_manager.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// A StreamSocket implementation with no functionality at all.
// TODO(ricea): If you need to use this in another file, please move it to
// socket_test_util.h.
class FakeStreamSocket : public StreamSocket {
 public:
  FakeStreamSocket() {}

  // StreamSocket implementation
  int Connect(const CompletionCallback& callback) override {
    return ERR_FAILED;
  }

  void Disconnect() override { return; }

  bool IsConnected() const override { return false; }

  bool IsConnectedAndIdle() const override { return false; }

  int GetPeerAddress(IPEndPoint* address) const override { return ERR_FAILED; }

  int GetLocalAddress(IPEndPoint* address) const override { return ERR_FAILED; }

  const BoundNetLog& NetLog() const override { return bound_net_log_; }

  void SetSubresourceSpeculation() override { return; }
  void SetOmniboxSpeculation() override { return; }

  bool WasEverUsed() const override { return false; }

  bool UsingTCPFastOpen() const override { return false; }

  bool WasNpnNegotiated() const override { return false; }

  NextProto GetNegotiatedProtocol() const override { return kProtoUnknown; }

  bool GetSSLInfo(SSLInfo* ssl_info) override { return false; }

  // Socket implementation
  int Read(IOBuffer* buf,
           int buf_len,
           const CompletionCallback& callback) override {
    return ERR_FAILED;
  }

  int Write(IOBuffer* buf,
            int buf_len,
            const CompletionCallback& callback) override {
    return ERR_FAILED;
  }

  int SetReceiveBufferSize(int32 size) override { return ERR_FAILED; }

  int SetSendBufferSize(int32 size) override { return ERR_FAILED; }

 private:
  BoundNetLog bound_net_log_;

  DISALLOW_COPY_AND_ASSIGN(FakeStreamSocket);
};

class FakeWaiter : public WebSocketEndpointLockManager::Waiter {
 public:
  FakeWaiter() : called_(false) {}

  void GotEndpointLock() override {
    CHECK(!called_);
    called_ = true;
  }

  bool called() const { return called_; }

 private:
  bool called_;
};

class WebSocketEndpointLockManagerTest : public ::testing::Test {
 protected:
  WebSocketEndpointLockManagerTest()
      : instance_(WebSocketEndpointLockManager::GetInstance()) {}
  ~WebSocketEndpointLockManagerTest() override {
    // Permit any pending asynchronous unlock operations to complete.
    RunUntilIdle();
    // If this check fails then subsequent tests may fail.
    CHECK(instance_->IsEmpty());
  }

  WebSocketEndpointLockManager* instance() const { return instance_; }

  IPEndPoint DummyEndpoint() {
    IPAddressNumber ip_address_number;
    CHECK(ParseIPLiteralToNumber("127.0.0.1", &ip_address_number));
    return IPEndPoint(ip_address_number, 80);
  }

  void UnlockDummyEndpoint(int times) {
    for (int i = 0; i < times; ++i) {
      instance()->UnlockEndpoint(DummyEndpoint());
      RunUntilIdle();
    }
  }

  static void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  WebSocketEndpointLockManager* const instance_;
  ScopedWebSocketEndpointZeroUnlockDelay zero_unlock_delay_;
};

TEST_F(WebSocketEndpointLockManagerTest, GetInstanceWorks) {
  // All the work is done by the test framework.
}

TEST_F(WebSocketEndpointLockManagerTest, LockEndpointReturnsOkOnce) {
  FakeWaiter waiters[2];
  EXPECT_EQ(OK, instance()->LockEndpoint(DummyEndpoint(), &waiters[0]));
  EXPECT_EQ(ERR_IO_PENDING,
            instance()->LockEndpoint(DummyEndpoint(), &waiters[1]));

  UnlockDummyEndpoint(2);
}

TEST_F(WebSocketEndpointLockManagerTest, GotEndpointLockNotCalledOnOk) {
  FakeWaiter waiter;
  EXPECT_EQ(OK, instance()->LockEndpoint(DummyEndpoint(), &waiter));
  RunUntilIdle();
  EXPECT_FALSE(waiter.called());

  UnlockDummyEndpoint(1);
}

TEST_F(WebSocketEndpointLockManagerTest, GotEndpointLockNotCalledImmediately) {
  FakeWaiter waiters[2];
  EXPECT_EQ(OK, instance()->LockEndpoint(DummyEndpoint(), &waiters[0]));
  EXPECT_EQ(ERR_IO_PENDING,
            instance()->LockEndpoint(DummyEndpoint(), &waiters[1]));
  RunUntilIdle();
  EXPECT_FALSE(waiters[1].called());

  UnlockDummyEndpoint(2);
}

TEST_F(WebSocketEndpointLockManagerTest, GotEndpointLockCalledWhenUnlocked) {
  FakeWaiter waiters[2];
  EXPECT_EQ(OK, instance()->LockEndpoint(DummyEndpoint(), &waiters[0]));
  EXPECT_EQ(ERR_IO_PENDING,
            instance()->LockEndpoint(DummyEndpoint(), &waiters[1]));
  instance()->UnlockEndpoint(DummyEndpoint());
  RunUntilIdle();
  EXPECT_TRUE(waiters[1].called());

  UnlockDummyEndpoint(1);
}

TEST_F(WebSocketEndpointLockManagerTest,
       EndpointUnlockedIfWaiterAlreadyDeleted) {
  FakeWaiter first_lock_holder;
  EXPECT_EQ(OK, instance()->LockEndpoint(DummyEndpoint(), &first_lock_holder));

  {
    FakeWaiter short_lived_waiter;
    EXPECT_EQ(ERR_IO_PENDING,
              instance()->LockEndpoint(DummyEndpoint(), &short_lived_waiter));
  }

  instance()->UnlockEndpoint(DummyEndpoint());
  RunUntilIdle();

  FakeWaiter second_lock_holder;
  EXPECT_EQ(OK, instance()->LockEndpoint(DummyEndpoint(), &second_lock_holder));

  UnlockDummyEndpoint(1);
}

TEST_F(WebSocketEndpointLockManagerTest, RememberSocketWorks) {
  FakeWaiter waiters[2];
  FakeStreamSocket dummy_socket;
  EXPECT_EQ(OK, instance()->LockEndpoint(DummyEndpoint(), &waiters[0]));
  EXPECT_EQ(ERR_IO_PENDING,
            instance()->LockEndpoint(DummyEndpoint(), &waiters[1]));

  instance()->RememberSocket(&dummy_socket, DummyEndpoint());
  instance()->UnlockSocket(&dummy_socket);
  RunUntilIdle();
  EXPECT_TRUE(waiters[1].called());

  UnlockDummyEndpoint(1);
}

// UnlockEndpoint() should cause any sockets remembered for this endpoint
// to be forgotten.
TEST_F(WebSocketEndpointLockManagerTest, SocketAssociationForgottenOnUnlock) {
  FakeWaiter waiter;
  FakeStreamSocket dummy_socket;

  EXPECT_EQ(OK, instance()->LockEndpoint(DummyEndpoint(), &waiter));
  instance()->RememberSocket(&dummy_socket, DummyEndpoint());
  instance()->UnlockEndpoint(DummyEndpoint());
  RunUntilIdle();
  EXPECT_TRUE(instance()->IsEmpty());
}

// When ownership of the endpoint is passed to a new waiter, the new waiter can
// call RememberSocket() again.
TEST_F(WebSocketEndpointLockManagerTest, NextWaiterCanCallRememberSocketAgain) {
  FakeWaiter waiters[2];
  FakeStreamSocket dummy_sockets[2];
  EXPECT_EQ(OK, instance()->LockEndpoint(DummyEndpoint(), &waiters[0]));
  EXPECT_EQ(ERR_IO_PENDING,
            instance()->LockEndpoint(DummyEndpoint(), &waiters[1]));

  instance()->RememberSocket(&dummy_sockets[0], DummyEndpoint());
  instance()->UnlockEndpoint(DummyEndpoint());
  RunUntilIdle();
  EXPECT_TRUE(waiters[1].called());
  instance()->RememberSocket(&dummy_sockets[1], DummyEndpoint());

  UnlockDummyEndpoint(1);
}

// Calling UnlockSocket() after UnlockEndpoint() does nothing.
TEST_F(WebSocketEndpointLockManagerTest,
       UnlockSocketAfterUnlockEndpointDoesNothing) {
  FakeWaiter waiters[3];
  FakeStreamSocket dummy_socket;

  EXPECT_EQ(OK, instance()->LockEndpoint(DummyEndpoint(), &waiters[0]));
  EXPECT_EQ(ERR_IO_PENDING,
            instance()->LockEndpoint(DummyEndpoint(), &waiters[1]));
  EXPECT_EQ(ERR_IO_PENDING,
            instance()->LockEndpoint(DummyEndpoint(), &waiters[2]));
  instance()->RememberSocket(&dummy_socket, DummyEndpoint());
  instance()->UnlockEndpoint(DummyEndpoint());
  instance()->UnlockSocket(&dummy_socket);
  RunUntilIdle();
  EXPECT_TRUE(waiters[1].called());
  EXPECT_FALSE(waiters[2].called());

  UnlockDummyEndpoint(2);
}

// UnlockEndpoint() should always be asynchronous.
TEST_F(WebSocketEndpointLockManagerTest, UnlockEndpointIsAsynchronous) {
  FakeWaiter waiters[2];
  EXPECT_EQ(OK, instance()->LockEndpoint(DummyEndpoint(), &waiters[0]));
  EXPECT_EQ(ERR_IO_PENDING,
            instance()->LockEndpoint(DummyEndpoint(), &waiters[1]));

  instance()->UnlockEndpoint(DummyEndpoint());
  EXPECT_FALSE(waiters[1].called());
  RunUntilIdle();
  EXPECT_TRUE(waiters[1].called());

  UnlockDummyEndpoint(1);
}

// UnlockEndpoint() should normally have a delay.
TEST_F(WebSocketEndpointLockManagerTest, UnlockEndpointIsDelayed) {
  const base::TimeDelta one_millisecond = base::TimeDelta::FromMilliseconds(1);
  instance()->SetUnlockDelayForTesting(one_millisecond);
  FakeWaiter waiters[2];
  EXPECT_EQ(OK, instance()->LockEndpoint(DummyEndpoint(), &waiters[0]));
  EXPECT_EQ(ERR_IO_PENDING,
            instance()->LockEndpoint(DummyEndpoint(), &waiters[1]));

  instance()->UnlockEndpoint(DummyEndpoint());
  RunUntilIdle();
  EXPECT_FALSE(waiters[1].called());
  base::RunLoop run_loop;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), one_millisecond);
  run_loop.Run();
  EXPECT_TRUE(waiters[1].called());
  instance()->SetUnlockDelayForTesting(base::TimeDelta());
  UnlockDummyEndpoint(1);
}

}  // namespace

}  // namespace net
