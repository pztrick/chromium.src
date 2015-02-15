// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"

#include "base/base64.h"
#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/state_store.h"
#include "net/cert/x509_certificate.h"

using content::BrowserThread;

namespace chromeos {

namespace {

const char kErrorKeyNotAllowedForSigning[] =
    "This key is not allowed for signing. Either it was used for signing "
    "before or it was not correctly generated.";
const char kStateStorePlatformKeys[] = "PlatformKeys";

scoped_ptr<base::StringValue> GetPublicKeyValue(
    const std::string& public_key_spki_der) {
  std::string public_key_spki_der_b64;
  base::Base64Encode(public_key_spki_der, &public_key_spki_der_b64);
  return make_scoped_ptr(new base::StringValue(public_key_spki_der_b64));
}

void RunGenerateKeyCallback(
    const PlatformKeysService::GenerateKeyCallback& callback,
    const std::string& public_key_spki_der) {
  callback.Run(public_key_spki_der, std::string() /* no error */);
}

// Callback used by |PlatformKeysService::Sign|.
// Is called with the old validity of |public_key_spki_der| (or false if an
// error occurred during reading the StateStore). If allowed, starts the actual
// signing operation which will call back |callback|. If not allowed, calls
// |callback| with an error.
void CheckValidityAndSign(const std::string& token_id,
                          const std::string& data,
                          const std::string& public_key,
                          bool sign_direct_pkcs_padded,
                          platform_keys::HashAlgorithm hash_algorithm,
                          const PlatformKeysService::SignCallback& callback,
                          content::BrowserContext* browser_context,
                          bool key_is_valid) {
  if (!key_is_valid) {
    callback.Run(std::string() /* no signature */,
                 kErrorKeyNotAllowedForSigning);
    return;
  }
  if (sign_direct_pkcs_padded) {
    platform_keys::subtle::SignRSAPKCS1Raw(token_id, data, public_key, callback,
                                           browser_context);
  } else {
    platform_keys::subtle::SignRSAPKCS1Digest(
        token_id, data, public_key, hash_algorithm, callback, browser_context);
  }
}

}  // namespace

PlatformKeysService::PlatformKeysService(
    content::BrowserContext* browser_context,
    extensions::StateStore* state_store)
    : browser_context_(browser_context),
      state_store_(state_store),
      weak_factory_(this) {
  DCHECK(state_store);
}

PlatformKeysService::~PlatformKeysService() {
}

void PlatformKeysService::DisablePermissionCheckForTesting() {
  permission_check_enabled_ = false;
}

void PlatformKeysService::GenerateRSAKey(const std::string& token_id,
                                         unsigned int modulus_length,
                                         const std::string& extension_id,
                                         const GenerateKeyCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  platform_keys::subtle::GenerateRSAKey(
      token_id,
      modulus_length,
      base::Bind(&PlatformKeysService::GenerateRSAKeyCallback,
                 weak_factory_.GetWeakPtr(),
                 extension_id,
                 callback),
      browser_context_);
}

void PlatformKeysService::SignRSAPKCS1Digest(
    const std::string& token_id,
    const std::string& data,
    const std::string& public_key,
    platform_keys::HashAlgorithm hash_algorithm,
    const std::string& extension_id,
    const SignCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ReadValidityAndInvalidateKey(
      extension_id, public_key,
      base::Bind(&CheckValidityAndSign, token_id, data, public_key,
                 false /* digest before signing */, hash_algorithm, callback,
                 browser_context_));
}

void PlatformKeysService::SignRSAPKCS1Raw(const std::string& token_id,
                                          const std::string& data,
                                          const std::string& public_key,
                                          const std::string& extension_id,
                                          const SignCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ReadValidityAndInvalidateKey(
      extension_id, public_key,
      base::Bind(&CheckValidityAndSign, token_id, data, public_key,
                 true /* sign directly without hashing */,
                 platform_keys::HASH_ALGORITHM_NONE, callback,
                 browser_context_));
}

void PlatformKeysService::SelectClientCertificates(
    const platform_keys::ClientCertificateRequest& request,
    const std::string& extension_id,
    const SelectCertificatesCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  platform_keys::subtle::SelectClientCertificates(
      request,
      base::Bind(&PlatformKeysService::SelectClientCertificatesCallback,
                 weak_factory_.GetWeakPtr(), extension_id, callback),
      browser_context_);
}

void PlatformKeysService::RegisterPublicKey(
    const std::string& extension_id,
    const std::string& public_key_spki_der,
    const base::Closure& callback) {
  GetPlatformKeysOfExtension(
      extension_id,
      base::Bind(&PlatformKeysService::RegisterPublicKeyGotPlatformKeys,
                 weak_factory_.GetWeakPtr(),
                 extension_id,
                 public_key_spki_der,
                 callback));
}

void PlatformKeysService::ReadValidityAndInvalidateKey(
    const std::string& extension_id,
    const std::string& public_key_spki_der,
    const base::Callback<void(bool)>& callback) {
  GetPlatformKeysOfExtension(extension_id,
                             base::Bind(&PlatformKeysService::InvalidateKey,
                                        weak_factory_.GetWeakPtr(),
                                        extension_id,
                                        public_key_spki_der,
                                        callback));
}

void PlatformKeysService::GetPlatformKeysOfExtension(
    const std::string& extension_id,
    const GetPlatformKeysCallback& callback) {
  state_store_->GetExtensionValue(
      extension_id, kStateStorePlatformKeys,
      base::Bind(&PlatformKeysService::GotPlatformKeysOfExtension,
                 weak_factory_.GetWeakPtr(), extension_id, callback));
}

void PlatformKeysService::SetPlatformKeysOfExtension(
    const std::string& extension_id,
    scoped_ptr<base::ListValue> platform_keys) {
  state_store_->SetExtensionValue(extension_id, kStateStorePlatformKeys,
                                  platform_keys.Pass());
}

void PlatformKeysService::GenerateRSAKeyCallback(
    const std::string& extension_id,
    const GenerateKeyCallback& callback,
    const std::string& public_key_spki_der,
    const std::string& error_message) {
  if (!error_message.empty()) {
    callback.Run(std::string() /* no public key */, error_message);
    return;
  }
  base::Closure wrapped_callback(
      base::Bind(&RunGenerateKeyCallback, callback, public_key_spki_der));
  RegisterPublicKey(extension_id, public_key_spki_der, wrapped_callback);
}

void PlatformKeysService::SelectClientCertificatesCallback(
    const std::string& extension_id,
    const SelectCertificatesCallback& callback,
    scoped_ptr<net::CertificateList> matches,
    const std::string& error_message) {
  if (permission_check_enabled_)
    matches->clear();

  // TODO(pneubeck): Remove all certs that the extension doesn't have access to.
  callback.Run(matches.Pass(), error_message);
}

void PlatformKeysService::RegisterPublicKeyGotPlatformKeys(
    const std::string& extension_id,
    const std::string& public_key_spki_der,
    const base::Closure& callback,
    scoped_ptr<base::ListValue> platform_keys) {
  scoped_ptr<base::StringValue> key_value(
      GetPublicKeyValue(public_key_spki_der));

  DCHECK(platform_keys->end() == platform_keys->Find(*key_value))
      << "Keys are assumed to be generated and not to be registered multiple "
         "times.";
  platform_keys->Append(key_value.release());
  SetPlatformKeysOfExtension(extension_id, platform_keys.Pass());
  callback.Run();
}

void PlatformKeysService::InvalidateKey(
    const std::string& extension_id,
    const std::string& public_key_spki_der,
    const base::Callback<void(bool)>& callback,
    scoped_ptr<base::ListValue> platform_keys) {
  scoped_ptr<base::StringValue> key_value(
      GetPublicKeyValue(public_key_spki_der));

  size_t index = 0;
  // If the key is found in |platform_keys|, it's valid for the extension to use
  // it for signing.
  bool key_was_valid = platform_keys->Remove(*key_value, &index);

  if (key_was_valid) {
    // Persist that the key is now invalid.
    SetPlatformKeysOfExtension(extension_id, platform_keys.Pass());
  }

  if (permission_check_enabled_) {
    // If permission checks are enabled, pass back the key permission (before
    // it was removed above).
    callback.Run(key_was_valid);
  } else {
    // Otherwise just allow signing with the key (which is enabled for testing
    // only).
    callback.Run(true);
  }
}

void PlatformKeysService::GotPlatformKeysOfExtension(
    const std::string& extension_id,
    const GetPlatformKeysCallback& callback,
    scoped_ptr<base::Value> value) {
  if (!value)
    value.reset(new base::ListValue);

  base::ListValue* keys = NULL;
  if (!value->GetAsList(&keys)) {
    LOG(ERROR) << "Found a value of wrong type.";

    keys = new base::ListValue;
    value.reset(keys);
  }

  ignore_result(value.release());
  callback.Run(make_scoped_ptr(keys));
}

}  // namespace chromeos
