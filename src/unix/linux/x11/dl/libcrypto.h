// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#define EVP_CTRL_AEAD_GET_TAG 0x10
#define EVP_CTRL_GCM_GET_TAG  EVP_CTRL_AEAD_GET_TAG

#define EVP_CTRL_AEAD_SET_TAG 0x11
#define EVP_CTRL_GCM_SET_TAG  EVP_CTRL_AEAD_SET_TAG

typedef struct engine_st ENGINE;
typedef struct evp_cipher_st EVP_CIPHER;
typedef struct evp_cipher_ctx_st EVP_CIPHER_CTX;
typedef struct evp_md_st EVP_MD;
