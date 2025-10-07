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

#include <test/app/wasm_fixtures/fixtures.h>
#include <test/jtx.h>

#include <xrpld/app/misc/AmendmentTable.h>
#include <xrpld/app/tx/detail/NFTokenUtils.h>
#include <xrpld/app/wasm/HostFunc.h>
#include <xrpld/app/wasm/WasmVM.h>
#include <xrpld/ledger/detail/ApplyViewBase.h>
#include <xrpld/app/wasm/BN254_codec.h>

namespace ripple {

namespace test {

struct TestLedgerDataProvider : public HostFunctions
{
    jtx::Env* env_;
    void const* rt_ = nullptr;

public:
    TestLedgerDataProvider(jtx::Env* env) : env_(env)
    {
    }

    virtual void
    setRT(void const* rt) override
    {
        rt_ = rt;
    }

    virtual void const*
    getRT() const override
    {
        return rt_;
    }

    Expected<std::int32_t, HostFunctionError>
    getLedgerSqn() override
    {
        return env_->current()->seq();
    }
};

struct TestHostFunctions : public HostFunctions
{
    test::jtx::Env& env_;
    AccountID accountID_;
    Bytes data_;
    int clock_drift_ = 0;
    void const* rt_ = nullptr;

public:
    TestHostFunctions(test::jtx::Env& env, int cd = 0)
        : env_(env), clock_drift_(cd)
    {
        accountID_ = env_.master.id();
        std::string t = "10000";
        data_ = Bytes{t.begin(), t.end()};
    }

    virtual void
    setRT(void const* rt) override
    {
        rt_ = rt;
    }

    virtual void const*
    getRT() const override
    {
        return rt_;
    }

    beast::Journal
    getJournal() override
    {
        return env_.journal;
    }

    bool getLedgersqn_called_ = false;
    std::optional<int32_t> current_ledger_sqn_;
    std::optional<int32_t> const& lastSqnOut() const { return current_ledger_sqn_; }

    // std::optional<Bytes> last_mul_p1_;
    // std::optional<Bytes> last_mul_scalar_;
    // std::optional<Bytes> last_mul_out_;

    Expected<std::int32_t, HostFunctionError>
    getLedgerSqn() override
    {
        getLedgersqn_called_ = true;
        current_ledger_sqn_ = 12345;
        return 12345;
    }

    Expected<std::int32_t, HostFunctionError>
    getParentLedgerTime() override
    {
        return 67890;
    }

    Expected<Hash, HostFunctionError>
    getParentLedgerHash() override
    {
        return env_.current()->info().parentHash;
    }

    Expected<Hash, HostFunctionError>
    getLedgerAccountHash() override
    {
        return env_.current()->info().accountHash;
    }

    Expected<Hash, HostFunctionError>
    getLedgerTransactionHash() override
    {
        return env_.current()->info().txHash;
    }

    Expected<int32_t, HostFunctionError>
    getBaseFee() override
    {
        return 10;
    }

    Expected<int32_t, HostFunctionError>
    isAmendmentEnabled(uint256 const& amendmentId) override
    {
        return 1;
    }

    Expected<int32_t, HostFunctionError>
    isAmendmentEnabled(std::string_view const& amendmentName) override
    {
        return 1;
    }

    virtual Expected<int32_t, HostFunctionError>
    cacheLedgerObj(uint256 const& objId, int32_t cacheIdx) override
    {
        return 1;
    }

    Expected<Bytes, HostFunctionError>
    getTxField(SField const& fname) override
    {
        if (fname == sfAccount)
            return Bytes(accountID_.begin(), accountID_.end());
        else if (fname == sfFee)
        {
            int64_t x = 235;
            uint8_t const* p = reinterpret_cast<uint8_t const*>(&x);
            return Bytes{p, p + sizeof(x)};
        }
        else if (fname == sfSequence)
        {
            auto const x = getLedgerSqn();
            if (!x)
                return Unexpected(x.error());
            std::uint32_t const data = x.value();
            auto const* b = reinterpret_cast<uint8_t const*>(&data);
            auto const* e = reinterpret_cast<uint8_t const*>(&data + 1);
            return Bytes{b, e};
        }
        return Bytes();
    }

    Expected<Bytes, HostFunctionError>
    getCurrentLedgerObjField(SField const& fname) override
    {
        auto const& sn = fname.getName();
        if (sn == "Destination" || sn == "Account")
            return Bytes(accountID_.begin(), accountID_.end());
        else if (sn == "Data")
            return data_;
        else if (sn == "FinishAfter")
        {
            auto t =
                env_.current()->parentCloseTime().time_since_epoch().count();
            std::string s = std::to_string(t);
            return Bytes{s.begin(), s.end()};
        }

        return Unexpected(HostFunctionError::INTERNAL);
    }

