/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentSignatureVerifier.h"

#include "BRNameMatchingPolicy.h"
#include "SharedCertVerifier.h"
#include "cryptohi.h"
#include "keyhi.h"
#include "mozilla/Assertions.h"
#include "mozilla/Base64.h"
#include "mozilla/Casting.h"
#include "mozilla/Unused.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsISupportsPriority.h"
#include "nsIURI.h"
#include "nsNSSComponent.h"
#include "nsPromiseFlatString.h"
#include "nsSecurityHeaderParser.h"
#include "nsStreamUtils.h"
#include "nsWhitespaceTokenizer.h"
#include "mozpkix/pkix.h"
#include "mozpkix/pkixtypes.h"
#include "secerr.h"

NS_IMPL_ISUPPORTS(ContentSignatureVerifier,
                  nsIContentSignatureVerifier,
                  nsIInterfaceRequestor,
                  nsIStreamListener)

using namespace mozilla;
using namespace mozilla::pkix;
using namespace mozilla::psm;

static LazyLogModule gCSVerifierPRLog("ContentSignatureVerifier");
#define CSVerifier_LOG(args) MOZ_LOG(gCSVerifierPRLog, LogLevel::Debug, args)

// Content-Signature prefix
const nsLiteralCString kPREFIX = NS_LITERAL_CSTRING("Content-Signature:\x00");

NS_IMETHODIMP
ContentSignatureVerifier::VerifyContentSignature(
  const nsACString& aData, const nsACString& aCSHeader,
  const nsACString& aCertChain, const nsACString& aName, bool* _retval)
{
  NS_ENSURE_ARG(_retval);
  nsresult rv = CreateContext(aData, aCSHeader, aCertChain, aName);
  if (NS_FAILED(rv)) {
    *_retval = false;
    CSVerifier_LOG(("CSVerifier: Signature verification failed\n"));
    if (rv == NS_ERROR_INVALID_SIGNATURE) {
      return NS_OK;
    }
    // This failure can have many different reasons but we don't treat it as
    // invalid signature.
    Accumulate(Telemetry::CONTENT_SIGNATURE_VERIFICATION_STATUS, 3);
    Telemetry::AccumulateCategoricalKeyed(mFingerprint,
      Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS::err3);
    return rv;
  }

  return End(_retval);
}

bool
IsNewLine(char16_t c)
{
  return c == '\n' || c == '\r';
}

