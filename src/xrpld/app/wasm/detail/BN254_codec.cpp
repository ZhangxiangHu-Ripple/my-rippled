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


#include <xrpld/app/wasm/BN254_codec.h>

namespace ripple {

void le32_to_bigint_q(
    uint8_t const le[32],
    libff::bigint<libff::alt_bn128_q_limbs>& out)
{
    mpz_t z; mpz_init(z);
    mpz_import(z, 32, -1, 1, 0, 0, le);
    out = libff::bigint<libff::alt_bn128_q_limbs>(z);
    mpz_clear(z);
}

bool be32_to_bigint_r(
    uint8_t const be[32],
    libff::bigint<libff::alt_bn128_r_limbs>& out) 
{
    mpz_t z; mpz_init(z);
    mpz_import(z, 32, 1, 1, 1, 0, be);
    out = libff::bigint<libff::alt_bn128_r_limbs>(z);
    mpz_clear(z);

    return true;
}

bool g1_from_uncompressed_be(
    uint8_t const in_be[64],
    libff::alt_bn128_G1& P)
{
    uint8_t in_le[64];
    uncompressed_be_to_le(in_be, in_le);

    auto const* x_le = in_le + 0;
    auto const* y_le = in_le + 32;

    libff::bigint<libff::alt_bn128_q_limbs> X, Y;
    le32_to_bigint_q(x_le, X);
    le32_to_bigint_q(y_le, Y);

    libff::alt_bn128_Fq x(X), y(Y);
    P.X = x;
    P.Y = y;
    P.Z = libff::alt_bn128_Fq::one();

    return P.is_well_formed();
}

bool g1_to_uncompressed_be(
    libff::alt_bn128_G1 const& point,
    uint8_t* out_be64)
{
    libff::alt_bn128_G1 affine = point;
    affine.to_affine_coordinates();

    // If infinity, you may want to reject/define encoding policy.
    // Here we encode (0,0). Adjust if you prefer to treat as error.
    // if (affine.is_zero()) return false;

    mpz_t x_mpz, y_mpz;
    mpz_init(x_mpz);
    mpz_init(y_mpz);

    affine.X.as_bigint().to_mpz(x_mpz);
    affine.Y.as_bigint().to_mpz(y_mpz);

    // Export as big-endian, 1-byte words
    size_t count = 0;
    std::memset(out_be64, 0, 64);

    // X
    mpz_export(out_be64, &count, /*order=*/1, /*size=*/1, /*endian=*/1, /*nail=*/0, x_mpz);
    if (count < 32) {
        std::memmove(out_be64 + (32 - count), out_be64, count);
        std::memset(out_be64, 0, 32 - count);
    }

    // Y
    count = 0;
    mpz_export(out_be64 + 32, &count, 1, 1, 1, 0, y_mpz);
    if (count < 32) {
        std::memmove(out_be64 + 32 + (32 - count), out_be64 + 32, count);
        std::memset(out_be64 + 32, 0, 32 - count);
    }

    mpz_clear(x_mpz);
    mpz_clear(y_mpz);

    return true;
}

bool g2_from_uncompressed_be(
    const uint8_t in_be[128], 
    libff::alt_bn128_G2& P) 
{
    uint8_t in_le[128];
    uncompressed_be_to_le(in_be, in_le);
    uncompressed_be_to_le(in_be + 64, in_le + 64);

    // G2 uncompressed: [x_c1, x_c0, y_c1, y_c0]
    const uint8_t* x_c1_le = in_le + 0;      // first 32 bytes (imag part)
    const uint8_t* x_c0_le = in_le + 32;     // next 32 bytes (real part)
    const uint8_t* y_c1_le = in_le + 64;     // next 32 bytes (imag part)
    const uint8_t* y_c0_le = in_le + 96;     // last 32 bytes (real part)

    libff::bigint<libff::alt_bn128_q_limbs> X_0, X_1, Y_0, Y_1;
    le32_to_bigint_q(x_c0_le, X_0);
    le32_to_bigint_q(x_c1_le, X_1);
    le32_to_bigint_q(y_c0_le, Y_0);
    le32_to_bigint_q(y_c1_le, Y_1);
    libff::alt_bn128_Fq x0_fq(X_0);
    libff::alt_bn128_Fq x1_fq(X_1);
    libff::alt_bn128_Fq y0_fq(Y_0);
    libff::alt_bn128_Fq y1_fq(Y_1);
    libff::alt_bn128_Fq2 x_fq2(x0_fq, x1_fq);  // x = x0 + x1*u
    libff::alt_bn128_Fq2 y_fq2(y0_fq, y1_fq);  // y = y0 + y1*u

    P = libff::alt_bn128_G2(x_fq2, y_fq2, libff::alt_bn128_Fq2::one());
    
    return P.is_well_formed();
}

}  // namespace ripple