    Expected<Bytes, HostFunctionError>
    getLedgerObjField(int32_t cacheIdx, SField const& fname) override
    {
        if (fname == sfBalance)
        {
            int64_t x = 10'000;
            uint8_t const* p = reinterpret_cast<uint8_t const*>(&x);
            return Bytes{p, p + sizeof(x)};
        }
        else if (fname == sfAccount)
        {
            return Bytes(accountID_.begin(), accountID_.end());
        }
        return data_;
    }

    Expected<Bytes, HostFunctionError>
    getTxNestedField(Slice const& locator) override
    {
        if (locator.size() == 4)
        {
            int32_t const* l = reinterpret_cast<int32_t const*>(locator.data());
            int32_t const sfield = l[0];
            if (sfield == sfAccount.fieldCode)
            {
                return Bytes(accountID_.begin(), accountID_.end());
            }
        }
        uint8_t const a[] = {0x2b, 0x6a, 0x23, 0x2a, 0xa4, 0xc4, 0xbe, 0x41,
                             0xbf, 0x49, 0xd2, 0x45, 0x9f, 0xa4, 0xa0, 0x34,
                             0x7e, 0x1b, 0x54, 0x3a, 0x4c, 0x92, 0xfc, 0xee,
                             0x08, 0x21, 0xc0, 0x20, 0x1e, 0x2e, 0x9a, 0x00};
        return Bytes(&a[0], &a[sizeof(a)]);
    }

    Expected<Bytes, HostFunctionError>
    getCurrentLedgerObjNestedField(Slice const& locator) override
    {
        if (locator.size() == 4)
        {
            int32_t const* l = reinterpret_cast<int32_t const*>(locator.data());
            int32_t const sfield = l[0];
            if (sfield == sfAccount.fieldCode)
            {
                return Bytes(accountID_.begin(), accountID_.end());
            }
        }
        uint8_t const a[] = {0x2b, 0x6a, 0x23, 0x2a, 0xa4, 0xc4, 0xbe, 0x41,
                             0xbf, 0x49, 0xd2, 0x45, 0x9f, 0xa4, 0xa0, 0x34,
                             0x7e, 0x1b, 0x54, 0x3a, 0x4c, 0x92, 0xfc, 0xee,
                             0x08, 0x21, 0xc0, 0x20, 0x1e, 0x2e, 0x9a, 0x00};
        return Bytes(&a[0], &a[sizeof(a)]);
    }

    Expected<Bytes, HostFunctionError>
    getLedgerObjNestedField(int32_t cacheIdx, Slice const& locator) override
    {
        if (locator.size() == 4)
        {
            int32_t const* l = reinterpret_cast<int32_t const*>(locator.data());
            int32_t const sfield = l[0];
            if (sfield == sfAccount.fieldCode)
            {
                return Bytes(accountID_.begin(), accountID_.end());
            }
        }
        uint8_t const a[] = {0x2b, 0x6a, 0x23, 0x2a, 0xa4, 0xc4, 0xbe, 0x41,
                             0xbf, 0x49, 0xd2, 0x45, 0x9f, 0xa4, 0xa0, 0x34,
                             0x7e, 0x1b, 0x54, 0x3a, 0x4c, 0x92, 0xfc, 0xee,
                             0x08, 0x21, 0xc0, 0x20, 0x1e, 0x2e, 0x9a, 0x00};
        return Bytes(&a[0], &a[sizeof(a)]);
    }

    Expected<int32_t, HostFunctionError>
    getTxArrayLen(SField const& fname) override
    {
        return 32;
    }

    Expected<int32_t, HostFunctionError>
    getCurrentLedgerObjArrayLen(SField const& fname) override
    {
        return 32;
    }

    Expected<int32_t, HostFunctionError>
    getLedgerObjArrayLen(int32_t cacheIdx, SField const& fname) override
    {
        return 32;
    }

    Expected<int32_t, HostFunctionError>
    getTxNestedArrayLen(Slice const& locator) override
    {
        return 32;
    }

    Expected<int32_t, HostFunctionError>
    getCurrentLedgerObjNestedArrayLen(Slice const& locator) override
    {
        return 32;
    }

    Expected<int32_t, HostFunctionError>
    getLedgerObjNestedArrayLen(int32_t cacheIdx, Slice const& locator) override
    {
        return 32;
    }

    Expected<int32_t, HostFunctionError>
    updateData(Slice const& data) override
    {
        return 0;
    }

    Expected<int32_t, HostFunctionError>
    checkSignature(
        Slice const& message,
        Slice const& signature,
        Slice const& pubkey) override
    {
        return 1;
    }

    Expected<Hash, HostFunctionError>
    computeSha512HalfHash(Slice const& data) override
    {
        return env_.current()->info().parentHash;
    }

    Expected<Bytes, HostFunctionError>
    accountKeylet(AccountID const& account) override
    {
        if (!account)
            return Unexpected(HostFunctionError::INVALID_ACCOUNT);
        auto const keylet = keylet::account(account);
        return Bytes{keylet.key.begin(), keylet.key.end()};
    }