nsresult
ReadChainIntoCertList(const nsACString& aCertChain, CERTCertList* aCertList)
{
  bool inBlock = false;
  bool certFound = false;

  const nsCString header = NS_LITERAL_CSTRING("-----BEGIN CERTIFICATE-----");
  const nsCString footer = NS_LITERAL_CSTRING("-----END CERTIFICATE-----");

  nsCWhitespaceTokenizerTemplate<IsNewLine> tokenizer(aCertChain);

  nsAutoCString blockData;
  while (tokenizer.hasMoreTokens()) {
    nsDependentCSubstring token = tokenizer.nextToken();
    if (token.IsEmpty()) {
      continue;
    }
    if (inBlock) {
      if (token.Equals(footer)) {
        inBlock = false;
        certFound = true;
        // base64 decode data, make certs, append to chain
        nsAutoCString derString;
        nsresult rv = Base64Decode(blockData, derString);
        if (NS_FAILED(rv)) {
          CSVerifier_LOG(("CSVerifier: decoding the signature failed\n"));
          return rv;
        }
        SECItem der = {
          siBuffer,
          BitwiseCast<unsigned char*, const char*>(derString.get()),
          derString.Length(),
        };
        UniqueCERTCertificate tmpCert(
          CERT_NewTempCertificate(CERT_GetDefaultCertDB(), &der, nullptr, false,
                                  true));
        if (!tmpCert) {
          return NS_ERROR_FAILURE;
        }
        // if adding tmpCert succeeds, tmpCert will now be owned by aCertList
        SECStatus res = CERT_AddCertToListTail(aCertList, tmpCert.get());
        if (res != SECSuccess) {
          return MapSECStatus(res);
        }
        Unused << tmpCert.release();
      } else {
        blockData.Append(token);
      }
    } else if (token.Equals(header)) {
      inBlock = true;
      blockData = "";
    }
  }
  if (inBlock || !certFound) {
    // the PEM data did not end; bad data.
    CSVerifier_LOG(("CSVerifier: supplied chain contains bad data\n"));
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
ContentSignatureVerifier::CreateContextInternal(const nsACString& aData,
                                                const nsACString& aCertChain,
                                                const nsACString& aName)
{
  MOZ_ASSERT(NS_IsMainThread());

  UniqueCERTCertList certCertList(CERT_NewCertList());
  if (!certCertList) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = ReadChainIntoCertList(aCertChain, certCertList.get());
  if (NS_FAILED(rv)) {
    return rv;
  }

  CERTCertListNode* node = CERT_LIST_HEAD(certCertList.get());
  if (!node || CERT_LIST_END(node, certCertList.get()) || !node->cert) {
    return NS_ERROR_FAILURE;
  }

  SECItem* certSecItem = &node->cert->derCert;

  Input certDER;
  mozilla::pkix::Result result =
    certDER.Init(BitwiseCast<uint8_t*, unsigned char*>(certSecItem->data),
                 certSecItem->len);
  if (result != Success) {
    return NS_ERROR_FAILURE;
  }

  // Get EE certificate fingerprint for telemetry.
  unsigned char fingerprint[SHA256_LENGTH] = {0};
  SECStatus srv =
    PK11_HashBuf(SEC_OID_SHA256, fingerprint, certSecItem->data,
                 AssertedCast<int32_t>(certSecItem->len));
  if (srv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }
  SECItem fingerprintItem = {siBuffer, fingerprint, SHA256_LENGTH};
  mFingerprint.Truncate();
  UniquePORTString tmpFingerprintString(CERT_Hexify(&fingerprintItem, 0));
  mFingerprint.Append(tmpFingerprintString.get());

  // Check the signerCert chain is good
  CSTrustDomain trustDomain(certCertList);
  result = BuildCertChain(trustDomain, certDER, Now(),
                          EndEntityOrCA::MustBeEndEntity,
                          KeyUsage::noParticularKeyUsageRequired,
                          KeyPurposeId::id_kp_codeSigning,
                          CertPolicyId::anyPolicy,
                          nullptr/*stapledOCSPResponse*/);
  if (result != Success) {
    // if there was a library error, return an appropriate error
    if (IsFatalError(result)) {
      return NS_ERROR_FAILURE;
    }
    // otherwise, assume the signature was invalid
    if (result == mozilla::pkix::Result::ERROR_EXPIRED_CERTIFICATE) {
      Accumulate(Telemetry::CONTENT_SIGNATURE_VERIFICATION_STATUS, 4);
      Telemetry::AccumulateCategoricalKeyed(mFingerprint,
        Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS::err4);
    } else if (result ==
               mozilla::pkix::Result::ERROR_NOT_YET_VALID_CERTIFICATE) {
      Accumulate(Telemetry::CONTENT_SIGNATURE_VERIFICATION_STATUS, 5);
      Telemetry::AccumulateCategoricalKeyed(mFingerprint,
        Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS::err5);
    } else {
      // Building cert chain failed for some other reason.
      Accumulate(Telemetry::CONTENT_SIGNATURE_VERIFICATION_STATUS, 6);
      Telemetry::AccumulateCategoricalKeyed(mFingerprint,
        Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS::err6);
    }
    CSVerifier_LOG(("CSVerifier: The supplied chain is bad (%s)\n",
                    MapResultToName(result)));
    return NS_ERROR_INVALID_SIGNATURE;
  }

  // Check the SAN
  Input hostnameInput;

  result = hostnameInput.Init(
    BitwiseCast<const uint8_t*, const char*>(aName.BeginReading()),
    aName.Length());
  if (result != Success) {
    return NS_ERROR_FAILURE;
  }

  BRNameMatchingPolicy nameMatchingPolicy(BRNameMatchingPolicy::Mode::Enforce);
  result = CheckCertHostname(certDER, hostnameInput, nameMatchingPolicy);
  if (result != Success) {
    // EE cert isnot valid for the given host name.
    Accumulate(Telemetry::CONTENT_SIGNATURE_VERIFICATION_STATUS, 7);
    Telemetry::AccumulateCategoricalKeyed(mFingerprint,
      Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS::err7);
    return NS_ERROR_INVALID_SIGNATURE;
  }

  mKey.reset(CERT_ExtractPublicKey(node->cert));

  // in case we were not able to extract a key
  if (!mKey) {
    Accumulate(Telemetry::CONTENT_SIGNATURE_VERIFICATION_STATUS, 8);
    Telemetry::AccumulateCategoricalKeyed(mFingerprint,
      Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS::err8);
    CSVerifier_LOG(("CSVerifier: unable to extract a key\n"));
    return NS_ERROR_INVALID_SIGNATURE;
  }

  // Base 64 decode the signature
  nsAutoCString rawSignature;
  rv = Base64Decode(mSignature, rawSignature);
  if (NS_FAILED(rv)) {
    CSVerifier_LOG(("CSVerifier: decoding the signature failed\n"));
    return rv;
  }

  // get signature object
  ScopedAutoSECItem signatureItem;
  SECItem rawSignatureItem = {
    siBuffer,
    BitwiseCast<unsigned char*, const char*>(rawSignature.get()),
    rawSignature.Length(),
  };
  // We have a raw ecdsa signature r||s so we have to DER-encode it first
  // Note that we have to check rawSignatureItem->len % 2 here as
  // DSAU_EncodeDerSigWithLen asserts this
  if (rawSignatureItem.len == 0 || rawSignatureItem.len % 2 != 0) {
    CSVerifier_LOG(("CSVerifier: signature length is bad\n"));
    return NS_ERROR_FAILURE;
  }
  if (DSAU_EncodeDerSigWithLen(&signatureItem, &rawSignatureItem,
                               rawSignatureItem.len) != SECSuccess) {
    CSVerifier_LOG(("CSVerifier: encoding the signature failed\n"));
    return NS_ERROR_FAILURE;
  }

  // this is the only OID we support for now
  SECOidTag oid = SEC_OID_ANSIX962_ECDSA_SHA384_SIGNATURE;

  mCx = UniqueVFYContext(
    VFY_CreateContext(mKey.get(), &signatureItem, oid, nullptr));
  if (!mCx) {
    // Creating context failed.
    Accumulate(Telemetry::CONTENT_SIGNATURE_VERIFICATION_STATUS, 9);
    Telemetry::AccumulateCategoricalKeyed(mFingerprint,
      Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS::err9);
    return NS_ERROR_INVALID_SIGNATURE;
  }

  if (VFY_Begin(mCx.get()) != SECSuccess) {
    // Creating context failed.
    Accumulate(Telemetry::CONTENT_SIGNATURE_VERIFICATION_STATUS, 9);
    Telemetry::AccumulateCategoricalKeyed(mFingerprint,
      Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS::err9);
    return NS_ERROR_INVALID_SIGNATURE;
  }

  rv = UpdateInternal(kPREFIX);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // add data if we got any
  return UpdateInternal(aData);
}

nsresult
ContentSignatureVerifier::DownloadCertChain()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mCertChainURL.IsEmpty()) {
    return NS_ERROR_INVALID_SIGNATURE;
  }

  nsCOMPtr<nsIURI> certChainURI;
  nsresult rv = NS_NewURI(getter_AddRefs(certChainURI), mCertChainURL);
  if (NS_FAILED(rv) || !certChainURI) {
    return rv;
  }

  // If the address is not https, fail.
  bool isHttps = false;
  rv = certChainURI->SchemeIs("https", &isHttps);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!isHttps) {
    return NS_ERROR_INVALID_SIGNATURE;
  }

  rv = NS_NewChannel(getter_AddRefs(mChannel), certChainURI,
                     nsContentUtils::GetSystemPrincipal(),
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                     nsIContentPolicy::TYPE_OTHER);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // we need this chain soon
  nsCOMPtr<nsISupportsPriority> priorityChannel = do_QueryInterface(mChannel);
  if (priorityChannel) {
    priorityChannel->AdjustPriority(nsISupportsPriority::PRIORITY_HIGHEST);
  }

  rv = mChannel->AsyncOpen2(this);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

