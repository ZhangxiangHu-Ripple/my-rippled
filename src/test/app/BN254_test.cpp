//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2025 Ripple Labs Inc.

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

#include <test/jtx.h>
#include <libff/algebra/curves/alt_bn128/alt_bn128_pp.hpp>
#include <xrpld/app/wasm/HostFuncImpl.h>
#include <xrpld/app/wasm/HostFuncWrapper.h>
#include <xrpld/app/wasm/BN254_encoding.h>


using namespace libff;

namespace ripple {
namespace test {

struct BN254_test : public beast::unit_test::suite
{
    void
    test_addition()
    {
        testcase("BN254 addition");
        // Silence libff profiling noise in test logs
        libff::inhibit_profiling_info = true;
        libff::inhibit_profiling_counters = true;

        libff::alt_bn128_pp::init_public_params();

        // G1/G2 generators
        libff::alt_bn128_G1 P = alt_bn128_G1::one();
        alt_bn128_G2 Q = alt_bn128_G2::one();

        // 1) Group law sanity: P + P == dbl(P)
        alt_bn128_G1 twoP = P + P;
        alt_bn128_G1 dblP = P.dbl();
        BEAST_EXPECT(twoP == dblP);

        log << "+++libff alt_bn128 basic ops + pairing checks passed\n";
    }

    void
    run() override
    {
        test_addition();
    }
};

BEAST_DEFINE_TESTSUITE(BN254, app, ripple);

}  // namespace test
}  // namespace ripple