    Expected<Bytes, HostFunctionError>
    ammKeylet(Asset const& issue1, Asset const& issue2) override
    {
        if (issue1 == issue2)
            return Unexpected(HostFunctionError::INVALID_PARAMS);
        if (issue1.holds<MPTIssue>() || issue2.holds<MPTIssue>())
            return Unexpected(HostFunctionError::INVALID_PARAMS);
        auto const keylet = keylet::amm(issue1, issue2);
        return Bytes{keylet.key.begin(), keylet.key.end()};
    }

    Expected<Bytes, HostFunctionError>
    credentialKeylet(
        AccountID const& subject,
        AccountID const& issuer,
        Slice const& credentialType) override
    {
        if (!subject || !issuer || credentialType.empty() ||
            credentialType.size() > maxCredentialTypeLength)
            return Unexpected(HostFunctionError::INVALID_ACCOUNT);
        auto const keylet = keylet::credential(subject, issuer, credentialType);
        return Bytes{keylet.key.begin(), keylet.key.end()};
    }

    Expected<Bytes, HostFunctionError>
    escrowKeylet(AccountID const& account, std::uint32_t seq) override
    {
        if (!account)
            return Unexpected(HostFunctionError::INVALID_ACCOUNT);
        auto const keylet = keylet::escrow(account, seq);
        return Bytes{keylet.key.begin(), keylet.key.end()};
    }

    Expected<Bytes, HostFunctionError>
    oracleKeylet(AccountID const& account, std::uint32_t documentId) override
    {
        if (!account)
            return Unexpected(HostFunctionError::INVALID_ACCOUNT);
        auto const keylet = keylet::oracle(account, documentId);
        return Bytes{keylet.key.begin(), keylet.key.end()};
    }

    Expected<Bytes, HostFunctionError>
    getNFT(AccountID const& account, uint256 const& nftId) override
    {
        if (!account || !nftId)
        {
            return Unexpected(HostFunctionError::INVALID_PARAMS);
        }

        std::string s = "https://ripple.com";
        return Bytes(s.begin(), s.end());
    }

    Expected<Bytes, HostFunctionError>
    getNFTIssuer(uint256 const& nftId) override
    {
        return Bytes(accountID_.begin(), accountID_.end());
    }

    Expected<std::uint32_t, HostFunctionError>
    getNFTTaxon(uint256 const& nftId) override
    {
        return 4;
    }

    Expected<int32_t, HostFunctionError>
    getNFTFlags(uint256 const& nftId) override
    {
        return 8;
    }

    Expected<int32_t, HostFunctionError>
    getNFTTransferFee(uint256 const& nftId) override
    {
        return 10;
    }

    Expected<std::uint32_t, HostFunctionError>
    getNFTSerial(uint256 const& nftId) override
    {
        return 4;
    }

    Expected<int32_t, HostFunctionError>
    trace(std::string_view const& msg, Slice const& data, bool asHex) override
    {
#ifdef DEBUG_OUTPUT
        auto& j = std::cerr;
#else
        auto j = getJournal().trace();
#endif
        if (!asHex)
        {
            j << "WAMR TRACE: " << msg << " "
              << std::string_view(
                     reinterpret_cast<char const*>(data.data()), data.size());
        }
        else
        {
            std::string hex;
            hex.reserve(data.size() * 2);
            boost::algorithm::hex(
                data.begin(), data.end(), std::back_inserter(hex));
            j << "WAMR DEV TRACE: " << msg << " " << hex;
        }

#ifdef DEBUG_OUTPUT
        j << std::endl;
#endif

        return msg.size() + data.size() * (asHex ? 2 : 1);
    }

    Expected<int32_t, HostFunctionError>
    traceNum(std::string_view const& msg, int64_t data) override
    {
#ifdef DEBUG_OUTPUT
        auto& j = std::cerr;
#else
        auto j = getJournal().trace();
#endif
        j << "WAMR TRACE NUM: " << msg << " " << data;

#ifdef DEBUG_OUTPUT
        j << std::endl;
#endif
        return msg.size() + sizeof(data);
    }

    Expected<int32_t, HostFunctionError>
    traceAccount(std::string_view const& msg, AccountID const& account) override
    {
#ifdef DEBUG_OUTPUT
        auto j = getJournal().error();
#else
        auto j = getJournal().trace();
#endif
        if (!account)
            return Unexpected(HostFunctionError::INVALID_ACCOUNT);

        auto const accountStr = toBase58(account);

        j << "WAMR TRACE ACCOUNT: " << msg << " " << accountStr;
        return msg.size() + accountStr.size();
    }

    Expected<int32_t, HostFunctionError>
    traceFloat(std::string_view const& msg, Slice const& data) override
    {
#ifdef DEBUG_OUTPUT
        auto& j = std::cerr;
#else
        auto j = getJournal().trace();
#endif
        auto const s = floatToString(data);
        j << "WAMR TRACE FLOAT: " << msg << " " << s;

#ifdef DEBUG_OUTPUT
        j << std::endl;
#endif
        return msg.size() + s.size();
    }