// Create a context for content signature verification using CreateContext below.
// This function doesn't require a cert chain to be passed, but instead aCSHeader
// must contain an x5u value that is then used to download the cert chain.
NS_IMETHODIMP
ContentSignatureVerifier::CreateContextWithoutCertChain(
  nsIContentSignatureReceiverCallback *aCallback, const nsACString& aCSHeader,
  const nsACString& aName)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCallback);
  if (mInitialised) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }
  mInitialised = true;

  // we get the raw content-signature header here, so first parse aCSHeader
  nsresult rv = ParseContentSignatureHeader(aCSHeader);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mCallback = aCallback;
  mName.Assign(aName);

  // We must download the cert chain now.
  // This is async and blocks createContextInternal calls.
  return DownloadCertChain();
}

// Create a context for a content signature verification.
// It sets signature, certificate chain and name that should be used to verify
// the data. The data parameter is the first part of the data to verify (this
// can be the empty string).
NS_IMETHODIMP
ContentSignatureVerifier::CreateContext(const nsACString& aData,
                                        const nsACString& aCSHeader,
                                        const nsACString& aCertChain,
                                        const nsACString& aName)
{
  if (mInitialised) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }
  mInitialised = true;
  // The cert chain is given in aCertChain so we don't have to download anything.
  mHasCertChain = true;

  // we get the raw content-signature header here, so first parse aCSHeader
  nsresult rv = ParseContentSignatureHeader(aCSHeader);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return CreateContextInternal(aData, aCertChain, aName);
}

