// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

struct MTY_Cert {
	char dummy;
};

struct MTY_DTLS {
	char dummy;
};

static MTY_Cert DTLS_CERT_DUMMY;
static MTY_DTLS DTLS_DUMMY;

MTY_Cert *MTY_CertCreate(void)
{
	return &DTLS_CERT_DUMMY;
}

void MTY_CertDestroy(MTY_Cert **cert)
{
}

void MTY_CertGetFingerprint(MTY_Cert *ctx, char *fingerprint, size_t size)
{
}

MTY_DTLS *MTY_DTLSCreate(MTY_Cert *cert, const char *peerFingerprint, uint32_t mtu)
{
	return &DTLS_DUMMY;
}

void MTY_DTLSDestroy(MTY_DTLS **dtls)
{
}

MTY_Async MTY_DTLSHandshake(MTY_DTLS *ctx, const void *buf, size_t size, MTY_DTLSWriteFunc writeFunc, void *opaque)
{
	return MTY_ASYNC_ERROR;
}

bool MTY_DTLSEncrypt(MTY_DTLS *ctx, const void *in, size_t inSize, void *out, size_t outSize, size_t *written)
{
	return false;
}

bool MTY_DTLSDecrypt(MTY_DTLS *ctx, const void *in, size_t inSize, void *out, size_t outSize, size_t *read)
{
	return false;
}