    Expected<int32_t, HostFunctionError>
    traceAmount(std::string_view const& msg, STAmount const& amount) override
    {
#ifdef DEBUG_OUTPUT
        auto j = getJournal().error();
#else
        auto j = getJournal().trace();
#endif
        auto const amountStr = amount.getFullText();
        j << "WAMR TRACE AMOUNT: " << msg << " " << amountStr;
        return msg.size() + amountStr.size();
    }

    Expected<Bytes, HostFunctionError>
    floatFromInt(int64_t x, int32_t mode) override
    {
        return floatFromIntImpl(x, mode);
    }

    Expected<Bytes, HostFunctionError>
    floatFromUint(uint64_t x, int32_t mode) override
    {
        return floatFromUintImpl(x, mode);
    }

    Expected<Bytes, HostFunctionError>
    floatSet(int64_t mantissa, int32_t exponent, int32_t mode) override
    {
        return floatSetImpl(mantissa, exponent, mode);
    }

    Expected<int32_t, HostFunctionError>
    floatCompare(Slice const& x, Slice const& y) override
    {
        return floatCompareImpl(x, y);
    }

    Expected<Bytes, HostFunctionError>
    floatAdd(Slice const& x, Slice const& y, int32_t mode) override
    {
        return floatAddImpl(x, y, mode);
    }

    Expected<Bytes, HostFunctionError>
    floatSubtract(Slice const& x, Slice const& y, int32_t mode) override
    {
        return floatSubtractImpl(x, y, mode);
    }

    Expected<Bytes, HostFunctionError>
    floatMultiply(Slice const& x, Slice const& y, int32_t mode) override
    {
        return floatMultiplyImpl(x, y, mode);
    }

    Expected<Bytes, HostFunctionError>
    floatDivide(Slice const& x, Slice const& y, int32_t mode) override
    {
        return floatDivideImpl(x, y, mode);
    }

    Expected<Bytes, HostFunctionError>
    floatRoot(Slice const& x, int32_t n, int32_t mode) override
    {
        return floatRootImpl(x, n, mode);
    }

    Expected<Bytes, HostFunctionError>
    floatPower(Slice const& x, int32_t n, int32_t mode) override
    {
        return floatPowerImpl(x, n, mode);
    }

    Expected<Bytes, HostFunctionError>
    floatLog(Slice const& x, int32_t mode) override
    {
        return floatLogImpl(x, mode);
    }

    bool bn254Mul_called_ = false;
    bool bn254Add_called_ = false;
    bool bn254Neg_called_ = false;
    bool bn254Pairing_called_ = false;
    
    // std::optional<Bytes> last_mul_p1_;
    // std::optional<Bytes> last_mul_scalar_;
    std::optional<Bytes> last_mul_out_;
    std::optional<Bytes> last_add_out_;
    std::optional<Bytes> last_neg_out_;
    std::optional<int32_t> last_pairing_out_;

    // getters
    // std::optional<Bytes> const& lastMulP1() const { return last_mul_p1_; }
    // std::optional<Bytes> const& lastMulScalar() const { return last_mul_scalar_; }
    std::optional<Bytes> const& lastMulOut() const { return last_mul_out_; }
    std::optional<Bytes> const& lastAddOut() const { return last_add_out_; }
    std::optional<Bytes> const& lastNegOut() const { return last_neg_out_; }
    std::optional<int32_t> const& lastPairingOut() const { return last_pairing_out_; }

    Expected<Bytes, HostFunctionError>
    bn254MulHelper( Slice const& p1_uncompressed_be64, Slice const& scalar_uncompressed_be32) override
    {
        bn254Mul_called_ = true;

        libff::alt_bn128_pp::init_public_params();

        try{
            if (p1_uncompressed_be64.size() != G1_LEN || scalar_uncompressed_be32.size() != SCALAR_LEN)
                return Unexpected(HostFunctionError::INVALID_PARAMS);

            libff::alt_bn128_G1 p1;
            if (!g1_from_uncompressed_be(p1_uncompressed_be64.data(), p1)) 
                return Unexpected(HostFunctionError::DECODING);

            libff::bigint<libff::alt_bn128_r_limbs> s;
            if (!be32_to_bigint_r((scalar_uncompressed_be32.data()), s))
                return Unexpected(HostFunctionError::DECODING);

            libff::alt_bn128_G1 result = s * p1;

            // std::cout << "\n --++ Multiplication Result in Projective format = (" << result.X << ", " << result.Y << ")\n";

            Bytes out(G1_LEN);
            if (!g1_to_uncompressed_be(result, out.data()))
                return Unexpected(HostFunctionError::INTERNAL);

            // std::ostringstream oss;
            // oss << "[";
            // for (size_t i = 0; i < out.size(); ++i)
            // {
            //     oss << static_cast<int>(out[i]);
            //     if (i + 1 < out.size())
            //         oss << ", ";
            // }
            // oss << "]";
            // JLOG(getJournal().debug()) << "bn254MulHelper out=" << oss.str();
            
            last_mul_out_ = out;

            return out;
        } 
        catch (...) 
        {
            return Unexpected(HostFunctionError::INTERNAL);
        }

        // return result;
    }