nsresult
ContentSignatureVerifier::UpdateInternal(const nsACString& aData)
{
  if (!aData.IsEmpty()) {
    if (VFY_Update(mCx.get(), (const unsigned char*)nsPromiseFlatCString(aData).get(),
                   aData.Length()) != SECSuccess){
      return NS_ERROR_INVALID_SIGNATURE;
    }
  }
  return NS_OK;
}

/**
 * Add data to the context that shold be verified.
 */
NS_IMETHODIMP
ContentSignatureVerifier::Update(const nsACString& aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  // If we didn't create the context yet, bail!
  if (!mHasCertChain) {
    MOZ_ASSERT_UNREACHABLE(
      "Someone called ContentSignatureVerifier::Update before "
      "downloading the cert chain.");
    return NS_ERROR_FAILURE;
  }

  return UpdateInternal(aData);
}

/**
 * Finish signature verification and return the result in _retval.
 */
NS_IMETHODIMP
ContentSignatureVerifier::End(bool* _retval)
{
  NS_ENSURE_ARG(_retval);
  MOZ_ASSERT(NS_IsMainThread());

  // If we didn't create the context yet, bail!
  if (!mHasCertChain) {
    Accumulate(Telemetry::CONTENT_SIGNATURE_VERIFICATION_STATUS, 2);
    MOZ_ASSERT_UNREACHABLE(
      "Someone called ContentSignatureVerifier::End before "
      "downloading the cert chain.");
    return NS_ERROR_FAILURE;
  }

  bool result = (VFY_End(mCx.get()) == SECSuccess);
  if (result) {
    Accumulate(Telemetry::CONTENT_SIGNATURE_VERIFICATION_STATUS, 0);
  } else {
    Accumulate(Telemetry::CONTENT_SIGNATURE_VERIFICATION_STATUS, 1);
    Telemetry::AccumulateCategoricalKeyed(mFingerprint,
      Telemetry::LABELS_CONTENT_SIGNATURE_VERIFICATION_ERRORS::err1);
  }
  *_retval = result;

  return NS_OK;
}

