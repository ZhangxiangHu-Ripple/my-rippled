//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2023 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#pragma once

#include <cstdint>
#include <libff/algebra/curves/alt_bn128/alt_bn128_pp.hpp>

namespace ripple {

/**
 *  Fixed sizes for BN254 encodings and pair packing.
 *
 * - G1_LEN:  uncompressed G1 (x||y), big-endian, 32B each (64B total)
 * - G2_LEN:  uncompressed G2 (x_c1||x_c0||y_c1||y_c0), big-endian (128B)
 * - IC_LEN:  generic 64B element container (e.g., G1 coord pair)
 * - SCALAR_LEN: scalar field element in 32B big-endian
 * - PAIR_LEN: bytes for one (G1,G2) uncompressed pair
 * - GROTH16_PAIR_LEN: space for 4 pairs (common in multi-pair checks)
 */
constexpr std::size_t G1_LEN = 64;
constexpr std::size_t G2_LEN = 128;
constexpr std::size_t IC_LEN = 64;
constexpr std::size_t SCALAR_LEN = 32;
constexpr std::size_t PAIR_LEN = G1_LEN + G2_LEN;
constexpr std::size_t GROTH16_PAIR_LEN = 4 * (G1_LEN + G2_LEN);

/**
 * Reverse a 32-byte big-endian limb into little-endian order.
 *
 * 32-byte big-endian input
 * 32-byte little-endian output
 */
inline void 
be32_to_le32(uint8_t const* be, uint8_t* le) 
{
    for (int i = 0; i < 32; ++i) le[i] = be[31 - i];
}

/**
 * Reverse both 32-byte halves of a 64B uncompressed (x||y) from BE to LE.
 *
 * @param Input:  in_be[0..31]=x_be, in_be[32..63]=y_be
 * @param Output: out_le[0..31]=x_le, out_le[32..63]=y_le
 */
inline void 
uncompressed_be_to_le(uint8_t const in_be[64], uint8_t out_le[64]) 
{
    be32_to_le32(in_be + 0,  out_le + 0);
    be32_to_le32(in_be + 32, out_le + 32);
}

/**
 * Import a 32B little-endian integer into a libff bigint over 𝔽_q.
 *
 * Uses GMP mpz_import with word-order little-endian semantics.
 *
 * @param le 32-byte little-endian input
 * @param out bigint over alt_bn128_q_limbs
 */
void 
le32_to_bigint_q(
    uint8_t const le[32],
    libff::bigint<libff::alt_bn128_q_limbs>& out
);

/**
 * Import a 32B big-endian integer into a libff bigint over 𝔽_r.
 * Reserved for future use.
 *
 * @param be 32-byte big-endian input
 * @param out bigint over alt_bn128_r_limbs
 * @return true if successful; false if input is out of range
 */
bool 
be32_to_bigint_r(
    const uint8_t be[32],
    libff::bigint<libff::alt_bn128_r_limbs>& out
);

/**
 * Decode an uncompressed big-endian G1 point into libff representation.
 *
 * Input format: x(32B be) || y(32B be).
 * Sets Z = 1 and returns P.is_well_formed().
 *
 * @param in_be 64-byte big-endian (x||y)
 * @param P     output point (projective; affine coords set)
 * @return true if point lies on curve; false otherwise
 */
bool 
g1_from_uncompressed_be(
    uint8_t const in_be[64],
    libff::alt_bn128_G1& P
);

/**
 * Encode a libff G1 point to 64B uncompressed big-endian (x||y).
 *
 * Converts to affine coordinates internally.
 *
 * @param point   input G1 point
 * @param out_be64 64-byte output buffer
 * @return true on success
 */
bool g1_to_uncompressed_be(
    libff::alt_bn128_G1 const& point,
    uint8_t* out_be64
);


/**
 * Decode an uncompressed big-endian G2 point into libff representation.
 *
 * Wire format (128B): x_c1||x_c0||y_c1||y_c0, each 32B big-endian in 𝔽_q.
 * Constructs Fq2 as (c0, c1): x = x0 + x1·u, y = y0 + y1·u.
 * Sets Z = 1 and returns P.is_well_formed().
 *
 * @param in_be 128-byte big-endian input
 * @param P     output G2 point
 * @return true if point lies on curve; false otherwise
 */
bool 
g2_from_uncompressed_be(
    uint8_t const in_be[128],
    libff::alt_bn128_G2& P
);

}  // namespace ripple