    Expected<Bytes, HostFunctionError>
    bn254AddHelper( Slice const& p1_uncompressed_be64, Slice const& p2_uncompressed_be64) override
    {
        bn254Add_called_ = true;
        libff::alt_bn128_pp::init_public_params();

        try{
            if (p1_uncompressed_be64.size() != G1_LEN || p2_uncompressed_be64.size() != G1_LEN)
                return Unexpected(HostFunctionError::INVALID_PARAMS);

            libff::alt_bn128_G1 p1, p2;
            if (!g1_from_uncompressed_be(p1_uncompressed_be64.data(), p1) ||
                !g1_from_uncompressed_be(p2_uncompressed_be64.data(), p2))
                return Unexpected(HostFunctionError::DECODING);
                
            libff::alt_bn128_G1 result = p1 + p2;

            Bytes out(G1_LEN);
            if (!g1_to_uncompressed_be(result, out.data()))
                return Unexpected(HostFunctionError::INTERNAL);

            last_add_out_ = out;

            return out;
        }
        catch (...)
        {
            return Unexpected(HostFunctionError::INTERNAL);
        }
    }

    Expected<Bytes, HostFunctionError>
    bn254NegHelper( Slice const& p1_uncompressed_be64) override
    {
        bn254Neg_called_ = true;
        libff::alt_bn128_pp::init_public_params();

        try{
            if (p1_uncompressed_be64.size() != G1_LEN)
            return Unexpected(HostFunctionError::INVALID_PARAMS);

            libff::alt_bn128_G1 p1;
            if (!g1_from_uncompressed_be(p1_uncompressed_be64.data(), p1)) 
                return Unexpected(HostFunctionError::DECODING);

            libff::alt_bn128_G1 result = -p1;

            Bytes out(G1_LEN);
            if (!g1_to_uncompressed_be(result, out.data()))
                return Unexpected(HostFunctionError::INTERNAL);

            last_neg_out_ = out;

            return out;
        }
        catch (...)
        {
            return Unexpected(HostFunctionError::INTERNAL);
        }
    }

    Expected<int32_t, HostFunctionError>
    bn254PairingHelper(Slice const& pairs) override
    {
        // Disabe profiling for libff operations
        libff::inhibit_profiling_info = true;
        libff::inhibit_profiling_counters = true;
        
        bn254Pairing_called_ = true;
        libff::alt_bn128_pp::init_public_params();
        
        try{
            if (pairs.size() % PAIR_LEN != 0)
                return Unexpected(HostFunctionError::INVALID_PARAMS);

            libff::alt_bn128_Fq12 acc = libff::alt_bn128_Fq12::one();
            size_t const npairs = pairs.size() / PAIR_LEN;

            for (std::size_t i = 0; i < npairs; ++i) {
                auto const* base = pairs.data() + i * PAIR_LEN;

                libff::alt_bn128_G1 P;
                libff::alt_bn128_G2 Q;

                // Decode P (G1) and Q (G2) from uncompressed big-endian encodings
                if (!g1_from_uncompressed_be(base + 0, P))
                    return Unexpected(HostFunctionError::DECODING);

                if (!g2_from_uncompressed_be(base + G1_LEN, Q))
                    return Unexpected(HostFunctionError::DECODING);

                // Convention: ignore (0) pairs; they contribute neutral element
                if (P.is_zero() || Q.is_zero())
                    continue;

                auto preP = libff::alt_bn128_pp::precompute_G1(P);
                auto preQ = libff::alt_bn128_pp::precompute_G2(Q);

                acc = acc * libff::alt_bn128_pp::miller_loop(preP, preQ);
            }
            auto const gt = libff::alt_bn128_pp::final_exponentiation(acc);
            bool const result = (gt == libff::alt_bn128_GT::one());
            // std::cout << "bn254PairingHelper result: " << result << std::endl;
            
            // out[0] = result ? uint8_t{1} : uint8_t{0};

            last_pairing_out_ = result;

            return result ? 1 : -1;
        }
        catch (...)
        {
            return Unexpected(HostFunctionError::INTERNAL);
        }
    }
};

struct TestHostFunctionsSink : public TestHostFunctions
{
    test::StreamSink sink_;
    beast::Journal jlog_;
    void const* rt_ = nullptr;

public:
    explicit TestHostFunctionsSink(test::jtx::Env& env, int cd = 0)
        : TestHostFunctions(env, cd)
        , sink_(beast::severities::kDebug)
        , jlog_(sink_)
    {
    }

    test::StreamSink&
    getSink()
    {
        return sink_;
    }