nsresult
ContentSignatureVerifier::ParseContentSignatureHeader(
  const nsACString& aContentSignatureHeader)
{
  MOZ_ASSERT(NS_IsMainThread());
  // We only support p384 ecdsa according to spec
  NS_NAMED_LITERAL_CSTRING(signature_var, "p384ecdsa");
  NS_NAMED_LITERAL_CSTRING(certChainURL_var, "x5u");

  const nsCString& flatHeader = PromiseFlatCString(aContentSignatureHeader);
  nsSecurityHeaderParser parser(flatHeader);
  nsresult rv = parser.Parse();
  if (NS_FAILED(rv)) {
    CSVerifier_LOG(("CSVerifier: could not parse ContentSignature header\n"));
    return NS_ERROR_FAILURE;
  }
  LinkedList<nsSecurityHeaderDirective>* directives = parser.GetDirectives();

  for (nsSecurityHeaderDirective* directive = directives->getFirst();
       directive != nullptr; directive = directive->getNext()) {
    CSVerifier_LOG(("CSVerifier: found directive %s\n", directive->mName.get()));
    if (directive->mName.Length() == signature_var.Length() &&
        directive->mName.EqualsIgnoreCase(signature_var.get(),
                                          signature_var.Length())) {
      if (!mSignature.IsEmpty()) {
        CSVerifier_LOG(("CSVerifier: found two ContentSignatures\n"));
        return NS_ERROR_INVALID_SIGNATURE;
      }

      CSVerifier_LOG(("CSVerifier: found a ContentSignature directive\n"));
      mSignature = directive->mValue;
    }
    if (directive->mName.Length() == certChainURL_var.Length() &&
        directive->mName.EqualsIgnoreCase(certChainURL_var.get(),
                                          certChainURL_var.Length())) {
      if (!mCertChainURL.IsEmpty()) {
        CSVerifier_LOG(("CSVerifier: found two x5u values\n"));
        return NS_ERROR_INVALID_SIGNATURE;
      }

      CSVerifier_LOG(("CSVerifier: found an x5u directive\n"));
      mCertChainURL = directive->mValue;
    }
  }

  // we have to ensure that we found a signature at this point
  if (mSignature.IsEmpty()) {
    CSVerifier_LOG(("CSVerifier: got a Content-Signature header but didn't find a signature.\n"));
    return NS_ERROR_FAILURE;
  }

  // Bug 769521: We have to change b64 url to regular encoding as long as we
  // don't have a b64 url decoder. This should change soon, but in the meantime
  // we have to live with this.
  mSignature.ReplaceChar('-', '+');
  mSignature.ReplaceChar('_', '/');

  return NS_OK;
}

/* nsIStreamListener implementation */

NS_IMETHODIMP
ContentSignatureVerifier::OnStartRequest(nsIRequest* aRequest,
                                         nsISupports* aContext)
{
  MOZ_ASSERT(NS_IsMainThread());
  return NS_OK;
}

NS_IMETHODIMP
ContentSignatureVerifier::OnStopRequest(nsIRequest* aRequest,
                                        nsISupports* aContext, nsresult aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIContentSignatureReceiverCallback> callback;
  callback.swap(mCallback);
  nsresult rv;

  // Check HTTP status code and return if it's not 200.
  nsCOMPtr<nsIHttpChannel> http = do_QueryInterface(aRequest, &rv);
  uint32_t httpResponseCode;
  if (NS_FAILED(rv) || NS_FAILED(http->GetResponseStatus(&httpResponseCode)) ||
      httpResponseCode != 200) {
    callback->ContextCreated(false);
    return NS_OK;
  }

  if (NS_FAILED(aStatus)) {
    callback->ContextCreated(false);
    return NS_OK;
  }

  nsAutoCString certChain;
  for (uint32_t i = 0; i < mCertChain.Length(); ++i) {
    certChain.Append(mCertChain[i]);
  }

  // We got the cert chain now. Let's create the context.
  rv = CreateContextInternal(NS_LITERAL_CSTRING(""), certChain, mName);
  if (NS_FAILED(rv)) {
    callback->ContextCreated(false);
    return NS_OK;
  }

  mHasCertChain = true;
  callback->ContextCreated(true);
  return NS_OK;
}

NS_IMETHODIMP
ContentSignatureVerifier::OnDataAvailable(nsIRequest* aRequest,
                                          nsISupports* aContext,
                                          nsIInputStream* aInputStream,
                                          uint64_t aOffset, uint32_t aCount)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsAutoCString buffer;

  nsresult rv = NS_ConsumeStream(aInputStream, aCount, buffer);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!mCertChain.AppendElement(buffer, fallible)) {
    mCertChain.TruncateLength(0);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
ContentSignatureVerifier::GetInterface(const nsIID& uuid, void** result)
{
  return QueryInterface(uuid, result);
}
