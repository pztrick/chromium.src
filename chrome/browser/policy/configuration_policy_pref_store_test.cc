// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_pref_store_test.h"

#include <string>

#include "base/run_loop.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Return;
using testing::_;

namespace policy {

ConfigurationPolicyPrefStoreTest::ConfigurationPolicyPrefStoreTest()
    : handler_list_(GetChromePolicyDetailsCallback()) {
  EXPECT_CALL(provider_, IsInitializationComplete(_))
      .WillRepeatedly(Return(false));
  provider_.Init();
  providers_.push_back(&provider_);
  policy_service_.reset(new PolicyServiceImpl(
      providers_, PolicyServiceImpl::PreprocessCallback()));
  store_ = new ConfigurationPolicyPrefStore(
      policy_service_.get(), &handler_list_, POLICY_LEVEL_MANDATORY);
}

ConfigurationPolicyPrefStoreTest::~ConfigurationPolicyPrefStoreTest() {}

void ConfigurationPolicyPrefStoreTest::TearDown() {
  provider_.Shutdown();
}

void ConfigurationPolicyPrefStoreTest::UpdateProviderPolicy(
    const PolicyMap& policy) {
  provider_.UpdateChromePolicy(policy);
  base::RunLoop loop;
  loop.RunUntilIdle();
}

}  // namespace policy