    beast::Journal
    getJournal() override
    {
        return jlog_;
    }
};

struct PerfHostFunctions : public TestHostFunctions
{
    Keylet leKey;
    static int constexpr MAX_CACHE = 256;
    std::array<std::shared_ptr<SLE const>, MAX_CACHE> cache;
    std::shared_ptr<STTx const> tx_;

    void const* rt_ = nullptr;

public:
    PerfHostFunctions(
        test::jtx::Env& env,
        Keylet const& k,
        std::shared_ptr<STTx const>&& tx)
        : TestHostFunctions(env), leKey(k), tx_(std::move(tx))
    {
    }

    virtual Expected<int32_t, HostFunctionError>
    cacheLedgerObj(uint256 const&, int32_t cacheIdx) override
    {
        static int32_t intIdx = 0;

        if (cacheIdx < 0 || cacheIdx > MAX_CACHE)
            return Unexpected(HostFunctionError::SLOT_OUT_RANGE);

        if (!cacheIdx)
        {
            for (cacheIdx = 0; cacheIdx < MAX_CACHE; ++cacheIdx)
                if (!cache[cacheIdx])
                    break;
            if (cacheIdx >= MAX_CACHE)
                cacheIdx = intIdx++ % MAX_CACHE;
        }
        else
            --cacheIdx;

        if (cacheIdx >= MAX_CACHE)
            return Unexpected(HostFunctionError::SLOTS_FULL);

        cache[cacheIdx] = env_.le(leKey);
        if (!cache[cacheIdx])
            return Unexpected(HostFunctionError::LEDGER_OBJ_NOT_FOUND);
        return cacheIdx + 1;
    }

    static Bytes
    getAnyFieldData(STBase const& obj)
    {
        // auto const& fname = obj.getFName();
        auto const stype = obj.getSType();
        switch (stype)
        {
            case STI_UNKNOWN:
            case STI_NOTPRESENT:
                return {};
                break;
            case STI_ACCOUNT: {
                auto const& super(static_cast<STAccount const&>(obj));
                auto const& data = super.value();
                return {data.begin(), data.end()};
            }
            break;
            case STI_AMOUNT: {
                auto const& super(static_cast<STAmount const&>(obj));
                int64_t const data = super.xrp().drops();
                auto const* b = reinterpret_cast<uint8_t const*>(&data);
                auto const* e = reinterpret_cast<uint8_t const*>(&data + 1);
                return {b, e};
            }
            break;
            case STI_VL: {
                auto const& super(static_cast<STBlob const&>(obj));
                auto const& data = super.value();
                return {data.begin(), data.end()};
            }
            break;
            case STI_UINT256: {
                auto const& super(static_cast<STBitString<256> const&>(obj));
                auto const& data = super.value();
                return {data.begin(), data.end()};
            }
            break;
            case STI_UINT32: {
                auto const& super(
                    static_cast<STInteger<std::uint32_t> const&>(obj));
                std::uint32_t const data = super.value();
                auto const* b = reinterpret_cast<uint8_t const*>(&data);
                auto const* e = reinterpret_cast<uint8_t const*>(&data + 1);
                return {b, e};
            }
            break;
            default:
                break;
        }

        Serializer msg;
        obj.add(msg);

        return msg.getData();
    }

    Expected<Bytes, HostFunctionError>
    getTxField(SField const& fname) override
    {
        auto const* field = tx_->peekAtPField(fname);
        if (!field)
            return Unexpected(HostFunctionError::FIELD_NOT_FOUND);
        if ((STI_OBJECT == field->getSType()) ||
            (STI_ARRAY == field->getSType()))
            return Unexpected(HostFunctionError::NOT_LEAF_FIELD);

        return getAnyFieldData(*field);
    }

    Expected<Bytes, HostFunctionError>
    getCurrentLedgerObjField(SField const& fname) override
    {
        auto const sle = env_.le(leKey);
        if (!sle)
            return Unexpected(HostFunctionError::LEDGER_OBJ_NOT_FOUND);

        auto const* field = sle->peekAtPField(fname);
        if (!field)
            return Unexpected(HostFunctionError::FIELD_NOT_FOUND);
        if ((STI_OBJECT == field->getSType()) ||
            (STI_ARRAY == field->getSType()))
            return Unexpected(HostFunctionError::NOT_LEAF_FIELD);

        return getAnyFieldData(*field);
    }

    Expected<Bytes, HostFunctionError>
    getLedgerObjField(int32_t cacheIdx, SField const& fname) override
    {
        --cacheIdx;
        if (cacheIdx < 0 || cacheIdx >= MAX_CACHE)
            return Unexpected(HostFunctionError::SLOT_OUT_RANGE);

        if (!cache[cacheIdx])
        {
            // return Unexpected(HostFunctionError::INVALID_SLOT);
            cache[cacheIdx] = env_.le(leKey);
        }

        auto const* field = cache[cacheIdx]->peekAtPField(fname);
        if (!field)
            return Unexpected(HostFunctionError::FIELD_NOT_FOUND);
        if ((STI_OBJECT == field->getSType()) ||
            (STI_ARRAY == field->getSType()))
            return Unexpected(HostFunctionError::NOT_LEAF_FIELD);

        return getAnyFieldData(*field);
    }

    static Expected<STBase const*, HostFunctionError>
    locateField(STObject const& obj, Slice const& loc)
    {
        if (loc.empty() || (loc.size() & 3))  // must be multiple of 4
            return Unexpected(HostFunctionError::LOCATOR_MALFORMED);

        int32_t const* l = reinterpret_cast<int32_t const*>(loc.data());
        int32_t const sz = loc.size() / 4;
        STBase const* field = nullptr;
        auto const& m = SField::getKnownCodeToField();

        {
            int32_t const c = l[0];
            auto const it = m.find(c);
            if (it == m.end())
                return Unexpected(HostFunctionError::LOCATOR_MALFORMED);
            auto const& fname(*it->second);

            field = obj.peekAtPField(fname);
            if (!field || (STI_NOTPRESENT == field->getSType()))
                return Unexpected(HostFunctionError::FIELD_NOT_FOUND);
        }

        for (int i = 1; i < sz; ++i)
        {
            int32_t const c = l[i];

            if (STI_ARRAY == field->getSType())
            {
                auto const* arr = static_cast<STArray const*>(field);
                if (c >= arr->size())
                    return Unexpected(HostFunctionError::LOCATOR_MALFORMED);
                field = &(arr->operator[](c));
            }
            else if (STI_OBJECT == field->getSType())
            {
                auto const* o = static_cast<STObject const*>(field);

                auto const it = m.find(c);
                if (it == m.end())
                    return Unexpected(HostFunctionError::LOCATOR_MALFORMED);
                auto const& fname(*it->second);

                field = o->peekAtPField(fname);
            }
            else  // simple field must be the last one
            {
                return Unexpected(HostFunctionError::LOCATOR_MALFORMED);
            }

            if (!field || (STI_NOTPRESENT == field->getSType()))
                return Unexpected(HostFunctionError::FIELD_NOT_FOUND);
        }

        return field;
    }

    Expected<Bytes, HostFunctionError>
    getTxNestedField(Slice const& locator) override
    {
        // std::cout << tx_->getJson(JsonOptions::none).toStyledString() <<
        // std::endl;
        auto const r = locateField(*tx_, locator);
        if (!r)
            return Unexpected(r.error());

        auto const* field = r.value();
        if ((STI_OBJECT == field->getSType()) ||
            (STI_ARRAY == field->getSType()))
            return Unexpected(HostFunctionError::NOT_LEAF_FIELD);

        return getAnyFieldData(*field);
    }

    Expected<Bytes, HostFunctionError>
    getCurrentLedgerObjNestedField(Slice const& locator) override
    {
        auto const sle = env_.le(leKey);
        if (!sle)
            return Unexpected(HostFunctionError::LEDGER_OBJ_NOT_FOUND);

        auto const r = locateField(*sle, locator);
        if (!r)
            return Unexpected(r.error());

        auto const* field = r.value();
        if ((STI_OBJECT == field->getSType()) ||
            (STI_ARRAY == field->getSType()))
            return Unexpected(HostFunctionError::NOT_LEAF_FIELD);

        return getAnyFieldData(*field);
    }

    Expected<Bytes, HostFunctionError>
    getLedgerObjNestedField(int32_t cacheIdx, Slice const& locator) override
    {
        --cacheIdx;
        if (cacheIdx < 0 || cacheIdx >= MAX_CACHE)
            return Unexpected(HostFunctionError::SLOT_OUT_RANGE);

        if (!cache[cacheIdx])
        {
            // return Unexpected(HostFunctionError::INVALID_SLOT);
            cache[cacheIdx] = env_.le(leKey);
        }

        auto const r = locateField(*cache[cacheIdx], locator);
        if (!r)
            return Unexpected(r.error());

        auto const* field = r.value();
        if ((STI_OBJECT == field->getSType()) ||
            (STI_ARRAY == field->getSType()))
            return Unexpected(HostFunctionError::NOT_LEAF_FIELD);

        return getAnyFieldData(*field);
    }

    Expected<int32_t, HostFunctionError>
    getTxArrayLen(SField const& fname) override
    {
        if (fname.fieldType != STI_ARRAY)
            return Unexpected(HostFunctionError::NO_ARRAY);

        auto const* field = tx_->peekAtPField(fname);
        if (!field)
            return Unexpected(HostFunctionError::FIELD_NOT_FOUND);

        if (field->getSType() != STI_ARRAY)
            return Unexpected(HostFunctionError::NO_ARRAY);
        int32_t const sz = static_cast<STArray const*>(field)->size();

        return sz;
    }

    Expected<int32_t, HostFunctionError>
    getCurrentLedgerObjArrayLen(SField const& fname) override
    {
        if (fname.fieldType != STI_ARRAY)
            return Unexpected(HostFunctionError::NO_ARRAY);

        auto const sle = env_.le(leKey);
        if (!sle)
            return Unexpected(HostFunctionError::LEDGER_OBJ_NOT_FOUND);

        auto const* field = sle->peekAtPField(fname);
        if (!field)
            return Unexpected(HostFunctionError::FIELD_NOT_FOUND);

        if (field->getSType() != STI_ARRAY)
            return Unexpected(HostFunctionError::NO_ARRAY);
        int32_t const sz = static_cast<STArray const*>(field)->size();

        return sz;
    }

    Expected<int32_t, HostFunctionError>
    getLedgerObjArrayLen(int32_t cacheIdx, SField const& fname) override
    {
        if (fname.fieldType != STI_ARRAY)
            return Unexpected(HostFunctionError::NO_ARRAY);

        if (cacheIdx < 0 || cacheIdx >= MAX_CACHE)
            return Unexpected(HostFunctionError::SLOT_OUT_RANGE);

        if (!cache[cacheIdx])
        {
            // return Unexpected(HostFunctionError::INVALID_SLOT);
            cache[cacheIdx] = env_.le(leKey);
        }

        auto const* field = cache[cacheIdx]->peekAtPField(fname);
        if (!field)
            return Unexpected(HostFunctionError::FIELD_NOT_FOUND);

        if (field->getSType() != STI_ARRAY)
            return Unexpected(HostFunctionError::NO_ARRAY);
        int32_t const sz = static_cast<STArray const*>(field)->size();

        return sz;
    }

    Expected<int32_t, HostFunctionError>
    getTxNestedArrayLen(Slice const& locator) override
    {
        auto const r = locateField(*tx_, locator);
        if (!r)
            return Unexpected(r.error());
        auto const* field = r.value();

        if (field->getSType() != STI_ARRAY)
            return Unexpected(HostFunctionError::NO_ARRAY);
        int32_t const sz = static_cast<STArray const*>(field)->size();

        return sz;
    }

    Expected<int32_t, HostFunctionError>
    getCurrentLedgerObjNestedArrayLen(Slice const& locator) override
    {
        auto const sle = env_.le(leKey);
        if (!sle)
            return Unexpected(HostFunctionError::LEDGER_OBJ_NOT_FOUND);
        auto const r = locateField(*sle, locator);
        if (!r)
            return Unexpected(r.error());
        auto const* field = r.value();

        if (field->getSType() != STI_ARRAY)
            return Unexpected(HostFunctionError::NO_ARRAY);
        int32_t const sz = static_cast<STArray const*>(field)->size();

        return sz;
    }

    Expected<int32_t, HostFunctionError>
    getLedgerObjNestedArrayLen(int32_t cacheIdx, Slice const& locator) override
    {
        --cacheIdx;
        if (cacheIdx < 0 || cacheIdx >= MAX_CACHE)
            return Unexpected(HostFunctionError::SLOT_OUT_RANGE);

        if (!cache[cacheIdx])
        {
            // return Unexpected(HostFunctionError::INVALID_SLOT);
            cache[cacheIdx] = env_.le(leKey);
        }

        auto const r = locateField(*cache[cacheIdx], locator);
        if (!r)
            return Unexpected(r.error());

        auto const* field = r.value();

        if (field->getSType() != STI_ARRAY)
            return Unexpected(HostFunctionError::NO_ARRAY);
        int32_t const sz = static_cast<STArray const*>(field)->size();

        return sz;
    }

    Expected<int32_t, HostFunctionError>
    updateData(Slice const& data) override
    {
        ripple::detail::ApplyViewBase v(
            env_.app().openLedger().current().get(), tapNONE);

        auto sle = v.peek(leKey);
        if (!sle)
            return Unexpected(HostFunctionError::LEDGER_OBJ_NOT_FOUND);

        sle->setFieldVL(sfData, data);
        v.update(sle);

        return data.size();
    }

    Expected<Hash, HostFunctionError>
    computeSha512HalfHash(Slice const& data) override
    {
        auto const hash = sha512Half(data);
        return hash;
    }

    Expected<Bytes, HostFunctionError>
    getNFT(AccountID const& account, uint256 const& nftId) override
    {
        if (!account || !nftId)
        {
            getJournal().trace() << "WAMR getNFT: Invalid account or NFT ID";
            return Unexpected(HostFunctionError::INVALID_PARAMS);
        }

        auto obj = nft::findToken(*env_.current(), account, nftId);
        if (!obj)
        {
            getJournal().trace() << "WAMR getNFT: NFT not found";
            return Unexpected(HostFunctionError::LEDGER_OBJ_NOT_FOUND);
        }

        auto ouri = obj->at(~sfURI);
        if (!ouri)
            return Bytes();

        Slice const s = ouri->value();
        return Bytes(s.begin(), s.end());
    }

};

}  // namespace test
}  // namespace ripple
